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
/*
 * private ipc structures and reource sizes
 */

#include	<sys/shm.h>
#include	<sys/sem.h>
#include	<sys/msg.h>

#define SHMMNI	32		/* Max number of the shmid */
#define SHMMIN	1		/* min. size of the shared memory */
#define SHMMAX	(1L<<30)	/* max. size of the shared memory */
#define SHMSEG	6		/*  max. # of shared memory segments per proc */

#define MSGMNI	32		/* max # of msg queue identifiers */
#define MSGMAX	4056		 /* max size of message (bytes) */
#define MSGMNB	16384		/* default max size of a message queue */

#define SEMMNI	32		/*  max # of semaphore identifiers */
#define SEMMSL	16		/* <= 512 max num of semaphores per id */
#define SEMMNS	(SEMMNI*SEMMSL)	/* max # of semaphores in system */
#define SEMOPM	32		/* 100 max num of ops per semop call */
#define SEMVMX	32767		/* semaphore maximum value */

/* _IPC_COMMON_ is the common header for Pshm_t, Pmsg_t, and Psem_t */

#define _IPC_COMMON_			\
	WoW(HANDLE,	handle);	\
	WoW(HANDLE,	event);		\
	WoW(HANDLE,	mutex);		\
	WoW(HANDLE,	lock);		\
	long		refcount;	\
	DWORD		ntpid;		\
	DWORD		ntmpid;		\
	DWORD		lockpid;

/* Ipc is common structure header for shm, msg, and sem */
typedef struct Pipc_s
{
	_IPC_COMMON_
	struct ipc_perm	perm;
} Pipc_t;

/* note that shmid_ds, msqid_ds, and semid_ds must begin with ipc_perm struct */

typedef struct Pshm_s
{
	_IPC_COMMON_
	struct shmid_ds	ntshmid_ds;
} Pshm_t;

typedef struct Pmsg_s
{
	_IPC_COMMON_
	struct msqid_ds	ntmsqid_ds;
	long		first;
	long		last;
	long		next;
} Pmsg_t;

typedef struct Psem_s
{
	_IPC_COMMON_
	struct semid_ds	ntsemid_ds;
	WoW(Pproc_t*,	proc);
} Psem_t;

#ifdef extern
#   undef extern
#   define _WAS_extern  1
#endif
extern unsigned short *Pshmtab;
#ifdef _WAS_extern
#   define extern __EXPORT__
#endif

/*
 *  needed for backwards compatibility
 */
struct omsqid_ds
{
	struct ipc_perm		msg_perm;
	long msg_qnum;		/* number of messages in queue */
	ushort msg_qbytes;	/* max number of bytes on queue */
	pid_t msg_lspid;	/* pid of last msgsnd */
	pid_t msg_lrpid;	/* last receive pid */
	time_t msg_stime;	/* last msgsnd time */
	time_t msg_rtime;	/* last msgrcv time */
	time_t msg_ctime;	/* last change time */
	ushort msg_cbytes;	/* current number of bytes on queue */
};
