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
#include	<windows.h>
#include	"fsnt.h"
#include	"procdir.h"
#include	"uwin_share.h"
#include	<uwin.h>
#include	<sys/procfs.h>
#include	<tlhelp32.h>

#define PSAGE	200000000

typedef struct Snapstate_s
{
	int			initialized;
	int			first;
	int			scan;
	__int64			ts;
	HANDLE			(WINAPI* snapshot)(DWORD, DWORD);
	BOOL			(WINAPI* procfirst)(HANDLE, PROCESSENTRY32*);
	BOOL			(WINAPI* procnext)(HANDLE, PROCESSENTRY32*);
	HANDLE			snaphp;
	PROCESSENTRY32		proc;
} Snapstate_t;

static Snapstate_t	snap_state;

static int snap_map_state(int flags)
{
	if (flags&1)
		return PROCSTATE_STOPPED;
	if (flags&0xf0000000)
		return PROCSTATE_TERMINATE;
	return PROCSTATE_RUNNING;
}

static DWORD snap_dirnext(Procdirstate_t* pd)
{
	int		r;

	if (snap_state.scan < 0)
		return 0;
	if (snap_state.first)
	{
		r = (*snap_state.procfirst)(snap_state.snaphp, &snap_state.proc);
		if (!snap_state.proc.th32ProcessID)
			r = (*snap_state.procnext)(snap_state.snaphp, &snap_state.proc);
	}
	else
		r = (*snap_state.procnext)(snap_state.snaphp, &snap_state.proc);
	if (r)
	{
		snap_state.first = 0;
		snap_state.scan = 1;
		return snap_state.proc.th32ProcessID;
	}
	snap_state.scan = -1;
	if (GetLastError() == ERROR_NO_MORE_FILES)
		return 0;
	else if (snap_state.first)
		logerr(0, "procfirst");
	else
		logerr(0, "procnext");
	return 0;
}

static int snap_dirinit(int force)
{
	FILETIME	ftime;
	__int64		ts;

	if (!snap_state.initialized)
		snap_state.initialized =
			(snap_state.snapshot = (HANDLE(WINAPI*)(DWORD,DWORD))getsymbol(MODULE_kernel, "CreateToolhelp32Snapshot")) &&
			(snap_state.procfirst = (BOOL(WINAPI*)(HANDLE,PROCESSENTRY32*))getsymbol(MODULE_kernel,"Process32First")) &&
			(snap_state.procnext = (BOOL(WINAPI*)(HANDLE,PROCESSENTRY32*))getsymbol(MODULE_kernel,"Process32Next")) ?
			1 : -1;
	if (snap_state.initialized < 0)
		return 0;
	time_getnow(&ftime);
	memcpy(&ts, &ftime, sizeof(ftime));
	if (snap_state.snaphp)
	{
		if (!force && (ts - snap_state.ts) <= PSAGE)
			return 1;
		CloseHandle(snap_state.snaphp);
	}
	if ((snap_state.snaphp = snap_state.snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE)
	{
		snap_state.snaphp = 0;
		logerr(0, "cannot get snapshot handle");
		return 0;
	}
	snap_state.ts = ts;
	snap_state.first = 1;
	snap_state.scan = 0;
	snap_state.proc.dwSize = sizeof(PROCESSENTRY32);
	return 1;
}

static void snap_getinfo(pid_t pid, Pproc_t* pp, struct prpsinfo* pr, char* exe, int exesiz)
{
	DWORD			ntpid = uwin_ntpid(pid);
	DWORD			oid;
	HANDLE			ph;
	char*			cp;
	int			loop;
	int			n;
	int			m;
	BOOL			shamwow;

	if (snap_state.proc.th32ProcessID != ntpid)
	{
		/*
		 * loop through the snapshot to find ntpid
		 * if not found then we break the loop where we started
		 */

		if (!(oid = snap_state.proc.th32ProcessID))
			oid = (DWORD)-1;
		loop = 0;
		for (;;)
		{
			if (snap_state.first)
			{
				snap_state.first = 0;
				n = (*snap_state.procfirst)(snap_state.snaphp, &snap_state.proc);
			}
			else
				n = (*snap_state.procnext)(snap_state.snaphp, &snap_state.proc);
			if (!n)
			{
				snap_dirinit(1);
				if (loop++ || !oid)
					return;
			}
			else if (snap_state.proc.th32ProcessID == ntpid)
				break;
			else if (snap_state.proc.th32ProcessID == oid)
				return;
		}
	}
	if (pp && !PROC_NATIVE(pp))
		n = (int)sfsprintf(cp = exe, exesiz, "%s", pp->prog_name);
	else
	{
		n = uwin_pathmap(snap_state.proc.szExeFile, exe, exesiz, UWIN_W2U);
		if (cp = strrchr(exe, '/'))
			cp++;
		else
			cp = exe;
	}
	if (state.shamwowed && (exesiz - n) >= 4)
	{
		m = 0;
		if ((!pp || !(m = pp->type & (PROCTYPE_32|PROCTYPE_64))) && (ph = OpenProcess(state.process_query, FALSE, ntpid)))
		{
			state.shamwowed(ph, &shamwow);
			closehandle(ph, HT_PROC);
			m = shamwow ? PROCTYPE_32 : PROCTYPE_64;
			if (pp)
				pp->type |= m;
		}
		if (m & PROCTYPE_32)
			strcpy(exe + n, "*32");
	}
	sfsprintf(pr->pr_fname, sizeof(pr->pr_fname), "%s", cp);
	pr->pr_fname[sizeof(pr->pr_fname) - 1] = 0;
	pr->pr_pid = pid;
	pr->pr_ntpid = ntpid;
	pr->pr_pgrp = pid;
	pr->pr_refcount = 0;
	if (streq(pr->pr_fname, "System"))
		pr->pr_ppid = state.system = pid;
	else if (snap_state.proc.th32ParentProcessID)
	{
		/*
		 * the uwin processes form a tree under init
		 * this forces the native processes to be a tree under System
		 */

		if ((ph = OpenProcess(PROCESS_TERMINATE, FALSE, snap_state.proc.th32ParentProcessID)) || GetLastError() != ERROR_INVALID_PARAMETER)
		{
			if (ph)
				closehandle(ph, HT_PROC);
			pr->pr_ppid = uwin_nt2unixpid(snap_state.proc.th32ParentProcessID);
		}
		else
			pr->pr_ppid = state.system;
	}
	else
		pr->pr_ppid = state.system;
	pr->pr_state = snap_map_state(snap_state.proc.dwFlags);
	pr->pr_pri = snap_state.proc.pcPriClassBase;
	pr->pr_lttydev = (unsigned)-1;
	pr->pr_tgrp = pr->pr_sid;
	pr->pr_time = 0;
}

/* === externs === */

int proc_commandline(DWORD pid, struct prpsinfo* pr)
{
	SIZE_T		len;
	DWORD		arg;
	HANDLE		ph;
	HANDLE		th;
	int		r;

	r = 0;
	if (ph = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_QUERY_INFORMATION, FALSE, pid))
	{
		if (th = CreateRemoteThread(ph, NULL, 0, (LPTHREAD_START_ROUTINE)GetCommandLine, 0, 0, &arg))
		{
			WaitForSingleObject(th, 300);
			len = GetExitCodeThread(th, &arg);
			r = len && arg == STILL_ACTIVE && ReadProcessMemory(ph, (void*)arg, pr->pr_psargs, sizeof(pr->pr_psargs), &len);
			closehandle(th, HT_THREAD);
		}
		closehandle(ph, HT_PROC);
	}
	return r;
}

int proc_dirinit(Procdirstate_t* pd)
{
	if (snap_dirinit(1))
		pd->getnext = snap_dirnext;
	else
		return -1;
	return 0;
}

void prpsinfo(pid_t pid, Pproc_t* pp, struct prpsinfo* pr, char* exe, int exesiz)
{
	Pdev_t*		dev;

	if (pp)
		pid = pp->pid;
	ZeroMemory(pr, sizeof(*pr));
	if (snap_dirinit(0))
		snap_getinfo(pid, pp, pr, exe, exesiz);
	if (pp)
	{
		if (pp->console > 0)
		{
			dev = dev_ptr(pp->console);
			pr->pr_lttydev = make_rdev(dev->major, dev->minor);
			pr->pr_tgrp = dev->tgrp;
		}
		else
			pr->pr_lttydev = (dev_t)-1;
		if (PROC_NATIVE(pp))
		{
			pp->pgid = pr->pr_sid;
			pp->ppid = pr->pr_ppid;
			pp->sid = pr->pr_sid;
			pp->state = pr->pr_state;
		}
	}
	else if (Share->Platform != VER_PLATFORM_WIN32_NT || !proc_commandline(pr->pr_ntpid, pr))
		strncpy(pr->pr_psargs, pr->pr_fname, sizeof(pr->pr_psargs));
}
