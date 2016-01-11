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
#ifndef	_UWIN_SHARE_H
#define _UWIN_SHARE_H			1

/*
 * NOTE: all structs in this file must have the same size
 *	 and offsets for 32 bit and 64 bit compilers
 */

/*
 * uwin kernel object names
 */

#define UWIN_SHARE_GLOBAL		"Global\\"
#define UWIN_SHARE_DOMAIN		"UWIN"
#define UWIN_SHARE_DOMAIN_OLD		"uwin"

#define UWIN_SHARE			".share"
#define UWIN_SHARE_OLD			".v1"

#define UWIN_EVENT_ALARM(b)		(sfsprintf(b,sizeof(b),"%s.event.alarm#%x",state.resource,P_CP->ntpid),b)
#define UWIN_EVENT_EXEC(b,p)		(sfsprintf(b,sizeof(b),"%s.event.exec#%x",state.resource,p),b)
#define UWIN_EVENT_FIFO(b,p)		(sfsprintf(b,sizeof(b),"%s.event.fifo.%zd%zd",state.resource,(p)->high,(p)->low),b)
#define UWIN_EVENT_INIT(b)		(sfsprintf(b,sizeof(b),"%s.event.init",state.resource),b)
#define UWIN_EVENT_INIT_RESTART(b)	(sfsprintf(b,sizeof(b),"%s.event.init.restart",state.resource),b)
#define UWIN_EVENT_INIT_START(b)	(sfsprintf(b,sizeof(b),"%s.event.init.start",state.resource),b)
#define UWIN_EVENT_PIPE(b,n)		(sfsprintf(b,sizeof(b),"%s.event.pipe.%zd",state.resource,n),b)
#define UWIN_EVENT_S(b,p)		(sfsprintf(b,sizeof(b),"%s.event.s#%x",state.resource,p),b)
#define UWIN_EVENT_SELECT(b,p)		(sfsprintf(b,sizeof(b),"%s.event.select.%d.%d",state.resource,(p)->major,(p)->minor),b)
#define UWIN_EVENT_SEM(b,n)		(sfsprintf(b,sizeof(b),"%s.event.sem#%x",state.resource,n),b)
#define UWIN_EVENT_SIG(b,p)		(sfsprintf(b,sizeof(b),"%s.event.sig#%x",state.resource,p),b)
#define UWIN_EVENT_SOCK(b,p,h)		(sfsprintf(b,sizeof(b),"%s.event.select#%x.%p",state.resource,p,h),b)
#define UWIN_EVENT_SOCK_CONNECT(b,p)	(sfsprintf(b,sizeof(b),"%s.event.sock.connect#%x.%x",state.resource,(p)[0],(p)[1]),b)
#define UWIN_EVENT_SOCK_IO(b,s)		(sfsprintf(b,sizeof(b),"%s.event.sock.io.%zd",state.resource,s),b)
#define UWIN_EVENT_SYNC(b,p)		(sfsprintf(b,sizeof(b),"%s.event.sync.%d.%d",state.resource,(p)->major,(p)->minor),b)
#define UWIN_EVENT_WRITE(b,p)		(sfsprintf(b,sizeof(b),"%s.event.write.%d.%d",state.resource,(p)->major,(p)->minor),b)

#define UWIN_MUTEX_CLOSE(b,s)		(sfsprintf(b,sizeof(b),"%s.mutex.close.%d",state.resource,s),b)
#define UWIN_MUTEX_INIT_RUNNING(b,p)	(sfsprintf(b,sizeof(b),"%s.mutex.init.running.%d",state.resource,p),b)
#define UWIN_MUTEX_PROC(b,o)		(sfsprintf(b,sizeof(b),"%s.mutex.proc.%d",state.resource,o),b)
#define UWIN_MUTEX_SELECT(b,p)		(sfsprintf(b,sizeof(b),"%s.mutex.select.%d.%d",state.resource,(p)->major,(p)->minor),b)
#define UWIN_MUTEX_SETEUID(b)		(sfsprintf(b,sizeof(b),"%s.mutex.seteuid",state.resource),b)
#define UWIN_MUTEX_SETUID(b)		(sfsprintf(b,sizeof(b),"%s.mutex.setuid",state.resource),b)

#define UWIN_RESOURCE_MAX		64

/*
 * 32/64 compatible handle type
 * "64 bit handles have only 32 significant bits"
 * this assumes 32 and 64 little endian
 */

#if _X64_
#define WoW(t,m)	t m
#else
#define WoW(t,m)	t m; t _wow_ ## m
#endif

/*
 * this is the structure of the shared memory segment header
 * for a uwin instance
 */

#include <windows.h>

#define UWIN_SHARE_NUM_MUTEX	4
#define UWIN_DCACHE		4

typedef struct Uwin_dcache_s
{
	int		count;
	unsigned short	index;
	unsigned short	len;
} Uwin_dcache_t;

typedef struct Uwin_mutex_s
{
	WoW(HANDLE,	mutex);
} Uwin_mutex_t;

typedef struct Uwin_ipclim_s
{
	int	msgmaxids;		/* max # of msg queue identifiers */
	int	msgmaxqsize;		/* default max message queue size */
	int	msgmaxsize;		/* max size of message (bytes) */
	int	semmaxids;		/* max # of semaphore identifiers */
	int	semmaxinsys;		/* max # of semaphores in system */
	int	semmaxopspercall;	/* max num of ops per semop call */
	int	semmaxsemperid;		/* max num of semaphores per id */
	int	semmaxval;		/* semaphore maximum value */
	int	shmmaxids;		/* Max number of the shmid */
	int	shmmaxsegperproc;	/* max. # of shared memory segments per proc */
	int	shmmaxsize;		/* max. size of the shared memory */
	int	shmminsize;		/* min. size of the shared memory */
} Uwin_ipclim_t;

#define UWIN_SHARE_MAGIC		0x1416080d
#define UWIN_SHARE_VERSION		20110115L

#define UWIN_SHARE_VERSION_MIN		20110115L
#define UWIN_SHARE_VERSION_MAX		21000101L

#define UWIN_INIT_START			1
#define UWIN_INIT_SHUTDOWN		2

typedef struct Uwin_share_s
{
	/* NOTE: the first 4 members <size,offset> must not change */
	unsigned int	magic;		/* UWIN_SHARE_MAGIC */
	unsigned int	version;	/* UWIN_SHARE_VERSION */
	unsigned int	block_size;
	unsigned int	root_offset;
	unsigned int	init_ntpid;
	unsigned int	init_bits;

	Uwin_mutex_t	lockmutex[UWIN_SHARE_NUM_MUTEX+1];

	Uwin_dcache_t	dcache[UWIN_DCACHE];

	WoW(HANDLE,	init_sock);
	WoW(HANDLE,	init_pipe);
	WoW(HANDLE,	init_handle);

	unsigned __int64 cycle0;

	FILETIME	boot;		/* system boot time */
	FILETIME	start;		/* uwin instance boot time */

	DWORD		pidxor;		/*95*/
	DWORD		pidbase;	/*95*/
	DWORD		pidlock;
	DWORD		maxtime;
	DWORD		maxltime;
	DWORD		argmax;
	DWORD		nmutex;
	DWORD		dcount;
	DWORD		locktid[UWIN_SHARE_NUM_MUTEX+1];

#if _X64_
	DWORD		unused_1;
#else
	LONG		(PASCAL* compare_exchange)(void*,LONG,LONG);	/* 32 bit compare&exchange function */
#endif

	int		vforkpid;
	int		block;
	int		top;
	int		mount_tbsize;
	int		otop;
	int		backlog;
	int		init_thread_id;
	int		fdtab_index;
	int		adjust;
	unsigned int	old_root_offset;	/* OBSOLETE in 2012 */
	int		nblockops;
	int		nblocks;
	int		nfiles;
	unsigned short	init_serial;
	unsigned short	old_block_size;		/* OBSOLETE in 2012 */
	int		nprocs;
	unsigned int	ihandles;
	unsigned int	blksize;
	unsigned int	mem_size;
	unsigned int	share_size;
	unsigned int	drivetime[2];
	unsigned int 	counters[2];
	unsigned int	lockwait[UWIN_SHARE_NUM_MUTEX+1];

	Uwin_ipclim_t	ipc_limits;

	int		Maxsid;
	unsigned short	Platform;
	unsigned short	Version;
	int		debug;
	int		base_drive;
	int		movex;
	int		linkno;

	unsigned short	nfifos;
	unsigned short	nsids;
	unsigned short	init_slot;
	unsigned short	blockdev_index;
	unsigned short	blockdev_size;
	unsigned short	chardev_index;
	unsigned short	chardev_size;
	unsigned short	mount_table;
	unsigned short	child_max;
	unsigned short	uid_index;
	unsigned short	name_index;
	unsigned short	pipe_times;
	unsigned short	block_table;
	unsigned short	nullblock;
	unsigned short	lastslot;
	short		rootdirsize;
	short		sockstate;
	short		affinity;
	short 		update_immediate;
	short		bitmap;
	short		saveblk;
	short		pid_counters;
	short		pid_shift;
	unsigned short	lockslot[UWIN_SHARE_NUM_MUTEX+1];
	unsigned short	lockdone[UWIN_SHARE_NUM_MUTEX+1];
	unsigned short	nlock[UWIN_SHARE_NUM_MUTEX+1];
	unsigned short	olockslot[UWIN_SHARE_NUM_MUTEX+1];
	unsigned short	lastlline[UWIN_SHARE_NUM_MUTEX+1];
	short		mutex_offset[UWIN_SHARE_NUM_MUTEX+1];

	unsigned char	init_state;
	unsigned char	stack_trace;
	unsigned char	case_sensitive;
	unsigned char	no_shamwow;
	char		inlog;
	char		lastline;
	char		lastfile[6];
	char		lastlfile[6*(UWIN_SHARE_NUM_MUTEX+1)];
} Uwin_share_t;

#endif
