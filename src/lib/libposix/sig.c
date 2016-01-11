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
#include "uwindef.h"
#include <setjmp.h>
#include "socket.h"

#define SIGBAD(signo) 	(((unsigned)(signo)) >= NSIG )
#define SIGBIT(sig)	(1L << ((sig)-1) )
static int sig_thrd_strt;
static int count;
static DWORD	myntpid;

#define FD_CEVT		(1 << 2)	/* 0x04 */

/*
 * This will be changed to map 3->21 and 21->3 for old a.outs
 */
static __inline sigfix(int sig)
{
	if((SIGQUIT==3) && dllinfo._ast_libversion<=9)
	{
		if(sig==3)
			sig = 21;
		else if(sig==21)
			sig = 3;
	}
	return(sig);
}

#define SWAPMASK	(SIGBIT(3)|SIGBIT(21))
/*
 * swap bits 3 and 21 if needed
 */
static __inline void sigfixmask(sigset_t *mask)
{
	if((SIGQUIT==3) && dllinfo._ast_libversion<=9)
	{
		sigset_t m = *mask;
		*mask &= ~SWAPMASK;
		if(m&SIGBIT(21))
			*mask |= SIGBIT(3);
		if(m&SIGBIT(3))
			*mask |= SIGBIT(21);
	}
}

static void (*sigfun[NSIG]) (int);
static sigset_t sigmasks[NSIG];
static void setdflsig (int);

extern void _exit(int);
static void suspend_proc(int);
static void resume_proc(int);
static void ignore_proc(int);
static long restartset;
static long reset;
static long noblock;

static void sig_abort(int sig)
{
	int sigstate;
	logmsg(LOG_PROC+3, "sig_abort ExitProcess 0x%x", sig);
	InterlockedIncrement(&P_CP->inuse);
	sigstate=P(0);
	P_CP->state = PROCSTATE_ABORT;
	P_CP->exitcode = sig;
	V(0,sigstate);
	fdcloseall(P_CP);
	if(P_CP->child)
		proc_orphan(P_CP->pid);
	proc_release(P_CP);
	ExitProcess(sig);
}

HANDLE mkevent(const char* ename, SECURITY_ATTRIBUTES* sattr)
{
	HANDLE event;
	SECURITY_ATTRIBUTES sdefault;
	if (!sattr)
	{
		sattr = &sdefault;
		sattr->nLength = sizeof(*sattr);
		sattr->lpSecurityDescriptor = nulldacl();
		sattr->bInheritHandle = FALSE;
	}
	if ((event = CreateEvent(sattr, TRUE, FALSE, ename)) == INVALID_HANDLE_VALUE)
		event = 0;
	if (!event)
		logerr(0, "CreateEvent %s failed", ename);
	return(event);
}

static pid_t getcurrenttgrp(void)
{
	if (P_CP->console == 0)
		return (-1);
	return(dev_ptr(P_CP->console)->tgrp);
}
static BOOL WINAPI sig_keyboard(DWORD type)
{
	register int sig;
	register pid_t tgrp;

	switch(type)
	{
		case CTRL_C_EVENT: 			//Control-C Event
			sig=SIGINT;
			if(sigfun[sig]==SIG_DFL)
				return(FALSE);
			tgrp=getcurrenttgrp();
			if((kill(-tgrp,sig) >= 0)) //Session Process Group
				return(TRUE);
			break;
		case CTRL_BREAK_EVENT: 		//Control-Break
			sig=SIGQUIT;
			if(sigfun[sig]==SIG_DFL)
				return(FALSE);
			if((kill(-P_CP->pid,sig) >= 0)) //Current Process Group
				return(TRUE);
			break;
		//case CTRL_CLOSE_EVENT: 		//Console Close
		//case CTRL_LOGOFF_EVENT: 	//Log off
		case CTRL_SHUTDOWN_EVENT: 	//ShutDown
			sig=SIGHUP;
			if(sigfun[sig]==SIG_DFL)
				return(FALSE);
			tgrp=getcurrenttgrp();
			// Current Process Group + Session Process Group
			//if((kill(-tgrp,sig) >= 0) && (kill(-P_CP->pid,sig) >=0))
			//	return(TRUE);
			signal(SIGHUP, SIG_IGN);
			kill(-tgrp,sig);
			Sleep(300);
			kill(-P_CP->pid,sig);
			Sleep(300);
			signal(SIGHUP, SIG_DFL);
			kill(P_CP->pid, sig);
			return TRUE;
			break;
	}
	return(FALSE);
}

static void sigterm_proc(int sig)
{
	logmsg(LOG_PROC+3, "sigterm_proc exitcode 0x%x", sig);
	P_CP->exitcode = sig ;
	P_CP->state = PROCSTATE_ABORT;
	P_CP->posixapp &= ~ORPHANING;
	P_CP->siginfo.siggot = 0;
	/* windows 95 sometimes gets an exception here so ignore it */
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
	terminate_proc((DWORD)P_CP->ntpid, sig);
}

#undef signal
void (*common_signal (int sig, void(*func)(int sig),int mode)) (int sig)
{
	void (*oldsig) (int);
	if(SIGBAD(sig) || sig == SIGKILL || sig == SIGSTOP)
	{
		errno = EINVAL;
		return(SIG_ERR);
	}
	sig = sigfix(sig);
	sigmasks[sig] = 0;
	restartset &= ~SIGBIT(sig);
	reset &= ~SIGBIT(sig);
	noblock &= ~SIGBIT(sig);
	if (!SigIgnore(P_CP, sig))
		oldsig = sigfun[sig];
	else
		oldsig = SIG_IGN;
	sigsetignore (P_CP,sig, func==SIG_IGN);
	if (func != SIG_DFL)
	{
		sigfun[sig] = func;
		if(func==SIG_IGN)
			sigsetgotten(P_CP,sig, 0);
		else
		{
			if(mode==0)
			{
				reset |= SIGBIT(sig);
				noblock |= SIGBIT(sig);
			}
			else
				restartset |= SIGBIT(sig);
		}
	}
	else
		setdflsig (sig);
	if(oldsig==sigterm_proc || oldsig==suspend_proc || oldsig==sig_abort || oldsig==resume_proc || oldsig==ignore_proc)
		oldsig = SIG_DFL;
	return(oldsig);
}

void (*signal (int sig, void (*func) (int sig))) (int sig)
{
	return(common_signal(sig,func,0));
}

void (* _ast_signal (int sig, void (*func) (int sig))) (int sig)
{
	return(common_signal(sig,func,0));
}

static void setdflsig (int sig)
{
	static int keyboard;
	sigsetignore (P_CP,sig, 0);
	sigfun[sig] =  0;
	switch (sig)
	{
	    case SIGINT: case SIGQUIT:
		if(!keyboard)
		{
			//This is to give system level shutdown range
			//to init process.This is to handle CTRL_LOGOFF_EVENT
			if (P_CP->pid == 1)
			{
				if(!SetProcessShutdownParameters(0x000,SHUTDOWN_NORETRY))
					logerr(LOG_PROC+3, "SetProcessShutdownParameters");
			}

			if(!SetConsoleCtrlHandler(sig_keyboard, TRUE))
				logerr(0, "SetConsoleCtrlHandler");
			keyboard = 1;
		}
		sigfun[sig] =  sigterm_proc;
		break;
	    case SIGHUP: case SIGKILL: case SIGPIPE: case SIGALRM:
		case SIGTERM: case SIGPROF: case SIGVTALRM: case SIGUSR1:
		case SIGUSR2:
		sigfun[sig] =  sigterm_proc;
		break;
	    case SIGILL: case SIGTRAP: case SIGABRT: case SIGFPE:
		case SIGEMT: case SIGBUS: case SIGSEGV: case SIGSYS:
		case SIGXCPU: case SIGXFSZ:
		sigfun[sig] = (void (*) (int)) sig_abort;
		break;
	    case SIGWINCH: case SIGURG:
	    case SIGPOLL:
		sigsetgotten (P_CP,sig, 0);
		sigsetignore (P_CP,sig, 1);
		break;
	    case SIGCHLD:
		sigsetgotten (P_CP,sig, 0);
		sigfun[sig] = (void (*) (int)) ignore_proc;
		break;
	    case SIGCONT:
		sigfun[sig] =  resume_proc;
		break;
	    case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
		sigfun[sig] =  suspend_proc;
		break;
	}
}
static int processasig(int i)
{
	long sigbit;
	if ((!SigIgnore(P_CP,i))&&(!sigmasked(P_CP,i)))
	{
		sigset_t old = P_CP->siginfo.sigmask;
		sigsetgotten (P_CP,i, 0);
		if (sigfun[i]==ignore_proc)
			return(1);
		sigbit = SIGBIT(i);
		P_CP->siginfo.sigmask |= sigmasks[i];
		P_CP->siginfo.sigmask |= (~noblock&sigbit);
		if(sigfun[i])
		{
			int saverr = errno;
			void(*fn)(int) = sigfun[i];
			if(reset&sigbit)
				setdflsig(i);
			(*fn)(sigfix(i));
			errno = saverr;
		}
		else
			logmsg(LOG_PROC+3, "processig called with no handler for signal %d", i);
		P_CP->siginfo.sigmask = old;
		return((restartset&SIGBIT(i))>0);
	}
	return(1);
}

int processsig (void)
{
	register int i, j=0;
	int r=0,canrestart=1;
	/* don't process signals during exec or not in the main thread */
	if(proc_exec_exit)
		return(1);
	if(GetCurrentThreadId() != main_thread)
		return(1);
	if (P_CP->state == PROCSTATE_STOPPED)
	{
		if (siggotten(P_CP, SIGKILL))
		{
			processasig(SIGKILL);
			canrestart = 0;
		}
		if (!siggotten(P_CP, SIGCONT))
			return 0;
	}

	for (i = 1; i < NSIG; i++)
	{
		if (siggotten(P_CP, i))
		{

			canrestart &= processasig(i);
			if((i == SIGCONT)&&(P_CP->state == PROCSTATE_STOPPED))
				resume_proc(SIGCONT);
		}
	}
	return(canrestart);
}

int sigaddset(sigset_t *mask, int signo)
{
	if ( SIGBAD(signo))
	{
		errno = EINVAL;
		return(-1);
	}
	signo = sigfix(signo);
	*mask |= SIGBIT(signo);
	return(0);
}

int sigdelset(sigset_t *mask, int signo)
{
	if ( SIGBAD(signo))
	{
		errno = EINVAL;
		return(-1);
	}
	signo = sigfix(signo);
	*mask &= ~SIGBIT(signo);
	return(0);
}

int sigemptyset (sigset_t *mask)
{
	return(*mask=0);
}

int sigfillset (sigset_t *mask)
{
	*mask = ~0U;
	return(0);
}

int sigismember (const sigset_t *mask, int signo)
{
	if ( SIGBAD(signo))
	{
		errno = EINVAL;
		return(-1);
	}
	signo = sigfix(signo);
	return((*mask & SIGBIT(signo))!=0);
}
int sigsetmask (int mask)
{
	sigset_t old = P_CP->siginfo.sigmask, newset;
	sigdefer(1);
	sigfixmask(&mask);
	newset = (mask) & ~(SIGBIT(SIGKILL)|SIGBIT(SIGSTOP));
	P_CP->siginfo.sigmask = newset;
	sigcheck(0);
	sigfixmask(&old);
	return((int)old);
}

extern int sigblock (int mask)
{
	sigset_t old = P_CP->siginfo.sigmask, newset;
	sigdefer(1);
	sigfixmask(&mask);
	newset = (mask) & ~(SIGBIT(SIGKILL)|SIGBIT(SIGSTOP));
	P_CP->siginfo.sigmask |= newset;
	sigcheck(0);
	sigfixmask(&old);
	return((int)old);
}

static int setsigmask(Psig_t* sp, int how, const sigset_t *cset, sigset_t *oldset)
{
	sigset_t old = sp->sigmask, newset;
	sigdefer(1);
	if(cset)
	{
		sigset_t set = *cset;
		sigfixmask(&set);
		newset = (set) & ~(SIGBIT(SIGKILL)|SIGBIT(SIGSTOP));
		switch(how)
		{
		    case SIG_BLOCK:
			sp->sigmask |=  newset;
			break;
		    case SIG_UNBLOCK:
			sp->sigmask &= ~newset;
			break;
		    case SIG_SETMASK:
			sp->sigmask = newset;
			break;
		    default:
			errno = EINVAL;
			sigcheck(0);
			return(-1);
		}
	}
	if(oldset)
	{
		*oldset = old;
		sigfixmask(oldset);
	}
	sigcheck(0);
	return(0);
}

int sigprocmask (int how, const sigset_t *cset, sigset_t *oldset)
{
	return(setsigmask(&P_CP->siginfo,how,cset,oldset));
}

int sigpending (sigset_t *set)
{
	if(!set)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadWritePtr(set,sizeof(sigset_t)))
	{
		errno = EFAULT;
		return(-1);
	}
	sigfixmask(set);
	*set = (P_CP->siginfo.siggot & P_CP->siginfo.sigmask);
	return(0);
}

int sigaction (int signo,const struct sigaction *act, struct sigaction *oldact)
{
	void (*oldsig) (int);
	struct sigaction tmp_oldact={0};

	if ( SIGBAD(signo))
	{
		errno = EINVAL;
		return(-1);
	}
	sigdefer(1);
	signo = sigfix(signo);
	if(oldact)
	{
		if (!SigIgnore(P_CP, signo))
			oldsig = sigfun[signo];
		else
			oldsig = SIG_IGN;

		if((oldsig==sigterm_proc) || (oldsig==suspend_proc)
			|| (oldsig==sig_abort) || (oldsig==resume_proc)  || oldsig==ignore_proc )
			oldsig = SIG_DFL;
		tmp_oldact.sa_handler = oldsig;
		tmp_oldact.sa_mask = sigmasks[signo];
		tmp_oldact.sa_flags = 0;
		if (( signo == SIGCHLD ) && ( P_CP->siginfo.sp_flags == FALSE))
			tmp_oldact.sa_flags = SA_NOCLDSTOP;
		if (restartset&SIGBIT(signo))
			tmp_oldact.sa_flags |= SA_RESTART;
		if (reset&SIGBIT(signo))
			tmp_oldact.sa_flags |= SA_RESETHAND;
	}
	if(act)
	{
		if (signo == SIGKILL || signo == SIGSTOP)
		{
			errno = EINVAL;
			return(-1);
		}
		if (signo == SIGCHLD)
			P_CP->siginfo.sp_flags=(act->sa_flags&SA_NOCLDSTOP) == 0;
		if ( act->sa_handler == SIG_IGN )
		{
			sigsetignore (P_CP,signo, 1);
			sigsetgotten(P_CP,signo, 0);
		}
		else
		{
			sigsetignore (P_CP,signo, 0);
			if (act->sa_handler != SIG_DFL )
				sigfun[signo] = act->sa_handler;
			else
				setdflsig(signo);
		}
		sigmasks[signo] = (act->sa_mask & ~(SIGBIT(SIGKILL)|SIGBIT(SIGSTOP)));
		if (act->sa_flags&SA_RESTART)
			restartset |= SIGBIT(signo);
		else
			restartset &= ~SIGBIT(signo);
		if (act->sa_flags&SA_RESETHAND)
			reset |= SIGBIT(signo);
		else
			reset &= ~SIGBIT(signo);
		noblock &= ~SIGBIT(signo);
	}
	if(oldact)
	    *oldact = tmp_oldact;

	sigcheck(0);
	return(0);
}

int sigtimedwait(const sigset_t *set, siginfo_t *info,const struct timespec *t)
{
	unsigned long s,n;
	int rval = 0;
	DWORD milli;
	if(t)
	{
		if(t->tv_nsec<0 || t->tv_nsec > 100000000)
		{
			errno = EINVAL;
			return(-1);
		}
		if(t->tv_sec==0 && t->tv_nsec==0)
			return(0);
		milli = (DWORD)(1000*t->tv_sec + (t->tv_nsec+500000)/100000);
	}
	else
		milli = INFINITE;
	begin_critical(CR_SIG);
	while(1)
	{
		if(s=(P_CP->siginfo.siggot&(*set)))
		{
			for(n=1; n <= 32 ; n++)
			{
				if(s && SIGBIT(n))
				{
					info->si_signo = n;
					info->si_code = SI_USER;
					P_CP->siginfo.siggot &= ~SIGBIT(n);
					goto done;
				}
			}
		}
		WaitForSingleObject(state.sevent,milli);
	}
done:
	end_critical(CR_SIG);
	return(0);
}

int sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	return(sigtimedwait(set, info, NULL));
}

int sigwait(const sigset_t *set, int *sig)
{
	siginfo_t info;
	int r;
	r = sigtimedwait(set, &info, NULL);
	*sig = info.si_signo;
	return(r);
}

int sigsuspend (const sigset_t *csigmask)
{
	sigset_t old=P_CP->siginfo.sigmask;
	sigset_t sigmask = *csigmask;
	sigdefer(1);
	sigfixmask(&sigmask);
	P_CP->siginfo.sigmask=(sigmask & ~(SIGBIT(SIGKILL)|SIGBIT(SIGSTOP)));
	if(!(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign))
	{
		P_CP->state = PROCSTATE_SIGWAIT;
		WaitForSingleObject(P_CP->sigevent,INFINITE);
		P_CP->state = PROCSTATE_RUNNING;
	}
	sigcheck(0);
	P_CP->siginfo.sigmask=old;
	sigcheck(0);
	errno = EINTR;
	return(-1);
}

static void suspend_proc(int sig)
{
	int i=0;
	BOOL  sa_flags ;
	register Pproc_t *proc;
	P_CP->state = PROCSTATE_STOPPED;
	P_CP->exitcode = 0177|(sig<<8);
	logmsg(LOG_PROC+3, "suspend_proc exitcode 0x%x", P_CP->exitcode);
	P_CP->notify = 1;
	sa_flags = TRUE;
	if ( proc = proc_getlocked(P_CP->ppid, 0))
	{
		sa_flags = proc->siginfo.sp_flags && !SigIgnore(proc, SIGCHLD);
		proc_release(proc);
	}
	if((P_CP->pid != P_CP->ppid) && (P_CP->ppid !=1) && sa_flags)
		kill1(P_CP->ppid, SIGCHLD);
	i = SuspendThread(P_CP->thread);
	if ( i == (DWORD)-1 )
		logerr(0, "SuspendThread tid=%04x for pid=%d", P_CP->thread, P_CP->pid);
}

static void resume_proc(int sig)
{
	if(P_CP->state == PROCSTATE_STOPPED)
	{
		P_CP->state = PROCSTATE_RUNNING;
		P_CP->exitcode =0;
		ResumeThread(P_CP->thread);
	}
	P_CP->notify = 0;
}

static void ignore_proc(int sig)
{
	return;
}

int sighold( int sig )
{
	sig = sigfix(sig);
	if ( SIGBAD(sig))
	{
		errno = EINVAL;
		return(-1);
	}
	if (( sig==SIGKILL ) || ( sig==SIGSTOP ))
	{
		return(0);
	}

	sigdefer(1);
	P_CP->siginfo.sigmask |= SIGBIT(sig);
	sigcheck(0);
	return(0);
}

int sigignore( int sig )
{
	sig = sigfix(sig);
	if(SIGBAD(sig) || sig==SIGKILL || sig==SIGSTOP)
	{
		errno = EINVAL;
		return(-1);
	}
	sigsetignore (P_CP,sig,1);
	return(0);
}

int siginterrupt( int sig, int flag )
{
	errno = ENOSYS;
	return(-1);
}

int sigpause( int sig )
{
	unsigned long omask=P_CP->siginfo.sigmask;
	sig = sigfix(sig);
	sigdefer(1);
	P_CP->siginfo.sigmask &= ~SIGBIT(sig);
	pause();
	P_CP->siginfo.sigmask = omask ;
	return(-1);
}

int sigrelse( int sig )
{
	sig = sigfix(sig);
	if ( SIGBAD(sig))
	{
		errno = EINVAL;
		return(-1);
	}

	if (( sig==SIGKILL ) || ( sig==SIGSTOP ))
		return(0);

	sigdefer(1);
	P_CP->siginfo.sigmask &= ~SIGBIT(sig);
	sigcheck(0);
	return(0);
}

extern void (*sigset(int sig,void (*func)(int sig )))(int sig)
{
	return(common_signal(sig,func,1));
}

int raise( int sig )
{
	sig = sigfix(sig);
	return(kill(P_CP->pid, sig));
}

#define JBI(p)		(*(int*)&(p))

_JBTYPE *_ast_sigsetjmp(sigjmp_buf env, int save)
{
	if(JBI(env[_JBLEN])=save)
		JBI(env[_JBLEN+1]) = sigsetmask(0);
	return(env);
}

void siglongjmp(sigjmp_buf env, int val)
{
	if(JBI(env[_JBLEN]))
		sigsetmask(JBI(env[_JBLEN+1]));
	longjmp(env,val);
}

static jmp_buf jmpbuf;
static void dumpenv(void *env, char *name)
{
        HANDLE hp;
        DWORD size;
        if(hp = createfile(name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL))
	{
		WriteFile(hp,env, sizeof(jmp_buf),&size,NULL);
		closehandle(hp,HT_FILE);
	}
}

struct Resume
{
	HANDLE	thread;
	CONTEXT context;
	int blocked;
};
static struct Resume rinfo;

static DWORD WINAPI resume_execution(void* arg)
{
	struct Resume *rp = (struct Resume*)arg;
	rp->context.ContextFlags = UWIN_CONTEXT_TRACE;
	while(rp->blocked)
		Sleep(1);
	SuspendThread(rp->thread);
	if(!SetThreadContext(rp->thread,&rp->context))
		logerr(0, "SetThreadContext tid=%04x", rp->thread);
	ResumeThread(rp->thread);
	ExitThread(0);
	return(0);
}

static void process_signal(void)
{
	DWORD tid;
	struct Resume resume;
	resume = rinfo;
	rinfo.blocked = 0;
	processsig();
	if(!CreateThread(NULL,8192,resume_execution,(void*)&resume,0,&tid))
		logerr(0, "CreateThread for resume");
	/* just loop until resume execution takes over */
	while(1)
	{
		resume.blocked = 0;
		Sleep(1000);
	}

}

void breakit(void)
{
#ifdef DEBUG
	/* don't to it until after initialization */
	if (saved_env)
	{
		logmsg(0, "break %(stack-thread)s", 0);
		DebugBreak();
	}
#endif
}

#define MAXDELAY	10
#define POLL_INTERVAL	100

/*
 * This function polls to see if any  pipes that are being selected on
 * are ready.  It returns the number of pipes currently being polled
 */
static DWORD pipecheck(DWORD milli)
{
	static int ocount;
	static DWORD tmout;
	register int i,j,count=0;
	DWORD n;
	Pfd_t *fdp;
	for(i=0; i < P_CP->maxfds; i++)
	{
		if((j=fdslot(P_CP,i))>=0 && (fdp= &Pfdtab[j]) && (fdp->oflag&O_TEMPORARY) && !(fdp->oflag&O_WRONLY))
		{
			if(fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE ||
			   fdp->type==FDTYPE_NPIPE || fdp->type==FDTYPE_FIFO ||
			   fdp->type==FDTYPE_EFIFO)
			{
				SetLastError(0);
				if((PeekNamedPipe(Phandle(i),NULL,0,NULL,&n,NULL) && n>0) || GetLastError()==ERROR_BROKEN_PIPE)
				{
				    if (fdp->socknetevents & FD_CEVT)
				    {
					char name[UWIN_RESOURCE_MAX];
					HANDLE event;
					fdp->oflag &= ~O_TEMPORARY;
					UWIN_EVENT_PIPE(name, j);
					if((event = OpenEvent(EVENT_MODIFY_STATE,FALSE,name)))
					{
						if(!SetEvent(event))
							logerr(0, "SetEvent %s failed", name);
						closehandle(event,HT_EVENT);
					}
					else
						logerr(0, "OpenEvent %s failed evt=0x%x", name, fdp->socknetevents);

				    }
				}
				else
					count++;
			}
		}
	}
	if(count)
	{
		if(count>ocount)
			tmout = 1;
		else if((tmout*=2) > POLL_INTERVAL)
			tmout = POLL_INTERVAL;
		milli = tmout;
	}
	ocount = count;
	return(milli);
}

static int sigchange;
static int sockhandles(HANDLE *objects,int index)
{
	register int i, n=2;
	Pfd_t *fdp;
	HANDLE hp;
	sigchange = 0;
	for(i=0; i < P_CP->maxfds; i++)
	{
		if(fdslot(P_CP,i)>=0)
		{
			fdp = getfdp(P_CP,i);
			if(fdp->type==FDTYPE_SOCKET && fdp->sigio)
			{
				if((hp=Fs_select(i,2,0)) && hp!=INVALID_HANDLE_VALUE)
					objects[n++] = hp;
				else if(!(fdp->socknetevents&FD_CLOSE))
					objects[n++] = Xhandle(i);
			}
		}
	}
	return(n);
}

/*
 * cause the sig thread to recompute the handles to wait on
 */
void signotify(void)
{
	sigchange = 1;
	SetEvent(state.sevent);
}

static DWORD WINAPI keep_alive(void* arg)
{
	DWORD		status;
	HANDLE		handle[2];
	char		name[2][UWIN_RESOURCE_MAX];

	UWIN_MUTEX_INIT_RUNNING(name[0], Share->init_ntpid);
	if (!(handle[0] = OpenMutex(SYNCHRONIZE, FALSE, name[0])))
		logerr(0, "init %s open failed", name[0]);
	UWIN_EVENT_INIT_RESTART(name[1]);
	if (!(handle[1] = OpenEvent(EVENT_MODIFY_STATE|SYNCHRONIZE, FALSE, name[1])))
		logerr(0, "init %s open failed", name[1]);
	for (;;)
		switch (status = WaitForMultipleObjects(elementsof(handle), handle, FALSE, 10000))
		{
		case WAIT_TIMEOUT:
			SetEvent(state.sevent);
			state.beat++;
			break;
		case WAIT_OBJECT_0+0:
			if (Share->init_state == UWIN_INIT_SHUTDOWN)
			{
				closehandle(handle[0], HT_MUTEX);
				handle[0] = 0;
			}
			else
			{
				logmsg(0, "init %s terminated", name[0]);
				init_init();
				closehandle(handle[0], HT_MUTEX);
				UWIN_MUTEX_INIT_RUNNING(name[0], Share->init_ntpid);
				if (!(handle[0] = OpenMutex(SYNCHRONIZE, FALSE, name[0])))
					logerr(0, "init %s open failed", name[0]);
				Share->init_state = 0;
				PulseEvent(handle[1]);
			}
			break;
		case WAIT_OBJECT_0+1:
			UWIN_MUTEX_INIT_RUNNING(name[0], Share->init_ntpid);
			handle[0] = OpenMutex(SYNCHRONIZE, FALSE, name[0]);
			if ((!(state.init_handle = Share->init_handle) || GetProcessId(state.init_handle) != Share->init_ntpid) && !(state.init_handle = open_proc(Share->init_ntpid, PROCESS_DUP_HANDLE|PROCESS_VM_WRITE|PROCESS_VM_OPERATION)))
				logerr(0, "init %s restart init_handle failed", name[0]);
			else
				logmsg(0, "init %s restart", name[0]);
			break;
		default:
			if (Share->init_state != UWIN_INIT_SHUTDOWN)
				logerr(0, "init %s + %s status=%s",
					name[0],
					name[1],
					status == WAIT_IO_COMPLETION ? "WAIT_IO_COMPLETION" :
					status == WAIT_FAILED ? "WAIT_FAILED" :
					"UNKNOWN");
			Sleep(INIT_START_TIMEOUT);
			break;
		}
	return 0;
}

static DWORD WINAPI sig_thread(void* arg)
{
	CONTEXT context;
	HANDLE thread = (HANDLE)arg;
	HANDLE objects[64];
	char name[UWIN_RESOURCE_MAX];
	int nwait = 2, delay=0, i;
	DWORD	pc;
	sig_thrd_strt=1;
	ResetEvent(state.sigevent);
	objects[0] = state.sevent;

	if(!P_CP->alarmevent)
		P_CP->alarmevent = mkevent(UWIN_EVENT_ALARM(name),NULL);
	if(!(objects[1]=P_CP->alarmevent))
		nwait = 1;
	if(P_CP->sevent==0)
		logmsg(0,"objects0 sevent=%p P_CP->sevent=%p posix=0x%x ntpid=%x",state.sevent,P_CP->sevent,P_CP->posixapp,myntpid);
	while(1)
	{
		DWORD milli,vmilli,tc,ret;
		vmilli = INFINITE;
		if(P_CP->tvirtual.remain)
		{
			if(P_CP->tvirtual.remain >(tc=vtick_count()))
				vmilli =  P_CP->tvirtual.remain-tc;
			else
				vmilli = 0;
		}
		if(P_CP->treal.remain)
		{
			if(P_CP->treal.remain >(tc=GetTickCount()))
				milli =  P_CP->treal.remain-tc;
			else
				milli = 0;
		}
		else
#if 1
			milli = INFINITE;
#else
			milli = 600000;
#endif
		if(vmilli < milli)
			milli = vmilli;
		if(delay)
			milli = 1;
		if(sigchange)
			nwait = sockhandles(objects,ret);
		if(milli>0)
			milli = pipecheck(milli);
		if(milli>0)
			ret = WaitForMultipleObjects(nwait,objects,FALSE, milli);

		if(ret==WAIT_FAILED)
		{
			logerr(0, "WaitForMultipleObjects");
			if((ret=WaitForSingleObject(objects[0],1))==WAIT_FAILED)
			{
				logmsg(0, "sevent fails hp=%p", objects[0]);
				closehandle(objects[0],HT_EVENT);
				P_CP->sevent = objects[0] = mkevent(UWIN_EVENT_S(name, P_CP->ntpid),NULL);
			}
			else if(objects[1] && (ret=WaitForSingleObject(objects[1],1))==WAIT_FAILED)
			{
				logmsg(0, "alarm event fails hp=%p", objects[1]);
				closehandle(objects[1],HT_EVENT);
				P_CP->alarmevent = objects[1] = mkevent(UWIN_EVENT_ALARM(name),NULL);
			}
			else
				for(i=2; i < nwait; i++)
				{
					if((ret=WaitForSingleObject(objects[i],1))==WAIT_FAILED)
					{
						logerr(0, "WaitForSingleObject id=%o", objects[i]);
						logmsg(0, "event fails i=%d", i);
					}
					sigchange = 1;
				}
			Sleep(50);
			continue;
		}
		if(ret == WAIT_OBJECT_0)
			ResetEvent(objects[0]);

		if (ret == WAIT_OBJECT_0+1)
			ResetEvent(objects[1]);
		if(ret >= (WAIT_OBJECT_0+2) && ret < WAIT_OBJECT_0+nwait)
		{
			sigchange = 1;
			continue;
		}
		if(!delay && milli!=INFINITE)
		{
			if(P_CP->tvirtual.remain && ((tc = vtick_count()) >=P_CP->tvirtual.remain))
			{
				if(P_CP->tvirtual.interval)

					P_CP->tvirtual.remain = tc+P_CP->tvirtual.interval;
				else
					P_CP->tvirtual.remain = 0;
				sigsetgotten (P_CP,SIGVTALRM, 1);
				if(P_CP->siginfo.sigsys)
					SetEvent(state.sigevent);
				else
					PulseEvent(state.sigevent);
				PulseEvent(P_CP->sevent);
				goto dosig;
			}
			if(P_CP->treal.remain && ((tc = GetTickCount()) >=P_CP->treal.remain))
			{
				if(P_CP->treal.interval)
					P_CP->treal.remain = tc+P_CP->treal.interval;
				else
					P_CP->treal.remain = 0;
				sigsetgotten (P_CP,SIGALRM, 1);
				if(P_CP->siginfo.sigsys)
					SetEvent(state.sigevent);
				else
					PulseEvent(state.sigevent);
				PulseEvent(P_CP->sevent);
				goto dosig;
			}
		}
		if(siggotten(P_CP, SIGCONT) && P_CP->state!=PROCSTATE_STOPPED && sigfun[SIGCONT]==resume_proc)
		{
			sigsetgotten (P_CP,SIGCONT, 0);
			if(!(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign))
				continue;
		}
		if (ret == WAIT_OBJECT_0)
		{
			if(siggotten(P_CP,SIGCHLD) && (SigIgnore(P_CP,SIGCHLD) || sigfun[SIGCHLD]==ignore_proc))
			{
				sigsetgotten(P_CP, SIGCHLD,0);
				/* might be waiting for a stopped child */
				PulseEvent(waitevent);
			}
			if(!siggotten(P_CP,SIGCONT) && !(P_CP->siginfo.siggot & ~P_CP->siginfo.sigmask & ~P_CP->siginfo.sigign))
				continue;
			i = 0;
			if(siggotten(P_CP,SIGSTOP))
				i = SIGSTOP;
			 else if(siggotten(P_CP,SIGTSTP) && sigfun[SIGTSTP]==suspend_proc)
				i = SIGTSTP;
			 else if(siggotten(P_CP,SIGTTIN) && sigfun[SIGTTIN]==suspend_proc)
				i = SIGTTIN;
			 else if(siggotten(P_CP,SIGTTOU) && sigfun[SIGTTOU]==suspend_proc)
				i = SIGTTOU;
			if(i)
			{
				sigsetgotten(P_CP,SIGTSTP,0);
				sigsetgotten(P_CP,SIGSTOP,0);
				sigsetgotten(P_CP,SIGTTIN,0);
				sigsetgotten(P_CP,SIGTTOU,0);
				suspend_proc(i);
				continue;
			}
			if(P_CP->siginfo.sigsys==2)
				logmsg(0, "signal while in malloc funtions");
			if(P_CP->siginfo.sigsys)
				SetEvent(state.sigevent);
			else
				PulseEvent(state.sigevent);
		}
	dosig:
		if(siggotten(P_CP,SIGLOST))
		{
			logmsg(0, "SIGLOST state=%(state)s milli=%d", P_CP->state, milli);
			continue;
		}
		if(siggotten(P_CP,SIGCONT) || sigispending(P_CP))
		{
			if(P_CP->state==PROCSTATE_STOPPED && siggotten(P_CP, SIGCONT))
			{
				if(sigfun[SIGCONT]==resume_proc && !sigmasked(P_CP,SIGCONT))
					sigsetgotten(P_CP,SIGCONT,0);
				resume_proc(SIGCONT);
				if(!sigispending(P_CP))
				{
					ResetEvent(state.sigevent);
					continue;
				}
			}
			/* give current process a chance to clear signal */
			for(i=0; i< 20; i++)
			{
				Sleep(20);
				if(!sigispending(P_CP))
					break;
			}
			if(i<20)
				continue;
			for (i = 1; i < NSIG; i++)
			{
				if(siggotten(P_CP,i) && (sigfun[i]==sigterm_proc || sigfun[i]==sig_abort))
					(*sigfun[i])(i);
			}
			/* cause a fault */
			{
				if(rinfo.blocked)
				{
					Sleep(10);
					continue;
				}
				rinfo.blocked = 1;
				SuspendThread(thread);
				context.ContextFlags = UWIN_CONTEXT_TRACE;
				if(!(GetThreadContext(thread,&context)))
					logerr(0, "GetThreadContext tid=%04x", thread);
				pc = (DWORD)prog_pc(&context);
				if(pc > 0x70000000)
				{
					if (delay==MAXDELAY)
						logmsg(LOG_PROC+4, "count=%d interrupt handler called at %(stack-thread)s", delay, 0);
					else
						logmsg(LOG_PROC+4, "count=%d interrupt handler called at %(pc)s", delay, pc);
					if(delay++ <= MAXDELAY)
					{
						rinfo.blocked = 0;
						ResumeThread(thread);
						continue;
					}
				}
				rinfo.thread = thread;
				rinfo.context = context;
				prog_pc(&context) = (void*)process_signal;
				if(!SetThreadContext(thread,&context))
					logerr(0, "SetThreadContext tid=%04x", thread);
				ResumeThread(thread);
			}
		}
		delay = 0;
	}
	return(5);
}

static int count=0;
void _uwin_exit(int n)
{
	if(P_CP->vfork)
		_vfork_done(0,(n<<8));
#if !_HUH_2009_03_03
	else if(P_CP->posixapp&UWIN_PROC && !P_CP->child)
		P_CP->state = PROCSTATE_EXITED;
#endif
	P_CP->exitcode = n;
	logmsg(LOG_PROC+4, "_uwin_exit exitcode 0x%x posixapp 0x%x", n, P_CP->posixapp);
	SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
	count++;
}

long uwin_exception_filter(unsigned long code, EXCEPTION_POINTERS* ep)
{
	CONTEXT *cp;
	int sigval=SIGSEGV;
	DWORD pc;
	cp = ep->ContextRecord;
	pc = (DWORD)prog_pc(cp);
	logmsg(LOG_PROC+4, "uwin_exception_filter count=%d code=0x%x pc=%(pc)s exit=%p _exit=%p", count, code, pc, dllinfo._ast_exitfn, dllinfo._ast__exitfn);
	switch(code)
	{
	case EXCEPTION_ACCESS_VIOLATION:
		if(P_CP->state != PROCSTATE_TERMINATE || P_CP->exitcode != 9)
			logmsg(0, "EXCEPTION_ACCESS_VIOLATION cmd=%s state=%(state)s exitcode=%d count=%d pc=%(pc)s,%(stack-exception)s", P_CP->cmd_line, P_CP->state, P_CP->exitcode, count, pc, ep);
		if(P_CP->state==PROCSTATE_TERMINATE || P_CP->state==PROCSTATE_EXITED)
			terminate_proc(P_CP->ntpid, sigval);
		break;
	case EXCEPTION_BREAKPOINT:
		logmsg(0, "EXCEPTION_BREAKPOINT %(stack-exception)s", ep);
		sigval = SIGTRAP;
		return(EXCEPTION_CONTINUE_SEARCH);
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		logmsg(0, "EXCEPTION_DATATYPE_MISALIGNMENT %(stack-exception)s", ep);
		sigval = SIGILL;
		break;
	case EXCEPTION_SINGLE_STEP:
		logmsg(0, "EXCEPTION_SINGLE_STEP");
		sigval = SIGTRAP;
		break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		logmsg(0, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED %(stack-exception)s", ep);
		break;
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		logmsg(0, "EXCEPTION_FLT_DENORMAL_OPERAND %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		logmsg(0, "EXCEPTION_FLT_DIVIDE_BY_ZERO %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_FLT_INEXACT_RESULT:
		logmsg(0, "EXCEPTION_FLT_INEXACT_RESULT %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_FLT_INVALID_OPERATION:
		logmsg(0, "EXCEPTION_FLT_INVALID_OPERATION %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_FLT_OVERFLOW:
		logmsg(0, "EXCEPTION_FLT_OVERFLOW %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_FLT_STACK_CHECK:
		logmsg(0, "EXCEPTION_FLT_STACK_CHECK %(stack-exception)s", ep);
		break;
	case EXCEPTION_FLT_UNDERFLOW:
		logmsg(0, "EXCEPTION_FLT_UNDERFLOW %(stack-exception)s", ep);
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		logmsg(0, "EXCEPTION_INT_DIVIDE_BY_ZERO %(stack-exception)s", ep);
		sigval = SIGFPE;
		break;
	case EXCEPTION_PRIV_INSTRUCTION:
		logmsg(0, "EXCEPTION_PRIV_INSTRUCTION %(stack-exception)s", ep);
		sigval = SIGIOT;
		break;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		logmsg(0, "EXCEPTION_NONCONTINUABLE_EXCEPTION %(stack-exception)s", ep);
		break;
	default:
		logmsg(0, "EXCEPTION(0x%x) unknown %(stack-exception)s", code, ep);
		return(EXCEPTION_CONTINUE_SEARCH);
	}
	if(P_CP->siginfo.sigign & (1L << (sigval-1)))
	{
		logmsg(0, "exception but signal ignored sig=%d sigval=0x%x sigign=0x%x", sigval,P_CP->siginfo.sigsys,P_CP->siginfo.sigign);
		if(sigval==SIGSEGV)
			P_CP->siginfo.sigign &= ~P_CP->siginfo.sigign;
		return(EXCEPTION_CONTINUE_EXECUTION);
	}
	if(GetCurrentThreadId() == main_thread)
	{
		sigsetgotten(P_CP,sigval,1);
		processsig();
	}
	if(P_CP->vfork)
		_vfork_done(0,sigval);
	return(EXCEPTION_CONTINUE_SEARCH);
}


/*
 * create events, and start signal thread
 * if mode==1 also initialize signal handlers
 */
void initsig(int mode)
{
	HANDLE me,hp;
	int i,rsize,aclbuf[256];
	unsigned long sigign = P_CP->siginfo.sigign;
	SID *usid, *asid;
	SECURITY_ATTRIBUTES sattr;
	SECURITY_DESCRIPTOR sd;
	char name[UWIN_RESOURCE_MAX];

	state.beat = 0;
	count=0;
	ZeroMemory (&sattr, sizeof (sattr));
	sattr.nLength=sizeof(sattr);
	sattr.bInheritHandle = FALSE;
	sattr.lpSecurityDescriptor = 0;
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
		goto skip;
	init_sid(1);
	if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
	{
		logerr(LOG_SECURITY+1, "InitializeSecurityDescriptor");
		goto skip;
	}
	usid = sid(SID_UID);
	if(!usid)
		goto skip;
	rsize = sizeof(ACL) + (2*(sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD))) + GetLengthSid(usid);
	if(asid = sid(SID_ADMIN))
		rsize +=  GetLengthSid(asid);
	if (!InitializeAcl((ACL*)aclbuf,rsize,ACL_REVISION))
	{
		logerr(LOG_SECURITY+1, "InitializeAcl");
		goto skip;
	}
	if(!AddAccessAllowedAce((ACL*)aclbuf,ACL_REVISION,GENERIC_ALL, usid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto skip;
	}
	if(asid && !AddAccessAllowedAce((ACL*)aclbuf,ACL_REVISION,GENERIC_ALL,asid))
	{
		logerr(LOG_SECURITY+1, "AddAccessAllowedAce");
		goto skip;
	}
	if(!SetSecurityDescriptorDacl(&sd, TRUE, (ACL*)aclbuf, FALSE))
		logerr(LOG_SECURITY+1, "SetSecurityDescriptorDacl");
	else
		sattr.lpSecurityDescriptor = &sd;
skip:
	if(mode)
	{
		noblock = 0;
		reset = 0;
		restartset = SIGBIT(SIGSTOP)|SIGBIT(SIGCONT);
		for (i = 1; i < NSIG; i++)
			setdflsig (i);
	}
	P_CP->siginfo.sigign |= sigign;
	if (!(P_CP->sigevent = mkevent(UWIN_EVENT_SIG(name, P_CP->ntpid), &sattr)))
		logmsg(0, "mkevent %s failed", name);
	if (!(P_CP->sevent = state.sevent = mkevent(UWIN_EVENT_S(name, P_CP->ntpid), &sattr)))
		logmsg(0, "mkevent %s failed", name);
	ResetEvent(state.sevent);
	myntpid = GetCurrentProcessId();
	state.sigevent = P_CP->sigevent;

	/* alarm event is created with NULL DACL..This is to make the alarmevent
	 * accessible after setuid().
	 */
	if (!(P_CP->alarmevent = mkevent(UWIN_EVENT_ALARM(name),NULL)))
		logmsg(0, "mkevent %s failed", name);
	me = GetCurrentProcess();
	if(!DuplicateHandle(me,GetCurrentThread(),me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle");
	sig_thrd_strt=0;
	if(!CreateThread(NULL,0,sig_thread,(void*)hp,0,&rsize))
		logerr(0, "CreateThread sig_thread");
	if(P_CP->pid != 1 && !CreateThread(NULL,0,keep_alive,(void*)hp,0,&rsize))
		logerr(0, "CreateThread keep_alive");
	i=0;
	while(sig_thrd_strt==0 && (i++<50))
		Sleep(1);
}

void sig_saverestore(void *fun,void *masks,int restore)
{
	if(restore)
	{
		memcpy((void*)sigfun,fun,sizeof(sigfun));
		memcpy((void*)sigmasks,masks,sizeof(sigmasks));
	}
	else
	{
		memcpy(fun,(void*)sigfun,sizeof(sigfun));
		memcpy(masks,(void*)sigmasks,sizeof(sigmasks));
	}
}

static Psig_t *sigthread_data(void)
{
	register Tthread_t*	tp;
	HANDLE			ep;
	DWORD			err;

#if _PER_THREAD_SIGNALS
	static Psig_t* (*thread_self)(void);
#endif

	if(!P_CP || !P_CP->sigevent)
		return(0);
	err = GetLastError();
#if _PER_THREAD_SIGNALS
	if(!thread_self && !(thread_self=(Psig_t*(*)(void))getsymbol(MODULE_pthread,"pthread_self")))
		thread_self = sigthread_data;
	if(thread_self && thread_self!=sigthread_data)
	{
		/* avoid recursive call from malloc() */
		ep = P_CP->sigevent;
		P_CP->sigevent = 0;
		tp = (Tthread_t*)((*thread_self)());
		P_CP->sigevent = ep;

	}
#endif
	if(!(tp = (Tthread_t*)TlsGetValue(P_CP->tls)))
	{
		ep = P_CP->sigevent;
		P_CP->sigevent = 0;
		tp = (Tthread_t*)malloc(sizeof(Tthread_t));
		if(!TlsSetValue(P_CP->tls,(void*)tp))
			logerr(0, "TlsSetValue");
		P_CP->sigevent = ep;
		if(!tp)
		{
			SetLastError(err);
			return(&P_CP->siginfo);
		}
		tp->siginfo.sigsys = 0;
	}
	SetLastError(err);
	return(&tp->siginfo);
}

int pthread_sigmask (int how, const sigset_t *cset, sigset_t *oldset)
{
	register Psig_t *sp = sigthread_data();
	return(setsigmask(sp,how,cset,oldset));
}

int pthread_kill(pthread_t tp, int sig)
{
	register Psig_t *sp = (Psig_t*)tp;
	return(0);
}

int sigdefer(register int level)
{
	register int	olevel;
	register Psig_t *sp = sigthread_data();
	if(!sp)
		return(0);
	olevel=sp->sigsys;
	if(olevel > level)
		level=olevel;
	sp->sigsys=level;
	return(olevel);
}

int sigcheck(register int level)
{
	register Psig_t *sp = sigthread_data();
	register Pproc_t *pp = P_CP;
	if(!sp || !sp->sigsys)
		return(1);
	if(level <0)
		return(sp->sigsys=0);
	if(sp->sigsys = level)
		return(1);
	if(!pp->sigevent)
		return(1);
#if 0
	if(pp->siginfo.siggot & ~sp->sigmask & ~pp->siginfo.sigign)
#else
	if(pp->siginfo.siggot & ~pp->siginfo.sigmask & ~pp->siginfo.sigign)
#endif
	{
		ResetEvent(pp->sigevent);
		return(processsig());
	}
	return(1);
}

