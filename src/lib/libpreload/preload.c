/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
/*
 * external hooks for the u/win LD_PRELOAD
 * along with the dll magic
 */

static const char id[] = "\n@(#)ld_preload.dll (AT&T Research) 1998-11-25\0\n";

#include <windows.h>
#include <uwin.h>
#if defined(DEBUG) || !defined(_DLL)
#   include <ast.h>
#endif

typedef int (*Syscall_f)(int,...);

typedef struct
{
	const char*	name;
	Syscall_f	traced;
	Syscall_f	actual;
	int		level;
} Syscall_t;

#define LIB_ADD		1
#define LIB_DELETE	2
#define LIB_LOOK	3

#if defined(__EXPORT__)
#   define extern	__EXPORT__
#endif
#ifndef elementsof
#   define elementsof(x)    (sizeof(x)/sizeof(x[0]))
#endif

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

#if defined(DEBUG)
#   define writelog(a,b,c)	sfprintf(sfstderr,a,b,c)
#else
#   define writelog(a,b,c)
#endif

struct library
{
	struct library *next;
	HANDLE		hp;
	Syscall_t	systab[1];
};

static struct library *liblist;

/*
 * These libraries are not checked for intercepts
 */
static char *skip_libs[] =
{
	"ntdll.dll",
	"mpr.dll",
	"rpcrt4.dll",
	0
};

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

static void dll_writable(void* addr)
{
        HANDLE				me = GetCurrentProcess();
        MEMORY_BASIC_INFORMATION	mem;
        int				r;

        if (!VirtualQueryEx(me, addr, &mem, sizeof(mem)))
                writelog("VirtualQueryEx failed addr=%x err=%d\n",addr,GetLastError());
        addr = (void*)mem.BaseAddress;
        if (!VirtualProtectEx(me, addr, 4096, PAGE_EXECUTE_READWRITE, &r))
                writelog("VirtualProtectEx failed addr=%x err=%d", addr,GetLastError());
}


static int dll_visited(void* p)
{
	register int	i;
	static void*	dlls[100];
	static int	ndlls;
	if(!p)
	{
		ndlls = 0;
		return 0;
	}
	for (i = 0; i < ndlls && p != dlls[i]; i++);
	if (i >= elementsof(dlls))
		return -1;
	if (p == dlls[i])
		return i + 1;
	dlls[i] = p;
	ndlls = i+1;
	writelog("dll_visited i=%d dp=%x\n",i,p);
	return(0);
}

static int dll_add(void* p, int flag)
{
	register int	i;
	static void*	dlls[100];
	static int	ndlls;

	if(!p)
	{
		ndlls = 0;
		return 0;
	}
	for (i = 0; i < ndlls && p != dlls[i]; i++);
	if (i >= elementsof(dlls))
		return -1;
	if (p == dlls[i])
	{
		if(flag==LIB_DELETE)
			dlls[i] = 0;
		return i + 1;
	}
	if(flag==LIB_ADD)
	{
		dlls[i] = p;
		ndlls = i+1;
		writelog("dll_add i=%d dp=%x\n",i,p);
	}
	return 0;
}

static int dll_intercept(register void* addr, Syscall_t* sys_call)
{
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_EXPORT_DIRECTORY*	exp;
	register int		i;
	register int		j;
	register int		base;
	char**			fname;
	WORD*			ords;
	void**			fns;
	char*			name;
	void*			fun;

	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return 0;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	exp = ptr(IMAGE_EXPORT_DIRECTORY, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if ((void*)exp == (void*)ntp)
		return 1;
	base = exp->Base - 1;
	fname = ptr(char*, addr, exp->AddressOfNames);
	ords = ptr(WORD, addr, exp->AddressOfNameOrdinals);
	fns = ptr(void*, addr, exp->AddressOfFunctions);
	/* we are assuming that both lists are sorted */
	for (i = j = 0; i < (int)exp->NumberOfNames; i++)
	{
		int n;
		name = ptr(char, addr, fname[i]);
		if ((n=strcmp(name, sys_call[j].name)) == 0)
		{
			fun = ptr(void, addr, fns[ords[i] - base]);
			sys_call[j].traced = (Syscall_f)fun;
			writelog("intercepted %s addr=%x\n",name,fun);
		}
		else if(n<0)
			continue;
		j++;
		if (sys_call[j].name == 0)
			return 1;
	}
	return 0;
}

static void dll_walk(void* addr, const char* parent, Syscall_t *sys_call)
{
	register Syscall_t	*sp;
	int			n;
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	IMAGE_THUNK_DATA*	tp;
	HINSTANCE		hp;
	char*			name;

	writelog("dll_walk parent=%s addr=%x\n",parent, addr);
	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	imp = ptr(IMAGE_IMPORT_DESCRIPTOR, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	if ((void*)imp == (void*)ntp || (void*)imp == addr)
		return;
	if(dll_visited(addr))
		return;
	for (; imp->Name; imp++)
	{
		name = ptr(char, addr, imp->Name);
		if (!(hp = GetModuleHandle(name)))
			continue;
		dll_walk(hp, name,sys_call);
		if((n=dll_add(hp,LIB_LOOK))==0)
			continue;
		for(sp=sys_call;sp->name;sp++)
		{
			if(sp->level==n)
				break;
		}
		if(!sp->name)
		{
			dll_add(hp,LIB_DELETE);
			continue;
		}
		for (tp = ptr(IMAGE_THUNK_DATA, addr, imp->FirstThunk); tp->u1.Function; tp++)
		{
			for(sp=sys_call;sp->name;sp++)
			{
				if(sp->level!=n)
					continue;
				if ((void*)tp->u1.Function == (void*)sp->actual)
				{
					if (sp->traced)
					{
						void**	ap= &(tp->u1.Function);
						dll_writable(ap);
						*ap = (void*)sp->traced;
					}
					writelog("%s caught actual=%x\n",sp->name,sp->actual);
					break;
				}
			}
		}
	}
	return;
}

static struct library * dll_exports(register void* addr)
{
	Syscall_t 		*systab,*sp;
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_EXPORT_DIRECTORY*	exp;
	register int		i;
	register int		base;
	char**			fname;
	WORD*			ords;
	void**			fns;
	struct library		*libold;

	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return 0;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	exp = ptr(IMAGE_EXPORT_DIRECTORY, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if ((void*)exp == (void*)ntp)
		return 0;
	base = exp->Base - 1;
	fname = ptr(char*, addr, exp->AddressOfNames);
	ords = ptr(WORD, addr, exp->AddressOfNameOrdinals);
	fns = ptr(void*, addr, exp->AddressOfFunctions);
	libold = liblist;
	liblist = (struct library*)malloc(sizeof(*liblist)+(exp->NumberOfNames)*sizeof(*sp));
	liblist->hp = addr;
	liblist->next = libold;
	systab = sp = liblist->systab;
	for (i = 0; i < (int)exp->NumberOfNames; i++)
	{
		sp->name = ptr(char, addr, fname[i]);
		sp->actual = 0;
		sp->level = 0;
		sp++;
	}
	sp->name = 0;
	return(liblist);
}

static int dll_actuals(HINSTANCE hp,Syscall_t *systab)
{
	void*			addr = (void*)hp;
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	char*			name;
	Syscall_t*		sp;
	int			n,left;

	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return 0;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	imp = ptr(IMAGE_IMPORT_DESCRIPTOR, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	if ((void*)imp == (void*)ntp || (void*)imp == addr)
		return 1;
	for (; imp->Name; imp++)
	{
		name = ptr(char, addr, imp->Name);
		if (!(hp = GetModuleHandle(name)))
			break;
		n = dll_add(hp,LIB_ADD);
		if(n>0)
			continue;
		writelog("look in name=%s addr=%x\n",name, hp);
		for(left=0,sp=systab;sp->name;sp++)
		{
			if(!sp->actual)
			{
				if(sp->actual = (Syscall_f)GetProcAddress(hp,sp->name))
				{
					writelog("%s: actual addr=%x\n",sp->name,sp->actual);
					if(n==0)
						n = dll_add(hp,LIB_LOOK);
					sp->level = n;
				}
				else
					left++;
			}
		}
		if(left)
			left = dll_actuals(hp,systab);
		if(!left)
			break;
	}
	return left;
}

extern HANDLE dllpreload(const char *name,int flags)
{
	struct library *lp,*ltab;
	char **skip;
	HANDLE hp;
	HINSTANCE	dp;

	if(hp = GetModuleHandle(name))
		return(hp);
	hp = LoadLibraryEx(name,0,flags);
	if(!hp)
	{
#if !defined(_DLL)
#   ifdef DEBUG
		sfprintf(sfstderr,"%s: cannot get module handle\n",name);
#   endif
		exit(2);
#else
		return(0);
#endif
	}
	dll_add(0,LIB_LOOK);
	lp = ltab = dll_exports(hp);
	while(lp = lp->next)
		dll_actuals(lp->hp,ltab->systab);	
	dll_actuals(GetModuleHandle(0),ltab->systab);
	if (!dll_intercept(ltab->hp, ltab->systab))
	{
#if defined(DEBUG)
#   ifdef DEBUG
		sfprintf(sfstderr, "preload: dll_intercept failed\n");
#   endif
#endif
		return 0;
	}
	dll_visited(0);
	for(skip=skip_libs; *skip; skip++)
	{
		if (dp = GetModuleHandle(*skip))
			dll_visited(dp);
	}
	dll_visited(ltab->hp);
	if (!(dp = GetModuleHandle(0)))
	{
#if defined(DEBUG)
		sfprintf(sfstderr, "preload: getmodulehandle(0) failed\n");
#endif
		return 0;
	}
	dll_walk(dp, "main",ltab->systab);
	return(hp);
}

void dll_init(void)
{
	HANDLE hp;
	char *cp;
	char buff[512],path[512];
	int n = GetEnvironmentVariable("LD_PRELOAD",buff,sizeof(buff));
	while(n>0)
	{
		buff[n] = 0;
		while(buff[--n]!=':' && n>0);
		if(buff[n]==':')
			cp = &buff[n+1];
		else
			cp = &buff[n];
		uwin_path(cp,path,sizeof(path));
#if defined(DEBUG)
		sfprintf(sfstderr,"preload=%s\n",path);
#endif
		hp = dllpreload(path,0);
	}
}

#if defined(_DLL)

/*
 * dll exception handler
 * we're only interested in process exit here
 */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, VOID* reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
		dll_init();
	return 1;
}


#else
int main(int argc, char *argv[])
{
	dll_init();
	sfprintf(sfstderr,"pid=%d\n",getpid());
	sfprintf(sfstderr,"ppid=%d\n",getppid());
	sfprintf(sfstderr,"atoi=%d\n",atoi("999"));
	return(0);
}
#endif

extern void *dllnextsym(const char *symbol)
{
        HANDLE				me = GetCurrentProcess();
        MEMORY_BASIC_INFORMATION	mem;
	void 				*addr;
	struct library			*lp;
	register Syscall_t		*sp;

        if (!VirtualQueryEx(me, (void*)symbol, &mem, sizeof(mem)))
                writelog("VirtualQueryEx failed addr=%x err=%d\n", symbol,GetLastError());
        addr = (void*)mem.AllocationBase;
	for(lp=liblist; lp; lp = lp->next)
	{
		if(lp->hp==addr)
			break;
	}
	if(lp)
	{
		for(sp=lp->systab;sp->name;sp++)
		{
			if(strcmp(sp->name,symbol)==0)
			{
				writelog("dllnextsym %x returned addr=%x\n",symbol,sp->actual);
				return(sp->actual);
			}
		}
	}
	writelog("dllnextsym %x returned addr=%x\n",symbol,0);
	return(0);
}
