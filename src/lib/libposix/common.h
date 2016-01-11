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
#ifndef _COMMON_H
#define _COMMON_H	1

/*
 * P*_t struct members ordered to have
 * the same 32/64 alignments
 */

#include "uwin_share.h"
#include "dirsys.h"

#define VIRTUAL_HANDLE_VALUE	((HANDLE)1)

#define WOW_FD_SHIFT	1
#define WOW_FD_VIEW	0x3f

#define SOCK_HANDLE	0x40

typedef int Ptime_t;

typedef struct Ptimeval_s
{
	Ptime_t		tv_sec;
	Ptime_t		tv_usec;
} Ptimeval_t;

typedef struct Pfd_s
{
	union
	{	/* 64 bit extra info */
	unsigned long long extra64;
	unsigned long extra32[2];
	unsigned short extra16[4];
	unsigned char extra8[8];
	};

	long refcount;		/* number of references */

	unsigned short  oflag;	/* open flags */
	unsigned short	mnt;	/* block number for mount entry */
	unsigned short	sigio;	/* process block number for SIGIO */
	short		devno;	/* Additional file information */
	short		index;	/* block name index */

	unsigned char	type;	/* type of file */
	union
	{
	unsigned char	wow;	/* wow view */
	unsigned char socknetevents;	/* Socket network events */
	};
} Pfd_t;

#define isfdvalid(pp,fd) (((unsigned int)(fd) < pp->cur_limits[RLIMIT_NOFILE]) && \
	(fdslot(pp,fd) >= 0) && (fdslot(pp,fd) < Share->nfiles) && \
	(getfdp(pp,fd)->type != FDTYPE_NONE))
#define rdflag(flag) ((flag & 0x3) == O_RDONLY || (flag & 0x3) == O_RDWR)
#define wrflag(flag) ((flag & 0x3) == O_WRONLY || (flag & 0x3) == O_RDWR)
#ifndef elementsof
#define elementsof(x)	((int)sizeof(x)/(int)sizeof(x[0]))
#endif
#ifndef integralof
#define integralof(x)	(((char*)(x))-((char*)0))
#endif
#ifndef OFFSETOF
#define OFFSETOF(b,p)	((int)(((char*)(p))-((char*)(b))))
#endif
#define fdslot(p,fd)	(((unsigned int)(fd))<OPEN_MAX?((p)->fdslots[(fd)]):_fdslot((p),(fd)))
#define file_slot(fdp)	((int)((fdp) - Pfdtab))
#define getfdp(p,fd)	(&Pfdtab[fdslot((p),(fd))])
#define iscloexec(fd)	(P_CP->fdtab[(fd)].cloexec&1)
#define issocket(fd)	(P_CP->fdtab[(fd)].cloexec&SOCK_HANDLE)
#define setsocket(fd)	(P_CP->fdtab[(fd)].cloexec|=SOCK_HANDLE)
#define setcloexec(fd)	(P_CP->fdtab[(fd)].cloexec|=1)
#define streq(a,b)	(*(a)==*(b)&&!strcmp(a,b))
#define strneq(a,b,n)	(*(a)==*(b)&&!strncmp(a,b,n))
#define resetcloexec(f)	(P_CP->fdtab[(f)].cloexec&=~1)
#define fdname(slot)	((char*)block_ptr(Pfdtab[(slot)].index))

#define Phandle(f)	(P_CP->fdtab[f].phandle)
#define Xhandle(f)	(P_CP->fdtab[f].xhandle)
#define Ehandle(f)	(P_CP->fdtab[f].xtra)

#undef signask

typedef struct Psig_s
{
	unsigned long	sigmask;
	unsigned long	sigign;
	unsigned long	siggot;

	unsigned char	sigsys;
	BOOL		sp_flags;
} Psig_t;

#define sigdfl(p,num) ((p)->siginfo.sigdfl & (1L << (num-1)))
#define sigmasked(p,num) ((p)->siginfo.sigmask & (1L << (num-1)))
#define siggotten(p,num) ((p)->siginfo.siggot & (1L << (num-1)))
#define SigIgnore(p,num) ((p)->siginfo.sigign & (1L << (num-1)))
#define sigispending(p)   (!(p)->siginfo.sigsys && ((p)->siginfo.siggot & \
        (~((p)->siginfo.sigmask)) & (~((p)->siginfo.sigign))))

#define sigsetmasked(p,num, val) ((val) ? \
        ((p)->siginfo.sigmask |= (1L << (num-1))) : \
        ((p)->siginfo.sigmask &= ~(1L << (num-1))))
#define sigsetgotten(p,num, val) ((val) ? \
        ((p)->siginfo.siggot |= (1L << (num-1))) : \
        ((p)->siginfo.siggot &= ~(1L << (num-1))))
#define sigsetignore(p,num, val) ((val) ? \
        ((p)->siginfo.sigign |= (1L << (num-1))) : \
        ((p)->siginfo.sigign &= ~(1L << (num-1))))

extern int processsig(void);

typedef struct Pprocfd_s
{
	WoW(HANDLE,	phandle);	/* per process handle */
	WoW(HANDLE,	xhandle);	/* extra handle for some types */
	WoW(HANDLE,	xtra);		/* extra events for some types */

	unsigned char	cloexec;	/* close on exec flag */
	unsigned char	unused[7];	/* WoW padding */
} Pprocfd_t;

/* Pfd_t type values */
#define FDTYPE_INIT		0
#define FDTYPE_FILE		1
#define FDTYPE_TTY		2
#define FDTYPE_PIPE		3
#define FDTYPE_SOCKET		4
#define FDTYPE_DIR		5
#define FDTYPE_CONSOLE		6
#define FDTYPE_NULL		7
#define FDTYPE_DEVFD		8
#define FDTYPE_REG		9
#define FDTYPE_PROC		10
#define FDTYPE_NPIPE		11
#define FDTYPE_PTY		12
#define FDTYPE_WINDOW		13
#define FDTYPE_CLIPBOARD	14
#define FDTYPE_MOUSE		15
#define FDTYPE_MODEM		16
#define FDTYPE_CONNECT_UNIX	17
#define FDTYPE_CONNECT_INET	18
#define FDTYPE_TAPE		19
#define FDTYPE_FLOPPY		20
#define FDTYPE_TFILE		21
#define FDTYPE_FIFO		22
#define FDTYPE_XFILE		23
#define FDTYPE_DGRAM		24
#define FDTYPE_LP 		25
#define FDTYPE_ZERO 		26
#define FDTYPE_EPIPE 		27
#define FDTYPE_EFIFO 		28
#define FDTYPE_NETDIR		29
#define FDTYPE_AUDIO		30
#define FDTYPE_RANDOM		31
#define FDTYPE_MEM		32
#define FDTYPE_FULL		33
#define FDTYPE_NONE		34
#define FDTYPE_MAX		FDTYPE_NONE	/* NOTE: coordinate with log.c::fmt_fdtype[] */

extern void *Fs_open(Pfd_t*, const char*, HANDLE*, int, mode_t);
extern void *Fs_pipe(Pfd_t*, Pfd_t*, HANDLE*, HANDLE*, HANDLE*);
extern HANDLE Fs_dup(int fd, Pfd_t*, HANDLE*, int);
extern HANDLE Fs_select(int fd, int flag, HANDLE);
extern int Fs_lock(int, Pfd_t*, int, struct flock*);
extern int Fs_umask(int);

/* process states */
#define PROCSTATE_RUNNING	0
#define PROCSTATE_CHILDWAIT	1
#define PROCSTATE_SIGWAIT	2
#define PROCSTATE_READ		3
#define PROCSTATE_WRITE		4
#define PROCSTATE_SPAWN		5
#define PROCSTATE_STOPPED	6
#define PROCSTATE_SELECT	7
#define PROCSTATE_MSGWAIT	8
#define PROCSTATE_SEMWAIT	9
#define PROCSTATE_UNKNOWN	10
#define PROCSTATE_EXITED	11
#define PROCSTATE_TERMINATE     12
#define PROCSTATE_ABORT		13
#define PROCSTATE_ZOMBIE	14

#define procexited(p)		((p)->state>=PROCSTATE_EXITED && (p)->state<=PROCSTATE_ZOMBIE)

#define PROCTYPE_32		0x01
#define PROCTYPE_64		0x02
#define PROCTYPE_ISOLATE	0x04

#define EXE_UWIN		0x01	/* uwin executable	*/
#define EXE_FORK		0x02	/* fork executable	*/
#define EXE_GUI			0x04	/* gui executable	*/
#define EXE_SCRIPT		0x08	/* script executable	*/
#define EXE_32			0x40	/* 32 bit executable	*/
#define EXE_64			0x80	/* 64 bit executable	*/

#define SID_BUF_MAX		64

typedef struct Palarm_s
{
	unsigned long	remain;
	unsigned long	interval;
} Palarm_t;

struct atfork
{
	struct atfork	*next;
	void	(*prepare)(void);
	void	(*parent)(void);
	void	(*child)(void);
};

/* per thread data */
typedef struct Tthread_s
{
	Psig_t		siginfo;
} Tthread_t;

typedef union Pptr_u
{
	void		*ptr;
	Pprocfd_t	*fdptr;
	int64_t		w;
} Pptr_t;

typedef struct Pproc_s
{
	Pptr_t		_forksp;
	Pptr_t		_sigact;	/* pointer to signal handlers */
	Pptr_t		_vfork;		/* non-zero for vforked child */
	Pptr_t		_fdtab;

	FILETIME	start;

	WoW(HANDLE,	ph);		/* handle of this proc entry */
	WoW(HANDLE,	ph2);		/* handle of original proc entry if execed */
	WoW(HANDLE,	thread);
	WoW(HANDLE,	sigevent);
	WoW(HANDLE,	sevent);
	WoW(HANDLE,	trace);		/* output file for trace */
	WoW(HANDLE,	etoken);	/* effective uid token */
	WoW(HANDLE,	rtoken);	/* real uid token */
	WoW(HANDLE,	phandle);
	WoW(HANDLE,	fin_thrd);
	WoW(HANDLE,	origph);
	WoW(HANDLE,	alarmevent);
	WoW(HANDLE,	stoken);	/* saved setuid token */
	WoW(HKEY,	regdir);	/* set when pwd is registry key */
	WoW(time_t,	cutime);
	WoW(time_t,	cstime);

	Psig_t		siginfo;
	Palarm_t	tvirtual;
	Palarm_t	treal;

	long		inuse;		/* zero means free */
	DWORD		ntpid;		/* NT process id */
	DWORD		wait_threadid;
	DWORD		threadid;
	pid_t		pid, ppid, pgid, sid;
	uid_t		uid;
	gid_t		gid, egid;
	mode_t		umask;
	int		nchild;
	int		traceflags;

	unsigned short	ipctab;		/* for per-process ipc info */
	unsigned short	acp;		/* code page index */
	unsigned short	lock_wait;	/* set when waiting for a lock */
	unsigned short	usrview;	/* /usr top view directory slot */
	unsigned short	exitcode;
	unsigned short	test;		/* test bitmask */
	short		tls;		/* thread local storage index */
	short		maxfds;
	short		console; 	/* dev index for console */
	short		next;
	short		child;
	short		sibling;
	short		group;
	short		parent;
	short		ntnext;
	short		curdir;
	short		rootdir;

	unsigned char	wow;		/* default wow */
	unsigned char	posixapp;	/* Yes/no */
	unsigned char	inexec;		/* set when process is to exec */
	unsigned char	state;		/* state the process is in */
	unsigned char	notify;		/* set if stopped but status not returned */
	unsigned char	console_child;	/* may be set if parent is not uwin */
	unsigned char	debug;		/* log message debug level */
	char		admin;		/* >0 if process has backup/restore privs */
	signed char	priority;	/* process priority */
	unsigned char	procdir;	/* set when pwd is /proc (sub)dir */
	unsigned char	type;		/* PROCTYPE_* flags */
	unsigned char	argsize;	/* args[] size */

	unsigned long	cur_limits[RLIM_NLIMITS+2];
	unsigned long	max_limits[RLIM_NLIMITS+2];
	int		shmbits[2];
	unsigned short	slotblocks[4];
	short		fdslots[OPEN_MAX];
	char		cmd_line[40];
	char		prog_name[16];  /* Program name, used in ps */
	char		args[256];
	WoW(HANDLE,	con);		/* console handle */
	pid_t		trpid;	/* pid of trace process */
} Pproc_t;

#define PROCENT(n,c,g,s,t,m,f,d)	{ n, sizeof(n)-1, c, g, s, t, m, f, d },

/* cannot use PROCESS_ALL_ACCESS if user is not in administrators group */
#define USER_PROC_ARGS (STANDARD_RIGHTS_REQUIRED|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_TERMINATE|PROCESS_DUP_HANDLE|PROCESS_SET_INFORMATION|PROCESS_SUSPEND_RESUME|PROCESS_SET_SESSIONID)

typedef long (*Procconv_f)(const char*, char**, int*, int*);
typedef ssize_t (*Procget_f)(Pproc_t*, int, HANDLE);
typedef ssize_t (*Procset_f)(Pproc_t*, int, char*, size_t);
typedef ssize_t (*Proctxt_f)(Pproc_t*, int, char*, int);

typedef struct Procfile_s
{
	const char*	name;
	int		len;
	Procconv_f	conv;
	Procget_f	get;
	Procset_f	set;
	Proctxt_f	txt;
	int		mode;
	int		fork;
	int		tree;
} Procfile_t;

typedef struct Procdir_s
{
	const Procfile_t*	pf;
	DIR_next_f		getnext;
} Procdir_t;

#undef  vfork
#define forksp	_forksp.ptr
#define sigact	_sigact.ptr
#define vfork	_vfork.ptr
#define fdtab	_fdtab.fdptr

/* flags for inexec */
#define PROC_INEXEC	0x01
#define PROC_HAS_PARENT	0x02
#define PROC_EXEC_EXIT	0x04
#define PROC_DAEMON	0x08
#define PROC_MESSAGE	0x10

/* Flags for posixapp */
#define UWIN_PROC	0x01
#define POSIX_PARENT	0x02
#define POSIX_DETACH	0x04
#define NOT_CHILD	0x08
#define ON_WAITLST	0x10
#define IN_RELEASE	0x20
#define ORPHANING	0x40
#define CHILD_FORK	0x80

#if 0
__inline int _sigcheck(register Pproc_t *pp)
{
	if(!pp->siginfo.sigsys)
		return(0);
	pp->siginfo.sigsys = 0;
	if(pp->siginfo.siggot & ~pp->siginfo.sigmask & ~pp->siginfo.sigign)
	{
		ResetEvent(pp->sigevent);
		return(processsig());
	}
	return(1);
}

#define	sigcheck()	_sigcheck(P_CP)
#endif

#define FDTABSIZE	(2*1024)

extern Pfd_t *Pfdtab;
extern unsigned short *Blocktype;
extern unsigned long *Generation;

#define PROCTABSIZE 256
extern Pproc_t *Pproctab;
extern short *Pprochead;
extern short *Pprocnthead;
extern Pproc_t *P_CP;
extern DWORD	Ntpid;

extern const Procdir_t	Procdir[];

#define BLOCK_SIZE	1024

#define _blk_ptr(slot)	((void*)((char*)Pproctab + ((slot)-1)*BLOCK_SIZE))
#define block_ptr(x)	(x?_blk_ptr(x):(logerr(6,"invalid blkptr %d", x),_blk_ptr(Share->nfiles+1)))
#define block_slot(ptr)	((unsigned short)(1+(((char*)ptr-(char*)Pproctab)/BLOCK_SIZE)))

#define proc_ptr(slot)	((Pproc_t*)block_ptr(slot))
#define proc_slot(pp)	(block_slot(pp))


/* DLL internal functions */
int filesw(int, ssize_t (*)(int, Pfd_t*, char*, size_t),
                ssize_t (*)(int, Pfd_t*, char*, size_t),
                off64_t (*)(int, Pfd_t*, off64_t, int),
                int (*)(int, Pfd_t*, struct stat*),
                int (*)(int, Pfd_t*,int),
		HANDLE (*)(int, Pfd_t*,int, HANDLE));

#define HASH_MPY(h)	((h)*0x63c63cd9L)
#define HASH_ADD(h)	(0x9c39c33dL)
#define HASHPART(h,c)	(h = HASH_MPY(h) + HASH_ADD(h) + (c))
extern unsigned long hashstring(unsigned long, const char*);
extern unsigned long hashnocase(unsigned long, const char*);
extern unsigned long hashname(const char*,int);
extern unsigned long hashpath(unsigned long, const char*,int, char**);
extern int pathcmp(const char*, const char*, int);
extern int prefix(const char*, const char*, int);
extern int suffix(char*, char*, int);

/* pathreal flags */
#define P_LSTAT		0x00000001
#define P_SPECIAL	0x00000002
#define P_SYMLINK	0x00000004
#define P_SLASH		0x00000008
#define P_NOMAP		0x00000010
#define P_USECASE	0x00000020	/* set if under case sensitive mount point */
#define P_NOFOLLOW	0x00000040	/* don't follow link */
#define P_EXIST		0x00000080	/* error if name doesn't exist */
#define P_NOEXIST	0x00000100	/* error of name exists */
#define P_DIR		0x00000200	/* must be a directory */
#define P_FILE		0x00000400	/* cannot be a directory */
#define P_NOHANDLE	0x00000800	/* don't return an open handle */
#define P_EXEC		0x00001000	/* requires execute */
#define P_CASE		0x00002000	/* check ignore case */
#define P_DELETE	0x00004000	/* open for deletion */
#define P_TRUNC		0x00008000	/* open with truncation */
#define P_USEBUF	0x00010000	/* use the buffer field */
#define P_STAT		0x00020000	/* only stat info needed */
#define P_FORCE		0x00040000	/* force write if possible */
#define P_MYBUF		0x00080000	/* path buffer supplied by caller */
#define P_HPFIND	0x00100000	/* handle is from findfile */
#define P_CHDIR         0x00200000	/* chdir for try_own */
#define P_CREAT         0x00400000	/* create file */
#define P_WOW		0x00800000	/* use the wow field */
#define P_VIEW		0x01000000	/* use the view field */
#define P_BIN		0x02000000	/* path in wow binary view */
#define P_LINK		0x04000000	/* hard link target */

/* block types */
#define BLK_MASK	0xf
#define BLK_FREE	BLK_MASK
#define BLK_VECTOR	0 /* 'V' */
#define BLK_FILE	1 /* 'F' */
#define BLK_PDEV	2 /* 'T' */
#define BLK_PROC	3 /* 'P' */
#define BLK_BUFF	4 /* 'B' */
#define BLK_IPC		6 /* 'I' */
#define BLK_MNT		7 /* 'M' */
#define BLK_DIR		8 /* 'D' */
#define BLK_SOCK	10 /* 'S' */
#define BLK_LOCK	11 /* 'L' */
#define BLK_OFT		12 /* 'O' */

extern int setfdslot(Pproc_t*, int,int);
extern int _fdslot(Pproc_t*, int);
extern int fdpclose(Pfd_t*);
extern void fdcloseall(Pproc_t*);
extern int fdfindslot(void);
extern int get_fd(int);
extern void ipc_init(void);
extern void ipcfork(HANDLE);
extern void ipcexit(Pproc_t*);
#ifdef BLK_CHECK
    extern unsigned short Xblock_alloc(int, const char*, int);
    extern void Xblock_free(unsigned short, const char*, int);
#   define block_alloc(a)	Xblock_alloc(a,__FILE__,__LINE__)
#   define block_free(a)	Xblock_free(a,__FILE__,__LINE__)
#else
    extern unsigned short block_alloc(int);
    extern void block_free(unsigned short);
#endif
extern unsigned long vtick_count(void);
extern char *path_exact(const char*, char*);

extern Pproc_t *proc_create(DWORD);

#ifdef BLK_CHECK
#undef proc_release
    extern void Xproc_release(Pproc_t*,const char *,int);
#   define proc_release(p) Xproc_release((p),__FILE__,__LINE__)
#else
    extern void proc_release(Pproc_t*);
#endif /* BLK_CHECK */
extern ssize_t get_uwin_slots(Pproc_t*, int, HANDLE);
extern int proc_active(Pproc_t*);
extern Pproc_t *proc_init(DWORD,int);
extern Pproc_t *proc_locked(pid_t);
extern Pproc_t *proc_getlocked(pid_t, int);
extern Pproc_t *proc_ntgetlocked(DWORD, int);
extern DWORD	proc_ntparent(Pproc_t*);
extern void proc_addlist(Pproc_t*);
extern int proc_deletelist(Pproc_t*);
extern Pproc_t *proc_findlist(pid_t);
extern void proc_orphan(pid_t);
extern void proc_reparent(Pproc_t*,int);
extern void proc_setntpid(Pproc_t*,DWORD);
extern void proc_setcmdline(Pproc_t*,char *const[]);
extern Procfile_t* procfile(char*, int, int*, int*, int*, unsigned int*);
extern HANDLE procselect(int, Pfd_t*, int, HANDLE);
extern int proc_tty(Pproc_t*, int, char*, size_t);
extern void reset_winsock(void);	/* winsock cleanup */
extern int is_administrator(void);
extern int is_admin(void);
extern int impersonate_user(HANDLE);
extern void breakit(void);
#if BLK_CHECK
    extern int XP(int,const char*,int);
    extern void XV(int,int,const char*,int);
#   define P(a)	XP(a,__FILE__,__LINE__)
#   define V(a,b)	XV(a,b,__FILE__,__LINE__)
#else
    extern int P(int);
    extern void V(int,int);
#endif /* BLK_CHECK */
extern void begin_critical(int);
extern void end_critical(int);
extern int sigdefer(int);
extern int sigcheck(int);
extern int terminate_proc(DWORD, DWORD);

#endif
