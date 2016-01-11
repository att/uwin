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
#include	"uwindef.h"
#include	<termio.h>
#include	<signal.h>
#include	<time.h>
#include	<uwin.h>
#include	<sys/procfs.h>
#include	<sys/mtio.h>
#include	"procdir.h"
#ifdef GTL_TERM
#   include	"raw.h"
#endif

extern int adjust_window_size(int, int, HANDLE);

#define	OPCODE(x)	((x)&IOCPARM_MASK)

//////////////////////////////////////////////////////////////////////
//	WARNING: throughout the ioctl function we sometimes need the
//	physical input handle, and/or the physical output handle for
//	the console.  There is an assumption made that the process
//	issuing this ioctl() has inherited the handle as the value
//	pdev->io.physical.input
// and
//	pdev->io.physical.output
//	This assumption is made because windows does not allow you to
//	duplicate a console handle for use between processes.  So when
//	the pdev for the console is constructed, we place the real
//	input and output handles here.
//
// Another assumption is made that when you enable packet mode for
// a pty, that you are the owner of the pty structure.  This too is
// problematic and we will have to correct this... possibly by dupping
// the packet_event handle into the owner process.
//////////////////////////////////////////////////////////////////////
int ioctl(int fd,int arg,...)
{
	va_list ap;
	int ret= -1, type = (arg>>8) &IOCPARM_MASK;

	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(-1);
	}
	sigdefer(1);
	va_start(ap,arg);
	switch(type)
	{
	    case 'S':
		errno = ENOSYS;
		break;
	    case 'P':
	    {
		int *val;
		val  = va_arg(ap,int*);
		ret=dsp_ioctl(fd,arg,val);
		break;
	    }
	    case 'f':
	    {
		int *val;
		if(arg==FIONBIO)
		{
			val  = va_arg(ap,int*);
			if(*val != 0)
				getfdp(P_CP,fd)->oflag |= O_NONBLOCK;
			else
				getfdp(P_CP,fd)->oflag &= ~O_NONBLOCK;
			/* If fd is a socket, pass ioctl to windows */
			if (issocket(fd))
				ioctlsocket(Phandle(fd), FIONBIO, val);
			ret = 0;
			break;
		}
		if(arg==FIONREAD)
		{
			HANDLE hp=Phandle(fd);
			Pdev_t *pdev=0;
#ifdef GTL_TERM
			Pdev_t *pdev_c=0;
#endif
			switch(getfdp(P_CP,fd)->type)
			{
			    case FDTYPE_SOCKET:
			    case FDTYPE_DGRAM:
				break;
			    case FDTYPE_CONSOLE:
				pdev = dev_ptr(getfdp(P_CP,fd)->devno);
#ifdef GTL_TERM
				if(!(pdev_c = pdev_lookup(pdev)))
					goto done;
				hp = pdev_c->io.input.hp;
#else
				hp = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadPipeInput);
#endif
			    case FDTYPE_PTY:
			    case FDTYPE_FIFO:
			    case FDTYPE_EFIFO:
			    case FDTYPE_PIPE:
			    case FDTYPE_EPIPE:
			    case FDTYPE_NPIPE:
				val  = va_arg(ap,int*);
				if(PeekNamedPipe(hp,NULL,0,NULL,&ret,NULL))
				{
#ifndef GTL_TERM
					if(pdev)
						closehandle(hp,HT_PIPE);
#endif
					*val = ret;
					goto done;
				}
				if((ret=GetLastError())==ERROR_BROKEN_PIPE)
				{
					ret = 0;
					goto done;
				}
				errno = unix_err(ret);
				ret = -1;
				goto done;
			    default:
				errno = EINVAL;
				goto done;
			}
		}
	    }
	    /* FALL THRU */
	    case 's':
	    {
		u_long *val;
		Pfd_t *fdp = getfdp(P_CP,fd);
		val = va_arg(ap, u_long*);
		if(issocket(fd))
			ret = ioctlsocket(Phandle(fd), arg, val);
		else
			errno = EINVAL;
		break;
	    }
	    case 't':
		{
			Pfd_t *fdp;
			Pdev_t *pdev;
			struct termios *tp=0;
			int mask, *mptr;
			switch (OPCODE(arg))
			{
			case 0:
			case TCSANOW:
			case TCSADRAIN:
			case TCSAFLUSH:
				tp = va_arg(ap, struct termios*);
				break;
			case OPCODE(TIOCMSET):
			case OPCODE(TIOCMBIS):
			case OPCODE(TIOCMBIC):
			case OPCODE(TIOCSPGRP):
				mask  = va_arg(ap,int);
				break;
			case OPCODE(TIOCMGET):
			case OPCODE(TIOCGPGRP):
				mptr  = va_arg(ap,int*);
				break;
			case OPCODE(TIOCPKT):
				{
				int* on = 0;
				fdp = getfdp(P_CP,fd);
				if((fdp->type==FDTYPE_SOCKET) || (fdp->type==FDTYPE_DGRAM))
				{
					errno = ENOTTY;
					goto done;
				}
				on = va_arg(ap, int*);
				pdev = dev_ptr(fdp->devno);
				/* Enable packet mode*/
#ifdef GTL_TERM
				if((*on)&&(!(pdev->io.master.packet_event)))
#else
				if((*on)&&(!(pdev->Pseudodevice.pckt_event)))
#endif
				{
#ifdef GTL_TERM
					if((pdev->major == PTY_MAJOR)&&(pdev->io.master.masterid))
#else
					if((pdev->major == PTY_MAJOR)&&(pdev->Pseudodevice.master))
#endif
					{
						HANDLE e=NULL;
						HANDLE me= GetCurrentProcess();
#ifdef GTL_TERM
						e=pdev_createEvent(FALSE);
#else
						e = CreateEvent(sattr(1),TRUE,FALSE,NULL);
#endif
						if(pdev->NtPid == GetCurrentProcessId())
#ifdef GTL_TERM
							pdev->io.master.packet_event = e;
#else
							pdev->Pseudodevice.pckt_event = e;
#endif
						else
						{
							HANDLE ph;
							if (ph = OpenProcess(PROCESS_DUP_HANDLE,FALSE,pdev->NtPid))
#ifdef GTL_TERM
								if(pdev_duphandle(me,e,ph,&(pdev->io.master.packet_event)))
#else
								if(!DuplicateHandle(me,e,ph,&(pdev->Pseudodevice.pckt_event),0,FALSE,DUPLICATE_SAME_ACCESS))
#endif
									logerr(0, "pdev_duphandle");
							closehandle(e,HT_EVENT);
						}
						ret = 0;
					}
					goto done;
				}
				/*Disable Packet mode*/
#ifdef GTL_TERM
				if((!(*on))&&(pdev->io.master.packet_event))
#else
				if((!(*on))&&(pdev->Pseudodevice.pckt_event))
#endif
				{
					if(pdev->NtPid==GetCurrentProcessId())
					{
#ifdef GTL_TERM
						if(closehandle(pdev->io.master.packet_event,HT_EVENT))
						{
							pdev->io.master.packet_event=0;
							ret = 0;
						}
#else
						if(closehandle(pdev->Pseudodevice.pckt_event,HT_EVENT))
						{
							pdev->Pseudodevice.pckt_event=0;
							ret = 0;
						}
#endif
						logerr(0, "CloseHandle");
						goto done;
					}
					else
					{
#ifdef GTL_TERM
						HANDLE ph= OpenProcess(PROCESS_DUP_HANDLE,FALSE,pdev->NtPid);
						HANDLE e=pdev->io.master.packet_event;
						Pdev_t *pdev_c=pdev_lookup(pdev);
						pdev->io.master.packet_event=0;
						if(ph)
						{
							pdev_event(e,PULSEEVENT,0);
							if(DuplicateHandle(ph,e,GetCurrentProcess(),&(e),0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
								closehandle(e,HT_EVENT);
						}
#endif
						goto done;
					}
				}
				}
				break;
			}
			if(arg&IOCPARM_MASK)
			{
				if(tp && wait_foreground(getfdp(P_CP,fd))<0)
					goto done;
				tcsetattr(fd,arg&IOCPARM_MASK,tp);
			}
			else
				tcgetattr(fd,tp);
			switch (OPCODE(arg))
			{
			case 0:
				ret = tcgetattr(fd,tp);
				goto done;
			case TCSANOW:
			case TCSADRAIN:
			case TCSAFLUSH:
				if(wait_foreground(getfdp(P_CP,fd))<0)
					goto done;
				ret = tcsetattr(fd,arg&IOCPARM_MASK,tp);
				goto done;
			case OPCODE(TIOCGPGRP):
				ret = tcgetpgrp(fd);
				if(ret>=0)
				{
					*mptr = ret;
					ret = 0;
				}
				break;
			case OPCODE(TIOCSPGRP):
				ret = tcsetpgrp(fd,mask);
				break;
			case OPCODE(TIOCMSET):
				dtrsetclr(fd,~mask,0);
				dtrsetclr(fd,mask,1);
				break;
			case OPCODE(TIOCCBRK):
				dtrsetclr(fd,TIOCM_BRK,0);
				break;
			case OPCODE(TIOCCDTR):
				mask = TIOCM_DTR;
			case OPCODE(TIOCMBIC):
				dtrsetclr(fd,mask,0);
				break;
			case OPCODE(TIOCSBRK):
				dtrsetclr(fd,TIOCM_BRK,1);
				break;
			case OPCODE(TIOCSDTR):
				mask = TIOCM_DTR;
			case OPCODE(TIOCMBIS):
				dtrsetclr(fd,mask,1);
				break;
			case OPCODE(TIOCMGET):
				getmodemstatus(fd, mptr);
				break;
			case OPCODE(TIOCNOTTY):
				fdp = getfdp(P_CP,fd);
				if(!isatty(fd) && fdp->type!=FDTYPE_PTY)
				{
					errno = ENOTTY;
					goto done;
				}
				P_CP->console = 0;
				FreeConsole();
				break;
			case OPCODE(TIOCSCTTY):
				fdp = getfdp(P_CP,fd);
				if(!isatty(fd) && fdp->type!=FDTYPE_PTY)
				{
					errno = ENOTTY;
					goto done;
				}
				pdev = dev_ptr(fdp->devno);
#ifdef GTL_TERM
				if(fdp->type==FDTYPE_PTY && pdev->io.master.masterid)
					pdev = dev_ptr(pdev->io.slave.iodev_index);
#else
				if(fdp->type==FDTYPE_PTY && pdev->Pseudodevice.master)
					pdev = dev_ptr(pdev->Pseudodevice.pseudodev);
#endif
				if(P_CP->console)
				{
					if(P_CP->console==fdp->devno)
						break;
					errno = EPERM;
					goto done;
				}
				pdev->tgrp = P_CP->pgid;
				P_CP->console = fdp->devno;
				pdev->devpid = P_CP->pid;
				break;
			default:
				errno=EINVAL;
				goto done;
			}
			ret = 0;
			goto done;
		}
	case 'g':
		{
			pid_t  *grp,gid;
			grp = va_arg(ap, pid_t*);
			if(arg&0xff)
				ret = tcsetpgrp(fd,*grp);
			else
			{
				if((gid = tcgetpgrp(fd)) <0)
					goto done;
				*grp = gid;
				ret = 0;
			}
			break;
		}
	case 'w':
		{
			Pfd_t *fdp;
			Pdev_t *devtab;
			struct winsize *wp;
			CONSOLE_SCREEN_BUFFER_INFO info;
			HANDLE hp = 0;
			fdp = getfdp(P_CP,fd);
			if(!isatty(fd) && fdp->type!=FDTYPE_PTY)
				goto done;
			wp = va_arg(ap, struct winsize*);
			devtab = dev_ptr(fdp->devno);
#ifdef GTL_TERM
			if(fdp->type==FDTYPE_PTY && devtab->io.master.masterid)
			{
				unsigned short *blks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
				if(blks[devtab->minor+MAX_PTY_MINOR])
					devtab = dev_ptr(devtab->io.slave.iodev_index);
			}
			///////////////////////////////////////////////////
			// NOTE:  there is an assumption here that this process
			//	  has inheritted the devtab->io.physical.output
			//	  handle.  This may (or may not) be true.
			//	  Use at your own risk until we get this
			//	  resolved.
			///////////////////////////////////////////////////
			if(fdp->type == FDTYPE_CONSOLE)
				hp = devtab->io.physical.output;
#else
			if(fdp->type==FDTYPE_PTY && devtab->Pseudodevice.master)
			{
				unsigned short *blks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
				if(blks[devtab->minor+MAX_PTY_MINOR])
					devtab = dev_ptr(devtab->Pseudodevice.pseudodev);
			}
			if(fdp->type == FDTYPE_CONSOLE)
				hp = devtab->DeviceOutputHandle;
#endif
			if(OPCODE(arg) == OPCODE(TIOCSWINSZ))
			{
				COORD coord;
				if(fdp->type == FDTYPE_CONSOLE )
				{
					coord.X = wp->ws_col+1;
					coord.Y = wp->ws_row;
					adjust_window_size((int)coord.X, (int)coord.Y, hp);
					devtab->MaxRow = coord.Y;
					devtab->MaxCol = coord.X;
				}
				else if(fdp->type == FDTYPE_PTY)
				{
					devtab->MaxRow = wp->ws_row;
					devtab->MaxCol = wp->ws_col;
				}
				/* Size is same as before if not tty; ioctl ignored */
			}
			else
			{
				if(fdp->type == FDTYPE_CONSOLE)
				{
					if(!GetConsoleScreenBufferInfo(hp,&info))
					{
						if(hp)
							logerr(0, "GetConsoleScreenBufferInfo");
						errno = EINVAL;
						goto done;
					}
					devtab->MaxRow=(info.srWindow.Bottom - info.srWindow.Top)+1;
					devtab->MaxCol=(info.srWindow.Right - info.srWindow.Left);
				}
				wp->ws_row = devtab->MaxRow;
				wp->ws_col = devtab->MaxCol;
#if 0
				if (Share->Platform != VER_PLATFORM_WIN32_NT)
					wp->ws_col-=1; //Temp patch for long line problem in VI
#endif
			}
			ret = 0;
		}
		break;

	case 'q':
		{
			Pproc_t *proc;
			Pfd_t *fdp;
			int sig, *maxsig;
			sigset_t *set;
			struct tms tms;
			struct prpsinfo* pr;
			pid_t pid = (pid_t)Phandle(fd)-1;
			DWORD ntpid;
			char buf[PATH_MAX];
			fdp = getfdp(P_CP,fd);
			if (fdp->type != FDTYPE_PROC)
			{
				errno = EINVAL;
				goto done;
			}
			if(proc = proc_getlocked(pid, 0))
			{
				ntpid = proc->ntpid;
				proc_release(proc);
			}
			else
			{
				HANDLE ph;
				ntpid = uwin_ntpid(pid);
				ph= OpenProcess(PROCESS_DUP_HANDLE,FALSE,ntpid);
				if(ph)
					closehandle(ph,HT_PROC);
				else if(GetLastError()==ERROR_INVALID_PARAMETER)
				{
					errno = EINVAL;
					goto done;
				}
			}
			switch(arg&0xff)
			{
			case 1:  /* Get Process status */
			case 2:  /*  POST stop request and wait....*/
			case 3:

			case 7:  /*  Get the current signal */
				errno = ENOSYS;
				break;
			case 8:  /*  PIOCKILL */
				sig = va_arg(ap, int);
				ret = kill(pid, sig);
				goto done;

			case 9: /* PIOCUNKILL */
				sig = va_arg(ap, int);
				if (sig <=0 || sig >= NSIG || sig==SIGKILL )
				{
					errno = EINVAL;
					goto done;
				}
				if(proc)
					sigsetgotten(proc, sig, 0);
				ret = 0;
				goto done;
			case 10: /* PIOCGHOLD Get held signal set*/
				set = va_arg(ap, sigset_t *);
				if (IsBadWritePtr(set, sizeof(*set)))
				{
					errno = EFAULT;
					goto done;
				}
				if(proc)
					*set = (proc->siginfo.siggot & proc->siginfo.sigmask);
				else
					*set = 0;
				ret = 0;
				goto done;
			case 11: /* Set held signal */
				set = va_arg(ap, sigset_t *);
				ret = sigprocmask(SIG_SETMASK,set,(sigset_t)NULL);
				goto done;
			case 12: /* Get Max signal number */
				maxsig = va_arg(ap, int*);
				*maxsig = NSIG;
				ret = 0;
				goto done;
			case 13: /* PIOCACTION: Open process memory & read the table */
				{
					HANDLE	hp;
					BOOL r;
					void (**sighand)(int);
					if(!proc)
					{
						errno = EINVAL;
						goto done;
					}
					sighand = va_arg(ap, void*);
					if (IsBadWritePtr(sighand, sizeof(*sighand)*NSIG))
					{
						errno = EFAULT;
						goto done;
					}
					if (!wrflag(fdp->oflag))
					{
						errno = EBADF;
						goto done;
					}
					if (!(hp = OpenProcess(PROCESS_VM_READ, FALSE, ntpid)))
					{
						errno = unix_err(GetLastError());
						goto done;
					}
					r = ReadProcessMemory(hp, proc->sigact, sighand, sizeof(void(*)(int)) * NSIG, 0);
					closehandle(hp,HT_PROC);
					if (!r)
						errno = unix_err(GetLastError());
					else
						ret = 0;
					goto done;
				}
			case 30: /* Fill the psinfo struct */
				pr = va_arg(ap, prpsinfo_t*);
				if (IsBadWritePtr(pr, sizeof(*pr)))
				{
					errno = EFAULT;
					goto done;
				}
				prpsinfo(pid, proc, pr, buf, sizeof(buf));
				if (proc)
				{
					sfsprintf(pr->pr_psargs, sizeof(pr->pr_psargs), "%s", proc->cmd_line);
					if(Share->Platform==VER_PLATFORM_WIN32_NT && strlen(proc->cmd_line)==sizeof(P_CP->cmd_line)-1)
						proc_commandline(proc->ntpid, pr);
					pr->pr_state = proc->state;
					pr->pr_zomb = procexited(proc);
#if 0
					pr->pr_nice =
#endif
#if 0
					pr->pr_flag =
#endif
					pr->pr_uid = proc->uid;
					pr->pr_gid = proc->gid;
					pr->pr_pid = proc->pid;
					pr->pr_ppid = proc->ppid;
					pr->pr_pgrp = proc->pgid;
					pr->pr_sid = proc->sid;
#if 0
					pr->pr_pri = proc->priority;
#endif
					pr->pr_ntpid = proc->ntpid;
					pr->pr_refcount = proc->inuse;
					unix_time(&proc->start, &pr->pr_start, 1);
					proc_times(proc, &tms);
					pr->pr_time = ((tms.tms_utime + tms.tms_stime))/HZ;
				}
				pr->pr_sname = *fmtstate(pr->pr_state, 1);
				pr->pr_ottydev = (unsigned short)pr->pr_lttydev;
				ret = 0;
				goto done;
			default:
				errno=ENOSYS;
				goto done;
			}
		}

	case 'i':
		{
			int  mode;
			mode = va_arg(ap, int);
			switch(arg&0xff)
			{
			case 0:  /* TCSBRK */
				if (!mode)
					tcsendbreak(fd, mode);
				/* Output drain is not needed because of the WriteThread */
				break;
			case 1: /* TCXONC Start/Stop control */
				tcflow ( fd, mode);
				break;
			case 2:
				tcflush(fd, mode );
				break;
			default:
				errno = EINVAL;
				goto done;
			}
			ret = 0;
			goto done;
		}
	case 'm': //tape ioctls
		{
			Pfd_t *fdp;

			fdp = getfdp(P_CP,fd);
			if(fdp->type != FDTYPE_TAPE)
			{
				errno = EINVAL;
				goto done;
			}

			if(OPCODE(arg) == OPCODE(MTIOCTOP))
			{
				//MTIOCTOP - magnetic tape operation command
				struct mtop *mp = va_arg(ap, struct mtop*);

				ret = tape_ioctl(fd,(struct mtop*) mp, 1);
			}
			else if(OPCODE(arg) == OPCODE(MTIOCGET))
			{
				struct mtget *mg = va_arg(ap, struct mtget*);

				ret = tape_ioctl(fd, (struct mtget*) mg, 2);
			}
			else if(OPCODE(arg) == OPCODE(MTIOCPOS))
			{
				//MTIOCPOS -  mag tape get position command
				struct mtpos* mtp = va_arg(ap, struct mtpos*);

				ret = tape_ioctl(fd, (struct mtpos*) mtp, 3);
				//not supported
			}
		}
		default:
			errno = EINVAL;
	}
done:
	sigcheck(0);
	return(ret);
}
