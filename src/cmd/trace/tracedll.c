/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
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
 * external hooks for the u/win trace dll
 * along with the dll magic
 */

static const char id[] = "\n@(#)trace.dll (AT&T Research) 2011-06-06\0\n";

#include "tracehdr.h"

/*
 * do the same stuff for exit to catch all the trace options
 * (except counting)
 */

static int
traced_exit(int status)
{
	return 0;
}

static Syscall_t	sysexit =
{
	"exit", 0, (Syscall_f)traced_exit, (Syscall_f)traced_exit, { ARG_INT, ARG_INT }
};

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))

static void dll_writable(void* addr)
{
        HANDLE				me = GetCurrentProcess();
        MEMORY_BASIC_INFORMATION	mem;
        int				r;

        if (!VirtualQueryEx(me, addr, &mem, sizeof(mem)))
                trace_error(2, "VirtualQueryEx() failed err=%d", GetLastError());
        else if (!VirtualProtectEx(me, (void*)mem.BaseAddress, 4096, PAGE_EXECUTE_READWRITE, &r))
                trace_error(2, "VirtualProtectEx() failed err=%d", GetLastError());
}

static int dll_add(void* p)
{
	register int	i;

	static void*	dlls[100];

	for (i = 0; i < elementsof(dlls); i++)
		if (p == dlls[i])
			return i + 1;
		else if (!dlls[i])
		{
			dlls[i] = p;
			return 0;
		}
	return -1;
}

static int dll_intercept(register void* addr)
{
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_EXPORT_DIRECTORY*	exp;
	register int		i;
	register int		j;
	register int		base;
	DWORD*			names;
	WORD*			ordinals;
	DWORD*			functions;
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
	names = ptr(DWORD, addr, exp->AddressOfNames);
	ordinals = ptr(WORD, addr, exp->AddressOfNameOrdinals);
	functions = ptr(DWORD, addr, exp->AddressOfFunctions);
	for (i = j = 0; i < (int)exp->NumberOfNames; i++)
	{
		name = ptr(char, addr, names[i]);
		if (strcmp(name, sys_call[j].name) == 0)
		{
			fun = ptr(void, addr, functions[ordinals[i]-base]);
			sys_call[j].actual = (Syscall_f)fun;
			trace_error(-2, "%3d %3d %p %s", i, j, fun, name);
			if (++j >= elementsof(sys_call))
				return 1;
		}
	}
	return 0;
}

static int dll_walk(void* addr, const char* parent)
{
	register int		j;
	int			n;
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	IMAGE_THUNK_DATA*	tp;
	HINSTANCE		hp;
	char*			name;
	void**			fun;

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
		n = dll_add(hp);
		trace_error(-1, "%s::%s %3d %p %s", parent, name, n, hp, "DYNAMIC");
		if (n < 0)
			return 1;
		else if (n == 0)
			dll_walk(hp, name);
		else if (n == 1)
			for (tp = ptr(IMAGE_THUNK_DATA, addr, imp->FirstThunk); tp->u1.Function; tp++)
			{
				trace_error(-1, "%s::%s     %p %s", parent, name, (void*)tp->u1.Function, "THUNK");
				for (j = 0; j < elementsof(sys_call); j++)
					if ((void*)tp->u1.Function == (void*)sys_call[j].actual)
					{
						if (sys_call[j].traced)
						{
							fun = (void**)&(tp->u1.Function);

							dll_writable(fun);
							*fun = (void*)sys_call[j].traced;
						}
						trace_error(-1, "%s::%s %3d %p %s", parent, name, j, (void*)sys_call[j].actual, sys_call[j].name);
						break;
					}
			}
	}
	return 1;
}

/*
 * external hook to kick start tracing
 */

extern int trace_init(HANDLE hp, int flags)
{
	HINSTANCE	dp;

	/*
	 * initialize the trace_printf() and trace_error() state
	 */

	trace.debug = !(flags & UWIN_TRACE_DEBUG) ? 0 : !(flags & UWIN_TRACE_VERBOSE) ? -1 : -2;
	trace.pid = trace.opid = getpid();
	trace.hp = hp;
	trace.flags = (flags & UWIN_TRACE_FLAGS);
	trace.oflags = O_CREAT;
	trace.ep = (trace.bp = trace.buf) + sizeof(trace.buf);

	/*
	 * wrap the current calls with the traced versions
	 */

	if (!(dp = GetModuleHandle("posix.dll")))
	{
		trace_error(2, "GetModuleHandle(\"posix.dll\") failed");
		return 0;
	}
	if (!dll_intercept(dp))
	{
		int	i;

		trace_error(2, "--- missing symbols ---");
		for (i = 0; i < elementsof(sys_call); i++)
			if (!sys_call[i].actual)
				trace_error(2, "%s", sys_call[i].name);
		trace_error(2, "dll_intercept(\"posix.dll\") failed");
		return 0;
	}
	dll_add(dp);

	/*
	 * avoid searching a few libraries
	 */

	if (dp = GetModuleHandle("kernel32.dll"))
		dll_add(dp);
	if (dp = GetModuleHandle("ntdll.dll"))
		dll_add(dp);
	if (dp = GetModuleHandle("msvcrt.dll"))
		dll_add(dp);
	if (!(dp = GetModuleHandle(0)))
	{
		trace_error(2, "GetModuleHandle(0) failed");
		return 0;
	}
	dll_walk(dp, "main");

	/*
	 * initialize the per-process trace data
	 */

	trace.start = trace.watch = cycles();
	trace_error(-1, "trace_init(%ld) trace.flags=0x%08x\n", trace.pid, trace.flags);
	return 1;
}

/*
 * external hook to shut down tracing
 */

extern int trace_done(int exitcode)
{
	if ((trace.flags & UWIN_TRACE_CALL) && (exitcode != STILL_ACTIVE))
	{
		TRACE_CALL((&sysexit, exitcode));
		if (*trace.sep)
			trace_printf(" ) = ");
		trace_printf(" ?\n");
		trace_flush();
	}
	if (trace.flags & UWIN_TRACE_COUNT)
	{
		register int	i;
		unsigned int_8	c;
		unsigned int_8	calls = 0;
		unsigned int_8	watch = 0;
		unsigned int_8	io = 0;
		unsigned int_8	errors = 0;

		trace.cc = -1;
		trace.cycles = cycles();
		trace_printf("\n%-16s%10s%16s%10s%10s", "NAME", "CALLS", "CYCLES", "IO", "ERRORS");
		trace_printf("\n%-16s%10s%16s%10s%10s\n", "----", "-----", "------", "--", "------");
		for (i = 0; i < elementsof(sys_call); i++)
			if (c = trace.count[i].calls)
			{
				calls += c;
				trace_printf("%-16s %9llu", sys_call[i].name + sys_call[i].prefix, c);
				c = trace.count[i].cycles;
				watch += c;
				trace_printf(" %15llu", c);
				if (c = trace.count[i].io)
				{
					io += c;
					trace_printf(" %9llu", c);
				}
				else
					trace_printf("%10s", "");
				if (c = trace.count[i].errors)
				{
					errors += c;
					trace_printf(" %9llu", c);
				}
				trace_printf("\n");
			}
		trace_printf("%16s %9s %15s %9s %9s\n", "", "----", "--------", "----", "----");
		trace_printf("%-16s %9llu %15llu %9llu %9llu\n", "", calls, watch, io, errors);
		trace_printf("%-16s %9s %15llu\n", "", "", trace.cycles - trace.start);
		trace_flush();
	}
	trace.flags = 0;
	trace_error(-1, "trace_done(%ld)", trace.pid);
	return 1;
}

#if NEED_DLLMAIN

/*
 * dll exception handler
 * we're only interested in process exit here
 */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, VOID* reserved)
{
	if (reason == DLL_PROCESS_DETACH)
		trace_done();
	return 1;
}

#endif
