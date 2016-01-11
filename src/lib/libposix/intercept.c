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
#include        "uwindef.h"

void *Mallocptr;

static int sopen(const char *name, int flags, int share, ...)
{
	va_list ap;
	mode_t mode;
	va_start(ap,share);
	mode = (flags & O_CREAT) ? va_arg(ap, int) : S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	va_end (ap);
	return(open(name,flags,mode));
}

static int setmode(int fd, int mode)
{
	logerr(0, "setmode");
	return(O_BINARY);
}

static void flushall(void)
{
	void (*fun)(void*);
	if(fun= (void(*)(void*))getsymbol(MODULE_ast,"sfsync"))
		(*fun)(NULL);
}

struct Intercept
{
	const char *name;
	void (*intercept)(void);
	void (*actual)(void);
};

#undef free

static struct Intercept exit_call[2];

static int my_shexit(int n)
{
	int (*fn)(int) = (int (*)(int))exit_call[0].actual;
	int slot, console, i;
	Pdev_t  *pdev;
	_uwin_exit(n);
	P_CP->state = PROCSTATE_TERMINATE;
	_ast_atexit();
	console=P_CP->console;
	i=(console>=0?0:P_CP->maxfds);
	pdev = dev_ptr(console);
	for(; i < P_CP->maxfds; i++)
	{
		if((slot=fdslot(P_CP,i)) >=0 && Pfdtab[slot].devno==console)
			goto found;
	}
	slot = -1;
found:
	logmsg(LOG_PROC+5, "my_shexit self terminate sid=%d group=%d pgid=%d, ntproc=0x%x cons=%d cons_chld=%d exit=0x%x", P_CP->sid, P_CP->group, P_CP->pgid, GetCurrentProcess(), console,  P_CP->console_child, n);
	if(P_CP->console_child && console>=0 &&  pdev->count>=0)
	{
		if(slot>=0 && Pfdtab[slot].refcount>=0)
		{
			logmsg(0,"intercept slot=%d refcount=%d type=%(fdtype)s index=%d",slot,Pfdtab[slot].refcount,Pfdtab[slot].type,Pfdtab[slot].index);
			fdpclose(&Pfdtab[slot]);
		}
		pdev->count = 0;
		P_CP->console = console;
		free_term(pdev,0);
	}
	P_CP->exitcode = n<<8;
	P_CP->posixapp |= POSIX_DETACH;
	if(!TerminateProcess(GetCurrentProcess(),P_CP->exitcode))
	{
		logerr(0,"terminate failed");
		TerminateProcess(GetCurrentProcess(),P_CP->exitcode);
		kill(0,SIGKILL);
	}
	return((*fn)(n));
}

static int my_exit(int n)
{
	int (*fn)(int) = (int (*)(int))exit_call[0].actual;
	_uwin_exit(n);
	return((*fn)(n));
}
static int my__exit(int n)
{
	int (*fn)(int) = (int (*)(int))exit_call[1].actual;
	_uwin_exit(n);
	return((*fn)(n));
}


static struct Intercept exit_call[2] =
{
	{ "exit",	 (void(*)(void))&my_exit},
	{ "_exit",	 (void(*)(void))&my__exit},
};

static struct Intercept sys_call[] =
{
	{ "??2@YAPAXI@Z",	(void(*)(void))&malloc},
	{ "??3@YAXPAX@Z",	(void(*)(void))&free},
	{ "_close",	(void(*)(void))&close},
	{ "_fdopen"	},
	{ "_fileno"	},
	{ "_filbuf"	},
	{ "_flsbuf"	},
	{ "_flushall",	(void(*)(void))&flushall},
	{ "_lseek",	(void(*)(void))&lseek},
	{ "_pclose"	},
	{ "_popen"	},
	{ "_putw"	},
	{ "_read",	(void(*)(void))&read },
	{ "_setmode",	(void(*)(void))&setmode },
	{ "_snprintf"	},
	{ "_sopen",	(void(*)(void))&sopen },
	{ "_vsnprintf"	},
	{ "_write",	(void(*)(void))&write},
	{ "calloc",	(void(*)(void))&calloc},
	{ "clearerr"	},
	{ "fclose"	},
	{ "feof"	},
	{ "ferror"	},
	{ "fflush"	},
	{ "fgetc"	},
	{ "fgetpos"	},
	{ "fgets"	},
	{ "fopen"	},
	{ "fprintf"	},
	{ "fputc"	},
	{ "fputs"	},
	{ "free",	(void(*)(void))&free},
	{ "fread"	},
	{ "fscanf"	},
	{ "fseek"	},
	{ "fsetpos"	},
	{ "ftell"	},
	{ "fwrite"	},
	{ "getc"	},
	{ "getchar"	},
	{ "malloc",	(void(*)(void))&malloc},
	{ "printf"	},
	{ "putc"	},
	{ "putchar"	},
	{ "puts"	},
	{ "realloc",	(void(*)(void))&realloc},
	{ "rewind"	},
	{ "scanf"	},
	{ "setbuf"	},
	{ "setvbuf"	},
	{ "sprintf"	},
	{ "sscanf"	},
	{ "tmpfile"	},
	{ "ungetc"	},
	{ "vfprintf"	},
	{ "vprintf"	},
	{ "vsprintf"	}
};


#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

static void dll_writable(void* addr)
{
        HANDLE				me = GetCurrentProcess();
        MEMORY_BASIC_INFORMATION	mem;
        int				r;

        if (!VirtualQueryEx(me, addr, &mem, sizeof(mem)))
                logerr(0, "VirtualQueryEx");
        addr = (void*)mem.BaseAddress;
        if (!VirtualProtectEx(me, addr, 4096, PAGE_EXECUTE_READWRITE, &r))
                logerr(0, "VirtualProtectEx");
}

static int dll_intercept(void* addr, HANDLE msvc, struct Intercept *sys_call, int nitems)
{
	register int		j;
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	IMAGE_THUNK_DATA*	tp;
	HINSTANCE		hp;
	char*			name;

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
		if(hp!=msvc)
			continue;
		for (tp = ptr(IMAGE_THUNK_DATA, addr, imp->FirstThunk); tp->u1.Function; tp++)
		{
			for (j = 0; j < nitems; j++)
			{
				if ((void*)tp->u1.Function == (void*)sys_call[j].actual)
				{
					if (sys_call[j].intercept)
					{
						void**	ap= (void**)&(tp->u1.Function);

						dll_writable(ap);
#if 0
						logmsg(0, "%s changed %p to %p ap=%p *ap=0x%x", sys_call[j].name, sys_call[j].actual, sys_call[j].intercept, ap, *ap);
#endif
						*ap = (void*)sys_call[j].intercept;
					}
					break;
				}
			}
		}
	}
	return 1;
}

/*
 * intercept stdio functions from msvicrt.dll and msvcp??.dll
 */
void intercept_init(void)
{
	char path[256];
	void *(*alloc)(int);
	HANDLE hp, msvci, msvcp, msvc, shell, cmd, ast;
	int j;
	if(!(msvc = GetModuleHandle(MSVCRT"d.dll")) && !(msvc = GetModuleHandle(MSVCRT".dll")))
		return;
	if(!(msvci=GetModuleHandle("msvcirt.dll")))
		msvci = GetModuleHandle("msvcirtd.dll");
	if(!(msvcp = GetModuleHandle("msvcp50.dll")))
		if(!(msvcp = GetModuleHandle("msvcp50d.dll")))
			if(!(msvcp = GetModuleHandle("msvcp60.dll")))
				msvcp = GetModuleHandle("msvcp60d.dll");
	cmd = GetModuleHandle("cmd12.dll");
	ast = GetModuleHandle("ast54.dll");
	shell = GetModuleHandle("shell11.dll");
	for (j = 0; j < elementsof(exit_call); j++)
	{
		const char *name = exit_call[j].name;
		exit_call[j].actual = (void(*)(void))GetProcAddress(msvc,name);
		if(!exit_call[j].actual)
			logerr(0, "GetProcAddress %s failed", name);
	}
	dll_intercept(GetModuleHandle(NULL),msvc,exit_call, elementsof(exit_call));
	if(cmd)
		dll_intercept(cmd,msvc,exit_call, elementsof(exit_call));
	if(ast)
		dll_intercept(ast,msvc,exit_call, elementsof(exit_call));
	if(shell)
	{
		exit_call[0].intercept = (void(*)(void))my_shexit;
		dll_intercept(shell,msvc,exit_call, elementsof(exit_call));
	}
	if(uwin_pathmap("/usr/bin/stdio.dll",path,sizeof(path),UWIN_U2W) <=0)
		return;
	if(!msvcp && !msvci)
		return;
	/* region of static initializers */
	if(alloc = (void*(*)(int))GetProcAddress(msvc,"malloc"))
		Mallocptr  = (*alloc)(64);
	if(!(hp = LoadLibraryEx(path,0,0)))
	{
		if(msvci || msvcp)
			logerr(0, "LoadLibraryEx %s failed", path);
		return;
	}
	for (j = 0; j < elementsof(sys_call); j++)
	{
		const char *name = sys_call[j].name;
		sys_call[j].actual = (void(*)(void))GetProcAddress(msvc,name);
		if(!sys_call[j].intercept)
		{
			sys_call[j].intercept = (void(*)(void))GetProcAddress(hp,name);
			if(!sys_call[j].intercept && *sys_call[j].name=='_')
				sys_call[j].intercept = (void(*)(void))GetProcAddress(hp,&name[1]);
		}
		if(!sys_call[j].intercept)
			logerr(0, "GetProcAddress %s failed", name);
		if(!sys_call[j].actual)
			logerr(0, "GetProcAddress %s failed", name);
	}
	dll_intercept(GetModuleHandle(NULL),msvc,sys_call, elementsof(sys_call));
	if(msvcp)
		dll_intercept(msvcp,msvc, sys_call, elementsof(sys_call));
	if(msvci)
		dll_intercept(msvci,msvc, sys_call, elementsof(sys_call));
	return;
}
