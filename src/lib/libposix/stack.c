/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
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
#include "uwinpsapi.h"
#include "uwin_stack.h"

#include <dbghelp.h>
#include <Tlhelp32.h>

#undef	GetThreadId
#define GetThreadId(h)		(0)

#define STACK_NAME_MAX		(1*1024)
#define MODULE_INFO_SIZE	(8*1024)

typedef PVOID   (__stdcall* SymFunctionTableAccess64_f)(HANDLE, DWORD64);
typedef BOOL    (__stdcall* SymGetLineFromAddr64_f)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef DWORD64 (__stdcall* SymGetModuleBase64_f)(HANDLE, DWORD64);
typedef BOOL    (__stdcall* SymGetModuleInfo64_f)(HANDLE, DWORD64, IMAGEHLP_MODULE64*);
typedef DWORD   (__stdcall* SymGetOptions_f)(VOID);
typedef BOOL    (__stdcall* SymGetSymFromAddr64_f)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
typedef BOOL    (__stdcall* SymInitialize_f)(HANDLE, LPSTR, BOOL);
typedef DWORD64 (__stdcall* SymLoadModule64_f)(HANDLE, HANDLE, LPSTR, LPSTR, DWORD64, DWORD);
typedef DWORD   (__stdcall* SymSetOptions_f)(DWORD);
typedef BOOL    (__stdcall* StackWalk64_f)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef DWORD   (WINAPI* UnDecorateSymbolName_f)(PCSTR, LPSTR, DWORD, DWORD);
typedef BOOL    (WINAPI* SymGetSearchPath_f)(HANDLE, LPSTR, DWORD);

typedef HANDLE  (__stdcall* CreateToolhelp32Snapshot_f)(DWORD, DWORD);
typedef BOOL    (__stdcall* Module32First_f)(HANDLE, LPMODULEENTRY32);
typedef BOOL    (__stdcall* Module32Next_f)(HANDLE, LPMODULEENTRY32);
typedef BOOL    (WINAPI* Thread32First_f)(HANDLE, LPTHREADENTRY32);
typedef BOOL    (WINAPI* Thread32Next_f)(HANDLE, LPTHREADENTRY32);

typedef struct Symbol_s
{
	IMAGEHLP_SYMBOL64	info;
	char			name[STACK_NAME_MAX];
	char			undecorated[STACK_NAME_MAX];
} Symbol_t;

typedef struct Stack_s
{
	const char*	sep;
	char*		buf;
	char*		ptr;
	char*		end;

	HANDLE		ph;
	HANDLE		th;
	DWORD		pid;
	DWORD		tid;
	CONTEXT*	context;
} Stack_t;

typedef struct Stack_state_s
{
	int				initialized;

	StackWalk64_f			StackWalk64;
	SymFunctionTableAccess64_f	SymFunctionTableAccess64;
	SymGetLineFromAddr64_f		SymGetLineFromAddr64;
	SymGetModuleBase64_f		SymGetModuleBase64;
	SymGetModuleInfo64_f		SymGetModuleInfo64;
	SymGetOptions_f			SymGetOptions;
	SymGetSearchPath_f		SymGetSearchPath;
	SymGetSymFromAddr64_f		SymGetSymFromAddr64;
	SymInitialize_f			SymInitialize;
	SymLoadModule64_f		SymLoadModule64;
	SymSetOptions_f			SymSetOptions;
	UnDecorateSymbolName_f		UnDecorateSymbolName;

	CreateToolhelp32Snapshot_f	CreateToolhelp32Snapshot;
	Module32First_f			Module32First;
	Module32Next_f			Module32Next;
	Thread32First_f			Thread32First;
	Thread32Next_f			Thread32Next;
} Stack_state_t;

static Stack_state_t	stackstate;

static BOOL __stdcall read_process_memory(HANDLE ph, DWORD64 addr, PVOID buf, DWORD size, LPDWORD bytes)
{
	BOOL	r;
	SIZE_T	n;

	r = ReadProcessMemory(ph, (LPVOID)addr, buf, size, &n);
	*bytes = (DWORD)n;
	return r;
}

static int load_tlhelp(Stack_t* ss)
{
	HANDLE		sh;
	MODULEENTRY32	module;
	int		i;

	static int	mods[] = { MODULE_kernel, MODULE_tlhelp };

	for (i = 0;; i++ )
	{
		if (i >= elementsof(mods))
			return 0;
		if ((stackstate.CreateToolhelp32Snapshot = (CreateToolhelp32Snapshot_f)getsymbol(mods[i], "CreateToolhelp32Snapshot")) &&
		    (stackstate.Module32First = (Module32First_f)getsymbol(mods[i], "Module32First")) &&
		    (stackstate.Module32Next = (Module32Next_f)getsymbol(mods[i], "Module32Next")) &&
		    (stackstate.Thread32First = (Thread32First_f)getsymbol(mods[i], "Thread32First")) &&
		    (stackstate.Thread32Next = (Thread32Next_f)getsymbol(mods[i], "Thread32Next")))
			break;
	}
	i = 0;
	if ((sh = stackstate.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ss->pid)) && sh != INVALID_HANDLE_VALUE)
	{
		module.dwSize = sizeof(module);
		if (stackstate.Module32First(sh, &module))
			do
			{
				stackstate.SymLoadModule64(ss->ph, 0, module.szExePath, module.szModule, (DWORD64)module.modBaseAddr, module.modBaseSize);
				i++;
			} while (stackstate.Module32Next(sh, &module));
		CloseHandle(sh);
	}
	return i;
}

Psapi_t* init_psapi(void)
{
	int			i;

	static int		pid;
	static Psapi_t		psapi;

	static int		mods[] = { MODULE_psapi, MODULE_kernel };

	if (P_CP->pid == pid)
		return &psapi;
	if (pid < 0)
		return 0;
	pid = -1;
	for (i = 0;; i++ )
	{
		if (i >= elementsof(mods))
			return 0;
		if ((psapi.EnumProcessModules = (Psapi_EnumProcessModules_f)getsymbol(mods[i], "EnumProcessModules")) &&
		    (psapi.GetModuleBaseName = (Psapi_GetModuleBaseName_f)getsymbol(mods[i], "GetModuleBaseNameA")) &&
		    (psapi.GetModuleFileNameEx = (Psapi_GetModuleFileNameEx_f)getsymbol(mods[i], "GetModuleFileNameExA")) &&
		    (psapi.GetModuleInformation = (Psapi_GetModuleInformation_f)getsymbol(mods[i], "GetModuleInformation")) &&
		    (psapi.GetProcessImageFileName = (Psapi_GetProcessImageFileName_f)getsymbol(mods[i], "GetProcessImageFileNameA")) &&
		    (psapi.GetProcessMemoryInfo = (Psapi_GetProcessMemoryInfo_f)getsymbol(mods[i], "GetProcessMemoryInfo")) &&
		    (psapi.QueryWorkingSet = (Psapi_QueryWorkingSet_f)getsymbol(mods[i], "QueryWorkingSet")))
		{
			pid = P_CP->pid;
			return &psapi;
		}
	}
	return 0;
}

static int load_psapi(Stack_t* ss)
{
	Psapi_t*		ps;
	int			i;
	int			n;
	MODULEINFO		module;
	char			dllname[MODULE_INFO_SIZE];
	char			modname[MODULE_INFO_SIZE];
	char			mhdata[sizeof(HMODULE) * (MODULE_INFO_SIZE / sizeof(HMODULE))];
	HMODULE*		mh = (HMODULE*)mhdata;

	if (!(ps = init_psapi()))
		return 0;
	if (!ps->EnumProcessModules(ss->ph, mh, MODULE_INFO_SIZE, &n))
		return 0;
	if (n > MODULE_INFO_SIZE)
		return 0;
	n /= sizeof(mh[0]);
	for (i = 0; i < n; i++)
	{
		ps->GetModuleInformation(ss->ph, mh[i], &module, sizeof(module));
		dllname[0] = 0;
		ps->GetModuleFileNameEx(ss->ph, mh[i], dllname, MODULE_INFO_SIZE);
		modname[0] = 0;
		ps->GetModuleBaseName(ss->ph, mh[i], modname, MODULE_INFO_SIZE);
		stackstate.SymLoadModule64(ss->ph, 0, dllname, modname, (DWORD64)module.lpBaseOfDll, module.SizeOfImage);
	}
	return i;
}

static int load(Stack_t* ss)
{
	DWORD		options;
	char		tmp[8*1024];
	char*		p = tmp;
	char*		e = p + sizeof(tmp) - 1;
	char*		v;
	size_t		n;

	if (state.install)
		bprintf(&p, e, "%s;", state.root);
	if ((v = getenv("INSTALLROOT")) && (n = uwin_pathmap(v, p, (int)(e-p), UWIN_U2W)) > 0)
	{
		p += n;
		bprintf(&p, e, "\\lib;");
	}
	if ((n = uwin_pathmap(state.wow ? "/32/usr/lib" : "/usr/lib", p, (int)(e-p), UWIN_U2W)) > 0)
	{
		p += n;
		bprintf(&p, e, ";");
	}
	if ((n = GetCurrentDirectoryA((DWORD)(e-p), p)) > 0)
	{
		p += n;
		bprintf(&p, e, ";");
	}
	if ((n = GetModuleFileNameA(0, p, (DWORD)(e-p))) > 0)
	{
		v = p;
		for (p += n; p > v; p--)
			if (*p == '\\' || *p == '/' || *p == ':')
			{
				bprintf(&p, e, ";");
				break;
			}
	}
	if ((n = GetEnvironmentVariableA("_NT_SYMBOL_PATH", p, (DWORD)(e-p))) > 0)
	{
		p += n;
		bprintf(&p, e, ";");
	}
	if ((n = GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", p, (DWORD)(e-p))) > 0)
	{
		p += n;
		bprintf(&p, e, ";");
	}
	if ((n = GetEnvironmentVariableA("SYSTEMROOT", p, (DWORD)(e-p))) > 0)
	{
		v = p;
		p += n;
		bprintf(&p, e, ";%-.*s\\system32;", p-v, v);
	}
	bprintf(&p, e, "SRV*");
	if ((n = GetEnvironmentVariableA("SYSTEMDRIVE", p, (DWORD)(e-p))) > 0)
	{
		v = p;
		p += n;
		bprintf(&p, e, ";%-.*s\\websymbols*http://msdl.microsoft.com/download/symbols;", p-v, v);
	}
	else
		bprintf(&p, e, "c:\\websymbols*http://msdl.microsoft.com/download/symbols;");
	if (!stackstate.SymInitialize(ss->ph, tmp, 0))
		return 0;
	options = stackstate.SymGetOptions();
	options |= SYMOPT_LOAD_LINES;
	options |= SYMOPT_FAIL_CRITICAL_ERRORS;
	stackstate.SymSetOptions(options);
	return load_tlhelp(ss) || load_psapi(ss);
}

static int init(Stack_t* ss)
{
	if (!(stackstate.StackWalk64 = (StackWalk64_f)getsymbol(MODULE_dbghelp, "StackWalk64")))
		return 0;
	if (!(stackstate.SymFunctionTableAccess64 = (SymFunctionTableAccess64_f)getsymbol(MODULE_dbghelp, "SymFunctionTableAccess64")))
		return 0;
	if (!(stackstate.SymGetLineFromAddr64 = (SymGetLineFromAddr64_f)getsymbol(MODULE_dbghelp, "SymGetLineFromAddr64")))
		return 0;
	if (!(stackstate.SymGetModuleBase64 = (SymGetModuleBase64_f)getsymbol(MODULE_dbghelp, "SymGetModuleBase64")))
		return 0;
	if (!(stackstate.SymGetModuleInfo64 = (SymGetModuleInfo64_f)getsymbol(MODULE_dbghelp, "SymGetModuleInfo64")))
		return 0;
	if (!(stackstate.SymGetOptions = (SymGetOptions_f)getsymbol(MODULE_dbghelp, "SymGetOptions")))
		return 0;
	if (!(stackstate.SymGetSearchPath =(SymGetSearchPath_f)getsymbol(MODULE_dbghelp, "SymGetSearchPath")))
		return 0;
	if (!(stackstate.SymGetSymFromAddr64 = (SymGetSymFromAddr64_f)getsymbol(MODULE_dbghelp, "SymGetSymFromAddr64")))
		return 0;
	if (!(stackstate.SymInitialize = (SymInitialize_f)getsymbol(MODULE_dbghelp, "SymInitialize")))
		return 0;
	if (!(stackstate.SymLoadModule64 = (SymLoadModule64_f)getsymbol(MODULE_dbghelp, "SymLoadModule64")))
		return 0;
	if (!(stackstate.SymSetOptions = (SymSetOptions_f)getsymbol(MODULE_dbghelp, "SymSetOptions")))
		return 0;
	if (!(stackstate.UnDecorateSymbolName = (UnDecorateSymbolName_f)getsymbol(MODULE_dbghelp, "UnDecorateSymbolName")))
		return 0;
	return load(ss);
}

static void uwin_stack_walk(Stack_t* ss)
{
	Symbol_t		symbol;
	union
	{
	IMAGEHLP_MODULE64	module;
	char			data[1680];
	}			module;
	IMAGEHLP_LINE64		line;
	CONTEXT			context;
	STACKFRAME64		frame;
    	DWORD64			base;
    	DWORD64			symboloffset;
    	DWORD			lineoffset;
	DWORD			image;
	char*			name;
	char*			object;
	char*			file;
	char*			s;
	int			resume = 0;

	if (!ss->ph)
		ss->ph = GetCurrentProcess();
	if (!ss->pid)
		ss->pid = GetProcessId(ss->ph);
	if (stackstate.initialized < 0)
		return;
	if (!stackstate.initialized)
	{
		if (!init(ss))
		{
			stackstate.initialized = -1;
			return;
		}
		stackstate.initialized = 1;
	}
	if (ss->context)
		context = *ss->context;
	else if (!ss->th || ss->th == GetCurrentThread())
	{
		memset(&context, 0, sizeof(context));
		context.ContextFlags = UWIN_CONTEXT_TRACE;
#ifdef _M_IX86
		__asm		call capture_context
		__asm capture_context: pop eax
		__asm		mov context.Eip, eax
		__asm		mov context.Ebp, ebp
		__asm		mov context.Esp, esp
#else
		RtlCaptureContext(&context);
#endif
	}
	else
	{
		SuspendThread(ss->th);
		memset(&context, 0, sizeof(CONTEXT));
		context.ContextFlags = UWIN_CONTEXT_TRACE;
		if (!GetThreadContext(ss->th, &context))
		{
			ResumeThread(ss->th);
			return;
		}
		resume = 1;
	}
	memset(&frame, 0, sizeof(frame));
#ifdef _M_IX86
	image = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Esp;
	frame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64 || _M_AMD64
	image = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context.Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.Rsp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.Rsp;
	frame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
	image = IMAGE_FILE_MACHINE_IA64;
	frame.AddrPC.Offset = context.StIIP;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = context.IntSp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = context.IntSp;
	frame.AddrStack.Mode = AddrModeFlat;
	frame.AddrBStore.Offset = context.RsBSP;
	frame.AddrBStore.Mode = AddrModeFlat;
#else
#error "unknown stack architecture"
#endif
	memset(&symbol, 0, sizeof(symbol));
	symbol.info.SizeOfStruct = sizeof(symbol.info);
	symbol.info.MaxNameLength = sizeof(symbol.name);
	memset(&line, 0, sizeof(line));
	line.SizeOfStruct = sizeof(line);
	memset(&module, 0, sizeof(module));
	module.module.SizeOfStruct = offsetof(IMAGEHLP_MODULE64, LoadedPdbName); /* splain this lucy */
	while (stackstate.StackWalk64(image, ss->ph, ss->th, &frame, &context, read_process_memory, stackstate.SymFunctionTableAccess64, stackstate.SymGetModuleBase64, 0) && frame.AddrPC.Offset != frame.AddrReturn.Offset)
	{
		if (frame.AddrPC.Offset && frame.AddrReturn.Offset)
		{
			if (!stackstate.SymGetSymFromAddr64(ss->ph, frame.AddrPC.Offset, &symboloffset, &symbol.info))
				name = 0;
			else if (strcmp(symbol.info.Name, "KiFastSystemCallRet") == 0 || strncmp(symbol.info.Name, "uwin_stack_", 11) == 0)
				goto skip;
			else if (stackstate.UnDecorateSymbolName(symbol.info.Name, symbol.undecorated, sizeof(symbol.undecorated), UNDNAME_COMPLETE) || stackstate.UnDecorateSymbolName(symbol.info.Name, symbol.undecorated, sizeof(symbol.undecorated), UNDNAME_NAME_ONLY))
				name = symbol.undecorated;
			else
				name = symbol.info.Name;
			if (stackstate.SymGetLineFromAddr64(ss->ph, frame.AddrPC.Offset, &lineoffset, &line))
				file = line.FileName;
			else
			{
				file = 0;
				line.LineNumber = 0;
			}
			if (stackstate.SymGetModuleInfo64(ss->ph, frame.AddrPC.Offset, &module.module) ||
			    (module.module.SizeOfStruct = 584) && stackstate.SymGetModuleInfo64(ss->ph, frame.AddrPC.Offset, &module.module) ||
			    (module.module.SizeOfStruct = 1664) && stackstate.SymGetModuleInfo64(ss->ph, frame.AddrPC.Offset, &module.module) ||
			    (module.module.SizeOfStruct = 1680) && stackstate.SymGetModuleInfo64(ss->ph, frame.AddrPC.Offset, &module.module))
			{
				object = module.module.ModuleName;
				base = module.module.BaseOfImage;
			}
			else
			{
				object = "??";
				base = 0;
			}
			if (ss->ptr > ss->buf)
				bprintf(&ss->ptr, ss->end, "%s", ss->sep);
			if (file)
			{
				for (s = file; *s; s++)
					if (*s == '\\')
						file = s + 1;
				bprintf(&ss->ptr, ss->end, "%s:%s:%d", object, file, line.LineNumber);
			}
			else
				bprintf(&ss->ptr, ss->end, "%s:%x", object, (DWORD)(frame.AddrPC.Offset - base));
			if (name)
				bprintf(&ss->ptr, ss->end, ":%s", name);
			else if (file)
				bprintf(&ss->ptr, ss->end, ":%x", (DWORD)(frame.AddrPC.Offset - base));
		}
	skip:
		if (!frame.AddrReturn.Offset || strcmp(symbol.info.Name, "main") == 0)
		{
			SetLastError(ERROR_SUCCESS);
			break;
		}
	}
	if (resume)
		ResumeThread(ss->th);
}

size_t uwin_stack_exception(EXCEPTION_POINTERS* exp, const char* sep, char* buf, size_t size)
{
	Stack_t		ss;

	ss.sep = sep;
	ss.ptr = ss.buf = buf;
	ss.end = buf + size - 1;
	ss.pid = 0;
	ss.ph = 0;
	ss.th = GetCurrentThread();
	ss.tid = GetThreadId(ss.th);
	ss.context = exp->ContextRecord;
	uwin_stack_walk(&ss);
	*ss.ptr = 0;
	return ss.ptr - buf;
}

ssize_t uwin_stack_thread(pid_t tid, void* th, const char* sep, char* buf, size_t size)
{
	Stack_t		ss;
	int		ch = 0;

	ss.sep = sep;
	ss.ptr = ss.buf = buf;
	ss.end = buf + size - 1;
	if (!th && tid)
	{
		if (!(th = OpenThread(THREAD_QUERY_INFORMATION, 0, tid)))
		{
			errno = ESRCH;
			goto nope;
		}
		ch = 1;
	}
	ss.pid = 0;
	ss.ph = 0;
	ss.tid = tid;
	ss.th = th;
	ss.context = 0;
	uwin_stack_walk(&ss);
	if (ch)
		CloseHandle(th);
	if (*sep == '\n' && ss.ptr < ss.end)
		*ss.ptr++ = '\n';
	*ss.ptr = 0;
	return (ssize_t)(ss.ptr - buf);
 nope:
	*ss.ptr = 0;
	return -1;
}

ssize_t uwin_stack_process(pid_t pid, void* ph, const char* sep, char* buf, size_t size)
{
	Stack_t		ss;
	HANDLE		sh;
	HANDLE		th;
	THREADENTRY32	te;
	int		ch = 0;

	ss.sep = strcmp(sep, "\n") ? sep : "\n\t";
	ss.ptr = ss.buf = buf;
	ss.end = buf + size - 1;
//logmsg(0, "stack_proc pid=%d ph=%x", pid, ph);
	if (!ph)
	{
		if (!pid)
			pid = getpid();
		pid = uwin_ntpid(pid);
		if (!(ph = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, 0, pid)))
		{
			errno = ESRCH;
			goto nope;
		}
		ch = 1;
	}
	else if (pid)
		pid = uwin_ntpid(pid);
	else
		pid = GetProcessId(ph);
	if (pid < 0)
	{
		errno = ESRCH;
		goto nope;
	}
	ss.pid = pid;
	ss.ph = ph;
	ss.context = 0;
//logmsg(0, "uwin_stack_process ntpid=%d ph=%x stackstate init=%d table access=%x", ss.pid, ss.ph, stackstate.initialized, stackstate.SymGetModuleInfo64);
	if (stackstate.initialized < 0)
	{
		errno = ENOSYS;
		goto nope;
	}
	if (!stackstate.initialized)
	{
		if (!init(&ss))
		{
			stackstate.initialized = -1;
			errno = ENOSYS;
			goto nope;
		}
		stackstate.initialized = 1;
	}
	if (!stackstate.CreateToolhelp32Snapshot)
	{
		errno = ENOSYS;
		goto nope;
	}
	if ((sh = stackstate.CreateToolhelp32Snapshot(TH32CS_SNAPALL, pid)) != INVALID_HANDLE_VALUE)
	{
		memset(&te, 0, sizeof(te));
		te.dwSize = sizeof(te);
		if (stackstate.Thread32First(sh, &te))
		{
			do
			{
				if (te.th32OwnerProcessID != pid)
					continue;
				if (th = OpenThread(THREAD_ALL_ACCESS, 0, te.th32ThreadID))
				{
					if (ss.ptr > ss.buf)
						bprintf(&ss.ptr, ss.end, "%s", *sep == '\n' ? "\n" : sep);
					bprintf(&ss.ptr, ss.end, "[thread-%x]", te.th32ThreadID);
					ss.tid = te.th32ThreadID;
					ss.th = th;
					uwin_stack_walk(&ss);
					CloseHandle(th);
				}
			} while(stackstate.Thread32Next(sh, &te));
		}
		CloseHandle(sh);
	}
	if (ch)
		CloseHandle(ph);
	if (*sep == '\n' && ss.ptr < ss.end)
		*ss.ptr++ = '\n';
	*ss.ptr = 0;
	return (ssize_t)(ss.ptr - buf);
 nope:
	*ss.ptr = 0;
	return -1;
}
