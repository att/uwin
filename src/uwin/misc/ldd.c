#pragma prototyped
/*
 * ldd.c
 * Written by Don Caldwell
 * Mon Jun  16 10:46:00 EDT 1997
 */

static const char usage[] =
"[-?\n@(#)$Id: ldd (AT&T Labs Research) 2011-06-01 $\n]"
USAGE_LICENSE
"[+NAME?ldd - print shared library dependencies]"
"[+DESCRIPTION?\bldd\b writes to standard output each shared library and "
    "corresponding pathname required by each program named by each \afile\a "
    "specified on the command line.]"
"\n"
"\nfile ...\n"
"\n"
"[+EXIT STATUS?]"
    "{"
        "[+0?All shared libraries for all programs listed successfully.]"
        "[+>0?One or more programs not found or could not get the shared "
            "libraries that it used.]"
    "}"
"[+SEE ALSO?\bld\b(1), \bnld\b(1)]"
;

#include <cmd.h>
#include <uwin.h>
#include <windows.h>
#include <sys/wait.h>
#include <signal.h>
#include <psapi.h>

/* retrieve name of module from module's open file handle */
static int modulenamepath(HANDLE ph, LOAD_DLL_DEBUG_INFO* dll, char* name, int namesize, char* path, int pathsize)
{
	HANDLE		mh = 0;
	LPVOID		vh = 0;
	int 		r = -1;
	int		n;
	char*		s;
	char*		x;
	char		buf[PATH_MAX];

	if (!(mh = CreateFileMapping(dll->hFile, 0, PAGE_READONLY, 0, 0, 0)))
		error(-1, "CreateFileMapping(%p) failed", dll->hFile);
	else if (!(vh = MapViewOfFile(mh, FILE_MAP_READ, 0, 0, 0)))
		error(-1, "MapViewOfFile(%p) failed", mh);
	else if (GetMappedFileName(ph, dll->lpBaseOfDll, buf, sizeof(buf)) <= 0)
		error(-1, "GetMappedFileName(%p) failed [%d]", ph, GetLastError());
	else
	{
		r = 0;
		uwin_pathmap(buf, path, pathsize, UWIN_W2U);
		if (s = strrchr(path, '/'))
			s++;
		else
			s = path;
		if (!(x = strrchr(s, '.')) || strcasecmp(x+1, "dll"))
			x = s + strlen(s);
		if ((n = (int)(x - s)) >= namesize)
			n = namesize - 1;
		memcpy(name, s, n);
		name[n] = 0;
	}
	if (vh)
    		UnmapViewOfFile(vh);
	if (mh)
    		CloseHandle(mh);
	return r;
}

static int doldd(const char* filename)
{
	STARTUPINFO		si;
	HANDLE			ph; 
	DEBUG_EVENT		event;
	struct spawndata	sdata;
	char*			av[2];
	pid_t			pid;
	int			status;
	int			r = 0;
	char			dllname[PATH_MAX+1];
	char			dllpath[PATH_MAX+1];

	memset(&sdata, 0, sizeof(sdata));
	memset(&si, 0, sizeof(si));
	sdata.start = &si;
	sdata.flags = DEBUG_PROCESS;
	si.cb		= sizeof(si);
	si.dwFlags	= STARTF_FORCEONFEEDBACK|STARTF_USESHOWWINDOW;
	si.wShowWindow	= SW_SHOWNORMAL;
	av[0] = (char*)filename;
	av[1] = 0;
	if ((pid = uwin_spawn(filename, av, NULL, &sdata)) < 0)
	{
		error(ERROR_system(0), "%s cannot create process", filename);
		return -1;
	}
	ph = sdata.handle;
	for (;;)
	{
		if (!WaitForDebugEvent(&event, 1500))
			break;
		if (event.dwProcessId != uwin_ntpid(pid))
			continue;
		switch(event.dwDebugEventCode)
		{
		case EXIT_PROCESS_DEBUG_EVENT:
			CloseHandle(ph);
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			wait(&status);
			return 0;
		case LOAD_DLL_DEBUG_EVENT:
			if (event.u.LoadDll.hFile)
			{
				if (modulenamepath(ph, &event.u.LoadDll, dllname, sizeof(dllname), dllpath, sizeof(dllpath)) < 0)
					r = -1;
				else
					sfprintf(sfstdout, "\t%s => %s\n", dllname, dllpath);
			}
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			break;
		case CREATE_PROCESS_DEBUG_EVENT:
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			break;
		case CREATE_THREAD_DEBUG_EVENT:
		case EXCEPTION_DEBUG_EVENT:
			kill(pid, SIGTERM);
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_TERMINATE_PROCESS);
			break;
		default:
			ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
			break;
		}
	}
	return 0;
}

int b_ldd(int argc, char** argv)
{
	int		n;
	char*		cp;

	SetErrorMode(SEM_NOGPFAULTERRORBOX);
	cmdinit(argc, argv, 0, 0, 0);
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case '?':
			error(ERROR_usage(2), "%s", opt_info.arg);
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	if (error_info.errors || !*argv)
		error(ERROR_usage(2), "%s", optusage((char*)0));
	n = 0;
	while (cp = *argv++)
	{
		if (argc > 1)
			sfprintf(sfstdout, "%s:\n", cp);
		if (doldd(cp) < 0)
			n = 1;
	}
	return n;
}
