/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#include "uwindef.h"
#include "dlls.h"

/*
 * modules are compile time dlls for POSIX_DLL or runtime dlls
 */

#define LOGLEVEL(index)		5

typedef struct Module_s
{
	const char*		library;
	const char**		aliases;
	int			naliases;
} Module_t;

typedef struct Module_state_s
{
	HMODULE		handle;
	pid_t		pid;
} Module_state_t;

static const char*		mod_advapi[] =	{ "advapi32" };
static const char*		mod_ast[] =	{ AST_DLL, AST_OLDDLL };
static const char*		mod_dbghelp[] =	{ "dbghelp" };
static const char*		mod_iphlpapi[] ={ "iphlpapi" };
static const char*		mod_kernel[] =	{ "kernel32" };
static const char*		mod_main[] =	{ 0 };
static const char*		mod_msi[] =	{ "msi" };
static const char*		mod_msvcrt[] =	{ MSVCRT, "msvcrt", "msvcr40", "msvcr20" };
static const char*		mod_netapi[] =	{ "netapi32" };
static const char*		mod_nt[] =	{ "ntdll" };
static const char*		mod_pdh[] =	{ "pdh" };
static const char*		mod_psapi[] =	{ "psapi" };
static const char*		mod_pthread[] =	{ "pthread" };
static const char*		mod_setup[] =	{ "setupapi" };
static const char*		mod_tlhelp[] =	{ "tlhelp32" };
static const char*		mod_winmm[] =	{ "winmm" };
static const char*		mod_winsock[] =	{ "ws2_32" };

#define MODULE_INFO(name)	{ #name, mod_##name, elementsof(mod_##name) }

/*
 * NOTE: order must match MODULE_* indices
 */

static const Module_t	module[] =
{
	MODULE_INFO(advapi),
	MODULE_INFO(ast),
	MODULE_INFO(dbghelp),
	MODULE_INFO(iphlpapi),
	MODULE_INFO(kernel),
	MODULE_INFO(main),
	MODULE_INFO(msi),
	MODULE_INFO(msvcrt),
	MODULE_INFO(netapi),
	MODULE_INFO(nt),
	MODULE_INFO(pdh),
	MODULE_INFO(psapi),
	MODULE_INFO(pthread),
	MODULE_INFO(setup),
	MODULE_INFO(tlhelp),
	MODULE_INFO(winmm),
	MODULE_INFO(winsock)
};

static Module_state_t	module_state[elementsof(module)];

static HMODULE
init_module(int index, const char* library, const char** aliases, int naliases)
{
	HMODULE		h;
	char*		f;
	int		i;
	char		buf[PATH_MAX];

	for (i = 0; i < naliases; i++)
	{
		if (!aliases[i])
		{
			if (h = GetModuleHandle(0))
				return h;
			continue;
		}
		f = "GetModuleHandle";
		sfsprintf(buf, sizeof(buf), "%s\\%s.dll", state.sys, aliases[i]);
		if (h = GetModuleHandle(buf))
			goto found;
		if (h = GetModuleHandle(aliases[i]))
			goto found;
		sfsprintf(buf, sizeof(buf), "%s\\%sd.dll", state.sys, aliases[i]);
		if (h = GetModuleHandle(buf))
			goto found;
		sfsprintf(buf, sizeof(buf), "%sd", aliases[i]);
		if (h = GetModuleHandle(buf))
			goto found;
		f = "LoadLibrary";
		sfsprintf(buf, sizeof(buf), "%s.dll", aliases[i]);
		if (h = LoadLibrary(buf))
			goto found;
		sfsprintf(buf, sizeof(buf), "%s\\%s.dll", state.sys, aliases[i]);
		if (h = LoadLibrary(buf))
			goto found;
	}
	logmsg(LOGLEVEL(index), "library %s not found", library);
	return 0;
 found:
	logmsg(LOGLEVEL(index), "library %s bound to %s (function=%s handle=%p)", library, buf, f, h);
	return h;
}

FARPROC
getsymbol(int index, const char* symbol)
{
	FARPROC		sym;
	pid_t		pid;

	if (P_CP)
	{
		pid = P_CP->pid;
		if (module_state[index].pid == -1)
			module_state[index].pid = pid;
	}
	else
		pid = -1;
	if (module_state[index].pid != pid)
	{
		module_state[index].pid = pid;
		module_state[index].handle = init_module(index, module[index].library, module[index].aliases, module[index].naliases);
	}
	if (!symbol)
		return (FARPROC)module_state[index].handle;
	if (!module_state[index].handle)
		return 0;
	if (!(sym = GetProcAddress(module_state[index].handle, symbol)))
		logmsg(LOGLEVEL(index), "symbol %s not found in library %s", symbol, module[index].library);
	return sym;
}

struct Dllist
{
	struct Dllist*	next;
	char*		name;
	int		count;
	int		mode;
	HANDLE		hp;
};

struct Symtab
{
	char*		name;
	FARPROC		fun;
};

static int		lasterror;
static struct Dllist*	first;
static int		nsym;
static struct Symtab*	symtab;

void _ast_setsymtab(void* addr, int n)
{
	nsym = n;
	symtab = (struct Symtab*)addr;
}

static FARPROC lookup(const char* name, struct Symtab* table, int n)
{
	int	mid;
	int	low = 0;
	int	high = n;

	while(high>=low)
	{
		mid = (high+low)/2;
		n = strcmp(name,table[mid].name);
		if(n==0)
			return(table[mid].fun);
		if(n>0)
			low = mid+1;
		else
			high = mid-1;
	}
	return(0);
}

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

static FARPROC dll_search(void* addr, const char* fname)
{
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	char*			name;
	FARPROC			fp;
	HANDLE			hp;

	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return 0;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	imp = ptr(IMAGE_IMPORT_DESCRIPTOR, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	if ((void*)imp == (void*)ntp || (void*)imp == addr)
		return 0;
	for (; imp->Name; imp++)
	{
		name = ptr(char, addr, imp->Name);
		if((hp=GetModuleHandle(name)) && (fp=GetProcAddress(hp,fname)))
			return(fp);
	}
	return(0);
}

static void addlist(Path_t* ip, int mode, HANDLE hp)
{
	struct Dllist*	lp;
	struct Dllist**	last;
	int		n;

	for(last= &first;lp= *last; last= &lp->next)
	{
		if(hp==lp->hp)
		{
			lp->count++;
			return;
		}
	}
	n = (int)strlen(ip->path)+1;
	lp = malloc(sizeof(*lp) + n);
	lp->next = 0;
	lp->count = 1;
	lp->name = (char*)(lp+1);
	lp->mode = mode;
	lp->hp = hp;
	memcpy(lp->name,ip->path,n);
	*last = lp;
}

static void dellist(HANDLE hp)
{
	struct Dllist *lp, **last;
	for(last= &first;lp= *last; last= &lp->next)
	{
		if(lp->hp==hp)
		{
			if(--lp->count==1)
			{
				*last = lp->next;
				free((void*)lp);
			}
			return;
		}
	}
	logmsg(0, "dlcose not on list hp=%p", hp);
}

void *dlopen(const char *pathname, int mode)
{
	HINSTANCE hp;
	size_t n;
	int i;
	char *path;
	Path_t info;
	info.oflags = GENERIC_READ;
	SetLastError(0);
	if (!pathname)
		return (HINSTANCE)getsymbol(MODULE_main, 0);
	if (!(path = strrchr(pathname, '/')) && !(path = strrchr(pathname, '\\')))
		path = (char*)pathname;
	n = strlen(path);
	if (n > 4 && !_stricmp(path + n - 4, ".dll"))
		n -= 4;
	for (i = 0; i < elementsof(module); i++)
		if (!_memicmp(path, module[i].library, n) && !module[i].library[n])
			return (HINSTANCE)getsymbol(i, 0);
	if (!(path = pathreal(pathname,P_EXIST|P_FILE|P_NOHANDLE,&info)))
	{
		lasterror = GetLastError();
		return(0);
	}
	SetLastError(0);
	if (hp = LoadLibraryEx(path,NULL,mode?0:DONT_RESOLVE_DLL_REFERENCES))
		addlist(&info,mode,hp);
	else
		lasterror = GetLastError();
	return(hp);
}

void *dlsym(void *handle, const char *name)
{
	FARPROC fp;
	static void *(*dllnext)(const char*);
	SetLastError(0);
	if(handle==RTLD_NEXT)
	{
		if(!dllnext && (handle=GetModuleHandle("preload10.dll")))
			dllnext = (void*(*)(const char*))GetProcAddress((HMODULE)handle,"dllnextsym");
		if(dllnext)
			return((*dllnext)(name));
		return(0);
	}
	SetLastError(0);
	if(handle==module_state[MODULE_main].handle)
	{
		if(symtab && (fp=lookup(name, symtab,nsym)))
			return(fp);
		return(dll_search(handle,name));
	}
	fp = GetProcAddress((HMODULE)handle,name);
	if(!fp)
		lasterror = GetLastError();
	return(fp);
}

int dlclose(void *handle)
{
	int i;
	for (i = 0; i < elementsof(module_state); i++)
		if (handle == module_state[i].handle)
			return(0);
	SetLastError(0);
	if(FreeLibrary(handle))
	{
		dellist(handle);
		return(0);
	}
	lasterror = GetLastError();
	return(-1);
}

char *dlerror(void)
{
	static char defmsg[128] = "error number xxxxx";
	int n,err = lasterror;
	if(err==0)
		return(0);
	n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		defmsg,sizeof(defmsg),NULL);
	if(n==0)
	{
		char *cp;
		strcpy(defmsg,"error number xxxxx");
		cp = defmsg+13;
		for(n=1000; n>0; n/=10)
		{
			*cp++ = '0' + err/n;
			err = err%n;
		}
		*cp=0;
	}
	return(defmsg);
}

void dll_fork(HANDLE hp)
{
	struct Dllist *lp;
	int c;
	for(lp=first;lp; lp=lp->next)
	{
		c=lp->count;
		while(c-- > 0)
		{
			if(!LoadLibraryEx(lp->name,NULL,lp->mode?0:DONT_RESOLVE_DLL_REFERENCES))
				logerr(0, "LoadLibraryEx %s failed", lp->name);
		}
	}
}
