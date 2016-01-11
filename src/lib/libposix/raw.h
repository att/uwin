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
#if ! defined( PIPEPOLLTIME )
#	define PIPEPOLLTIME	250
#endif

#define PDEV_REINITIALIZE 	0x0001	// pdev must be reinitialized
#define PDEV_INITIALIZE		0x0002	// pdev must be initialied
#define PDEV_INITIALIZING	0x0004	// pdev is initializing
#define PDEV_INITIALIZED	0x0008	// the dev_startthreads is in progress
#define PDEV_ACTIVE 		0x0010	// pdev is running
#define PDEV_SUSPENDED		0x0020	// pdev is suspended
#define PDEV_SHUTDOWN		0x0040	// pdev is shut(ting) down
#define PDEV_ABANDONED		0x0080	// pdev was abandoned by owner of threads
#define PDEV_CLOSED		0x0100	// unique flag for master pty only.
					// indicates that a slave was opened,
					// but has now closed.  This is used
					// in the ptym_read function to give
					// an EOF condition.

#define TABARRAYSIZE	80
#define RESETEVENT	1	// reset the event
#define	PULSEEVENT	2	// pulse the event
#define SETEVENT	3	// set the event
#define WAITEVENT	4	// wait GTLPIPEPOLLTIME for event
#define	EVENT_TYPE	1
#define	PROCESS_TYPE	2
#define	MUTEX_TYPE	3
#define PIPE_TYPE	4
#define	THREAD_TYPE	5
#define	FILE_TYPE	6
#define DEVICE_TYPE	7
#define CSB_TYPE	8
#define MAX_PDEV_CACHE_SIZE	20

#define INIT 		0
#define BUTTON_DOWN	1
#define FINAL		2
#define CLIP_MAX	4096
#define PIPE_SIZE	2048
#define WRITE_CHUNK	512

#define FILL 0x70 /* Reverse Text Mode */
#ifndef MAX_CANON
#   define MAX_CANON	256
#endif
#define EOF_MARKER	0xff
#define	DEL_CHAR	0177
#define ESC_CHAR	033
#define CNTL(x)		((x)&037)
#ifndef CBAUD
#   define CBAUD	0xf
#endif
#define TABSTOP		8

#define TABARRAY(pdev)	((unsigned char*)pdev->io.physical.tab_array)
#define PUTBUFF(x)	do {				\
				(PBUFF[CURINDEX++] = (x)); \
				if(CURINDEX == MAX_CANON)	\
					CURINDEX = 0;	\
			} while(0)
#define PUTCHAR_RPIPE(x) do {				\
				if(!WriteFile(pdev->io.read_thread.output, \
					(x),1, &wrote, NULL))	\
					logerr(0,"WriteFile");		\
			} while(0)
#define PUTPBUFF_RPIPE				\
{						\
	if(CURINDEX > STARTINDEX)		\
	{					\
		if(!WriteFile(pdev->io.read_thread.output, PBUFF, CURINDEX, &wrote, NULL))					\
			logerr(0,"WriteFile");			\
		if(!PulseEvent(pdev->io.input.event)) logerr(0,"Pulse"); \
	}							\
}
#if 1
#define OUTPUTCHAR(t,x,n)					\
	do {							\
		if(!WriteFile (pdev->io.physical.output,	\
				(x), 				\
				(n), 				\
				&wrote,				\
				NULL ))				\
			logerr(0,"WriteFile");			\
	} while(0);

#else
#define OUTPUTCHAR(x,n) 	do {					\
		if(ISECHO)  			\
		{				\
			if(!WriteFile (pdev->io.physical.output,(x), (n), &wrote,NULL )) logerr(0,"WriteFile");	\
		}				\
	} while(0)
#endif
#define PRINTCNTL(a)	do {\
					if (ISECHOCTL)\
					{\
						char x[2]; \
						*x = '^';\
						x[1] = (a) ^ 64;\
						OUTPUTCHAR(x, 2);\
					}\
			} while(0)
#define ERASECHAR	do {\
				if(ISECHOE)\
					OUTPUTCHAR("\b \b",3);	\
				else	\
					OUTPUTCHAR("\b",1);	\
			} while(0)
#define CHECKBUFF(x)	( PBUFF[CURINDEX - 1] == (x))
#define STARTINDEX	0
#if 1
#define CURINDEX	pdev->io.input.iob.index
#else
#define CURINDEX	pdev->io.write_thread.iob.index
#endif
#define PBUFF		((char*)(pdev->io.write_thread.iob.buffer))
#define RESETBUFF	(CURINDEX = STARTINDEX)

#define IS_ARROWKEY(a) (((a) >= VK_END) && ((a) <= VK_DOWN))
#define IS_FPADKEY(a) ((a) >= VK_F1 && (a) <= VK_F12)
#define MAX_CHARS	20

#define RAWVMIN		(pdev->tty.c_cc[VMIN])
#define RAWVTIME	(pdev->tty.c_cc[VTIME])
#if 0
#define ISCANON 	(pdev->tty.c_lflag & ICANON)
#else
#define ISCANON		((pdev->major==PTY_MAJOR)?((pdev->io.master.masterid)?(dev_ptr(pdev->io.slave.iodev_index)->tty.c_lflag&ICANON):pdev->tty.c_lflag&ECHO):pdev->tty.c_lflag&ICANON)
#endif
#define ISECHO 		((pdev->major==PTY_MAJOR)?((pdev->io.master.masterid)?(dev_ptr(pdev->io.slave.iodev_index)->tty.c_lflag&ECHO):pdev->tty.c_lflag&ECHO):pdev->tty.c_lflag&ECHO)
#define ISECHOE		(pdev->tty.c_lflag & ECHOE)
#define ISECHOK		(pdev->tty.c_lflag & ECHOK)
#define ISECHOCTL	(pdev->tty.c_lflag & ECHOCTL)
#define ISSPACECHAR(x)	(((x) == ' ')|| ( (x) =='\t'))
#define ISCNTLCHAR(x)	(((x) < ' ') || ((x)==DEL_CHAR))
#define IS_ISIG		(pdev->tty.c_lflag & ISIG)
#define ISDIGIT(x)	((x) >= '0'  && (x) <= '9')
#define INIT_STATE		0
#define ESCAPE_STATE 	1
#define SQBKT 			2
#define RSQBKT 			3
#define FIRST_PARAM		4
#define	SECOND_PARAM	5
#define	THIRD_PARAM		6
#define CHAR_VALUE		7
#define TITLE_STATE		8
#define ERROR_1			9

#if ! defined(PIPEPOLLTIME)
#	define PIPEPOLLTIME	250
#endif
#if ! defined(PIPE_SIZE)
#	define PIPE_SIZE	4096
#endif

//////////////////////////////////////////////////////////////////////
//	For the control_device_function
//////////////////////////////////////////////////////////////////////
#	define READ_THREAD	1
#	define WRITE_THREAD	2

#	define STOP_DEVICE	1
#	define SUSPEND_DEVICE	2
#	define FLUSH_DEVICE	3
#	define RESUME_DEVICE	4
#	define SHUTDOWN_DEVICE	5
#	define WAITFOR_DEVICE	6

#if ! defined(MAX_CHARS)
#	define MAX_CHARS	20
#endif
#if ! defined(MAX_PDEV_CACHE_SIZE)
#	define MAX_PDEV_CACHE_SIZE	20
#endif
#undef CSBHANDLE
#define CSBHANDLE	((pdev->Cursor_Mode==FALSE)?pdev->io.physical.output:pdev->NewCSB)

struct mouse_state
{
	int	state;
	int	length;
	COORD	coordStart;
	COORD	coordEnd;
	COORD	coordPrev;
	COORD	coordBegin;
	COORD	coordNew;
	COORD	coordTemp;
	BOOL	drag;
	BOOL	mode;
	BOOL	clip;
};

struct _clipdata
{
	int	count;
	int	state;
};

extern BOOL keypad_xmit_flag;

//	public functions from pdev.c
extern int 	pdev_close(Pdev_t *);
extern int 	pdev_closehandle(HANDLE);
extern HANDLE 	pdev_createEvent(int);
extern int 	pdev_dup(HANDLE, Pdev_t *, HANDLE, Pdev_t *);
extern int 	pdev_duphandle(HANDLE, HANDLE, HANDLE, HANDLE *);
extern int 	pdev_init(Pdev_t *, HANDLE, HANDLE);
extern HANDLE 	pdev_lock(Pdev_t *, int);
extern int 	pdev_unlock(HANDLE, int);

// 	public functions from pdevutils.c
extern int	pdev_event(HANDLE, int, int);
extern int 	pdev_waitread(Pdev_t *, int, int, int);
extern int 	pdev_waitwrite(HANDLE, HANDLE, HANDLE, int,HANDLE);

extern void 	termio_init(Pdev_t *);
extern int 	wait_foreground(Pfd_t *);

//	public functions from pdevcache.c
extern Pdev_t * pdev_cache(Pdev_t *, int);
extern void     pdevcachefork(void);
extern Pdev_t * pdev_lookup(Pdev_t *);
extern int 	pdev_uncache(Pdev_t *);

extern int	device_open(int,HANDLE, Pfd_t *, HANDLE);
extern void 	device_reinit(void);
extern int	device_close(Pdev_t *);
extern Pdev_t * pdev_device_lookup(Pdev_t *);

//	public functions from rawdev.c
extern int  device_control(Pdev_t *, int, int);
extern int  device_isreadable(HANDLE);
extern int  flush_device(Pdev_t *, int);
extern int  pulse_device(HANDLE, HANDLE);
extern void reset_device(Pdev_t *);
extern int  resume_device(Pdev_t *, int);
extern int  shutdown_device(Pdev_t *, int);
extern int  suspend_device(Pdev_t *, int);

///////////////////////////////////////////////////////////////////////
//	io devices
//
//	Every low level i/o device is allowed an open, read, write, and
//	close function.   These are in addition to the section(3)
//	equivalents.
//
//
//	The i/o devices table *must* correspond with the character
//	device table as described in xinit.c
////////////////////////////////////////////////////////////////////////

#if 0
#define sigdefer(p,n)	((p)->siginfo.sigsys=(n))
#endif
