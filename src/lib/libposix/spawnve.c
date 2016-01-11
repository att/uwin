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
#include "uwindef.h"
#include "dlls.h"
#include <setjmp.h>
#include <uwin.h>
#include "vmalloc/vmhdr.h"
#include "uwin_serve.h"
#include "mnthdr.h"
#ifdef GTL_TERM
#   include "raw.h"
#endif

#define is_slotvalid(s)	(((s) && (s)< Share->block && Blocktype[(s)&0xf]!=BLK_PROC)?1:(logmsg(0,"badslot%d s=%d",__LINE__,(s)),0))

#ifndef	CREATE_NO_WINDOW
#define CREATE_NO_WINDOW		DETACHED_PROCESS
#endif

#ifndef ERROR_ELEVATION_REQUIRED
#define ERROR_ELEVATION_REQUIRED	740
#endif

#ifndef PROCESS_VM_OPERATION
#define PROCESS_VM_OPERATION		0
#endif

#ifndef THREAD_SUSPEND_RESUME
#define THREAD_SUSPEND_RESUME		0
#endif
#ifndef THREAD_GET_CONTEXT
#define THREAD_GET_CONTEXT		0
#endif
#ifndef THREAD_SET_CONTEXT
#define THREAD_SET_CONTEXT		0
#endif

#define	IN_CLEANUP	0x1
#define	IN_EXPUNGE	0x2
#define	IN_MESSAGE	0x4

#define NEWFORK_VERSION	5

#define isadrive(c)	((1L<<(fold(c)-'a'))&GetLogicalDrives())
#define SANE_MODE	(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT)
#define	segment(x)	(((unsigned long)(x)&~0xffff))
#define newaddr(x,seg)	((char*)((seg)|((unsigned long)(x)&0xffff)))
#define isconsole_child(pp)	((pp)->console_child)
#define COPY_ENV	1
#define UWIN_NOINHERIT	0x400000
#define FD_NATIVE	(1<<6)
#ifndef PAGESIZE
#   define PAGESIZE 65536
#endif

#define logini(d,a ...)	(P_CP->pid==1?logsrc(d,__FILE__,__LINE__,a):0)

HANDLE waitevent;
static int wait_thrd_strt;
extern void *malloc_first_addr;
extern void *(*clib_malloc)(size_t);

typedef BOOL (WINAPI *CPU_t)(HANDLE,const char*,char*,
	SECURITY_ATTRIBUTES*,SECURITY_ATTRIBUTES*,BOOL,DWORD,
	VOID*,const char*, STARTUPINFOA*,PROCESS_INFORMATION*);

static void grantaccess(int);
static int process_message(int, int);

static unsigned short Lastslot;
static unsigned long oldseg,newseg;
static int eventnum, nomorechildren;
static Pproc_t *lastproc;
static HANDLE thwait;
static HANDLE child_proc;
static int fork_err;
static HANDLE parent_thread;
static CONTEXT pcontext;
static DWORD  parent_tid;
static DWORD  init_slot;
static unsigned short init_serial;
static int major_version;
static void *arg_buff, *arg_base, *arg_argv;
static struct atfork *Atfork;
static int	infork;
static HANDLE	objects[4*MAXIMUM_WAIT_OBJECTS];
static Pproc_t	*procs[4*MAXIMUM_WAIT_OBJECTS];
static DWORD	nwait;

struct vstate
{
	jmp_buf	jmpbuf;
	Pproc_t	*oproc;
	void	(*sigfun[NSIG])();
	sigset_t sigmasks[NSIG];
};

#define ARG_SLOP	8192

char	*arg_getbuff(void)
{
	if(arg_buff && IsBadCodePtr(arg_buff))
	{
		logmsg(0, "arg_getbuff buff=%p", arg_buff);
		arg_buff = 0;
	}
	if(!arg_buff)
	{
		DWORD l = Share->argmax+ARG_SLOP;
		arg_base = arg_buff = VirtualAlloc(NULL,l,MEM_COMMIT,PAGE_READWRITE);
		if(!arg_buff)
		{
			logerr(0, "VirtualAlloc");
			return(malloc(l));
		}
	}
	return(arg_buff);
}

void arg_setbuff(char *ptr)
{
	arg_buff = ptr;
}

/*
 * copies env variable ename into upath converting format
 * flags:
 *	UWIN_W2U	windows paths converted to unix paths
 * 	UWIN_U2W	unix paths converted to windows paths
 *	UWIN_WOW	32 bit view
 */
int env_convert(register const char *ename, char *upath, int size, int flags)
{
	register char *ep,*dp,*cp;
	int i,n,flag;
	Mount_t *mp;
	Path_t info;
	flag = flags & UWIN_W2U;
	if (flags & UWIN_WOW)
		info.wow = 32;
	if(!flag && (void*)ename>= arg_base && (void*)ename<arg_buff)
	{
		/* optimization - use the value that came in */
		cp = (char*)ename - sizeof(char*);
		dp = *((char**)cp);
		n = (int)strlen(dp)+1;
		if(n>size)
			return(0);
		memcpy(upath,dp,n);
		return(n);
	}
	if(cp=strchr(ename,'='))
		cp++;
	else
		return(0);
	i = (int)((const char*)cp-ename);
	if(flag)
	{
		if(*cp=='/')
			return(0);
	}
	else if(*cp!='/' && cp[1]==':' && (!cp[2]||cp[2]==';'||cp[2]=='\\') && ((*cp>='a' && *cp<='z') || (*cp>='A' && *cp<='Z')))
	{
		/* nt-style path already */
		return(0);
	}
	memcpy(upath,ename,i);
	while(1)
	{
		if(ep = strchr(cp,(flag?';':':')))
			*ep = 0;
		if(*cp)
		{
			dp = &upath[i];
			if (flag)
			{
				if (ep)
					n = (int)(ep-cp);
				else
					n = (int)strlen(cp);
				if (mp = mount_look(cp,n,1,0))
					cp = mp->path;
				n = uwin_pathmap(cp,dp,size-i,UWIN_W2U);
			}
			else
			{
				if (flags & UWIN_WOW)
					info.wow = 32;
				if ((n = uwin_pathinfo(cp, dp, size-i, flags, &info)) && (flags & UWIN_WOW) && info.view)
				{
					dp[info.view+1] = '3';
					dp[info.view+2] = '2';
				}
			}
			i += n;
		}
		if(ep==0)
			break;
		upath[i++] = flag?':':';';
		*ep = (flag?';':':');
		cp = ep+1;
	}
	return(i+1);
}

/*
 * returns 1 if environmental variable <var> is in blank separated  namelist
 */
int env_inlist(const char *var, const char *list)
{
	register int c= *var,d;
	register const char *cp=list,*sp;
	while(*cp)
	{
		while(*cp && (*cp==' ' || *cp=='\t'))
			cp++;
		if(*cp==0)
			break;
		if(*cp++ == c)
		{
			for(sp=var+1;(d= *sp) && d!='=';sp++)
			{
				if(d != *cp++)
					break;
			}
			if(d=='=' && (*cp==0 || *cp==' ' || *cp=='\t'))
				return(1);
		}
		while(*cp && *cp!=' ' && *cp!='\t')
			cp++;
	}
	return(0);
}

pid_t nt2unixpid(DWORD ntpid)
{
	static unsigned char *pid_counters;
	if(Share->pidbase)
		return(((Share->pidxor^ntpid) - Share->pidbase)>>2);
	if(Share->pid_counters)
	{
		register int n = (ntpid>>Share->pid_shift)&0x1ff;
		if(!pid_counters)
			pid_counters = (unsigned char*)block_ptr(Share->pid_counters);
		return((pid_t)((ntpid<<(7-Share->pid_shift))|(pid_counters[n])));
	}
	return((pid_t)ntpid);
}

unsigned long uwin_ntpid(pid_t pid)
{
	unsigned long ntpid;
	Pproc_t *proc;
	if(proc=proc_getlocked(pid, 0))
	{
		ntpid = proc->ntpid;
		proc_release(proc);
		return(ntpid);
	}
	if(Share->pidbase)
	{
		ntpid = (((unsigned long)pid)<<2) + Share->pidbase;
		return(ntpid^Share->pidxor);
	}
	if(Share->pid_counters)
		return((unsigned long)(((pid>>7))<<Share->pid_shift));
	return((unsigned long)pid);
}

pid_t   uwin_nt2unixpid(DWORD ntpid)
{
	Pproc_t *proc =	proc_ntgetlocked(ntpid,1);
	pid_t		pid;
	if(proc)
	{
		pid = proc->pid;
		proc_release(proc);
		return(pid);
	}
	else if(Share->pidbase)
		pid = ((Share->pidxor^ntpid) - Share->pidbase)>>2;
	else
		pid = (pid_t)((ntpid<<(7-Share->pid_shift))|127);
	if(pid==1)
		logmsg(0,"pid==1 ntpid=%x",ntpid);
	return(pid);
}

static void sync_io(void)
{
	static int (*iosync)(void*);
	/* synchronize stdio */
	if(!iosync)
		iosync = (int(*)(void*))getsymbol(MODULE_ast,"sfsync");
	if(iosync)
		(*iosync)(NULL);
}

static char *packenv(char *ebuff, int bufsize, char *const envp[], int flags)
{
	register char *ep, *dp;
	register const char *sp;
	register int esize, i;
	char buff[257];
	const char *dospath=0, *emax= &ebuff[bufsize];
	int sysroot=0, sysdrive=0;

	esize = GetCurrentDirectory(sizeof(buff),buff)+256;
	for (i=0; sp=envp[i]; i++)
	{
		if(newseg && (segment(sp)==oldseg))
			sp =  newaddr(sp,newseg);
		if(!IsBadStringPtr(sp,3))
		{
			esize += (int)strlen(sp) + 1;
			if((sp[0]=='D') && (sp[1]=='O') && (sp[2]=='S') && memcmp(sp,"DOSPATHVARS=",12)==0)
				dospath = &sp[12];
			else if((sp[0]=='S') && (sp[1]=='y') && (sp[2]=='s'))
			{
				if(memcmp(sp,"SystemDrive=",12)==0)
					sysdrive = 1;
				else if(memcmp(sp,"SystemRoot=",11)==0)
					sysroot = sp[11];
			}
		}
		else
			logmsg(0, "packenv badptr=%p", sp);
	}
	if(!sysdrive)
		esize += 14;
	if(!sysroot)
		esize += 32;
	if(esize>bufsize)
		return(0);
	if(ep = dp = ebuff)
	{
		*dp++ = '=';
		*dp++ = buff[0];
		*dp++ = ':';
		*dp++ = '=';
		sp = buff;
		while(*dp++ = *sp++);
		while(sp= *envp++)
		{
			if(newseg && (segment(sp)==oldseg))
				sp =  newaddr(sp,newseg);
			if(IsBadStringPtr(sp,3))
				continue;
			if(*sp==0)
				continue;
			/* change UNIX PATH to Win32 PATH */
			i = 0;
			if((sp[0]=='P') && (sp[1]=='A') && (sp[2]=='T') && (sp[3]=='H') && (sp[4]=='='))
			{
				/* put original PATH= as =PATH= */
				const char *cp = sp;
				*dp++ = '=';
				while(*dp++ = *cp++);
				i = env_convert(sp,dp,(int)(emax-dp),flags);
			}
			else if(dospath && env_inlist(sp,dospath))
				i = env_convert(sp,dp,(int)(emax-dp),flags);
			if(i)
			{
				dp += i;
				esize += (i-1) - (int)strlen(sp);
				if(esize>bufsize)
					return(0);
			}
			else
				while(*dp++ = *sp++);
		}
		if(!sysroot)
		{
			memcpy(dp,"SystemRoot=",11);
			dp += 11;
			i = uwin_pathmap("/win",dp,(int)(emax-dp),UWIN_U2W);
			sysroot = *dp;
			dp += i;
			*dp++ = 0;
		}
		if(!sysdrive)
		{
			memcpy(dp,"SystemDrive=",12);
			dp += 12;
			*dp++ = sysroot;
			*dp++ = ':';
			*dp++ = 0;
		}
		*dp++ = 0;
		*dp = 0;
	}
	return(dp);
}

static isaletter(int c)
{
	return((c>='a' && c<='z') || (c>='A' && c<='Z'));
}

/*
 * move double quotes to beginning of the string
 * returns the end of the string
 */
static char *requote(char *string)
{
	register char *sp = strchr(string,'"');
	register char *ep=sp+1;
	register int c;
	if(!sp)
		return(string);
	while(sp > string)
	{
		c = *--sp;
		sp[1] = c;
	}
	*sp = '"';
	while(*ep && *ep!='"')
		ep++;
	if(*ep==0)
		return(string);
	sp = ep;
	while(c = *ep++)
	{
		if(c!='"')
			*sp++ = c;
	}
	*sp++ = '"';
	*sp = 0;
	return(sp);
}

/*
 * create the command line in cmdbuff, whose length is sysconf(_SC_ARG_MAX)
 */
int getcmdline(char *cmdbuff,char *const argv[],char *script,const char *shell,const char *argv0,const char *argv1,int dos)
{
	register char *dp;
	register const char *sp;
	register int asize=0, i, c,shlen;
	char *begin;
	char buf[PATH_MAX];
	int quoted;

	if(!argv)
	{
		*cmdbuff = 0;
		return(1);
	}
	for (i=0; sp=argv[i]; i++)
	{
		while(c= *sp++)
		{
			if(c==' ' || c == '\t')
				asize += 2;
			else if(c=='"')
				asize += 1;
			else if(c=='\\' && *sp=='"')
				asize += 1;
		}
		asize += (int)(sp-argv[i]);
	}
	if(argv0)
		asize += (int)strlen(argv0);
	if(argv1)
		asize += (int)strlen(argv1);
	if(script)
	{
		shlen = (int)strlen(shell);
		asize += shlen+1;
		asize += (int)strlen(script)+1;
	}
	if(asize > (int)Share->argmax) /* if command line size > ARG_MAX */
		return(0);
	if(dp = cmdbuff)
	{
		if (script)
		{
			memcpy (dp, shell, shlen);
			dp += shlen;
			*dp++ = ' ';
			if((sp=strchr(shell,' ')) || (sp=strchr(shell,'\t')))
				*(char*)sp = 0;
		}
		for (i = 0; sp=argv[i]; i++)
		{
			if(*sp==0)
			{
				*dp++ = '"';
				*dp++ = '"';
				*dp++ = ' ';
				continue;
			}
			begin = dp;
			if(i==0)
			{
				if(argv0)
					sp = argv0;
				if(argv1)
				{
					i--;
					sp = argv1;
					argv1 = 0;
				}
				else if(!script)
				{
					/* quote argv[0] if it contains space or tab */
					if(c=(strchr(argv[0],' ') || strchr(argv[0],'\t')))
						*dp++ = '"';
					if(sp[0]=='/' && sp[2]=='/' && isaletter(sp[1]))
					{
						*dp++ = sp[1];
						*dp++ = ':';
						sp +=2;
					}
					while(*dp = *sp++)
						dp++;
					if(c)
						*dp++ = '"';
					*dp++ = ' ';
					continue;
				}
				else if(*script==0 && sp[0]=='/' && sp[2]=='/' && isadrive(sp[1]))
				{
					/* use win32 name */
					*dp++ = sp[1];
					*dp++ = ':';
					sp += 2;
				}
				else if(dos && uwin_pathmap(argv[0],buf,sizeof(buf),UWIN_U2W)>0)
					sp = buf;
			}
			quoted = 0;
			while(c= *sp++)
			{
				if(c==' ' || c == '\t')
				{
					*dp++ = '"';
					if(quoted==0)
						quoted=1;
					while(1)
					{
						*dp++ = c;
						if((c= *sp)!=' ' && c!='\t')
							break;
						sp++;
					}
					*dp++ = '"';
					continue;
				}
				if(c=='"')
				{
					quoted = -1;
					*dp++ = '\\';
				}
				else if(c=='\\')
				{
					int count=1;
					while(*sp=='\\')
					{
						*sp++;
						*dp++ = '\\';
						count++;
					}
					if(*sp=='"' || *sp==' ' || *sp=='\t')
					{
						quoted = -1;
						while(count-->0)
							*dp++ = '\\';
					}
				}
				else if(c=='/' && i==0 && !script) /* dos format */
					c = '\\';
				*dp++ = c;
			}
			if(quoted>0)
			{
				*dp = 0;
				dp = requote(begin);
			}
			if(script && *script && i==0)
			{
				*dp++ = '.';
				while(*dp = *script++)
					dp++;
			}
			*dp++ = ' ';
		}
		if(dp>cmdbuff)
			*--dp = 0;
	}
	return (int)(dp - cmdbuff) + 1;
}

static void set_exec_event(Pproc_t *proc, int who)
{
	char ename[UWIN_RESOURCE_MAX];
	HANDLE event;
	proc->inexec &= ~PROC_HAS_PARENT;
	if(!(proc->posixapp&UWIN_PROC))
		return;
	UWIN_EVENT_EXEC(ename, proc->pid);
	if(event=OpenEvent(EVENT_MODIFY_STATE,FALSE,ename))
	{
		if(!SetEvent(event))
			logerr(0, "SetEvent %s failed", ename);
		closehandle(event,HT_EVENT);
	}
	else if(proc->phandle)
		logerr(0, "%s %s failed state=%s name=%s", who>0 ? "wait_thread_Event" : "dup2_init_Event pid=%d slot=%d ph=%p", ename, proc->pid, proc_slot(proc), proc->phandle, proc->state, proc->prog_name);
}

/*
 * need to use inherited handle for vista since OpenProcess() can fail
 */
HANDLE open_proc(DWORD ntpid, DWORD access)
{
	char		pname[64];
	HANDLE		hp;
	HANDLE		ih;
	DWORD		n;
	DWORD		r = 0;
	unsigned short	slot;

	if (hp = OpenProcess(PROCESS_DUP_HANDLE|access, FALSE, ntpid))
		return hp;
	if (ntpid != Share->init_ntpid)
	{
		if(P_CP->uid != geteuid())
		{
			RevertToSelf();
			hp=OpenProcess(PROCESS_DUP_HANDLE|access,FALSE,ntpid);
			impersonate_user(P_CP->etoken);
		}
		return hp;
	}
	if (!access && (ih = Share->init_handle))
	{
		HANDLE	me = GetCurrentProcess();

		if (DuplicateHandle(me, ih, me, &hp, 0, FALSE, DUPLICATE_SAME_ACCESS))
			return hp;
	}
	n = 0;
	for (;;)
	{
		UWIN_PIPE_INIT(pname, Share->init_ntpid);
		if (hp = createfile(pname, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
		{
			HANDLE	hpinit=0, hpsave=P_CP->phandle;
			DWORD ntpidsave = P_CP->ntpid;
			P_CP->phandle = 0;
			P_CP->ntpid = GetCurrentProcessId();
			slot=proc_slot(P_CP);
			if(WriteFile(hp, &slot, sizeof(slot), &n, NULL))
			{
				for(n=0; !P_CP->phandle || n<30; n++)
					Sleep(40);
				hpinit = P_CP->phandle;
			}
			else
				logerr(0, "WriteFile %s failed", pname);
			closehandle(hp,HT_FILE);
			P_CP->phandle = hpsave;
			P_CP->ntpid = ntpidsave;
			return hpinit;
		}
		if (++n > 7)
			break;
		SleepEx(n*200, FALSE);
	}
	logerr(0, "CreateFile %s failed", pname);
	return 0;
}

/*
 * duplicate the process handle to the process given by <slot>
 */

int dup_to_init(Pproc_t *proc, int slot)
{
	HANDLE ph,origph,toproc=0,me=GetCurrentProcess();
	int dupflag = DUPLICATE_SAME_ACCESS, init;
	if(proc && proc->pid<=3)
		return(0);
	/* vista might not be able to OpenProcess() so try inherited handle */
	if (slot != Share->init_slot && (toproc = open_proc(proc_ptr(slot)->ntpid,PROCESS_VM_WRITE|PROCESS_VM_OPERATION)))
		init = 0;
	else if (!(toproc = init_handle()))
		return 0;
	else
		init = 1;
	/* note that proc->ph is owned by proc parent */
	/* otherwise save a handle to this process in init process */
	if(proc)
	{
		ph = proc->ph;
		if (!ph)
			logmsg(0, "dup_to_init proc ntpid=%x ph=%p", proc->ntpid, ph);
	}
	else if(!(ph=OpenProcess(USER_PROC_ARGS,FALSE,P_CP->ntpid)))
		return(0);
	Lastslot = proc?proc_slot(proc):proc_slot(P_CP);
	origph = ph;
	if(DuplicateHandle(me,ph,toproc,&ph,0,FALSE,dupflag))
	{
		if(!proc)
			P_CP->phandle = ph;
		else
			proc->ph = ph;
		if(!init)
			closehandle(toproc,HT_PROC);
		else if(proc && (origph=proc->origph))
		{
			DWORD code;
			if(GetExitCodeProcess(origph,&code))
			{
				if(!DuplicateHandle(me,origph,toproc,&proc->origph,0,FALSE,DUPLICATE_SAME_ACCESS))
				{
					logerr(0,"DuplicateOrigHandle origph=%p",origph);
					proc->origph = 0;
				}
				closehandle(origph,HT_PROC);
			}
			else
			{
				logerr(0,"getexitcode origphx origph=%p me=%x toproc=%x slot=%d pid=%x-%d state=%(state)s name=%s",origph,me,toproc,proc_slot(proc),proc->ntpid,proc->pid,proc->state,proc->prog_name);
				proc->origph = 0;
			}
		}
		return(1);
	}
	logerr(0, "dup_to_init DuplicateHandle handle=%p:%p toproc=%p init=%d toproc=%d", origph, ph, toproc,init,proc_ptr(slot)->ntpid);
	if(!init)
		closehandle(toproc,HT_PROC);
	else if(WaitForSingleObject(toproc,0)==0)
		Sleep(INIT_START_TIMEOUT);
	if(dupflag&DUPLICATE_CLOSE_SOURCE)
		closehandle(ph,HT_PROC);
	return(0);
}

/*
 * Get back and close process handle from init process
 */
int dup_from_init(Pproc_t *proc)
{
	HANDLE ih,hp,me=GetCurrentProcess();
	if(!(ih = init_handle()))
		return(-1);
	if(DuplicateHandle(ih,proc->phandle,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
	{
		closehandle(hp,HT_PROC);
		return(1);
	}
	//logerr(0, "dup_from_init pid=%x-%d slot=%d: handle=%p ref=%d", proc->ntpid, proc->pid, proc_slot(proc), proc->phandle, proc->inuse);
	logerr(0, "dup_from_init slot=%d ph=%04x phand=%04x pid=%x-%d pgid=%d inuse=%d procid=%d parent=%d gent=%d posix=0x%x state=%(state)s name=%s cmd=%s",proc_slot(proc),proc->ph,proc->phandle,proc->ntpid,proc->pid,proc->pgid,proc->inuse,proc->ppid,proc->parent,proc->group,proc->posixapp,proc->state,proc->prog_name,proc->cmd_line);
	return(0);
}

static int notify_init(void)
{
	HANDLE event;
	DWORD err = GetLastError();
	char ename[UWIN_RESOURCE_MAX];
	UWIN_EVENT_INIT(ename);
	if(event=OpenEvent(EVENT_MODIFY_STATE,FALSE,ename))
	{
		if(!SetEvent(event))
			logerr(0, "SetEvent");
		closehandle(event,HT_EVENT);
		return(1);
	}
	logerr(0, "OpenEvent %s failed", ename);
	SetLastError(err);
	return(0);
}

static int wait_acknowledge(Pproc_t *pp, DWORD threadid, int line)
{
	register unsigned int i;
	for(i=1; (pp->inexec&PROC_MESSAGE) && i < 5000; i<<=1)
		Sleep(i);
	if(i>32)
	{
		if(threadid)
		{
			HANDLE th;
			DWORD code=0;
			if ((th = OpenThread(THREAD_QUERY_INFORMATION,FALSE,threadid)) == INVALID_HANDLE_VALUE)
				th = 0;
			if(!th || !GetExitCodeThread(th,&code) || code!=STILL_ACTIVE)
			{
				logerr(0,"wait_ack: line=%d thid=%x th=%o slot=%d pid=%x-%d ppid=%d state=%(state)s inexec=0x%x code=%d\n",line,threadid,th,proc_slot(pp),pp->ntpid,pp->pid,pp->ppid,pp->state,pp->inexec,code);

				pp->inexec &= ~PROC_MESSAGE;
				if(th)
					closehandle(th,HT_THREAD);
				return(0);
			}
			if(th)
				closehandle(th, HT_THREAD);
		}
		if(i>Share->maxtime)
		{
			if(i>1)
				logmsg(1,"maxtime: %d slot=%d pid=%x-%d ppid=%d line=%d",i,proc_slot(pp),pp->ntpid,pp->pid,pp->ppid,line);
			Share->maxtime = i;
		}
	}
	if(pp->inexec&PROC_MESSAGE)
		logmsg(0,"waitmsg: line=%d slot=%d pid=%x-%d ppid=%d state=%(state)s threadid=%x inexec=0x%x",line,proc_slot(pp),pp->ntpid,pp->pid,pp->ppid,pp->state,threadid,pp->inexec);
	return(1);
}

/*
 *  send message <msgid> to the init proc with parameter <slot>
 */
int send2slot(int slot,int msgid, int message)
{
	Pproc_t*	pp = proc_ptr(slot);
	DWORD		i,threadid=0;

	if(proc_ptr(message)->inuse<=0)
	{
		Pproc_t *proc = proc_ptr(message);
		logmsg(0,"send2slot slot=%d pid=%x-%d inuse=%d state=%(state)s name=%s",message,proc->ntpid,proc->pid,proc->inuse,proc->state,proc->prog_name);
	}
	if(slot==Share->init_slot && P_CP->pid==1)
	{
		if(i = process_message(msgid,message))
		{
			procs[--i] = proc_ptr(message);
			objects[i] = procs[i]->ph;
			if(i>=nwait)
				nwait++;
		}
		return(1);
	}
	if(slot==Share->init_slot && Share->init_pipe)
	{
		DWORD retry=0,r,size;
		HANDLE ih,hp,me=GetCurrentProcess();
		message |= msgid<<16;
		if(msgid==5)
			msgid = 0;
	again:
		if((ih = init_handle()) && DuplicateHandle(ih,Share->init_pipe,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
		{
			r=WriteFile(hp,&message,sizeof(DWORD),&size,NULL);
			if(r && size==sizeof(DWORD))
			{
				notify_init();
				goto poll;
			}
			else
			{
				if(r)
					logmsg(0, "writefile wrong size=%d", size);
				else
					logerr(0, "WriteFile");
				Sleep(50);
				if(retry++ < 3)
					goto again;
			}
			if(!closehandle(hp,HT_PIPE))
				logerr(0, "closehandle");
		}
		else
		{
			logerr(0, "DuplicateHandle of init_pipe=%p for type=%d msg=%d", Share->init_pipe, msgid, message);
			notify_init();
			Sleep(50);
			if(retry++ < 3)
				goto again;
		}
	}
	else
	{
		i = 2;
		threadid = pp->wait_threadid;
		for (;;)
		{
			if (PostThreadMessage(threadid,WM_wait,msgid,message))
				goto poll;
			if (!--i)
				break;
			Sleep(300);
		}
		logerr(0, "PostThreadMessage %x failed slot=%d", threadid, slot);
	}
	return 0;
poll:
	if(P_CP->pid!=1 && msgid==0)
	{
		if(!wait_acknowledge(proc_ptr(message&0xffff),threadid,__LINE__))
			goto again;
	}
	return(1);
}

/*
 *  make the init process be the parent for <proc>
 */
void proc_reparent(Pproc_t *proc,int slot)
{
	Pproc_t *pp=proc_ptr(slot);
	if(proc->forksp)
	{
		logmsg(0, "proc_reparent called for forked child");
		return;
	}
	wait_acknowledge(proc,0,__LINE__);
	proc->inexec |= PROC_MESSAGE;
	if(proc!=P_CP && proc->ppid!=P_CP->pid)
	{
		InterlockedIncrement(&proc->inuse);
		send2slot(slot, 5, proc_slot(proc));
	}
	else if(dup_to_init(proc,slot))
	{
		InterlockedIncrement(&proc->inuse);
		logmsg(LOG_PROC+4,"reparent slot=%d pid=%x-%d state=%(state)s",proc_slot(proc),proc->ntpid,proc->pid,proc->state);

		/* lock process slot while sending message */
		if(slot!=Share->init_slot)
			logmsg(0, "proc_reparent not init slot=%d", slot);
		proc->ppid = pp->pid;
		proc->parent = slot;
		send2slot(slot, 0, proc_slot(proc));
	}
	else /*if(proc->state!=PROCSTATE_EXITED)*/
	{
		logerr(0, "proc_reparent parent=%d pid=%d slot=%d posix=0x%x state=%(state)s handle=%d inuse=%d inexec=0x%x procname=%s", proc->ppid, proc->pid, proc_slot(proc), proc->posixapp, proc->state, proc->ph, proc->inuse, proc->inexec,proc->cmd_line);
		proc->inexec &= ~PROC_MESSAGE;
	}
}

unsigned long *_procbits = NULL;
unsigned long *_oldbits = NULL;
unsigned long _procbits_sz;
__inline void set_procbit(unsigned long n)
{
	static unsigned long max = 0;
	if (_procbits == NULL)
		return;
	if(n< _procbits_sz)
		_procbits[n>>7] |= (1L<< ((n>>2)&0x1f));
	else if(n > max+64)
	{
		logmsg(0,"procbit overflow hp=0x%x max=0x%x",n,_procbits_sz);
		max = n;
	}
}
__inline void set_oldprocbit(unsigned long n)
{
	static unsigned long max = 0;
	if (_oldbits == NULL)
		return;
	if(n< _procbits_sz)
		_oldbits[n>>7] |= (1L<< ((n>>2)&0x1f));
	else if(n > max+64)
	{
		logmsg(0,"oldprocbit overflow hp=0x%x max=0x%x",n,_procbits_sz);
		max = n;
	}
}
__inline unsigned long is_procbit(unsigned long n)
{
	if(_procbits != NULL && n< _procbits_sz)
		return(_procbits[n>>7] & (1L<< ((n>>2)&0x1f)));
	return(0);
}
__inline unsigned long is_oldprocbit(unsigned long n)
{
	if(_oldbits != NULL && n< _procbits_sz)
		return(_oldbits[n>>7] & (1L<< ((n>>2)&0x1f)));
	return(0);
}

static int process_message(int type, int slot)
{
	DWORD i;
	int r=0;
	Pproc_t *proc=0;
	logmsg(LOG_PROC+7, "process_message type=%d slot=%d", type, slot);
	Share->init_bits |= IN_MESSAGE;
	switch(type)
	{
	    case 0:
		break;
	    case 2:
		utentry(dev_ptr(slot),0,NULL,NULL);
		goto done;
	    case 3:
	    {
		Pdev_t *pdev = dev_ptr(slot);
		utentry(pdev,1,pdev->uname,NULL);
		goto done;
	    }
	    case 4:
		P_CP->debug = slot & 0xff;
		goto done;
	    case 5:
		type = 0;
		proc = proc_ptr(slot);
		if(proc->ph=open_proc(proc->ntpid,PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE))
			break;
		proc_release(proc);
		goto done;
	    default:
		logmsg(0, "message_unknown type=0x%x", type);
		goto done;
	}
	if(slot==0)
	{
		logmsg(0, "wait_thread: invalid process slot 0, msg=0x%x", type);
		goto done;
	}
	proc = proc_ptr(slot);
	if (proc->posixapp & IN_RELEASE)
		logmsg(0,"proc_msg in_release slot=%d pid=%x-%d state=%(state)s exitval=0%x posix=0%x inexec=0%x name=%s cmd=%s",proc_slot(proc),proc->ntpid,proc->pid,proc->state,proc->exitcode,proc->posixapp,proc->inexec,proc->prog_name, proc->cmd_line);
	proc->posixapp |= ON_WAITLST;
	if(proc->inexec&PROC_HAS_PARENT)
		set_exec_event(proc,0);
	for(i=1; i < nwait; i++)
		if(procs[i]==proc)
		{
			proc_release(proc);
			if(P_CP->pid==1 && proc->origph)
				set_procbit((unsigned long)proc->origph);
			r = i+1;
			goto done;
		}
	procs[nwait] = proc;
	logini(LOG_PROC+1, "init_wait_add index=%d slot=%d inuse=%d pid=%x-%d handle=%p inexec=0x%x", nwait, slot, proc->inuse, proc->ntpid,proc->pid, proc->ph,proc->inexec);
	if(P_CP->pid==1 && proc->ppid!=1)
	{
		logmsg(1,"process_message pid=%d ppid=%d",proc->pid,proc->ppid);
		proc->ppid = 1;
	}
	if((i=WaitForSingleObject(proc->ph,0))!=WAIT_TIMEOUT)
	{
		if(i!=WAIT_OBJECT_0)
			logerr(0, "WaitForSingleObject failed=%d nwait=%d slot=%d use=%d id=%x-%d state=%(state)s handle=%p", i, nwait, slot, proc->inuse, proc->ntpid,proc->pid, proc->state,proc->ph);
		if(P_CP->pid==1)
		{
			HANDLE	hp=0;
			DWORD	code= -1;
			proc->posixapp &= ~ON_WAITLST;
			proc_release(proc);
			if((hp=open_proc(proc->ntpid,PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE)) && GetExitCodeProcess(hp,&code) && code==STILL_ACTIVE)
			{
				closehandle(proc->ph,HT_PROC);
				proc->ph = hp;
				goto good;
			}
			if(hp)
				closehandle(hp,HT_PROC);
			proc->exitcode = 1;
		}
		else
		{
			logmsg(0, "sigchild from pid=%d", proc->pid);
			sigsetgotten (P_CP,SIGCHLD, 1);
		}
		proc->posixapp &= ~ON_WAITLST;
		proc_release(proc);
		goto done;
	}
	if(P_CP->pid==1)
	{
		/* exitcode=1 prevents uwin_exec from creating sync event */
		proc->exitcode = 1;
		proc->posixapp &= ~ON_WAITLST;
		proc_release(proc);
	}
good:
	if(P_CP->pid==1 && proc->origph)
		set_procbit((unsigned long)proc->origph);
	r = nwait+1;
	if (r == MAXIMUM_WAIT_OBJECTS && P_CP->pid!=1)
	{
		nomorechildren = 1;
		r++;
	}
done:
	if(proc)
		proc->inexec &= ~PROC_MESSAGE;
	Share->init_bits &= ~IN_MESSAGE;
	return(r);
}

/*
 * determines if the process is in init's list of processes to reap
 */
static int onproclist(Pproc_t* proc)
{
	HANDLE	ph;
	DWORD	pid;
	DWORD	i;

	for (i = 0; i < nwait; i++)
		if (procs[i] == proc)
		{
			if (objects[i] != proc->ph)
				closehandle(objects[i], HT_PROC);
			if ((pid = GetProcessId(proc->ph)) == proc->ntpid)
			{
				objects[i] = proc->ph;
				return 1;
			}
			logmsg(0, "onproclist ph=%p slot=%d ntpid(%x) != ph_pid(%x)", proc->ph, proc_slot(proc), proc->ntpid, pid);
			if (!(ph = OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE, FALSE, proc->ntpid)))
				logerr(0, "onproclist ntpid=%x ph %p => %p", proc->ntpid, proc->ph, ph);
			closehandle(proc->ph, HT_PROC);
			proc->ph = objects[i] = ph;
			return 1;
		}
	return 0;
}

struct procdone
{
	pid_t		pid;
	unsigned short	generation;
	int		inuse;
	int		state;
	unsigned short	slot;
};



static struct procdone	finished[100];
static unsigned long	*Blockgen;
static unsigned long	max_handle;
static unsigned long	old_max_handle = 0;

/*
 * called periodically by init process to clean up exited processes
 */
static int proc_cleanup(void)
{
	int	fd,r=0,i, procstate;
	HANDLE	hp=0;
	Pproc_t *pp, *pproc;
	Pfd_t	*fdp;
	char name[256];
	max_handle = 0;
	Share->init_bits |= IN_CLEANUP;
	if (!_procbits)
	{
		_procbits = (unsigned long *)_ast_malloc(sizeof(unsigned long)*2048);
		_procbits_sz = sizeof(unsigned long)*1024 * 32; /* bits in unsigned long */
		if (_procbits)
			memset(_procbits, 0, _procbits_sz);
		_oldbits = &_procbits[1024];
		memset(_oldbits, 1, _procbits_sz);
	}
	if(!_oldbits)
	{
		_oldbits = (unsigned long *)_ast_malloc(sizeof(unsigned long)*1024);
		if (_oldbits)
			memset(_oldbits, 0, _procbits_sz);
		logmsg(0, "_oldbits allocated 0x%x", _oldbits);
	}
	clean_deleted(uwin_deleted(0, name, sizeof(name), 0));
	memset(Blockgen,0,Share->nfiles*sizeof(long));
	for(i=0; i < Share->nfiles; i++)
	{
		if((Blocktype[i]&BLK_MASK)==BLK_FILE || (Blocktype[i]&BLK_MASK)==BLK_DIR)
			Blockgen[i] = Generation[i];
		if((fdp = &Pfdtab[i]) && fdp->refcount>=0 && fdp->type!=FDTYPE_MEM && fdp->type!=FDTYPE_FULL && fdp->type<FDTYPE_NONE)
			fdp->extra32[1] |= 3;
	}
	Blockgen[Share->nullblock] = 0;
	for(i=1; i < Share->nfiles; i++)
	{
		if((Blocktype[i]&BLK_MASK) != BLK_PROC)
			continue;
		pp = (Pproc_t*)proc_ptr(i);
		procstate = pp->state;
		if(pp->inuse < 0 && (procexited(pp)||procstate==PROCSTATE_UNKNOWN))
			continue;
		InterlockedIncrement(&pp->inuse);
		for(fd=0; fd<pp->maxfds; fd++)
			if(fdslot(pp,fd)>=0 && (fdp = getfdp(pp,fd)))
			{
				if(fdp->index)
					Blockgen[fdp->index]=0;
				if(fdp->type!=FDTYPE_MEM && fdp->type!=FDTYPE_FULL)
					fdp->extra32[1] &= ~2;
			}
		if(pp->curdir>=0)
		{
			fdp = &Pfdtab[pp->curdir];
			if(fdp->index)
				Blockgen[fdp->index] = 0;
			fdp->extra32[1] &= ~2;
		}
		if(pp->rootdir>=0)
		{
			fdp = &Pfdtab[pp->rootdir];
			if(fdp->index)
				Blockgen[fdp->index] = 0;
			fdp->extra32[1] &= ~2;
		}
		if(pp->state==PROCSTATE_SPAWN)
			goto cont;
		if(procexited(pp) && pp->inuse > 1)
			logmsg(1, "exited_proc inuse>1 slot=%d inuse=%d pid=%x-%d posix=0x%x parent=%d nchild=%d child=%d gent=%d state=%(state)s name=%s cmd=%s", proc_slot(pp), pp->inuse, pp->ntpid, pp->pid, pp->posixapp, pp->parent, pp->nchild, pp->child, pp->group, pp->state, pp->prog_name, pp->cmd_line);
		if(pp->inuse!=1)
		{
#if SET_HANDLES
			if(pp->ppid == 1)
			{
				set_procbit((unsigned long)pp->phandle);
				set_procbit((unsigned long)pp->ph);
				set_procbit((unsigned long)pp->origph);
				if ((unsigned long)pp->phandle > max_handle)
					max_handle = (unsigned long)pp->phandle;
			}
#endif /* SET_HANDLES */
			goto cont;
		}
		if(!procexited(pp))
		{
			DWORD status = STILL_ACTIVE;
			if(pp->state==PROCSTATE_SPAWN || pp->ntpid <=10)
				goto cont;
			if(hp=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,pp->ntpid))
			{
				GetExitCodeProcess(hp,&status);
				closehandle(hp,HT_PROC);
				hp = NULL;
			}
			if(status==STILL_ACTIVE)
				goto cont;
			if((Blocktype[i]&BLK_MASK) != BLK_PROC)
				goto cont;
			logmsg(LOG_PROC+4,"proc_cleanup process %x-%d ppid=%d block %d btype=0x%x state=%d name=%s cmd=%s does not exist - set to terminated",pp->ntpid,pp->pid,pp->ppid,block_slot(pp),Blocktype[i],pp->state,pp->prog_name,pp->cmd_line);
		}
		if(onproclist(pp))
			goto cont;
		logmsg(LOG_PROC+6, "exited process not on list pid=%x-%d slot=%d state=%d inuse=%d ppid=%d pgid=%d gent=%d root=%d name=%s cmd=%s", pp->ntpid, pp->pid, proc_slot(pp), pp->state,pp->inuse, pp->pid, pp->pgid, pp->group,pp->rootdir,pp->prog_name, pp->cmd_line);
		if (pp->pid==pp->pgid && pp->group && (Blocktype[pp->group]&BLK_MASK) != BLK_PROC)
			logmsg(0,"badgroup slot=%d pid=%x-%d state=%(state)s ppid=%d gent=%d type=%d btype=0x%x name=%s cmd=%s",i,pp->ntpid,pp->pid,pp->state,pp->ppid,pp->group,(Blocktype[pp->group]&BLK_MASK),Blocktype[pp->group],pp->prog_name,pp->cmd_line);
		else if (pp->pid==pp->pgid && pp->group)
		{
			Pproc_t *proc = proc_ptr(pp->group);
			logmsg(LOG_PROC+4,"leader i=%d g=%d inuse=%d pid=%x-%d posix=0%x nchild=%d child=%d state=%(state)s name=%s cmd=%s GRP_HD pid=%x-%d parent=%d ppid=%d posix=0%x state=%(state)s grp=%d gent=%d name=%s cmd=%s",i,pp->group,pp->inuse,pp->ntpid,pp->pid,pp->posixapp,pp->nchild,pp->child,pp->state,pp->prog_name,pp->cmd_line,proc->ntpid,proc->pid,proc->parent,proc->ppid,proc->posixapp,proc->state,proc->pgid,proc->group,proc->prog_name,proc->cmd_line);
			logmsg(LOG_PROC+4, "pgrp_leader skipped %(state)s pid=%x-%d slot=%d inuse=%d ppid=%d pgid=%d gent=%d name=%s cmd=%s findlst=0%x", pp->state,pp->ntpid, pp->pid, proc_slot(pp), pp->inuse, pp->ppid, pp->pgid, pp->group,pp->prog_name, pp->cmd_line, proc_findlist(pp->pgid));
			goto cont;
		}
		if(pp->state!=PROCSTATE_EXITED && pp->ppid!=1 && (pproc=proc_getlocked(pp->ppid, 0)))
		{
			DWORD status=0;
			if (hp)
			{
				logmsg(0, "proc_cleanup open handle=%04x", hp);
				closehandle(hp, HT_PROC);
				hp = NULL;
			}
			if(hp=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,pproc->ntpid))
			{
				GetExitCodeProcess(hp,&status);
				closehandle(hp,HT_PROC);
				hp = NULL;
			}
			proc_release(pproc);
			if(status==STILL_ACTIVE)
				goto cont;
		}
		if((Blocktype[i]&BLK_MASK) != BLK_PROC || pp->state!=procstate)
		{
			logmsg(0,"xxx btype=0%x state=%(state)s-%(state)s",Blocktype[i],procstate,pp->state);
			goto cont;
		}
		if(pp->inuse<=0)
		{
			logmsg(0, "proc_cleanup type=%d btype=0%x %(state)s process deleted pid=%x-%d slot=%d inuse=%d ppid=%d pgid=%d gent=%d name=%s cmd=%s", Blocktype[i]&BLK_MASK,Blocktype[i],pp->state,pp->ntpid, pp->pid, proc_slot(pp), pp->inuse, pp->ppid, pp->pgid, pp->group,pp->prog_name,pp->cmd_line);
			proc_release(pp);
			if (hp)
			{
				logmsg(0, "proc_cleanup handle=%04x active", hp);
				closehandle(hp, HT_PROC);
				hp = NULL;
			}
			continue;
		}
		InterlockedDecrement(&pp->inuse);
		finished[r].slot = i;
		finished[r].pid = pp->pid;
		finished[r].generation = (unsigned short)Generation[i];
		finished[r].inuse = pp->inuse;
		finished[r].state = procstate;
		if(r++ >= sizeof(finished)/sizeof(*finished))
		{
			logmsg(0,"proc_cleanup more than %d finished", sizeof(finished)/sizeof(*finished));
			break;
		}
		continue;
	cont:
		if (hp)
		{
			logmsg(0, "proc_cleanup handle=%04x active", hp);
			closehandle(hp, HT_PROC);
		}
		proc_release(pp);
	}
	logmsg(1,"proc_cleanup: r=%d",r);
	Share->init_bits &= ~IN_CLEANUP;
	return(r+1);
}

static int handle_cnt=0;
static int proc_expunge(int n)
{
	int  i;
	Pproc_t *pp;
	Pfd_t	*fdp;
	struct procdone *dp;
	static int last;
	static int olast;
	DWORD	x;
	DWORD	ntpid;
	HANDLE	hp;
	max_handle += 256;
	/* check for dead handles*/
	if (_oldbits)
	{
		if(olast != last)
		{
			if(olast==4*_procbits_sz)
				olast = 0;
			for(i=olast,olast+=_procbits_sz; i<olast; i+=4)
			//for(i=0; i<(int)old_max_handle; i+=4)
			{
				if (!is_oldprocbit(i))
					continue;
				if((hp=(HANDLE)i) && GetExitCodeProcess(hp,&x) && x!=STILL_ACTIVE && (ntpid=GetProcessId(hp)))
				{
					if(pp = proc_ntgetlocked(ntpid,1))
					{
						if(pp->state!=PROCSTATE_EXITED)
							proc_release(pp);
					}
					else
					{
						for(x=0; x < nwait; x++)
						{
							if(objects[x]==hp)
							{
								logmsg(0,"noexpunge %p on waitlist at %d",hp,x);
								break;
							}
						}
						if(x<nwait)
							continue;
#ifdef CLOSE_HANDLE
						if (init_handles)
						{
							memset(&init_handles[(int)hp], 0, sizeof(struct init_handles));
							logmsg(0, "unknown_old_handle=%p ihandles=%d ecode=%d ntpid=%x data seq=%d line=%d:%d:%d:%d op=0x%x", hp, Share->ihandles, x, ntpid, init_handles[(int)hp].seqno, init_handles[(int)hp].line[3], init_handles[(int)hp].line[2], init_handles[(int)hp].line[1], init_handles[(int)hp].line[0], init_handles[(int)hp].op);
						}
#else
						logmsg(0, "unknown_old_handle=%p ihandles=%d ecode=%d ntpid=%x", hp, Share->ihandles, x, ntpid);
#endif
						if(!closehandle(hp,HT_PROC))
							logerr(0,"closehandle");
					}
				}
				else
				{
#ifdef CLOSE_HANDLE
					logmsg(0, "cleared_old handle=%p data seq=%d line=%d op=0x%x", hp, init_handles[(int)hp].seqno, init_handles[(int)hp].line, init_handles[(int)hp].op);
#else
					logmsg(0, "cleared_old proc handle=%04x", hp);
#endif
				}
			}
		}
		memset(_oldbits, 0, _procbits_sz);
	}
	if(last==4*_procbits_sz)
		last = 0;
	for(i=last,last+=_procbits_sz; i<last; i+=4)
	//for(i=0; i<(int)max_handle; i+=4)
	{
		if(is_procbit(i))
			continue;
		if((hp=(HANDLE)i) && GetExitCodeProcess(hp,&x) && (x!=STILL_ACTIVE) && (ntpid=GetProcessId(hp)))
		{
			if(pp = proc_ntgetlocked(ntpid,1))
			{
				if(pp->state!=PROCSTATE_EXITED)
					proc_release(pp);
			}
			else
			{
				for(x=0; x < nwait; x++)
				{
					if(objects[x]==hp)
					{
						logmsg(0,"noexpunge %p on waitlist at %d",hp,x);
						break;
					}
				}
				if(x<nwait)
					continue;
				if(_oldbits)
				{
					if((++handle_cnt%10)==0)
						logmsg(0,"unknown_proc handles cnt=%d", handle_cnt);
					//logmsg(0, "set_old proc handle=%04x", hp);
#ifdef CLOSE_HANDLE
					if (init_handles)
					{
						//logmsg(0, "set old_handle=%p ihandles=%d ecode=%d ntpid=%x data seq=%d line=%d:%d:%d:%d op=0x%x", hp, Share->ihandles, x, ntpid, init_handles[(int)hp].seqno, init_handles[(int)hp].line[3], init_handles[(int)hp].line[2], init_handles[(int)hp].line[1], init_handles[(int)hp].line[0], init_handles[(int)hp].op);
					}
#endif
					set_oldprocbit(i);
				}
				else
				{
#if 0
					if((++handle_cnt%100)==0)
						logmsg(0,"unknown_proc handles cnt=%d", handle_cnt);
#else
					logmsg(0, "unknown proc handle=%04x", hp);
#endif
					if(!closehandle(hp,HT_PROC))
						logerr(0,"closehandle");
				}
			}
		}
	}
	old_max_handle = max_handle;
	for(i=0; i < Share->nfiles; i++)
	{
		if((fdp = &Pfdtab[i]) && fdp->refcount>=0)
		{
			char *name="???";
			if(fdp->type!=FDTYPE_MEM && fdp->type!=FDTYPE_FULL && fdp->type<FDTYPE_NONE)
			{
				if(fdp->extra32[1] == 3)
				{
					unsigned short j;
					if((j=fdp->index)>0)
					{
						Blockgen[j] = 0;
						name = fdname(i);
					}
					logmsg(0,"unref_slot fslot=%d type=%(fdtype)s ref=%d index=%d oflag=%o name=%s",i,fdp->type,fdp->refcount,fdp->index, fdp->oflag,name);
					fdpclose(fdp);
				}
				fdp->extra32[1] = 0;
			}
		}
	}
	n--;
	for(i=1; i < Share->nfiles; i++)
	{
		if(Blockgen[i] && Blockgen[i]==Generation[i] && ((Blocktype[i]&BLK_MASK)==BLK_FILE || (Blocktype[i]&BLK_MASK)==BLK_DIR))
		{
			logmsg(1,"unref_index index=%d gen=%d type=%d btype=0x%x file=%s",i,Generation[i],(Blocktype[i]&BLK_MASK),Blocktype[i],block_ptr(i));
			block_free(i);
		}
	}
	for(i=0,x=0; i < n; i++)
	{
		dp = &finished[i];
		pp = (Pproc_t*)proc_ptr(dp->slot);
		if((Blocktype[i]&BLK_MASK)!=BLK_PROC || dp->pid!=pp->pid || dp->state!=pp->state || dp->inuse!=pp->inuse || dp->generation!=Generation[dp->slot])
		{
			if(dp->state!=PROCSTATE_SPAWN)
				x++;
			continue;
		}
		logmsg(LOG_PROC+3,"proc_cleanup %(state)s inuse=%d process deleted pid=%x-%d slot=%d inuse=%d inexec=0x%x posix=0x%x ppid=%d pgid=%d gent=%d ph=%p name=%s",dp->state,dp->inuse,pp->ntpid,pp->pid,proc_slot(pp),pp->inuse,pp->inexec,pp->posixapp,pp->ppid,pp->pgid,pp->group,pp->ph,pp->prog_name);
		if(dp->inuse<0)
			block_free(dp->slot);
		else
		{
			DWORD ntpid=GetProcessId(pp->ph);
			if(ntpid)
				closehandle(pp->ph,HT_PROC);
			pp->ph = 0;
			pp->posixapp&= ~ON_WAITLST;
			proc_release(pp);
		}
	}
	if(x)
		logmsg(1,"proc_expunge %d not expunged",x);
	return(0);
}


#define CLEANUP_SHIFT 15

static DWORD WINAPI cleanup_thread(void* arg)
{
	DWORD	cleanup = Share->nblockops>>CLEANUP_SHIFT;
	int	ndone=0;
	/* Initialize per thread storage allocation */
	ndone=P(2);
	V(2,ndone);
	while(1)
	{
		Sleep(10000);
		if((Share->nblockops>>CLEANUP_SHIFT)!=cleanup)
		{
			if(ndone = proc_cleanup())
			{
				Sleep(60000);
				proc_expunge(ndone);
			}
			cleanup = Share->nblockops>>CLEANUP_SHIFT;
		}
	}
	ExitThread(1);
}

static DWORD WINAPI finish_thread(void* arg)
{
	DWORD n=5, i=(DWORD)arg;
	Pproc_t *proc = procs[i];
	int ntpid;
	ntpid=proc->ntpid;
	while(n-->0)
	{
		if(WaitForSingleObject(objects[i],2000)!=WAIT_TIMEOUT)
			break;
		if(!thwait)
			ExitThread(0);
		if (proc->ntpid != ntpid)
			break;
		if(!TerminateProcess(objects[i],proc->exitcode))
		{
			HANDLE ph;
			logerr(1,"terminate_proc slot=%d objects[%d]=%p ph=%p pid=%x-%d sav_ntpid=%x ppid=%d parent=%d pgid=%d gent=%d state=%(state)s exitval=0x%x posix=0x%x inexec=0x%x name=%s cmd=%s",proc_slot(proc),i,objects[i],proc->ph,proc->ntpid,proc->pid,ntpid,proc->ppid,proc->parent,proc->pgid,proc->group,proc->state,proc->exitcode,proc->posixapp,proc->inexec,proc->prog_name, proc->cmd_line);
			if (proc->ntpid != ntpid)
				break;
			if(ph = open_proc(proc->ntpid,PROCESS_TERMINATE|PROCESS_QUERY_INFORMATION|SYNCHRONIZE))
			{
				if(proc->ph!=objects[i])
					closehandle(proc->ph,HT_PROC);
				closehandle(objects[i],HT_PROC);
				proc->ph = objects[i] = ph;
			}
			else
				logerr(0,"open_proc_term slot=%d objects[%d]=%p ph=%p pid=%x-%d sav_ntpid=%x ppid=%d parent=%d pgid=%d gent=%d state=%(state)s exitval=0x%x posix=0x%x inexec=0x%x name=%s cmd=%s",proc_slot(proc),i,objects[i],proc->ph,proc->ntpid,proc->pid,ntpid,proc->ppid,proc->parent,proc->pgid,proc->group,proc->state,proc->exitcode,proc->posixapp,proc->inexec,proc->prog_name, proc->cmd_line);
		}
	}
	if (proc->ntpid == ntpid)
	{
		if(n==0)
			logmsg(0,"not_exiting  i=%d slot=%d pid=%x-%d state=%(state)s ph=%p maxfd=%d exitval=0x%x posix=0x%x inexec=0x%x name=%s cmd=%s",i,proc_slot(proc),proc->ntpid,proc->pid,proc->state,proc->ph,proc->maxfds,proc->exitcode,proc->posixapp,proc->inexec,proc->prog_name,proc->cmd_line);
		else if(n<4)
		{
			logmsg(n!=1,"exited i=%d n=%d slot=%d pid=%x-%d posix=0x%x inexec=0x%x state=%(state)s ph=%p inuse=%d nchild=%d child=%d name=%s cmd=%s",i,n,proc_slot(proc),proc->ntpid,proc->pid,proc->posixapp,proc->inexec,proc->state,proc->ph,proc->inuse,proc->nchild,proc->child,proc->prog_name,proc->cmd_line);
		}
	}
	ExitThread(0);
}

/*
 * This thread waits for any process to complete and sends SIGCLD
 */
static DWORD WINAPI wait_thread(void* index)
{
	register Pproc_t *proc;
	DWORD msgflag = QS_POSTMESSAGE;
	DWORD rsize;
	register DWORD i;
	int j;
	wait_thrd_strt=1;
	nwait=0;
	proc = proc_ptr((int)index);
	if(state.clone.ev)
		objects[nwait++] = state.clone.ev;
	procs[nwait] = proc;
	objects[nwait++] = proc->ph;
	proc->posixapp |= ON_WAITLST;
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
	SetEvent(waitevent);
	logini(LOG_PROC+1, "begin wait_thread tid=%04x", GetCurrentThreadId());
	if(P_CP->pid==1)
	{
		char dir[256],drive;
		GetCurrentDirectory(sizeof(dir),dir);
		drive = *dir;
		strcpy(dir,"/c/.deleted");
		dir[1] = drive;
		chdir(dir);
		logmsg(1,"init directory=%s curdir=%d cd=%s",dir,P_CP->curdir,state.pwd);
		Blockgen = (unsigned long*)calloc(sizeof(long),Share->nfiles);
		if(!CreateThread(sattr(0),0,cleanup_thread,(void*)0,0,&rsize))
			logerr(0,"cleanup_thread creation failed");
	}
	while(nwait>0 || msgflag)
	{
		while(1)
		{
			i= mwait(nwait,objects,FALSE,5000,msgflag);
			if(i!=WAIT_TIMEOUT)
				break;
			i = (proc_slot(P_CP)==Share->init_slot);
			if(state.clone.hp)
				i++;
			for(; i < nwait; i++)
			{
				proc = procs[i];
				if((proc->state==PROCSTATE_TERMINATE || proc->state==PROCSTATE_ABORT || (proc->posixapp&POSIX_DETACH)) && proc->ph==objects[i])
				{
					HANDLE th;
					if(!(th=CreateThread(sattr(0),0,finish_thread,(void*)i,0,&rsize)))
						logerr(0,"finish_thread creation failed");
					closehandle(th,HT_THREAD);
				}
				else if(proc->posixapp&POSIX_DETACH)
					logmsg(0,"detach i=%d ph=%p objects[i]=%p ntpid=%x-%x-%x-%d slot=%d pgid=%d posix=0x%x state=%(state)s name=%s cmd=%s",i,proc->ph,objects[i], GetProcessId(proc->ph),GetProcessId(objects[i]),proc->ntpid,proc->pid,proc_slot(proc),proc->pgid,proc->posixapp,proc->state,proc->prog_name,proc->cmd_line);
			}
			continue;
		}
		if (i==WAIT_OBJECT_0+nwait)
		{
			MSG	msg;
			void*	vp;
			vp = TlsGetValue(P_CP->tls);
			while(PeekMessage(&msg,(HWND)-1, 0,32000,PM_REMOVE))
			{
				if(!vp && (vp = TlsGetValue(P_CP->tls)) && (((unsigned long)vp) & 0xf0000000))
				{
#if 0 /* logerr() here adds to the corruption */
					logerr(LOG_PROC+2, "TlsGetValue(%d)=%p corrupt after PeekMessage -- resetting to 0", P_CP->tls, vp);
#endif
					vp = 0;
					TlsSetValue(P_CP->tls, vp);
				}
				if(msg.lParam==0 && msg.wParam==99)
				{
					if (state.clone.hp)
						goto exit_thread;
					ExitThread(2);
				}
				i=(int)process_message((int)msg.wParam,(int)msg.lParam);
				if(i==0)
					continue;
				proc = procs[--i];
				if(i>nwait)
					break;
				if(i<nwait)
				{
					if(objects[i]!=proc->ph)
					{
						closehandle(objects[i],HT_PROC);
						objects[i] = proc->ph;
					}
					continue;
				}
				objects[nwait++] = proc->ph;
			}
			SetEvent(waitevent);
		}
		else if(i>=WAIT_OBJECT_0 && i<WAIT_OBJECT_0+nwait)
		{
			i -= WAIT_OBJECT_0;
			proc = procs[i];
			if (parent_thread && objects[i] == child_proc)
			{
				fork_err = proc->exitcode;
				logmsg(0, "fork child %d failed", proc->pid);
				while ((j = ResumeThread(parent_thread)) > 0);
				if (j < 0)
					logmsg(0, "fork child %d parent thread resume failed fork_err=%d", proc->pid,fork_err);
				parent_thread = 0;
			}
			if(i==0 && ((proc_slot(P_CP)==Share->init_slot) || state.clone.ev))
			{
				DWORD message,size;
				HANDLE hp=0;
				int n = 0;
				if(state.clone.ev)
				{
					message = state.clone.msg;
					state.clone.msg = 0;
					ResetEvent(state.clone.ev);
					if(message== (99<<16) )
						goto exit_thread;
					goto skip;
				}
				hp=proc->ph2;
				ResetEvent(objects[0]);
				SetLastError(0);
				while(i=PeekNamedPipe(hp,NULL,0,NULL,&n, NULL))
				{
					if(n==0)
						break;
					while(n>0 && ReadFile(hp,&message,sizeof(DWORD),&size,NULL))
					{
						n -= sizeof(DWORD);
					skip:
						i=process_message(message>>16,message&0xffff);
						if(i==0)
							continue;
						if(state.clone.ev)
							SetEvent( waitevent);
						proc = procs[--i];
						if(i>nwait)
							break;
						if(i<nwait)
						{
							if(objects[i]!=proc->ph)
							{
								closehandle(objects[i],HT_PROC);
								objects[i] = proc->ph;
							}
							continue;
						}
						objects[nwait++] = proc->ph;
					}
					if(state.clone.ev)
						break;
				}
				if(i==0)
					logerr(0, "PeekNamedPipe");
				if(i==0 && (GetLastError()==ERROR_INVALID_HANDLE || GetLastError()==ERROR_BROKEN_PIPE))
				{
					/* try to recover */
					HANDLE hpin,hpout;
					if(CreatePipe(&hpin,&hpout,sattr(0),1024))
					{
						closehandle(proc->ph2,HT_PROC);
						proc->ph2 = hpin;
						Share->init_pipe = hpout;
					}
				}
				continue;
			}
			if(!(proc->inexec&PROC_HAS_PARENT))
			{
				struct tms tm;
				if(proc->ph && proc->ph!=objects[i])
				{
					unsigned short slot=proc_slot(proc);
					if((Blocktype[slot]&BLK_MASK)!=BLK_PROC)
					{
						logmsg(0,"wait_proc_deleted: slot=%d btype=0x%x pid=%x-%d ntpid=%x ntpid2=%x i=%d state=%(state)s inexec=0x%x ph=%p objects[i]=%p",slot,Blocktype[slot], proc->ntpid,proc->pid,GetProcessId(objects[i]),GetProcessId(proc->ph),i,proc->state,proc->inexec,proc->ph,objects[i]);
						objects[i] = objects[--nwait];
						procs[i] = procs[nwait];
						SetEvent(waitevent);
						PulseEvent(P_CP->sevent);
						eventnum++;
						continue;
					}
					if(GetProcessId(proc->ph)==proc->ntpid)
					{
						setlastpid(proc->ntpid);
						closehandle(objects[i],HT_PROC);
						objects[i] = proc->ph;
						if(WaitForSingleObject(proc->ph,0)==WAIT_TIMEOUT)
							continue;
					}
					else
						closehandle(proc->ph,HT_PROC);
					proc->ph = objects[i];
				}
				if(proc_active(proc))
				{
					DWORD code;
					if(GetExitCodeProcess(objects[i],&code))
						closehandle(objects[i],HT_PROC);
					else
					{
						code = GetLastError();
						logmsg(0,"active2 objects[i]=%p slot=%d ph=%p pid=%x-%d posix=0x%x state=%(state)s btype=0x%x ntpid=%d err=%d",objects[i],proc_slot(proc),proc->ph,proc->ntpid,proc->pid,proc->posixapp,proc->state,Blocktype[proc_slot(proc)],GetProcessId(objects[i]),code);
					}
					objects[i] = proc->ph;
					continue;
				}
				rsize = 0;
				if(!GetExitCodeProcess(objects[i],&rsize))
					logerr(0, "GetExitCodeProcess handle=%p rsize=%d i=%d pid=%d slot=%d", objects[i], rsize,i, proc->pid, proc_slot(proc));
				if(proc->ph!=objects[i])
				{
					if(rsize)
						closehandle(objects[i],HT_PROC);
					objects[i] = proc->ph;
				}
				logini(LOG_PROC+1, "init_wait_delete index=%d slot=%d pid=%d ph=%p phandle=%p maxfd=%d", i, proc_slot(proc), proc->pid, proc->ph, proc->phandle,proc->maxfds);
				if(proc->ntpid==0)
					logmsg(0, "wait_thread process already deleted slot=%d", proc_slot(proc));
				if(proc->sid==0)
					logmsg(0, "wait_thread sid==0 slot=%d inuse=%d pid=%d", proc_slot(proc),proc->inuse,proc->pid);
				logini(LOG_PROC+1, "exitcode=%u ntpid=%x pid=%d inuse=%d name=%s", rsize, proc->ntpid, proc->pid, proc->inuse, proc->prog_name);
				tm.tms_stime = tm.tms_cutime = 0;
				logmsg(LOG_PROC+2, "WaitThread exitcode=0x%x rsize=0x%x pid=%x-%d slot=%d inuse=%d posix=0x%x state=%(state)s nchild=%d ph=%p phandle=%p", proc->exitcode,rsize,proc->ntpid,proc->pid,proc_slot(proc),proc->inuse,proc->posixapp,proc->state,proc->nchild,proc->ntpid,proc->ph,proc->phandle);
				if(proc->child)
				{
					logmsg(1,"orphan1 slot=%d pid=%x-%d nchild=%d name=%s",proc_slot(proc),proc->ntpid,proc->pid,proc->nchild,proc->prog_name);
					proc_orphan(proc->pid);
				}
				logmsg(LOG_PROC+4, "utime=%lu", tm.tms_cutime);
#if 0
				if(proc->state == PROCSTATE_TERMINATE)
					proc->exitcode = (unsigned short)rsize;
				else
					proc->exitcode = (unsigned short)(rsize << 8);
#else
				if(proc->state == PROCSTATE_TERMINATE || proc->state == PROCSTATE_ABORT)
				{
					proc->exitcode = (unsigned short)rsize;
					logmsg(LOG_PROC+4, "%(state)s exitcode 0x%x rsize=0x%x", proc->state, proc->exitcode, rsize);
				}
				else
				{
					proc->exitcode = (unsigned short)(rsize << 8);
					logmsg(LOG_PROC+4, "%(state)s != PROCSTATE_TERMINATE 0x%x", proc->state, proc->exitcode);
				}
#endif
				proc->state = PROCSTATE_ZOMBIE;
				objects[i] = objects[--nwait];
				procs[i]->posixapp &= ~ON_WAITLST;
				procs[i] = procs[nwait];
				nomorechildren = 0;
				if(proc->maxfds>0)
					fdcloseall(proc);
				proc->posixapp &= ~ON_WAITLST;
				if (!SigIgnore(P_CP,SIGCHLD) && !isconsole_child(proc))
				{
					proc->notify = 1;
					sigsetgotten (P_CP,SIGCHLD, 1);
				}
				else
				{
					proc->state = PROCSTATE_EXITED;
					if(!(proc->posixapp&UWIN_PROC))
						proc_release(proc);
				}
				if(proc->posixapp&UWIN_PROC) /* abnormal exit */
				{
					proc->notify = 1;
					if(rsize==99) /* from process viewer*/
						proc->exitcode = SIGTERM;
#if 0
					/* Previously signals where placed in high order byte */
					else if((rsize&0xff)==0 && (rsize>>8)<32)
						proc->exitcode = (unsigned short)(rsize>>8);

					else
						proc->exitcode = SIGSEGV;
#endif
					logmsg(LOG_PROC+3, "wait_thread abnormal exitcode 0x%x pid=%d slot=%d inuse=%d ppid=%d", proc->exitcode,proc->pid,proc_slot(proc),proc->inuse, proc->ppid);
					if(proc->ppid==1)
					{
						if(proc->child)
							proc_orphan(proc->pid);
						proc_release(proc);
					}
				}
				SetEvent(waitevent);
				PulseEvent(P_CP->sevent);
				eventnum++;
			}
			else
			{
				/*
				 * This child has been exec'ed
				 * replace wait with new process wait
				 */
				if(proc->ntpid==0)
					logmsg(0, "wait_thread replaced process already deleted slot=%d inuse=%d", proc_slot(proc), proc->inuse);
				logini(LOG_PROC+1, "init_wait_replace index=%d slot=%d pid=%d handle=%p obj=%p", i, proc_slot(proc), proc->pid, proc->ph,objects[i]);
				/* on first exec, don't close handle so
				 * that process id won't be reused
				 */
				if(!proc->origph)
				{
					proc->origph = objects[i];
					set_procbit((unsigned long)proc->origph);
				}
				else if(!closehandle(objects[i],HT_PROC))
					logerr(0, "closehandle slot=%d i=%d hp=%p objects[i]=%p",proc_slot(proc),i,proc->ph,objects[i]);
				objects[i] = proc->ph;
				logmsg(LOG_PROC+3, "wait_thread newproc=0x%x newprocid=%d", procs[i]->ntpid, procs[i]->pid);
				proc->inexec &= (PROC_DAEMON|PROC_MESSAGE);
				set_exec_event(proc,1);
			}
		}
		else
		{
			DWORD n = nwait,j;
			logerr(7, "WaitForMultipleObjects %d objects", n);
			for(i=0; i < n; i++)
			{
				if((j=WaitForSingleObject(objects[i],1))==WAIT_FAILED)
				{
					DWORD code;
					Pproc_t *pp = procs[i];
					unsigned slot = proc_slot(pp);
					logerr(0, "WaitForSingleObject wait_thread: index=%d nwait=%d slot=%d handle=%p ph=%p inuse=%d name=%s pid=%x-%d ppid=%d state=%(state)s inexec=0x%x ntpid1=%x", i,n,slot, objects[i],pp->ph,pp->inuse,pp->prog_name,pp->ntpid,pp->pid, pp->ppid,pp->state,pp->inexec,GetProcessId(pp->ph));
					if(GetExitCodeProcess(objects[i],&code))
						closehandle(objects[i],HT_PROC);
					else
						logerr(0,"badhandle objects[%d]=%p",i,objects[i]);
					if((Blocktype[slot]&BLK_MASK)!=BLK_PROC)
					{
						logmsg(0,"wait_proc_deleted2 slot=%d btype=0x%x",proc_slot(pp),Blocktype[slot]);
						SetEvent(waitevent);
						PulseEvent(P_CP->sevent);
						eventnum++;
					}
					else if(proc_active(pp))
					{
						objects[i] = pp->ph;
						break;
					}
					else if(is_slotvalid(proc_slot(pp)))
					{
						proc->posixapp &= ~ON_WAITLST;
						proc_release(pp);
					}
					else
						logmsg(0, "wait_proc invalid slot=%d btype=0x%x", proc_slot(pp), Blocktype[slot]);
					objects[i] = objects[--nwait];
					procs[i] = procs[nwait];
					break;
				}
			}
		}
	}
	logmsg(0, "end wait_thread tid=%04x", P_CP->wait_threadid);
	P_CP->wait_threadid=0;
	closehandle(waitevent,HT_EVENT);
	return(0);
exit_thread:
	for(i=0; i < nwait; i++)
		CloseHandle(objects[i]);
	return(2);
}

static HANDLE createsuidproc(char *cmdname, char *cmdline, const char *title, void *env, DWORD cflags, STARTUPINFO *sinfo, PROCESS_INFORMATION *pinfo)
{
	DWORD rsize,buffer[PATH_MAX/sizeof(DWORD)];
	UMS_child_data_t cr;
	HANDLE hserver,mutex;
	char mutexname[UWIN_RESOURCE_MAX];
	memset(&cr, 0, sizeof(cr));
	UWIN_MUTEX_SETEUID(mutexname);
	if(!(mutex = CreateMutex(sattr(0),FALSE,mutexname)))
	{
		logerr(0, "CreateMutex %s failed", mutexname);
		Sleep(200);
		if(!(mutex = CreateMutex(NULL,FALSE,mutexname)))
		{
			logerr(0, "CreateMutex %s failed", mutexname);
			return(0);
		}

	}
	if(WaitForSingleObject(mutex,3000)!=WAIT_OBJECT_0)
		logerr(0, "WaitForSingleObject");
	if(!(hserver = createfile(UWIN_PIPE_UMS, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)))
	{
		logerr(0, "CreateFile %s", UWIN_PIPE_UMS);
		closehandle(mutex,HT_MUTEX);
		return(0);
	}
	buffer[0] = P_CP->ntpid;
	if(hserver)
	{
		char	*cmd= (char*)&buffer[1], *cp=cmd;
		DWORD	dsize = 0;
		if(cmdname[1]!=':' && cmdname[0]!='\\')
		{
			dsize = GetCurrentDirectory(sizeof(buffer)-sizeof(DWORD),cmd);
			cp += dsize;
			*cp++ = '\\';
		}
		strcpy(cp,cmdname);
		dsize = (DWORD)strlen(cmd)+sizeof(DWORD)+1;
		if (!WriteFile(hserver,(void*)buffer,dsize,&rsize,NULL))
			logerr(0, "WriteFile");
		if(ReadFile(hserver,(void*)&cr,sizeof(cr),&rsize,NULL))
		{
			SECURITY_ATTRIBUTES sattr;
			sattr.nLength = sizeof(sattr);
			sattr.lpSecurityDescriptor = nulldacl();
			sattr.bInheritHandle = TRUE;
			cr.child = 0;
			cr.err = 0;
			if (CreateProcess(cmdname,cmdline,&sattr,&sattr,1,CREATE_SUSPENDED|cflags, env, NULL, sinfo, pinfo))
				cr.child = pinfo->hProcess;
			else
				cr.err = GetLastError();
			if (!WriteFile(hserver, (void*)&cr, sizeof(cr), &rsize, NULL))
				logerr(0, "WriteFile");
		}
		else
			logerr(0, "ReadFile");
		if(!ReadFile(hserver,(void*)&cr,sizeof(cr),&rsize,NULL))
			logerr(0, "ReadFile");
		if(cr.atok==0 || cr.err)
		{
			if(cr.err)
				SetLastError(cr.err);
			cr.atok = INVALID_HANDLE_VALUE;
		}
		closehandle(hserver,HT_PIPE);
	}
	if(mutex)
	{
		/* To make sure that the pipe is released properly */
		Sleep(10);
		ReleaseMutex(mutex);
		closehandle(mutex,HT_MUTEX);
	}
	return(cr.atok);
}

static int issuidneeded(char* progname, int suid)
{
	char			buf[1024];
	SECURITY_DESCRIPTOR*	psd = (SECURITY_DESCRIPTOR*)buf;
	DWORD			sdsize = 1024, rsdsize, usize = 256, domsize = 256;
	SID_NAME_USE		siduse = SidTypeUser;
	SID*			psid;
	BOOL			owndef;
	char			tokown[1024];
	int			type = 0;
	DWORD			len;

	if (!P_CP->rtoken)
		return 0;
	if (suid&S_ISUID)
		type = OWNER_SECURITY_INFORMATION;
	if (suid&S_ISGID)
		type |= GROUP_SECURITY_INFORMATION;
	if (!type || !GetFileSecurity(progname, type, psd, sdsize, &rsdsize))
		return 0;
	if ((suid&S_ISUID) &&
	   GetSecurityDescriptorOwner(psd, &psid, &owndef) &&
	   GetTokenInformation(P_CP->rtoken, TokenOwner, tokown, sizeof(tokown), &len) &&
	   !EqualSid(psid, ((TOKEN_OWNER*)tokown)->Owner) &&
	   GetTokenInformation(P_CP->rtoken, TokenUser, tokown, sizeof(tokown), &len) &&
	   !EqualSid(psid, ((TOKEN_USER*)tokown)->User.Sid))
		return 1;
	if ((suid&S_ISGID) &&
	    GetSecurityDescriptorGroup(psd, &psid, &owndef) &&
	    GetTokenInformation(P_CP->rtoken, TokenPrimaryGroup, tokown, sizeof(tokown), &len) &&
	    !EqualSid(psid, ((TOKEN_PRIMARY_GROUP*)tokown)->PrimaryGroup))
		return 1;
	return 0;
}


/*
 * start wait thread and wait for it to initialize
 */
static int max_wait_cnt=0;
HANDLE start_wait_thread(int index, DWORD *thwaitid)
{
	HANDLE id;
	int i;
	if(!(waitevent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		logerr(0, "CreateEvent");
	wait_thrd_strt=0;
	if(state.clone.hp)
	{
		if(!(state.clone.ev = CreateEvent(NULL,TRUE,FALSE,NULL)))
			logerr(0,"CreateEvent");
	}
	id = CreateThread(sattr(0),0,wait_thread,(void*)index,0,thwaitid);
	if(id)
	{
		/* save thread id for init process */
		if(P_CP->pid==1)
		{
			Share->init_thread_id = *thwaitid;
			logini(LOG_PROC+1, "init_thread_id=%04x", *thwaitid);
		}
		i=0;
		while ((wait_thrd_strt==0) && (i++<20))
			Sleep(1);

	}
	else
		logerr(0, "CreateThread");
	return(id);
}

#define fdinherit(p,d,f)	(fdslot(p,d)>=0&&(f=getfdp(p,d))&&(!iscloexec(d)||(exeflags&EXE_FORK)&&(f->type==FDTYPE_CONSOLE||f->type==FDTYPE_CONNECT_INET)))

/*
 * copy open file descriptor table into buffer in startupinfo
 * format compatible with C library
 * process slot and x-handles also passed
 */
static int setup_fdtable(int* buffer, int bufsize, STARTUPINFO* sp, Pproc_t* proc, int* detached, char** pphandle, char** pxhandle, int exeflags)
{
	register int	maxfd = P_CP->maxfds;
	register int	i;
	unsigned char*	ftype;
	char*		phandle;
	char*		xhandle;
	Pfd_t*		fdp;
	DWORD		h1;
	DWORD		h2;
	int		console = 0;
	int		hsize;

	while (--maxfd >= 0 && !fdinherit(P_CP, maxfd, fdp));
	maxfd++;
	sp->lpReserved2 = (void*)buffer;
	ftype = (unsigned char*)&buffer[1];
	if ((P_CP->inexec&PROC_INEXEC) && !P_CP->vfork)
		proc = P_CP;
	hsize = maxfd;
	if(exeflags&EXE_UWIN)
	{
		i = 0;
		hsize *= 2;
		xhandle = (char*)(ftype + hsize + maxfd * sizeof(HANDLE));
	}
	else
	{
		i = 0xff;
		xhandle = 0;
	}
	*pxhandle = xhandle;
	*pphandle = phandle = (char*)(ftype + hsize);
	sp->cbReserved2 = (WORD)(sizeof(DWORD) + hsize * (sizeof(unsigned char) + sizeof(HANDLE)));
	memset(ftype, 0, hsize);
	memset(ftype + hsize, i, hsize * sizeof(HANDLE));
	logmsg(LOG_PROC+4, "exetype=%(exetype)s maxfd=%d maxhandle=%d cbReserved2=%d", exeflags, maxfd, hsize, sp->cbReserved2);
	buffer[0] = hsize;
	for (i = 0; i < maxfd; i++)
	{
		if (fdinherit(P_CP, i, fdp))
		{
			if (fdp->type == FDTYPE_CONSOLE || fdp->type ==FDTYPE_CONNECT_INET)
			{
				if (fdp->type == FDTYPE_CONSOLE)
				{
					if (*detached)
					{
						setfdslot(proc, i, -1);
						goto next;
					}
					console = 1;
				}
				if (iscloexec(i))
					Phandle(i) = Fs_dup(i, fdp, &Xhandle(i), 3);
			}
			if (fdp->type == FDTYPE_PTY && (i==1||i==2))
			{
				memcpy(phandle,&Xhandle(i),sizeof(HANDLE));
				if(xhandle && Phandle(i))
					memcpy(xhandle,&Phandle(i),sizeof(HANDLE));
			}
			else
			{
				memcpy(phandle,&Phandle(i),sizeof(HANDLE));
				if(xhandle && Xhandle(i))
					memcpy(xhandle,&Xhandle(i),sizeof(HANDLE));
			}
			if(fdp->type==FDTYPE_EPIPE||fdp->type==FDTYPE_EFIFO)
			{
				if ((fdp->oflag&O_ACCMODE)!=O_RDONLY)
				{
					HANDLE in,hp,me=GetCurrentProcess();
					int pipestate = PIPE_WAIT;
					if(fdp->oflag&O_RDWR)
					{
						memcpy(&in,xhandle,sizeof(HANDLE));
						if(DuplicateHandle(me,in,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
						{
							if(SetNamedPipeHandleState(in,&pipestate,NULL,NULL))
								memcpy(xhandle,&hp,sizeof(HANDLE));
							else if(Share->Platform==VER_PLATFORM_WIN32_NT)
								logerr(0, "SetNamedPipeHandle");
						}
						else
							logerr(0, "DuplicateHandle");
					}
					else
					{
						memcpy(&in,phandle,sizeof(HANDLE));
						if(DuplicateHandle(me,in,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
						{
							if(SetNamedPipeHandleState(in,&pipestate,NULL,NULL))
								memcpy(phandle,&hp,sizeof(HANDLE));
							else if(Share->Platform==VER_PLATFORM_WIN32_NT)
								logerr(0, "SetNamedPipeHandle");
						}
						else
							logerr(0, "DuplicateHandle");
					}
				}
				else if(!(exeflags&(EXE_FORK|EXE_UWIN)))
				{
					/* read-end is native process */
					if(fdp->type==FDTYPE_EFIFO)
						Pfifotab[fdp->devno-1].events |= FD_NATIVE;
					else if(fdp->sigio>1)
						Pfdtab[fdp->sigio-2].socknetevents |= FD_NATIVE;
				}
			}
			incr_refcount(fdp);
			ftype[i] = F_OPEN;
			if(fdp->oflag&O_APPEND)
				ftype[i] |= F_APPEND;
			switch(fdp->type)
			{
			case FDTYPE_FILE:
			case FDTYPE_XFILE:
				break;
			case FDTYPE_TFILE:
				ftype[i] |= F_TEXT;
				break;
			case FDTYPE_SOCKET:
			case FDTYPE_NPIPE:
			case FDTYPE_EPIPE:
			case FDTYPE_PIPE:
			case FDTYPE_FIFO:
			case FDTYPE_EFIFO:
			case FDTYPE_PTY:
				ftype[i] |= F_PIPE;
				break;
			default:
				ftype[i] |= F_DEV;
				break;
			}
			logmsg(LOG_PROC+4, "fd%3d %x%02x %08x:%08x %(fdtype)s", i, iscloexec(i), ftype[i], (memcpy(&h1, phandle, sizeof(h1)), h1), xhandle ? (memcpy(&h2, xhandle, sizeof(h2)), h2) : (h2 = 0), fdp->type);
		}
	next:
		phandle += sizeof(HANDLE);
		if (xhandle)
			xhandle += sizeof(HANDLE);
	}
	*detached = !console;
	if((exeflags&EXE_UWIN) && getfdp(P_CP,1)->type==FDTYPE_PTY)
	{
		phandle = *pphandle;
		memcpy(&sp->hStdInput,phandle,sizeof(HANDLE));
		memcpy(&sp->hStdOutput,phandle+sizeof(HANDLE),sizeof(HANDLE));
		memcpy(&sp->hStdError,phandle+2*sizeof(HANDLE),sizeof(HANDLE));
		sp->dwFlags |= STARTF_USESTDHANDLES;
	}
	return(maxfd);
}

/*
 * only used for windows 95 to prevent thread handle duplication for .com
 * and processes
 */

static int check_cmd(const char *cmd)
{
	size_t l = strlen(cmd);
	if(cmd[l-4]!='.')
		return(1);
	if(fold(cmd[l-3])=='c' && fold(cmd[l-2])=='o' && fold(cmd[l-1])=='m')
		return(0);
	return(1);
}

static int isremote(void)
{
	char	fs[5];

	if (P_CP->curdir<0)
		return 0;
	if(state.pwd[0]=='\\' && state.pwd[1]=='\\')
		return 1;
	memcpy(fs, state.pwd, 3);
	fs[2] = '\\';
	fs[3] = 0;
	return GetDriveType(fs)==DRIVE_REMOTE;
}

#define ptr(t,a,b)	((t*)((char*)(a) + (unsigned long)(b)))
#define out(e,p,n)	((((char*)p)+n)>e)

/*
 * return EXE_* flags for exe fp
 * fp close on return
 */
static int exetype(HANDLE fp, const char* cmdname)
{
	int				r;
	DWORD				n = 0;
	DWORD				v;
	void*				addr;
	char*				end;
	HANDLE*				mp;
	IMAGE_DOS_HEADER*		dhp;
	IMAGE_NT_HEADERS32*		nh32;
	IMAGE_NT_HEADERS64*		nh64;
	IMAGE_SECTION_HEADER*		shp;
	IMAGE_SECTION_HEADER*		she;
	IMAGE_IMPORT_DESCRIPTOR*	ihp;
	char*				name;
	MEMORY_BASIC_INFORMATION	mi;

	r = EXE_UWIN|EXE_SCRIPT;
	if (!(mp = CreateFileMapping(fp, NULL, PAGE_READONLY, 0, n, NULL)))
	{
		logmsg(3, "%s: cannot map file", cmdname);
		goto done;
	}
	if (!(addr = MapViewOfFileEx(mp, FILE_MAP_READ, 0, 0, 0, NULL)))
	{
		logmsg(3, "%s: cannot read file map", cmdname);
		goto done;
	}
	if (VirtualQueryEx(GetCurrentProcess(), addr, &mi, sizeof(mi)) != sizeof(mi))
	{
		logmsg(3, "%s: cannot determine map size", cmdname);
		goto done;
	}
	end = (char*)addr + mi.RegionSize;
	dhp = (IMAGE_DOS_HEADER*)addr;
	if (out(end, dhp, sizeof(*dhp)))
	{
		logmsg(3, "%s: invalid map address -- DOS-header=%p size=%d", cmdname, dhp, sizeof(*dhp));
		goto done;
	}
	if (dhp->e_magic != IMAGE_DOS_SIGNATURE || !dhp->e_lfanew)
	{
		logmsg(3, "%s: invalid exec header -- e_magic mismatch", cmdname);
		goto done;
	}
	r = 0;
	nh32 = (IMAGE_NT_HEADERS32*)((char*)dhp + dhp->e_lfanew);
	if (out(end, nh32, sizeof(*nh32)))
	{
		logmsg(3, "%s: invalid exec header -- address out of bounds", cmdname);
		goto done;
	}
	if (nh32->Signature & 0xFFFF0000)
	{
		logmsg(3, "%s: invalid exec header -- Signature mismatch", cmdname);
		goto done;
	}
	if (nh32->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
	{
		r |= EXE_32;
		if (!(v = nh32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress))
			goto done;
		if (nh32->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
			r |= EXE_GUI;
		shp = (IMAGE_SECTION_HEADER*)&nh32->OptionalHeader.DataDirectory[nh32->OptionalHeader.NumberOfRvaAndSizes];
		n = nh32->FileHeader.NumberOfSections;
	}
	else if (nh32->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64))
	{
		r |= EXE_64;
		nh64 = (IMAGE_NT_HEADERS64*)nh32;
		if (!(v = nh64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress))
			goto done;
		if (nh64->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
			r |= EXE_GUI;
		shp = (IMAGE_SECTION_HEADER*)&nh64->OptionalHeader.DataDirectory[nh64->OptionalHeader.NumberOfRvaAndSizes];
		n = nh64->FileHeader.NumberOfSections;
	}
	else
	{
		logmsg(3, "%s: invalid exec header -- FileHeader.SizeOfOptionalHeader mismatch", cmdname);
		goto done;
	}
	if (out(end, shp, n * sizeof(*shp)))
	{
		logmsg(0, "%s: cannot read section headers address=%p size=%d base=%p", cmdname, shp, sizeof(*ihp), addr);
		goto done;
	}
	for (she = shp + n; shp < she; shp++)
		if (shp->VirtualAddress <= v && v <= (shp->VirtualAddress + shp->SizeOfRawData))
		{
			n = shp->VirtualAddress - shp->PointerToRawData;
			break;
		}
	ihp = ptr(IMAGE_IMPORT_DESCRIPTOR, addr, v - n);
	if (out(end, ihp, sizeof(*ihp)))
	{
		logmsg(0, "%s: cannot read import descriptor address=%p size=%d base=%p adjust=%p", cmdname, ihp, sizeof(*ihp), addr, n);
		goto done;
	}
	for (; ihp->Name; ihp++)
	{
		name = ptr(char, addr, ihp->Name - n);
		if (out(end, name, 15))
		{
			logmsg(0, "%s: cannot read import name address=%p size=%d base=%p", cmdname, name, 15, addr);
			break;
		}
		if (!_stricmp(name, AST_DLL ".dll") || !_stricmp(name, POSIX_DLL ".dll") || !_stricmp(name, SHELL_DLL ".dll") || !_stricmp(name, CMD_DLL ".dll"))
		{
			r |= EXE_UWIN;
			break;
		}
	}
 done:
	if (addr)
		UnmapViewOfFile(addr);
	if (mp)
		closehandle(mp, HT_MAPPING);
	closehandle(fp, HT_FILE);
	logmsg(LOG_PROC+3, "exetype=%(exetype)s %s", r, cmdname);
	return r;
}

static int copytochild(void* addr, SIZE_T size)
{
	if (!VirtualAllocEx(child_proc, addr, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE))
	{
		logerr(0, "VirtualAllocEx addr=%p size=%lu", addr, size);
		return -1;
	}
	if (!WriteProcessMemory(child_proc, addr, addr, size, &size))
	{
		logerr(0, "WriteProcessMemory addr=%p size=%lu", addr, size);
		return -1;
	}
	return 0;
}

static int vcopy(Vmalloc_t* vm, void* addr, size_t size, Vmdisc_t* dp)
{
	int	locked;
	int	r;

	/* copy malloc regions to child */
	if (((unsigned long)addr) & 0xffff)
	{
		logmsg(0,"vcopylog: addr=%p size=%d",addr,size);
		return -2;
	}
	if (locked = ISLOCK(vm->data, 0))
		CLRLOCK(vm->data, 0);
	r = copytochild(addr, size);
	if (r>=0 && locked)
		SETLOCK(vm->data, 0);
	return r;
}

void post_wait_thread(Pproc_t *pp, int i)
{
	int r;
	if(!pp->wait_threadid)
		thwait = start_wait_thread(i,&pp->wait_threadid);
	else
	{
		int n = 8;
		ResetEvent(waitevent);
		if(state.clone.ev)

		{
			int ntry=10;
			while(state.clone.msg  && --ntry>=0)
				Sleep(20);
			state.clone.msg = i;
			n= SetEvent(state.clone.ev);
		}
		else
		while(!PostThreadMessage(pp->wait_threadid,WM_wait,0,i))
		{
			int err = GetLastError();
			if(n>32)
				logerr(0, "PostThreadMessage threadid=%d slot=%d pid=%x-%d name=%s",pp->wait_threadid,proc_slot(pp),pp->ntpid,pp->pid,pp->prog_name);
			if(err!=ERROR_INVALID_THREAD_ID)
				break;
			Sleep(n);
			if(n <= 8192)
				n *= 2;
		}
	}
	if((r=WaitForSingleObject(waitevent,2000))!=WAIT_OBJECT_0)
	{
		if(r>WAIT_OBJECT_0)
		{
			Pproc_t *p = proc_ptr(i);
			r=WaitForSingleObject(waitevent,2000);
			logmsg(0, "WaitForSingleObject waitevent=%p timeout pid=%d state=%(state)s name=%s r=%d", waitevent, p->pid, p->state, p->prog_name, r);
		}
		else
			logerr(0, "WaitForSingleObject waitevent=%p",waitevent);
	}
}

pid_t set_unixpid(Pproc_t *proc)
{
	pid_t	pid;
	Pproc_t	*pp;
	static unsigned char *pid_counters;
	if(Share->pid_counters && !pid_counters)
		pid_counters = (unsigned char*)block_ptr(Share->pid_counters);
	if(pid_counters)
	{
		/* increment ntpid counter */
		int n = (proc->ntpid>>Share->pid_shift)&0x1ff;
		int ntry=9,s = P(3);
		while(ntry-->=0)
		{
			pid = nt2unixpid(proc->ntpid);
			pp = proc_findlist(pid);
			pid_counters[n]++;
			if(pid_counters[n]>=126)
				pid_counters[n] = 0;
			if(!pp)
				break;
			if(ntry <= 6)
				logmsg(0, "start_proc: pid=%d exists for ntpid=%x at slot=%d ntry=%d", pid, proc->ntpid, proc_slot(pp), ntry);
		}
		V(3,s);
	}
	else
		pid = nt2unixpid(proc->ntpid);
	return(pid);
}


/*
 * Releases all allocations done by start_proc()
 */
static void stop_proc(Pproc_t *proc)
{
	if (proc->traceflags)
		closehandle(proc->trace, HT_UNKNOWN);
	proc_uninit(proc);
	InterlockedDecrement(&proc->inuse);
	proc->maxfds = 0;
	proc->pid = 1;
	if((proc->ntpid>=2) && proc->fdtab)
		free((void*)proc->fdtab);
	proc_release(proc);
}

static STARTUPINFO xinfo;

/*
 * start the given program
 */
static Pproc_t* start_proc(char* title, char* cmdname, char* cmdline, char* const argv[],  void* env, struct spawndata* sp, int suid, int exeflags, int init)
{
	register Pproc_t*	proc;
	BOOL			inherit = TRUE;
	DWORD			cflags = NORMAL_PRIORITY_CLASS;
	STARTUPINFO		sinfo;
	STARTUPINFO*		start = &sinfo;
	PROCESS_INFORMATION	pinfo;
	SECURITY_ATTRIBUTES	sattr;
	int			buffer[1024];
	char			tbuff[100];
	char*			curdir=0;
	char*			phandle;
	char*			xhandle;
	HANDLE			hp;
	HANDLE			me = GetCurrentProcess();
	Pproc_t*		pp;
	pid_t			pid= -1;
	int			i;
	int			maxfd;
	int			hastoimpersonate = 0;
	int			detached = 0;
	int			ntry = 0;
	char*			cp;
	char*			dp;
	char*			ep;
	Pfd_t*			fdp;
	SECURITY_DESCRIPTOR*	sd;

	static CPU_t		CPU;
	static int		sdbuff[512];

	if((i=Share->child_max)==0)
		i = CHILD_MAX;
	if (nomorechildren || P_CP->nchild >= i)
	{
		SetLastError(ERROR_TOO_MANY_CMDS);
		errno = EAGAIN;
		return(0);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		/* change each / to \ */
		for(cp=cmdname; *cp; cp++)
			if(*cp=='/')
				*cp = '\\';
	}
	sd = nulldacl();
	lastproc = 0;
	if (!(proc = proc_init(1+!cmdline, P_CP->inexec&PROC_INEXEC)))
		return(0);
	memset(&sinfo, 0,sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	if (exeflags & EXE_FORK)
	{
		cmdline = GetCommandLine();
		memcpy(proc->cmd_line, P_CP->cmd_line, sizeof(proc->cmd_line));
		memcpy(proc->args, P_CP->args, proc->argsize = P_CP->argsize);
		exeflags |= EXE_UWIN|(sizeof(char*) == 8 ? EXE_64 : EXE_32);
		if (isremote())
			exeflags |= EXE_GUI;
		else if (P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
			cflags |= CREATE_NO_WINDOW;
	}
	else
	{
		if ((exeflags&EXE_GUI) && Share->Platform==VER_PLATFORM_WIN32_NT)
			start->lpDesktop = "winsta0\\default";
		if (P_CP->console && ((exeflags&EXE_UWIN) || GetConsoleCP()==0) && dev_ptr(P_CP->console)->major==PTY_MAJOR)
			cflags |= CREATE_NO_WINDOW;
	}
	if (exeflags & EXE_64)
		proc->type |= PROCTYPE_64;
	else if (exeflags & EXE_32)
		proc->type |= PROCTYPE_32;
	sattr.nLength=sizeof(sattr);
	sattr.lpSecurityDescriptor = init ? sd : makeprocsd((SECURITY_DESCRIPTOR*)sdbuff);
	sattr.bInheritHandle = TRUE;
	if(P_CP->console==0 || !dev_ptr(P_CP->console)->notitle)
    {
		start->lpTitle = title;
    }
	else
	{
		GetConsoleTitle(tbuff,sizeof(tbuff));
		start->lpTitle = tbuff;
	}
	if(init)
		cflags |= DETACHED_PROCESS|CREATE_NO_WINDOW;
	else if(P_CP->console==0 && GetConsoleCP()==0)
		cflags |= DETACHED_PROCESS;
	if(sp)
	{
		cflags |= (sp->flags&~(UWIN_TRACE_FLAGS|UWIN_SPAWN_EXEC));
		if(sp->start)
			start = sp->start;
		if(sp->flags&UWIN_TRACE_FLAGS)
		{
			proc->traceflags = (sp->flags&UWIN_TRACE_FLAGS);
			if(proc->traceflags)
			{
				if(getfdp(P_CP,sp->trace)->type==FDTYPE_PTY)
					hp = Xhandle(sp->trace);
				else
					hp = Phandle(sp->trace);
				if(!DuplicateHandle(me,hp,me,&proc->trace,0,TRUE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
			}
		}
		if(sp->flags&UWIN_NOINHERIT)
			inherit = FALSE;
		if(sp->grp)
			cflags |= CREATE_NEW_PROCESS_GROUP;
		if(sp->grp>1)
			proc->pgid = sp->grp;
		else if(sp->grp==1 || sp->grp==-2)
			proc->pgid = 0;
	}
	if(P_CP->inexec&PROC_DAEMON)
	{
		cflags &= ~CREATE_NEW_CONSOLE;
		cflags |= DETACHED_PROCESS;
		proc->inexec |= PROC_DAEMON;
	}
	if(cflags&DETACHED_PROCESS)
		detached = 1;
	if(title)
	{
		cp = title + strlen(title)+1;
		cp = cmdline + roundof((cp-cmdline),sizeof(int));
	}
	else
		cp = (char*)buffer;
	maxfd = setup_fdtable((int*)cp, sizeof(buffer), start, proc, &detached, &phandle, &xhandle, exeflags);
	if((cflags&DETACHED_PROCESS) && (cflags&CREATE_NEW_CONSOLE))
		cflags &= ~DETACHED_PROCESS;
	if(P_CP->inexec&PROC_INEXEC)
		proc->pid = P_CP->pid;
	/* prevent process from going away until pid is returned */
	InterlockedIncrement(&proc->inuse);
 retry:
	i = 0;
	if(sp && sp->tok)
	{
		proc->etoken = 0;
		proc->gid = proc->egid;
		proc->egid = -1;
		if (P_CP->etoken)
		{
			RevertToSelf();
			hastoimpersonate=1;
		}
		grantaccess(1); //Access to use Desktop Resources
		if(!CPU)
			CPU = (CPU_t)getsymbol(MODULE_advapi,"CreateProcessAsUserA");
		impersonate_user(sp->tok);
		if(CPU)
		{
			if(state.clone.hp)
				i= cl_createproc(sp->tok,cmdname,cmdline,NULL,&sattr,inherit,CREATE_SUSPENDED|cflags,env,NULL,start,&pinfo);
			else
				i= (*CPU)(sp->tok,cmdname,cmdline,NULL,&sattr,inherit,CREATE_SUSPENDED|cflags,env,NULL,start,&pinfo);
		}
		else
			SetLastError(ERROR_ACCESS_DENIED);
		RevertToSelf();
		if(hastoimpersonate)
			impersonate_user(P_CP->etoken);
		if(i==0)
			logerr(0, "createprocess as user failed ruid=%d euid=%d rgid=%d egid=%d etok=%p", getuid(), geteuid(), getgid(), getegid(), proc->etoken);
	}
	else
	{
		if(exeflags&EXE_GUI)
			curdir = state.sys;
		if(!(exeflags & EXE_FORK) && (suid&(S_ISUID|S_ISGID)) && issuidneeded(cmdname, suid) && (proc->etoken = createsuidproc(cmdname, cmdline, title, env, cflags, start, &pinfo)))
		{
			i = 1;
			if(proc->etoken == INVALID_HANDLE_VALUE)
				proc->etoken = 0;
		}
		if (P_CP->etoken)
		{
			RevertToSelf();
			hastoimpersonate=1;
		}
		if (!i)
		{
			if(uidset)
			{
				grantaccess(1); //Access to use Desktop Resources
				if(!CPU)
					CPU = (CPU_t)getsymbol(MODULE_advapi,"CreateProcessAsUserA");
				RevertToSelf();
				i= (*CPU)(P_CP->rtoken,cmdname,cmdline,&sattr,&sattr,1,CREATE_SUSPENDED|cflags,env,curdir,start,&pinfo);
				if(!i && P_CP->etoken)
				{
					/* see of etoken can do it */
					impersonate_user(P_CP->etoken);
					i= (*CPU)(P_CP->rtoken,cmdname,cmdline,&sattr,&sattr,1,CREATE_SUSPENDED|cflags,env,curdir,start,&pinfo);
					RevertToSelf();
				}
				impersonate_user(P_CP->rtoken);
			}
			else
			{
				i = CreateProcess(cmdname,cmdline,&sattr,&sattr,inherit,CREATE_SUSPENDED|cflags,env,curdir,start,&pinfo);
#ifdef ERROR_ELEVATION_REQUIRED
				if (!i && GetLastError() == ERROR_ELEVATION_REQUIRED)
				{
					Pproc_t*		pp;
					pid_t			pi;
					int			uac;
					char*			line;
					char			cmd[MAX_PATH];

					static const char	uacmd[] = "cmd /c ";
					static const char	uaexe[] = "\\cmd.exe";

					uac = 1;
					pi = P_CP->pid;
					while (uac && pi > 1 && (pp = proc_getlocked(pi, 0)))
					{
						if (streq(pp->prog_name, "ssh"))
							uac = 0;
						pi = pi > 1 && pp->posixapp ? pp->ppid : -1;
						proc_release(pp);
					}
					if ((uac || pi == 1) && (line = malloc(strlen(cmdname) + strlen(cmdline) + sizeof(uacmd) + 3)))
					{
						i = 0;
						for (ep = cmdline; *ep; ep++)
							if (*ep == '"')
								i = !i;
							else if (!i && *ep == ' ')
								break;
						cp = line;
						i = sizeof(uacmd) - 1;
						memcpy(cp, uacmd, i);
						cp += i;
						*cp++ = '"';
						i = (int)strlen(cmdname);
						memcpy(cp, cmdname, i);
						cp += i;
						*cp++ = '"';
						if (*ep)
						{
							i = (int)strlen(ep);
							memcpy(cp, ep, i);
							cp += i;
						}
						*cp = 0;
						sfsprintf(cmd, sizeof(cmd), "%s%s", state.sys, uaexe);
						logmsg(0, "ERROR_ELEVATION_REQUIRED %s '%s'", cmd, line);
						i = CreateProcess(cmd,line,&sattr,&sattr,inherit,CREATE_SUSPENDED|cflags,env,curdir,start,&pinfo);
						free(line);
					}
				}
#endif
			}
		}
		if(hastoimpersonate)
			impersonate_user(P_CP->etoken);
	}
	if(sp && (sp->flags&UWIN_TRACE_FLAGS) && proc->trace)
		closehandle(proc->trace,HT_FILE);
	if(i)
	{
		Pproc_t	*pp;
		char	*cpmin = cmdname;
		int	vdp = '.';
		logmsg(LOG_PROC+2, "run %s", cmdline);
		proc->ntpid = pinfo.dwProcessId;
		proc->ph = pinfo.hProcess;
		if(exeflags&EXE_UWIN)
			proc->posixapp |= UWIN_PROC;
		/* save handle to new process in new process in ph2 */
		if(!DuplicateHandle(me,pinfo.hProcess,pinfo.hProcess,&proc->ph2,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if (!((P_CP->inexec&PROC_INEXEC) && P_CP->pid==uwin_nt2unixpid(P_CP->ntpid)))

			SetHandleInformation(pinfo.hProcess, HANDLE_FLAG_INHERIT, 0);
		/* store the program name as last component of path */
		dp = 0;
		if(memicmp(cmdline,"sh ",3)==0 && cmdline[4]!='-')
		{
			cpmin = &cmdline[3];
			if(dp = strchr(cpmin,' '))
			{
				*dp = 0;
				vdp = ' ';
			}
		}
		cp = ep = cpmin+strlen(cpmin);
		for (;;)
		{
			if(cp <= cpmin)
			{
				if(vdp!='.')
					dp = 0;
				break;
			}
			if((i = *--cp)=='/' || i=='\\')
			{
				cp++;
				break;
			}
			if(i=='.' && !dp)
				dp = cp;
		}
		if (dp && *dp)
		{
			if ((ep - dp) == 4 && (dp[1] == 'e' || dp[1] == 'E') && (dp[2] == 'x' || dp[2] == 'X') && (dp[3] == 'e' || dp[3] == 'E'))
				*dp = 0;
			else
				dp = 0;
		}
		strncpy(proc->prog_name,cp,sizeof(proc->prog_name)-1);
		/* construct command line */
		if(argv)
			proc_setcmdline(proc,argv);
		if (dp)
			*dp = vdp;
		if(init)
		{
			proc->parent = proc_slot(proc);
			Share->init_slot = proc->parent;
			Share->init_ntpid = proc->ntpid;
			if(!DuplicateHandle(me,proc->ph,me,&Share->init_handle,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateInitHandle");
			pid = 1;
		}
		else
			pid = set_unixpid(proc);
		if((cflags&CREATE_NEW_CONSOLE) || (sp && sp->grp<0))
		{
			proc->console = 0;
			proc->sid = proc->pgid = (pid_t)pid;
		}
		else if(sp && (sp->grp==1 || sp->grp==-2))
		{
			proc->pgid = pid;
			if(sp->grp==-2)
				tcsetpgrp(2,pid);
		}
		if(!(P_CP->inexec&PROC_INEXEC))
		{
			proc->pid = (pid_t)pid;
			proc_addlist(proc);
		}
		if(init)
		{
			proc->ppid = 1;
			proc->pgid = pid;
			sigcheck(-1);
			ResumeThread(pinfo.hThread);
			closehandle(pinfo.hThread,HT_THREAD);
			return(proc);
		}
		fdp = getfdp(P_CP,0);
		if(fdp->type==FDTYPE_CONSOLE && fdp->devno>0 && dev_ptr(fdp->devno)->state==1)
		{
			DWORD mode;
			if(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode))
			{
				dev_ptr(fdp->devno)->state = 0;
				SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),(mode|SANE_MODE));
			}
		}
		/*
		 * Saving the Primary thread of the new process for
		 * later use to suspend the primary thread
		 * This causes an exception for native DOS processes on 9X
		 * so don't do it in this case.
		 */
		hp = 0;
		if(Share->Platform==VER_PLATFORM_WIN32_NT || check_cmd(cmdname))
		{
			if(!DuplicateHandle(me,pinfo.hThread,pinfo.hProcess,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
		}
		proc->thread = hp;
		if((P_CP->inexec&PROC_INEXEC) && !P_CP->vfork)
		{
			proc->phandle = pinfo.hThread;
			goto done;
		}
		lastproc = proc;
		i = proc_slot(proc);
		pp = P_CP->vfork?((struct vstate*)P_CP->vfork)->oproc:P_CP;
		if(exeflags & EXE_FORK)
		{
			begin_critical(CR_MALLOC);
			fork_err=50;
			if(major_version>=NEWFORK_VERSION)
			{
				void*		addr;
				child_proc = proc->ph;
				free(malloc(1024));
				vmcompact(Vmregion);
				if(vmwalk(0,vcopy)==-1)
				{
					logmsg(0,"fork_vcopy failure");
					logmsg(LOG_PROC+2,"parent=%(memory)s",GetCurrentProcess());
					logmsg(LOG_PROC+2,"child=%(memory)s",child_proc);
				terminate:
					end_critical(CR_MALLOC);
					SetLastError(ERROR_TOO_MANY_CMDS);
					errno = EAGAIN;
					TerminateProcess(pinfo.hProcess,126);
					closehandle(pinfo.hProcess,HT_PROC);
					closehandle(pinfo.hThread,HT_THREAD);
					proc->state = PROCSTATE_TERMINATE;
					stop_proc(proc);
					return(0);
				}
				fork_err++;
				if(addr = dllinfo._ast__argv)
				{
					MEMORY_BASIC_INFORMATION	minfo;

					if(VirtualQueryEx(me,addr,&minfo,sizeof(minfo))!=sizeof(minfo))
						logerr(0, "VirtualQueryEx addr=%08x", addr);
					else if(copytochild(minfo.BaseAddress, minfo.RegionSize)<0)
					{
						logerr(0,"copytochild minfo failed");
						if(addr!=arg_argv)
						{
							arg_argv = addr;
							goto terminate;
						}
					}
				}
				fork_err++;
				if((addr = arg_getbuff()) && copytochild((void*)(((char*)addr-(char*)0)&~(PAGESIZE-1)), Share->argmax+ARG_SLOP)<0)
				{
					logmsg(0,"copytochild arg_buff failed");
					goto terminate;
				}
			}
			logmsg(LOG_PROC+6, "parent %(memory)s", GetCurrentProcess());
			logmsg(LOG_PROC+6, "child %(memory)s", proc->ph);
		}
		post_wait_thread(pp,i);
		if((exeflags & EXE_FORK) || P_CP->vfork)
		{
			/*
			 * TEMP HACK
			 * Share->Version = (MajorVersion<<8) | MinorVersion
			 * On Windows XP this causes ssh connections to
			 * immediately terminate. Workaround is to
			 * correct thread handle loss is to have different
			 * implementation for XP vs later windows versions.
			 * MajorVersion
			 *    5         Server 2003, XP, and 2000
			 *    6         Vista, Server 2008 & 2012, Windows7 & 8
			 */
			if ((Share->Version>>8) > 5)
			{
				/* Windows 7 and later systems */
				if(!(hp = OpenThread(THREAD_QUERY_INFORMATION|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT, FALSE, pinfo.dwThreadId)) || hp == INVALID_HANDLE_VALUE)
				{
					logerr(0, "start_proc OpenThread failed");
					return(0);
				}
				closehandle(pinfo.hThread, HT_THREAD);
				proc->phandle = hp;
				pinfo.hThread = hp;
			}
			else
			{
				/* Windows XP based systems */
				proc->phandle = pinfo.hThread;
			}
			goto done;
		}
		ResumeThread(pinfo.hThread);
		closehandle(pinfo.hThread,HT_THREAD);
	}
	else
	{
		int err = GetLastError();
		if(ntry++==0 && (err==ERROR_BAD_FORMAT || err==ERROR_BAD_EXE_FORMAT))
		{
			Path_t info;
			info.oflags = GENERIC_READ;
			memcpy(cmdline-=3,"sh ",3);
			if(cmdname=pathreal("/usr/bin/ksh",P_EXIST|P_FILE|P_NOHANDLE,&info))
			{
				if(P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
				{
					cflags |= CREATE_NO_WINDOW;
					detached = 1;
					exeflags |= EXE_UWIN;
				}
				goto retry;
			}
		}
		if(exeflags & EXE_FORK)
		{
			xinfo.cb = sizeof(xinfo);
			logerr(0,"forkfail cmdname=%s cmdline=%s env=%p curdir=%s cflags=0x%x suid=0x%x uidset=%d mcmp=%d",cmdname,cmdline,env,curdir,cflags,suid,uidset,memcmp(start,&xinfo,sizeof(xinfo)));
		}
		if(err!=ERROR_BAD_FORMAT && err!= ERROR_BAD_EXE_FORMAT)
			logerr(LOG_PROC+3, "CreateProcess %s failed", cmdname);
		errno = unix_err(err);
		stop_proc(proc);
		proc = 0;
		SetLastError(err);
		return(0);
	}
 done:
	pp = (P_CP->inexec&PROC_INEXEC) && !(P_CP->vfork) ? P_CP : proc;
	if(xhandle)
		for(i=0; i < maxfd; i++)
		{
			fdp = getfdp(P_CP,i);
			if(!iscloexec(i) && (fdp->type==FDTYPE_EPIPE||fdp->type==FDTYPE_EFIFO) && ((fdp->oflag&O_ACCMODE)!=O_RDONLY))
			{
				if(fdp->oflag&O_RDWR)
				{
					memcpy(&hp,xhandle,sizeof(HANDLE));
					if (!closehandle(hp,HT_PIPE))
						logerr(0, "closehandle X %2d/%-2d %p", i, maxfd, hp);
				}
				else
				{
					memcpy(&hp,phandle,sizeof(HANDLE));
					if(!closehandle(hp,HT_PIPE))
						logerr(0, "closehandle P %2d/%-2d %p fdp=%x slot=%d %(fdtype)s ref=%d index=%x", i, maxfd, hp, fdp, file_slot(fdp), fdp->type, fdp->refcount, fdp->index);
				}
			}
			phandle += sizeof(HANDLE);
			xhandle += sizeof(HANDLE);
		}
	if(proc && sp)
		sp->handle = proc->ph;
	return(proc);
}

void init_init(void)
{
	HANDLE			me;
	HANDLE			eh;
	HANDLE			ph;
	Pproc_t*		proc;
	char*			av[2];
	struct spawndata	sdata;
	char			name[UWIN_RESOURCE_MAX];
	char			cmdname[PATH_MAX];

	int			maxfd = P_CP->maxfds;

	UWIN_EVENT_INIT_START(name);
	if (!(eh = CreateEvent(0, TRUE, FALSE, name)))
		logerr(0, "init %s create failed", name);
	Share->init_state = UWIN_INIT_START;
	uwin_pathmap(state.install ? "/init" : "/usr/etc/init", cmdname, sizeof(cmdname), UWIN_U2W);
	if (GetFileAttributes(cmdname) == INVALID_FILE_ATTRIBUTES && !GetModuleFileName(NULL, cmdname, sizeof(cmdname)))
	{
		logerr(0, "cannot find init executable");
		return;
	}
	logmsg(LOG_INIT+1, "init executable %s", cmdname);
	ZeroMemory(&sdata, sizeof(sdata));
	sdata.flags = DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP;
	P_CP->maxfds = 0;
	av[0] = "/etc/init";
	av[1] = 0;
	proc = start_proc(NULL, cmdname, "init", av, NULL, &sdata, 0, EXE_UWIN, 1);
	P_CP->maxfds = maxfd;
	if (!proc)
	{
		logerr(0, "init start_proc failed");
		return;
	}
	InterlockedDecrement(&proc->inuse);
	me = GetCurrentProcess();
	/* duplicate my process handle into init */
	if (!DuplicateHandle(me, proc->ph, proc->ph, &ph, 0, FALSE, DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
	{
		logerr(0, "DuplicateHandle %p => %p", me, proc->ph);
		if (!(me = OpenProcess(PROCESS_ALL_ACCESS, FALSE, P_CP->ntpid)))
			logerr(0, "OpenProcess");
		else
		{
			if (!DuplicateHandle(me, proc->ph, proc->ph, &ph, 0, FALSE, DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
				logerr(0, "DuplicateHandle %p => %p", me, proc->ph);
			closehandle(me, HT_PROC);
		}
	}
	if (Share->init_serial++)
	{
		Share->nblockops |= (1<<CLEANUP_SHIFT)-1;
		Share->nblockops -= 100;
	}
	switch (eh ? WaitForSingleObject(eh, INIT_START_TIMEOUT) : WAIT_FAILED)
	{
	case WAIT_OBJECT_0:
		break;
	case WAIT_TIMEOUT:
		logmsg(0, "WaitForSingleObject %s timed out", name);
		break;
	default:
		logmsg(0, "WaitForSingleObject %s failed", name);
		Sleep(INIT_START_TIMEOUT);
		break;
	}
	if (eh)
		CloseHandle(eh);
	logmsg(0, "init serial=%d install=%d", Share->init_serial, state.install);
	state.init_handle = proc->ph;
}

/*
 * return handle to init proc
 * (re)start init if not already running
 */

HANDLE init_handle(void)
{
	if ((state.init_handle = Share->init_handle) && GetProcessId(state.init_handle) == Share->init_ntpid)
		return state.init_handle;
	return state.init_handle = open_proc(Share->init_ntpid, PROCESS_DUP_HANDLE|PROCESS_VM_WRITE|PROCESS_VM_OPERATION);
}

void stop_wait(void)
{
	DWORD status=0;
	if(thwait)
	{
		ResetEvent(waitevent);
		if(state.clone.ev)
		{
			state.clone.msg = 99<<16;
			SetEvent(state.clone.ev);
		}
		else if(!PostThreadMessage(P_CP->wait_threadid,WM_wait,99,0))
			logerr(0,"PostThreadMessage");
		if(WaitForSingleObject(thwait,5000)==WAIT_TIMEOUT)
		{
			logmsg(0, "stop_wait timeout");
			if(!terminate_thread(thwait, 0))
				logerr(0, "terminate_thread=%04x", thwait);
		}
		GetExitCodeThread(thwait,&status);
	}
	P_CP->wait_threadid = 0;
	thwait = 0;
}

/*
 * copy string into <sp> and return strlen+1 on success, 0 on failure
 */
static int mycopy(char *sp, const char *str, size_t size)
{
	size_t	len = strlen(str)+1;
	if(len >=size)
		return(0);
	memcpy(sp,str,len);
	return (int)len;
}

static Pproc_t *launch(const char *path,char *const argv[],char *const envp[],struct spawndata *sdp)
{
	void *env=0;
	Pproc_t *pp;
	char buff[PATH_MAX+1];
	char *sp=0, *suffix=0, *cmdname, *shell=0, *argv0=0, *argv1=0, *null=0;
	Path_t info;
	int sigsys, i, l, c, dos=0, envflags, exeflags;
	char *cmdline;
	info.oflags = GENERIC_READ;
	if(!path)
	{
		errno = EFAULT;
		return(0);
	}
	c = P_EXIST|P_FILE|P_EXEC;
	if(state.clone.hp && *path=='.' && path[1]=='/')
		path+=2;
	cmdname = pathreal(path,c,&info);
	if(!cmdname)
		return(0);
	if(!argv)
		argv = &null;
	l = (int)strlen(cmdname);
	for(i=0; i < 4; i++,l--)
	{
		if((c=cmdname[l-1])=='.')
		{
			if(i==2 && fold(cmdname[l])=='s' && fold(cmdname[l+1])=='h')
			{
				sp = (char*)&cmdname[l];
				if(sdp && P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
					sdp->flags |= CREATE_NO_WINDOW;
			}
			else if(i==3)
			{
				if((fold(cmdname[l])=='e' && fold(cmdname[l+1])=='x' && fold(cmdname[l+2])=='e') || (fold(cmdname[l])=='b' && fold(cmdname[l+1])=='a' && fold(cmdname[l+2])=='t') || (fold(cmdname[l])=='k' && fold(cmdname[l+1])=='s' && fold(cmdname[l+2])=='h') && P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR && (sdp && (sdp->flags |= CREATE_NO_WINDOW)))
				sp = (char*)&cmdname[l];
			}
			break;
		}
		else if(c=='\\')
			break;
	}
	sigsys = sigdefer(1);
	begin_critical(CR_SPAWN);
	if(!sp && info.hp)
	{
		DWORD nread;
		/* check for #! interpreter */
		if(ReadFile(info.hp, buff,PATH_MAX,&nread,NULL) && nread>2 && buff[0]=='#' && buff[1]=='!')
		{
			buff[nread] = 0;
			if(sp = strchr(buff,'\n'))
				*sp = 0;
			if(sp[-1]=='\r')
				*--sp = 0;
			sp = buff+2;
			while(*sp==' ' || *sp=='\t')
				sp++;
			shell = sp;
			while(*sp && *sp!=' ' && *sp!='\t')
				sp++;
			if(*sp)
			{
				*sp++ = 0;
				while(*sp==' ' || *sp=='\t')
					sp++;
				if(*sp)
					argv1 = sp;
			}
			argv0 = (char*)path;
			cmdname = 0;
			suffix = "";
			if(sdp && P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
				sdp->flags |= CREATE_NO_WINDOW;
		}
		goto skip;
	}
	if(sp && !((fold(sp[0]) == 'e') && (fold(sp[1]) == 'x') && (fold(sp[2]) == 'e')))
	{
		suffix = "";
		l = (int)(strlen(path) - strlen(sp));
		if(l<=1 || strcmp(&sp[-1],&path[l-1]))
			suffix = sp;
		if ((sp[0] == 'b' || sp[0] == 'B') && (sp[1] == 'a' || sp[1] == 'A') && (sp[2] == 't' || sp[2] == 'T'))
		{
			if(Share->Platform>=VER_PLATFORM_WIN32_NT)
			{
				sfsprintf(buff, sizeof(buff), "%s\\cmd.exe", state.sys);
				shell = "cmd /q /c";
			}
			else
			{
				strcpy(buff, state.sys);
				i = (int)strlen(buff);
				memcpy(&buff[i-6],"command.com",12);
				shell = "command /c";
			}
			cmdname = buff;
			dos = 1;
		}
	}
	else if(Share->Platform!=VER_PLATFORM_WIN32_NT && info.hp)
	{
		/* if dos file then set suffix to Exe, otherwise exe */
		HANDLE hp;
		unsigned char *addr;
		sp[0] = 'e';
		if(hp=CreateFileMapping(info.hp,NULL,PAGE_READONLY,0, 0x1000, NULL))
		{
			if(addr=(unsigned char*)MapViewOfFileEx(hp,FILE_MAP_READ, 0,0,0, NULL))
			{
				if(addr[2] != 0220)
					sp[0] = 'E';
				if(!UnmapViewOfFile((void*)addr))
					logerr(0, "UnmapViewOfFile");
			}
			else
				logerr(0, "MapViewOfFileEx");
			closehandle(hp,HT_MAPPING);
		}
		else
			logerr(0, "CreateFileMapping");
	}
	if(suffix && !shell)
	{
		shell = buff;
		if(envp)
		{
			for (i = 0; envp[i]; i++)
			{
				if (strncmp (envp[i], "SHELL=", 6) == 0)
				{
					strncpy (buff, &envp[i][6],sizeof(buff));
					cmdname = 0;
					argv0 = (char*)path;
					if(sdp && P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
						sdp->flags |= CREATE_NO_WINDOW;
					goto skip;
				}
			}
			suffix = 0;
		}
		else
		{
			if(GetEnvironmentVariable("SHELL",buff,sizeof(buff))==0)
				shell = "/usr/bin/ksh";
			cmdname = 0;
			argv0 = (char*)path;
			if(sdp && P_CP->console && dev_ptr(P_CP->console)->major==PTY_MAJOR)
				sdp->flags |= CREATE_NO_WINDOW;
		}
	}
skip:
	cmdline = arg_getbuff();
	if(!cmdline)
		goto err;
	/* save room for prepending sh */
	cmdline += 3;
	l = Share->argmax + ARG_SLOP-1;
	if(!(c=getcmdline(cmdline,argv,suffix,shell,argv0,argv1,dos)))
	{
		errno = E2BIG;
		goto err;
	}
	sp = cmdline+c;
	if(!cmdname)
	{
		closehandle(info.hp,HT_FILE);
		if(!(cmdname=pathreal(shell,P_EXIST|P_FILE|P_EXEC,&info)))
			goto err;
	}
	exeflags = exetype(info.hp, cmdname);
	envflags = UWIN_U2W|(((exeflags & EXE_32) && (state.wow || sizeof(char*) == 8)) ? UWIN_WOW : 0);
	l -=c;
	if((c = mycopy(sp,cmdname,l))==0)
	{
		errno = E2BIG;
		goto err;
	}
	cmdname = sp;
	sp +=c;
	l -= c;
	if(envp)
	{
		if(!(sp=packenv(env=sp, l, envp, envflags)))
		{
			errno = E2BIG;
			goto err;
		}
	}
	else if (*dllinfo._ast_environ != saved_env)
	{
		if(!(sp=packenv(env=sp, l-c, *dllinfo._ast_environ, envflags)))
		{
			errno = E2BIG;
			goto err;
		}
	}
	if(env)
		l -= (int)(sp-(char*)env);
	if(mycopy(sp,path,l)==0)
	{
		errno = E2BIG;
		goto err;
	}
	pp = start_proc(sp, cmdname, cmdline, argv, env, sdp, info.mode&(S_ISUID|S_ISGID), exeflags, 0);
	end_critical(CR_SPAWN);
	return(pp);
err:
	if(info.hp)
		closehandle(info.hp,HT_FILE);
	end_critical(CR_SPAWN);
	sigcheck(sigsys);
	return(NULL);
}

__EXPORT__ pid_t spawnvetok(const char *path,char *const argv[],char *const envp[],HANDLE tok)
{
	struct spawndata sdata;
	Pproc_t *pp;
	pid_t pid = -1;
	ZeroMemory(&sdata,sizeof(sdata));
	sdata.tok = tok;
	if(pp = launch(path,argv,envp,&sdata))
	{
		pid = pp->pid;
		proc_release(pp);
	}
	sigcheck(0);
	return(pid);
}

pid_t spawnveg(const char *path,char *const argv[],char *const envp[],pid_t grp)
{
	struct spawndata sdata;
	Pproc_t *pp;
	pid_t pid = -1;
	ZeroMemory(&sdata,sizeof(sdata));
	sdata.grp = grp;
	if(pp = launch(path,argv,envp,&sdata))
	{
		pid = pp->pid;
		proc_release(pp);
	}
	sigcheck(0);
	return(pid);
}


pid_t spawnve(const char *path,char *const argv[],char *const envp[])
{
	Pproc_t *pp;
	pid_t pid = -1;
	if(pp = launch(path,argv,envp,NULL))
	{
		pid = pp->pid;
		proc_release(pp);
	}
	sigcheck(0);
	return(pid);
}

int	*_ast_vfork(void)
{
	struct vstate *vp = (struct vstate*)malloc(sizeof(struct vstate));
	Pproc_t *pp;
	int *addr = dllinfo._ast_exitfn;
	if(Share->vforkpid < 0)
	{
		logmsg(0,"vforkpid=%d",Share->vforkpid);
		Share->vforkpid = 0;
	}
	InterlockedIncrement(&Share->vforkpid);
	pp = proc_init(2+Share->vforkpid,0);
	vp->oproc = P_CP;
	sig_saverestore((void*)vp->sigfun,(void*)vp->sigmasks,0);
	pp->vfork = (void*)vp;
	pp->sevent = P_CP->sevent;
	pp->sigevent = P_CP->sigevent;
	pp->siginfo.siggot = 0;
	proc_addlist(pp);
	P_CP = pp;
	return((int*)vp);
}

void	_vfork_done(Pproc_t *proc,int exitval)
{
	register Pproc_t *pp = P_CP;
	register int i;
	pid_t pid;
	struct vstate vs;
	if(!P_CP->vfork)
		return;
	pid = P_CP->pid;
	vs = *((struct vstate*)P_CP->vfork);
	sig_saverestore((void*)vs.sigfun,(void*)vs.sigmasks,1);
	free(pp->vfork);
	InterlockedDecrement(&Share->vforkpid);
	if(Share->vforkpid<0)
	{
		logmsg(0, "vforkpid<0");
		Share->vforkpid = 0;
	}
	fdcloseall(P_CP);
	if(P_CP->fdtab)
		free(P_CP->fdtab);
	P_CP = vs.oproc;
	for(i=0; i < P_CP->maxfds; i++)
		if(fdslot(P_CP,i)>=0 && !iscloexec(i))
		{
			Pfd_t *fdp = getfdp(P_CP,i);
			Phandle(i) = Fs_dup(i,fdp,&Xhandle(i),3);
		}
	pp->exitcode = (unsigned short)(exitval);
	logmsg(LOG_PROC+3, "_vfork_done exitcode 0x%x", pp->exitcode);
	if(proc)
	{
		HANDLE child = proc->phandle;
		proc->phandle = 0;
		pid = proc->pid = nt2unixpid(proc->ntpid);
		proc->ppid = P_CP->pid;
		pp->inexec &= ~PROC_INEXEC;
		proc_release(pp);
		proc_addlist(proc);
		P_CP->inexec &= (PROC_DAEMON|PROC_MESSAGE);
		proc->siginfo.sigsys = 0;
		sigcheck(-1);
		ResumeThread(child);
		closehandle(child,HT_THREAD);
	}
	else if(!SigIgnore(P_CP,SIGCHLD))
	{
		pp->state=PROCSTATE_ZOMBIE;
		sigsetgotten (P_CP,SIGCHLD, 1);
		SetEvent(P_CP->sevent);
		pp->notify = 1;
	}
	else
	{
		pp->state = PROCSTATE_EXITED;
		proc_release(pp);
	}
	longjmp(vs.jmpbuf,pid);
}

static void uwin_exec(const char *path, char *const argv[], char *const envp[], struct spawndata *sp)
{
	char ename[UWIN_RESOURCE_MAX];
	unsigned char ch[MAX_PTY_MINOR/8+1];
	register Pproc_t *proc;
	register int i;
	int sigsys, noaccess=0;
	DWORD ntpid;
	HANDLE hp,child,ph,me=GetCurrentProcess();
#ifdef GTL_TERM
	HANDLE ep=0;
#endif
	ZeroMemory(ch,sizeof(ch));
	if(P_CP->inexec&PROC_INEXEC)
	{
		if(proc = proc_getlocked(P_CP->pid, 0))
		{
			if(proc!=P_CP)
				logmsg(0,"proc!=P_CP proc=%d use=%d",proc_slot(proc),proc->inuse);
			logmsg(0, "inexec flag should be zero value=%o inuse=%d vfork=%x", P_CP->inexec,P_CP->inuse,P_CP->vfork);
			proc_release(proc);
			Sleep(1000);
			if(P_CP->inexec&PROC_INEXEC)
				logmsg(0, "again inexec flag should be zero value=%o", P_CP->inexec);
		}
	}
	P_CP->inexec |= PROC_INEXEC;
	if(!(proc=launch(path, argv, envp,sp)))
	{
		P_CP->inexec &= (PROC_DAEMON|PROC_MESSAGE);
		sigcheck(0);
		return;
	}
	proc_release(proc);
	if(P_CP->vfork)
	{
		_vfork_done(proc,0);
		return;
	}
#ifdef GTL_TERM
	if(!DuplicateHandle(me,proc->ph,me,&ep,0,FALSE,DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle");
#endif
	child = proc->phandle;
	sigsys = sigdefer(1);
	proc->phandle = 0;
	if(thwait && SuspendThread(thwait)==(DWORD)(-1))
		logerr(0, "SuspendThread th=%p", thwait);
	if(Share->init_slot==0)
	{
		logmsg(0, "before init");
		Sleep(500);
	}
	ph = P_CP->ph;
	DuplicateHandle(me,me,proc->ph,&P_CP->ph2,0,FALSE,DUPLICATE_SAME_ACCESS);
	wait_acknowledge(P_CP,0,__LINE__);
	ntpid = P_CP->ntpid;
	P_CP->ntpid = proc->ntpid;
	P_CP->inexec &= PROC_DAEMON;
	proc_exec_exit = 1;
	P_CP->phandle = 0;
	if(!isconsole_child(P_CP))
	{
		int code=0;
		DWORD status, ntparent=proc_ntparent(P_CP);
		HANDLE pproc = open_proc(ntparent,PROCESS_QUERY_INFORMATION);
		if(pproc)
		{
			wait_acknowledge(P_CP,0,__LINE__);
			code = P_CP->exitcode;
			if(GetExitCodeProcess(pproc,&status) && status==STILL_ACTIVE)
			{
				if(DuplicateHandle(me,proc->ph,pproc,&P_CP->ph,0,FALSE,DUPLICATE_SAME_ACCESS))
				{
					if(P_CP->ppid==1 && ntparent!=Share->init_ntpid && Share->init_ntpid)
						status = 0;
					else if(P_CP->ppid!=1 || code)
						P_CP->inexec |= PROC_HAS_PARENT;
				}
				else
				{
					logerr(0,"DuplicateHandle ph=%p ntpid=%x-%x ntpidph=%p",proc->ph,GetProcessId(pproc),ntparent,GetProcessId(proc->ph));
					status = 0;
				}
			}
			if(status!=STILL_ACTIVE)
			{
				P_CP->inexec |= PROC_MESSAGE;
				P_CP->parent = Share->init_slot;
				P_CP->ppid = 1;
				P_CP->ph = proc->ph;
				if(dup_to_init(P_CP,Share->init_slot))
				{
					InterlockedIncrement(&P_CP->inuse);
					P_CP->inexec |= PROC_HAS_PARENT;
					send2slot(Share->init_slot, 0, proc_slot(P_CP));
				}
				else
					logerr(0,"dup_to_init proc=%d",proc_slot(proc));
			}
			P_CP->inexec &= ~PROC_MESSAGE;
			closehandle(pproc,HT_PROC);
		}
		else
			logerr(0, "OpenProcess block=%d parent=0x%x state=%d",P_CP->parent,ntparent);
	}
	/*
	 * currently child processes are passed to init
	 * in the future they should probably stay with
	 * the new process
	 */
	if(P_CP->child)
		proc_orphan(P_CP->pid);
	proc_setntpid(P_CP,ntpid);
	P_CP->admin = 0;
	P_CP->siginfo.siggot |= proc->siginfo.siggot;
	P_CP->child = 0;
	P_CP->posixapp &= ~UWIN_PROC;
	P_CP->posixapp |= (proc->posixapp&UWIN_PROC);
	P_CP->trace = proc->trace;
	P_CP->traceflags = proc->traceflags;
	P_CP->wait_threadid = GetCurrentProcessId();
    if (P_CP->sevent) CloseHandle(P_CP->sevent);
    if (P_CP->sigevent) CloseHandle(P_CP->sigevent);
	P_CP->sigevent = P_CP->sevent = 0;
	if(proc==P_CP)
		logmsg(0,"proc==P_CP proc=%d",block_slot(proc));
	proc->pid = 1;
	for(i=0; i < proc->maxfds; i++)
	{
		if(fdslot(P_CP,i)>=0)
		{
			Pfd_t *fdp = getfdp(P_CP,i);
			if(iscloexec(i))
				close(i);
			else
			{
				decr_refcount(fdp);
				if(fdp->type==FDTYPE_PTY || fdp->type==FDTYPE_CONSOLE || fdp->type==FDTYPE_TTY)
				{
					Pdev_t *pdev = dev_ptr(fdp->devno);
#ifdef GTL_TERM
					if(pdev->NtPid==ntpid)
					{
//////////////////////////////////////////////////////////////
// Reset the device if it is a master, otherwise, suspend it.
//////////////////////////////////////////////////////////////
						if(fdp->type == FDTYPE_PTY && pdev->io.master.masterid)
							reset_device(pdev);
						else
							suspend_device(pdev,READ_THREAD|WRITE_THREAD);
						pdev_dup(me,pdev,ep,pdev);
						pdev->NtPid = proc->ntpid;
						pdev->state=PDEV_REINITIALIZE;
					}
#else
					if(pdev->NtPid==ntpid && !(fdp->type==FDTYPE_PTY && pdev->Pseudodevice.master))
					{
						dev_duphandles(pdev,me,proc->ph);
						pdev->NtPid = proc->ntpid;
						pdev->started = 0;
					}
#endif
				}
			}
		}
	}
	sync_io();
	if(proc->curdir>=0)
		fdpclose(&Pfdtab[proc->curdir]);
	if(proc->rootdir>=0)
		fdpclose(&Pfdtab[proc->rootdir]);
	for(i=0; i < elementsof(proc->slotblocks); i++)
		if(proc->slotblocks[i])
			block_free(proc->slotblocks[i]);
	proc->maxfds = 0;
	ipcexit(proc);
	strcpy(P_CP->prog_name, proc->prog_name);
	memcpy(P_CP->cmd_line, proc->cmd_line, sizeof(proc->cmd_line));
	memcpy(P_CP->args, proc->args, P_CP->argsize = proc->argsize);
	hp = proc->ph;
	proc->ph = 0;
	if(P_CP->etoken)
		P_CP->gid = getegid();
	P_CP->etoken = proc->etoken;
	proc->siginfo.sigsys = 0;
	proc->pid = 0;
	proc_release(proc);
	if(isconsole_child(P_CP) || noaccess)
	{
		DWORD	rval;
		/* parent process is not a u/win process, wait for completion */
		/* close console handles and terminate threads */
#ifndef GTL_TERM
		if(P_CP->console>0)
			term_close(dev_ptr(P_CP->console));
#endif
		if(thwait)
			terminate_thread(thwait, 0);
		P_CP->inexec &= ~PROC_HAS_PARENT;
		if(!DuplicateHandle(me,hp,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		P_CP->ph = ph;
		proc->siginfo.sigsys = 0;
		sigcheck(-1);
		if(!ResumeThread(child))
			logerr(0, "ResumeThread tid=%04x", child);
		rval=WaitForSingleObject(hp,INFINITE);
		if(!GetExitCodeProcess(hp,&rval))
			logerr(0, "GetExitCodeProcess");
		proc_exec_exit = 1;
		logmsg(LOG_PROC+3, "uwin_exec ExitProcess 0x%x", rval);
		ExitProcess(rval);
	}
	if(P_CP->inexec&PROC_HAS_PARENT)
	{
		ph = hp;
		hp = 0;
		if(P_CP->posixapp&UWIN_PROC)
		{
			UWIN_EVENT_EXEC(ename, P_CP->pid);
			if(!(hp = CreateEvent(sattr(0), TRUE, FALSE, ename)))
				logerr(0, "CreateEvent %s failed", ename);
			if(ph && hp && !DuplicateHandle(me,hp,ph,&P_CP->phandle,0,FALSE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
		}
		if(ph)
			closehandle(ph,HT_PROC);
		if(hp)
			closehandle(hp,HT_EVENT);
	}
	sigcheck(-1);
	if(!ResumeThread(child))
		logerr(0, "ResumeThread tid=%04x", child);
	TerminateProcess(GetCurrentProcess(),0);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
	uwin_exec(path,argv,envp,NULL);
	return(-1);
}

__EXPORT__ pid_t uwin_spawn(const char *path,char *const argv[],char *const envp[],struct spawndata *sp)
{
	Pproc_t *pp;
	pid_t pid = -1;
	if(sp && (sp->flags&UWIN_SPAWN_EXEC))
	{
		uwin_exec(path,argv,envp,sp);
		return(-1);
	}
	if(pp=launch(path,argv,envp,sp))
	{
		pid = pp->pid;
		proc_release(pp);
	}
	sigcheck(0);
	return(pid);
}


static pid_t waitcommon(pid_t pid, siginfo_t *infop, int options,struct rusage *rp)
{
	Pproc_t *proc=0;
	DWORD 	timeout= 20;
	char haschild;
	Pdev_t *pdev=0;
	int ntry,num,interrupted=0;
	struct tms tm;
	HANDLE objects[2];
	if(options & ~(WEXITED|WSTOPPED|WCONTINUED|WNOHANG|WNOWAIT|WUNTRACED))
	{
		errno = EINVAL;
		return( -1 );
	}
	if(pid == 0)
		pid = 0 - getpgrp();
again:
	while(1)
	{
		int r,sigsys;
		num = eventnum;
		haschild = 0;
		if (pid > 0)
		{
			if(proc = proc_getlocked(pid, 1))
				proc_release(proc);
			if(!proc || proc->ppid!=P_CP->pid)
			{
				if (!proc)
					logmsg(0, "waitpid=%d proc=%p", pid, proc);
				else
					logmsg(0, "waitpid=%d proc=%p slot=%d inuse=%d pid=%x-%d ppid=%d P_CP=%d state=%(state)s name=%s cmd=%s", pid, proc, proc_slot(proc), proc->inuse, proc->ntpid, proc->pid, proc->ppid, P_CP->pid, proc->state, proc->prog_name, proc->cmd_line);
				errno = ECHILD;
				return(-1);
			}
			logmsg(LOG_PROC+4, "waitproc=%d exited=%d state=%(state)s, notify=%d", proc->pid, procexited(proc), proc->state, proc->notify);
			if((proc->state==PROCSTATE_STOPPED) && (options&WUNTRACED) && proc->notify)
				proc->notify = 0;
			else if(!procexited(proc) || !proc->notify)
			{
#if 0
				proc_release(proc);
#endif
				proc = 0;
			}
		}
		else
		{
			register short *next;
			int count=256;
			short s;
			for(next= &P_CP->child; (s= *next) && count-->0; next= &proc->sibling)
			{
				proc = proc_ptr(s);
				if(proc->inuse<0)
					continue;
				if(proc->ppid==P_CP->pid)
				{
					if ((pid == -1) || (proc->pgid==(-pid)))
					{
						haschild = 1;
						if((proc->state==PROCSTATE_STOPPED) &&
							(options&WUNTRACED) &&
							proc->notify)
						{
							proc->notify=0;
							goto found;
						}
						if(procexited(proc) && proc->notify)
							goto found;
					}
				}
			}
			if(count==0)
				logmsg(0, "waitcommon infinite child loop pid=%x-%d name=%s", P_CP->ntpid, P_CP->pid, P_CP->prog_name);
			proc = 0;
		found:
			if (!haschild)
			{
				errno = ECHILD;
				return(-1);
			}
		}
		if(proc)
		{
			logmsg(LOG_PROC+4, "procfound=%d", proc->pid);
			if (pid == -1 && proc->ppid == getpid ())
				break;
			if ((pid > 0) && (proc->pid == pid))
				break;
			if ((pid < -1) && (proc->pgid == (-pid)))
				break;
		}
		else if(interrupted)
		{
			errno = EINTR;
			return(-1);
		}
		else if (options & WNOHANG) /* Not completed */
		{
			/* give process a head start */
			Sleep(1);
			return(0);
		}
		P_CP->state = PROCSTATE_CHILDWAIT;
		sigsys = sigdefer(1);
		ResetEvent(waitevent);
		objects[0] = P_CP->sigevent;
		objects[1] = waitevent;
		r = 1;
		ntry = 0;
		while(eventnum<=num)
		{
			r = WaitForMultipleObjects(2,objects,FALSE,timeout);
			if(r!=WAIT_TIMEOUT)
				break;
			else if(timeout==10000 && (eventnum>num) && ntry++>1)
				logmsg(0, "waitcommon timeout eventnum=%d num=%d sigpending=0x%x ntry=%d", eventnum, num, sigispending(P_CP), ntry);
			timeout = 10000;
		}
		if(r==WAIT_OBJECT_0)
			ResetEvent(P_CP->sigevent);
		SetEvent(waitevent);
		P_CP->state = PROCSTATE_RUNNING;
		logmsg(LOG_PROC+3, "waitdone");
		if(!sigcheck(sigsys))
		{
			interrupted = 1;
			continue;
		}
	}
	if(!proc)
	{
		logmsg(0, "waitcommon: proc==0");
		goto again;
	}
	pid = proc->pid;
	infop->si_code = proc->exitcode;
	infop->si_signo = SIGCHLD;
	infop->si_pid = pid;
	proc_times(proc,&tm);
	P_CP->cutime += tm.tms_utime + tm.tms_cutime;
	P_CP->cstime += tm.tms_stime + tm.tms_cstime;
	if(rp)
	{
		clock2timeval(tm.tms_utime,&rp->ru_utime);
		clock2timeval(tm.tms_stime,&rp->ru_stime);
	}
	if(proc->state == PROCSTATE_STOPPED || (options&WNOWAIT))
		return(pid);
	proc->state = PROCSTATE_EXITED;
#if 0
	if(ph=proc->ph)
	{
		if(!closehandle(ph,HT_PROC))
			logerr(0, "closehandle");
		proc->ph = 0;
	}
#endif
	proc_release(proc);
	return(pid);
}

pid_t waitid(idtype_t type, id_t id, siginfo_t *info, int options)
{
	pid_t pid;
	switch(type)
	{
	    case P_PID:
		pid = (pid_t)id;
		break;
	    case P_PGID:
		pid = -(pid_t)id;
		break;
	    case P_ALL:
		pid = -1;
		break;
	    default:
		errno = EINVAL;
		return( -1 );
	}
	return(waitcommon(pid,info,options,0));
}

pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
	siginfo_t info;
	if((pid=waitcommon(pid,&info,options,0))<=0)
		return(pid);
	if(stat_loc)
		*stat_loc = info.si_code;
	return(info.si_pid);
}

pid_t wait3(int *stat_loc, int options, struct rusage *rp)
{
	siginfo_t info;
	pid_t pid;
	if((pid=waitcommon((pid_t)-1,&info,options,rp))<=0)
		return(pid);
	if(stat_loc)
		*stat_loc = info.si_code;
	return(info.si_pid);
}

pid_t wait4(pid_t pid, int *stat_loc, int options, struct rusage *rp)
{
	siginfo_t info;
	if((pid=waitcommon(pid,&info,options,rp))<=0)
		return(pid);
	if(stat_loc)
		*stat_loc = info.si_code;
	return(info.si_pid);
}

pid_t wait(int *stat_loc)
{
	siginfo_t info;
	pid_t pid;
	if((pid=waitcommon((pid_t)-1,&info,0,0))<=0)
		return(pid);
	if(stat_loc)
		*stat_loc = info.si_code;
	return(info.si_pid);
}

static jmp_buf env;
static void *trace_addr;

/*
 * try to make the region given by address and size accessible
 */
static int make_access(void *addr, size_t size, const char *name)
{
	MEMORY_BASIC_INFORMATION minfo;
	DWORD r,mode;
	size_t left;
	if(IsBadStringPtr((void*)name,1))
		name="???";
	while(VirtualQuery(addr,&minfo,sizeof(minfo))==sizeof(minfo))
	{
		left = 0;
		if((minfo.Type&MEM_MAPPED) && Share->Platform==VER_PLATFORM_WIN32_NT)
			mode = PAGE_WRITECOPY;
		else
			mode = PAGE_READWRITE;
		if(minfo.RegionSize < size)
		{
			left = minfo.RegionSize-size;
			size = minfo.RegionSize;
		}
		if(minfo.State==MEM_FREE || (minfo.State&(MEM_RESERVE|MEM_COMMIT)) == MEM_RESERVE)
		{
			if(VirtualAlloc((void*)segment(addr),roundof(size,64*1024),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE))
			{
				if(left==0)
					return(1);
			}
			logerr(0, "VirtualAlloc %s failed addr=%08x size=%lu", name, segment(addr), roundof(size,64*1024));
			return(0);
		}
		else if(VirtualProtect(addr,size,mode,&r))
		{
			if(left==0)
				return(1);
		}
		else
		{
			logerr(0, "VirtualProtect: name=%s addr=%08x size=%d mode=0x%x base=%p rsize=%d attr=0x%x", name, addr, size, mode, minfo.BaseAddress, minfo.RegionSize, minfo.Protect|minfo.State|minfo.Type);
			break;
		}
		addr = (void*)((char*)addr+size);
		size = left;
	}
	return(0);
}

/*
 * copy read/write pages from parent segment
 * <hp> is handle to parent
 * <addr> is an address in the segment
 * if <sp>==<addr> just copy given region.
 * if <sp>==NULL copy all read/write pages
 * otherwise, <sp> is stack pointer for stack segment
 * if all is set, then readonly pages are also copied
 */
static int region(HANDLE hp, void* addr, char *name,void *sp, int all)
{
	MEMORY_BASIC_INFORMATION minfo;
	MEMORY_BASIC_INFORMATION rinfo;
	char *cur, *base;
	SIZE_T size=0;
	if(VirtualQueryEx(hp,addr,&minfo,sizeof(minfo))!=sizeof(minfo))
	{
		logerr(0, "VirtualQueryEx addr=%08x", addr);
		goto bad;
	}

	if(addr==sp)
	{
		size = minfo.RegionSize;
		cur = minfo.BaseAddress;
		if(!ReadProcessMemory(hp,(void*)cur,(void*)cur,size,&size))
		{
			logerr(0, "ReadProcessMemory %s failed", name);
			goto bad;
		}
		return(0);
	}
	if(VirtualQueryEx(hp,(void*)minfo.AllocationBase,&rinfo,sizeof(rinfo)) !=sizeof(rinfo))
	{
		logerr(0, "VirtualQueryEx addr=%08x", addr);
		goto bad;
	}
	base = cur = rinfo.BaseAddress;
	while(1)
	{
		if(VirtualQueryEx(hp,(void*)cur,&minfo,sizeof(minfo))!=sizeof(minfo))
		{
			logerr(0, "VirtualQueryEx %s failed addr=%08x", name, cur);
			goto bad;
		}
		if(minfo.AllocationBase!=(void*)base)
			break;
		if((all || (minfo.Protect&PAGE_READWRITE)) && (minfo.State&MEM_COMMIT) && !((minfo.Protect&PAGE_GUARD)))
		{
			size = minfo.RegionSize;
			if(sp)
			{
				/* This following should never happen but it
				 * does on windows 95 so compensate for it
				 */
				if((char*)sp >= (char*)cur+size)
				{
					long nsize = (long)((char*)sp-(char*)cur);
					size = roundof(nsize,4096);
				}
				if((char*)sp > (char*)cur)
				{
					size -= ((char*)sp-(char*)cur);
					cur = sp;
				}
				else
					size -= ((char*)cur-(char*)sp);
			}
			else if(!all)
				VirtualAlloc((void*)cur,minfo.RegionSize,MEM_COMMIT,PAGE_READWRITE);
			if(size>0)
			{
				SIZE_T r;
				if(!ReadProcessMemory(hp,(void*)cur,(void*)cur,size,&r) && GetLastError()==ERROR_NOACCESS)
				{
					if(!make_access(cur,size,name))
						goto bad;
					if(!ReadProcessMemory(hp,(void*)cur,(void*)cur,size,&size))
					{
						logerr(0, "ReadProcessMemory %s failed", name);
						goto bad;
					}
				}
			}
		}
		cur = (char*)minfo.BaseAddress + minfo.RegionSize;
	}
	return(0);
bad:
	logerr(LOG_PROC+2, "region copy failed");
	return(-1);
}

static DWORD *mlist, *mlistp;

static unsigned long *_visited;

__inline int is_visited(void *addr)
{
	register unsigned x = ((unsigned int)addr)>>16;
	return(_visited[x>>5]&(1L << (x&0x1f)) );
}
__inline void visited(void *addr)
{
	register unsigned x = ((unsigned int)addr)>>16;
	_visited[x>>5] |= (1L << (x&0x1f));
}

static const char *skiplist[] =
{
	"user32.dll",
	"kernel32.dll",
	"advapi32.dll",
	"gdi32.dll",
	"msctf.dll",
	"rpct4.dll",
	"secur32.dll",
	"ntdll.dll",
	"ole32.dll",
	"comctl32.dll",
	"shell32.dll",
	"shlwapi.dll",
	"mpr.dll",
	"winmm.dll",
	"pthread.dll",
	"apphelp.dll",
	0
};

/*
 * walk through dlls and copy UWIN generated ones except posix.dll
 */
static int dll_copy(HANDLE pp, void* addr)
{
	IMAGE_DOS_HEADER*	dosp = (IMAGE_DOS_HEADER*)addr;
	IMAGE_NT_HEADERS*	ntp;
	IMAGE_IMPORT_DESCRIPTOR*imp;
	HINSTANCE		hp;
	char*			name;
	if (IsBadReadPtr(addr, sizeof(*dosp)))
		return 0;
	if (dosp->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	ntp = ptr(IMAGE_NT_HEADERS, dosp, dosp->e_lfanew);
	imp = ptr(IMAGE_IMPORT_DESCRIPTOR, addr, ntp->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	if ((void*)imp == (void*)ntp || (void*)imp == addr)
		return 1;
	for (; imp->Name; imp++)
	{
		const char **lib;
		name = ptr(char, addr, imp->Name);
		if (!(hp = GetModuleHandle(name)))
			break;
		if(is_visited(hp))
			continue;
		if(memicmp(name,"msvc",4)==0)
		{
			visited(hp);
			continue;
		}
		for(lib=skiplist; *lib; lib++)
		{
			if(_stricmp(name,*lib)==0)
			{
				visited(hp);
				break;
			}
		}
		if(*lib)
			continue;
		if(_stricmp(name,POSIX_DLL ".dll"))
			region(pp,hp,name,0,0);
		visited(hp);
		dll_copy(pp,hp);
	}
	return 1;
}
#undef ptr

static int vwalk(Vmalloc_t* vp, void * addr, size_t size, Vmdisc_t* dp)
{
	*mlistp++ = (DWORD)addr;
	*mlistp++ = (DWORD)size;
	return(0);
}

static int argvsize(register char *argv[])
{
	register char **av=argv;
	while(*av)
		av++;
	av--;
	return (int)(*av-(char*)argv) + (int)strlen(*av) + 1;
}

int child_fork(Pproc_t *pp,void *sp, HANDLE *extra)
{
	static HANDLE th;
	HANDLE hp, pp_trace = pp->trace;
	HANDLE me = GetCurrentProcess();
	char **new_argv,**new_env;
	Pproc_t *proc;
	int direction,i,maxfd;
	void *addr,*haddr;
	void *first_addr = malloc_first_addr;
	char *cp;
	DWORD dummy[2048];
	SIZE_T size;
	/* grow stack as necessary */
	P_CP->exitcode = 61;
	direction = (&hp > extra? 1: -1);
	cp = (char*)sp + (direction>0?2048:-2048);
	/* prevent optimizer from eliminating dummy */
	hp = (HANDLE)dummy[pp->maxfds];
	logmsg(LOG_PROC+5, "child fork extra=%p", extra);
	if (direction < 0 && (char*)&hp > cp || direction > 0 && (char*)&hp < cp)
		return(child_fork(pp,sp,&hp));
	/* add a little stack space to spare */
	sp  = (void*)((char*)sp + 1000*direction);
	if(Share->Platform==VER_PLATFORM_WIN32_NT && P_CP->etoken)
		RevertToSelf();
	logmsg(LOG_PROC+6, "child_fork before %(memory)s", GetCurrentProcess());
	hp = open_proc(pp->ntpid,USER_PROC_ARGS);
	maxfd = pp->maxfds;
	proc_release(pp);
	if(!hp || hp==INVALID_HANDLE_VALUE)
	{
		logerr(0, "open_proc %s failed", "child fork access");
		longjmp(env,ENOMEM+1);
	}
	if(major_version==0)
	{
		OSVERSIONINFO osinfo;
		osinfo.dwOSVersionInfoSize = sizeof(osinfo);
		if(GetVersionEx(&osinfo))
			major_version = osinfo.dwMajorVersion;
	}
	P_CP->exitcode = 62;
	mlistp = mlist = dummy;
	if(major_version < NEWFORK_VERSION)
	{
		vmwalk(0,vwalk);
		if(arg_base)
		{
			*mlistp++ = (DWORD)arg_base;
			*mlistp++ = Share->argmax+ARG_SLOP;
		}
	}
	*mlistp = 0;
	haddr = (void*)mlist[0];
	new_argv = dllinfo._ast__argv;
	new_env = *dllinfo._ast_environ;
	_visited = dummy;
	ZeroMemory((void*)dummy,sizeof(dummy));
	P_CP->exitcode = 63;
	if(!dll_copy(hp,GetModuleHandle(0)))
		logerr(0, "regions not copied");
	P_CP->exitcode = 64;
	region(hp,GetModuleHandle(0),"data",0,0);
	P_CP->exitcode = 65;
	proc = P_CP;
	region(hp,&env,POSIX_DLL ".dll",0,0);
	Ntpid = GetCurrentProcessId();
#if 1
	proc_mutex(0,0);
#endif
	P_CP = proc;
	P_CP->forksp = 0;
	P_CP->exitcode = 66;
	if(malloc_first_addr != first_addr)
		logmsg(LOG_PROC+3, "malloc_first_addr=%p first_addr=%p", malloc_first_addr, first_addr);
	logmsg(LOG_PROC+6, "child_fork after %(memory)s", GetCurrentProcess());
	if(major_version < NEWFORK_VERSION)
	{
		if(malloc_first_addr != first_addr)
			region(hp,malloc_first_addr,"malloc_first",0,0);
		size = sizeof(DWORD) + (char*)(mlistp)-(char*)mlist;
		if(size > sizeof(dummy))
		{
			logmsg(0, "too many malloc regions=%d", size/8);
			size = sizeof(dummy)/sizeof(*dummy);
			dummy[sizeof(dummy)/sizeof(*dummy)-1] = 0;
		}
		if(!ReadProcessMemory(hp,(void*)mlist,(void*)dummy,size,&size))
			logerr(0, "ReadProcessMemory");
		mlistp = mlist = dummy;
	}
	P_CP->exitcode = 67;
	if(pp_trace && trace_addr)
	{
		if(P_CP->trace)
			region(hp,trace_addr,"trace10.lib",0,0);
		else
		{
			HANDLE map;
			if(map=CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,2<<16, NULL))
			{
				if(addr=MapViewOfFileEx(map,FILE_MAP_READ|FILE_MAP_WRITE, 0,0,0,trace_addr))
					region(hp,trace_addr,"trace10.lib",0,1);
				else
					logerr(0, "MapViewOfFile");
				closehandle(map,HT_MAPPING);
			}
			else
				logerr(0, "CreateFileMapping");
		}
	}
	P_CP->exitcode = 68;
	if(mlist)
	{
		void*	addr;
		SIZE_T	size;
		int	err;

		for(mlistp=mlist; *mlistp; mlistp+=2)
		{
			addr = (void*)mlistp[0];
			size = mlistp[1];
			if(!VirtualAlloc(addr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))
			{
				err = GetLastError();
				if(haddr!=addr)
					logerr(0, "VirtualAlloc addr=%08x size=%lu", addr, size);
				if(haddr!=addr && err==ERROR_NOT_ENOUGH_MEMORY)
					longjmp(env,ENOMEM+1);
			}
			if(!ReadProcessMemory(hp,addr,(void*)addr,size,&size))
				logerr(0, "ReadProcessMemory addr=%08x size=%lu", addr, size);
		}
	}
	P_CP->exitcode = 69;
	/* Make the close-on-exec handles accessable */
	crit_init = 0;
	P_CP->maxfds = maxfd;
	P_CP->exitcode = 70;
	for(i=0; i < maxfd;i++)
	{
		HANDLE handle;
		Ehandle(i) = 0;
		if(fdslot(P_CP,i)>=0 && iscloexec(i))
		{
			Pfd_t *fdp = getfdp(P_CP,i);
			if(fdp->type==FDTYPE_CONSOLE||fdp->type==FDTYPE_CONNECT_INET)
			{
				/* make them non-inheritible again */
				Phandle(i) = Fs_dup(i,fdp,&Xhandle(i),2);
				continue;
			}
			if(handle=Phandle(i))
			{
				handle = (HANDLE)(((unsigned long)handle)&~3);
				if(!DuplicateHandle(hp,handle,me,&handle,0,FALSE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				else
					Phandle(i) = handle;
			}
			if(handle=Xhandle(i))
			{
				handle = (HANDLE)(((unsigned long)handle)&~3);
				if(!DuplicateHandle(hp,handle,me,&handle,0,FALSE,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				else
					Xhandle(i) = handle;
			}
		}
	}
	if(!(th = OpenThread(THREAD_QUERY_INFORMATION|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT, FALSE, parent_tid)) || th == INVALID_HANDLE_VALUE)
	{
		logerr(0, "child_fork OpenThread failed tid=%x",parent_tid);
		return(-1);
	}
	if(region(hp,(char*)sp+direction,"stack",sp,0)<0)
	{
		P_CP->state = PROCSTATE_TERMINATE;
		i=ResumeThread(th);
		logmsg(0,"stack region copy failed resume=%d",i);
		ExitProcess(-1);
	}
	P_CP->exitcode = 72;
	main_thread = GetCurrentThreadId();
	sinit = 0;
	if(Share->Platform==VER_PLATFORM_WIN32_NT && P_CP->etoken)
		impersonate_user(P_CP->etoken);
	if(!infork)
		ipcfork(hp);
	P_CP->exitcode = 73;
	map_fork(hp);
	sbrk_fork(hp);
	P_CP->exitcode = 74;
	dll_fork(hp);
#ifdef GTL_TERM
	fsntfork();
	pdevcachefork();
#endif
	infork = 0;
	if(SuspendThread(th) < 0)
	{
		logerr(0, "child_fork SuspendThread failed");
		return(-1);
	}
	pcontext.ContextFlags = UWIN_CONTEXT_FORK;
	if(!GetThreadContext(th,&pcontext))
		logerr(0,"GetThreadContext tid=%04x",parent_tid);
	if(!WriteProcessMemory(hp,(void*)&pcontext,(void*)&pcontext,sizeof(CONTEXT),&size))
		logerr(0, "WriteProcessMemory %s failed", "pcontext");
	while ((i = ResumeThread(th)) > 0);
	if(i < 0)
		logerr(0, "ResumeThread failed");
	closehandle(th,HT_THREAD);
	closehandle(hp,HT_PROC);
	SuspendThread(GetCurrentThread());
	logmsg(0, "child_fork parent SetThreadContext failed");
	return(-1);
}


static void atparent(struct atfork *ap)
{
	if(ap->next)
		atparent(ap->next);
	else if(ap->parent)
		(*ap->parent)();
}

static void atchild(struct atfork *ap)
{
	if(ap->next)
		atchild(ap->next);
	else if(ap->child)
		(*ap->child)();
}

static pid_t parent_fork(void)
{
	DWORD access = DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE;
	HANDLE me,ph;
	void *fdtabp=0;
	DWORD size,err;
	int sigsys,i, maxtry=25;
	Pproc_t *pp;
	char cmdname[PATH_MAX];
	pid_t pid=0;
	sigsys = sigdefer(1);
	if(!GetModuleFileName(NULL,cmdname,sizeof(cmdname)))
	{
		logerr(0, "GetModuleFileName");
		goto bad;
	}
	me = GetCurrentProcess();
	arg_argv = 0;
again:
	if(!(pp=start_proc(0, cmdname, 0, 0, NULL, NULL, P_CP->etoken?(S_ISUID|S_ISGID):0, EXE_FORK, 0)))
	{
		if(errno!=EAGAIN || maxtry--==0) 
			goto bad;
		logmsg(0,"parent_fork failed - try again maxtry=%d exitval=%d",maxtry,fork_err);
		goto again;
	}
	/* put stack address into forksp */
	fork_err = 0;
	pp->forksp = (void*)&size;
	for(i=0; i < P_CP->maxfds; i++)
	{
		if(iscloexec(i))
		{
			incr_refcount(getfdp(P_CP,i));
			pp->fdtab[i].cloexec = iscloexec(i);
		}
	}
	logmsg(LOG_PROC+3,"fork child slot=%d pid=%x-%d forksp=%p",proc_slot(pp),pp->ntpid,pp->pid,pp->forksp);
	pid = pp->pid;
	pp->exitcode = 56;
	pp->siginfo.siggot = 0;
	fdtabp = pp->fdtab;
	ph = pp->phandle;
	pp->phandle = 0;
	pp->exitcode = 57;
	parent_tid = P_CP->threadid;
	if(!(parent_thread = OpenThread(THREAD_QUERY_INFORMATION|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT, FALSE, GetCurrentThreadId())) || parent_thread == INVALID_HANDLE_VALUE)
	{
		parent_thread = 0;
		logerr(0,"OpenThread");
		TerminateProcess(ph,126);
		ResumeThread(ph);
		goto bad;
	}
	i=ResumeThread(ph);
    if (i < 0 )
		logerr(0, "parent_fork resume thrd=%04x failed", ph);
	SuspendThread(parent_thread);
	closehandle(parent_thread,HT_THREAD);
	parent_thread = 0;
	if((infork && fork_err) || pp->state==PROCSTATE_TERMINATE)
	{
		if(maxtry-->0)
		{
			logerr(0, "fork fail maxtry=%d state=%(state)s pid=%x-%d slot=%d fork_state=%d size=%d fork_err=%d posix=0x%x tid=%x", maxtry, pp->state, pp->ntpid, pp->pid, proc_slot(pp), pp->exitcode,size,fork_err,pp->posixapp,parent_tid);
			if(pp->posixapp&CHILD_FORK)
				goto ok;
			closehandle(ph,HT_THREAD);
			ph = 0;
			InterlockedDecrement(&pp->inuse);
			end_critical(CR_MALLOC);
			pp->state = PROCSTATE_EXITED;
			Sleep(100);
			goto again;
		}
		closehandle(ph,HT_THREAD);
		ph = 0;
		pid = -1;
	}
ok:
	end_critical(CR_MALLOC);
	fork_err = 0;
	if(infork)
	{
		if(pid>0)
		{
			if(!ph)
			{
				logerr(0, "parent_fork infork ph=%p", ph);
				goto bad;
			}
			if(SuspendThread(ph) < 0)
			{
				logerr(0,"parent_fork SuspendThread");
				goto bad;
			}
			if(!SetThreadContext(ph,&pcontext))
			{
				logerr(0,"parent_fork SetThreadContext");
				goto bad;
			}
			while ((i = ResumeThread(ph)) > 0)
				Sleep(1);
			if (i < 0)
				logerr(0, "child resume failed");
		}
		if(ph)
			closehandle(ph,HT_THREAD);
	}
	else
	{
		/* This code should only be run by the child */
		initsig(0);
		eventnum = 0;
		thwait = 0;
		uidset = 0;
		P_CP->posixapp |= CHILD_FORK;
		P_CP->exitcode = 0;
		P_CP->phandle = 0;
		vmtrace(-1);
		sigcheck(0);
		if(Atfork)
			atchild(Atfork);
		return(0);
	}
	infork=0;
	proc_release(pp);
	/*
	 * make close-on-exec console handles non-inheritible again
	 * since setup_fdtable has made them inheritible
	 */
	for(i=0; i<P_CP->maxfds; i++)
		if(fdslot(P_CP,i)>=0 && iscloexec(i))
		{
			Pfd_t *fdp = getfdp(P_CP,i);
			if(fdp->type==FDTYPE_CONSOLE||fdp->type==FDTYPE_CONNECT_INET)
				Phandle(i) = Fs_dup(i,fdp,&Xhandle(i),2);
		}
	if(fdtabp)
		free(fdtabp);
 	sigcheck(sigsys);
	return(pid);
bad:
	infork=0;
	errno = EAGAIN;
	if((err=GetLastError())==ERROR_NO_SYSTEM_RESOURCES)
		errno = ENOMEM;
	else if(err!=ERROR_BAD_COMMAND)
	{
		if(err == ERROR_ACCESS_DENIED)
			errno = EPERM;
		logerr(0, "fork error fdtab=%p errno=%d err=%d pp=%p",fdtabp,errno,err,pp);
	}
	if(fdtabp)
		free(fdtabp);
	sigcheck(sigsys);
	if(Atfork)
		atparent(Atfork);
	sigignore(SIGCHLD);
	return(-1);
}

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	struct atfork *ap = (struct atfork*)malloc(sizeof(struct atfork));
	if(!ap)
	{
		errno = ENOMEM;
		return(-1);
	}
	ap->next = Atfork;
	ap->prepare = prepare;
	ap->parent = parent;
	ap->child = child;
	Atfork = ap;
	return(0);
}

static void prepare(struct atfork *ap)
{
	while(ap)
	{
		if(ap->prepare)
			(*ap->prepare)();
		ap = ap->next;
	}
}

/*
 * This is the first attempt at fork, it is not complete
 * malloc() not handled
 */
pid_t fork(void)
{
	pid_t 	pid;
	if(major_version==0)
	{
		OSVERSIONINFO osinfo;
		osinfo.dwOSVersionInfoSize = sizeof(osinfo);
		if(GetVersionEx(&osinfo))
			major_version = osinfo.dwMajorVersion;
	}
	if(Atfork)
		prepare(Atfork);
	sync_io();
	if(major_version < NEWFORK_VERSION)
	{
		free(malloc(1024));
		vmcompact(Vmregion);
		mlist = (DWORD*)arg_getbuff();
		mlistp = mlist;
		vmwalk(0,vwalk);
		if(arg_base)
		{
			*mlistp++ = (DWORD)arg_base;
			*mlistp++ = Share->argmax+ARG_SLOP;
		}
		*mlistp = 0;
	}
	else
	{
		static int skipclone;
		pid = -1;
		if(!skipclone)
			pid = ntclone();
		if(pid>=0)
		{
			if(pid==0)
			{
				DWORD i;
				CloseHandle(waitevent);
				for(i=0; i < nwait; i++)
					CloseHandle(objects[i]);  
				thwait = 0;
			}
			return(pid);
		}
		else if(pid==-2)
			skipclone = 1;
	}
	if(P_CP->trace)
		trace_addr = (void*)GetModuleHandle("trace10.dll");
	infork=1;
	return(parent_fork());
}


#ifdef _ALPHA_
    void 	*__cdecl _alloca(size_t);
#   pragma intrinsic(_alloca)
    static int va_count(va_list args)
    {
	char *cp;
	int count=2;
	while(cp = va_arg(args,char*))
		count++;
	return(count);
    }

    int execl(const char *path, const char *arg, ...)
    {
	char *cp, **av, **argv;
	va_list args;
	va_start(args,arg);
	argv = av = (char**)_alloca(va_count(args)*sizeof(char*));
	*av++ = (char*)arg;
	while(cp = va_arg(args,char*))
		*av++ = cp;
	*av = 0;
	va_end(args);
	return(execve(path, (char*const*)argv,0));
    }
#else
    int execl(const char *path, const char *arg, ...)
    {
	return(execve(path, (char*const*)&arg,0));
    }
#endif

int execv(const char *path, char *const arg[])
{
	return(execve(path,arg,0));
}

#ifdef _ALPHA_
    int execle(const char *path, const char *arg, ...)
    {
	char const *cp, **av, **argv, **env;
	va_list args;
	va_start(args,arg);
	argv = av = (char**)_alloca(va_count(args)*sizeof(char*));
	*av++ = (char*)arg;
	while(cp = va_arg(args,char*))
		*av++ = cp;
	*av = 0;
	env = (char**) va_arg(args,char*);
	va_end(args);
	return(execve(path, (char*const*)argv,(char*const*)env));
    }

    pid_t spawnle(const char *path, const char *arg, ...)
    {
	register char *cp, **av, **argv, **env;
	va_list args;
	va_start(args,arg);
	argv = av = (char**)_alloca(va_count(args)*sizeof(char*));
	*av++ = (char*)arg;
	while(cp = va_arg(args,char*))
		*av++ = cp;
	*av = 0;
	env = (char**) va_arg(args,char*);
	va_end(args);
	return(spawnve(path, (char*const*)argv,(char*const*)env));
    }
#else
    int execle(const char *path, const char *arg, ...)
    {
	va_list args;
	int r;
	const char *tmp=arg;
	const char **env;
	va_start(args,arg);
	while(tmp)
		tmp = va_arg(args, const char*);
	env = va_arg(args,const char**);
	r = execve(path, (char*const*)&arg, (char*const*)env);
	va_end(args);
	return(r);
    }

    pid_t spawnle(const char *path, const char *arg, ...)
    {
	va_list args;
	pid_t pid;
	const char *tmp=arg;
	const char **env;
	va_start(args,arg);
	while(tmp)
		tmp = va_arg(args, const char*);
	env = va_arg(args,const char**);
	pid = spawnve(path, (char*const*)&arg, (char*const*)env);
	va_end(args);
	return(pid);
    }
#endif

pid_t spawnv(const char *path, char *const arg[])
{
	return(spawnve(path,arg,0));
}

#ifdef _ALPHA_
    pid_t spawnl(const char *path, const char *arg, ...)
    {
	char const *cp, **av, **argv;
	va_list args;
	va_start(args,arg);
	argv = av = (char**)_alloca(va_count(args)*sizeof(char*));
	*av++ = (char*)arg;
	while(cp = va_arg(args,char*))
		*av++ = cp;
	*av = 0;
	va_end(args);
	return(spawnve(path, (char*const*)argv,0));
    }
#else
    pid_t spawnl(const char *path, const char *arg, ...)
    {
	return(spawnve(path, (char*const*)&arg,0));
    }
#endif

static void grantaccess(int level)
{
	SECURITY_DESCRIPTOR *sd;
	HWINSTA hws;
	HDESK hdesk;
	SECURITY_INFORMATION si;
	sd  = nulldacl();

	if(hws = GetProcessWindowStation())
	{
		si = DACL_SECURITY_INFORMATION;
		SetUserObjectSecurity(hws, &si, sd);
	}
	else if(!state.clone.hp)
		logerr(0, "GetProcessWindowStation");
	if (hdesk = GetThreadDesktop(GetCurrentThreadId()))
	{
		si = DACL_SECURITY_INFORMATION;
		SetUserObjectSecurity(hdesk, &si, sd);
	}
	if(level)
	{
		if (isfdvalid(P_CP, 0))
		{
			si = DACL_SECURITY_INFORMATION;
			SetUserObjectSecurity(Phandle(0), &si, sd);
		}
		if (isfdvalid(P_CP, 1))
		{
			si = DACL_SECURITY_INFORMATION;
			SetUserObjectSecurity(Phandle(1), &si, sd);
		}
		if (isfdvalid(P_CP, 2))
		{
			si = DACL_SECURITY_INFORMATION;
			SetUserObjectSecurity(Phandle(2), &si, sd);
		}
	}
}

/*
 * Create a security token for the  user given by <name>
 */
Handle_t uwin_mktoken(const char *name, const char *passwd, int flags)
{
	struct passwd *pwent=0;
	char domain[PATH_MAX],username[32];
	const char *cp;
	Handle_t hp=0;
	int n,err,sid[64];
	SID_NAME_USE puse;
	static BOOL (WINAPI *log_user)(const char*,const char*,const char*,DWORD,DWORD,Handle_t*);
	if(!log_user)
		log_user = (BOOL (WINAPI*)(const char*,const char*,const char*,DWORD,DWORD, Handle_t*))getsymbol(MODULE_advapi,"LogonUserA");
	if(!log_user)
	{
		errno = ENOSYS;
		return(0);
	}
	if(flags&UWIN_TOKETOK)
	{
		HANDLE me=GetCurrentProcess();
		if(!P_CP->etoken)
			return(0);
		if(!DuplicateHandle(me,P_CP->etoken,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(!(flags&UWIN_TOKADMIN))
			set_mandatory(1);
		return(hp);
	}
	if(!passwd || *passwd==0)
	{
		errno = EINVAL;
		return(0);
	}
	cp = 0;
	if((pwent=getpwnam(name)) && uwin_uid2sid(pwent->pw_uid,(Sid_t*)sid,sizeof(sid)))
	{
		int sidlen=sizeof(username), domlen=sizeof(domain);
		if(LookupAccountSid(NULL, sid, username, &sidlen, domain,&domlen, &puse))
			cp = username;
		else
			logerr(0, "LookupAccountSid");
	}
	if(!cp)
	{
		if(cp=strchr(name,'/'))
		{
			n = (int)(cp-name);
			memcpy(domain,name,n);
			domain[n] = 0;
			cp++;
		}
		else
		{
			cp = name;
			domain[0] = '.';
			domain[1] = 0;
		}
	}
	if(!(*log_user)(cp,domain,passwd, LOGON32_LOGON_INTERACTIVE,
		  LOGON32_PROVIDER_DEFAULT, &hp))
	{
		hp = 0;
		err = GetLastError();
		if(err==ERROR_LOGON_FAILURE && (!passwd || *passwd || strcmp(cp,"NOUSER")==0))
		{
			errno = EPERM;
			return(0);
		}
	}
	else if(!(flags&UWIN_TOKCLOSE) && !SetHandleInformation(hp,HANDLE_FLAG_INHERIT,TRUE))
		logerr(0, "SetHandleInformation");

	if(!hp && (err==ERROR_PRIVILEGE_NOT_HELD || err==ERROR_ACCESS_DENIED) && is_admin() && (pwent || (pwent=getpwnam(name))))
	{
		/* ask ums to get this token */
		int buffer[512];
		char *name;
		UMS_setuid_data_t* sp = (UMS_setuid_data_t*)buffer;
		memset(sp, 0, sizeof(*sp));
		sp->gid = UMS_MK_TOK;
		sp->uid = pwent->pw_uid;
		sp->dupit = 1;
		name = (char*)(sp+1);
		n = (int)strlen(passwd)+1;
		memcpy(name,passwd,n);
		name +=n;
		n = (int)strlen(domain)+1;
		memcpy(name,domain,n);
		hp = ums_setids(sp,(int)(name-(char*)buffer)+n);
	}
	if(hp)
	{
		/* update or create ucs server */
		HANDLE pread = 0, pwrite = 0;
		char cmdname[PATH_MAX];
		char param[PATH_MAX];
		STARTUPINFO sinfo;
		PROCESS_INFORMATION pinfo;

		uwin_pathmap("/usr/etc/ucs.exe",cmdname,sizeof(cmdname),UWIN_U2W);
		ZeroMemory(&sinfo,sizeof(sinfo));
		if (CreatePipe(&pread,&pwrite,sattr(1),PIPE_BUF) != 0)
		{
		    int cnt;
			if (domain[0] == '.')
				sfsprintf(param,sizeof(param),"ucs install \"%s\" \"%s\"",cp,"-");
			else
				sfsprintf(param,sizeof(param),"ucs install \"%s/%s\" \"%s\" quiet",domain,cp,"-");
			sinfo.dwFlags = STARTF_USESTDHANDLES;
			sinfo.hStdInput = pread;
			sinfo.hStdOutput = NULL;
			sinfo.hStdError = NULL;
			WriteFile(pwrite, passwd, (DWORD)strlen(passwd), &cnt, NULL);
			if (cnt != strlen(passwd))
				logerr(0, "mktoken passwd write failed wrote=%d expected=%d", cnt, (strlen(passwd)));
		}
		else
		{
			if (domain[0] == '.')
				sfsprintf(param,sizeof(param),"ucs install \"%s\" \"%s\"",cp,passwd);
			else
				sfsprintf(param,sizeof(param),"ucs install \"%s/%s\" \"%s\" quiet",domain,cp,passwd);
		}
		if(CreateProcess(cmdname,param,NULL,NULL,1, DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP|NORMAL_PRIORITY_CLASS, NULL, NULL, &sinfo, &pinfo))
		{
			closehandle(pinfo.hThread,HT_THREAD);
			/* UCS startup can take 8 seconds */
			n=WaitForSingleObject(pinfo.hProcess,10000);
			if(n==0)
			{
				if(!GetExitCodeProcess(pinfo.hProcess,&n))
					logerr(0, "GetExitCodeProcess");
				else if(n)
					logmsg(0, "ucs_startup_failed exit status=0x%x", n);
			}
			else
				logmsg(0, "ucs_initialization did not complete n=0x%x", n);
			closehandle(pinfo.hProcess,HT_PROC);
		}
		else
			logerr(0, "CreateProcess %s failed", cmdname);

		if (pwrite)
			closehandle(pwrite, HT_PIPE);
		if (pread)
			closehandle(pread, HT_PIPE);
		if(flags&UWIN_TOKUSE)
		{
#if 0
			HANDLE me=GetCurrentProcess();
			if(P_CP->etoken)
				closehandle(P_CP->etoken,HT_TOKEN);
#endif
			impersonate_user(hp);
#if 0
			if(!DuplicateHandle(me,hp,me,&P_CP->etoken,0,TRUE,DUPLICATE_SAME_ACCESS))
				logerr(0, "DuplicateHandle");
			else
				P_CP->etoken = 0;
			init_sid(1);
#endif
		}
		if(flags&UWIN_TOKCLOSE)
			closehandle(hp,HT_TOKEN);
		return(hp);
	}
	logerr(LOG_PROC+3, "LogonUser %s failed", cp);
	errno = unix_err(err);
	return(0);
}

#ifdef CLOSE_HANDLE
extern struct init_handles *init_handles;
#   undef DuplicateHandle
    BOOL XDuplicateHandle(HANDLE from, HANDLE src, HANDLE to, HANDLE *dest, DWORD access, BOOL inherit,DWORD options, const char *file, int line)
	{
		HANDLE ih;
		DWORD topid, ntpid;
		BOOL x = DuplicateHandle(from,src,to,dest,access,inherit,options);
		if(x && P_CP && Share->init_slot)
		{
			if(src==GetCurrentProcess())
				ntpid = GetCurrentProcessId();
			else
				ntpid = GetProcessId(src);
			ih = init_handle();
			if(from==ih && (options&DUPLICATE_CLOSE_SOURCE))
			{
				InterlockedDecrement(&Share->ihandles);
				set_init_handle(src,line,HANDOP_DUPF);
				//logmsg(0,"dupi: at %s:%d ph=%p pid=%x ih=%d slot=%d src=%p",file,line,*dest,ntpid,Share->ihandles,Lastslot,src);
			}
			if(to==ih)
				topid = Share->init_ntpid;
			else
				topid = GetProcessId(to);
			if(topid==Share->init_ntpid)
			{
				InterlockedIncrement(&Share->ihandles);
				set_init_handle(to,line,HANDOP_DUP2);
				//logmsg(0,"dup: at %s:%d ph=%p pid=%x ih=%d slot=%d src=%p",file,line,*dest,ntpid,Share->ihandles,Lastslot,src);
			}
		}
		Lastslot = 0;
		return(x);
	}
#endif
