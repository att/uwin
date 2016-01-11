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
#include	"uwindef.h"
#include	<uwin.h>
#include	"uwin_serve.h"
#include	<stddef.h>
#include	<sys/utsname.h>
#include	<sys/procfs.h>
#include	"procdir.h"
#include	"mnthdr.h"
#include	"nethdr.h"
#include	"ident.h"
#include	"ipchdr.h"
#include	<arpa/inet.h>
#include	<sys/socket.h>
#include	<socket.h>
#include	<netinet/in.h>
#include	"uwinpsapi.h"
#include	<sys/param.h>

#define FMT_stat	"%d (%s) %1(state)s %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %lu %lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n"
#define FMT_statm	"%d %d %d %d %d %d %d\n"
#define FMT_status	"\
Name:    %s\n\
State:   %1(state)s (%(state)s)\n\
Tgid:    %d\n\
Pid:     %d\n\
PPid:    %d\n\
NPid:    %d\n\
Uid:     %d %d %d %d\n\
Gid:     %d %d %d %d\n\
VmSize:  %8d KB\n\
VmLck:   %8d KB\n\
VmRSS:   %8d KB\n\
VmData:  %8d KB\n\
VmStk:   %8d KB\n\
VmExe:   %8d KB\n\
VmLib:   %8d KB\n\
SigPnd:  %08.8x\n\
SigBlk:  %08.8x\n\
SigIgn:  %08.8x\n\
WoW:     0x%02x\n\
"

Pproc_t* proc_locked(pid_t pid)
{
	register Pproc_t*	pp;

	static Pproc_t		proc;

	if (!(pp = proc_getlocked(pid, 0)))
	{
		/* XXX: not a uwin process -- fail here if pid is not a native windows process */
		pp = &proc;
		memset(pp, 0, sizeof(*pp));
		pp->inuse = 1;
		pp->pid = pid;
		pp->ntpid = uwin_ntpid(pid);
	}
	return pp;
}

static ssize_t get_locks(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	int	i;
	int	j;
	int	r;
	Pfd_t*	fdp;
	char	buf[256];

	n = sfsprintf(buf, sizeof(buf), "    START       END    PID TYPE NAME\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	r = w;
	for (i = 0; i < Share->nfiles; i++)
	{
		fdp = &Pfdtab[i];
		if(fdp->refcount >= 0 && fdp->devno > 0 && (fdp->type == FDTYPE_FILE || fdp->type == FDTYPE_TFILE || fdp->type == FDTYPE_XFILE))
		{
			for(j = 0; j < i && Pfdtab[j].devno != fdp->devno; j++);
			if (j == i)
				r += lock_dump(hp, fdp);
		}
	}
	return r;
}

static ssize_t get_ofiles(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	ssize_t	r;
	DWORD	w;
	int	i;
	char* 	name;
	char	buf[256];

	n = sfsprintf(buf, sizeof(buf), "SLOT REFS INDX TYPE      OFLAGS NAME\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	r = w;
	for (i = 0; i < Share->nfiles; i++)
		if (Pfdtab[i].refcount >= 0)
		{
			name = Pfdtab[i].index > 0 ? fdname(i) : "";
			n = sfsprintf(buf, sizeof(buf), "%4d %4d% %4d %(fdtype)s\t%07o\t%s\n", i, Pfdtab[i].refcount+1, Pfdtab[i].index, Pfdtab[i].type, Pfdtab[i].oflag, name);
			WriteFile(hp, buf, (DWORD)n, &w, 0);
			r += w;
		}
	return r;
}

static ssize_t get_devices(Pproc_t* pp, int fid, HANDLE hp)
{
	DWORD	w;
	int	i;
	char	buf[1024];
	char*	p = buf;
	char*	e = &buf[sizeof(buf)];

	p += sfsprintf(p, e - p, "Character devices:\n");
	for (i = 0; i < Share->chardev_size; i++)
		p += sfsprintf(p, e - p, "%3d %s\n", i, Chardev[i].node);
	p += sfsprintf(p, e - p, "\nBlock devices:\n");
	for (i = 0; i < Share->blockdev_size; i++)
		p += sfsprintf(p, e - p, "%3d %s\n", i, Blockdev[i].node);
	WriteFile(hp, buf, (DWORD)(p-buf), &w, 0);
	return w;
}

static ssize_t get_version(Pproc_t* pp, int fid, HANDLE hp)
{
	const char*	cp;
	ssize_t		n;
	DWORD		w;
	char		buf[256];

	cp = IDENT_TEXT(version_id);
	n = sfsprintf(buf, sizeof(buf), "%-.*s\n", IDENT_LENGTH(cp), cp);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uptime(Pproc_t* pp, int fid, HANDLE hp)
{
	FILETIME	f;
	struct timespec	tv;
	ssize_t		n;
	DWORD		w;
	char		buf[256];

	time_diff(&f, 0, &Share->boot);
	unix_time(&f, &tv, 0);
	n = sfsprintf(buf, sizeof(buf), "%d.%.02d\n", tv.tv_sec,  tv.tv_nsec/10000000);
	WriteFile(hp, buf, (DWORD)n,  &w, 0);
	return w;
}

static ssize_t get_cpuinfo(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t		n;
	DWORD		w;
	struct utsname	uts;
	char		buff[256];

	uname(&uts);
	n = sfsprintf(buff, sizeof(buff), "%s\n", uts.machine);
	WriteFile(hp, buff, (DWORD)n, &w, 0);
	return w;
}

static ssize_t disp_uwin_block(Pproc_t* pp, int fid, HANDLE hp)
{
	char	*bp;
	char	*h, *he, *a, *ae;
	char	*nl;
    int		i, offset, n, w;
	char outbuf[80];
	char abuf[22], hbuf[45];

	if (!fid || ((Blocktype[fid]&BLK_MASK)==BLK_FREE))
		return 0;
	bp = block_ptr(fid);
	h = &outbuf[6];
	he = h + 37;
	a = &outbuf[43];
	ae = a + 22;
	nl = hp == state.log ? "\r\n" : "\n";
	n = w = offset = 0;
	memset(outbuf, ' ', sizeof(outbuf));
	sfsprintf(outbuf, sizeof(outbuf), "%04x: ", offset);
	outbuf[n] = ' ';
    for (i=0; i < (int)Share->block_size; i++) {
        h += sfsprintf(h, (he-h), "%02.2x", (*bp&0xff));
		a += sfsprintf(a, (ae-a), "%c", (isprint(*bp&0xff) ?  (*bp) : '.'));
        if ((i%4) == 3)
		{
			*h++ = ' ';
			*a++ = ' ';
		}
		bp++;
        if ((i%16) == 15)
		{
			a += sfsprintf(++a, (ae-a), "%s", nl);
			n = a - outbuf;
			WriteFile(hp, outbuf, (DWORD)n, &n, 0);
			w += n;
			h = &outbuf[6];
			he = h + 37;
			a = &outbuf[43];
			ae = a + 22;	
			offset += 16;
			memset(outbuf, ' ', sizeof(outbuf));
			sfsprintf(outbuf, sizeof(outbuf), "%04x: ", offset);
			outbuf[n] = ' ';
		}
    }
    if (i%16 != 0)
	{
		*a=*h=' ';
		a += sfsprintf(a, (ae-a), "%s", nl);
		n = a - outbuf;
		WriteFile(hp, outbuf, (DWORD)n, &n, 0);
		w += n;
	}
	return w;
}

static ssize_t get_uwin_block(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t		n;
	DWORD		w;
	int		type;
	char*		b;
	char*		e;
	char*		nl;
	Pdev_t*		dp;
	char		buf[1024];

	b = buf;
	e = b + sizeof(buf);
	nl = hp == state.log ? "\r\n" : "\n";
	b += sfsprintf(b, e-b, "%05d ", fid);
	switch (type = (Blocktype[fid]&BLK_MASK))
	{
	case BLK_BUFF:
		b += sfsprintf(b, e-b, "%-6s%s", "BUFF", nl);
		break;
	case BLK_DIR:
		b += sfsprintf(b, e-b, "%-6s %s%s", "DIR", (char*)block_ptr(fid), nl);
		break;
	case BLK_FILE:
		b += sfsprintf(b, e-b, "%-6s %s%s", "FILE", (char*)block_ptr(fid), nl);
		break;
	case BLK_FREE:
		b += sfsprintf(b, e-b, "%-6s%s", "FREE", nl);
		break;
	case BLK_IPC:
		b += sfsprintf(b, e-b, "%-6s%s", "IPC", nl);
		break;
	case BLK_LOCK:
		b += sfsprintf(b, e-b, "%-6s%s", "LOCK", nl);
		break;
	case BLK_MNT:
		b += sfsprintf(b, e-b, "%-6s ", "MNT");
		b += mnt_print(b, e-b, mnt_ptr(fid));
		b += sfsprintf(b, e-b, "%s", nl);
		break;
	case BLK_OFT:
		b += sfsprintf(b, e-b, "%-6s%s", "OFT", nl);
		break;
	case BLK_PDEV:
		dp = (Pdev_t*)proc_ptr(fid);
		b += sfsprintf(b, e-b, "%-6s major=%d minor=%d uname=%s devname=%s%s", "PDEV", dp->major, dp->minor, dp->uname, dp->devname, nl);
		break;
	case BLK_PROC:
		pp = (Pproc_t*)proc_ptr(fid);
		b += sfsprintf(b, e-b, "%-6s ntpid=%x pid=%d ppid=%d %(state)s refs=%d wow=%d name=%s%s%s"
			, "PROC"
			, pp->ntpid
			, pp->pid
			, pp->ppid
			, pp->state
			, pp->inuse
			, pp->wow
			, pp->prog_name, (pp->type & PROCTYPE_ISOLATE) ? ((pp->type & PROCTYPE_32) ? "=32" : "=64") : (pp->type & PROCTYPE_32) ? "*32" : ""
			, nl
			);
		break;
	case BLK_SOCK:
		b += sfsprintf(b, e-b, "%-6s%s", "SOCK", nl);
		break;
	case BLK_VECTOR:
		b += sfsprintf(b, e-b, "%-6s%s", "VECTOR", nl);
		break;
	default:
		b += sfsprintf(b, e-b, "UNKNOWN=%d%s", type, nl);
		break;
	}
	if (n = (int)(b - buf))
		WriteFile(hp, buf, (DWORD)n, &w, 0);
	else
		w = 0;
	return w;
}

ssize_t get_uwin_slots(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	r;
	int n;

	if (fid > 0)
		r = get_uwin_block(pp, fid, hp);
	else
		for (fid = 1, r = 0; fid < Share->nfiles; fid++)
			if ((Blocktype[fid]&BLK_MASK) != BLK_FREE)
				r += get_uwin_block(pp, fid, hp);
	return r;
}

static ssize_t get_uwin_blocksize(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[32];

	n = sfsprintf(buf, sizeof(buf), "%u\n", Share->block_size);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_mem(Pproc_t* pp, int fid, HANDLE hp)
{
	Pdev_t*	pdev = (Pdev_t*)P_CP;
	ssize_t	n;
	DWORD	w;
	char	buf[1024];

	n = sfsprintf(buf, sizeof(buf), "\
%s%s=%d\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
%24s %9u\n\
"
		, state.resource
		, UWIN_SHARE
		, Share->mem_size
		, "Pshmtab", OFFSETOF(Share, Pshmtab)
		, "Pfifotab", OFFSETOF(Share, Pfifotab)
		, "Psidtab", OFFSETOF(Share, Psidtab)
		, "Pfdtab", OFFSETOF(Share, Pfdtab)
		, "Pprochead", OFFSETOF(Share, Pprochead)
		, "Pprocnthead", OFFSETOF(Share, Pprocnthead)
		, "Blocktype", OFFSETOF(Share, Blocktype)
		, "Pproctab", OFFSETOF(Share, Pproctab)
		, "Generation", OFFSETOF(Share, Generation)
		);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_pdev_t(Pproc_t* pp, int fid, HANDLE hp)
{
	Pdev_t*	pdev = (Pdev_t*)pp;
	ssize_t	n;
	DWORD	w;
	char	buf[2*1024];

	n = sfsprintf(buf, sizeof(buf), "\
Pdev_t=%u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
"
		, sizeof(Pdev_t)
		, "count", offsetof(Pdev_t, count), sizeof(pdev->count)
		, "major", offsetof(Pdev_t, major), sizeof(pdev->major)
		, "minor", offsetof(Pdev_t, minor), sizeof(pdev->minor)
		, "DeviceInputHandle", offsetof(Pdev_t, DeviceInputHandle), sizeof(pdev->DeviceInputHandle)
		, "DeviceOutputHandle", offsetof(Pdev_t, DeviceOutputHandle), sizeof(pdev->DeviceOutputHandle)
		, "NewCSB", offsetof(Pdev_t, NewCSB), sizeof(pdev->NewCSB)
		, "BuffFlushEvent", offsetof(Pdev_t, BuffFlushEvent), sizeof(pdev->BuffFlushEvent)
		, "WriteSyncEvent", offsetof(Pdev_t, WriteSyncEvent), sizeof(pdev->WriteSyncEvent)
		, "WriteOverlappedEvent", offsetof(Pdev_t, WriteOverlappedEvent), sizeof(pdev->WriteOverlappedEvent)
		, "ReadOverlappedEvent", offsetof(Pdev_t, ReadOverlappedEvent), sizeof(pdev->ReadOverlappedEvent)
		, "ReadWaitCommEvent", offsetof(Pdev_t, ReadWaitCommEvent), sizeof(pdev->ReadWaitCommEvent)
		, "process", offsetof(Pdev_t, process), sizeof(pdev->process)
		, "write_event", offsetof(Pdev_t, write_event), sizeof(pdev->write_event)
		, "SrcInOutHandle", offsetof(Pdev_t, SrcInOutHandle), sizeof(pdev->SrcInOutHandle)
		, "ReadBuff", offsetof(Pdev_t, ReadBuff), sizeof(pdev->ReadBuff)
		, "Pseudodevice", offsetof(Pdev_t, Pseudodevice), sizeof(pdev->Pseudodevice)
		, "access", offsetof(Pdev_t, access), sizeof(pdev->access)
		, "devpid", offsetof(Pdev_t, devpid), sizeof(pdev->devpid)
		, "tgrp", offsetof(Pdev_t, tgrp), sizeof(pdev->tgrp)
		, "tty", offsetof(Pdev_t, tty), sizeof(pdev->tty)
		, "NumChar", offsetof(Pdev_t, NumChar), sizeof(pdev->NumChar)
		, "cur_phys", offsetof(Pdev_t, cur_phys), sizeof(pdev->cur_phys)
		, "SuspendOutputFlag", offsetof(Pdev_t, SuspendOutputFlag), sizeof(pdev->SuspendOutputFlag)
		, "writecount", offsetof(Pdev_t, writecount), sizeof(pdev->writecount)
		, "NtPid", offsetof(Pdev_t, NtPid), sizeof(pdev->NtPid)
		, "slot", offsetof(Pdev_t, slot), sizeof(pdev->slot)
		, "boffset", offsetof(Pdev_t, boffset), sizeof(pdev->boffset)
		, "started", offsetof(Pdev_t, started), sizeof(pdev->started)
		, "codepage", offsetof(Pdev_t, codepage), sizeof(pdev->codepage)
		, "TabIndex", offsetof(Pdev_t, TabIndex), sizeof(pdev->TabIndex)
		, "MaxRow", offsetof(Pdev_t, MaxRow), sizeof(pdev->MaxRow)
		, "MaxCol", offsetof(Pdev_t, MaxCol), sizeof(pdev->MaxCol)
		, "notitle", offsetof(Pdev_t, notitle), sizeof(pdev->notitle)
		, "SlashFlag", offsetof(Pdev_t, SlashFlag), sizeof(pdev->SlashFlag)
		, "LnextFlag", offsetof(Pdev_t, LnextFlag), sizeof(pdev->LnextFlag)
		, "Cursor_Mode", offsetof(Pdev_t, Cursor_Mode), sizeof(pdev->Cursor_Mode)
		, "state", offsetof(Pdev_t, state), sizeof(pdev->state)
		, "discard", offsetof(Pdev_t, discard), sizeof(pdev->discard)
		, "dsusp", offsetof(Pdev_t, dsusp), sizeof(pdev->dsusp)
		, "uname", offsetof(Pdev_t, uname), sizeof(pdev->uname)
		, "devname", offsetof(Pdev_t, devname), sizeof(pdev->devname)
		, "notused", offsetof(Pdev_t, notused), sizeof(pdev->notused)
		, "round_to_multiple_of_8", offsetof(Pdev_t, round_to_multiple_of_8), sizeof(pdev->round_to_multiple_of_8)
		);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_pproc_t(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[4*1024];

	n = sfsprintf(buf, sizeof(buf), "\
Pproc_t=%u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
"
		, sizeof(Pproc_t)
		, "_forksp", offsetof(Pproc_t, _forksp), sizeof(P_CP->_forksp)
		, "_sigact", offsetof(Pproc_t, _sigact), sizeof(P_CP->_sigact)
		, "_vfork", offsetof(Pproc_t, _vfork), sizeof(P_CP->_vfork)
		, "_fdtab", offsetof(Pproc_t, _fdtab), sizeof(P_CP->_fdtab)
		, "start", offsetof(Pproc_t, start), sizeof(P_CP->start)
		, "ph", offsetof(Pproc_t, ph), sizeof(P_CP->ph)
		, "ph2", offsetof(Pproc_t, ph2), sizeof(P_CP->ph2)
		, "thread", offsetof(Pproc_t, thread), sizeof(P_CP->thread)
		, "sigevent", offsetof(Pproc_t, sigevent), sizeof(P_CP->sigevent)
		, "sevent", offsetof(Pproc_t, sevent), sizeof(P_CP->sevent)
		, "trace", offsetof(Pproc_t, trace), sizeof(P_CP->trace)
		, "etoken", offsetof(Pproc_t, etoken), sizeof(P_CP->etoken)
		, "rtoken", offsetof(Pproc_t, rtoken), sizeof(P_CP->rtoken)
		, "phandle", offsetof(Pproc_t, phandle), sizeof(P_CP->phandle)
		, "fin_thrd", offsetof(Pproc_t, fin_thrd), sizeof(P_CP->fin_thrd)
		, "origph", offsetof(Pproc_t, origph), sizeof(P_CP->origph)
		, "alarmevent", offsetof(Pproc_t, alarmevent), sizeof(P_CP->alarmevent)
		, "stoken", offsetof(Pproc_t, stoken), sizeof(P_CP->stoken)
		, "regdir", offsetof(Pproc_t, regdir), sizeof(P_CP->regdir)
		, "cutime", offsetof(Pproc_t, cutime), sizeof(P_CP->cutime)
		, "cstime", offsetof(Pproc_t, cstime), sizeof(P_CP->cstime)
		, "siginfo", offsetof(Pproc_t, siginfo), sizeof(P_CP->siginfo)
		, "tvirtual", offsetof(Pproc_t, tvirtual), sizeof(P_CP->tvirtual)
		, "treal", offsetof(Pproc_t, treal), sizeof(P_CP->treal)
		, "inuse", offsetof(Pproc_t, inuse), sizeof(P_CP->inuse)
		, "ntpid", offsetof(Pproc_t, ntpid), sizeof(P_CP->ntpid)
		, "wait_threadid", offsetof(Pproc_t, wait_threadid), sizeof(P_CP->wait_threadid)
		, "threadid", offsetof(Pproc_t, threadid), sizeof(P_CP->threadid)
		, "pid", offsetof(Pproc_t, pid), sizeof(P_CP->pid)
		, "ppid", offsetof(Pproc_t, ppid), sizeof(P_CP->ppid)
		, "pgid", offsetof(Pproc_t, pgid), sizeof(P_CP->pgid)
		, "sid", offsetof(Pproc_t, sid), sizeof(P_CP->sid)
		, "uid", offsetof(Pproc_t, uid), sizeof(P_CP->uid)
		, "gid", offsetof(Pproc_t, gid), sizeof(P_CP->gid)
		, "egid", offsetof(Pproc_t, egid), sizeof(P_CP->egid)
		, "umask", offsetof(Pproc_t, umask), sizeof(P_CP->umask)
		, "nchild", offsetof(Pproc_t, nchild), sizeof(P_CP->nchild)
		, "traceflags", offsetof(Pproc_t, traceflags), sizeof(P_CP->traceflags)
		, "ipctab", offsetof(Pproc_t, ipctab), sizeof(P_CP->ipctab)
		, "acp", offsetof(Pproc_t, acp), sizeof(P_CP->acp)
		, "lock_wait", offsetof(Pproc_t, lock_wait), sizeof(P_CP->lock_wait)
		, "usrview", offsetof(Pproc_t, usrview), sizeof(P_CP->usrview)
		, "exitcode", offsetof(Pproc_t, exitcode), sizeof(P_CP->exitcode)
		, "test", offsetof(Pproc_t, test), sizeof(P_CP->test)
		, "tls", offsetof(Pproc_t, tls), sizeof(P_CP->tls)
		, "maxfds", offsetof(Pproc_t, maxfds), sizeof(P_CP->maxfds)
		, "console", offsetof(Pproc_t, console), sizeof(P_CP->console)
		, "next", offsetof(Pproc_t, next), sizeof(P_CP->next)
		, "child", offsetof(Pproc_t, child), sizeof(P_CP->child)
		, "sibling", offsetof(Pproc_t, sibling), sizeof(P_CP->sibling)
		, "group", offsetof(Pproc_t, group), sizeof(P_CP->group)
		, "parent", offsetof(Pproc_t, parent), sizeof(P_CP->parent)
		, "ntnext", offsetof(Pproc_t, ntnext), sizeof(P_CP->ntnext)
		, "curdir", offsetof(Pproc_t, curdir), sizeof(P_CP->curdir)
		, "rootdir", offsetof(Pproc_t, rootdir), sizeof(P_CP->rootdir)
		, "wow", offsetof(Pproc_t, wow), sizeof(P_CP->wow)
		, "posixapp", offsetof(Pproc_t, posixapp), sizeof(P_CP->posixapp)
		, "inexec", offsetof(Pproc_t, inexec), sizeof(P_CP->inexec)
		, "state", offsetof(Pproc_t, state), sizeof(P_CP->state)
		, "notify", offsetof(Pproc_t, notify), sizeof(P_CP->notify)
		, "console_child", offsetof(Pproc_t, console_child), sizeof(P_CP->console_child)
		, "debug", offsetof(Pproc_t, debug), sizeof(P_CP->debug)
		, "admin", offsetof(Pproc_t, admin), sizeof(P_CP->admin)
		, "priority", offsetof(Pproc_t, priority), sizeof(P_CP->priority)
		, "procdir", offsetof(Pproc_t, procdir), sizeof(P_CP->procdir)
		, "type", offsetof(Pproc_t, type), sizeof(P_CP->type)
		, "argsize", offsetof(Pproc_t, argsize), sizeof(P_CP->argsize)
		, "cur_limits", offsetof(Pproc_t, cur_limits), sizeof(P_CP->cur_limits)
		, "max_limits", offsetof(Pproc_t, max_limits), sizeof(P_CP->max_limits)
		, "shmbits", offsetof(Pproc_t, shmbits), sizeof(P_CP->shmbits)
		, "slotblocks", offsetof(Pproc_t, slotblocks), sizeof(P_CP->slotblocks)
		, "fdslots", offsetof(Pproc_t, fdslots), sizeof(P_CP->fdslots)
		, "cmd_line", offsetof(Pproc_t, cmd_line), sizeof(P_CP->cmd_line)
		, "prog_name", offsetof(Pproc_t, prog_name), sizeof(P_CP->prog_name)
		, "args", offsetof(Pproc_t, args), sizeof(P_CP->args)
		);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_pshare_t(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[4*1024];

	n = sfsprintf(buf, sizeof(buf), "\
Uwin_share_t=%u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
%24s %4u %4u\n\
"
		, sizeof(Uwin_share_t)
		, "magic", offsetof(Uwin_share_t, magic), sizeof(Share->magic)
		, "version", offsetof(Uwin_share_t, version), sizeof(Share->version)
		, "block_size", offsetof(Uwin_share_t, block_size), sizeof(Share->block_size)
		, "root_offset", offsetof(Uwin_share_t, root_offset), sizeof(Share->root_offset)
		, "init_ntpid", offsetof(Uwin_share_t, init_ntpid), sizeof(Share->init_ntpid)
		, "init_bits", offsetof(Uwin_share_t, init_bits), sizeof(Share->init_bits)
		, "lockmutex", offsetof(Uwin_share_t, lockmutex), sizeof(Share->lockmutex)
		, "dcache", offsetof(Uwin_share_t, dcache), sizeof(Share->dcache)
		, "init_sock", offsetof(Uwin_share_t, init_sock), sizeof(Share->init_sock)
		, "init_pipe", offsetof(Uwin_share_t, init_pipe), sizeof(Share->init_pipe)
		, "init_handle", offsetof(Uwin_share_t, init_handle), sizeof(Share->init_handle)
		, "cycle0", offsetof(Uwin_share_t, cycle0), sizeof(Share->cycle0)
		, "boot", offsetof(Uwin_share_t, boot), sizeof(Share->boot)
		, "start", offsetof(Uwin_share_t, start), sizeof(Share->start)
		, "pidxor", offsetof(Uwin_share_t, pidxor), sizeof(Share->pidxor)
		, "pidbase", offsetof(Uwin_share_t, pidbase), sizeof(Share->pidbase)
		, "pidlock", offsetof(Uwin_share_t, pidlock), sizeof(Share->pidlock)
		, "maxtime", offsetof(Uwin_share_t, maxtime), sizeof(Share->maxtime)
		, "maxltime", offsetof(Uwin_share_t, maxltime), sizeof(Share->maxltime)
		, "argmax", offsetof(Uwin_share_t, argmax), sizeof(Share->argmax)
		, "nmutex", offsetof(Uwin_share_t, nmutex), sizeof(Share->nmutex)
		, "dcount", offsetof(Uwin_share_t, dcount), sizeof(Share->dcount)
		, "locktid", offsetof(Uwin_share_t, locktid), sizeof(Share->locktid)
		, "vforkpid", offsetof(Uwin_share_t, vforkpid), sizeof(Share->vforkpid)
		, "block", offsetof(Uwin_share_t, block), sizeof(Share->block)
		, "top", offsetof(Uwin_share_t, top), sizeof(Share->top)
		, "mount_tbsize", offsetof(Uwin_share_t, mount_tbsize), sizeof(Share->mount_tbsize)
		, "otop", offsetof(Uwin_share_t, otop), sizeof(Share->otop)
		, "backlog", offsetof(Uwin_share_t, backlog), sizeof(Share->backlog)
		, "init_thread_id", offsetof(Uwin_share_t, init_thread_id), sizeof(Share->init_thread_id)
		, "fdtab_index", offsetof(Uwin_share_t, fdtab_index), sizeof(Share->fdtab_index)
		, "adjust", offsetof(Uwin_share_t, adjust), sizeof(Share->adjust)
		, "old_root_offset", offsetof(Uwin_share_t, old_root_offset), sizeof(Share->old_root_offset)
		, "nblockops", offsetof(Uwin_share_t, nblockops), sizeof(Share->nblockops)
		, "nblocks", offsetof(Uwin_share_t, nblocks), sizeof(Share->nblocks)
		, "nfiles", offsetof(Uwin_share_t, nfiles), sizeof(Share->nfiles)
		, "nprocs", offsetof(Uwin_share_t, nprocs), sizeof(Share->nprocs)
		, "old_block_size", offsetof(Uwin_share_t, old_block_size), sizeof(Share->old_block_size)
		, "ihandles", offsetof(Uwin_share_t, ihandles), sizeof(Share->ihandles)
		, "blksize", offsetof(Uwin_share_t, blksize), sizeof(Share->blksize)
		, "mem_size", offsetof(Uwin_share_t, mem_size), sizeof(Share->mem_size)
		, "share_size", offsetof(Uwin_share_t, share_size), sizeof(Share->share_size)
		, "drivetime", offsetof(Uwin_share_t, drivetime), sizeof(Share->drivetime)
		, "counters", offsetof(Uwin_share_t, counters), sizeof(Share->counters)
		, "lockwait", offsetof(Uwin_share_t, lockwait), sizeof(Share->lockwait)
		, "ipc_limits", offsetof(Uwin_share_t, ipc_limits), sizeof(Share->ipc_limits)
		, "Maxsid", offsetof(Uwin_share_t, Maxsid), sizeof(Share->Maxsid)
		, "Platform", offsetof(Uwin_share_t, Platform), sizeof(Share->Platform)
		, "debug", offsetof(Uwin_share_t, debug), sizeof(Share->debug)
		, "base_drive", offsetof(Uwin_share_t, base_drive), sizeof(Share->base_drive)
		, "movex", offsetof(Uwin_share_t, movex), sizeof(Share->movex)
		, "linkno", offsetof(Uwin_share_t, linkno), sizeof(Share->linkno)
		, "nfifos", offsetof(Uwin_share_t, nfifos), sizeof(Share->nfifos)
		, "nsids", offsetof(Uwin_share_t, nsids), sizeof(Share->nsids)
		, "init_slot", offsetof(Uwin_share_t, init_slot), sizeof(Share->init_slot)
		, "blockdev_index", offsetof(Uwin_share_t, blockdev_index), sizeof(Share->blockdev_index)
		, "blockdev_size", offsetof(Uwin_share_t, blockdev_size), sizeof(Share->blockdev_size)
		, "chardev_index", offsetof(Uwin_share_t, chardev_index), sizeof(Share->chardev_index)
		, "chardev_size", offsetof(Uwin_share_t, chardev_size), sizeof(Share->chardev_size)
		, "mount_table", offsetof(Uwin_share_t, mount_table), sizeof(Share->mount_table)
		, "child_max", offsetof(Uwin_share_t, child_max), sizeof(Share->child_max)
		, "uid_index", offsetof(Uwin_share_t, uid_index), sizeof(Share->uid_index)
		, "name_index", offsetof(Uwin_share_t, name_index), sizeof(Share->name_index)
		, "pipe_times", offsetof(Uwin_share_t, pipe_times), sizeof(Share->pipe_times)
		, "block_table", offsetof(Uwin_share_t, block_table), sizeof(Share->block_table)
		, "nullblock", offsetof(Uwin_share_t, nullblock), sizeof(Share->nullblock)
		, "lastslot", offsetof(Uwin_share_t, lastslot), sizeof(Share->lastslot)
		, "rootdirsize", offsetof(Uwin_share_t, rootdirsize), sizeof(Share->rootdirsize)
		, "sockstate", offsetof(Uwin_share_t, sockstate), sizeof(Share->sockstate)
		, "affinity", offsetof(Uwin_share_t, affinity), sizeof(Share->affinity)
		, "update_immediate", offsetof(Uwin_share_t, update_immediate), sizeof(Share->update_immediate)
		, "bitmap", offsetof(Uwin_share_t, bitmap), sizeof(Share->bitmap)
		, "saveblk", offsetof(Uwin_share_t, saveblk), sizeof(Share->saveblk)
		, "pid_counters", offsetof(Uwin_share_t, pid_counters), sizeof(Share->pid_counters)
		, "pid_shift", offsetof(Uwin_share_t, pid_shift), sizeof(Share->pid_shift)
		, "lockslot", offsetof(Uwin_share_t, lockslot), sizeof(Share->lockslot)
		, "lockdone", offsetof(Uwin_share_t, lockdone), sizeof(Share->lockdone)
		, "nlock", offsetof(Uwin_share_t, nlock), sizeof(Share->nlock)
		, "olockslot", offsetof(Uwin_share_t, olockslot), sizeof(Share->olockslot)
		, "lastlline", offsetof(Uwin_share_t, lastlline), sizeof(Share->lastlline)
		, "mutex_offset", offsetof(Uwin_share_t, mutex_offset), sizeof(Share->mutex_offset)
		, "init_state", offsetof(Uwin_share_t, init_state), sizeof(Share->init_state)
		, "stack_trace", offsetof(Uwin_share_t, stack_trace), sizeof(Share->stack_trace)
		, "case_sensitive", offsetof(Uwin_share_t, case_sensitive), sizeof(Share->case_sensitive)
		, "no_shamwow", offsetof(Uwin_share_t, no_shamwow), sizeof(Share->no_shamwow)
		, "inlog", offsetof(Uwin_share_t, inlog), sizeof(Share->inlog)
		, "lastline", offsetof(Uwin_share_t, lastline), sizeof(Share->lastline)
		, "lastfile", offsetof(Uwin_share_t, lastfile), sizeof(Share->lastfile)
		, "lastlfile", offsetof(Uwin_share_t, lastlfile), sizeof(Share->lastlfile)
		);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_resource(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[1021024];

	n = sfsprintf(buf, sizeof(buf), "%s\n", state.resource);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_uwin_uptime(Pproc_t* pp, int fid, HANDLE hp)
{
	FILETIME	f;
	struct timespec	tv;
	ssize_t		n;
	DWORD		w;
	char		buf[256];

	time_diff(&f, 0, &Share->start);
	unix_time(&f, &tv, 0);
	n = sfsprintf(buf, sizeof(buf), "%d.%.02d\n", tv.tv_sec,  tv.tv_nsec/10000000);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

/* For lsof(1M) and fuser(1M) */
static ssize_t get_mounts(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t n;
	ssize_t r;
	DWORD	w;
	void*	mnt;
	Mnt_t*	ent;
	char	buf[1024];

	if (!(mnt = mntopen(0, "r")))
		return -1;
	r = 0;
	while (ent = mntread(mnt))
	{
		n= sfsprintf(buf, sizeof(buf), "%s on %s type %s (%s)\n", ent->fs, ent->dir, ent->type, ent->options);
		WriteFile(hp, buf, (DWORD)n, &w, 0);
		r += w;
	}
	mntclose(mnt);
	return r;
}

static ssize_t get_pid_slots(Pproc_t* pp, int fid, HANDLE hp)
{
	DWORD	w;
	int	i;
	int	r;
	char	buf[2*1024+1];
	char*	b = buf;
	char*	e = b + sizeof(buf) - 1;

	b += sfsprintf(b, e-b, "%d\t", pp->pid);
	for (i = 0; i < pp->maxfds; i++)
		if ((r = pp->fdslots[i]) >= 0)
			b += sfsprintf(b, e-b, "%d\t", r);
	if (b > buf)
		*b++ = '\n';
	WriteFile(hp, buf, (DWORD)(b-buf), &w, 0);
	return w;
}

static ssize_t get_stat(Pproc_t* pp, int fid, HANDLE hp)
{
	struct timespec	tv;
	ssize_t		n;
	DWORD		w;
	char		buf[1024];

	unix_time(&Share->boot, &tv, 1);
	n = sfsprintf(buf, sizeof(buf), "btime %lu\n", tv.tv_sec);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_swaps(Pproc_t* pp, int fid, HANDLE hp)
{
	char	buf[1024];
	ssize_t	n;
	DWORD	w;

	n = 0;
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

#define minor(dev)	(dev&0xf)

static ssize_t txt_pid_fd(Pproc_t* pp, int fd, char* buf, int size)
{
	char*	cp = 0;
	Pfd_t*	fdp;
	int	n;
	char	tmp[PATH_MAX];

	if (!pp)
		return sfsprintf(buf, size, "%d", fd);
	if (fd < 0 || fd >= pp->maxfds || !isfdvalid(pp, fd))
		return sfsprintf(buf, size, "[INVALID:fd=%d]", fd);
	fdp = &Pfdtab[pp->fdslots[fd]];
	switch (fdp->type)
	{
	case FDTYPE_FILE:
	case FDTYPE_TFILE:
	case FDTYPE_XFILE:
		cp = tmp;
		n = uwin_pathmap(fdname(pp->fdslots[fd]), cp, sizeof(tmp), UWIN_W2U);
		/* dos device name */
		if (cp[0] == '/' && cp[1] == '/')
			return sfsprintf(buf, size, "/dev%s", strrchr(cp, '/'));
		break;
	case FDTYPE_CONSOLE:
	case FDTYPE_MODEM:
	case FDTYPE_PTY:
	case FDTYPE_TTY:
		if (proc_tty(pp, fd, buf, size))
			goto unknown;
		return (int)strlen(buf);
	case FDTYPE_PIPE:
	case FDTYPE_EPIPE:
	case FDTYPE_NPIPE:
	case FDTYPE_FIFO:
	case FDTYPE_EFIFO:
	case FDTYPE_SOCKET:
	case FDTYPE_DIR:
	case FDTYPE_FULL:
		if (!(cp = fdname(file_slot(fdp))))
			goto unknown;
		n = (int)strlen(cp);
		break;
	case FDTYPE_NULL:
		return sfsprintf(buf, size, "/dev/null");
	case FDTYPE_CONNECT_UNIX:
	case FDTYPE_CONNECT_INET:
		if (!(cp = fdname(file_slot(fdp))) || !*cp)
			return sfsprintf(buf, size, "socket[%x;%x]", file_slot(fdp), fdp->devno);
		break;
	case FDTYPE_TAPE:
		return sfsprintf(buf, size, "/dev/mt%d", minor(fdp->devno));
	case FDTYPE_FLOPPY:
		return sfsprintf(buf, size, "/dev/fd%d", minor(fdp->devno));
	case FDTYPE_DGRAM:
		if (!(cp = fdname(file_slot(fdp))) || !*cp)
			return sfsprintf(buf, size, "dgram[%x%x]", file_slot(fdp), fdp->devno);
		break;
	case FDTYPE_LP:
		/* differentate between lp and lp? */
		return sfsprintf(buf, size, "/dev/lp%d", minor(fdp->devno));
	case FDTYPE_ZERO:
		return sfsprintf(buf, size, "/dev/zero");
	case FDTYPE_NETDIR:
		return sfsprintf(buf, size, "/dev/netdir");
	case FDTYPE_AUDIO:
		return sfsprintf(buf, size, "/dev/audio");
	case FDTYPE_RANDOM:
		return sfsprintf(buf, size, "/dev/%srandom", minor(fdp->devno) == 1 ? "" : "u");
	case FDTYPE_MEM:
		return sfsprintf(buf, size, "/dev/mem");
	case FDTYPE_NONE:
		return sfsprintf(buf, size, "[NONE]");
	case FDTYPE_WINDOW:
		return sfsprintf(buf, size, "/dev/window");
	case FDTYPE_CLIPBOARD:
		return sfsprintf(buf, size, "/dev/clipboard");
	case FDTYPE_MOUSE:
		return sfsprintf(buf, size, "/dev/mouse");
	default:
	unknown:
		return sfsprintf(buf, size, "[UNKNOWN:type=%(fdtype)s;slot=%d;name=%s]", fdp->type, file_slot(fdp), fdname(file_slot(fdp)));
	}
	return uwin_pathmap(cp, buf, size, UWIN_W2U);
}

static ssize_t txt_pid_cwd(Pproc_t* pp, int fid, char* buf, int bufsz)
{
	char*	cp;
	ssize_t	n;

	if (pp->curdir >= 0 && (cp = fdname(pp->curdir)))
	{
		n = uwin_pathmap(cp, buf, bufsz, UWIN_W2U);
		if (n > 1 && buf[n-1] == '/')
			buf[--n] = 0;
	}
	else 
		n = sfsprintf(buf, bufsz, ".");
	return n;
}

static ssize_t txt_pid_root(Pproc_t* pp, int fid, char* buf, int bufsz)
{
	char*		cp;
	Mount_t*	mp;
	ssize_t		n; 

	if (pp->rootdir >= 0 && (cp = fdname(pp->rootdir)) || pp->rootdir != -1 && (cp = state.root))
	{
		n = uwin_pathmap(cp, buf, bufsz-1, UWIN_W2U);
		if (n > 1 && buf[n-1] == '/')
			buf[--n] = 0;
	}
	else if (mp = mount_look(0, 0, 0, 0))
		n = uwin_pathmap(mnt_onpath(mp), buf, bufsz-1, UWIN_W2U);
	else
		n = sfsprintf(buf, bufsz, "//");
	return n;
}

static ssize_t txt_self(Pproc_t* pp, int fid, char* buf, int size)
{
	return sfsprintf(buf, size, "%d", getpid());
}

static void proc_uid(Pproc_t *pp, uid_t *uid, uid_t *euid)
{
	if (!pp->etoken)
		*uid = *euid = pp->uid;
	else
	{
		*uid = sid_to_unixid(sid(SID_UID));
		*euid = sid_to_unixid(sid(SID_EUID));
	}
}

static void proc_gid(Pproc_t *pp, gid_t *gid, gid_t *egid)
{
	if (pp->gid != (gid_t)-1)
		*gid = *egid = pp->gid;
	else
	{
		*gid = sid_to_unixid(sid(SID_GID));
		*egid = sid_to_unixid(sid(SID_EGID));
	}
}

int ntpid2pid(int ntpid)
{
	Pproc_t*	pp;
	int		pid;

	if (pp = proc_ntgetlocked(ntpid, 0))
	{
		pid = pp->pid;
		proc_release(pp);
	}
	else
		pid = -1;
	return pid;
}

int ntpid2uid(int ntpid)
{
	Pproc_t*	pp;
	uid_t		uid;
	uid_t		euid;

	uid = -1;
	if (pp = proc_ntgetlocked(ntpid,0))
	{
		proc_uid(pp, &uid, &euid);
		proc_release(pp);
	}
	return uid;
}

static ssize_t get_net_tcp(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[512];

	n = sfsprintf(buf, sizeof(buf), "Slot Local_address Remote_address st tx_queue rx_queue timeout  ntpid    pid uid\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w + get_net_tcpipv4(pp, fid, hp);
}

static ssize_t get_net_tcp6(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[512];

	n = sfsprintf(buf, sizeof(buf), "Slot Local_address                         Remote_address                        st tx_queue rx_queue timeout  ntpid    pid uid\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w + get_net_tcpipv6(pp, fid, hp);
}

static ssize_t get_net_udp(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[512];

	n = sfsprintf(buf, sizeof(buf), "Slot Local_address Remote_address st tx_queue rx_queue timeout  ntpid    pid uid\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w + get_net_udpipv4(pp, fid, hp);
}

static ssize_t get_net_udp6(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[512];

	n = sfsprintf(buf, sizeof(buf), "Slot Local_address                         Remote_address                        st tx_queue rx_queue timeout  ntpid    pid uid\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w + get_net_udpipv6(pp, fid, hp);
}

static ssize_t get_net_unix(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	ssize_t	r;
	DWORD	w;
	int	i;
	char*	name;
	char	buf[256];

	n = sfsprintf(buf, sizeof(buf), " Num RefCount Protocol Flags    Type St Path\n");
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	r = w;
	for (i = 0; i < Share->nfiles; i++)
		if (Pfdtab[i].refcount >= 0 && (Pfdtab[i].type == FDTYPE_SOCKET || Pfdtab[i].type == FDTYPE_CONNECT_UNIX))
		{
			name = Pfdtab[i].index > 0 ? fdname(i) : "";
			n = sfsprintf(buf, sizeof(buf), "%4d %8d %8d  %4d %07o %2d %s\n", i, Pfdtab[i].refcount+1, Pfdtab[i].type, 0, 0, 0, name);
			WriteFile(hp, buf, (DWORD)n, &w, 0);
			r += w;
		}
	return r;
}

static ssize_t get_pid_cmdline(Pproc_t* pp, int fid, HANDLE hp)
{
	struct prpsinfo	pr;
	char*		s;
	ssize_t		n;
	DWORD		w;
	char		cmd[PATH_MAX+1];

	if (n = pp->argsize)
		s = pp->args;
	else
	{
		s = cmd;
		prpsinfo(0, pp, &pr, s, sizeof(cmd) - 1);
		n = strlen(s);
		if (n > 3 && s[n-1] == '2' && s[n-2] == '3' && s[n-3] == '*')
			n -= 3;
		s[n++] = 0;
	}
	WriteFile(hp, s, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_debug(Pproc_t* pp, int fid, HANDLE hp)
{
	char*	s;
	DWORD	w;
	int	n;
	char	buf[64];

	n = pp->debug;
	s = buf;
	if (n & LOG_DEV)
		*s++ = 'd';
	if (n & LOG_IO)
		*s++ = 'i';
	if (n & LOG_PROC)
		*s++ = 'p';
	if (n & LOG_REG)
		*s++ = 'r';
	if (n & LOG_SECURITY)
		*s++ = 's';
	if (!Share->stack_trace)
		*s++ = 'E';
	if (Share->backlog < 0)
		*s++ = 'W';
	s += sfsprintf(s, &buf[sizeof(buf)] - s, "%u", n & LOG_LEVEL);
	if (n = pp->test)
	{
		*s++ = 't';
		s += sfsprintf(s, &buf[sizeof(buf)] - s, "0x%x", n);
	}
	*s++ = '\n';
	WriteFile(hp, buf, (DWORD)(s-buf), &w, 0);
	return w;
}

static ssize_t set_pid_debug(Pproc_t* pp, int fid, char* buf, size_t size)
{
	char*	s;
	char*	t;
	char*	e;
	int	n;
	char	num[32];

	s = buf;
	e = s + size;
	n = 0;
	while (s < e)
	{
		switch (*s++)
		{
		case 'd':
		case 'D':
			n |= LOG_DEV;
			continue;
		case 'e':
			Share->stack_trace = 1;
			continue;
		case 'E':
			Share->stack_trace = 0;
			continue;
		case 'i':
		case 'I':
			n |= LOG_IO;
			continue;
		case 'p':
		case 'P':
			n |= LOG_PROC;
			continue;
		case 'r':
		case 'R':
			n |= LOG_REG;
			continue;
		case 's':
		case 'S':
			n |= LOG_SECURITY;
			continue;
		case 't':
		case 'T':
			t = num;
			while (s < e && t < &num[sizeof(num)-1])
				*t++ = *s++;
			*t = 0;
			pp->test = (unsigned short)strtol(num, 0, 0);
			break;
		case 'w':
			if (Share->backlog < 0)
				Share->backlog = 0;
			continue;
		case 'W':
			Share->backlog = -1;
			continue;
		case '0': /* redundant, but avoids an I/O error when clearing debug */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			n += *(s - 1) - '0';
			continue;
		}
		break;
	}
	pp->debug = n;
	return (ssize_t)(s - buf);
}

static ssize_t txt_pid_exe(Pproc_t* pp, int fid, char* buf, int sz)
{
	ssize_t		n = 0;
	HANDLE		ph;
	Psapi_t*	psapi;
	char		fbuf[PATH_MAX];

	if ((psapi = init_psapi()) && (ph = OpenProcess(state.process_query, FALSE, pp->ntpid)))
	{
		if (psapi->GetProcessImageFileName(ph, fbuf, sizeof(fbuf)) > 0)
			n = uwin_pathmap(fbuf, buf, sz, fid ? UWIN_W2U : UWIN_U2W);
		else
			logerr(0, "GetProcessImageFilename");
		closehandle(ph, HT_PROC);
	}
	else if (GetLastError() != ERROR_ACCESS_DENIED)
		logerr(0, "OpenProcess(%x) failed", pp->ntpid);
	if (n <= 0)
		n = sfsprintf(buf, sz, "<unknown>");
	return n;
}

static ssize_t get_pid_exename(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[PATH_MAX+1];

	n = txt_pid_exe(pp, -1, buf, sizeof(buf)-1);
	buf[n++] = '\n';
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_winexename(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[PATH_MAX+1];

	n = txt_pid_exe(pp, 0, buf, sizeof(buf)-1);
	buf[n++] = '\n';
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_gid(Pproc_t* pp, int fid, HANDLE hp)
{
	gid_t	gid;
	ssize_t	n;
	DWORD	w;
	char	buf[64];

	proc_gid(pp, &gid, &(gid_t)n);
	n = sfsprintf(buf, sizeof(buf), "%d\n", gid);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_pgid(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[64];

	n = sfsprintf(buf, sizeof(buf), "%d\n", pp->pgid);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_ppid(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[64];

	n = sfsprintf(buf, sizeof(buf), "%d\n", pp->ppid);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_sid(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[64];

	n = sfsprintf(buf, sizeof(buf), "%d\n", pp->sid);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

static ssize_t get_pid_winpid(Pproc_t* pp, int fid, HANDLE hp)
{
	ssize_t	n;
	DWORD	w;
	char	buf[64];

	n = sfsprintf(buf, sizeof(buf), "%d\n", pp->ntpid);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return w;
}

#define alarm_to_jiffies(a)	(ULONG)((a).remain - GetTickCount())
#define start_to_jiffies(s)	(ULONG)(((*(ULONGLONG*)&(s))-(*(ULONGLONG*)&Share->boot))/(FZ/HZ))

#define siginfo_to_pending(p)	(sigispending((p)))
#define siginfo_to_blocked(p)	((p)->siginfo.sigmask)
#define siginfo_to_ignore(p)	((p)->siginfo.sigign)
#define siginfo_to_caught(p)	((p)->siginfo.siggot)

/*
 * For lsof(1M) and fuser(1M), prtstat(1M), and killall(1M)
 * pid=%d comm=(%s) state=%c ppid=%d pgrp=%d session=%d tty_nr=%d tpgid=%d \
 * flags=%lu minflt=%lu cminflg=%lu majflt=%lu cmajflt=%lu utime=%lu stime=%lu
 * cutime=%ld cstime=%ld priority=%ld nice=%ld num_threads=%ld itrealvalue=%ld
 * starttime=%lu vsize=%lu rss=%ld rlim=%lu startcode=%lu endcode=%lu
 * startstack=%lu kstkesp=%lu kstkeip=%lu signal=%lu blocked=%lu sigignore=%lu
 * sigcatch=%lu wchan=%lu nswap=%lu cnswap=%lu exit_signal=%d processor=%d
 * rt_priority=%lu policy=%lu
 */

static ssize_t get_pid_stat(Pproc_t* pp, int fid, HANDLE hp)
{
	HANDLE		ph;
	Psapi_t*	psapi;
	ssize_t		n;
	DWORD		w;
	struct tms	ptime;
	struct prpsinfo	pr;
	unsigned long	startcode, endcode, startstack, kstkesp, kstkeip;
	long		nice, num_threads, processor, minflt; 
	char		buf[2*PATH_MAX];
	char		cmd[PATH_MAX];

	/* don't look for values of these parameters for lsof */
	startcode = endcode = startstack = kstkesp = kstkeip = 0UL;
	nice = processor = 0;
	/* num_threads should be the pthread count, not windows threads */
	num_threads = 1;
	proc_times(pp, &ptime);
	prpsinfo(0, pp, &pr, cmd, sizeof(cmd));
	minflt = 0;
	if ((psapi = init_psapi()) && (ph = OpenProcess(state.process_query|PROCESS_VM_READ, FALSE, pp->ntpid)))
	{
		PROCESS_MEMORY_COUNTERS_EX	mem;

		mem.cb = sizeof(mem);
		if (psapi->GetProcessMemoryInfo(ph, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem)))
			minflt = mem.PageFaultCount;
		closehandle(ph, HT_PROC);
	}
	n = sfsprintf(buf, sizeof(buf), FMT_stat
		, pp->pid
		, cmd
		, pp->state
		, pp->ppid
		, pp->pgid
		, pp->sid
		, pr.pr_lttydev
		, pp->sid
		, 0 /* flags */
		, minflt
		, 0 /* cminflt */
		, 0 /* majflt */
		, 0 /* cmajflt */
		, (unsigned long)ptime.tms_utime
		, (unsigned long)ptime.tms_stime
		, pp->cutime
		, pp->cstime
		, (unsigned long)pr.pr_pri
		, nice
		, num_threads
		, alarm_to_jiffies(pp->treal)
		, start_to_jiffies(pp->start)
		, (unsigned long)pr.pr_size
		, (unsigned long)pr.pr_rssize
		, 4294967295 /* rlim */
		, startcode
		, endcode
		, startstack
		, 0 /* kstkesp */
		, 0 /* kstkeip */
		, siginfo_to_pending(pp)
		, siginfo_to_blocked(pp)
		, siginfo_to_ignore(pp)
		, siginfo_to_caught(pp)
		, pr.pr_wchan
		, 0 /* nswap */
		, 0 /* cnswap */
		, SIGCLD
		, processor
		, 0 /* rt_priority */
		, 0 /* policy */
		, 0 /* delayacct_blkio_ticks */
		, 0 /* pad */
		, 0 /* pad */
		, 0 /* pad */
		, (unsigned long)(*(ULONGLONG*)&Share->boot / FZ) /* pad */
		, (unsigned long)(*(ULONGLONG*)&pp->start / FZ) /* pad */
		, pp->ntpid
	);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return (ssize_t)w;
}

static char* fmt_mode(int mode)
{
	static char	buf[5];

	buf[0] = (mode&S_IRUSR) ? 'r' : '-';
	buf[1] = (mode&S_IWUSR) ? 'w' : '-';
	buf[2] = (mode&S_IXUSR) ? 'x' : '-';
	buf[3] = 'p';
	return buf;
}

#define DEV_MAJOR(d)	(((d)>>8)&0xff)
#define DEV_MINOR(d)	((d)&0xff)

static ssize_t get_pid_enum_modules(Pproc_t* proc, int fid, HANDLE hp, int only_libs)
{
	HMODULE		mods[1024];
	HANDLE		ph;
	MODULEINFO	minfo;
	DWORD		all_mods;
	DWORD		w;
	int		i;
	int		r;
	ssize_t		n;
	ssize_t		t = 0;
	Psapi_t*	psapi;
	struct stat	status;
	char		buf[PATH_MAX];
	char		mod_name[PATH_MAX];
	char		path[PATH_MAX];

	if ((psapi = init_psapi()) && (ph = OpenProcess(state.process_query|PROCESS_VM_READ, FALSE, proc->ntpid)))
	{
		if (psapi->EnumProcessModules(ph, mods, sizeof(mods), &all_mods))
		{
			for (i = 0; i < (int)(all_mods/sizeof(HMODULE)); i++)
			{
				mod_name[0] = 0;
				memset((void*)&minfo, 0, sizeof(minfo));
				psapi->GetModuleFileNameEx(ph,mods[i],mod_name,sizeof(mod_name));
				if (only_libs)
				{
					r = (int)strlen(mod_name);
					/* Verify a .dll or .lib */
					if (mod_name[r-4] != '.')
						continue;
					if (mod_name[r-3] != 'd' && mod_name[r-3] != 'D' && mod_name[r-3] != 'l' && mod_name[r-3] != 'L')
						continue;
					if (mod_name[r-2] != 'l' && mod_name[r-2] != 'L' && mod_name[r-2] != 'i' && mod_name[r-2] != 'I')
						continue;
					if (mod_name[r-1] != 'l' && mod_name[r-1] != 'L' && mod_name[r-1] != 'b' && mod_name[r-1] != 'B')
						continue;
				}
				psapi->GetModuleInformation(ph, mods[i], &minfo, sizeof(minfo));
				uwin_pathmap(mod_name, path, sizeof(path), UWIN_W2U);
				stat(path, &status);
				n = sfsprintf(buf, sizeof(buf), "%8.8x-%8.8x %s %8.8x %2.2d:%2.2d %10.10lu %s\n", minfo.lpBaseOfDll,((DWORD)minfo.lpBaseOfDll+minfo.SizeOfImage), fmt_mode(status.st_mode), minfo.EntryPoint, DEV_MAJOR(status.st_dev), DEV_MINOR(status.st_dev), status.st_ino, path);
				WriteFile(hp, buf, (DWORD)n, &w, 0);
				t += w;
			}
		}
		else
			logerr(0, "get_pid_enum_modules EnumProcessModules failed");
		closehandle(ph, HT_PROC);
	}
	else
		logerr(0, "get_pid_enum_modules access to pid=%x-%d", proc->ntpid, proc->pid);
	return t;
}

static ssize_t get_pid_lib(Pproc_t *proc, int fid, HANDLE hp)
{
	return get_pid_enum_modules(proc, fid, hp, 1);
}

static ssize_t get_pid_maps(Pproc_t* proc, int fid, HANDLE hp)
{
	return get_pid_enum_modules(proc, fid, hp, 0);
}

static ssize_t get_pid_statm(Pproc_t* pp, int fid, HANDLE hp)
{
	int	i, r;
	ssize_t	n;
	DWORD	w;
	char	buf[64];
	int vmsize, shared, text, lib, stk, data;
	HANDLE	ph;
	SIZE_T	rss=-1;
	PROCESS_MEMORY_COUNTERS_EX mem;
	PSAPI_WORKING_SET_INFORMATION rss_info, *rss_set;
	SYSTEM_INFO sys_info;
	Psapi_t	*psapi;

	vmsize=shared=text=lib=stk=data=-1;
	/* text size appears to be same as reported by size(1M) */
	if ((psapi = init_psapi()) && (ph = OpenProcess(state.process_query|PROCESS_VM_READ, FALSE, pp->ntpid)))
	{
		GetSystemInfo(&sys_info);
		mem.cb = sizeof(mem);
		if (psapi->GetProcessMemoryInfo(ph, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem)))
		{
			rss = mem.WorkingSetSize / sys_info.dwPageSize;
			vmsize = (int)(mem.PagefileUsage / sys_info.dwPageSize);
		}
		memset((void*)&rss_info, 0, sizeof(rss_info));
		rss_info.NumberOfEntries = 1;
		r = psapi->QueryWorkingSet(ph, &rss_info, sizeof(rss_info));
		i = GetLastError();
		if (r==0)
		{
		    if (i == ERROR_BAD_LENGTH)
		    {
			rss_set = (PSAPI_WORKING_SET_INFORMATION *)malloc(sizeof(rss_info) + (rss_info.NumberOfEntries*sizeof(PSAPI_WORKING_SET_BLOCK)));
			if (rss_set)
			{
				if (psapi->QueryWorkingSet(ph, rss_set, (DWORD)(sizeof(rss_info)*rss_info.NumberOfEntries)))
				{
					shared = text = lib = data = 0;
					for (i = 0; i < (int)rss_info.NumberOfEntries; i++)
						switch (rss_set->WorkingSetInfo[i].Protection)
						{
						case PG_COW:
						case PG_EXEC_RDONLY:
						case PG_RDONLY:
							if (rss_set->WorkingSetInfo[i].Shared)
								lib++;
							break;
						case PG_EXEC_RDWR:
							if (!rss_set->WorkingSetInfo[i].Shared)
								text++;
							break;
						case PG_RDWR:
							if (rss_set->WorkingSetInfo[i].Shared)
								shared++;
							else
								data++;
							break;
						}
				}
				else
					logerr(0, "QueryWorkingSet entries=%d", rss_info.NumberOfEntries);
				free(rss_set);
			}
			else
				logmsg(0, "malloc failed ret=%x for working set entries=%d size=%d failed", rss_set, rss_info.NumberOfEntries, (rss_info.NumberOfEntries*sizeof(rss_info)));
		    }
		}
		closehandle(ph, HT_PROC);
	}
	else
		logerr(0, "GetProcessMemory slot=%d pid=%x-%d failed", proc_slot(pp), pp->ntpid, pp->pid);
	/* size, resident size, shared pages, text, library, data, dirty */
	n = sfsprintf(buf, sizeof(buf), FMT_statm
		, vmsize
		, rss
		, shared
		, text
		, lib
		, data
		, 0
	);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return (ssize_t)w;
}

static ssize_t get_pid_stack(Pproc_t* pp, int fid, HANDLE hp)
{
	char	buf[PAGESIZE];
	ssize_t	n;
	DWORD	w;
	
	n = uwin_stack_process(pp->pid, 0, "\n\t", buf, sizeof(buf));
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return (ssize_t)w;
}

static HANDLE trace_thread;

static void stop_trace(void)
{
	if (state.trace_done)
	{
		(*state.trace_done)(STILL_ACTIVE);
		state.trace_done = 0;
	}
	closehandle(P_CP->trace, HT_FILE);
	P_CP->trace = 0;
	P_CP->trpid = 0;
}

/* 
 * Thread to wait for trace process to terminate
 * Since the trace process is not the parent of the 
 * traced process.
 */
static DWORD WINAPI tr_wait(void *arg)
{
	HANDLE *ph = (HANDLE *)arg;
	DWORD n;
	
	n = WaitForSingleObject(ph, INFINITE);
	if (n != WAIT_OBJECT_0)
		logerr(0, "tr_wait wait failed=x%x(%d)", n, n);
	stop_trace();
	closehandle(ph, HT_PROC);
	ExitThread(0);
}


typedef struct
{
	DWORD	ntpid;
	pid_t	pid;
	char	win_name[BLOCK_SIZE-2*(sizeof(DWORD)+sizeof(pid_t))];
} Tr_args_t;

/*
 * Thread to initialize tracing for a process not a child of the
 * trace command.
 */
static DWORD WINAPI load_trace(void* arg)
{
	unsigned short	slot = (unsigned short)arg;
	Tr_args_t*	tr;
	HANDLE		ph;
	HANDLE		fh = INVALID_HANDLE_VALUE;
	int		n;
	int		r = 0;

	tr = (Tr_args_t *) block_ptr(slot);
	if (!P_CP->traceflags)
	{
		TerminateThread(trace_thread, 3);
		n=WaitForSingleObject(trace_thread, INFINITE);
		GetExitCodeThread(trace_thread, &r);
		stop_trace();
		r=1;
		goto fail;
	}
	if (P_CP->traceflags & UWIN_TRACE_INHERIT)
		fh = CreateFile(tr->win_name,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,sattr(1),CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
	else
		fh = CreateFile(tr->win_name,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,sattr(0),CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
	if (fh == INVALID_HANDLE_VALUE)
	{
		r = GetLastError();
		logerr(0, "load_trace open %s failed:", tr->win_name);
		goto fail;
	}
	/* truncate output file */
	SetEndOfFile(fh);
	if (ph=OpenProcess(PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE, tr->ntpid))
	{
		if (!(trace_thread=CreateThread(sattr(0),0,tr_wait,(void *)ph,0,0)))
		{
			logerr(0, "load_trace failed to create tr_wait thread");
			goto fail;
		}
		ph = 0;
	}
	else
	{
		logerr(0, "load_trace unable to get trace process pid=%x-%d handle", tr->ntpid, tr->pid);
		goto fail;
	}
	P_CP->trace = fh;
	r = trace(P_CP->trace, P_CP->traceflags);
fail:
	if (ph)
		closehandle(ph, HT_PROC);
	block_free(slot);
	ExitThread(r);
}

/*
 * perform read of named pipe from tracing process and write 
 * to the device file given by trace process.
 */
typedef struct
{
	HANDLE		ph;	/* pipe handle */
    	int		wfd;	/* stdout of trace command */
	HANDLE		th;	/* dev_thread handle */
} dev_arg_t;

static dev_arg_t	dev_arg;

static DWORD WINAPI dev_thread(void *arg)
{
	DWORD	n;
	char	buf[PAGESIZE];


	if (ConnectNamedPipe(dev_arg.ph, 0) != TRUE)
	{
		logerr(0, "dev_thread connect failed");
		ExitThread(1);
	}
	while (ReadFile(dev_arg.ph, buf, sizeof(buf), &n, 0) == TRUE)
		write(dev_arg.wfd, buf, n);
	/* expect to error when writer exits */
	ExitThread(0);
}

/*
 * initial trace for a process to allow trace -p
 * Expected buffer contents: <octal trace_flags> <nul-terminated tracefile>
 * The <tracefile> is mapped to the windows filename for load_trace to use.
 * If the filename is a device file a windows pipe is created
 * and a thread is started. The thread reads from the pipe and write
 * to the handles for the special file.
 */

static ssize_t set_pid_trace(Pproc_t* pp, int fid, char* buf, size_t size)
{
	ssize_t		n;
	ssize_t		ret = 0;
	int		flags;
	int		c;
	Pproc_t*	tproc = 0;
	Tr_args_t*	tr;
	char*		end;
	unsigned short	slot = block_alloc(BLK_VECTOR);
	HANDLE		ph;
	HANDLE		th;
	DWORD		texit;

	/* Verify process owns trace */
	ph=th=0;
	if (pp->trpid && pp->trpid != P_CP->pid)
	{
		errno = EBUSY;
		goto fail;
	}
	tr = (Tr_args_t*)block_ptr(slot);
	flags = 0;
	end = buf + size;
	while (buf < end && (c = *buf++) >= '0' && c <= '7')
		flags = (flags << 3) + (c - '0');
	if (buf >= end || c != ' ' || !(n = (end - buf)) || n >= sizeof(tr->win_name))
	{
		errno = EINVAL;
		goto fail;
	}
	memcpy(tr->win_name, buf, n);
	tr->win_name[n] = 0;
	if (!(tproc=proc_getlocked(pp->pid, 0)))
	{
		logerr(0, "set_pid_trace invalid pid=%d", pp->pid);
		errno = EACCES;
		goto fail;
	}
	tproc->traceflags = flags;
	tr->ntpid = P_CP->ntpid;
	tr->pid = P_CP->pid;
	if (flags)
	{
		if (strncmp(tr->win_name, "/dev/", 5) == 0)
		{
			int fd;
			char pname[PATH_MAX];
			HANDLE ph, th;
			/* device file, open file, create windows pipe, start thread */
			if ((fd = open(tr->win_name, O_WRONLY)) < 0)
			{
				errno = EACCES;
				goto fail;
			}
			UWIN_PIPE(pname,P_CP->ntpid, pp->ntpid, pp->pid);
			if (flags & UWIN_TRACE_INHERIT)
				ph=CreateNamedPipe(pname, PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_WAIT,1,PIPE_BUF,PIPE_BUF,0,sattr(1));
			else
				ph=CreateNamedPipe(pname, PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_WAIT,1,PIPE_BUF,PIPE_BUF,0,sattr(0));
			if (ph == INVALID_HANDLE_VALUE)
			{
				logerr(0, "set_pid_trace failed to create pipe for tty io");
				close(fd);
				errno = EACCES;
				goto fail;
			}
			dev_arg.ph = ph;
			dev_arg.wfd = fd;
			if (!(th=CreateThread(sattr(0),PAGESIZE,dev_thread,0,0,0)))
			{
				logerr(0, "set_pid_trace failed to create thread for tty io");
				close(fd);
				errno = EACCES;
				goto fail;
			}
			dev_arg.th = th;
			strcpy(tr->win_name, pname);
		}
		else
		{
		Path_t ip;
		char *ntpath;
			/* regular file, map to windows filename */
			ip.oflags = GENERIC_READ|GENERIC_WRITE;
			ip.hp = 0;
			ip.extra = 0;
			ntpath = pathreal(tr->win_name, P_FILE|P_NOHANDLE, &ip);
			strcpy(tr->win_name, ntpath);
		}
	}
	else
	{
		/* terminate dev_thread and release resources */
		if (dev_arg.th)
		{
			TerminateThread(dev_arg.th, 0);
			closehandle(dev_arg.th, HT_THREAD);
			closehandle(dev_arg.ph, HT_FILE);
			close(dev_arg.wfd);
			dev_arg.th = dev_arg.ph = 0;
			dev_arg.wfd = -1;
		}
	}
	
	/* start thread to start tracing in the requested process */
	if (ph = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_DUP_HANDLE|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|SYNCHRONIZE,FALSE,pp->ntpid))
	{
		if ((th=CreateRemoteThread(ph,0,0,load_trace,(void *)slot,0,0)) != INVALID_HANDLE_VALUE)
		{
			if ((n=WaitForSingleObject(th, 1000)) == WAIT_OBJECT_0)
			{
				GetExitCodeThread(th, &texit);
				if (texit != 1)
				{
					/* trace_init success return code */
					errno = EACCES;
					goto fail;
				}
			}
			else
			{
				logerr(0, "set_pid_trace load_trace failed %d:", n);
				errno = EACCES;
				goto fail;
			}
		}
		else
		{
			logerr(0, "set_pid_trace create_remote_thread for proc=%x-%d failed", pp->ntpid, pp->pid);
			errno = EACCES;
			goto fail;
		}
	}
	else
	{
		logerr(0, "set_pid_trace OpenProcess=%x pid=%x-%d failed", ph, pp->ntpid, pp->pid);
		errno = EACCES;
		goto fail;
	}
	if (flags)
		pp->trpid = P_CP->pid;
	ret = size;
fail:
	if (tproc)
		proc_release(tproc);
	if (ph)
		closehandle(ph, HT_PROC);
	if (th)
		closehandle(th, HT_THREAD);
	return ret;
}

static ssize_t get_pid_status(Pproc_t* pp, int fid, HANDLE hp)
{
	int				i, r;
	int				vmsize, lck, rss, data, shared, stk, text, lib;
	ssize_t				n;
	DWORD				w;
	uid_t				uid, euid;
	gid_t				gid, egid;
	char*				s;
	HANDLE				ph;
	PROCESS_MEMORY_COUNTERS_EX	mem;
	SYSTEM_INFO			sys_info;
	struct prpsinfo			pr;
	PSAPI_WORKING_SET_INFORMATION	rss_info;
	PSAPI_WORKING_SET_INFORMATION*	rss_set;
	Psapi_t*			psapi;
	char				buf[2*PATH_MAX];

	vmsize = lck = rss = data = stk = text = lib = -1;
	uid = euid = gid = egid = -1;
	prpsinfo(0, pp, &pr, buf, sizeof(buf));
	proc_uid(pp, &uid, &euid);
	proc_gid(pp, &gid, &egid);
	if (!pp->prog_name[0])
	{
		if (s = strrchr(buf, '/'))
			s++;
		else
			s = buf;
		sfsprintf(pp->prog_name, sizeof(pp->prog_name), "%s", s);
	}
	if ((psapi = init_psapi()) && (ph = OpenProcess(state.process_query|PROCESS_VM_READ, FALSE, pp->ntpid)))
	{
		GetSystemInfo(&sys_info);
		mem.cb = sizeof(mem);
		if (psapi->GetProcessMemoryInfo(ph, (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem)))
		{
			rss = (int)(mem.WorkingSetSize / 1024);
			vmsize = (int)(mem.PagefileUsage / 1024);
		}
		memset((void*)&rss_info, 0, sizeof(rss_info));
		rss_info.NumberOfEntries = 1;
		r = psapi->QueryWorkingSet(ph, &rss_info, sizeof(rss_info));
		i = GetLastError();
		if (r==0)
		{
		    if (i == ERROR_BAD_LENGTH)
		    {
			rss_set = (PSAPI_WORKING_SET_INFORMATION *)malloc(sizeof(rss_info) + (rss_info.NumberOfEntries*sizeof(PSAPI_WORKING_SET_BLOCK)));
			if (rss_set)
			{
				if (psapi->QueryWorkingSet(ph, rss_set, (DWORD)(sizeof(rss_info)*rss_info.NumberOfEntries)))
				{
					lck = shared = text = lib = data = stk = 0;
					for (i = 0; i < (int)rss_info.NumberOfEntries; i++)
						switch (rss_set->WorkingSetInfo[i].Protection)
						{
						case PG_COW:
						case PG_EXEC_RDONLY:
						case PG_RDONLY:
							if (rss_set->WorkingSetInfo[i].Shared)
								lib++;
							break;
						case PG_EXEC_RDWR:
							if (!rss_set->WorkingSetInfo[i].Shared)
								text++;
							break;
						case PG_RDWR:
							if (rss_set->WorkingSetInfo[i].Shared)
								shared++;
							else
								data++;
							break;
						}
					data *= (sys_info.dwPageSize/1024);
					lib *= (sys_info.dwPageSize/1024);
					shared *= (sys_info.dwPageSize/1024);
					stk *= (sys_info.dwPageSize/1024);
					text *= (sys_info.dwPageSize/1024);
				}
				else
					logerr(0, "QueryWorkingSet entries=%d", rss_info.NumberOfEntries);
				free(rss_set);
			}
			else
				logmsg(0, "malloc failed ret=%x for working set entries=%d size=%d failed", rss_set, rss_info.NumberOfEntries, (rss_info.NumberOfEntries*sizeof(rss_info)));
		    }
		}
		closehandle(ph, HT_PROC);
	}
	n = sfsprintf(buf, sizeof(buf), FMT_status
		, pp->prog_name
		, pp->state
		, pp->state
		, pr.pr_lttydev
		, pp->pid
		, pp->ppid
		, pp->ntpid
		, uid
		, euid
		, uid
		, euid
		, gid
		, egid
		, gid
		, egid
		, vmsize
		, lck
		, rss
		, data
		, stk
		, text
		, lib
		, pp->siginfo.siggot
		, pp->siginfo.sigmask
		, pp->siginfo.sigign
		, pp->wow
	);
	WriteFile(hp, buf, (DWORD)n, &w, 0);
	return (ssize_t)w;
}

static int procdir(DIRSYS* dirp, struct dirent* ep)
{
	Procdirstate_t*	ps = (Procdirstate_t*)(dirp+1);
	int		i;

	i = (++ep->d_fileno) & PROCDIR_MASK;
	if (!ps->pf[i].name)
		return 0;
	switch (ps->pf[i].mode)
	{
	case S_IFDIR:
		ep->d_type = DT_DIR;
		break;
	case S_IFLNK:
		ep->d_type = DT_LNK;
		break;
	default:
		ep->d_type = DT_REG;
		break;
	}
	return (int)sfsprintf(dirp->fname, 15, "%s", ps->pf[i].name);
}

static int procdirblock(DIRSYS* dirp, struct dirent* ep)
{
	int	i;

	while ((i = (++ep->d_fileno) & PROCDIR_MASK) < Share->nfiles)
		if ((Blocktype[i] & BLK_MASK) != BLK_FREE)
		{
			ep->d_type = DT_REG;
			return (int)sfsprintf(dirp->fname, 15, "%d", i);
		}
	return 0;
}

static int procdirfd(DIRSYS* dirp, struct dirent* ep)
{
	Pproc_t*	pp;
	ssize_t		n;
	int		fd;

	if (pp = proc_locked(dirp->subdir))
	{
		for (fd = (ep->d_fileno & PROCDIR_MASK) - 1; fd < pp->maxfds; fd++)
			if (fdslot(pp, fd) > 0)
			{
				ep->d_fileno = (ep->d_fileno & ~PROCDIR_MASK) | (fd + 2);
				ep->d_type = DT_LNK;
				n = sfsprintf(dirp->fname, 15, "%d", fd);
				if (dllinfo._ast_libversion > 4)
					dirp->flen = (int)n;
				proc_release(pp);
				return 1;
			}
		proc_release(pp);
	}
	return 0;
}

static int procdirpid(DIRSYS* dirp, struct dirent* ep)
{
	Procdirstate_t*	ps = (Procdirstate_t*)(dirp+1);
	Pproc_t*	pp;
	pid_t		pid;
	ssize_t		n;

	if (pid = ps->getnext(ps))
	{
		if (pp = proc_ntgetlocked(pid, 0))
		{
			pid = pp->pid;
			proc_release(pp);
		}
		else
			pid = uwin_nt2unixpid(pid);
		n = sfsprintf(dirp->fname, 15, "%d", pid);
		if (dllinfo._ast_libversion > 4)
			dirp->flen = (int)n;
		ep->d_type = DT_DIR;
		return 1;
	}
	dirp->getnext = procdir;
	ep->d_fileno = ps->type;
	return procdir(dirp, ep);
}

static int procdirtask(DIRSYS* dirp, struct dirent* ep)
{
	Pproc_t*	pp;
	ssize_t		n;

	if ((n = ep->d_fileno++ & PROCDIR_MASK) > 1)
		return 0;
	if (!(pp = proc_locked(dirp->subdir)))
		return 0;
	n = sfsprintf(dirp->fname, 15, "%d", pp->pid);
	proc_release(pp);
	ep->d_type = DT_DIR;
	return 1;
}

static int file_fd(const char* path, char** next, int* pid, int* fid)
{
	register char*	s = (char*)path;
	register int	n;

	if (s[0] == 's' && s[1] == 't' && s[2] == 'd')
	{
		if (s[3] == 'i' && s[4] == 'n')
		{
			s += 5;
			n = 0;
		}
		else if (s[3] == 'o' && s[4] == 'u' && s[5] == 't')
		{
			s += 6;
			n = 1;
		}
		else if (s[3] == 'e' && s[4] == 'r' && s[5] == 'r')
		{
			s += 6;
			n = 2;
		}
	}
	else
	{
		n = 0;
		while (*s >= '0' && *s <= '9')
			n = n * 10 + (*s++ - '0');
	}
	if (*s && *s != '/' && *s != '\\')
		return 0;
	*next = s;
	if (fid)
		*fid = n;
	return 1;
}

static int file_fid(const char* path, char** next, int* pid, int* fid)
{
	register char*	s = (char*)path;
	register int	n;

	n = 0;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (*s++ - '0');
	if (*s && *s != '/' && *s != '\\')
		return 0;
	*next = s;
	if (fid)
		*fid = n;
	return 1;
}

static int file_pid(const char* path, char** next, int* pid, int* fid)
{
	register char*	s = (char*)path;
	register int	n;

	n = 0;
	while (*s >= '0' && *s <= '9')
		n = n * 10 + (*s++ - '0');
	if (*s && *s != '/' && *s != '\\')
		return 0;
	*next = s;
	if (pid)
		*pid = n;
	return 1;
}

/*
 * NOTE: coordinate PROCDIR_* with Procdir[] below
 * NOTE: only Proc_pid[] may have set() callouts
 */

#define PROCDIR_root		0
#define PROCDIR_pid		1
#define PROCDIR_pid_fd		2
#define PROCDIR_pid_task	3
#define PROCDIR_pid_task_tid	4
#define PROCDIR_net		5
#define PROCDIR_uwin		6
#define PROCDIR_uwin_block	7

static const Procfile_t Proc_pid_fd[] =
{
PROCENT("/fd",		0,	0,			0,		0,		0,	 0, -1)
PROCENT("#fd",		file_fd,0,			0,		txt_pid_fd,	S_IFLNK, 0, -1)
{ 0 }
};

static const Procfile_t Proc_pid_task[] =
{
PROCENT("/task",	0,	0,			0,		0,		0,	 0, -1)
PROCENT("#task",	file_fid,0,			0,		0,		S_IFDIR, 0, PROCDIR_pid_task_tid)
{ 0 }
};

static const Procfile_t Proc_pid_task_tid[] =
{
PROCENT("/tid",		0,	0,			0,		0,		0,	 0, -1)
{ 0 }
};

static const Procfile_t Proc_pid[] =
{
PROCENT("/pid",		0,	0,			0,		0,		0,	 0, -1)
PROCENT("cmdline",	0,	get_pid_cmdline,	0,		0,		S_IFREG, 0, -1)
PROCENT("cwd",		0,	0,			0,		txt_pid_cwd,	S_IFLNK, 0, -1)
PROCENT("debug",	0,	get_pid_debug,		set_pid_debug,	0,		S_IFREG|S_IWUSR, 0, -1)
PROCENT("exe",		0,	0,			0,		txt_pid_exe,	S_IFLNK, 0, -1)
PROCENT("exename",	0,	get_pid_exename,	0,		0,		S_IFREG, 0, -1)
PROCENT("fd",		0,	0,			0,		0,		S_IFDIR, 0, PROCDIR_pid_fd)
PROCENT("gid",		0,	get_pid_gid,		0,		0,		S_IFREG, 0, -1)
PROCENT("lib",		0,	get_pid_lib,		0,		0,		S_IFREG, 1, -1)
PROCENT("maps",		0,	get_pid_maps,		0,		0,		S_IFREG, 1, -1)
PROCENT("pgid",		0,	get_pid_pgid,		0,		0,		S_IFREG, 0, -1)
PROCENT("ppid",		0,	get_pid_ppid,		0,		0,		S_IFREG, 0, -1)
PROCENT("root", 	0,	0, 			0,		txt_pid_root,	S_IFLNK, 0, -1)
PROCENT("sid",		0,	get_pid_sid,		0,		0,		S_IFREG, 0, -1)
PROCENT("slots",	0,	get_pid_slots,		0,		0,		S_IFREG, 0, -1)
PROCENT("stack",	0,	get_pid_stack,		0,		0,		S_IFREG, 0, -1)
PROCENT("stat",		0,	get_pid_stat,		0,		0,		S_IFREG, 0, -1)
PROCENT("statm", 	0,	get_pid_statm,		0,		0,		S_IFREG, 0, -1)
PROCENT("status", 	0,	get_pid_status,		0,		0,		S_IFREG, 0, -1)
PROCENT("task",		0,	0,			0,		0,		S_IFDIR, 0, PROCDIR_pid_task)
PROCENT("trace",	0,	0,			set_pid_trace,	0,		S_IFREG|S_IWUSR, 0, -1)
PROCENT("winexename",	0,	get_pid_winexename,	0,		0,		S_IFREG, 0, -1)
PROCENT("winpid",	0,	get_pid_winpid,		0,		0,		S_IFREG, 0, -1)
#ifdef HANDLEWATCH
PROCENT("handles",	0,	get_pid_htab,		0,		0,		S_IFREG, 1, -1)
#endif
{ 0 }
};

static const Procfile_t Proc_net[] =
{
PROCENT("/net",		0,	0,			0,		0,		0,	 0, -1)
PROCENT("tcp",		0,	get_net_tcp,		0,		0,		S_IFREG, 0, -1)
PROCENT("tcp6",		0,	get_net_tcp6,		0,		0,		S_IFREG, 0, -1)
PROCENT("udp",		0,	get_net_udp,		0,		0,		S_IFREG, 0, -1)
PROCENT("udp6",		0,	get_net_udp6,		0,		0,		S_IFREG, 0, -1)
PROCENT("unix",		0,	get_net_unix,		0,		0,		S_IFREG, 0, -1)
{ 0 }
};

static const Procfile_t Proc_uwin[] =
{
PROCENT("/uwin",	0,	0,			0,		0,		0,	 0, -1)
PROCENT("block",	0,	0,			0,		0,		S_IFDIR, 0, PROCDIR_uwin_block)
PROCENT("slots",	0,	get_uwin_slots,	0,		0,		S_IFREG, 0, -1)
PROCENT("blocksize",	0,	get_uwin_blocksize,	0,		0,		S_IFREG, 0, -1)
PROCENT("mem",		0,	get_uwin_mem,		0,		0,		S_IFREG, 0, -1)
PROCENT("ofiles",	0,	get_ofiles,		0,		0,		S_IFREG, 1, -1)
PROCENT("pdev_t",	0,	get_uwin_pdev_t,	0,		0,		S_IFREG, 0, -1)
PROCENT("pproc_t",	0,	get_uwin_pproc_t,	0,		0,		S_IFREG, 0, -1)
PROCENT("pshare_t",	0,	get_uwin_pshare_t,	0,		0,		S_IFREG, 0, -1)
PROCENT("resource",	0,	get_uwin_resource,	0,		0,		S_IFREG, 0, -1)
PROCENT("uptime",	0,	get_uwin_uptime,	0,		0,		S_IFREG, 0, -1)
{ 0 }
};

static const Procfile_t Proc_uwin_block[] =
{
PROCENT("/block",	0,	0,			0,		0,		0,	 0, -1)
PROCENT("#block",	file_fid,disp_uwin_block,	0,		0,		S_IFREG, 0, -1)
{ 0 }
};

static const Procfile_t Proc[] =
{
PROCENT("/root",	0,	0,			0,		0,		S_IFDIR, 0,	0)
PROCENT("#root",	file_pid,0,			0,		0,		S_IFDIR, 0, PROCDIR_pid)
PROCENT("cpuinfo",	0,	get_cpuinfo,		0,		0,		S_IFREG, 0, -1)
PROCENT("devices",	0,	get_devices,		0,		0,		S_IFREG, 0, -1)
PROCENT("locks",	0,	get_locks,		0,		0,		S_IFREG, 1, -1)
PROCENT("mounts",	0,	get_mounts,		0,		0,		S_IFREG, 1, -1)
PROCENT("net",		0,	0,			0,		0,		S_IFDIR, 1, PROCDIR_net)
PROCENT("self",		0,	0,			0,		txt_self,	S_IFLNK, 0, PROCDIR_pid)
PROCENT("stat",		0,	get_stat,		0,		0,		S_IFREG, 0, -1)
PROCENT("swaps",	0,	get_swaps,		0,		0,		S_IFREG, 0, -1)
PROCENT("uptime",	0,	get_uptime,		0,		0,		S_IFREG, 0, -1)
PROCENT("uwin",		0,	0,			0,		0,		S_IFDIR, 1, PROCDIR_uwin)
PROCENT("version",	0,	get_version,		0,		0,		S_IFREG, 0, -1)
{ 0 }
};

/*
 * NOTE: coordinate Procdir[] with PROCDIR_* above
 */

const Procdir_t Procdir[] =
{
	&Proc[0],		procdirpid,
	&Proc_pid[0],		procdir,
	&Proc_pid_fd[0],	procdirfd,
	&Proc_pid_task[0],	procdirtask,
	&Proc_pid_task_tid[0],	procdir,
	&Proc_net[0],		procdir,
	&Proc_uwin[0],		procdir,
	&Proc_uwin_block[0],	procdirblock,
};

Procfile_t* procfile(char* path, int flags, int* pid, int* fid, int* dir, unsigned int* ino)
{
	register const Procfile_t*	tab;
	char*				t;
	ssize_t				n;
	int				f;
	int				i;
	int				p;
	int				v;
	char				buf[PATH_MAX];

	static const char		proc[] = "/usr/proc";

	if (!path || !(n = (int)strlen(path)))
		goto noent;
	if (*(path + 1) == ':')
	{
		if (n < (int)(state.rootlen + sizeof(proc) - 1))
			goto noent;
		path += state.rootlen + sizeof(proc) - 1;
	}
	if (prefix(path, proc, sizeof(proc) - 1))
		path += sizeof(proc) - 1;
	while (*path == '/' || *path == '\\')
		if (*++path == '.' && (*(path + 1) == '/' || *(path + 1) == '\\'))
			path += 2;
	p = 0;
	v = 0;
	if (pid)
		*pid = -1;
	if (!fid)
		fid = &f;
	*fid = -1;
	tab = Procdir[0].pf;
	if (!*path)
	{
		i = 0;
		goto done;
	}
 descend:
	for (i = 1; tab[i].name; i++)
		if (tab[i].conv)
		{
			if (tab[i].conv(path, &t, pid, fid))
			{
				path = t;
				goto found;
			}
		}
		else if (prefix(path, tab[i].name, tab[i].len))
		{
			path += tab[i].len;
			goto found;
		}
	for (;;)
	{
		switch (*path++)
		{
		case 0:
			break;
		case '/':
		case '\\':
			goto nodir;
		default:
			continue;
		}
		break;
	}
 noent:
	errno = ENOENT;
	return 0;
 nodir:
	errno = ENOTDIR;
	return 0;
 found:
	if (tab[i].mode == S_IFLNK && tab[i].tree >= 0 && (*path || !(flags & (P_LSTAT|P_NOFOLLOW))) && (n = tab[i].txt(P_CP, *fid, buf, sizeof(buf))) > 0)
	{
		if (*path)
			sfsprintf(buf + n, sizeof(buf) - n, "%s", path);
		else if (flags & P_CHDIR)
			strcpy(path - tab[i].len, buf);
		path = buf;
		goto descend;
	}
	if (*path)
	{
		if (tab[i].mode != S_IFDIR)
			goto nodir;
		path++;
		if (tab[i].tree >= 0)
		{
			p = tab[i].tree;
			tab = Procdir[tab[i].tree].pf;
			goto descend;
		}
	}
 done:
	if (dir)
		*dir = tab[i].tree;
	if (ino)
		*ino = (PROCDIR_TYPE|(p<<PROCDIR_SHIFT)) + i;
	return (Procfile_t*)&tab[i];
}

int proc_setdir(char* path, int slot, int* procdir)
{
	Procfile_t*	pf;
	Pproc_t*	pp;
	int		pid;
	int		dir;

	if (!(pf = procfile(path, P_CHDIR, &pid, 0, &dir, 0)))
		return 0;
	if (pf->mode != S_IFDIR)
		goto nodir;
	if (pid > 0)
	{
		if (!(pp = proc_locked(pid)))
			goto nodir;
		proc_release(pp);
	}
	*procdir = dir + 1;
	return 1;
 nodir:
	errno = ENOTDIR;
	return 0;
}

int procstat(Path_t* ip, const char* path, struct stat* st, int flags)
{
	struct timespec	utime;
	FILETIME	filetime;
	HANDLE		hp;
	int		pid;
	int		fid;
	unsigned int	ino;
	int		n;
	Procfile_t*	pf;
	Pproc_t*	pp;
	char		buf[PATH_MAX];

	if (!(pf = procfile((char*)path, flags, &pid, &fid, 0, &ino)))
		return -1;
	if (pid < 0)
		pp = P_CP;
	else if (!(pp = proc_locked(pid)))
	{
		if (hp = OpenProcess(READ_CONTROL, FALSE, uwin_ntpid(pid)))
			closehandle(hp, HT_PROC);
		else if (GetLastError() != ERROR_ACCESS_DENIED)
		{
			errno = pf->mode == S_IFDIR ? ENOENT : ENOTDIR;
			return -1;
		}
	}
	st->st_mode = pf->mode;
	switch (pf->mode)
	{
	case S_IFDIR:
		st->st_mode |= pf->tree == PROCDIR_pid_fd ? (S_IRUSR|S_IXUSR) : (S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
		st->st_nlink = 2;
		st->st_size = 512;
		goto typed;
	default:
		st->st_mode |= S_IRUSR|S_IRGRP|S_IROTH;
		st->st_nlink = 1;
		st->st_size = 0;
	typed:
		if (pp)
		{
			proc_uid(pp, &st->st_uid, (uid_t*)&n);
			proc_gid(pp, &st->st_gid, (gid_t*)&n);
		}
		else
		{
			st->st_ino = Share->nfiles + pid + PROC_DIR + PROC_SUBDIR; 
			st->st_uid = 0;
			st->st_gid = 0;
		}
		break;
	case S_IFLNK:
		if (!(flags & (P_LSTAT|P_NOFOLLOW)) && pf->txt(pp, fid, buf, sizeof(buf)) > 0 && !stat(buf, st))
			goto done;
		st->st_mode |= S_IRUSR|S_IRGRP|S_IROTH;
		st->st_size = ip ? ip->shortcut : 0;
		if (pp)
			goto typed;
		st->st_uid = 0;
		st->st_gid = 0;
		break;
	}
	st->st_dev = 27;
	st->st_ino = ino;
	st->st_nlink = 1;
	time_getnow(&filetime);
	unix_time(&filetime, &utime, 1);
	st->st_atime = utime.tv_sec;
	st->st_mtime = utime.tv_sec;
	st->st_ctime = utime.tv_sec;
 done:
	if (pp && pid >= 0)
		proc_release(pp);
	return 0;
}

extern Pproc_t *proc_srchpid(pid_t pid);
HANDLE procselect(int fd, Pfd_t* fdp, int type, HANDLE hp)
{
	/* Phandle(fd) == pid+1 */
	register Pproc_t *p;

	if (p=proc_srchpid((pid_t)(Phandle(fd))-1))
	{
		/* Need to generate a handle for proc */
		if (!Ehandle(fd))
			Ehandle(fd) = OpenProcess(SYNCHRONIZE, TRUE, p->ntpid);
		else if (procexited(p))
		{
			Ehandle(fd) = INVALID_HANDLE_VALUE;
			fdp->socknetevents |= FD_CLOSE;
		}
	}
	else
	{
		Ehandle(fd) = INVALID_HANDLE_VALUE;
		fdp->socknetevents |= FD_CLOSE;
	}
	return Ehandle(fd);

}

HANDLE procopen(Pfd_t* fdp, const char* path, int set)
{
	Procfile_t*	pf;
	Pproc_t*	pp;
	Pproc_t*	np;
	HANDLE		hp;
	HANDLE		ho;
	int		pid;
	int		fid;
	unsigned int	ino;
	Path_t		pi;
	char		buf[PATH_MAX];

	fdp->type = FDTYPE_PROC;
	if (!(pf = procfile((char*)path, 0, &pid, &fid, 0, &ino)))
		return 0;
	if (pid < 0)
		pp = 0;
	else if (!(pp = proc_locked(pid)))
	{
		errno = ENOTDIR;
		return 0;
	}
	hp = 0;
	if (pf->mode == S_IFDIR)
		hp = (HANDLE)(pid+1); /* XXX: this assumes /proc/<pid> only -- could cause fchdir() problems */
	else if (pf->txt)
	{
		if (pf->txt(pp, fid, buf, sizeof(buf)) > 0)
		{
			pi.oflags = GENERIC_READ;
			if (pathreal(buf, P_EXIST, &pi))
				hp = pi.hp;
		}
	}
	else if (set)
	{
		if (!pf->set)
			errno = EIO;
		else
		{
			hp = (HANDLE)2;
			fdp->extra32[1] = pid;
			fdp->devno = fid;
			fdp->mnt = (ino>>PROCDIR_SHIFT)&PROCDIR_MASK;
			fdp->sigio = ino&PROCDIR_MASK;
		}
	}
	else if (!CreatePipe(&hp, &ho, sattr(1), 8*1024+24)) 
		logerr(0, "CreatePipe");
	else
	{
		if (pf->fork)
		{
			if ((pid = fork()) < 0)
				return 0;
			if (!pid)
			{
				strcpy(P_CP->prog_name, pf->name);
				closehandle(hp, HT_PIPE);
				pf->get(pp, fid, ho);
				_exit(0);
			}
			Sleep(100);
			if (np = proc_locked(pid))
			{
				np->console_child = 1;
				np->posixapp &= ~UWIN_PROC;
				proc_release(np);
			}
			waitpid(pid, &fid, WNOHANG);
		}
		else
		{
			pf->get(pp, fid, ho);
			closehandle(ho, HT_PIPE);
		}
		fdp->type = FDTYPE_PIPE;
	}
	if (pp)
		proc_release(pp);
	return hp;
}

ssize_t procwrite(int fd, Pfd_t* fdp, char* buf, size_t size)
{
	const Procfile_t*	pf;
	Pproc_t*		pp;
	ssize_t			n;

	if (fdp->extra32[1] == (unsigned long)(-1))
	{
		errno = EIO;
		return -1;
	}
	pf = Procdir[fdp->mnt].pf + fdp->sigio;
	if (!fdp->extra32[1])
		pp = 0;
	else if (!(pp = proc_locked(fdp->extra32[1])))
	{
		errno = EROFS;
		return -1;
	}
	fdp->extra32[1] = (unsigned long)(-1);
	n = pf->set(pp, fdp->devno, buf, size);
	if (pp)
		proc_release(pp);
	return n;
}
