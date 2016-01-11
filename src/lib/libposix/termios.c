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
#include 	<string.h>
#include 	<time.h>
#include	"vt_uwin.h"
#include	"mnthdr.h"

extern BOOL keypad_xmit_flag;
extern void ReadModemThread(Pdev_t*);
extern void WriteModemThread(Pdev_t*);
extern void WriteConsoleThread(Pdev_t*);

#define FD_CLOSE	(1<<5)
#define INIT 		0
#define BUTTON_DOWN	1
#define FINAL		2
#define CLIP_MAX	4096
#define PIPE_SIZE	4096
#define WRITE_CHUNK	1024
static BOOL Debug = FALSE;
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
}mouse;

struct
{
	int	count;
	int	state;
} ClipData;

static char *buffer;
static WORD   color_attrib;
static unsigned int  UNFILL;
static DWORD BytesWritten;

static void MarkRegion(Pdev_t *,int);
static SHORT getConX(HANDLE);
static void select_word(COORD,Pdev_t*);
WORD GetConsoleTextAttributes(HANDLE hConsole);

#define FILL 0x70 /* Reverse Text Mode */

#ifndef MAX_CANON
#   define MAX_CANON	256
#endif /* MAX_CANON */
#ifndef MAX_MODEM_SIZE
#   define MAX_MODEM_SIZE	512
#endif

#define EOF_MARKER	0xff
#define	DEL_CHAR	0177
#define ESC_CHAR	033
#define CNTL(x)		((x)&037)
#ifndef CBAUD
#   define CBAUD	0xf
#endif
#define TABSTOP		8

#undef CSBHANDLE
#define CSBHANDLE	((devtab->Cursor_Mode==FALSE)?devtab->DeviceOutputHandle:devtab->NewCSB)

#define TABARRAY(dev)	((unsigned char*)proc_ptr((dev)->slot)+MAX_CANON)

#define PUTBUFF(x)	do {				\
				(PBUFF[CURINDEX++] = (x)); \
				if(CURINDEX == MAX_CANON)	\
					CURINDEX = 0;	\
			} while(0)

#define PUTCHAR_RPIPE(x) do {				\
				WriteFile(devtab->SrcInOutHandle.ReadPipeOutput, \
					(x),1, &BytesWritten, NULL);	\
					InterlockedIncrement(&devtab->NumChar);\
			} while(0)

#define PUTPBUFF_RPIPE	do {			\
				if(CURINDEX > STARTINDEX) \
					WriteFile(devtab->SrcInOutHandle.ReadPipeOutput, PBUFF,\
						CURINDEX, &BytesWritten, NULL);	\
				InterlockedExchange(&devtab->NumChar, \
					(devtab->NumChar+ CURINDEX)); \
			} while(0)

#define CHECKBUFF(x)	( PBUFF[CURINDEX - 1] == (x))
#define STARTINDEX	devtab->ReadBuff.startIndex
#define CURINDEX	devtab->ReadBuff.curIndex
#define PBUFF		((char*)proc_ptr(devtab->slot))
#define RESETBUFF	(CURINDEX = STARTINDEX)

#define OUTPUTCHAR(x,n)	do {\
			if(ISECHO)  \
				{\
					WriteFile (devtab->SrcInOutHandle.WritePipeOutput,(x), \
						(n), &BytesWritten,NULL ); \
				}\
			} while(0)


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

#define RAWVMIN		(devtab->tty.c_cc[VMIN])
#define RAWVTIME	(devtab->tty.c_cc[VTIME])
#define ISCANON 	(devtab->tty.c_lflag & ICANON)
#define ISECHO 		((devtab->major==PTY_MAJOR)?((devtab->Pseudodevice.master)?(dev_ptr(devtab->Pseudodevice.pseudodev)->tty.c_lflag&ECHO):devtab->tty.c_lflag&ECHO):devtab->tty.c_lflag&ECHO)
#define ISECHOE		(devtab->tty.c_lflag & ECHOE)
#define ISECHOK		(devtab->tty.c_lflag & ECHOK)
#define ISECHOCTL	(devtab->tty.c_lflag & ECHOCTL)
#define ISSPACECHAR(x)	(((x) == ' ')|| ( (x) =='\t'))
#define ISCNTLCHAR(x)	(((x) < ' ') || ((x)==DEL_CHAR))
#define IS_ISIG		(devtab->tty.c_lflag & ISIG)

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

static int GetTheChar(Pfd_t *, char *,int );
static void BuildDcbFromTermios(DCB *, const struct termios *);
static int checkfg_wr(Pdev_t*);
static void ReadPtyThread(Pdev_t*);
static void WritePtyThread(Pdev_t*);
static int getblock_console(int);
static int getblock_tty(int);

static is_alphanumeric(int c)
{
	return(c=='_' || (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z'));
}

void ttysetmode(Pdev_t *pdev)
{
	DWORD mode=0;
	HANDLE event;
	char ename[UWIN_RESOURCE_MAX];
	logmsg(LOG_DEV+3, "ttysetmode dev=<%d,%d>", block_slot(pdev), pdev->major);
	if(pdev->major==CONSOLE_MAJOR)
	{
		/*Set console mode to raw mode, to read character by character*/
		HANDLE hp = pdev->DeviceInputHandle;
		if(GetConsoleMode(hp, &mode))
		{
			mode = ENABLE_WINDOW_INPUT|ENABLE_MOUSE_INPUT;
			if(!SetConsoleMode(hp, mode))
				logerr(0, "SetConsoleMode handle=%p", hp);
		}
		if(GetHandleInformation(hp,&mode)&&!(mode&HANDLE_FLAG_INHERIT))
			SetHandleInformation(hp,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);

	}
	pdev->state = 1;
	UWIN_EVENT_S(ename, pdev->NtPid);
	if(event=OpenEvent(EVENT_MODIFY_STATE,FALSE,ename))
	{
		SetEvent(event);
		closehandle(event,HT_EVENT);
	}
}

int tcgetattr(int fd, struct termios *tp)
{
	Pfd_t *fdp;

	if (isatty (fd) <= 0)
		return(-1);
	if(IsBadWritePtr((void*)tp,sizeof(struct termios)))
	{
		errno = EFAULT;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(dllinfo._ast_libversion >= 10)
		*tp = dev_ptr(fdp->devno)->tty;
	else
		memcpy((void*)tp,(void*)&(dev_ptr(fdp->devno)->tty),sizeof(*tp)-(NCCS-16)*sizeof(long));
	return(0);
}

int tcsetattr (int fd, int action, const struct termios *tp)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	DCB	dcb;
	HANDLE hCom, hSync;

	if (isatty (fd) <= 0)
		return(-1);
	if(IsBadReadPtr((void*)tp,sizeof(struct termios)))
	{
		errno = EFAULT;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	/* Checking for invalid cases */
	if (( action != TCSANOW ) && ( action != TCSADRAIN ) && (action != TCSAFLUSH))
	{
		errno = EINVAL;
		return ( -1);
	}
	pdev = dev_ptr(fdp->devno);
	if(P_CP->console==fdp->devno && !checkfg_wr(pdev))
	{
		errno = EIO;
		return(-1);
	}

	switch(action)
	{
	    case TCSAFLUSH:
		/* Discard all input received, but not read */
		if(pdev->major != MODEM_MAJOR)
		{
			HANDLE hp;
			DWORD avail=1, nread;
			char buff[512];
			if(hp=dev_gethandle(pdev, pdev->SrcInOutHandle.ReadPipeInput))
			{
				while(PeekNamedPipe(hp,NULL,0,NULL,&avail, NULL) && avail)
				{
					if(!ReadFile(hp, buff, sizeof(buff), &nread, NULL))
					{
						logerr(0, "ReadFile");
						break;
					}
				}
				if(avail)
					logerr(0, "PeekNamedPipe");
				closehandle(hp,HT_PIPE);
			}
		}
		else
		{
			/* For Modems */
		}
		/* Fall through */
	    case TCSADRAIN:
		/* Wait for all ouput to get transmitted */
		if(pdev->major != MODEM_MAJOR)
		{
			if(hSync=dev_gethandle(pdev,pdev->WriteSyncEvent))
			{
				int sigsys = sigdefer(1);
				if(WaitForSingleObject(hSync,4000) != WAIT_OBJECT_0)
					logerr(0, "WaitForSingleObject");
				closehandle(hSync,HT_EVENT);
				sigcheck(sigsys);
			}
		}
		else
		{
			/* For Modems */
		}
		/* Fall through */
	    case TCSANOW:
		pdev->tty = *tp;
		if(!pdev->tty.c_cc[VDISCARD] && pdev->discard)
			pdev->discard = FALSE;
		if(pdev->major==TTY_MAJOR)
		{
			ZeroMemory(&dcb, sizeof(DCB));
			hCom = dev_gethandle(pdev, pdev->DeviceInputHandle);
			GetCommState(hCom, &dcb);
			BuildDcbFromTermios(&dcb, tp);
			SetCommState(hCom, &dcb);
			closehandle(hCom,HT_ICONSOLE);
		}
		if(pdev->major==MODEM_MAJOR)
		{
			COMMTIMEOUTS comm_timeouts = {1, 0, 0, 0, 0};
			hCom = Phandle(fd);
			if (0 < tp->c_cc[VTIME])
				comm_timeouts.ReadIntervalTimeout = (DWORD)((tp->c_cc[VTIME]));
			if ((0 == tp->c_cc[VTIME])&&(0 == tp->c_cc[VMIN]))
				comm_timeouts.ReadIntervalTimeout = MAXDWORD;
			if(!SetCommTimeouts(hCom, &comm_timeouts))
				logerr(0, "SetCommTimeouts");
			GetCommState(hCom, &dcb);
			ZeroMemory(&dcb, sizeof(DCB));
			BuildDcbFromTermios(&dcb, tp);
			SetCommState(hCom, &dcb);
			if (tp->c_cflag & CREAD)
			{
				if(-1 == dtrsetclr(fd, TIOCM_DTR,1))
					return -1;
			}
			else
			{
				if(-1 == dtrsetclr(fd, TIOCM_DTR,0))
					return -1;
			}
		}
		break;
	}
	return(0);
}

static void BuildDcbFromTermios(DCB *pdcb, const struct termios *ptp)
{
	static int BaudRates[16] =
	{
		0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
		1800, 2400, 4800, 9600, 19200, 38400
	};
	pdcb->BaudRate	 = BaudRates[ptp->c_cflag & CBAUD];
	if (!pdcb->BaudRate )
		kill1(0, SIGHUP);
	/* Sending SIGHUP signal to the process group */

	pdcb->fBinary = TRUE;
	pdcb->fParity = ptp->c_iflag & INPCK? TRUE : FALSE;
	pdcb->Parity  = (PARENB & ptp->c_cflag)?(ptp->c_cflag & PARODD? ODDPARITY:EVENPARITY):NOPARITY;
	pdcb->StopBits = ptp->c_cflag & CSTOPB ? TWOSTOPBITS : ONESTOPBIT;
	switch((ptp->c_cflag & CSIZE))
	{
		case CS5:
			pdcb->ByteSize = 5;
			break;
		case CS6:
			pdcb->ByteSize = 6;
			break;
		case CS7:
			pdcb->ByteSize = 7;
			break;
		case CS8:
		default:
			pdcb->ByteSize = 8;
	}
	if((!ptp->c_iflag & IGNPAR)&&(!ptp->c_iflag & PARMRK))
	{
		pdcb->fErrorChar = TRUE;
		pdcb->ErrorChar = 0;
	}
	pdcb->fDtrControl = DTR_CONTROL_ENABLE;
	pdcb->fRtsControl = RTS_CONTROL_ENABLE;
	pdcb->fDsrSensitivity = FALSE;
	pdcb->fOutxDsrFlow = FALSE;
	pdcb->fOutxCtsFlow = FALSE;
	pdcb->fOutX = ptp->c_iflag & IXON ? TRUE : FALSE;
	pdcb->fInX = ptp->c_iflag & IXOFF ? TRUE : FALSE;
	pdcb->XonChar = pdcb->fInX ?(char)ptp->c_cc[VSTART]:0; //Ctrl-Q ,ASCII 17
	pdcb->XoffChar =pdcb->fInX ?(char)ptp->c_cc[VSTOP]:0; //Ctrl-S, ASCII 19
	pdcb->XonLim = 0;
	pdcb->XoffLim =0;
	pdcb->fTXContinueOnXoff = ((ptp->c_iflag & IXON)||(ptp->c_iflag & IXOFF)) ? FALSE : TRUE;
}

speed_t cfgetospeed (const struct termios *termios_p)
{
	return ((termios_p->c_cflag & CBAUD));
}

int cfsetospeed (struct termios *termios_p, speed_t speed)
{
	termios_p->c_cflag &= ~CBAUD;
	termios_p->c_cflag |= (speed & CBAUD);
	return(0);
}

speed_t cfgetispeed (const struct termios *termios_p)
{
	return ((termios_p->c_cflag & CBAUD));
}

int cfsetispeed (struct termios *termios_p, speed_t speed)
{
	termios_p->c_cflag &= ~CBAUD;
	termios_p->c_cflag |= (speed & CBAUD);
	return(0);
}

int cfmakeraw(struct termios *tp)
{
	if(!tp)
	{
		errno = EFAULT;
		return(-1);
	}
	tp->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tp->c_oflag &= ~OPOST;
	tp->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	tp->c_cflag &= ~(CSIZE|PARENB);
	tp->c_cflag |= CS8;
	return(0);
}

int tcsendbreak(int fd, int duration)
{
	Pdev_t *pdev;
	Pfd_t	*fdp;
	HANDLE	hCom;

	if (isatty (fd) <= 0)
		return(-1);

	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if(pdev->major==TTY_MAJOR)
	{
		hCom = dev_gethandle(pdev, pdev->DeviceInputHandle);
		SetCommBreak (hCom);
		Sleep ( (duration)?250:500);
		ClearCommBreak (hCom);
		closehandle(hCom,HT_ICONSOLE);
	}
	return(0);
}

int tcdrain(int fd)
{
	return(isatty(fd)-1);
}

int tcflush(int fd,int que)
{
	Pdev_t *pdev;
	Pfd_t	*fdp;
	char ch[4096];
	DWORD ByteRead;
	HANDLE hR_Th, hR_Pipe;
	if (isatty (fd) <= 0)
		return(-1);
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);

	if(pdev->major==TTY_MAJOR)
	{
		switch (que)
		{
		    case TCIFLUSH:
		    case TCIOFLUSH:
			if (pdev->NumChar)
			{
				hR_Th = dev_gethandle(pdev,pdev->SrcInOutHandle.ReadThreadHandle);
				SuspendThread(hR_Th);
				hR_Pipe = dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
				pdev->ReadBuff.curIndex = 0;
				ReadFile(hR_Pipe, ch, pdev->NumChar, &ByteRead, NULL);
				InterlockedExchange(&pdev->NumChar,pdev->NumChar - ByteRead);
				ResumeThread(hR_Th);
				closehandle(hR_Th,HT_THREAD);
				closehandle(hR_Pipe,HT_PIPE);
			}
			break;
		    case TCOFLUSH:
			break;
		    default:
			errno=EINVAL;
			return(-1);
		}
	}
	else
	{ /* This is for console and invalid 'que' selector error checking */
		switch (que)
		{
		    case TCIFLUSH:
		    case TCIOFLUSH:
		    case TCOFLUSH:
			break;
		    default:
			errno=EINVAL;
			return(-1);
		}
	}
#if 0
	if(que==TCIFLUSH || que==TCIOFLUSH)
	FlushConsoleInputBuffer(Phandle(fd));
#endif
	return(0);
}

static int	pclosehandle(HANDLE hp, HANDLE proc)
{
	if(proc && !DuplicateHandle(proc,hp,GetCurrentProcess(),&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		return(0);
	return(closehandle(hp,HT_EVENT|HT_PIPE|HT_ICONSOLE|HT_OCONSOLE));
}

HANDLE dev_gethandle(Pdev_t *pdev, HANDLE handle)
{
	HANDLE ph=pdev->process, hp=0, hp2=0, me=GetCurrentProcess();
	if(!handle || pdev->NtPid==0)
	{
		Sleep(10);
		return(NULL);
	}
	if(ph && (!P_CP->console || pdev!=dev_ptr(P_CP->console)))
		ph = 0;
	if(!ph)
		hp2 = ph = open_proc(pdev->NtPid,PROCESS_VM_READ);
	if(ph)
	{
		if(!DuplicateHandle(ph,handle,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
		{
			logerr(LOG_DEV+6, "dev_gethandle failed src=%p ntpid=%x name=%s", ph, pdev->NtPid, pdev->devname);
			hp = 0;
		}
		if(hp2)
			closehandle (hp2,HT_PROC);
	}
	else
		logerr(0, "open_proc");
	return(hp);
}

int tcflow(int fd,int action)
{
	Pdev_t *pdev;
	Pfd_t	*fdp;
	HANDLE	hp=0;

	if (isatty (fd) <= 0)
		return(-1);
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if(pdev->major==TTY_MAJOR)
	{
		switch(action)
		{
		    case TCOOFF:
			if(!pdev->SuspendOutputFlag)
			{
				hp = dev_gethandle(pdev, pdev->SrcInOutHandle.WriteThreadHandle);
				SuspendThread(hp);
				InterlockedExchange(&pdev->SuspendOutputFlag,1);
			}
			break;
		    case TCOON:
			if(pdev->SuspendOutputFlag)
			{
				hp = dev_gethandle(pdev,pdev->SrcInOutHandle.WriteThreadHandle);
				ResumeThread(hp);
				InterlockedExchange(&pdev->SuspendOutputFlag,0);
			}
			break;
		    case TCIOFF:
			hp = dev_gethandle(pdev, pdev->DeviceInputHandle);
			TransmitCommChar(hp, (char) pdev->tty.c_cc[VSTOP]);
			break;

		    case TCION:
			hp = dev_gethandle(pdev, pdev->DeviceInputHandle);
			TransmitCommChar(hp, (char) pdev->tty.c_cc[VSTART]);
			break;
		    default:
			errno = EINVAL;
			return(-1);
		}
		if(hp)
			closehandle(hp,HT_THREAD|HT_ICONSOLE);
	}
	else   /* This is for console error checking of invalid action values */
	{
		switch(action)
		{
		    case TCOOFF:
		    case TCOON:
		    case TCIOFF:
		    case TCION:
			break;
		    default:
			errno = EINVAL;
			return(-1);
		}
	}
	return(0);
}

pid_t tcgetpgrp(int fd)
{
	Pfd_t *fdp = getfdp(P_CP,fd);
	Pdev_t *pdev;

	if (fdp->type!=FDTYPE_PTY && isatty (fd) <= 0)
		return(-1);
	pdev = dev_ptr(fdp->devno);
	if(fdp->type==FDTYPE_PTY && pdev->Pseudodevice.master)
		pdev = dev_ptr(pdev->Pseudodevice.pseudodev);
	if(pdev->tgrp==0)
	{
		errno = ENOTTY;
		return(-1);
	}
	return(pdev->tgrp);
}

pid_t tcgetsid(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	if (isatty (fd) <= 0)
		return(-1);
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	return(pdev->devpid);
}

int tcsetpgrp(int fd, pid_t grp)
{
	Pproc_t *proc;
	Pfd_t *fdp;
	Pdev_t *pdev;

	if (isatty(fd) <= 0)
		return(-1);
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	/* Added to make sure that fd is the controlling terminal */
	if (fdp->devno != P_CP->console || P_CP->sid!=pdev->devpid)
	{
		errno = ENOTTY;
		return -1;
	}
	/* Added to verify the session id and the grp */
	proc = proc_findlist(grp);
	if (!proc)
	{
		errno = EINVAL;
		return(-1);
	}
	if (proc->sid != P_CP->sid)
	{
		errno = EPERM;
		return(-1);
	}
	pdev->tgrp = grp;
	return(0);
}

static int checkfg_wr( Pdev_t *pdev)
{
	if(P_CP->pgid != pdev->tgrp && pdev->tty.c_lflag&TOSTOP &&
		!(SigIgnore(P_CP, SIGTTOU)||sigmasked(P_CP,SIGTTOU)))
	{
		if(P_CP->ppid != 1)
			kill1(-P_CP->pgid, SIGTTOU);
		return(0);
	}
	return(1);
}

/*
 * update the modification time
 */
static void update_mtime(Pfd_t *fdp, Pdev_t *pdev)
{
	FILETIME mtime;
	HANDLE hp=0;
	char path[PATH_MAX],*name;
	Mount_t *mp;
	if(fdp->index)
		name = fdname(file_slot(fdp));
	else
	{
		mp = mount_look("",0,0,0);
		memcpy(path,mnt_onpath(mp),mp->len-1);
		memcpy(&path[mp->len-1],"/usr",4);
		strcpy(&path[mp->len+3],pdev->devname);
		name = path;
	}
	if(name && (hp=createfile(name,GENERIC_WRITE,FILE_SHARE_WRITE, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)))
	{
		time_getnow(&mtime);
		if(!SetFileTime(hp,NULL,&mtime,&mtime))
			logerr(0, "SetFileTime");
		CloseHandle(hp);
	}
}

static ssize_t ttywrite(int fd, register Pfd_t *fdp, const char *buff, size_t asize)
{
	DWORD rsize=0,tsize,wsize,size;
	HANDLE hp,hpsync;
	BOOL r=1;
	Pdev_t *pdev = dev_ptr(fdp->devno);

	if(asize>SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (DWORD)asize;
	if(P_CP->console==fdp->devno && !checkfg_wr(pdev))
	{
		errno = (P_CP->ppid == 1) ? EIO : EINTR;
		return(-1);
	}
	hp = dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	InterlockedIncrement(&pdev->writecount);
	if(hpsync=dev_gethandle(pdev,pdev->WriteSyncEvent))
	{
		if(!ResetEvent(hpsync))
			logerr(0, "ResetEvent");
		closehandle(hpsync,HT_EVENT);
	}
	while(size>0)
	{
		if((tsize=size) > WRITE_CHUNK)
			tsize = WRITE_CHUNK;
		r = WriteFile(hp, buff, tsize, &wsize, NULL);
		if(!r || wsize==0)
			break;
		rsize += wsize;
		size -= wsize;
		if(size==0)
			break;
		buff += wsize;
		if(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign)
			break;
	}
	InterlockedDecrement(&pdev->writecount);
	if(!r)
	{
		errno = unix_err(GetLastError());
		if(rsize==0)
			rsize = -1;
	}
	else if(P_CP->ntpid==pdev->NtPid && pdev->WriteSyncEvent)
		/* delay session leader to prevent data loss on exit */
		WaitForSingleObject(pdev->WriteSyncEvent, 4000);
	closehandle(hp,HT_PIPE);
	if(rsize>0)
		update_mtime(fdp,pdev);
	return(rsize);
}

static int dowait(HANDLE *objects, DWORD wait_time)
{
	int val;
	while(1)
	{
		P_CP->state = PROCSTATE_READ;
		val=WaitForMultipleObjects(2,objects,FALSE,wait_time);
		P_CP->state = PROCSTATE_RUNNING;
		if (val==WAIT_OBJECT_0)
		{
			if(sigcheck(0))
			{
				sigdefer(1);
				continue;
			}
			errno=EINTR;
			return(-1);
		}
		return(val!=WAIT_TIMEOUT);
	}
}

static int setNumChar(Pdev_t *pdev)
{
	DWORD total,dummy;
	HANDLE hR_Pipe = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadPipeInput);
	if (hR_Pipe && PeekNamedPipe(hR_Pipe, NULL, 0, NULL, &total, &dummy))
		pdev->NumChar=total;
	closehandle(hR_Pipe,HT_PIPE);
	return(pdev->NumChar);
}

/*
 * waits for process to be in the foreground and stops process
 * returns -1 and sets errno on error
 */
int wait_foreground(Pfd_t *fdp)
{
	Pdev_t *pdev;
	int sigsys = sigdefer(1);
	if(fdp->devno==0)
		goto err;
	pdev = dev_ptr(fdp->devno);
	while(P_CP->console==fdp->devno && pdev->tgrp != P_CP->pgid)
	{
		if(SigIgnore(P_CP, SIGTTIN) || sigmasked(P_CP,SIGTTIN)
				|| (P_CP->ppid == 1))
			goto err;
		kill1(-P_CP->pid, SIGTTIN);
	}
	sigcheck(sigsys);
	return(0);
err:
	errno = EIO;
	sigcheck(sigsys);
	return(-1);
}

int process_dsusp(Pdev_t* pdev, char *in_buf, int nsize)
{
	int i;
	if(pdev->dsusp==0)
		return(nsize);
	if((pdev->tty.c_lflag & ISIG) && (pdev->tty.c_lflag & ICANON))
	{
		for (i=0;i<nsize;i++)
		{
			if(pdev->tty.c_cc[VDSUSP]&&(in_buf[i] == (char)pdev->tty.c_cc[VDSUSP]))
			{
				for(;i<nsize-1;i++)
					in_buf[i] = in_buf[i+1];
				pdev->dsusp--;
				kill1(-pdev->tgrp, SIGTSTP);
				return(nsize-1);
			}
		}
	}
	return(nsize);
}

static ssize_t ttyread(int fd, Pfd_t *fdp, char *buff, size_t asize)
{
	DWORD size, rsize, raw_time, raw_min;
	Pdev_t *pdev;
	HANDLE objects[2];
	int saverror = errno;
	DWORD wait_time = INFINITE;
	DWORD size_to_r, nsize = 0;
	int storedchar= 0;

	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size_to_r = size = (DWORD)asize;
	pdev = dev_ptr(fdp->devno);
	if(wait_foreground(fdp) < 0)
		return(-1);
	if(!pdev->state)
		ttysetmode(pdev);
	raw_time = pdev->tty.c_cc[VTIME];
	raw_min = pdev->tty.c_cc[VMIN];

	if(fdp->oflag&(O_NONBLOCK|O_NDELAY))
	{
		if(pdev->NumChar)
			nsize = GetTheChar(fdp, buff, size);
		else
		{
			errno = EAGAIN;
			nsize = -1;
		}
		return(nsize);
	}
	objects[0]=P_CP->sigevent;
	objects[1] = dev_gethandle(pdev, pdev->BuffFlushEvent);
	if(!objects[1])
		logerr(0, "dev_gethandle");

	if(!(pdev->tty.c_lflag & ICANON))
	{
		if((fdp->oflag&O_NONBLOCK) || (fdp->oflag&O_NDELAY))
		{
			if(pdev->NumChar)
				nsize = GetTheChar(fdp, buff, size);
			else if(fdp->oflag&O_NDELAY) /* System V ism */
				nsize = 0;
			else
			{
				errno = EAGAIN;
				nsize = -1;
			}
			goto done;
		}

		if(raw_time && !raw_min)
		{
			if(!pdev->NumChar && !setNumChar(pdev))
			{
				HANDLE hp;
				hp = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadThreadHandle);
				ResumeThread(hp);
				closehandle(hp,HT_THREAD);
				rsize=dowait(objects,raw_time*100);
				if (rsize<0)
					nsize=-1;
				else if(rsize==0)
				{
					errno = EAGAIN;
					nsize = 0;
				}
			}
			else
				nsize = GetTheChar(fdp, buff, size);
			goto done;
		}
		else if(!raw_time && !raw_min)
		{
			if(pdev->NumChar)
				nsize = GetTheChar(fdp, buff,size);
			else
			{
				errno = EAGAIN;
				nsize = 0;
			}
			goto done;
		}
	}
	while(1)
	{
		if(!pdev->NumChar && !setNumChar(pdev))
		{
			rsize=dowait(objects,wait_time);
			if (rsize<0)
			{
				nsize=-1;
				goto done;
			}
			else if (rsize==0)
			{
				errno = EAGAIN;
				if(size<=nsize)
					nsize = size;
				goto done;
			}
		}
		if(size_to_r > 0)
			nsize += GetTheChar(fdp, &buff[nsize],size_to_r);
		else
		{
			nsize += pdev->NumChar - storedchar;
			storedchar = pdev->NumChar;
		}

		if(!(pdev->tty.c_lflag & ICANON))
		{
			if(nsize >= raw_min)
			{
				if(size<=nsize)
					nsize = size;
				goto done;
			}
			if(raw_min)
			{
				if(raw_time)
				{
					wait_time = raw_time * 100;
				}

				size_to_r = size - nsize;
				if(size_to_r < 0)
					size_to_r =0;
				if(!size_to_r)
					goto done;
			}
			else
				goto done;
		}
		else
			goto done;
	}
done:
	closehandle(objects[1],HT_EVENT);
	return(process_dsusp(pdev,buff,nsize));
}

static int GetTheChar(Pfd_t *fdp, char *buff,int size)
{
	unsigned char ch;
	int total,dummy;
	int nsize = 0;
	DWORD bytesRead;
	Pdev_t *pdev;
	HANDLE hR_Pipe;
	int tsize = 0, size_r = size;

	pdev = dev_ptr(fdp->devno);
	hR_Pipe = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadPipeInput);
	if(pdev->tty.c_lflag & ICANON)
	{
		register struct termios *tp = &(pdev->tty);
		while(1)
		{
			if(!ReadFile(hR_Pipe,&ch,1, &bytesRead,NULL))
			{
				logerr(0, "ReadFile");
				closehandle(hR_Pipe,HT_PIPE);
				return(nsize);
			}
			InterlockedDecrement(&pdev->NumChar);
			if (ch != EOF_MARKER)
			{
				*buff++ = ch;
				nsize++;
			}
			if ((ch == EOF_MARKER) || (ch == '\n' || ch==tp->c_cc[VEOL] || ch==tp->c_cc[VEOL2]) || nsize >= size)
			{
				closehandle(hR_Pipe,HT_PIPE);
				return(nsize);
			}
		}
	}
	/* raw read */
	while(pdev->NumChar)
	{
		if(size_r <= 0 ) break;
		nsize = (pdev->NumChar >= size_r )? size_r:pdev->NumChar;

		if(!ReadFile(hR_Pipe, buff+tsize,nsize,&bytesRead, NULL))
			logerr(0, "ReadFile");
		if(PeekNamedPipe(hR_Pipe, NULL, 0, NULL, &total, &dummy))
			InterlockedExchange(&pdev->NumChar,total);
		tsize += bytesRead;
		size_r -= bytesRead;
	}
	closehandle(hR_Pipe,HT_PIPE);
	return(tsize);
}

void processtab(Pdev_t *devtab)
{
	int	i, pos;

	pos = TABSTOP - ((int)devtab->cur_phys % TABSTOP );

	if (!pos)
		pos = TABSTOP;

	/* if ( devtab->cur_phys + pos >= devtab->MaxCol )
		pos = (devtab->MaxCol ) - devtab->cur_phys;
	 */

	TABARRAY(devtab)[devtab->TabIndex++] = pos;

	for ( i = 0 ; i < pos ; i++)
		OUTPUTCHAR(" ", 1);
}


/*
 * perform canonical processing on character <ch>
 */
static void CharProcFunct(unsigned int ch, Pdev_t *devtab, int index)
{
	register struct termios *tp = &(devtab->tty);
	int i=0, pos=0;
	if(ch == '\\')
	{
		devtab->SlashFlag = 1;
		PUTBUFF(ch);
		OUTPUTCHAR(&ch,1);
		return;
	}
	if(ch == '\t')
	{
		devtab->SlashFlag = FALSE;
		PUTBUFF(ch);
		if(devtab->tty.c_lflag&ECHO)
			processtab(devtab);
		return;
	}
	if(devtab->SlashFlag)
	{
		devtab->SlashFlag = FALSE;
		if(index==VERASE || index==VKILL || index==VEOF)
		{
			CURINDEX--;
			PUTBUFF(ch);
			OUTPUTCHAR("\b", 1);
			if (ISCNTLCHAR(ch))
				PRINTCNTL(ch);
			else
				OUTPUTCHAR(&ch,1);
			return;
		}
	}
	switch(index)
	{
		case VERASE:
			if(CURINDEX > STARTINDEX)
			{
				ERASECHAR;
				if (PBUFF[CURINDEX - 1 ] == '\t')
				{
					pos = TABARRAY(devtab)[--devtab->TabIndex];
					for (i = 1; i < pos; i++)
						ERASECHAR;
				}
				else if(ISCNTLCHAR(PBUFF[CURINDEX - 1]))
					ERASECHAR;
				CURINDEX--;
			}
			else
				OUTPUTCHAR ("\a" ,1);
			return;
		case VWERASE:
			while(ISSPACECHAR(PBUFF[CURINDEX-1]))
			{
				if(PBUFF[CURINDEX-1] == '\t')
				{
					pos = TABARRAY(devtab)[--devtab->TabIndex];
					for (i = 1; i < pos; i++)
						OUTPUTCHAR("\b",1);
				}
				OUTPUTCHAR("\b",1);
				CURINDEX--;
			}
			while(!ISSPACECHAR(PBUFF[CURINDEX-1]) && (CURINDEX>STARTINDEX))
			{
				if(ISCNTLCHAR(PBUFF[CURINDEX-1]))
					ERASECHAR;
				CURINDEX--;
				ERASECHAR;
			}
			return;
		case VKILL:
			RESETBUFF;
			if(ISCNTLCHAR(ch))
				PRINTCNTL(ch);
			else
				OUTPUTCHAR(&ch,1);
			if(ISECHOK)
				OUTPUTCHAR ("\n",1);
			return;
		case VEOF: /* CNTL('D') */
			/* EOF is not echoed */
			PUTBUFF((char)EOF_MARKER);
			PUTPBUFF_RPIPE;  /* Put EOF ascii char to Read Pipe */
			RESETBUFF;
			return;
	}
	if(ch == '\r')
	{
		devtab->SlashFlag = FALSE;
		PUTBUFF(ch);
		OUTPUTCHAR (&ch ,1);
		return;
	}
	if((ch == tp->c_cc[VEOL]) || (ch==tp->c_cc[VEOL2]))
	{
		if(ISCNTLCHAR(ch))
			PRINTCNTL(ch);
		else
			OUTPUTCHAR(&ch,1);
		PUTBUFF(ch);
		PUTPBUFF_RPIPE; /*Put all the character in PBUFF to read Pipe */
		RESETBUFF;
		return;
	}
	if(ch == '\n')
	{
		devtab->TabIndex = 0; /* Reset keeping track of tab insertions */
		if (tp->c_lflag & ECHONL) /* echo NL even if ECHO is not set */
		{
			WriteFile (devtab->SrcInOutHandle.WritePipeOutput,
				(const void*)"\r\n", 2, &BytesWritten, NULL );
		}
		else
			OUTPUTCHAR ("\n",1);
		/* Write all the characters in the read buffer into the pipe */
		PUTBUFF(0x0a);
		PUTPBUFF_RPIPE; /*Put all the character in PBUFF to read Pipe */
		RESETBUFF;
		return;
	}
	PUTBUFF(ch);
	if(ISCNTLCHAR(ch))
		PRINTCNTL(ch);
	else
		OUTPUTCHAR(&ch,1);
}

static void TtyInitialProcess(Pdev_t *devtab, char *str)
{
	struct termios  *tp = &devtab->tty;
	char		ch;
	int 		index, pos=0;
	static char reprint_buf[512];
	static int r_index=0;

	if(devtab->SuspendOutputFlag && ((UCHAR)str[0] != devtab->tty.c_cc[VSTART]))
	{
		if (IS_ISIG && ((UCHAR)str[0] == devtab->tty.c_cc[VQUIT]
			|| (UCHAR)str[0] == devtab->tty.c_cc[VINTR]))
		{
			ResumeThread(devtab->SrcInOutHandle.WriteThreadHandle);
			InterlockedExchange(&devtab->SuspendOutputFlag, 0 );
			if(devtab->Pseudodevice.pckt_event && !SetEvent(devtab->Pseudodevice.pckt_event))
				logerr(0, "SetEvent");
		}
		else
			return;
	}

	if(devtab->discard && ((UCHAR)str[0] != devtab->tty.c_cc[VDISCARD]))
		devtab->discard = 0;
	while (ch=*str++)
	{
		switch (ch)
		{
			case '\r':
				if (!(devtab->tty.c_iflag & IGNCR))
				{
					if (devtab->tty.c_iflag & ICRNL)
						ch = '\n';
					break;
				}
				return;
			case '\n':
				if (devtab->tty.c_iflag & INLCR)
					ch = '\r';
				break;
		}
		if(devtab->LnextFlag)
		{
			if(ISCNTLCHAR(ch))
			{
				if ( ch=='\t')
					processtab(devtab);
				else
					PRINTCNTL(ch);

				PUTBUFF(ch);
				devtab->LnextFlag = FALSE;
				if(!ISCANON)
				{
					PUTPBUFF_RPIPE;
					RESETBUFF;
				}
				return;
			}
		}
		for (index = VEOF; index <=VSWTCH; ++index)
		{
			if ((ULONG)ch == tp->c_cc[index])
				break;
		}
		switch (index)
		{
		    case VLNEXT:
			devtab->LnextFlag = TRUE;
			if(ISCANON)
			{
				OUTPUTCHAR("^\b",2);/*For CANONICAL MODE ONLY */
				return;
			}
			break;
		    case VDSUSP:
			if (IS_ISIG)
				devtab->dsusp++;
			break;
		    case VSUSP:
			if (IS_ISIG)
			{
				if(CURINDEX > STARTINDEX)
					RESETBUFF;
				kill1(-devtab->tgrp, SIGTSTP);
				/* Send the SIGSTOP SIGNAl to the FG processes */
				return;
			}
			break;
		    case VSTOP:
			if (!devtab->SuspendOutputFlag && (tp->c_iflag & IXON))
			{
				SuspendThread(devtab->SrcInOutHandle.WriteThreadHandle);
				InterlockedExchange(&devtab->SuspendOutputFlag, 1 );
				if(devtab->Pseudodevice.pckt_event && !SetEvent(devtab->Pseudodevice.pckt_event))
					logerr(0, "SetEvent");
				return;
			}
			break;
		    case VSTART:
			if (devtab->SuspendOutputFlag && (tp->c_iflag & IXON))
			{
				ResumeThread(devtab->SrcInOutHandle.WriteThreadHandle);
				InterlockedExchange(&devtab->SuspendOutputFlag, 0 );
					if(devtab->Pseudodevice.pckt_event && !SetEvent(devtab->Pseudodevice.pckt_event))
					logerr(0, "SetEvent");
				return;
			}
			break;
		    case VINTR:
		    case VQUIT:
			if(IS_ISIG)
			{
				/* Send the SIGINTR SIGNAL to the FG processes */
				PRINTCNTL(ch);
				while (devtab->NumChar)
				{
					char tmp_buf[4096]; /* Input Pipe Size */
					DWORD byteread;
					DWORD to_read=0;
					int ret;
					/* Not sure about the devtab->NumChar, better check with PeekNamedPipe*/
					if(!(ret=PeekNamedPipe(devtab->SrcInOutHandle.ReadPipeInput, NULL,0, NULL,&to_read,NULL)))
						logerr(0, "PeekNamedPipe");
					if(ret && to_read > 0)
					{
					ReadFile(devtab->SrcInOutHandle.ReadPipeInput,
							tmp_buf, devtab->NumChar,
							&byteread, NULL);
					InterlockedExchange(&devtab->NumChar,
							devtab->NumChar-byteread);
					}
					else
						break;
				}
				RESETBUFF;
				kill1(-devtab->tgrp,(index == VINTR) ? SIGINT : SIGQUIT);
				errno = EINTR;
				return;
			}
			break;
		    case VREPRINT:
			if(devtab->tty.c_lflag&ICANON)
			{
				if(!WriteFile (devtab->SrcInOutHandle.WritePipeOutput,
					(const void*)"\r\n", 2, &BytesWritten, NULL ))
					logerr(0, "WriteFile");
				if(!WriteFile (devtab->SrcInOutHandle.WritePipeOutput,reprint_buf,
					r_index, &BytesWritten,NULL ))
					logerr(0, "WriteFile");
				return;
			}
			break;
		    case VDISCARD:
			if(devtab->tty.c_lflag&ICANON)
			{
				devtab->discard = !devtab->discard;
				return;
			}
			break;
		    default:
			break;
		}
		if(ch=='\r' || ch == '\n')
			r_index = 0;
		else if(ch == '\b')
			r_index--;
		else if(r_index < sizeof(reprint_buf))
			reprint_buf[r_index++]=ch;
		if(!ISCANON)
		{
			if(ISCNTLCHAR(ch))
				PRINTCNTL(ch);
			else
				OUTPUTCHAR(&ch,1);
			PUTBUFF(ch);	/* new change */
		}
		else
			CharProcFunct(ch, devtab, index);
	}
	if(!ISCANON)
	{
		PUTPBUFF_RPIPE;
		RESETBUFF;
	}
}

/*
 * duplicate all the pdev handles from <from> proc to <to> proc
 */
void dev_duphandles(Pdev_t *pdev, HANDLE from, HANDLE to)
{
	/* pdev->DeviceNewCSB not handled yet */
	logmsg(LOG_DEV+3, "dev_duphandles dev=<%d,%d>", block_slot(pdev), pdev->major);
	if(pdev->major!=CONSOLE_MAJOR)
	{
		if(!DuplicateHandle(from,pdev->DeviceInputHandle,to,&pdev->DeviceInputHandle,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(from,pdev->DeviceOutputHandle,to,&pdev->DeviceOutputHandle,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle");
	}
	if(!DuplicateHandle(from,pdev->SrcInOutHandle.ReadPipeInput,to,&pdev->SrcInOutHandle.ReadPipeInput,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(from,pdev->SrcInOutHandle.ReadPipeOutput,to,&pdev->SrcInOutHandle.ReadPipeOutput,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(from,pdev->SrcInOutHandle.WritePipeInput,to,&pdev->SrcInOutHandle.WritePipeInput,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(from,pdev->SrcInOutHandle.WritePipeOutput,to,&pdev->SrcInOutHandle.WritePipeOutput,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(from,pdev->BuffFlushEvent,to,&pdev->BuffFlushEvent,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
	if(!DuplicateHandle(from,pdev->write_event,to,&pdev->write_event,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
		logerr(0, "DuplicateHandle");
}

/*
 * start the threads for the device emulation
 * if pdev->ntpid != P_CP->pid) then all the handles are created
 */
int dev_startthreads(register Pdev_t *pdev, LPTHREAD_START_ROUTINE read_thread, LPTHREAD_START_ROUTINE write_thread)
{
	HANDLE rth, wth, pin, pout,me=GetCurrentProcess();
	SECURITY_ATTRIBUTES sattr;
	DWORD thid;

	logmsg(LOG_DEV+2, "startthreads dev=<%d,%d> started=%d slot=%d", block_slot(pdev), pdev->major, pdev->started, pdev->slot);
	if(pdev->started)
		return(1);
	ZeroMemory(&sattr,sizeof(sattr));
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = nulldacl();
#if 0
	sattr.bInheritHandle = FALSE;
#else
	sattr.bInheritHandle = TRUE;
	/* inheritable so that threads can be restarted in child */
#endif

	if(!pdev->slot)
	{
		/* this should be non-zero only first reinitializing */
		pdev->slot = block_alloc(BLK_BUFF);
		pdev->ReadBuff.startIndex = 0;
		pdev->ReadBuff.curIndex = 0;
		pdev->NumChar = 0;
		pdev->LnextFlag = FALSE;
		pdev->TabIndex = 0;
		ZeroMemory( TABARRAY(pdev), MAX_CANON);
		if(!(pdev->BuffFlushEvent=CreateEvent(&sattr,TRUE,FALSE,NULL)))
			logerr(0, "CreateEvent");
		if(!(pdev->write_event=CreateEvent(&sattr,TRUE,FALSE,NULL)))
			logerr(0, "CreateEvent");
		/* Create ReadPipe */
		if(!CreatePipe(&pin, &pout,&sattr, 0))
		{
			logerr(0, "CreatePipe");
			return(0);
		}
		pdev->SrcInOutHandle.ReadPipeInput = pin;
		pdev->SrcInOutHandle.ReadPipeOutput = pout;

		/* CreatePipe WritePipe */
		if(!CreatePipe(&pin, &pout,&sattr, WRITE_CHUNK+24))
		{
			logerr(0, "CreatePipe");
			return(0);
		}
		logmsg(LOG_DEV+3, "startthreads dev=<%d,%d> slot=%d inpipe=%p outpipe=%p", block_slot(pdev), pdev->major, pdev->slot, pin, pout);
		pdev->SrcInOutHandle.WritePipeInput = pin;
		pdev->SrcInOutHandle.WritePipeOutput = pout;
		if(pdev->major == PTY_MAJOR)
		{
			HANDLE hp;
			hp = pdev->SrcInOutHandle.ReadPipeInput;
			if(!DuplicateHandle(me,hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			pdev->Pseudodevice.m_inpipe_read = hp;
			hp = pdev->SrcInOutHandle.WritePipeOutput;
			if(!DuplicateHandle(me,hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			pdev->Pseudodevice.m_outpipe_write=hp;
		}
	}
	pdev->NtPid = P_CP->ntpid;
	/* pass process handle to all child processes */
	if(!DuplicateHandle(me,me,me, &pdev->process,PROCESS_DUP_HANDLE|PROCESS_VM_READ,TRUE,0))
		logerr(0, "DuplicateHandle");
	/* Create ReadThread */
#if 1
	sattr.bInheritHandle = FALSE;
#endif
	rth = CreateThread(&sattr,0,read_thread,pdev,CREATE_SUSPENDED,&thid);
	if(!rth)
	{
		logerr(0, "CreateThread");
		return(0);
	}
	pdev->SrcInOutHandle.ReadThreadHandle = rth;

	/* Create WriteThread */
	wth = CreateThread(&sattr,0,write_thread,pdev,CREATE_SUSPENDED,&thid);
	if(!wth)
	{
		logerr(0, "CreateThread");
		return(0);
	}
#if 1
	sattr.bInheritHandle = TRUE;
#endif
	pdev->SrcInOutHandle.WriteThreadHandle =  wth;
	if(!(pdev->WriteSyncEvent=CreateEvent(&sattr,TRUE,FALSE,NULL)))
		logerr(0, "CreateEvent");
	/* resume the device thread */
	ResumeThread(pdev->SrcInOutHandle.ReadThreadHandle);
	ResumeThread(pdev->SrcInOutHandle.WriteThreadHandle);
	pdev->started = 1;
	return(1);
}

/*
 * stop the threads associated with pdev and close the handles
 * if proc, then the threads are not in the current process
 * The proccess handle proc is closed before returning
 */
static void dev_stopthreads(register Pdev_t *pdev, HANDLE proc)
{
	HANDLE thr=0,thw=0;
	logmsg(LOG_DEV+3, "dev_stopthreads dev=<%d,%d> ntpid=%d", block_slot(pdev), pdev->major, pdev->NtPid);
	if(!pdev->NtPid)
	{
		logmsg(LOG_DEV+6, "threads already closed minor=%d", pdev->minor);
		return;
	}
#ifdef CloseHandle
	pdev->NtPid = 0;
#endif
	pclosehandle(pdev->DeviceInputHandle,proc);
	pclosehandle(pdev->DeviceOutputHandle,proc);
	pclosehandle(pdev->BuffFlushEvent,proc);
	pclosehandle(pdev->WriteSyncEvent,proc);
	pclosehandle(pdev->write_event,proc);

	if(pdev->WriteOverlappedEvent)
		pclosehandle(pdev->WriteOverlappedEvent,proc);
	if(pdev->ReadOverlappedEvent)
		pclosehandle(pdev->ReadOverlappedEvent,proc);
	if(pdev->ReadWaitCommEvent)
		pclosehandle(pdev->ReadWaitCommEvent,proc);
	if(proc)
	{
		HANDLE me = GetCurrentProcess();
		if(!DuplicateHandle(proc, pdev->SrcInOutHandle.ReadThreadHandle,me,&thr,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(proc, pdev->SrcInOutHandle.WriteThreadHandle,me,&thw,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle");
	}
	else
	{
		thr = pdev->SrcInOutHandle.ReadThreadHandle;
		thw = pdev->SrcInOutHandle.WriteThreadHandle;
	}
	if(thr)
		terminate_thread(thr, 0);
	if(thw)
		terminate_thread(thw, 0);
	pclosehandle(pdev->SrcInOutHandle.ReadPipeInput,proc);
	pclosehandle(pdev->SrcInOutHandle.ReadPipeOutput,proc);
	pclosehandle(pdev->SrcInOutHandle.WritePipeInput,proc);
	pclosehandle(pdev->SrcInOutHandle.WritePipeOutput,proc);
	if(proc)
		closehandle(proc,HT_PROC);
}

void ReadTtyThread(Pdev_t *devtab)
{
	DWORD			length, error, j, emask, transfer;
	COMSTAT			comstat;
	OVERLAPPED		os, ReadOverlapped;
	unsigned char	ch[10], Buff[MAX_CANON];
	BOOL			rc,lerr;

	logmsg(LOG_DEV+3, "ReadTtyThread started dev=<%d,%d>", block_slot(devtab), devtab->major);
	ZeroMemory (&os, sizeof( OVERLAPPED ) ) ;
	ZeroMemory ( &ReadOverlapped, sizeof( OVERLAPPED ) ) ;

	os.hEvent = devtab->ReadWaitCommEvent;
	if (os.hEvent == NULL)
	{
		logmsg(LOG_DEV+6, "failed to create event for thread!");
		return;
	}
	ReadOverlapped.hEvent = devtab->ReadOverlappedEvent;
	if (ReadOverlapped.hEvent == NULL)
	{
		logmsg(LOG_DEV+6, "failed to create event for thread!");
		return;
	}
	if (!SetCommMask(devtab->DeviceInputHandle, EV_RXCHAR ))
		return;
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	devtab->state = 1;
	while(1)
	{
		emask = 0;
		if (devtab->NumChar >= ((devtab->tty.c_lflag&ICANON)?1:(int)devtab->tty.c_cc[VMIN]))
			PulseEvent(devtab->BuffFlushEvent);
		while (!WaitCommEvent(devtab->DeviceInputHandle, &emask,&os))
		{
			DWORD err = GetLastError();
			if(err==ERROR_IO_PENDING)
			{
				GetOverlappedResult( devtab->DeviceInputHandle,
						&os, &transfer, TRUE );

				os.Offset += transfer;
			}
			else
			{
				logerr(0, "WaitCommEvent");
				Sleep(250);
				continue;
			}
			if(devtab->state)
				break;
			WaitForSingleObject(P_CP->sigevent,INFINITE);
		}
		if(!ClearCommError( devtab->DeviceInputHandle,&error,&comstat))
			logerr(0, "ClearCommError");

		length = comstat.cbInQue;
		if (length > 0)
		{
			time_getnow(&devtab->access);
			rc = ReadFile(devtab->DeviceInputHandle, Buff,
				length, &length, &ReadOverlapped);
			if (!rc)
			{
				if ((lerr = GetLastError())==ERROR_IO_PENDING)
				{
					/* wait for a second for this transmission to complete */

					if (WaitForSingleObject(ReadOverlapped.hEvent,1000))
						length = 0 ;
					else
					{
						GetOverlappedResult(devtab->DeviceInputHandle,
						    &ReadOverlapped,&length, FALSE);
						ReadOverlapped.Offset += length ;
					}
				}
				else /* some other error occurred */
					length = 0 ;
			}
		}
		for ( j = 0; j < length; j++ )
		{
			ch [0] = (devtab->tty.c_iflag & ISTRIP)?Buff [j]&0x7f:Buff[j] ;
			ch[1] = 0;
			TtyInitialProcess(devtab, ch);

		} /* for loop */
	}
}

#define IS_ARROWKEY(a) (((a) >= VK_END) && ((a) <= VK_DOWN))
#define IS_FPADKEY(a) ((a) >= VK_F1 && (a) <= VK_F12)
#define MAX_CHARS	20
void ReadConsoleThread( Pdev_t *devtab)
{
	INPUT_RECORD 	*rp, inputBuffer[MAX_CHARS];
	DWORD	nread;
	unsigned char	ch[10];
	CONSOLE_SCREEN_BUFFER_INFO	Cbufinfo;
	static const char arrowbuf[] = "YHDACB";
	static const char fkeybuf[] = "PQRSTUVWXYZA";
	ssize_t ret;
	unsigned int c,num_of_buttons, paste_button;

	CURINDEX = 0;
	STARTINDEX = 0;
	devtab->MaxRow = 25;
	devtab->MaxCol = 80;

	GetNumberOfConsoleMouseButtons(&num_of_buttons);
	if(num_of_buttons == 2)
		paste_button = RIGHTMOST_BUTTON_PRESSED;
	else
		paste_button = FROM_LEFT_2ND_BUTTON_PRESSED;

	logmsg(LOG_DEV+3, "ReadConsoleThread started dev=<%d,%d> num_buttons=%d", block_slot(devtab), devtab->major, num_of_buttons);
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	ttysetmode(devtab);

	// Initializations for MOUSE based cut & paste.
	mouse.state=INIT;
	ClipData.state=FALSE;
	ClipData.count=0;
	mouse.length=CLIP_MAX; /* Pasting CLIP_MAX bytes the first time. */
	mouse.clip=TRUE; //Enable Paste initially

	while (1)
	{
		nread=0;
		if(ClipData.state)
		{
			if((c=buffer[ClipData.count]) != 0)
			{
 				inputBuffer[0].EventType=KEY_EVENT;
				inputBuffer[0].Event.KeyEvent.bKeyDown=TRUE;
				inputBuffer[0].Event.KeyEvent.wRepeatCount=1;
				if(c=='\r' && buffer[ClipData.count+1]=='\n')
					ClipData.count++;
				if(c=='\r')
					c = '\n';
				inputBuffer[0].Event.KeyEvent.uChar.AsciiChar=c;
				ClipData.count++;
				nread++;

				if (devtab->NumChar >= ((devtab->tty.c_lflag&ICANON)?1:(int)devtab->tty.c_cc[VMIN]))
					PulseEvent(devtab->BuffFlushEvent);

				while(1)
				{
					if(devtab->state)
						break;
					WaitForSingleObject(P_CP->sevent,INFINITE);
				}
				goto AfterClip;
			}
			else
			{
				ClipData.state=FALSE;
				free(buffer);
				ClipData.count=0;
			}
		}

		if (devtab->NumChar >= ((devtab->tty.c_lflag&ICANON)?1:(int)devtab->tty.c_cc[VMIN]))
			PulseEvent(devtab->BuffFlushEvent);
		while(1)
		{
			WaitForSingleObject(devtab->DeviceInputHandle,INFINITE);
			if(devtab->state)
				break;
			WaitForSingleObject(P_CP->sevent,1000);
		}

  		ReadConsoleInput(devtab->DeviceInputHandle,inputBuffer,MAX_CHARS,&nread);
		time_getnow(&devtab->access);
AfterClip:
		rp = inputBuffer-1;
		while(nread-- > 0) switch((++rp)->EventType)
		{
		    case KEY_EVENT:
			 devtab->notitle = 0;
			 ch[0]=0;
			 ch[3]=0;
			 if (rp->Event.KeyEvent.bKeyDown)
			 {
				register DWORD  keycode = rp->Event.KeyEvent.wVirtualKeyCode;
				switch(keycode)
				{
				    case VK_DELETE:
					ch[0] = DEL_CHAR; ch[1] = 0;
					break;
				    case VK_INSERT:
					ch[0]=27; ch[1]='['; ch[2]='@';
					break;
					default:
					if (IS_ARROWKEY(keycode))
					{
						ch[0]=27;
						if(NewVT && IS_DECCKM(vt.vt_opr_mode))
							ch[1]='O'; //application mode
						else if(!NewVT && keypad_xmit_flag)
							ch[1]='O'; //application mode
						else
							ch[1]='['; //normal mode
						ch[2]=arrowbuf[keycode-VK_END];
					}
					else if (IS_FPADKEY(keycode))
					{
						ch[0]=27; ch[1]='O'; ch[2]=fkeybuf[keycode-VK_F1];
					}
					if((VK_RETURN == keycode) && NewVT)
					{
							ch[0] = CR;
							if(IS_LNM(vt.vt_opr_mode))
							{
								ch[1] = LF;
								ch[2] = 0;
							}
							else
								ch[1] = 0;
					}
					else if(rp->Event.KeyEvent.uChar.AsciiChar)
					{
						ch[0] = rp->Event.KeyEvent.uChar.AsciiChar;
						ch[0] = (devtab->tty.c_iflag&ISTRIP)?ch[0]&0x7f:ch[0];
						ch[1] = 0;
					}
				}
				if(NewVT)
				{
				  if(!IS_KAM(vt.vt_opr_mode))
					TtyInitialProcess(devtab, ch);
				}
				else
				TtyInitialProcess(devtab, ch);
			 }
			break;

		    case WINDOW_BUFFER_SIZE_EVENT:
			GetConsoleScreenBufferInfo (devtab->DeviceOutputHandle, &Cbufinfo);
			if(NewVT)
			{
				int max_col=devtab->MaxCol;
				devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
				devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left);
				if (((vt.bottom_margin - vt.top_margin+1) - devtab->MaxRow)||(devtab->MaxCol != max_col))
				{
					/*
					 * these lines should be executed only when a change of size
					 * actually occurs.
					 */
					vt.bottom_margin = vt.top_margin+devtab->MaxRow-1;
					kill1(-devtab->tgrp,SIGWINCH);
				}
			}
			else
			{
				devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
				devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left)+1;
				kill1(-devtab->tgrp,SIGWINCH);
			}
			break;
		 	case MOUSE_EVENT:
			switch(mouse.state)
			{
			    case INIT:
			  	if(rp->Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
				{
					//clearing off the rectangle
			   		mouse.mode=FALSE;
					mouse.clip=TRUE;
					mouse.drag=FALSE;
			  		MarkRegion(devtab,0);

					mouse.coordStart=mouse.coordPrev=mouse.coordNew= \
							rp->Event.MouseEvent.dwMousePosition;
					mouse.state=BUTTON_DOWN;
					mouse.drag=TRUE;
					mouse.clip=TRUE;
				}
				else if(rp->Event.MouseEvent.dwButtonState == paste_button)
					goto FINAL1;
				break;
			    case BUTTON_DOWN:
			   	if(rp->Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
				{
			   		if(rp->Event.MouseEvent.dwEventFlags == MOUSE_MOVED)
			   		{
			   			mouse.coordNew=rp->Event.MouseEvent.dwMousePosition;
			   			mouse.mode=FALSE;
			   			MarkRegion(devtab,0);
			   			mouse.mode=TRUE;
			   			MarkRegion(devtab,0);
			   		}
				}
				else if(rp->Event.MouseEvent.dwEventFlags == 0)
				{
				  	mouse.mode=TRUE;
					mouse.clip=TRUE;
			  		MarkRegion(devtab,1);
				  	mouse.state=FINAL;
					if(NewVT)
					{
						int max_col=devtab->MaxCol;
						GetConsoleScreenBufferInfo (CSBHANDLE, &Cbufinfo);
						devtab->MaxRow = (Cbufinfo.srWindow.Bottom - Cbufinfo.srWindow.Top)+1;
						devtab->MaxCol = (Cbufinfo.srWindow.Right - Cbufinfo.srWindow.Left);
						if (((vt.bottom_margin - vt.top_margin+1) - devtab->MaxRow)||(devtab->MaxCol != max_col))
						{
							vt.bottom_margin = vt.top_margin+devtab->MaxRow-1;
							kill1(-devtab->tgrp,SIGWINCH);
						}
						if(vt.top < 0)
							normal_text = vt.color_attr = Cbufinfo.wAttributes;
						SetConsoleTextAttribute(CSBHANDLE,Cbufinfo.wAttributes);
					}
				}
			   	break;
			    case FINAL:
			  	if(rp->Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED)
			  	{
					//clearing off the rectangle
			   		mouse.mode=FALSE;
					mouse.clip=FALSE;
					mouse.drag=FALSE;
			  		MarkRegion(devtab,1);
					if(rp->Event.MouseEvent.dwEventFlags == DOUBLE_CLICK)
					{
						mouse.coordNew = rp->Event.MouseEvent.dwMousePosition;
				       		select_word(mouse.coordNew,devtab);
			   			mouse.mode=TRUE;
						mouse.clip=TRUE;

						MarkRegion(devtab,1);
				  		mouse.state=FINAL;
					}
					else
					{
						mouse.coordStart=mouse.coordPrev=mouse.coordNew= \
							rp->Event.MouseEvent.dwMousePosition;
						mouse.state= BUTTON_DOWN;
						mouse.drag=TRUE;
						mouse.clip=FALSE;
					}
			  	}
				else if(rp->Event.MouseEvent.dwButtonState == paste_button)
			  	{
				FINAL1:
					//Copy the selected text from Clipboard to the Terminal
					if(mouse.clip == TRUE)
					{
						FILETIME ft;
						unsigned __int64 ntime;
						static unsigned __int64 otime;
						time_getnow(&ft);
						ntime = ((unsigned __int64)(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;
						/* prevent multiple pastes */
						if(otime && ((ntime-otime)<2500000))
							break;
						ret=clipread(0,0,(char *)&buffer, mouse.length);

						if(ret==-1)
						{
							logmsg(LOG_DEV+6, "clipboard read failed");
							mouse.clip=FALSE;
							mouse.state=INIT;
							ClipData.state=FALSE;
							break;
						}
						buffer[ret]=0;
						otime = ntime;
						mouse.clip=TRUE;
						mouse.state=INIT;
						ClipData.state=TRUE;
					}
				}
			  	break;
			    default:
			 	break;
			}
		}
	}
	logmsg(0, "ReadConsoleThread exits");
}

/*
 * Read from the device handle and clear event when pipe is empty
 */
int devread(register Pdev_t *pdev,char *ch,int size)
{
	int nread=0,maxtry=0,set=0;
	while(PeekNamedPipe(pdev->SrcInOutHandle.WritePipeInput,NULL,0, NULL, &nread, NULL))
	{
		if(nread)
			break;
		else if(pdev->major==PTY_MAJOR && !SetEvent(pdev->write_event))
			logerr(0, "SetEvent");
		if(pdev->writecount==0)
		{
			if(!SetEvent(pdev->WriteSyncEvent))
				logerr(0, "SetEvent");
			break;
		}
		Sleep(20);
		if(++maxtry>100)
		{
			InterlockedDecrement(&pdev->writecount);
			maxtry = 0;
		}
	}
	if(pdev->major==PTY_MAJOR && nread==0)
	{
		if(SetEvent(pdev->write_event))
			set = 1;
		else
			logerr(0, "SetEvent");
	}
	if(!ReadFile(pdev->SrcInOutHandle.WritePipeInput,ch,size,&nread,NULL))
		return(-1);
	if(set && !ResetEvent(pdev->write_event))
		logerr(0, "ResetEvent");
	return(nread);
}

/* Function 	: WriteTtyThread
 * Description 	: Thread Function for Write Thread, which will read
 * 				  data from the WritePipe and output to the device
 * Arguments 	: devtab;Device Table entry corresponding to the Device
 */

void WriteTtyThread(Pdev_t *devtab)
{

	char 		ch[MAX_CANON] , ch1[2*MAX_CANON];
	OVERLAPPED	WriteOverlapped;
	DWORD		nread, nwrite;
	register unsigned int	c, i=0, col=1;
	BOOL 		rc ;
	int j,maxtry = 0;

	logmsg(LOG_DEV+3, "WriteTtyThread started dev=<%d,%d>", block_slot(devtab), devtab->major);
	ZeroMemory(ch,sizeof(ch) );
	ZeroMemory(ch1, sizeof(ch1));

	/* Create an Event on which write thread will wait for the output to
	 *  resume to implement flow control
	 */
	ZeroMemory (&WriteOverlapped,  sizeof(OVERLAPPED));
	WriteOverlapped.hEvent = devtab->WriteOverlappedEvent;
	if (WriteOverlapped.hEvent == NULL)
	{
		logmsg(LOG_DEV+6, "failed to create event for thread!");
		return;
	}
	SetEvent(devtab->WriteSyncEvent);
	while(1)
	{
		j = nread = devread(devtab,ch,sizeof(ch));
		if(j<0)
		{
			Sleep(250);
			continue;
		}
		if(devtab->tty.c_cc[VDISCARD] && devtab->discard)
			continue;
		if(devtab->tty.c_oflag & OPOST)
		{
			for(i=0, j=0; i< nread; i++, j++)
	                {
				switch(c=ch[i])
				{
				    case '\n':
						if(devtab->tty.c_oflag & ONLCR)
						{
							ch1[j++] = '\r';
							ch1[j] = c;
							col = 1;
						}
						else
							ch1[j] = c;
						break;
					case '\r':
						if(devtab->tty.c_oflag & OCRNL)
						{
							ch1[j] = '\n';
						}
						else
						{
							if(!((devtab->tty.c_oflag & ONOCR) && col == 1))
							{
								ch1[j] = c;
								col = 1;
							}
						}
						break;
				    case '\b':
						ch1[j] = c;
						if(--col<1)
							col = 1;
						break;
				    default:
						if(devtab->tty.c_oflag & OLCUC)
							ch1[j] = toupper(c);
						else
							ch1[j] = c;
						if(++col == (unsigned)devtab->MaxRow)
							col = 1;
				}
	                }
			InterlockedExchange(&devtab->cur_phys, col);
		}
	 	rc = WriteFile( devtab->DeviceOutputHandle,
					((devtab->tty.c_oflag & OPOST)?ch1:ch), j,
					&nwrite, &WriteOverlapped) ;
		if(!rc && (GetLastError() == ERROR_IO_PENDING))
		{
			/* wait a second for this transmission to complete */
			if(WaitForSingleObject(WriteOverlapped.hEvent,INFINITE))
				nwrite = 0 ;
			else
			{
				GetOverlappedResult( devtab->DeviceOutputHandle,
					&WriteOverlapped,&nwrite,FALSE);
				WriteOverlapped.Offset += nwrite ;
			}
	 	}

	}
	return; /* To make the compiler happy */
}

/*
 *  This function is a generic function to initialise termios structure
 */
void termio_init(Pdev_t *pdev)
{
	int i;
	static cc_t cc_ini[] =
	{
		/*EOF, EOL, ERASE, INTR*/
		CNTL('D'),
		0,
		'\b',
		CNTL('C'),
		/*KILL, MIN, QUIT, SUSP*/
		CNTL('U'),
		1,
		CNTL('\\'),
		CNTL('Z'),
		/*TIME, START,STOP, LNEXT*/
		0,
		CNTL('Q'),
		CNTL('S'),
		CNTL('V'),
		/*WERASE, EOL2, REPRINT,DISCARD*/
		CNTL('W'),
		0,
		CNTL('R'),
		CNTL('O'),
		/*DSUSP, SWTCH*/
		CNTL('Y'),
		0
	};


	ZeroMemory(&(pdev->tty), sizeof(pdev->tty));
	for(i= VEOF; i <= VSWTCH; ++i)
		pdev->tty.c_cc[i] = cc_ini[i];


	switch(pdev->major)
	{
		case MODEM_MAJOR:
			{
				DCB dcb;
				speed_t baud=0;
				HANDLE hp = pdev->DeviceInputHandle;

				if(!GetCommState(hp, &dcb))
					logerr(0, "GetCommState");
				else
				{
					if (dcb.fParity)
						pdev->tty.c_iflag |= INPCK;
					if(dcb.fErrorChar && (dcb.ErrorChar == 0))
						pdev->tty.c_iflag &= ~(IGNPAR|PARMRK);
					if(dcb.fInX)
						pdev->tty.c_iflag |= IXON;
					if(dcb.fOutX)
						pdev->tty.c_iflag |= IXOFF;

					switch(dcb.Parity)
					{
						case ODDPARITY:
						pdev->tty.c_cflag |= PARENB|PARODD;
							break;
						case EVENPARITY:
						pdev->tty.c_cflag |= PARENB;
						pdev->tty.c_cflag &= ~PARODD;
							break;
						default :
						case NOPARITY:
						pdev->tty.c_cflag &= ~PARENB;
							break;

					}
					pdev->tty.c_cflag &= ~CSIZE;
					switch(dcb.ByteSize)
					{
						case 5:
							pdev->tty.c_cflag |= CS5;
							break;
						case 6 :
							pdev->tty.c_cflag |= CS6;
							break;
						case 7 :
							pdev->tty.c_cflag |= CS7;
							break;
						default:
						case 8:
							pdev->tty.c_cflag |= CS8;
							break;

					}
					switch(dcb.StopBits)
					{
						case TWOSTOPBITS:
							pdev->tty.c_cflag &= ~CSTOPB;
							break;
						default:
							pdev->tty.c_cflag |= CSTOPB;
					}
					switch(dcb.BaudRate)
					{
						case CBR_110:
							baud |= B110;
							break;
						case CBR_300:
							baud |= B300;
							break;
						case CBR_600:
							baud |= B600;
							break;
						case CBR_1200:
							baud |= B1200;
							break;
						case CBR_2400:
							baud |= B2400;
							break;
						case CBR_4800:
							baud |= B4800;
							break;
						case CBR_9600:
							baud |= B9600;
							break;
						case CBR_19200:
							baud |= B19200;
							break;
						case CBR_38400:
							baud |= B38400;
							break;
						default :
							baud |= B9600;
							break;
					}
					cfsetispeed(&(pdev->tty),baud);
				}
			}
			break;
		case TTY_MAJOR: 	// TTY startup modes
		case CONSOLE_MAJOR: // CONSOLE startup modes
			pdev->tty.c_lflag = ICANON|ECHO|ISIG|ECHOE|ECHOK|ECHOCTL;
			pdev->tty.c_cflag = CS8|B19200;
			pdev->tty.c_oflag = OPOST | ONLCR;
			pdev->tty.c_iflag = ICRNL | IXON;
			break;
		case PTY_MAJOR: 	// PTY startup modes
			pdev->tty.c_lflag = ICANON|ECHO|ISIG|ECHOE|ECHOK|ECHOCTL;
			pdev->tty.c_cflag = CS8|B19200;
			pdev->tty.c_oflag = OPOST | ONLCR;
			pdev->tty.c_iflag = ICRNL | IXON;
			break;

		default :
			break;
	}
}

/*
 * open a communication port corresponding to path /dev/tty??
 */
HANDLE open_comport(unsigned short minor_no)
{
	char portname[8]="com";
	DWORD dev_err;
	HANDLE hp;
	SECURITY_ATTRIBUTES sattr;
	/*
	 *	minor no
	 *	0 			-> com1.
	 *	1 			-> com2.
	 */
	sfsprintf(portname,sizeof(portname),"com%d",minor_no+1);
	sattr.nLength = sizeof(sattr);
	sattr.lpSecurityDescriptor = NULL;
	sattr.bInheritHandle = TRUE;

	if(!(hp=createfile(portname,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sattr,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,0)))
	{
		errno = unix_err(GetLastError());
		logerr(0, "CreateFile %s failed", portname);
		return(0);
	}
	if(!ClearCommError(hp,&dev_err,NULL))
		logerr(0, "ClearCommError");
	return(hp);
}

void device_reinit(void)
{
	HANDLE me=GetCurrentProcess();
	Pfd_t *fdp;
	Pdev_t *pdev;
	int i;
	logmsg(LOG_DEV+4,"device_reinit");
	for(i=0; i < P_CP->maxfds; i++)
	{
		if(fdslot(P_CP,i) < 0)
			continue;
		fdp = getfdp(P_CP, i);
		if(fdp->devno==0)
			continue;
		pdev = dev_ptr(fdp->devno);
		logmsg(LOG_DEV+4, "device_reinit dev=<%d,%d> devno=%d type=%(fdtype)s", block_slot(pdev), pdev->major, fdp->devno, fdp->type);
		switch(fdp->type)
		{
		    case FDTYPE_PTY:
			if(pdev->NtPid && pdev->NtPid!=P_CP->ntpid)
				break;
			if(pdev->Pseudodevice.master)
				pdev = dev_ptr(pdev->Pseudodevice.pseudodev);
			dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadPtyThread, (LPTHREAD_START_ROUTINE)WritePtyThread);
			break;

		    case FDTYPE_CONSOLE:
			if(pdev->NtPid!=P_CP->ntpid)
				break;
			if((fdp->oflag&O_ACCMODE)==O_RDONLY)
			{
				if(!DuplicateHandle(me,Phandle(i),me,&pdev->DeviceInputHandle,0,FALSE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
			}
			else
			{
				if(!DuplicateHandle(me,Phandle(i),me,&pdev->DeviceOutputHandle,0,TRUE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				color_attrib = GetConsoleTextAttributes(Phandle(i));
				UNFILL = ((unsigned int)(color_attrib));
			}
			dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadConsoleThread, (LPTHREAD_START_ROUTINE)WriteConsoleThread);
			break;

		    case FDTYPE_TTY:
			if(pdev->NtPid==P_CP->ntpid)
			{
				if(!DuplicateHandle(me,Phandle(i),me,&pdev->DeviceInputHandle,0,FALSE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				if(!DuplicateHandle(me,Phandle(i),me,&pdev->DeviceOutputHandle,0,FALSE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadTtyThread, (LPTHREAD_START_ROUTINE)WriteTtyThread);
			}
			break;
		}
	}
}

/*
 * device_open: initializes the device structure and creates terminal threads.
 */
int device_open(int blkno,HANDLE hpin, Pfd_t *fdp, HANDLE hpout)
{
	Pdev_t *pdev;
	LPTHREAD_START_ROUTINE read_thread, write_thread;
	SECURITY_ATTRIBUTES sa;
	HANDLE me = GetCurrentProcess();

	fdp->devno = blkno;
	pdev = dev_ptr(blkno);
	switch(fdp->type)
	{
	case FDTYPE_CONSOLE:
		sa.nLength=sizeof(sa);
		sa.bInheritHandle=TRUE;
		sa.lpSecurityDescriptor=NULL;
		if (!hpin)
			hpin=createfile("CON", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
		if (!hpout)
			hpout=createfile("CON", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
		if (!hpin)
			logmsg(0, "proceeding with Invalid Input Handle");
		if (!hpout)
			logmsg(0, "proceeding with Invalid Output Handle");
		read_thread = (LPTHREAD_START_ROUTINE)ReadConsoleThread;
		write_thread = (LPTHREAD_START_ROUTINE)WriteConsoleThread;

		/* This code is used to get the console attributes while
		 * initializing the console. This is used to UNFILL the
		 *  area after it got selected marked.
		 */
		color_attrib = GetConsoleTextAttributes(hpout);
		UNFILL = ((unsigned int)(color_attrib));
		break;
	case FDTYPE_PTY:
		read_thread = (LPTHREAD_START_ROUTINE)ReadPtyThread;
		write_thread = (LPTHREAD_START_ROUTINE)WritePtyThread;
		break;
	case FDTYPE_TTY:
		read_thread = (LPTHREAD_START_ROUTINE)ReadTtyThread;
		write_thread = (LPTHREAD_START_ROUTINE)WriteTtyThread;
		break;
	case FDTYPE_MODEM:
		read_thread = (LPTHREAD_START_ROUTINE)ReadModemThread;
		write_thread= (LPTHREAD_START_ROUTINE)WriteModemThread;
		break;
	}
	if(fdp->type==FDTYPE_MODEM || Share->Platform!=VER_PLATFORM_WIN32_NT && fdp->type==FDTYPE_CONSOLE)
	{
		if(!DuplicateHandle(me,hpin,me, &hpin,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle %p", hpin);
		if(!DuplicateHandle(me,hpout,me, &hpout,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle %p", hpout);

	}
	pdev->DeviceInputHandle = hpin;
	pdev->DeviceOutputHandle = hpout;
	termio_init(pdev); /* Initialize termios structure */
	return !!dev_startthreads(pdev,read_thread,write_thread);
}

void term_close(Pdev_t *pdev)
{
	DWORD ntpid = pdev->NtPid;
	if (P_CP->pid != pdev->devpid)
	{
		logmsg(0, "inappropriate process trying to close the threads");
		return;
	}
	if(pdev->NewCSB)
		closehandle(pdev->NewCSB,HT_OCONSOLE);
	dev_stopthreads(pdev,0);
	pdev->NtPid = ntpid;
}

/*
	Free the controlling terminal.
	Terminate the read, write threads.
	Close the read & write pipe handles, deivce input & output handles.
*/
int free_term(Pdev_t *pdev, int noclose)
{
	HANDLE proc=0;
	if(P_CP->console == block_slot(pdev))
	{
		if(!utentry(pdev,0,NULL,NULL) && Share->init_thread_id)
			send2slot(Share->init_slot,2,P_CP->console);
		P_CP->console = 0;
	}
	if(pdev->NtPid && P_CP->ntpid!=pdev->NtPid)
		proc=open_proc(pdev->NtPid,0);
	if(noclose<=0 || proc)
	{
		if(pdev->NewCSB,proc)
			pclosehandle(pdev->NewCSB,proc);
		dev_stopthreads(pdev,proc);
	}
	if(pdev->slot)
		block_free((unsigned short)pdev->slot);

	if(pdev->count != 0)
		logmsg(0, "free_term: bad reference count=%d", pdev->count);
	if(pdev->major)
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,pdev->major);
		unsigned short blkno = blocks[pdev->minor];
		blocks[pdev->minor] = 0;
		block_free(blkno);
	}
	return(0);
}

int pty_read(Pfd_t* fdp, char *buff, int size)
{
	long rsize, raw_time, raw_min;
	Pdev_t *pdev;
	HANDLE objects[2];
	int saverror = errno, nsize= 0, sigsys= -1;
	DWORD wait_time = INFINITE;
	int size_to_r = size, storedchar= 0;

	pdev = dev_ptr(fdp->devno);
	if(wait_foreground(fdp) < 0)
		return(-1);
	if(!pdev->state)
		ttysetmode(pdev);
	raw_time = pdev->tty.c_cc[VTIME];
	raw_min = pdev->tty.c_cc[VMIN];
	ZeroMemory(buff, size);

	if(fdp->oflag&(O_NONBLOCK|O_NDELAY))
	{
		if(pdev->NumChar)
			nsize = GetTheChar(fdp, buff, size);
		else
		{
			errno = EAGAIN;
			nsize = -1;
		}
		return(nsize);
	}
	objects[0]=P_CP->sigevent;
	objects[1] = dev_gethandle(pdev, pdev->BuffFlushEvent);
	if(!objects[1] && GetLastError()==ERROR_INVALID_PARAMETER)
	{
		pdev->started = 0;
		dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadPtyThread, (LPTHREAD_START_ROUTINE)WritePtyThread);
		objects[1] = dev_gethandle(pdev, pdev->BuffFlushEvent);
	}
	if(!objects[1])
	{
		/* if master pty has closed, then return EOF */
		if(!pdev->NtPid)
			return(0);
		logerr(0, "dev_gethandle");
	}

	setNumChar(pdev);
	if(!(pdev->tty.c_lflag & ICANON))
	{
		if((fdp->oflag&O_NONBLOCK) || (fdp->oflag&O_NDELAY))
		{
			if(pdev->NumChar)
				nsize = GetTheChar(fdp, buff, size);
			else if(fdp->oflag&O_NDELAY) /* System V ism */
				nsize = 0;
			else
			{
				errno = EAGAIN;
				nsize = -1;
			}
			goto done;
		}

		if(raw_time && !raw_min)
		{
			if(!pdev->NumChar)
			{
				HANDLE hp;
				hp = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadThreadHandle);
				ResumeThread(hp);
				closehandle(hp,HT_THREAD);
				P_CP->state = PROCSTATE_READ;
			retry:
				if(sigsys<0)
					sigsys = sigdefer(1);
				else
					sigdefer(1);
				rsize = WaitForMultipleObjects(2,objects,FALSE,raw_time * 100);
				if(rsize==WAIT_TIMEOUT)
				{
					errno = EAGAIN;
					nsize = 0;
				}
				else if(rsize==WAIT_OBJECT_0)
				{
					if(sigcheck(sigsys))
						goto retry;
					errno = EINTR;
					nsize = -1;
				}
				P_CP->state = PROCSTATE_RUNNING;
			}
			else
				nsize = GetTheChar(fdp, buff, size);
			goto done;
		}
		else if(!raw_time & !raw_min)
		{
			if(pdev->NumChar)
				nsize = GetTheChar(fdp, buff,size);
			else
			{
				errno = EAGAIN;
				nsize = 0;
			}
			goto done;
		}
	}
	while(1)
	{
		if(!pdev->NumChar)
		{
		retry2:
			{
				unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
				if(!blocks[pdev->minor-128])
				{
					errno = EIO;
					nsize = -1;
					goto done;
				}
			}
			P_CP->state = PROCSTATE_READ;
			rsize= WaitForMultipleObjects(2,objects,FALSE,wait_time);
			P_CP->state = PROCSTATE_RUNNING;
			if(rsize==WAIT_TIMEOUT)
			{
				P_CP->state = PROCSTATE_RUNNING;
				errno = EAGAIN;
				if(size<=nsize)
					nsize = size;
				goto done;
			}
			else if(rsize==WAIT_OBJECT_0)
			{
				if(sigcheck(sigsys))
				{
					sigdefer(1);
					goto retry2;
				}
				errno = EINTR;
				nsize = -1;
				goto done;
			}
		}
		if(size_to_r > 0)
			nsize += GetTheChar(fdp, &buff[nsize],size_to_r);
		else
		{
			nsize += pdev->NumChar - storedchar;
			storedchar = pdev->NumChar;
		}

		if(!(pdev->tty.c_lflag & ICANON))
		{
			if(nsize >= raw_min)
			{
				if(size<=nsize)
					nsize = size;
				goto done;
			}
			if(raw_min)
			{
				if(raw_time)
				{
					wait_time = raw_time * 100;
				}

				size_to_r = size - nsize;
				if(size_to_r < 0)
					size_to_r =0;
			}
			else
				goto done;
		}
		else
			goto done;
	}
done:
	closehandle(objects[1],HT_EVENT);

	return(nsize);
}

ssize_t ptyread(int fd, Pfd_t *fdp, char *buf, size_t asize)
{
	DWORD size, rsize, ret;
	Pdev_t *pdev;
	HANDLE objects[3];
	int saverror = errno, sigsys= 0;
	char *buff;

	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (DWORD)asize;
	buff = buf;
	pdev = dev_ptr(fdp->devno);
	if (pdev->Pseudodevice.master)
	{
			if(!pdev->NtPid)
				pdev->NtPid = P_CP->ntpid;
			if(pdev->Pseudodevice.pckt_event)
			{
				buff[0] = TIOCPKT_DATA;
				buff = &buff[1];
			}
			if(fdp->oflag&(O_NDELAY|O_NONBLOCK))
			{
				if(!PeekNamedPipe(Phandle(fd),NULL,0,NULL,&rsize,NULL))
				{
					if(GetLastError()==ERROR_BROKEN_PIPE)
						return(0);
					errno = unix_err(GetLastError());
					return(-1);
				}
				if(rsize==0)
				{
					if(pdev->Pseudodevice.pseudodev==0)
						return(0);
					errno = EWOULDBLOCK;
					return((fdp->oflag&O_NDELAY)?0:-1);
				}
				if(size>rsize)
					size = rsize;
				goto doread;
			}
			objects[0]=P_CP->sigevent;
			objects[1]=pdev->Pseudodevice.SelEvent;
			objects[2]=pdev->Pseudodevice.pckt_event;
		retry:
			P_CP->state = PROCSTATE_READ;
			if(objects[2])
				rsize = WaitForMultipleObjects(3,objects,FALSE,INFINITE);
			else
				rsize = WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			P_CP->state = PROCSTATE_RUNNING;
			if(rsize==WAIT_OBJECT_0)
			{
				if(sigcheck(sigsys))
				{
					sigdefer(1);
					goto retry;
				}
				errno = EINTR;
				return(-1);
			}
			if(rsize == WAIT_OBJECT_0+2 )
			{
				if(ResetEvent(objects[2]))
				{
					if(pdev->SuspendOutputFlag)
						buff[0] |= TIOCPKT_STOP;
					else
						buff[0] |= TIOCPKT_START;
					return 1; // This should be  1;
				}
			}
			if(!PeekNamedPipe(Phandle(fd),NULL,0,NULL,&rsize,NULL))
				return(0);
			if(rsize==0 && pdev->Pseudodevice.pseudodev==0)
				return(0);
		doread:
			if(!ReadFile(Phandle(fd), buff, size, &rsize, NULL))
			{
				if(GetLastError()==ERROR_BROKEN_PIPE)
					return(0);
				logerr(0, "ReadFile");
				return (-1);
			}
			else
			{
				WaitForSingleObject(pdev->Pseudodevice.SelMutex,INFINITE);
				pdev->Pseudodevice.SelChar -= rsize;
				if(!pdev->Pseudodevice.SelChar)
					ResetEvent(pdev->Pseudodevice.SelEvent);
				ReleaseMutex(pdev->Pseudodevice.SelMutex);
				if(pdev->Pseudodevice.pckt_event)
					return(process_dsusp(pdev,buff,rsize+1)); /* This is for packet mode */
				else
					return(process_dsusp(pdev,buff,rsize));
			}
	}
	if(!pdev->NtPid)
		dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadPtyThread, (LPTHREAD_START_ROUTINE)WritePtyThread);
	ret = pty_read(fdp,buff,(int)size);
	ret = process_dsusp(pdev,buff,ret);
	return(ret);
}

ssize_t ptywrite(int fd, register Pfd_t *fdp, const char *buff, size_t asize)
{
	Pdev_t *pdev;
	DWORD size,tsize,rsize=0,wsize;
	HANDLE hp,hpsync,hpwrite;
	BOOL r;

	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (DWORD)asize;
	pdev = dev_ptr(fdp->devno);
	if(pdev->Pseudodevice.master)
	{
		DWORD n = PIPE_SIZE;
		if(!pdev->NtPid)
			pdev->NtPid = P_CP->ntpid;
		if(n>size)
			n = size;
		if(!Phandle(fd) && !Xhandle(fd))
		{
			errno = EBADF;
			return(-1);
		}
		hp= Xhandle(fd);
		if(!WriteFile(hp, buff, n, &rsize, NULL))
		{
			errno = unix_err(GetLastError());
			logerr(0, "WriteFile");
			return(-1);
		}
		else
		{
			pdev->Pseudodevice.NumOfChar += rsize;
			ResetEvent(pdev->write_event);
			SetEvent(pdev->Pseudodevice.SyncEvent);
			return(rsize);
		}

	}
	if(!pdev->NtPid)
		dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadPtyThread, (LPTHREAD_START_ROUTINE)WritePtyThread);

	hp = dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
	if(!hp && GetLastError()==ERROR_INVALID_PARAMETER)
	{
		pdev->started = 0;
		dev_startthreads(pdev, (LPTHREAD_START_ROUTINE)ReadPtyThread,(LPTHREAD_START_ROUTINE)WritePtyThread);
		hp = dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
	}
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	while(size>0)
	{
		if((tsize=size)>WRITE_CHUNK)
			tsize=WRITE_CHUNK;
		InterlockedIncrement(&pdev->writecount);
		if(hpsync=dev_gethandle(pdev,pdev->WriteSyncEvent))
		{
			if(!ResetEvent(hpsync))
				logerr(0, "ResetEvent");
			closehandle(hpsync,HT_EVENT);
		}
		r = WriteFile(hp, buff, tsize, &wsize, NULL);
		InterlockedDecrement(&pdev->writecount);
		if(!r || wsize==0)
			break;
		rsize += wsize;
		size -= wsize;
		if(size==0)
			break;
		buff += wsize;
		if(hpwrite=dev_gethandle(pdev, pdev->write_event))
		{
			if(!ResetEvent(hpwrite))
				logerr(0, "ResetEvent");
			closehandle(hpwrite,HT_EVENT);
		}
		Sleep(100);
		if(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign)
			break;
	}
	if(!r && rsize==0)
	{
		errno = unix_err(GetLastError());
		rsize = -1;
	}
	closehandle(hp,HT_PIPE);
	if(rsize>0)
		update_mtime(fdp,pdev);
	return(rsize);
}

int free_pty(Pdev_t *pdev, int noclose)
{
	HANDLE proc=0;
	logmsg(LOG_DEV+2, "free_pty dev=<%d,%d>", block_slot(pdev), pdev->major);
	if (pdev->Pseudodevice.master)
	{
		Pdev_t *s_pdev;
		unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
		int blkno = blocks[pdev->minor+128];

		if(blkno)
		{
			s_pdev = dev_ptr(blkno);
			/* if slave is controlling terminal remove from utmp */
			if(s_pdev->boffset == blkno)
				utentry(s_pdev,0,NULL,NULL);
			if(s_pdev->tgrp && !(s_pdev->tty.c_cflag&CLOCAL))
				kill1(s_pdev->devpid,SIGHUP);
			if(pdev->process)
				dev_stopthreads(s_pdev,pdev->process);
		}
		else
		{
			pdev->devname[5] = 't';
			utentry(pdev,0,NULL,NULL);
			pdev->devname[5] = 'p';
		}
		if(noclose<=0 || (proc=open_proc(pdev->NtPid,0)))
		{
			if(pdev->Pseudodevice.pckt_event)
				pclosehandle(pdev->Pseudodevice.pckt_event,proc);
#ifdef CloseHandle
			pdev->NtPid = 0;
#endif
			pclosehandle(pdev->Pseudodevice.m_outpipe_read,proc);
			pclosehandle(pdev->Pseudodevice.m_inpipe_write,proc);
			pclosehandle(pdev->Pseudodevice.m_outpipe_write,proc);
			pclosehandle(pdev->Pseudodevice.m_inpipe_read,proc);
			pclosehandle(pdev->Pseudodevice.SelEvent,proc);
			pclosehandle(pdev->write_event,proc);
			pclosehandle(pdev->Pseudodevice.SelMutex,proc);
			pclosehandle(pdev->Pseudodevice.SyncEvent,proc);
			if(proc)
				closehandle(proc,HT_PROC);
		}
		if(pdev->count != 0)
			logmsg(0, "free_pty master: bad reference count=%d", pdev->count);
	}
	else
	{
		Pdev_t *mpdev;
		if(mpdev = dev_ptr(pdev->Pseudodevice.pseudodev))
		{
			Sleep(200);
			mpdev->Pseudodevice.pseudodev = 0;
			SetEvent(mpdev->Pseudodevice.SelEvent);
		}
		if(P_CP->console==block_slot(pdev))
		{
			if(utentry(pdev,0, NULL, NULL))
				P_CP->console = 0;
		}
		if(pdev->NtPid==P_CP->ntpid || (proc=open_proc(pdev->NtPid,0)))
			dev_stopthreads(pdev,proc);
		if(pdev->slot)
			block_free((unsigned short)pdev->slot);
		if(pdev->count != 0)
			logmsg(0, "free_pty slave: bad reference count=%d", pdev->count);
	}
	if(pdev->major)
	{
		unsigned short *blocks = devtab_ptr(Share->chardev_index,pdev->major);
		unsigned short blkno = blocks[pdev->minor];
		blocks[pdev->minor] = 0;
		block_free(blkno);
	}
	return(0);
}

static void WritePtyThread(Pdev_t *devtab)
{

	char 		*out, ch[2*WRITE_CHUNK] , ch1[4*WRITE_CHUNK];
	DWORD		nread, nwrite;
	register unsigned int	c, i=0, j=0, col=1;
	int 	midx;
	HANDLE sel;
	Pdev_t *pdev;

	ZeroMemory(ch,sizeof(ch));
	ZeroMemory(ch1, sizeof(ch1));
	midx = devtab->Pseudodevice.pseudodev;
	pdev = dev_ptr(midx);

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	sel = pdev->Pseudodevice.SelEvent;
	if( sel == NULL)
		logerr(0, "open event failed");

	while(1)
	{
		if((nread = devread(devtab,ch,sizeof(ch))) <0)
		{
			Sleep(250);
			continue;
		}
		if(devtab->tty.c_cc[VDISCARD] && devtab->discard)
			continue;
		j = nread;
		if(devtab->tty.c_oflag & OPOST)
		{
			out = ch1;
			for(i=0, j=0; i< nread; i++, j++)
			{
				switch(c=ch[i])
				{
				    case '\n':
						if(devtab->tty.c_oflag & ONLCR)
						{
							ch1[j++] = '\r';
							ch1[j] = c;
							col = 1;
						}
						else
							ch1[j] = c;
						break;
					case '\r':
						if(devtab->tty.c_oflag & OCRNL)
						{
							ch1[j] = '\n';
						}
						else
						{
							if(!((devtab->tty.c_oflag & ONOCR) && col == 1))
							{
								ch1[j] = c;
								col = 1;
							}
						}
						break;
				    case '\b':
						ch1[j] = c;
						if(--col<1)
							col = 1;
						break;
				    default:
						if(devtab->tty.c_oflag & OLCUC)
							ch1[j] = toupper(c);
						else
							ch1[j] = c;
						col++;
				}
			}
			InterlockedExchange(&devtab->cur_phys, col);
		}
		else
			out = ch;
		while(j>0)
		{
			if((c=j) > WRITE_CHUNK)
				c = WRITE_CHUNK;
		 	if(WriteFile(devtab->DeviceOutputHandle,out,c,&nwrite,NULL))
			{
				j -= nwrite;
				out += nwrite;
			}
			else
			{
				logerr(0, "WriteFile");
				continue;
			}
			WaitForSingleObject(pdev->Pseudodevice.SelMutex, INFINITE);
			pdev->Pseudodevice.SelChar += nwrite;
			if(!(pdev->Pseudodevice.SelChar - nwrite))
				SetEvent(sel);
			ReleaseMutex(pdev->Pseudodevice.SelMutex);
		}
	}
	return; /* To make the compiler happy */
}

#define BUFFSIZE 20 /* This value depends on the buffer size mentioned
		   while creaing the device pipes. It is observed
		   that CreatePipe() for anonymous pipes assigns
		   buffer of 1/25 th of the mentioned size in the
		   api call*/
static void ReadPtyThread(Pdev_t *devtab)
{
	int			len, midx;
	unsigned char	ch[10], Buff[4096];
	HANDLE sync;
	char 	*cp;
	register int 	i=0;
	Pdev_t *pdev;
	int rsize=0;

	ch[1] = 0;
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	devtab->state = 1;
	midx= devtab->Pseudodevice.pseudodev;
	pdev = dev_ptr(midx);
	devtab->MaxRow = pdev->MaxRow;
	devtab->MaxCol = pdev->MaxCol;
	sync = pdev->Pseudodevice.SyncEvent;
	if(sync==NULL)
		logerr(0, "open event failed");

	while(1)
	{
		i = 0;
		if(!PeekNamedPipe(devtab->DeviceInputHandle,NULL, 0, NULL, &rsize,NULL))
			logerr(0, "PeekNamedPipe");
		else
		{
			if(!rsize)
			{
				if(!SetEvent(pdev->write_event))
					logerr(0, "SetEvent");
				while(1)
				{
					if(!pdev->Pseudodevice.NumOfChar)
					{
						WaitForSingleObject(sync,INFINITE);
						if(!ResetEvent(sync))
							logerr(0, "ResetEvent");
					}
					if(devtab->state)
						break;
					WaitForSingleObject(P_CP->sigevent,INFINITE);
				}
			}
		}
		cp = Buff;
		if(!ReadFile(devtab->DeviceInputHandle,cp,sizeof(Buff),&len,NULL))
		{
			ch[0] = EOF_MARKER;
			PUTCHAR_RPIPE(ch);
			closehandle (devtab->SrcInOutHandle.ReadPipeOutput,HT_PIPE);
			ExitThread(1);
		}
		time_getnow(&devtab->access);
		pdev->Pseudodevice.NumOfChar -= len;
		if(len==0)
		{
			logmsg(LOG_DEV+6, "read of 0 bytes");
			ch[0] = EOF_MARKER;
			PUTCHAR_RPIPE(ch);
			continue;
		}
		while(len-- > 0)
		{
			if(devtab->tty.c_iflag&ISTRIP)
				ch[0] = (*cp++)&0x7f;
			else
				ch[0] = *cp++;
			TtyInitialProcess(devtab, ch);
			i++;
			if((i == BUFFSIZE) || (len == 0))
			{
				i = devtab->tty.c_cc[VMIN];
				if(devtab->NumChar >= ((devtab->tty.c_lflag&ICANON)?1:((i>BUFFSIZE)?BUFFSIZE:i)))
					PulseEvent(devtab->BuffFlushEvent);
				i = 0;
			}
		} /* for loop */
	}
}

int grantpt(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	char ptyname[20];
	struct group *grptr;
	int gid;

	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if (!(pdev->Pseudodevice.pseudodev&&pdev->Pseudodevice.master))
		return(-1);
	grptr = getgrnam("None");
	if(grptr!=NULL)
		gid = grptr->gr_gid;
	else
		gid = -1;
	strcpy(ptyname, ptsname(fd));
	chown(ptyname, getuid(), gid);
	chmod(ptyname, S_IRUSR|S_IWUSR|S_IWGRP);
	return(0);
}

int unlockpt(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if (!(pdev->Pseudodevice.pseudodev&&pdev->Pseudodevice.master))
		return(-1);

	return(0);
}

char *ptsname(int fd)
{
	Pfd_t *fdp;
	Pdev_t *pdev;
	static char ptyname[20];
	char arrX[] = {'p', 'q', 'r', 's', 't', 'u', 'v', 'w'};
	char arrY[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e', 'f'};

	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(NULL);
	}
	fdp = getfdp(P_CP,fd);
	pdev = dev_ptr(fdp->devno);
	if (!(pdev->Pseudodevice.pseudodev&&pdev->Pseudodevice.master))
		return(NULL);

	strcpy(ptyname, "/dev/ttyXY");
	ptyname[8] = arrX[pdev->minor / 16];
	ptyname[9] = arrY[(pdev->minor) % 16];
	return(ptyname);
}

static SHORT getConX(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(hConsole, &csbi);
	return(csbi.dwSize.X);
}

static int cliptrim(char *region, int first, int width, int len)
{
	register char *cp,*dp,*start;
	int size = width-first;
	start = dp = region;
	while(1)
	{
		cp = start;
		cp += size;
		if(cp >= &region[len])
			cp = &region[len];
		else
		{
			/* eliminate trailing white space */
			while(--cp>=start &&  *cp==' ');
			if(*cp!=' ')
				cp++;
		}
		if(cp>start)
		{
			memcpy(dp,start,cp-start);
			dp += cp-start;
		}
		if(start >= &region[len])
			break;
		*dp++ = '\n';
		start += size;
		size = width;
	}
	return (int)(dp-region);
}

static void MarkRegion(Pdev_t *devtab,int copy)
{
	char sbuff[1024];
	WORD width,counter;
	WORD *line_attr;
	DWORD nread;
	HANDLE myHandle;
	WORD  color,tmpcolor;
	unsigned short fill = get_reverse_text((unsigned short)normal_text);


	/* This is to take care of VI allocating a new ConsoleBuffer */
	if(!devtab->NewCSB)
		myHandle = devtab->DeviceOutputHandle;
	else
		myHandle = devtab->NewCSB;

	width = getConX(myHandle);


	//Adjust according to call, whether to PAINT or to UNPAINT
	if(mouse.mode == FALSE)
	{
		mouse.coordTemp = mouse.coordPrev;
	}
	else if(mouse.mode == TRUE)
	{
		mouse.coordTemp = mouse.coordNew;
	}
	else
		return;

	if(mouse.coordStart.Y < mouse.coordTemp.Y)
	{
		mouse.coordBegin = mouse.coordStart;
		mouse.coordEnd = mouse.coordTemp;
		mouse.length = (width-mouse.coordBegin.X) +
			abs(mouse.coordBegin.Y-mouse.coordEnd.Y)*width + mouse.coordEnd.X - width;
	}
	else if(mouse.coordStart.Y == mouse.coordTemp.Y)
	{
		if(mouse.coordPrev.X > mouse.coordStart.X)
		{
			mouse.coordBegin = mouse.coordStart;
			mouse.coordEnd = mouse.coordTemp;
		}
		else
		{
			mouse.coordBegin = mouse.coordTemp;
			mouse.coordEnd = mouse.coordStart;
		}
		mouse.length = mouse.coordEnd.X - mouse.coordBegin.X;
	}
	else if(mouse.coordStart.Y > mouse.coordTemp.Y)
	{
		mouse.coordBegin = mouse.coordTemp;
		mouse.coordEnd = mouse.coordStart;
		mouse.length = (width-mouse.coordBegin.X) +
			abs(mouse.coordBegin.Y-mouse.coordEnd.Y)*width + mouse.coordEnd.X - width;
	}

	if((mouse.length*sizeof(DWORD)) <= sizeof(sbuff))
		line_attr = (WORD*)sbuff;
	else
		line_attr = (WORD*)malloc(mouse.length * sizeof(DWORD));
	if(!line_attr)
	{
		logmsg(LOG_DEV+6, "malloc out of space");
		return;
	}
	tmpcolor = color_attrib;
	if(NewVT)
		color = (unsigned short)normal_text;
	else
		color = GetConsoleTextAttributes(myHandle);
	if ((color != tmpcolor) && (mouse.mode == TRUE))
		UNFILL = (unsigned int) color;

	ReadConsoleOutputAttribute(myHandle, line_attr,
			mouse.length, mouse.coordBegin, &nread);
	for(counter = 0; counter < mouse.length; counter++)
	{

		if(mouse.mode == TRUE)
			line_attr[counter] = NewVT? fill:(unsigned short)FILL;
		else if(mouse.mode == FALSE)
			line_attr[counter] = (unsigned short)UNFILL;
	}

	WriteConsoleOutputAttribute(myHandle, line_attr,
		mouse.length, mouse.coordBegin, &nread);
	if((mouse.length*sizeof(DWORD)) > sizeof(sbuff))
		free(line_attr);

	if(copy && mouse.clip==TRUE)
	{
		/* Do the clipboard copying... */
		char sbuff[1024];
		DWORD len = (mouse.length+abs(mouse.coordBegin.Y-mouse.coordEnd.Y)+1)*sizeof(char);
		if(len <= sizeof(sbuff))
			buffer = sbuff;
		else
			buffer=(char *)malloc(len*sizeof(char));
		if(ReadConsoleOutputCharacter(myHandle,buffer,mouse.length,mouse.coordBegin,&nread))
		{
			if(mouse.coordBegin.Y !=mouse.coordEnd.Y)
			{
				width = cliptrim(buffer, mouse.coordBegin.X,
					width,mouse.length);
			}
			else
				width = mouse.length;
			clipwrite(0,0,buffer,width);
		}
		else
			logerr(0, "ReadConsoleOutputCharacter");
		if(len> sizeof(sbuff))
			free(buffer);
	}
	mouse.coordPrev = mouse.coordNew;
	mouse.length=CLIP_MAX;
	return;
}

static void select_word(COORD Current_Mouse_Position,Pdev_t *devtab)
{
	int found,leftid,rightid;
	COORD temp,left,right;
	char  char_left[1],char_right[1];
	long  int nread;
	HANDLE myHandle;
	INT MAX_COL ;

	found  = leftid = rightid = 0;
	temp.X = Current_Mouse_Position.X;
	temp.Y = Current_Mouse_Position.Y;

	left.Y  = temp.Y;
	left.X  = temp.X;
	right.X = temp.X;
	right.Y = temp.Y;

	if (!devtab->NewCSB)
		myHandle = devtab->DeviceOutputHandle;
	else
		myHandle = devtab->NewCSB;

	MAX_COL = getConX(myHandle);
	while ( !found)
	{
		ReadConsoleOutputCharacter(myHandle,char_left,1,left,&nread);
		ReadConsoleOutputCharacter(myHandle,char_right,1,right,&nread);

		if((is_alphanumeric(*char_left) != 0) && \
			((leftid != 1)&& (left.X >= 0)))
			left.X--;
		else
			leftid = 1;

		if ((is_alphanumeric(*char_right) != 0) && \
			((rightid != 1)&& (right.X < MAX_COL)))
			right.X++;
		else
			rightid = 1;

		if ((leftid == 1) && (rightid == 1))
			found = 1;
	}
	if (left.X != 0)
		left.X++;
	mouse.coordPrev.X=right.X;
	mouse.coordPrev.Y=right.Y;
	mouse.coordStart.X=left.X;
	mouse.coordStart.Y=left.Y;
	mouse.coordNew = mouse.coordPrev;
}


WORD GetConsoleTextAttributes(HANDLE hConsole)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(hConsole,&csbi);
	return(csbi.wAttributes);
}

int dtrsetclr(int fd, int mask, int set)
{

	Pfd_t *fdp;
	HANDLE hCom;
	int ret = 0 ;

	fdp = getfdp(P_CP,fd);
	if (fdp->type != FDTYPE_MODEM)
	{
		errno = ENOTTY;
		return(-1);
	}

	hCom = Phandle(fd);
	if(set)
	{
		if(mask&TIOCM_DTR)
		{
			SetupComm(hCom,2048,1024);
			ret = EscapeCommFunction(hCom, SETDTR);
		}
		if(mask&TIOCM_RTS)
			ret = EscapeCommFunction(hCom, SETRTS);
		if(mask&TIOCM_BRK)
			ret = EscapeCommFunction(hCom, CLRBREAK);
	}
	else
	{
		if(mask&TIOCM_DTR)
			ret = EscapeCommFunction(hCom, CLRDTR);
		if(mask&TIOCM_RTS)
			ret = EscapeCommFunction(hCom, CLRRTS);
		if(mask&TIOCM_BRK)
			ret = EscapeCommFunction(hCom, CLRBREAK);
	}
	if (!ret)
		return -1;
	else
		return 0;
}

int ttyfstat(int fd,Pfd_t* fdp, struct stat *st)
{
	HANDLE hp,hpsave;
	int r;
	char name[100];
	register Pdev_t *pdev = dev_ptr(fdp->devno);
	if(fdp->devno == 0)
		return(-1);
	uwin_pathmap(pdev->devname,name,sizeof(name),UWIN_U2W);
	if(!(hp = createfile(name,GENERIC_READ,FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) && GetLastError()==ERROR_ACCESS_DENIED)
		hp = createfile(name,READ_CONTROL,FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(!hp)
		logerr(0, "CreateFile %s failed", name);
	hpsave = Phandle(fd);
	Phandle(fd) = hp;
	if((r = filefstat(fd,fdp,st))>=0)
		st->st_rdev = make_rdev(pdev->major,pdev->minor);
	closehandle(hp,HT_FILE);
	Phandle(fd) = hpsave;
	unix_time(&pdev->access,&st->st_atim,1);
	return(r);
}

static void control_terminal(int blkno)
{
	register Pdev_t *pdev = dev_ptr(blkno);
	setpgid(0,0);
	P_CP->sid = P_CP->pid;
	P_CP->console=blkno;
	pdev->tgrp = P_CP->pid;
	pdev->devpid = P_CP->pid;
	if(pdev->major==CONSOLE_MAJOR)
		SetErrorMode(0);
	else
		SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
	if(!utentry(pdev,1,NULL,NULL) && Share->init_thread_id)
	{
		int i=sizeof(pdev->uname)-1;
		GetUserName(pdev->uname, &i);
		pdev->uname[i] = 0;
		send2slot(Share->init_slot,3,P_CP->console);
	}
}

static void close_console(void)
{
	register Pfd_t*	fdp;
	register int	count = 0;
	register int	i;
	register int	n;
	register int	maxfds;

	if (maxfds = P_CP->maxfds)
	{
		for(i=0; i < maxfds; i++)
		{
			if((n=fdslot(P_CP,i))<0)
				continue;
			fdp = &Pfdtab[n];
			if(fdp->type!=FDTYPE_TTY && fdp->type!=FDTYPE_PTY && fdp->type != FDTYPE_CONSOLE)
				continue;
			if(fdp->devno==P_CP->console)
			{
				if(count++>0)
					return;
			}
		}
		if(!count)
			logmsg(0, "reference count for console is zero");
		P_CP->console = 0;
	}
}

static int ttyclose(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev;
	int r = 0;
	if(fdp->devno==0)
	{
		logmsg(0, "ttyclose devno==0 fd=%d fslot=%d refcount=%d type=%(fdtype)s index=%d noclose=%d", fd,file_slot(fdp),fdp->refcount,fdp->type,fdp->index,noclose);
		return(fileclose(fd, fdp, noclose));
	}
	pdev  = dev_ptr(fdp->devno);
	if(fdp->refcount> pdev->count)
		badrefcount(fd, fdp, pdev->count);
	r = fileclose(fd, fdp, noclose);
	if(pdev->count<=0)
		free_term(pdev, noclose);
	else
	{
		if(fdp->devno==P_CP->console)
			close_console();
		InterlockedDecrement(&pdev->count);
		if(noclose<0 && pdev->NtPid==P_CP->ntpid && !(P_CP->inexec&PROC_HAS_PARENT))
		{
			logmsg(LOG_DEV+6, "ttyclose of thread holder fd=%d", fd);
			pdev->NtPid = 0;
		}
	}
	return(r);
}

/*
 * returns true of no other file descriptor in this process has the
 * device open
 */
static lastopen(int fd, int devno)
{
	int slot,i = P_CP->maxfds;
	Pfd_t *fdp;
	while(--i>=0)
	{
		if(i==fd || (slot=fdslot(P_CP,i))<0)
			continue;
		fdp = &Pfdtab[slot];
		if(fdp->devno== devno)
		{
			return(0);
		}
	}
	return(1);
}

int ptyclose(int fd, Pfd_t* fdp,int noclose)
{
	register Pdev_t *pdev = dev_ptr(fdp->devno);
	int r = 0;
	if(fdp->refcount> pdev->count)
		badrefcount(fd,fdp,pdev->count);
	r = fileclose(fd,fdp,noclose);
	if(pdev->count<=0)
		r=free_pty(pdev, noclose);
	else
	{
		if(fdp->devno==P_CP->console)
			close_console();
		InterlockedDecrement(&pdev->count);
		if(noclose<0 && pdev->NtPid==P_CP->ntpid && lastopen(fd,fdp->devno))
		{
			logmsg(LOG_DEV+6, "ptyclose of thread holder fd=%d", fd);
			pdev->NtPid = 0;
			pdev->started = 0;
		}
	}
	return(r);
}

HANDLE pty_mcreate(Pdev_t *pdev, const char *pathname)
{
	char name[UWIN_RESOURCE_MAX];
	SECURITY_ATTRIBUTES sattr;
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = 0;
	sattr.bInheritHandle = TRUE;

	if (!CreatePipe(&pdev->Pseudodevice.m_inpipe_read, &pdev->Pseudodevice.m_inpipe_write,&sattr,PIPE_SIZE+24))
	{
		logerr(0, "CreatePipe");
		return(0);
	}
	if (!CreatePipe(&pdev->Pseudodevice.m_outpipe_read, &pdev->Pseudodevice.m_outpipe_write,&sattr,PIPE_SIZE+24))
	{
		logerr(0, "CreatePipe");
		return(0);
	}
	uwin_pathmap(pathname, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
	UWIN_EVENT_SYNC(name, pdev);
	pdev->Pseudodevice.SyncEvent = CreateEvent(&sattr, TRUE, FALSE,name);
	if(!pdev->Pseudodevice.SyncEvent)
		logerr(0, "CreateEvent %s failed", name);
	UWIN_EVENT_SELECT(name, pdev);
	pdev->Pseudodevice.SelEvent = CreateEvent(&sattr, TRUE, FALSE,name);
	if(!pdev->Pseudodevice.SelEvent)
		logerr(0, "CreateEvent %s failed", name);
	UWIN_EVENT_WRITE(name, pdev);
	pdev->write_event = CreateEvent(&sattr, TRUE, FALSE,name);
	if(!pdev->write_event)
		logerr(0, "CreateEvent %s failed", name);
	UWIN_MUTEX_SELECT(name, pdev);
	pdev->Pseudodevice.SelMutex = CreateMutex(&sattr, FALSE,name);
	if(!pdev->Pseudodevice.SelMutex)
		logerr(0, "CreateMutex %s failed", name);
	pdev->Pseudodevice.NumOfChar = 0;
	pdev->Pseudodevice.SelChar = 0;
	pdev->Pseudodevice.pseudodev = 1;
	pdev->Pseudodevice.master = 1;
	pdev->NtPid = P_CP->ntpid;
	pdev->MaxCol = 80;
	pdev->MaxRow = 24;
	return(pdev->Pseudodevice.m_inpipe_read);
}

HANDLE pty_screate(Pdev_t *pdev_master, Pdev_t *pdev,Pfd_t *fdp, const char *pathname, int master, int oflags, int slave)
{
	HANDLE hpin,hpout, me = GetCurrentProcess();

	if(!(hpin=dev_gethandle(pdev_master,pdev_master->Pseudodevice.m_outpipe_read)))
		logerr(0, "dev_gethandle");
	if(!(hpout=dev_gethandle(pdev_master,pdev_master->Pseudodevice.m_inpipe_write)))
		logerr(0, "dev_gethandle");
	pdev->Pseudodevice.pseudodev = master;
	pdev_master->Pseudodevice.pseudodev = slave;
	uwin_pathmap(pathname, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
	if(!(oflags & O_NOCTTY) && P_CP->console==0)
		control_terminal(slave);
	device_open(slave,hpin,fdp,hpout);
	return(pdev->Pseudodevice.m_inpipe_read);
}


/* console_open: if the requested console is already open, it returns
 * the handle. else, it opens a console and starts the terminal threads
 * by calling device_open
 */
HANDLE console_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp;
	int blkno, minor=ip->name[1];
	Pdev_t *pdev;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,CONSOLE_MAJOR);

	if(blkno=blocks[minor])	/* console already open */
	{
		fdp->devno = blkno;
		pdev = dev_ptr(blkno);
		hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
		if(hp)
			InterlockedIncrement(&pdev->count);
	}
	else
	{
		HANDLE in=NULL, out=NULL;

		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = CONSOLE_MAJOR;
		pdev->minor = minor;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
		blocks[minor]=blkno;
		if(!(oflags & O_NOCTTY) && P_CP->console==0)
			control_terminal(blkno);
		//if stdin/stdout are invalid, the new device will point to
		//the CONSOLE input/output
		device_open(blkno, Phandle(0), fdp, Phandle(1));
		hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
	}
	return(hp);
}

static HANDLE ttyselect(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	Pdev_t *pdev;
	int dontwait=0;
	if(hp)
	{
		if(type<0)
			closehandle(hp,HT_EVENT);
		return(0);
	}
	pdev = dev_ptr(fdp->devno);
	if(type==0)
	{
		if(hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput))
		{
			if(!PeekNamedPipe(hp,NULL,0,NULL,&dontwait,NULL))
				logerr(0, "PeekNamedPipe");
			if(GetLastError()==ERROR_BROKEN_PIPE)
				dontwait = 1;
			closehandle(hp,HT_PIPE);
			hp = 0;
		}
		else
			dontwait =1;
		if(!dontwait)
			hp = dev_gethandle(pdev, pdev->BuffFlushEvent);
	}
#if 0
	else if(type==1)
		hp = dev_gethandle(pdev,pdev->write_event);
#endif
	else if(type==2)
		hp = open_proc(pdev->NtPid,SYNCHRONIZE);
	return(hp);
}

static HANDLE consoleselect(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	Pdev_t *pdev = dev_ptr(fdp->devno);
	if(fd == 0 && P_CP->console && type<=0)
	{
		if(hp)
		{
			if(type<0 && pdev->state==2)
				pdev->state = 1;
		}
		else
		{
			if(pdev->state == 0)
			{
				ttysetmode(pdev);
				kill1(pdev->devpid, SIGCONT);
			}
			pdev->state = 2;
		}
	}
	return(ttyselect(fd,fdp,type,hp));
}


static HANDLE ptyselect(int fd, Pfd_t *fdp,int type, HANDLE hp)
{
	Pdev_t *pdev = dev_ptr(fdp->devno);
	if(hp)
	{
		if(type<0 && !((-type)&4))
			closehandle(hp,HT_EVENT);
		return(0);
	}
	if(pdev->Pseudodevice.pseudodev==0)
	{
		fdp->socknetevents |= FD_CLOSE;
		return(0);
	}
	if(!pdev->Pseudodevice.master)
		return(ttyselect(fd,fdp,type,hp));
	if(type==0)
		hp = dev_gethandle(pdev,pdev->Pseudodevice.SelEvent);
	else if(type==1)
		hp = dev_gethandle(pdev, pdev->write_event);
	else
		hp = pdev->Pseudodevice.pckt_event;
	return(hp);
}

void init_console(Devtab_t *dp)
{
	filesw(FDTYPE_CONSOLE,ttyread, ttywrite,ttylseek,ttyfstat,ttyclose,consoleselect);
	dp->openfn = console_open;
}

/* tty_open: if the tty is already opened, returns the handle to it.
 * else, opens the tty and starts the terminal threads by calling
 * device_open
 */
HANDLE tty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp, hp1, hp2, me = GetCurrentProcess();
	int blkno, minor=ip->name[1];
	Pdev_t *pdev;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,TTY_MAJOR);

	if(blkno=blocks[minor])	/* tty already open */
	{
		fdp->devno = blkno;
		pdev = dev_ptr(blkno);
		hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
		if(hp)
			InterlockedIncrement(&pdev->count);
		if(!(oflags & O_NOCTTY) && P_CP->console==0 && pdev->tgrp==0)
			control_terminal(blkno);
	}
	else //tty not open
	{
		if(!(hp=open_comport((unsigned short)minor)))
			return 0;
		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = TTY_MAJOR;
		pdev->minor = minor;
		blocks[minor]=blkno;
		uwin_pathmap(ip->path, pdev->devname,sizeof(pdev->devname),UWIN_W2U);
		if(!(oflags & O_NOCTTY) && P_CP->console==0)
			control_terminal(blkno);

		/* Create overlapped events for device threads */
		pdev->ReadWaitCommEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
		pdev->ReadOverlappedEvent = CreateEvent( NULL,TRUE,FALSE, NULL);
		pdev->WriteOverlappedEvent = CreateEvent( NULL,TRUE,FALSE, NULL);

		if(!DuplicateHandle(me, hp,me, &hp1,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(me, hp,me, &hp2,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		device_open(blkno, hp1, fdp, hp2);
	}
	return(hp);
}

/* pty_open: if the pty is already opened, returns the handle to it.
 * else, opens the pty and starts the terminal threads by calling
 * device_open
 */
HANDLE pty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hp, me = GetCurrentProcess();
	int blkno, minor=ip->name[1];
	Pdev_t *pdev;
	unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);


	logmsg(LOG_DEV+2,"pty_open minor=%d",minor);
	if(minor < MAX_PTY_MINOR)
	{
		/* open master, this could be made atomic */
		if(blocks[minor])
		{
			errno = EIO;
			return(0);	/* already open */
		}
		// Error if slave is not free
		if(blocks[minor+MAX_PTY_MINOR])
		{
			logmsg(LOG_DEV+6, "slave not free minor=%d", minor);
			errno = EIO;
			return(0);
		}
		if((blkno = block_alloc(BLK_PDEV))==0)
			return(0);
		pdev = dev_ptr(blkno);
		ZeroMemory((void *)pdev, BLOCK_SIZE-1);
		pdev->major = PTY_MAJOR;
		pdev->minor = minor;
		if((hp=pty_mcreate(pdev,ip->path))==0)
		{
			logerr(0, "pty_mcreate %s failed", ip->path);
			block_free((unsigned short)blkno);
			return(0);
		}
		blocks[minor] = blkno;
		fdp->devno = blkno;
		*extra=pdev->Pseudodevice.m_outpipe_write;
		if(!DuplicateHandle(me, hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(!DuplicateHandle(me, *extra,me, extra,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
	}
	else
	{
		if(blkno=blocks[minor])	/* device already open */
		{
			fdp->devno = blkno;
			pdev = dev_ptr(blkno);
			logmsg(LOG_DEV+2,"pty_open blkno=%d",blkno);
			hp=dev_gethandle(pdev, pdev->Pseudodevice.m_inpipe_read);
			*extra=dev_gethandle(pdev, pdev->Pseudodevice.m_outpipe_write);
			if(hp)
				InterlockedIncrement(&pdev->count);
			if(!(oflags & O_NOCTTY) && P_CP->console==0 && pdev->tgrp==0)
				control_terminal(blkno);
		}
		else
		{
			Pdev_t *pdev_master;

			if((blkno = block_alloc(BLK_PDEV))==0)
				return(0);
			blocks[minor] = blkno;
			pdev_master = dev_ptr(blocks[minor-MAX_PTY_MINOR]);
			pdev_master->Pseudodevice.pseudodev = blkno;
			pdev = dev_ptr(blkno);
			logmsg(LOG_DEV+2,"pty_open blkno=%d",blkno);
			ZeroMemory((void *)pdev, BLOCK_SIZE-1);
			pdev->major = PTY_MAJOR;
			pdev->minor = minor;
			hp = pty_screate(pdev_master, pdev,fdp, ip->path, blocks[minor-MAX_PTY_MINOR], oflags, blkno);
			*extra =pdev->Pseudodevice.m_outpipe_write;
			if(!DuplicateHandle(me, hp,me, &hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			if(!DuplicateHandle(me, *extra,me, extra,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
		}
	}
	return(hp);
}

HANDLE devtty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	Pdev_t *pdev;
	HANDLE hp;
	if(!P_CP->console)
	{
		errno = ENXIO;
		return(NULL);
	}

	pdev = dev_ptr(P_CP->console);
	logmsg(LOG_DEV+2, "devtty_open dev=<%d,%d>", block_slot(pdev), pdev->major);

	InterlockedIncrement(&pdev->count);
	fdp->devno = P_CP->console;
	fdp->type = Chardev[pdev->major].type;
	if(fdp->type==FDTYPE_PTY)
	{
		pdev->major = PTY_MAJOR;
		hp=dev_gethandle(pdev,pdev->Pseudodevice.m_inpipe_read);
		*extra=dev_gethandle(pdev, pdev->Pseudodevice.m_outpipe_write);
	}
	else if(fdp->type == FDTYPE_CONSOLE)
	{
		pdev->major = CONSOLE_MAJOR;
		hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
		*extra=dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
	}
	else
	{
		pdev->major = TTY_MAJOR;
		hp=dev_gethandle(pdev,pdev->SrcInOutHandle.ReadPipeInput);
		*extra=dev_gethandle(pdev,pdev->SrcInOutHandle.WritePipeOutput);
	}
	return(hp);
}

/*   This function allocates a new master side pty and returns handle to
 *   the master
 */
static HANDLE devpty_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	DWORD flags=GENERIC_READ;
	DWORD cflags=OPEN_EXISTING;
	char devname[20];
	unsigned short *blocks;
	int i, j, info_flags = P_EXIST;
	HANDLE mh;
	char arrX[] = {'p', 'q', 'r', 's', 't', 'u', 'v', 'w'};
	char arrY[] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e', 'f'};

	strcpy(devname, "/dev/ptyXY");
	setupflags(oflags, &info_flags, &flags, &cflags);
	if(fdp->index)
		info_flags |= P_MYBUF;

	for(i=0;i<8;i++)
		for(j=0;j<16;j++)
		{
			devname[8] = arrX[i];
			devname[9] = arrY[j];

			if(!pathreal(devname, info_flags, ip))
				continue;
			closehandle(ip->hp,HT_FILE);

			blocks = devtab_ptr(Share->chardev_index, ip->name[0]);
			if(blocks[ip->name[1]] || blocks[ip->name[1]+128])
				continue; /* PTY cannot be allocated */

			dp = &Chardev[ip->name[0]];
			mh = pty_open(dp, fdp, ip, oflags, extra);
			if(mh)
				return(mh);
		}
	return(0);
}

void init_tty(Devtab_t *dp)
{
	filesw(FDTYPE_TTY,ttyread, ttywrite,ttylseek,ttyfstat,ttyclose,ttyselect);
	dp->openfn = tty_open;
}

void init_pty(Devtab_t *dp)
{
	filesw(FDTYPE_PTY,ptyread, ptywrite,ttylseek,ttyfstat,ptyclose,ptyselect);
	dp->openfn = pty_open;
}

void init_devtty(Devtab_t *dp)
{
	dp->openfn = devtty_open;
}

void init_devpty(Devtab_t *dp)
{
	dp->openfn = devpty_open;
}

int dev_getdevno(int type)
{
	int major = CONSOLE_MAJOR;
	register int minor=0, blkno;
	Devtab_t *dp;
	register Pdev_t *pdev;
	unsigned short *blocks;
	struct stat statbuff;

	if(type==FDTYPE_CONSOLE)
		minor = 10;
	else
		major = TTY_MAJOR;
	dp = &Chardev[major];
	blocks = devtab_ptr(Share->chardev_index,major);

	for(; minor<dp->minor; minor++)
	{
		if(blocks[minor]) // minor device already allocated
			continue;
		goto found;
	}
	return -1;
found:
	if((blkno = block_alloc(BLK_PDEV))==0)
		return -1;
	blocks[minor] = blkno;
	pdev = dev_ptr(blkno);
	ZeroMemory((void *)pdev, BLOCK_SIZE-1);
	pdev->minor = minor;
	pdev->major = major;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && type==FDTYPE_CONSOLE)
		pdev->notitle = 1;
	sfsprintf(pdev->devname,sizeof(pdev->devname),"/dev/tty%d",minor);
	control_terminal(blkno);
	if(stat(pdev->devname,&statbuff)) /* file doesnt exists */
	{
		mode_t mask = P_CP->umask;
		P_CP->umask = 0;
		mknod(pdev->devname,S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH,(pdev->major<<8)|(0xff&pdev->minor));
		P_CP->umask = mask;
	}
	return(blkno);
}

#ifdef CloseHandle
#   undef CloseHandle
    int myCloseHandle(HANDLE hp, const char *file, int line)
    {
	WINBASEAPI BOOL WINAPI CloseHandle(HANDLE);
	DWORD devpid=0;
	int badclose=0;
	Pdev_t *pdev = 0;
	if(!P_CP)
		goto doclose;
	if(P_CP->console)
	{
		pdev = dev_ptr(P_CP->console);
		devpid = pdev->NtPid;
	}
	if(devpid ==P_CP->ntpid)
	{
		if(pdev->DeviceInputHandle == hp)
			badclose = 1;
		else if(pdev->DeviceOutputHandle == hp)
			badclose = 2;
		else if(pdev->SrcInOutHandle.ReadPipeInput == hp)
			badclose = 3;
		else if(pdev->SrcInOutHandle.ReadPipeOutput == hp)
			badclose = 4;
		else if(pdev->SrcInOutHandle.WritePipeInput == hp)
			badclose = 5;
		else if(pdev->SrcInOutHandle.WritePipeOutput == hp)
			badclose = 6;
		else if(pdev->BuffFlushEvent==hp)
			badclose = 7;
		else if(pdev->SrcInOutHandle.ReadThreadHandle==hp)
			badclose = 8;
		else if(pdev->SrcInOutHandle.WriteThreadHandle==hp)
			badclose = 9;
		else if(pdev->WriteSyncEvent==hp)
			badclose = 10;
		else if(pdev->write_event==hp)
			badclose = 21;
		else if(pdev->Pseudodevice.pseudodev)
		{
			pdev = dev_ptr(pdev->Pseudodevice.pseudodev);
			devpid = pdev->NtPid;
		}
	}
	if(badclose==0 && devpid ==P_CP->ntpid)
	{
		if(pdev->Pseudodevice.m_outpipe_read==hp)
			badclose = 11;
		else if(pdev->Pseudodevice.m_inpipe_write==hp)
			badclose = 12;
		else if(pdev->Pseudodevice.m_outpipe_write==hp)
			badclose = 13;
		else if(pdev->Pseudodevice.m_inpipe_read==hp)
			badclose = 14;
		else if(pdev->Pseudodevice.SelEvent==hp)
			badclose = 15;
		else if(pdev->Pseudodevice.SelMutex==hp)
			badclose = 16;
	}
	if(P_CP->sigevent==hp)
		badclose = 17;
	else if(P_CP->sevent==hp)
		badclose = 18;
	else if(P_CP->alarmevent==hp)
		badclose = 19;
	else if(pdev && hp && pdev->process==hp)
		badclose = 20;
	if(badclose && (!pdev || pdev->started))
	{
		logsrc(0, file, line, "badclose handle=%p close=%d", hp, badclose);
		breakit();
	}
doclose:
	if(!CloseHandle(hp))
	{
		logsrc(0+LOG_SYSTEM, file, line, "CloseHandle failed handle=%p", hp);
		return(FALSE);
	}
	return(TRUE);
    }
#endif
