/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2013 AT&T Intellectual Property          *
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
#ifndef _FSNT_H
#define _FSNT_H	1

#ifdef  __BASE__
#undef  __FILE__
#define __FILE__	__BASE__
#endif

#include	"headers.h"
#include	"common.h"
#include	"dl.h"

#include	"uwin_share.h"
#include	"uwin_keys.h"

#define FZ	10000000	/* FILETIME/sec == 100 nsec / sec */

#define ST_SIZE(st)	(((struct stat64*)(st))->st_size)

#ifdef extern
#   undef extern
#   define _WAS_extern	1
#endif

typedef struct Blockhead_s
{
	long		next;
} Blockhead_t;

struct qword
{
	unsigned long	low;
	unsigned long	high;
};

typedef BOOL (WINAPI* Shamwowed_f)(HANDLE, BOOL*);

#define fold(x)		((x)|040)
#define unfold(x)	((x)&~040)

#define HIGHWORD(x)	(((struct qword*)&x)->high)
#define LOWWORD(x)	(((struct qword*)&x)->low)
#define QUAD(high,low)  ((((__int64)((unsigned)(high)))<<32)|(unsigned)(low))

#ifdef _WIN32
#   include	<windows.h>
#endif

#include "clone.h"

typedef struct State_s
{
	char		sys[128];		/* /sys path		*/
	char		root[128];		/* root path		*/
	char		home[128];		/* root path		*/
	char		uroot[128];		/* unix root path	*/
	char		uhome[128];		/* unix root path	*/
	char		dll[128];		/* dll path		*/
	char		logpath[128];		/* log file path	*/
	char		resource[64];		/* resource prefix	*/
	char		rel[64];		/* release string	*/
	char*		pwd;			/* pwd path		*/
	char*		share;			/* shared mem address	*/
	char*		local;			/* local resource	*/
	HKEY		key;			/* root key		*/
	HANDLE		init_handle;		/* init process handle	*/
	HANDLE		log;			/* log file handle	*/
	HANDLE		map;			/* shared mem map handle*/
	HANDLE		sevent;			/* interval event	*/
	HANDLE		sigevent;		/* signal event		*/
	DWORD		process_query;		/* PROCESS_QUERY_* flag	*/
	DWORD		beat;			/* keep_alive beat	*/
	unsigned long	system;			/* System pid		*/
	int		init;			/* Share initialization	*/
	int		install;		/* installation uwin	*/
	int		rootlen;		/* state.root len	*/
	int		homelen;		/* state.home len	*/
	int		urootlen;		/* state.uroot len	*/
	int		uhomelen;		/* state.uhome len	*/
	int		pwdlen;			/* state.pwd len	*/
	int		standalone;		/* standlone uwin	*/
	int		shamwow;		/* STARTUPINFO hack	*/
	int		test;			/* test bits		*/
	int		wow;			/* 32 uwin on 64	*/
	int		wow32;			/* 64 uwin with 32 view	*/
	int		wowx86;			/* /c/program files (x86)*/
	Shamwowed_f	shamwowed;		/* is handle 32/64 bit	*/
	void		(*trace_done)(int);	/* trace cleanup	*/
	_CLONE_STATE_
} State_t;

#ifdef myCloseHandle
#   undef myCloseHandle
#   define CloseHandle(x)	myCloseHandle((x),__FILE__,__LINE__)
    extern int myCloseHandle(HANDLE, const char*, int);
#endif

#define LOG_LEVEL_BITS	3
#define LOG_LEVEL	00007
#define LOG_TYPE	00370

#define LOG_DEV		00010	/* 'd' */
#define LOG_IO		00020	/* 'i' */
#define LOG_PROC	00040	/* 'p' */
#define LOG_REG		00100	/* 'r' */
#define LOG_SECURITY	00200	/* 's' */

#define LOG_INIT	00400
#define LOG_SYSTEM	01000

#define logshow(d)	(((d)&state.init)||P_CP&&((((d)&LOG_LEVEL)<=(P_CP->debug&LOG_LEVEL))&&(!((d)&LOG_TYPE)||!(P_CP->debug&LOG_TYPE)||(P_CP->debug&(d)&LOG_TYPE)))||!P_CP&&((d)&LOG_LEVEL)<=state.install)
#define logerr(d,a ...)	(logshow(d)?logsrc((d)|LOG_SYSTEM,__FILE__,__LINE__,a):0)
#define logmsg(d,a ...)	(logshow(d)?logsrc(d,__FILE__,__LINE__,a):0)

#define WOW_C_32	0x01
#define WOW_C_64	0x02
#define WOW_MSDEV_32	0x04
#define WOW_MSDEV_64	0x08
#define WOW_REG_32	0x10
#define WOW_REG_64	0x20
#define WOW_SYS_32	0x40
#define WOW_SYS_64	0x80

#define WOW_ALL_32	(WOW_C_32|WOW_MSDEV_32|WOW_REG_32|WOW_SYS_32)
#define WOW_ALL_64	(WOW_C_64|WOW_MSDEV_64|WOW_REG_64|WOW_SYS_64)

#define roundof(x,y)	(((x)+(y)-1)&~((y)-1))

extern void* _ast_calloc(size_t,size_t);
extern void* _ast_realloc(void*,size_t);
extern void* _ast_malloc(size_t);
extern void _ast_free(void*);

/* headers for special files */
#define FAKELINK_MAGIC	"!<symlink>"
#define FAKEFIFO_MAGIC	"!<fifo>"
#define FAKEBLOCK_MAGIC	"!<block>"
#define FAKECHAR_MAGIC	"!<char>"
#define FAKESOCK_MAGIC	"!<socket>"
#define INTERLINK	"IntxLNK"

/* critical region names */
#define CR_MALLOC	0
#define CR_CHDIR	1
#define CR_FILE		2
#define CR_SPAWN	3
#define CR_SIG		4

/* handle types - used for debugging */
#define HT_NONE		0
#define HT_FILE		0x00001
#define HT_PIPE		0x00002
#define HT_EVENT	0x00004
#define HT_MUTEX	0x00008
#define HT_PROC		0x00010
#define HT_THREAD	0x00020
#define HT_TOKEN	0x00040
#define HT_MAPPING	0x00080
#define HT_OCONSOLE	0x00100
#define HT_ICONSOLE	0x00200
#define HT_COMPORT	0x00400
#define HT_SOCKET	0x00800
#define HT_MAILSLOT	0x01000
#define HT_CHANGE	0x02000
#define HT_SEM		0x04000
#define HT_CHAR		0x08000
#define HT_NPIPE	0x10000
#define HT_DIR		0x20000
#define HT_NOACCESS	0x40000
#define HT_UNKNOWN	0xffffff

/* values from Microsoft C library */
#define F_OPEN		1
#define F_PIPE		8
#define F_APPEND	0x20
#define F_DEV		0x40
#define F_TEXT		0x80

#define SID_UID		0
#define SID_GID		1
#define SID_EUID	2
#define SID_EGID	3
#define SID_ADMIN	4
#define SID_OWN		5
#define SID_SYSTEM	6
#define SID_SUID	7
#define SID_SGID	8
#define SID_WORLD	9

#define MIN_CONSOLE_MINOR 	10 	/* Minimum console minor number */
#define MAX_CONSOLE_MINOR 	256	/* Maximum console minor number */
#define MIN_TTY_MINOR 		0	/* Minimum tty minor number */
#define MAX_TTY_MINOR 		10	/* Maximum tty minor number */
#define MIN_PTY_MINOR 		0	/* Minimum pty minor number */
#define MAX_PTY_MINOR 		128	/* Maximum pty minor number */
#define MIN_MOD_MINOR		0	/* Maximum mod minor number */
#define MAX_MOD_MINOR		7	/* Maximum mod minor number */

/* Device Major Numbers */
#define NULL_MAJOR  		0	/* Major device number for /dev/null */
#define TTY_MAJOR  		1	/* Major device number for ttys */
#define CONSOLE_MAJOR		2 	/* Major device number for consoles */
#define PTY_MAJOR  		3 	/* Major device number for ptys */
#define MODEM_MAJOR		9	/* Major device number for modems */
#define AUDIO_MAJOR		13	/* Major device number for audio */
#define RANDOM_MAJOR		14	/* Major device number for randoms */
#define	DEVMEM_MAJOR		15	/* Major device number for /dev/mem */

/* virtual mount directories in /usr */
#define WIN_BIT		(1L<<24)
#define PROC_BIT	(1L<<25)
#define BIN_BIT		(1L<<26)
#define LIB_BIT		(1L<<27)
#define ETC_BIT		(1L<<28)
#define SYS_BIT		(1L<<29)
#define DEV_BIT		(1L<<30)
#define REG_BIT		(1L<<31)

#define WM_wait		(WM_USER+250)

/*
 * This bit must not be used by fcntl.h
 */
#define O_CDELETE			0x20	/* delete on close bit for files */
#define O_CASE				0x20	/* case sensitive directory bit */
#define O_CRLAST			O_EXCL
#define ACCESS_TIME_UPDATE 		O_CREAT
#define MODIFICATION_TIME_UPDATE	O_TRUNC

#ifndef FILE_ATTRIBUTE_REPARSE_POINT
#define FILE_ATTRIBUTE_REPARSE_POINT	0x400
#endif

#ifndef FILE_FLAG_OPEN_REPARSE_POINT
#define FILE_FLAG_OPEN_REPARSE_POINT	0x00200000
#endif

#define INIT_START_TIMEOUT		2000

/*
 * these are used to add DELETE permission to files
 */
#define DELETE_USER	0x10000
#define DELETE_ALL	0x20000

#define EFFECTIVE	0
#define REAL		1

typedef struct Pfifo_s
{
	WoW(HANDLE,	hread);		// the primary read handle
	WoW(HANDLE,	hwrite);	// the primary write handle
#if GTL
	WoW(HANDLE,	revent);	// the read event handle. used by select
					// to determine when reading is possible
	WoW(HANDLE,	wevent);	// the write event handle. used by select
					// to determine when writing is possible
#else
	WoW(HANDLE,	mutex);
#endif

	long	count;		// the total number of references

	DWORD	high;
	DWORD	low;
#if GTL
	DWORD	bytesInPipe;	// current bytes in the fifo
	DWORD	maxIn;		// maximum input size
	DWORD	maxOut;		// maximum input size
	int	oflag;		// status bits
#else
	DWORD	left;
#endif

	int	nread;		// the number of reader instances
	int	nwrite;		// the number of writer instances

#if !GTL
	unsigned short lread;	/* last reader */
	unsigned short lwrite;	/* last writer */
#endif
	short	events;		/* ready events */
	short	blocked;	/* blocked events */
} Pfifo_t;

#ifdef GTL_TERM
#   include	"pdev_gtl.h"
#else
#   include	"pdev.h"
#endif

typedef struct Path_s
{
	int		type;
	int		flags;
	DWORD		oflags;
	DWORD		cflags;
	HANDLE		hp;
	HANDLE		extra;
	unsigned long	hash;
	mode_t		mode;
	int		nlink;
	char 		*buff;
	int		buffsize;
	int		size;
	int 		shortcut;
	DWORD		name[2];
	BY_HANDLE_FILE_INFORMATION hinfo;
	char		filler[sizeof(WIN32_FIND_DATA)-sizeof(BY_HANDLE_FILE_INFORMATION)];
	char		*path;
	char		*checked;
	int		dirlen;
	int		delim;
	unsigned short	mount_index;
	int		rootdir;
	int		view;
	unsigned char	wow;
	char		pbuff[PATH_MAX+25];
} Path_t;

#ifndef SID_MAX_SUB_AUTHORITIES
#   define SID_MAX_SUB_AUTHORITIES 8
#endif

typedef struct Psid_s
{
	SID	sid;
	DWORD	extra[SID_MAX_SUB_AUTHORITIES-2];
} Psid_t;

#define MAXSID		128
extern Psid_t*	Psidtab;

#define drive_time(x)	(time_getnow((x)),((x)->dwHighDateTime<<4)|((x)->dwLowDateTime>>28))

extern Uwin_share_t*	Share;

typedef struct devtab
{
	char		*node;
	int		minor;
	unsigned char	type;
	short		vector;
	void		(*initfn)(struct devtab*);
	char		*name;
	HANDLE		(*openfn)(struct devtab*,Pfd_t*, Path_t*, int, HANDLE*);
} Devtab_t;

extern Devtab_t Blockdev[], Chardev[];

__inline unsigned short *devtab_ptr(int table, int major)
{
	register unsigned short *tp;
	tp = (unsigned short*)_blk_ptr(table);
	return((unsigned short*)_blk_ptr(tp[major]));
}

#define make_rdev(major,minor)	(((major)<<8)|((minor)&0xff))

extern int *errno_addr;

#define	dev_ptr(i)	((Pdev_t*)block_ptr(i))

#if GTL
#define FIFOTABSIZE 64
#else
#define FIFOTABSIZE 46
#endif
#define PROC_DIR	0x10000
#define PROC_SUBDIR	0x20000

extern Pfifo_t *Pfifotab;
extern int sinit;
extern int uidset;
extern char *saved_env0;
extern char **saved_env;
extern DWORD progc;
extern struct _astdll dllinfo;
extern HANDLE waitevent;
extern int proc_exec_exit;
extern DWORD main_thread;
extern int crit_init;
extern HANDLE sigevent;

extern State_t		state;

#define memicmp		_memicmp

extern int unix_err(int);
extern void sethandle(Pfd_t*,int);

extern void fs_init(void);
extern void init_init(void);
extern void reg_init(void);
extern void socket_init(void);

extern char* fmtbuf(size_t);
extern char* fmtstate(int, int);

extern void incr_refcount(Pfd_t*);
extern void decr_refcount(Pfd_t*);
extern HANDLE regopen(const char*, int, int, mode_t, HANDLE*, char**);
extern int regrename(const char*, int, const char*, int);
extern int regstat(const char*, int, struct stat*);
extern HANDLE keyopen(const char*, int, int, mode_t, char**, char**);
extern int regdelete(const char*, int);
extern int regtouch(const char*, int);
extern int regchstat (const char*, int, SID*, SID*, mode_t);
extern LONG WINAPI regclosekey(HKEY);
extern DIRSYS* reginit(const char*, int);
extern HANDLE audio_dup(int,Pfd_t*,int);

extern ssize_t fileread(int, Pfd_t*, char *, size_t);
extern ssize_t filewrite(int, Pfd_t*, char *, size_t);
extern int filefstat(int, Pfd_t*, struct stat*);
extern int ttyfstat(int, Pfd_t*, struct stat*);
extern int pipefstat(int, Pfd_t*, struct stat*);
extern void pipewakeup(int, Pfd_t*, Pfifo_t*, HANDLE, int, int);
extern int fileclose(int, Pfd_t*, int);
extern off64_t filelseek (int, Pfd_t*, off64_t, int);
extern off64_t pipelseek (int, Pfd_t*, off64_t, int);
extern off64_t ttylseek (int, Pfd_t*, off64_t, int);
extern off64_t nulllseek (int, Pfd_t*, off64_t, int);
extern HANDLE procopen(Pfd_t*, const char*, int);
extern HANDLE procselect(int, Pfd_t *, int, HANDLE);
extern ssize_t procwrite(int, Pfd_t*, char *, size_t);

extern char *pathreal(const char*,int, Path_t*);
extern int unmapfilename(char*, int, int*);

extern int tape_ioctl(int, void*, int);
extern int dsp_ioctl(int, int, int*);
extern int ioctlsocket(HANDLE, long, unsigned long*);
extern void unix_time(const FILETIME*,struct timespec*,int);
extern void winnt_time(const struct timespec*,FILETIME*);
extern clock_t proc_times(Pproc_t*,struct tms*);
extern clock_t proc_time(Pproc_t*,struct timespec*,struct timespec*);
extern int proc_setdir(char*, int, int*);
extern mode_t getstats(SECURITY_DESCRIPTOR*, struct stat*, int(*)(DWORD));
extern mode_t getperm(HANDLE);
extern int child_fork(Pproc_t*,void*,HANDLE*);
extern HANDLE open_comport(unsigned short);
extern void clock2timeval(clock_t,struct timeval*);
extern int free_term(Pdev_t*, int);
extern void term_close(Pdev_t*);
extern void termio_init(Pdev_t*);
extern void ttysetmode(Pdev_t*);
extern void stop_wait(void);
extern int wait_foreground(Pfd_t*);
extern int dtrsetclr(int, int, int);
extern int getmodemstatus(int, int*);
extern int mod_select(HANDLE, OVERLAPPED*, DWORD*, DWORD, BOOL);
#ifdef GTLNEWINITSIG
extern void initsig(void);
#else
extern void initsig(int);
#endif
extern int unixid_to_sid(int, SID*);
extern int sid_to_unixid(register SID*);
extern void setcurdir(int, int, unsigned long, int);
extern int getcurdrive(void);
extern int getcurdir(char*,int);
extern HANDLE startprog(char*, char*,int drive, HANDLE);
extern ssize_t logsrc(int, const char*, int, const char*, ...);
extern ssize_t sfsprintf(char*, size_t, const char*, ...);
extern ssize_t bprintf(char**, char*, const char*, ...);
extern ssize_t printsid(char*, size_t, SID*, int);
/* Added for pseudoterminals */
extern int try_chmod(Path_t *);
extern HANDLE try_write(const char*,int);
extern HANDLE try_own(Path_t*,const char*, DWORD, DWORD, DWORD, SECURITY_DESCRIPTOR*);
extern BOOL (PASCAL *sImpersonateLoggedOnUser)(HANDLE);
extern void post_wait_thread(Pproc_t*,int);
extern pid_t set_unixpid(Pproc_t*);


#ifdef CLOSE_HANDLE

#define HANDOP_CLOSE	0
#define HANDOP_OPEN	1
#define HANDOP_DUPF	2
#define HANDOP_DUP2	3
#define MAX_INIT_HANDLES 4096*32

typedef struct Init_handle_s
{
	unsigned short line[4];
	unsigned char seqno;
	unsigned char op;
} Init_handle_t;

extern Init_handle_t*	init_handles;

#    define set_init_handle(h,l,o) \
	if(init_handles) {\
		init_handles[(int)h].op <<=2; \
		init_handles[(int)h].op |= o; \
		init_handles[(int)h].line[init_handles[(int)h].seqno%4] = l; \
		init_handles[(int)h].seqno++; \
	}

    extern DWORD	Lastpid;
#   define setlastpid(x)	Lastpid=(x)
    extern int Xclosehandle(HANDLE,int, const char*, int);
#   define closehandle(x,y)	Xclosehandle((x),(y),__FILE__,__LINE__)
    extern HANDLE XOpenProcess(DWORD,BOOL,DWORD,const char*,int);
#   define OpenProcess(a,b,c)	XOpenProcess((a),(b),(c),__FILE__,__LINE__)
    extern XDuplicateHandle(HANDLE,HANDLE,HANDLE,HANDLE*,DWORD,BOOL,DWORD,const char*,int);
#   define DuplicateHandle(a,b,c,d,e,f,g) XDuplicateHandle(a,b,c,d,e,f,g,__FILE__,__LINE__)
#else
#   define setlastpid(x)
#   ifdef CLOSE_HANDLE2
    extern int Xclosehandle(HANDLE,int, const char*, int);
#   define closehandle(x,y)	Xclosehandle((x),(y),__FILE__,__LINE__)
#   else
    extern int closehandle(HANDLE,int);
#   endif
#endif
extern SECURITY_DESCRIPTOR *makeprocsd(SECURITY_DESCRIPTOR*);
extern SID *sid(int);
extern ACL *makeacl(SID*, SID*,mode_t, dev_t, DWORD(*)(int));
extern SECURITY_DESCRIPTOR *makesd(SECURITY_DESCRIPTOR*,SECURITY_DESCRIPTOR*,SID*,SID*,mode_t,dev_t,DWORD(*)(int));
extern void init_sid(int);
extern int worldmask(ACL*,int);
extern int free_pty(Pdev_t*, int);
extern HANDLE pty_mcreate(Pdev_t*, const char*);
extern void _vfork_done(Pproc_t*,int);
extern int kill1(pid_t,int);
extern HANDLE init_handle(void);
extern int procstat(Path_t*, const char*, struct stat*, int);
extern void _badrefcount(int, Pfd_t*, int, const char *, int);
#define badrefcount(a,b,c)	_badrefcount(a,b,c,__FILE__,__LINE__)
extern HANDLE proc_mutex(int,int);
extern HANDLE start_wait_thread(int, DWORD*);
extern pid_t nt2unixpid(DWORD);
extern int mwait(int,HANDLE*,int,unsigned long, int);
extern int dup_to_init(Pproc_t*,int);
extern int dup_from_init(Pproc_t*);
extern HANDLE findfirstfile(const char*, void *info, int);
extern void setupflags(int, int*, DWORD*, DWORD*);
extern ssize_t clipread(int, Pfd_t*, char *, size_t);
extern ssize_t clipwrite(int, Pfd_t*, const char *, size_t);
extern int eject(const char*);
extern unsigned long getinode(void*, int, unsigned long);
extern int terminate_thread(HANDLE, DWORD);
extern void sig_saverestore(void*, void*,int);
extern int getnamebyid(id_t, char *name, int, int);
extern SID *getsidbyname(const char*, SID*, int);
extern void sbrk_fork(HANDLE);
extern void map_fork(HANDLE);
extern void dll_fork(HANDLE);
extern int map_truncate(HANDLE, const char*);
extern int env_inlist(const char*, const char*);
extern int env_convert(const char*, char*, int, int);
extern char *arg_getbuff(void);
extern void arg_setbuff(char*);
extern void device_reinit(void);
extern int device_open(int,HANDLE, Pfd_t*, HANDLE);
extern int dev_getdevno(int);
extern void time_diff(FILETIME*, FILETIME*, FILETIME*);
extern void time_sub(FILETIME*, FILETIME*, ULONG);
extern int time_getboot(FILETIME*);
extern int time_getnow(FILETIME*);
extern HANDLE setprivs(void*, int, TOKEN_PRIVILEGES*(*)(void));
extern int process_dsusp(Pdev_t*, char *, int);
extern long uwin_exception_filter(unsigned long, struct _EXCEPTION_POINTERS*);
extern int uwin_pathinfo(const char*, char*, int, int, Path_t*);
extern char *uwin_deleted(const char*, char*, size_t, Path_t*);
extern int utentry(Pdev_t*,int, char*, char *);
extern int sendsig(Pproc_t*, int);
extern int adjust_window_size(int, int, HANDLE);
extern HANDLE mkevent(const char*, SECURITY_ATTRIBUTES*);
extern SECURITY_DESCRIPTOR* nulldacl(void);
extern int lock_dump(HANDLE,Pfd_t*);
extern void lock_procfree(Pfd_t*,Pproc_t*);
extern int mksocket (const char*, mode_t, DWORD, HANDLE);
extern HANDLE sock_open(Pfd_t*, Path_t*, int, mode_t, HANDLE*);
extern ssize_t read_shortcut(HANDLE, UWIN_shortcut_t*, char*, size_t);
extern int lchmod(const char*, mode_t);
extern int is_anysocket(Pfd_t*);
extern void start_sockgen(void);
extern int sock_mapevents(HANDLE, HANDLE,int);
extern int xmode(Path_t*);
extern void signotify(void);
extern int slot2fd(int);
extern int dirclose_and_delete(const char*);
extern int send2slot(int,int,int);
extern void intercept_init(void);
extern SECURITY_ATTRIBUTES *sattr(int);
extern TOKEN_PRIVILEGES *backup_restore(void);
extern int makeviewdirs(char*, char*, char*);
extern int movefile(Path_t*, const char*, const char*);
extern unsigned long trandom(void);
extern HANDLE open_proc(DWORD, DWORD);
extern int trace(HANDLE, int);

extern void _uwin_exit(int);
extern void _ast_atexit(void);
extern void tzset(void);
extern void clean_deleted(char*);

#ifdef _WAS_extern
#   define extern __EXPORT__
#endif /* _WAS_extern */

extern HANDLE createfile(const char*, DWORD,DWORD,SECURITY_ATTRIBUTES*,DWORD, DWORD,HANDLE);

#undef	CreateFile
#define CreateFile		createfile

#ifdef LONGNAMES

    extern HANDLE FindFirstFileL(const char*, WIN32_FIND_DATAA *info);
    extern DWORD GetFileAttributesL(const char*);
    extern BOOL SetFileAttributesL(const char*, DWORD);
    extern BOOL CreateDirectoryL(const char*, SECURITY_ATTRIBUTES*);
    extern BOOL RemoveDirectoryL(const char*);
    extern BOOL SetCurrentDirectoryL(const char*);
    extern BOOL GetFileSecurityL(const char*, SECURITY_INFORMATION,SECURITY_DESCRIPTOR*,DWORD,DWORD*);
    extern BOOL SetFileSecurityL(const char*, SECURITY_INFORMATION,SECURITY_DESCRIPTOR*);

#   undef FindFirstFile
#   define FindFirstFile	FindFirstFileL
#   undef SetFileAttributes
#   define SetFileAttributes	SetFileAttributesL
#   undef GetFileAttributes
#   define GetFileAttributes	GetFileAttributesL
#   undef CreateDirectory
#   define CreateDirectory	CreateDirectoryL
#   undef RemoveDirectory
#   define RemoveDirectory	RemoveDirectoryL
#   undef SetCurrentDirectory
#   define SetCurrentDirectory	SetCurrentDirectoryL
#   undef GetFileSecurity
#   define GetFileSecurity	GetFileSecurityL
#   undef SetFileSecurity
#   define SetFileSecurity	SetFileSecurityL

#endif

extern char *mbstrchr(const char*, wchar_t);
extern char *mbstrrchr(const char*, wchar_t);
#define strchr(a,b)	(MB_CUR_MAX>1?mbstrchr((a),(b)):strchr((a),(b)))
#define strrchr(a,b)	(MB_CUR_MAX>1?mbstrrchr((a),(b)):strrchr((a),(b)))

extern int assign_group(int,int);
extern int set_mandatory(int);

#include "handles.h"

#ifndef	InterlockedIncrement
#define InterlockedIncrement	InterlockedIncrementAcquire
#endif
#ifndef InterlockedDecrement
#define InterlockedDecrement	InterlockedDecrementRelease
#endif

#undef	environ
#undef	_environ
#if _X86_
#if _BLD_DLL
__declspec(dllimport)  extern char **_environ;
#endif
extern char **environ;
#endif

#endif /* _FSNT_H */
