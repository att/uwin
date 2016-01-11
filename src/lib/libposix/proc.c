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
#include	<stddef.h>
#include	<uwin.h>
#include	<sys/utsname.h>
#include	"ident.h"
#include	"uwin_serve.h"

#define is_slotvalid(s)	(((s) && (s)< Share->block && Blocktype[(s)&BLK_MASK]!=BLK_PROC)?1:(logmsg(0,"badslot%d s=%d",__LINE__,(s)),0))

extern unsigned long *Generation;

#if BLK_CHECK
    struct Mutexlog
    {
	DWORD		counter;
	unsigned short	slot;
	char		type;
	char		op;
	char		file[6];
	unsigned short	line;
	DWORD		time;
	DWORD		thid;
    };
    static struct Mutexlog	*Mlog;
    static void dumpmutex(void);
    static void logmutex(char,int, const char*, int);
    static unsigned __int64	cycle0;
#   define MUTMASK		0xff
#   define mutexdump()	(type==3?Share->dcount = Share->nmutex + 64:0)

#   define FMT_ATBLK	" at %s:%d"
#   define FMT_FMBLK	" from %s:%d"
#   define BLK_ARGS	,file,line
#else
#   define mutexdump()
#   define logmutex(a,b,c,d)
#   define FMT_ATBLK
#   define FMT_FMBLK
#   define BLK_ARGS
#endif

#ifndef BELOW_NORMAL_PRIORITY_CLASS
#    define BELOW_NORMAL_PRIORITY_CLASS 0x4000
#    define ABOVE_NORMAL_PRIORITY_CLASS 0x8000
#endif

#define BLOCK_TRACE	1

#define proc_hash(pid)	((unsigned short)(pid&(Share->nprocs-1)))

#if _X64_
#   define COMPARE_EXCHANGE(p,x,c)	InterlockedCompareExchange((void*)(p),(LONG)(x),(LONG)(c))
#else
#   define COMPARE_EXCHANGE(p,x,c)	(*Share->compare_exchange)((void*)p,(LONG)x,(LONG)c)
#   define USE_SEMAPHORE		(!Share->compare_exchange)
#endif

extern unsigned long*	_procbits;
extern long		_procbits_sz;

__inline void clr_procbit(int n)
{
	if (_procbits == NULL)
		return;
	if ((n>>7) < _procbits_sz)
	        _procbits[n>>7] &= ~(1L<< ((n>>2)&0x1f));
	else
		logmsg(0,"clr_procbit overflow hp=0x%x max=0x%x",n,_procbits_sz);
}

#ifdef BLOCK_TRACE
/* keep a bit map of allocated blocks
 * also keep track of last 256 block alloc/free calls for dumping
 * this is only atomic when USE_SEMAPHORE is non-zero
 */
static unsigned int*	_save;
static unsigned short*	_save_blk;

static void dumpit(int n)
{
	register unsigned short *sp = _save_blk;
	int i = Share->nblockops&255;
	while(n-->0)
	{
		if(--i<0)
			i = 255;
		logmsg(0, (sp[i]&0x1000)?"nfree=%d":"nalloc=%d", sp[i]&~0x1000);
	}
}

__inline unsigned int block_mark(register unsigned x)
{
	register unsigned int r=Share->nblockops;
	InterlockedIncrement(&Share->nblockops);
	if(_save_blk)
		_save_blk[r&0xff] = x;
	r = _save[x/32] & (1<<(x&0x1f));
	_save[x/32] |= (1<< (x&0x1f));
	return(r);
}
__inline unsigned int block_clear(register unsigned x)
{
	register unsigned int r=Share->nblockops;
	InterlockedIncrement(&Share->nblockops);
	if(_save_blk)
		_save_blk[r&0xff] = x|0x1000;
	r = _save[x/32] & (1<<(x&0x1f));
	_save[x/32] &= ~(1<<(x&0x1f));
	return(r);
}
#endif /* BLOCK_TRACE */

#if 0
static void proc_check(short chain[],int myslot)
{
	Pproc_t *proc;
	register int i,slot,count2,count=Share->nprocs;
	unsigned short *next;
	for(i= 0; i < Share->nprocs; i++)
	{
		for(slot=chain[i]; slot>0 && count-->0; slot=proc->next)
		{
			if(slot==myslot)
			{
				logmsg(0, "allocated block in use=%d", slot);
				breakit();
				continue;
			}
			count2 = 10;
			proc = proc_ptr(slot);
			for(next= &proc->group; *next && count2-->0; next= &proc->group)
			{
				if(*next==myslot)
				{
					logmsg(0, "unallocated block in use myslot=%d slot=%d chain[i]=%d type=%d btype=0x%x grp=%d pgroup=%d", myslot, slot, chain[i], Blocktype[slot]&BLK_MASK, Blocktype[slot], proc->pgid, proc->group);
					breakit();
				}
			}
		}
	}
}
#endif

static HANDLE	Proc_mutex[UWIN_SHARE_NUM_MUTEX];
static short	Offsets[UWIN_SHARE_NUM_MUTEX];
static	int	lockcount[UWIN_SHARE_NUM_MUTEX];

HANDLE proc_mutex(int flag, int v)
{
	HANDLE hp;
	int i,offset;
	char name[UWIN_RESOURCE_MAX];
	if(!flag)
	{
		for(i=0; i < elementsof(Proc_mutex); i++)
			Proc_mutex[i] = 0;
		return(0);
	}
	if(flag > sizeof(Proc_mutex))
		return(0);
	i = flag-1;
	offset= Share->mutex_offset[i]<<3;
	if((hp=Proc_mutex[i]) && (v || Offsets[i]==offset))
	{
		if(Offsets[i]!=offset)
			Proc_mutex[i] = 0;
		return(hp);
	}
	if(Proc_mutex[i])
		CloseHandle(Proc_mutex[i]);
	UWIN_MUTEX_PROC(name, offset+i);
	Offsets[i] = offset;
	if(!(Proc_mutex[i] = CreateMutex(sattr(0),FALSE,name)))
		logerr(0, "CreateMutex %s", name);
	return(Proc_mutex[i]);
}

#ifdef BLK_CHECK
static int get_lock(int type,const char *file, int line)
#define get_lock(a)	get_lock(a,file,line)
#else
static int get_lock(int type)
#endif
{
	unsigned int ntry=1, ndone;
	register HANDLE mutex;

	while(1)
	{
		Pproc_t *pp=0;
		DWORD ntpid=0;
		int pid=0,err,n=-1;
		char *name="???";
		if(mutex=proc_mutex(type,0))
		{
			SetLastError(0);
			ndone = Share->lockdone[type];
			InterlockedIncrement(&Share->lockwait[type-1]);
			if(ntry==1 && type==3)
				logmutex('P',type,file,line);
			if((n=WaitForSingleObject(mutex,250*type*Share->lockwait[type-1]))==WAIT_OBJECT_0)
				break;
			err=GetLastError();
			if(Share->olockslot[type])
			{
				pp = proc_ptr(Share->olockslot[type]);
				pid = pp->pid;
				ntpid = pp->ntpid;
				name = pp->prog_name;
			}
			if(n==WAIT_ABANDONED)
			{
				logmsg(0, "mutex_abandoned  type %d mutex=%p slot=%d inuse=%d pid=%x-%d name=%s" FMT_ATBLK, type, mutex, Share->lockslot[type], proc_ptr(Share->lockslot[type])->inuse, ntpid,pid, name BLK_ARGS);
				Share->lockslot[type] = 0;
				Share->lockdone[type]++;
				break;
			}
		}
		else
		{
			InterlockedDecrement(&Share->lockwait[type-1]);
			logerr(0, "P(%d)", type);
			n=0;
			break;
		}
		if(n==WAIT_TIMEOUT)
		{
			if(Share->lockdone[type] >= ndone)
				ndone = Share->lockdone[type] - ndone;
			else
				ndone = Share->lockdone[type]+0x10000-ndone;
			if(ntry > Share->maxltime)
			{
				Share->maxltime = ntry;
				logmsg(1, "maxltime type %d time out ntry=%d mutex=%p nwait=%d slot=%d-%d pid=%x-%d at %s:%d-%d name=%s ndone=%d" FMT_ATBLK, type, ntry, mutex, Share->lockwait[type-1],Share->lockslot[type], Share->olockslot[type], ntpid,pid, &Share->lastlfile[6*type],Share->lastlline[type],name,lockcount[type],ndone BLK_ARGS);
				mutexdump();
			}
			else if(ntry>1)
			{
				logmsg(1, "mutex type %d time out ntry=%d mutex=%p nwait=%d slot=%d-%d pid=%x-%d at %s:%d-%d name=%s ndone=%d" FMT_ATBLK, type, ntry, mutex, Share->lockwait[type-1],Share->lockslot[type], Share->olockslot[type], ntpid,pid, &Share->lastlfile[6*type],Share->lastlline[type],name,lockcount[type],ndone BLK_ARGS);
				mutexdump();
			}
			if(ndone || ++ntry<12)
				continue;
			closehandle(mutex,HT_MUTEX);
			Proc_mutex[type-1] = 0;
			Share->mutex_offset[type-1]++;
			logmsg(0, "mtimeout type %d time out ntry=%d mutex=%p slot=%d pid=%x-%d name=%s" FMT_ATBLK, type, ntry, mutex, Share->lockslot[type], ntpid,pid, name BLK_ARGS);
			logmsg(4, "try again");
		}
		else
		{
			closehandle(mutex,HT_MUTEX);
			Proc_mutex[type-1] = 0;
			Share->mutex_offset[type-1]++;
			logmsg(0, "mutex_failed P(%d) retcode=%d lasterr=%d ntry=%d mutex=%p slot=%d pid=%x-%d name=%s" FMT_ATBLK, type, n, GetLastError(), ntry, mutex, Share->lockslot[type], ntpid,pid, name BLK_ARGS);
			Share->lockslot[type] = 0;
			logmsg(4, "try again");
			Sleep(100);
		}

	}
	if(type==3)
		logmutex('R',type,file,line);
	InterlockedDecrement(&Share->lockwait[type-1]);
#if 0
	if(Share->lockslot[type] && P_CP && proc_slot(P_CP)!=Share->lockslot[type])
		logmsg(0,"get_lock type=%d already locked by %d count=%d",type,Share->lockslot[type],lockcount[type]);
#endif
	if(P_CP)
		Share->lockslot[type] = proc_slot(P_CP);
	else
		Share->lockslot[type] = (unsigned short)-1;
	Share->olockslot[type] = Share->lockslot[type];
	Share->lockmutex[type].mutex = mutex;
	Share->locktid[type] = GetCurrentThreadId();
#ifdef BLK_CHECK
	Share->lastlline[type] = line;
	memcpy(&Share->lastlfile[6*type],file,5);
#endif
	lockcount[type]++;
	return ntry;
}

#if BLK_CHECK
    __inline unsigned __int64 cycles(void)
    {
#if _X86_
        unsigned int	lo;
        unsigned int	hi;
	unsigned __int64 n;
        _asm _emit 0Fh
        _asm _emit 31h
        _asm mov hi, edx
        _asm mov lo, eax
	n = hi;
	return(n = (n<<32)|lo);
#else
	LARGE_INTEGER	n;

	if (QueryPerformanceCounter(&n))
		return n.QuadPart;
#endif
	return 0;
    }

    static void dumpmutex(void)
    {
	char	buff[100], file[7],*name;
	int	i,k,mutexstate;
	DWORD n;
	Pproc_t	*pp;
	struct Mutexlog *mp = Mlog;
	unsigned _int64 t1,t2,otime,scale;
	FILETIME  now;
	DWORD ocount;
	scale = cycles();
	time_getnow(&now);
	Share->dcount = 0;
	t1 = QUAD(Share->start.dwHighDateTime,Share->start.dwLowDateTime);
	t2 = QUAD(now.dwHighDateTime,now.dwLowDateTime);
	scale /= (10*(t2-t1));
	Share->inlog = 1;
	n = (DWORD)sfsprintf(buff,sizeof(buff),"   COUNTER TYPE OP  SLOT  NTPID FILE:LINE STATE    TIME       DIFF   NAME\n");
	WriteFile(state.log,buff,n,&n,NULL);
	file[6] = 0;
	ocount = mp->counter;
	for(k=1; k <= MUTMASK; k++,mp++)
	{
		if(mp->counter < ocount)
			break;
		otime = mp->time;
		ocount = mp->counter;
	}
	k--;
	for(i=k+1; i!=k ; i= (i+1)&MUTMASK)
	{
		mp = Mlog + i;
		if(mp->counter < ocount)
			otime = mp->time;
		pp = 0;
		if(is_slotvalid(mp->slot))
			pp = proc_ptr(mp->slot);
		mutexstate = 0;
		name = "???";
		if(pp && (Blocktype[mp->slot]&BLK_MASK)==BLK_PROC)
		{
			mutexstate = pp->state;
			name = pp->prog_name;
		}
		memcpy(file,mp->file,6);
		n = (DWORD)sfsprintf(buff,sizeof(buff),"%10u   %d   %c %4d %6x %6s:%4d  %2d %10llu %8llu %s\n", mp->counter,mp->type,mp->op,mp->slot,mp->thid,file,mp->line,mutexstate,(mp->time<<8)/scale,mp->time-otime,name);
		WriteFile(state.log,buff,n,&n,NULL);
		otime = mp->time;
		ocount = mp->counter;

	}
	Share->inlog = 0;
    }

    static void logmutex(char op,int type, const char *file, int line)
    {
	struct Mutexlog *mp,mutlog;
	if(!cycle0)
	{
		if(!Share->cycle0)
			Share->cycle0 = cycles();
		cycle0 = Share->cycle0;
	}
	mutlog.counter = InterlockedIncrement(&Share->nmutex);
	if(Share->inlog)
		return;
	if(Share->dcount==mutlog.counter)
		dumpmutex();
	mutlog.slot = proc_slot(P_CP);
	mutlog.type = type;
	mutlog.line = line;
	mutlog.op = op;
	strncpy(mutlog.file,file,sizeof(mutlog.file));
	mutlog.time = (DWORD)((cycles()-cycle0)>>8);
	mutlog.thid = GetCurrentThreadId();
	if(!Mlog)
		Mlog = (struct Mutexlog*)((char*)Pproctab+BLOCK_SIZE*(Share->block+7));
	mp = Mlog+(mutlog.counter&MUTMASK);
	memcpy(mp,&mutlog,sizeof(mutlog));

    }

 int XP(int type,const char* file,int line)
#else
 int P(int type)
#endif
{
	/* block signals */
	int sigsys = 1;

	if(P_CP)
		sigsys = sigdefer(3);
	if (type == 0)
	{
		int i;
		HANDLE hp;

		/* for all locks; if mutex has been used, acquire lock */
		for (i=elementsof(Proc_mutex); --i>=0;)
			if ((hp=Proc_mutex[i]))
				get_lock(i+1);
	}
	else
		get_lock(type);
	return(sigsys);
}

#if BLK_CHECK
    static void rel_lock(int type, int sigstate, const char* file, int line)
#   define rel_lock(a,b)	rel_lock(a,b, file,line)
#else
    static void rel_lock(int type, int sigstate)
#endif
{
	HANDLE hp = proc_mutex(type,1);
	unsigned short lslot = Share->lockslot[type];
	DWORD	tid = Share->locktid[type];
	if(P_CP && proc_slot(P_CP)!=lslot && (lslot||Proc_mutex[type-1]))
		logmsg(0,"rel_lock type=%d lockslot=%d count=%d",type,lslot,lockcount[type]);
	if(--lockcount[type]==0)
	{
		Share->lockslot[type] = 0;
		Share->locktid[type] = 0;
	}
	Share->lockdone[type]++;
	if(type==3)
		logmutex('V',type,file,line);
	if(!ReleaseMutex(hp))
	{
		int v = WaitForSingleObject(hp,0);
		logerr(0, "ReleaseMutex V(%d,%d) hp=%p mutex=%p failed slot=%d pid=%d tid=0x%x waitthid=0x%x name=%s at %s:%d offset=%d v=%d count=%d", type, sigstate, hp, Share->lockmutex[type].mutex, lslot, P_CP->pid, tid,P_CP->wait_threadid,P_CP->prog_name,&Share->lastlfile[6*type],Share->lastlline[type],Offsets[type-1],v,Share->nmutex);
		mutexdump();
	}
	Share->olockslot[type] = 0;
	if(Proc_mutex[type-1]==0)
		closehandle(hp,HT_MUTEX);
}

#ifdef BLK_CHECK
 void XV(int type,int sigstate, const char* file,int line)
#else
 void V(int type,int sigstate)
#endif
{
	if (type == 0)
	{
		int i, offset;
		HANDLE hp;

		/* for all locks; if lock has been used, release it */
		for (i=0; i < elementsof(Proc_mutex); i++)
		{
			offset= Share->mutex_offset[i]<<3;
			if((hp=Proc_mutex[i]) && Offsets[i]==offset)
				rel_lock((i+1), sigstate);
		}
	}
	else
	{
		rel_lock(type, sigstate);
	}
	if(!sigstate && sigispending(P_CP))
		logmsg(LOG_PROC+3, "signal pending on V() sig=0x%x", P_CP->siginfo.siggot);
	sigcheck(sigstate);
}

static int block_isfree(Blockhead_t *blk)
{
	unsigned long next,index = block_slot(blk);
	if(index==0 || index >= (unsigned long)Share->nfiles)
		return(0);
	for(next=Share->top; next;  next = blk->next)
	{
		if(next==index)
			return(index);
		blk = block_ptr(next);
	}
	return(0);
}

#ifdef BLK_CHECK
 void Xblock_free(register unsigned short index, const char *file, int line)
#else
 void block_free(register unsigned short index)
#endif
{
	register Blockhead_t *blk;
	long slot;
#ifdef USE_SEMAPHORE
	int sigstate;
#endif
	if(index==0 || index>Share->block)
	{
		logmsg(0, "block_free called bad block=%d" FMT_ATBLK, index BLK_ARGS);
		return;
	}
#ifdef BLK_CHECK
	Share->lastline =  line;
	memcpy(Share->lastfile,file,5);
#endif
#ifdef USE_SEMAPHORE
	if(USE_SEMAPHORE)
		sigstate = P(1);
#endif
	if((Blocktype[index]&BLK_MASK)==BLK_FREE)
	{
		int otype = Blocktype[index]>>4;
		char *name = (otype==1?block_ptr(index):"unknown");
		logmsg(0, "attempt to free already freed block=%d type=%d otype=%d btype=0x%x name=%s"FMT_ATBLK, index, Blocktype[index]&BLK_MASK,otype&BLK_MASK,Blocktype[index],name BLK_ARGS);
#ifdef USE_SEMAPHORE
		if(USE_SEMAPHORE)
			V(1,sigstate);
#endif
		return;
	}
#ifdef BLOCK_TRACE
	if(!_save)
		_save = (unsigned int*)block_ptr(Share->bitmap);
	if(!_save_blk && Share->saveblk)
		_save_blk = (unsigned short*)block_ptr(Share->saveblk);
	if(!block_clear(index))
	{
		int otype = Blocktype[index]>>4;
		logmsg(0, "block_freed that wasn't allocated index=%d type=%d otype=%d btype=0x%x"FMT_ATBLK, index,Blocktype[index]&BLK_MASK,otype&BLK_MASK,Blocktype[index] BLK_ARGS);
		dumpit(20);
	}
#endif
	blk = block_ptr(index);
	if((Blocktype[index]&BLK_MASK)==BLK_FREE && block_isfree(blk))
	{
		int otype = Blocktype[index]>>4;
		logmsg(0, "attempt2 to free already freed block=%d type=%d otype=%d btype=0x%x"FMT_ATBLK, index,Blocktype[index]&BLK_MASK,otype&BLK_MASK,Blocktype[index] BLK_ARGS);
#ifdef USE_SEMAPHORE
		if(USE_SEMAPHORE)
			V(1,sigstate);
#endif
		return;
	}
	Blocktype[index] <<= 4;
	Blocktype[index] |= BLK_FREE;
	InterlockedIncrement(&Share->nblocks);
	while(1)
	{
		slot = block_slot(blk);
		blk->next = Share->top;
#ifdef USE_SEMAPHORE
		if(USE_SEMAPHORE)
		{
			Share->top = slot;
			V(1,sigstate);
			break;
		}
#endif
		if(COMPARE_EXCHANGE(&Share->top,slot,blk->next)==blk->next)
			return;
		logmsg(LOG_PROC+3, "block_free: concurrent access to slot=%d", slot);
		Sleep(1);
	}
}

static int _comp(int a, int b)
{
	return(a==b);
}

static void dump_chain(char *name,int slot, unsigned short *first, int offset)
{
	Pproc_t *pp;
	char buff[1024];
	char *cp,*cpend = &buff[sizeof(buff)];
	int count=40;
	cp = buff;
	*cp = 0;
	while(*first && --count)
	{
		cp += sfsprintf(cp,cpend-cp," %d",*first);
		pp = proc_ptr(*first);
		first = (unsigned short*)((char*)pp+offset);
	}
	if(*first)
		sfsprintf(cp,cpend-cp,"*");
	if (*buff)
		logmsg(0, "%s chain %d:%s", name, slot, buff);
}

Pproc_t *proc_create(DWORD ntpid)
{
	register Pproc_t *proc;
	register unsigned int i;
	unsigned short slot = block_alloc(BLK_PROC);
	if(slot==0)
	{
		errno = EAGAIN;
		return(0);
	}
	proc = (Pproc_t*)block_ptr(slot);
	memset(proc, 0, sizeof(*proc));
	proc->ntpid = ntpid;
	proc->ppid = 1;
	proc->sid = 1;
	proc->pgid = 1;
	proc->gid = (gid_t)-1;
	proc->egid = (gid_t)-1;
	proc->pid = nt2unixpid(ntpid);
	proc->curdir = -1;
	proc->rootdir = -1;
	if(proc->pid < 0)
	{
		proc->pid = -proc->pid;
		if(!(proc->pid&1))
			logmsg(0, "pid not odd pid=%x-%d", ntpid, proc->pid);
		proc->pid >>= 1;
	}
	for(i=0; i < RLIM_NLIMITS; i++)
		proc->cur_limits[i] = proc->max_limits[i] = RLIM_INFINITY;;
	proc->max_limits[RLIMIT_NOFILE] = proc->cur_limits[RLIMIT_NOFILE] = OPEN_MAX;
	for(i=0; i < OPEN_MAX; i++)
		setfdslot(proc,i,-1);
	proc->state = PROCSTATE_SPAWN;
	proc->siginfo.sp_flags  = TRUE;
	sigdefer(1);
	time_getnow(&proc->start);
	return(proc);
}

int proc_active(Pproc_t *proc)
{
	HANDLE	ph;
	DWORD	status = 0;
	if (!is_slotvalid(proc_slot(proc)))
	{
		logmsg(0, "proc_active invalid slot=%d btype=0x%x", proc_slot(proc), (proc_slot(proc)?Blocktype[proc_slot(proc)]:0xbad));
		return(0);
	}

	if(!(ph=proc->ph) || GetProcessId(proc->ph)!=proc->ntpid)
		ph = OpenProcess(PROCESS_DUP_HANDLE|PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,proc->ntpid);
	GetExitCodeProcess(ph,&status);
	if(ph && ph!=proc->ph)
	{
		DWORD pid=proc->pid;
		Pproc_t *xp =  proc_ntgetlocked(proc->ntpid,0);
		if(xp)
		{
			pid = xp->pid;
			proc_release(xp);
		}
		if(pid!=proc->pid)
		{
			logmsg(0,"practive pid=%x-%d pid=%d",proc->ntpid,proc->pid,pid);
			setlastpid(proc->ntpid);
			closehandle(ph,HT_PROC);
			return(0);
		}
		closehandle(proc->ph,HT_PROC);
		if(proc->ph)
			proc->ph = ph;
		else
		{
			setlastpid(proc->ntpid);
			closehandle(ph,HT_PROC);
		}
	}
	return(status==STILL_ACTIVE);
}

#ifdef BLK_CHECK
void Xproc_release(register Pproc_t *proc, const char *file, int line)
#else
void proc_release(register Pproc_t *proc)
#endif /* BLK_CHECK */
{
	if(proc->inuse<=0)
	{
		static Pproc_t	*curproc;
		DWORD		ntpid=0;
		HANDLE		ph=0;
		int	 	sigstate,type,pid,ppid;
		unsigned short	slot = proc_slot(proc);
		proc->posixapp |= IN_RELEASE;
		if(proc==P_CP || proc==curproc)
		{
			proc->posixapp &= ~IN_RELEASE;
			return;
		}
		type = Blocktype[slot]&BLK_MASK;
		if(type!=BLK_PROC || proc->inuse<0)
			return;
		if(proc->pid > (Share->vforkpid+4) && proc->ppid==P_CP->pid)
			ph = OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,proc->ntpid);
		ntpid = proc->ntpid;
		if(ph && proc->ph)
		{
			DWORD pid;
			if((pid=GetProcessId(proc->ph))==ntpid)
				closehandle(proc->ph,HT_PROC);
			else
				logmsg(0,"release1" FMT_ATBLK" slot=%d pid=%x-%d mypid=%d err=%d" BLK_ARGS,slot,ntpid,proc->pid,pid,GetLastError());
		}
		proc->ph = ph;
		sigstate = P(3);
		type = Blocktype[slot]&BLK_MASK;
		curproc = proc;
		if(type!=BLK_PROC || proc->inuse<0)
		{
			proc->posixapp &= ~IN_RELEASE;
			logmsg(LOG_PROC+1,"premature_release" FMT_FMBLK " type=%d otype=%d btype=0x%x inuse=%d slot=%d pid=%d parent=%d posix=0x%x state=%(state)s" BLK_ARGS,type,(Blocktype[slot]&BLK_MASK>>4),Blocktype[slot],proc->inuse,proc_slot(proc),proc->pid,proc->ppid,proc->posixapp,proc->state);
			setlastpid(proc->ntpid);
			closehandle(ph,HT_PROC);
			curproc = 0;
			V(3,sigstate);
			return;
		}
		if (proc->posixapp & ON_WAITLST)
		{
			proc->posixapp &= ~IN_RELEASE;
			logmsg(LOG_PROC+1,"premature_release" FMT_FMBLK " inuse=%d slot=%d pid=%d parent=%d posix=0x%x state=%(state)s" BLK_ARGS,proc->inuse,proc_slot(proc),proc->pid,proc->ppid,proc->posixapp,proc->state);
			setlastpid(proc->ntpid);
			closehandle(ph,HT_PROC);
			curproc = 0;
			V(3,sigstate);
			return;
		}
		if(ntpid!=proc->ntpid)
			logmsg(0,"ntpidchanged" FMT_FMBLK " slot=%d pid=%x-%d ntpid=%x" BLK_ARGS,proc_slot(proc),proc->ntpid,proc->pid,ntpid);
		if(!proc_exec_exit && ph)
		{
			int 		status= -1;
			WaitForSingleObject(ph,100);
			if(GetExitCodeProcess(ph,&status) && status==STILL_ACTIVE)
			{

				proc->posixapp &= ~IN_RELEASE;
				logmsg(0,"active_process" FMT_FMBLK " delete attempt for pid=%x-%d slot=%d posix=0x%x state=%(state)s" BLK_ARGS,proc->ntpid,proc->pid,proc_slot(proc),proc->posixapp,proc->state);
				curproc = 0;
				if(proc->ppid != P_CP->pid)
					closehandle(ph,HT_PROC);
				V(3,sigstate);
				return;
			}
			setlastpid(proc->ntpid);
			closehandle(ph,HT_PROC);
			ph = 0;
		}
		logmsg(LOG_PROC+3,"proc_release" FMT_ATBLK " pid=%d slot=%d inuse=%d state=%(state)s" BLK_ARGS,proc->pid,proc_slot(proc),proc->inuse,proc->state);
		if(proc->maxfds)
			fdcloseall(proc);
		if(proc->pid!=1 && proc->pid && proc_deletelist(proc)==0)
		{
			proc->posixapp &= ~IN_RELEASE;
			logmsg(LOG_PROC+5, "grp_ldr_entries" FMT_FMBLK " slot=%d inuse=%d gent=%d posix=0x%x state=%(state)s" BLK_ARGS, proc_slot(proc), proc->inuse, proc->group, proc->posixapp, proc->state);
			curproc = 0;
			V(3,sigstate);
			//if (P_CP->pid==1)
			if(ph)
				logmsg(0, "return with open handle=%04x", ph);
			return;
		}
		else if(proc->group && proc->pid!=1 && !(proc->inexec&PROC_INEXEC))
		{
			/* actually a group leader, but possibly phandle invalid */
			if (proc->posixapp & (UWIN_PROC|POSIX_PARENT))
			{
				proc->posixapp &= ~IN_RELEASE;
				logmsg(1,"grp_ldr_still_active" FMT_FMBLK " slot=%d pid=%x-%d phand=%04x inexec=0x%x posix=0x%x state=%(state)s pgid=%d gent=%d parent=%d name=%s cmd=%s" BLK_ARGS,proc_slot(proc),proc->ntpid,proc->pid,proc->phandle,proc->inexec,proc->posixapp,proc->state,proc->pgid,proc->group,proc->parent,proc->prog_name,proc->cmd_line);
				if(proc->state!=PROCSTATE_EXITED)
					curproc = 0;
				V(3,sigstate);
				if (ph)
					logmsg(0, "return with open handle=%p", ph);
				return;
			}
		}
		/* proc_deletelist succeeded, if necessary clear proc->ph */
		if (ph == 0)
			proc->ph = 0;
		/* Don't have proc_cleanup harvest this process slot */
		logmsg(LOG_PROC+3, "del_proc" FMT_ATBLK " slot=%d inuse=%d" BLK_ARGS, proc_slot(proc), proc->inuse);
		/* avoid race by freeing block before closing handle(s) */
		if(proc->ppid == P_CP->pid)
			ph = proc->ph;
		else if(proc->pid!=1)
			logmsg(LOG_PROC+2, "proc_release not self" FMT_FMBLK " slot=%d pid=%d ppid=%d" BLK_ARGS, proc_slot(proc), proc->pid, proc->ppid);
		if(proc->origph && proc->ppid==P_CP->pid)
		{
			DWORD code;
			if(GetExitCodeProcess(proc->origph,&code))
				closehandle(proc->origph,HT_PROC);
			else
				logerr(0,"origphy" FMT_ATBLK " origph=%p slot=%d pid=%x-%d ppid=%d state=%(state)s" BLK_ARGS,proc->origph,proc_slot(proc),proc->ntpid,proc->pid,proc->ppid,proc->state);
			clr_procbit((unsigned long)proc->origph);
		}
		proc->state = PROCSTATE_UNKNOWN;
		pid = proc->pid;
		ntpid = proc->ntpid;
		ppid = proc->ppid;
		type = proc_slot(proc);
		ph = proc->ph;
#ifdef DEBUG_GRP_LDR
		if(proc->posixapp & ORPHANING)
			logmsg(0, "free_grp_ldr" FMT_FMBLK " slot=%d inuse=%d gent=%d posix=0x%x state=%(state)s" BLK_ARGS, proc_slot(proc), proc->inuse, proc->group, proc->posixapp, proc->state);
#endif
		InterlockedDecrement(&proc->inuse);
		V(3,sigstate);
		if(type==proc_slot(P_CP))
			logmsg(0,"releaseme");
		block_free(type);
		curproc = 0;
		if(ph && !closehandle(ph, HT_PROC))
			logerr(0,"CloseHandle" FMT_ATBLK " ph=%p slot=%d pid=%x-%d parent=%d" BLK_ARGS,ph,type,ntpid,pid,ppid);
		return;
	}
	InterlockedDecrement(&proc->inuse);
}

/*
 * Let each of the child processes be inherited by init
 * PROCSTATE_EXITED processes have already been handled so skip them
 */
void  proc_orphan(pid_t parent)
{
	HANDLE 		ph=0,ph2;
	DWORD		status;
	register Pproc_t *proc, *pproc;
	register int	sigstate;
	int		pchld=-1, snchld, cnt=0;
	int	sigstatus;
	if(parent<=3 || !(pproc=proc_getlocked(parent, 0)))
		return;
	sigstatus = sigdefer(3);
	if (pproc->posixapp & ORPHANING)
	{
		logmsg(LOG_PROC+5, "already orphaning slot=%d thrd=0x%x name=%s cmd=%s", proc_slot(pproc), GetCurrentThreadId(), pproc->prog_name, pproc->cmd_line);
		sigcheck(sigstatus);
		proc_release(pproc);
		return;
	}
	pproc->posixapp |= ORPHANING;
	if(parent==P_CP->pid)
		ph = GetCurrentProcess();
	if (!pproc->nchild && pproc->child)
		logmsg(0,"nchild=%d but child=%d slot=%d pid=%x-%d state=%(state)s", pproc->nchild,pproc->child,proc_slot(pproc),pproc->ntpid,pproc->pid,pproc->state);
	logmsg(LOG_PROC+4, "proc_orphan pid=%d slot=%d state=%(state)s nchild=%d child=%d", parent, proc_slot(pproc),pproc->state,pproc->nchild, pproc->child);
	if(pproc->child && !ph)
		ph = open_proc(pproc->ntpid,PROCESS_QUERY_INFORMATION|SYNCHRONIZE);
	pchld = pproc->child;
	snchld = pproc->nchild;
	while (1)
	{
		sigstate = P(3);
		if(pproc->child==0)
		{
			if (cnt!=snchld && snchld!=0)
			{
				logmsg(0,"orphanerr slot=%d inuse=%d pid=%x-%d nchild=%d snchld=%d cnt=%d pchld=%d forksp=%p name=%s cmd=%s",proc_slot(pproc),pproc->inuse,pproc->ntpid,pproc->pid,pproc->nchild,snchld,cnt,pchld,pproc->forksp,pproc->prog_name,pproc->cmd_line);
				if (pchld >=0)
				{
					if ((Blocktype[pchld]&BLK_MASK) == BLK_PROC)
						logmsg(0,"orphan_prevchld slot=%d inuse=%d pid=%x-%d ppid=%d parent=%d sibling=%d state=%(state)s name=%s cmd=%s",pchld,proc_ptr(pchld)->inuse,proc_ptr(pchld)->ntpid,proc_ptr(pchld)->pid,proc_ptr(pchld)->ppid,proc_ptr(pchld)->parent,proc_ptr(pchld)->sibling,proc_ptr(pchld)->state,proc_ptr(pchld)->prog_name,proc_ptr(pchld)->cmd_line);
					else
						logmsg(0,"orphan_prevchd slt=%d btype=0x%x", pchld, Blocktype[pchld]);
				}
			}
			V(3,sigstate);
			break;
		}
		cnt++;
		proc = proc_ptr(pproc->child);
		pproc->child = proc->sibling;
		pchld = pproc->child;
		proc->sibling = 0;
		InterlockedDecrement(&pproc->nchild);
		V(3,sigstate);
		if(proc->forksp)
			continue;
		/*
		 * When the parent process terminates and it has
		 * any stopped children send SIGHUP followed by a
		 * SIGCONT signal
		 */
		//logmsg(LOG_PROC+3,"orphan slot=%d pid=%x-%d ppid=%d state=%(state)s",proc_slot(proc),proc->ntpid,proc->pid,proc->ppid,proc->state);
		if (proc->ppid==P_CP->pid)
			ph2 = proc->ph;
		else
			ph2 = open_proc(proc->ntpid,PROCESS_QUERY_INFORMATION|SYNCHRONIZE);
		if(procexited(proc) && !(ph2 || GetExitCodeProcess(ph2,&status) && status==STILL_ACTIVE))
		{
			proc->posixapp &= ~ON_WAITLST;
			proc_release(proc);
		}
		else
		{
			proc->posixapp &= ~ON_WAITLST;
			proc_reparent(proc,Share->init_slot);
			if(proc->state == PROCSTATE_STOPPED )
			{
				kill(-proc->pid, SIGHUP);
				kill(-proc->pid, SIGCONT);
			}
		}
		if (ph2 && proc->ppid!=P_CP->pid)
			closehandle(ph2,HT_PROC);
	}
	sigcheck(sigstatus);
	if(ph && parent!=P_CP->pid)
		closehandle(ph,HT_PROC);
	if(pproc->nchild)
		logmsg(0,"deadorphans nchild=%d child=%d slot=%d pid=%x-%d name=%s",pproc->nchild,pproc->child,proc_slot(pproc),pproc->ntpid,pproc->pid,pproc->prog_name);
	pproc->posixapp &= ~ORPHANING;
	proc_release(pproc);
}

/*
 * return a pointer to the process structure corresponding to
 * the given <pid>.  The reference count is incremented.
 */
Pproc_t *proc_getlocked(pid_t pid, int all)
{
	register Pproc_t *proc;
	register short *next;
	int count;
	unsigned short slot =  proc_hash(pid);
	if(pid<1)
		return(0);
	count=Share->nprocs;
	for(next= &Pprochead[slot];*next && count-->0;next= &proc->next)
	{
		if(!is_slotvalid(*next))
			break;
		proc = proc_ptr(*next);
		if(proc->pid!=pid || proc->state==PROCSTATE_EXITED && !all)
			continue;
		if(InterlockedIncrement(&proc->inuse))
			return(proc);
		proc_release(proc);
		return(0);
	}
	if (count <= 0)
		logmsg(0,"no_proc pid=%d count=%d nprocs=%d",pid,count,Share->nprocs);
	return(0);
}

/*
 * return a pointer to the process structure corresponding to
 * the given <ntpid>.  The reference count is incremented.
 * almost the same as above, except ntpid is given instead
 */
Pproc_t *proc_ntgetlocked(DWORD ntpid,int all)
{
	register Pproc_t *proc;
	register short *next;
	int count;
	unsigned short slot =  proc_hash(ntpid);
	if(ntpid==0)
		return(0);
	count=Share->nprocs;
	for(next= &Pprocnthead[slot];*next && count-->0;next= &proc->ntnext)
	{
		if(!is_slotvalid(*next))
			break;
		proc = proc_ptr(*next);
		if(proc->ntpid!=ntpid || (proc->state==PROCSTATE_EXITED && !all))
			continue;
		if(InterlockedIncrement(&proc->inuse))
			return(proc);
		proc_release(proc);
	}
	if (count <= 0)
		logmsg(0,"no_proc ntpid=%x",ntpid);
	return(0);
}

/*
 * ntpid is the old ntpid
 */
void proc_setntpid(register Pproc_t *pp, DWORD ntpid)
{
	unsigned short slot;
	short s,*next;
	int sigstate,count = Share->nprocs;
	int me = proc_slot(pp);
	if(pp->ntpid==ntpid)
		return;
	sigstate = P(3);
	slot = proc_hash(ntpid);
	/* delete from the old chain */
	next= &Pprocnthead[slot];
	while(*next && (s= *next)!=me && count-->0)
	{
		if(!is_slotvalid(s))
			break;
		next = &(proc_ptr(s)->ntnext);
	}
	if(!next || count<=0)
		logmsg(0, "setntpid failed count=%d slot=%d pid=%x-%d ntpid=%x", count, proc_slot(pp), pp->ntpid, pp->pid, ntpid);
	if(next && (me== *next))
		*next = pp->ntnext;
	/* add to the new chain */
	slot =  proc_hash(pp->ntpid);
	pp->ntnext = Pprocnthead[slot];
	Pprocnthead[slot] = proc_slot(pp);
	V(3,sigstate);
}

int pause(void)
{
	sigdefer(1);
	P_CP->state = PROCSTATE_SIGWAIT;
	WaitForSingleObject(P_CP->sigevent,INFINITE);
	P_CP->state = PROCSTATE_RUNNING;
	sigcheck(0);
	errno = EINTR;
	return(-1);
}

static DWORD (PASCAL *exitproc)(DWORD);

static DWORD WINAPI exit_thread(void* arg)
{
	int cnt = 0;
	if(!exitproc)
		exitproc = (DWORD(PASCAL*)(DWORD))getsymbol(MODULE_kernel,"ExitProcess");
	if (P_CP->maxfds)
		fdcloseall(P_CP);
	while (sigcheck(3) && cnt < 5)
	{
		Sleep(200);
		cnt++;
	}
	if (cnt>1)
		logmsg(0, "exit_thread arg=%d cnt=%d slot=%d inuse=0x%x pid=%x-%d pgid=%d ph=%04x phand=%04x nchild=%d gent=%d parent=%d sibling=%d posix=0x%x state=%(state)s name=%s cmd=%s", (DWORD)arg, cnt, proc_slot(P_CP), P_CP->inuse, P_CP->ntpid, P_CP->pid, P_CP->pgid, P_CP->ph, P_CP->phandle, P_CP->nchild, P_CP->group, P_CP->parent, P_CP->sibling, P_CP->posixapp, P_CP->state, P_CP->prog_name, P_CP->cmd_line);
	(exitproc)((DWORD)arg);
	return(0);
}

/*
 * terminate thread, free thread stack on NT, and close thread handle
 */
int terminate_thread(HANDLE hp, DWORD code)
{
	void*				addr = 0;
	DWORD				ret;
	int				r;
	int				s;
	MEMORY_BASIC_INFORMATION	minfo;
	CONTEXT				context;

	if(!GetExitCodeThread(hp,&r))
	{
		logerr(0, "GetExitCodeThread thread=%04x",hp);
		return 0;
	}
	if(r!=STILL_ACTIVE)
		return(0);
	if(Share->Platform==VER_PLATFORM_WIN32_NT && SuspendThread(hp)>=0)
	{
		context.ContextFlags = UWIN_CONTEXT_TRACE;
		if(GetThreadContext(hp, &context) && (addr = stack_ptr(&context)) && VirtualQuery(addr, &minfo, sizeof(minfo)) != sizeof(minfo))
		{
			logmsg(0, "terminate_thread %04x VirtualQuery failed addr=%p", hp, addr);
			addr = 0;
		}
		ResumeThread(hp);
	}
	s = P(0);
	ret = TerminateThread(hp, code);
	V(0, s);
	if(!ret)
		logerr(0, "TerminateThread tid=%04x", hp);
	if(!closehandle(hp,HT_THREAD))
		logerr(0, "CloseHandle");
	if(addr)
		/* ignore any errors here since at least w7/32 already freed the stack */
		VirtualFree(minfo.AllocationBase, 0, MEM_RELEASE);
	return 1;
}

static proc_isrunning(DWORD ntpid)
{
	HANDLE pp;
	DWORD err=GetLastError();
	if(pp = OpenProcess(state.process_query,FALSE,ntpid))
	{
		DWORD status,r;
		r=GetExitCodeProcess(pp,&status);
		closehandle(pp,HT_PROC);
		if(!r && status!=STILL_ACTIVE)
			return(0);
	}
	else
		logerr(0, "OpenProcess ntpid=%x %s failed", ntpid, "QUERY_INFORMATION");
	SetLastError(err);
	return(1);
}

int terminate_proc(DWORD ntpid, DWORD sig)
{
	Pproc_t *pp;
	HANDLE hp=0,th=0;
	int id, procstate,n;
	if(ntpid==P_CP->ntpid)
	{
		fdcloseall(P_CP);
		if (P_CP->state != PROCSTATE_ABORT)
			P_CP->state = PROCSTATE_TERMINATE;
		P_CP->exitcode = (unsigned short)sig;
		logmsg(LOG_PROC+3, "terminate_proc TERMINATE WINPROC=%x ExitProcess 0x%x", ntpid, sig);
		ExitProcess(sig);
	}
	else if(pp = proc_ntgetlocked(ntpid,0))
	{
		procstate = pp->state;
		if (pp->state != PROCSTATE_ABORT)
			pp->state = PROCSTATE_TERMINATE;
		pp->exitcode = (unsigned short)sig;
		logmsg(LOG_PROC+3, "terminate_proc TERMINATE UWINPROC=%d sig 0x%x", pp->pid, sig);
		proc_release(pp);
	}
	if(!proc_isrunning(ntpid) || pp && pp->ntpid != ntpid)
	{
		logmsg(0, "terminate_proc ntpid %d isrunning %d pp.ntpid=%d sig 0x%x", ntpid, proc_isrunning(ntpid), pp ? pp->ntpid : -1, sig);
		return 0;
	}
	if(pp)
	{
		HANDLE event;
		char ename[UWIN_RESOURCE_MAX];
		UWIN_EVENT_SIG(ename, ntpid);
		sigcheck(-1);
		if(event=OpenEvent(EVENT_MODIFY_STATE,FALSE,ename))
		{
			SetEvent(event);
			closehandle(event, HT_EVENT);
		}
	}
	else
		sig <<= 8;
	if(!proc_isrunning(ntpid) || pp && pp->ntpid != ntpid)
	{
		logmsg(0, "terminate_proc ntpid %d isrunning %d pp.ntpid=%d sig 0x%x", ntpid, proc_isrunning(ntpid), pp ? pp->ntpid : -1, sig);
		return 0;
	}
	if(!exitproc)
		exitproc = (DWORD(PASCAL*)(DWORD))getsymbol(MODULE_kernel,"ExitProcess");
	/* is ast_exit running */
	if (pp && (pp->posixapp & ORPHANING) && pp->ntpid==ntpid)
	{
		/* Wait for process to terminate */
		n=0;
		if (hp = OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,ntpid))
		{
			n=WaitForSingleObject(hp, 2000);
			closehandle(hp,HT_PROC);
			if (n == WAIT_OBJECT_0)
				return(0);
		}
		logerr(0, "exiting_process didn't terminate ntpid=%d n=0x%x hp=%p slot=%d inuse=%d pid=%d-%d posix=0x%x state=%(state)s name=%s cmd=%s", n, ntpid, proc_slot(pp), pp->inuse, pp->ntpid, pp->pid, pp->posixapp, pp->state, pp->prog_name, pp->cmd_line);
	}
	/* Try calling ExitProcess in a remote thread */
	if((hp = OpenProcess(PROCESS_TERMINATE|PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|SYNCHRONIZE,FALSE,ntpid)) && (th=CreateRemoteThread(hp,NULL,0,exit_thread,(void*)sig,0,&id)))
	{
		n = WaitForSingleObject(th,500);
		closehandle(th,HT_THREAD);
		if(n==WAIT_OBJECT_0)
		{
			closehandle(hp,HT_PROC);
			return(0);
		}
		if(n!=WAIT_FAILED)
			SetLastError(n);
	}
	else if(!hp)
		hp=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,(DWORD)ntpid);
	if(hp)
	{
		int s;
		DWORD ret;

		if(pp && pp->ntpid==ntpid)
		{
			if(pp->maxfds)
				fdcloseall(pp);
			proc_orphan(pp->pid);
			/* Wait for process to terminate */
			n=WaitForSingleObject(hp, 2000);
			if (n == WAIT_OBJECT_0)
			{
				closehandle(hp,HT_PROC);
				return 0;
			}
			else
				logerr(0, "exiting_process didn't terminate ret=%d ntpid=%d slot=%d inuse=%d pid=%d-%d hp_pid=%d ph_pid=%d posix=0x%x state=%(state)s name=%s cmd=%s", n, ntpid, proc_slot(pp), pp->inuse, pp->ntpid, pp->pid, GetProcessId(hp), GetProcessId(pp->ph), pp->posixapp, pp->state, pp->prog_name, pp->cmd_line);

		}
		/*
		 * acquire all mutexes, before terminating process, so none
		 * become abandoned.
		 */
		s = P(0);
		ret = TerminateProcess(hp, sig);
		V(0, s);
		if(!ret && proc_isrunning(ntpid))
		{
			char *name= (pp?pp->prog_name:"native");
			int slot = (pp?proc_slot(pp):0);
			DWORD err = GetLastError();
			int ref = (pp?pp->inuse:0);
			logmsg(LOG_PROC+3, "TerminateProcess failed err=%d ntpid=%d-%d slot=%d ref=%d name=%s", err, ntpid, uwin_nt2unixpid(ntpid), slot, ref, name);
		}
		closehandle(hp,HT_PROC);
		return(0);
	}
	else
		logerr(LOG_PROC+3, "OpenProcess");
	if(pp)
		pp->state = procstate;
	if(GetLastError()==ERROR_INVALID_PARAMETER)
		return(0);
	return(-1);
}

int sendsig(register Pproc_t *pp,int sig)
{
	HANDLE hp,tok=0;
	int r=0, id = -1;
#if 0
	if(sig==SIGCONT && pp->state!=PROCSTATE_STOPPED)
		return(0);
#endif
	while(!(hp=OpenProcess(PROCESS_DUP_HANDLE|PROCESS_TERMINATE,FALSE,(DWORD)pp->ntpid)))
	{
		UMS_setuid_data_t	sdata;

		if(GetLastError()!=ERROR_ACCESS_DENIED)
		{
			errno = ESRCH;
			return(-1);
		}
		else if(id>=1 || (!tok && !is_admin()))
		{
			errno = EPERM;
			return(-1);
		}
		id++;
		memset(&sdata, 0, sizeof(sdata));
		sdata.uid = pp->uid;
		sdata.gid = UMS_NO_ID;
		if(tok)
		{
			RevertToSelf();
			closehandle(tok,HT_TOKEN);
		}
		if((tok=ums_setids(&sdata,sizeof(sdata))) && impersonate_user(tok)<0)
		{
			logerr(LOG_PROC+3, "impersonate_user");
			closehandle(tok,HT_TOKEN);
			tok = 0;
		}
	}
	if(sig==0)
	{
		closehandle(hp, HT_PROC);
		goto done;
	}
	closehandle(hp,HT_PROC);
	hp = 0;
	if(sig!=SIGKILL && SigIgnore(pp,sig))
		goto done;
	if(pp==P_CP)
		hp = P_CP->sevent;
	else if((pp->posixapp&UWIN_PROC) && pp->sevent)
	{
		char ename[UWIN_RESOURCE_MAX];
		UWIN_EVENT_S(ename, pp->ntpid);
		if(!(hp=OpenEvent(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,ename)))
		{
			logerr(0, "OpenEvent %s sevent=%04x sig=%d failed state=%(state)s name=%s", ename, pp->sevent, sig, pp->state, pp->prog_name);
			if(sig==SIGKILL)
				r = terminate_proc((DWORD)pp->ntpid, sig);
			else
			{
				errno = EPERM;
				r = -1;
			}
			goto done;
		}
	}
	sigsetgotten (pp,sig,1);
	if(sig==SIGCONT)
	{
		sigsetgotten(pp, SIGSTOP, 0);
		sigsetgotten(pp, SIGTSTP, 0);
		sigsetgotten(pp, SIGTTIN, 0);
		sigsetgotten(pp, SIGTTOU, 0);
	}
	if(sig==SIGSTOP || sig==SIGTSTP || sig==SIGTTIN || sig==SIGTTOU)
		sigsetgotten(pp, SIGCONT, 0);

	if(sig==SIGCONT || (sig==SIGINT && sigmasked(pp,sig))
		|| (sig==SIGTSTP && sigmasked(pp,sig))
		|| (!SigIgnore(pp,sig) && !sigmasked(pp,sig)))
	{
		if(pp==P_CP && P_CP->siginfo.sigsys==0 && P_CP->threadid==GetCurrentThreadId())
			processsig();
		else if(hp && !SetEvent(hp))
			logerr(0, "SetEvent");
	}
	if(hp && pp!=P_CP)
		closehandle(hp,HT_EVENT);
	if((hp || pp->siginfo.sigsys) && sig!=SIGKILL)
		goto done;
	/* give process a chance to terminate naturally */
	if(sig!=SIGCONT)
	{
		if (pp->pid == pp->pgid && pp->group && !(pp->posixapp&ORPHANING) && !pp->phandle)
		{
			HANDLE sph = pp->ph;
			dup_to_init(pp,Share->init_slot);
			pp->phandle = pp->ph;
			pp->ph = sph;
			if (!pp->phandle)
				logmsg(0, "sendsig failed to gen phandle");
		}
		r = terminate_proc((DWORD)pp->ntpid, sig);
	}
done:
	if(tok)
	{
		RevertToSelf();
		closehandle(tok,HT_TOKEN);
	}
	return(r);
}

int kill1(register pid_t pid, int sig)
{
	int count,sigstate;
	int r= -1;
	register Pproc_t *proc;
	short s,*next;

	if((unsigned)sig>=NSIG)
	{
		errno = EINVAL;
		return(r);
	}
	if (pid == 0)
		pid = -P_CP->pgid;
	if (pid < 0)
	{
		if(pid==-1) /* kill all processes except me */
		{
			int		r = 0;
			int		n = 10;
			int		i;
			unsigned short	slot;

			if (sig == SIGTERM || sig == SIGKILL)
				Share->init_state = UWIN_INIT_SHUTDOWN;
			do
			{
				for (i = 0; i < Share->nprocs; i++)
				{
					slot = Pprochead[i];
					while (slot > 0)
					{
						proc = proc_ptr(slot);
						slot = proc->next;
						if (proc != P_CP && !procexited(proc) && sendsig(proc, sig))
							r = -1;
					}
				}
			} while (r && sig == SIGKILL && --n);
			return(r);
		}
		proc = proc_findlist(-pid);
		if(!proc)
		{
			errno = ESRCH;
			return(-1);
		}

		if(proc!=P_CP && !procexited(proc))
			r = sendsig(proc,sig);
		count=Share->block;
		sigstate = P(3);
		for(next= &proc->group; (s=*next) && count-->0; next= &proc->group)
		{
			if(!is_slotvalid(s))
				break;
			proc = proc_ptr(s);
			if(proc->pgid != -pid)
				logmsg(LOG_PROC+2, "invalid process on group %d chain slot=%d(%d) gent=%d pid=%d-%d ppid=%d pgid=%d refcount=%d type=%d btype=0x%x name=%s", -pid, *next, s, proc->group, proc->ntpid, proc->pid, proc->ppid, proc->pgid, proc->inuse, Blocktype[s]&BLK_MASK, Blocktype[s], proc->prog_name);
			else if(proc!=P_CP && !procexited(proc))
			{
				r = sendsig(proc,sig);
			}
		}
		V(3,sigstate);
		/* send me the signal last */
		if(P_CP->pgid == -pid)
			r = sendsig(P_CP,sig);
		return(0);
	}
	else
	{
		if(proc = proc_getlocked(pid, 0))
		{
			proc_release(proc);
			r = sendsig(proc,sig);
		}
		else
		{
			HANDLE hp;
			DWORD ntpid = uwin_ntpid(pid);
			SetLastError(0);
			if((pid&0xff)==0xff && (hp=OpenProcess(PROCESS_TERMINATE,FALSE,ntpid)))
			{
				closehandle(hp,HT_PROC);
				if(sig==0 || terminate_proc(ntpid, sig)>=0)
					return(0);
				else
					errno = EPERM;
			}
			else if(sig==0 && GetLastError()==ERROR_ACCESS_DENIED)
				return(0);
			errno = ESRCH;
		}
	}
	return(r);
}

Pproc_t *proc_srchpid(pid_t pid)
{
	register Pproc_t *pp;
	unsigned slot =  proc_hash(pid);
	short s,*next;
	for(next= &Pprochead[slot];s=*next; next = &pp->next)
	{
		if(!is_slotvalid(s))
			break;
		pp = proc_ptr(s);
		if(pp->pid==pid)
			return(pp);
	}
	return(0);
}

int kill(register pid_t pid, int sig)
{
	int val;
	if (sig==0)
	{
		Pproc_t *p;
		int r=0;
		if (pid < 0)
			p = proc_srchpid(-pid);
		else
			p = proc_srchpid(pid);
		if (!p)
		{
			errno=ESRCH;
			r=-1;
		}
		return r;
	}
	sigdefer(1);
	val=kill1(pid, sig);
	sigcheck(0);
	return(val);
}

int killpg(register pid_t pid, int sig)
{
	return(kill(-pid,sig));
}

void abort(void)
{
	kill(P_CP->pid,SIGABRT);
	logmsg(LOG_PROC+3, "abort ExitProcess 0x%x", SIGABRT<<8);
	ExitProcess(SIGABRT<<8);
}

pid_t getpid(void)
{
	return(P_CP->pid);
}

pid_t getppid(void)
{
#if 1
	register Pproc_t *proc;
	register short *next;
	int count,sigstate;
	int found;
	unsigned short slot;
	if (P_CP->ppid > 1)
	{
		slot =  proc_hash(P_CP->ppid);
		found = 0;
		count=Share->nprocs;
		sigstate = P(3);
		for(next= &Pprochead[slot];*next && count-->0;next= &proc->next)
		{
			if(!is_slotvalid(*next))
				break;
			proc = proc_ptr(*next);
			if(proc->pid==P_CP->ppid)
			{
				if(!procexited(proc))
					found = 1;
				break;
			}
		}
		V(3,sigstate);
		if(!found)
			return(1);
	}
#endif
	return(P_CP->ppid);
}

pid_t getpgrp(void)
{
	return(P_CP->pgid);
}

pid_t getsid(pid_t pid)
{
	Pproc_t *pp;
	if(pid==0 || pid==P_CP->pid)
		return(P_CP->sid);
	else if (!(pp=proc_getlocked(pid, 0)))
	{
		errno = ESRCH;
		return(-1);
	}
	pid = pp->sid;
	proc_release(pp);
	return(pid);
}

/*
 *  Strategy is straightforward. The inner loop is to avoid
 *  repetition of supplementary/primary group ids in the output.
 *
 *  Caveat: The group_info & groups buffers should be dynamically allocated.
 */
int getgroups(int ngrp, gid_t *gp)
{
	int	i, j, group_info[1024], current, maxcnt, size;
	gid_t	groups[1024], *pgrp, gid, primary_id;

	current = 0;
	if (!P_CP->rtoken)
	{
		errno = EPERM;
		return(-1);
	}
	if(ngrp<0)
	{
		errno = EINVAL;
		return(-1);
	}
	if(ngrp>0 && IsBadReadPtr((void*)gp,ngrp*sizeof(gid_t)))
	{
		errno = EFAULT;
		return(-1);
	}
	if (!GetTokenInformation(P_CP->rtoken, TokenPrimaryGroup, group_info,
						   sizeof(group_info), &size))
	{
		logerr(LOG_PROC+3, "GetTokenInformation");
		errno = (size > sizeof(group_info)) ? EINVAL : EPERM;
		return(-1);
	}
	primary_id = sid_to_unixid(((TOKEN_PRIMARY_GROUP *)group_info)->PrimaryGroup);
	if (!GetTokenInformation(P_CP->rtoken, TokenGroups, group_info,
						   sizeof(group_info), &size))
	{
		logerr(LOG_PROC+3, "GetTokenInformation");
		errno = (size > sizeof(group_info)) ? EINVAL : EPERM;
		return(-1);
	}
	if (ngrp)
	{
		pgrp = gp;
		maxcnt = ngrp;
	}
	else
	{
		pgrp = groups;
		maxcnt = 1024;
	}
	pgrp[current++] = primary_id;
	for (i = 0; i < (int)((TOKEN_GROUPS *) group_info)->GroupCount; ++i)
	{
		if ((int)(gid = sid_to_unixid(((TOKEN_GROUPS *)group_info)->Groups[i].Sid)) <= 0 || gid == primary_id)
			continue;
		for (j = 0; j < current && pgrp[j] != gid; ++j);
		if (j == current)
		{
			if (current == maxcnt)
			{
				errno = EINVAL;
				return(-1);
			}
			pgrp[current++] = gid;
		}
	}
	return(current);
}

pid_t getpgid(pid_t pid)
{
	Pproc_t *proc;
	pid_t gid;
	if(pid==0 || pid==P_CP->pid)
		return P_CP->pgid;
	if (!(proc=proc_getlocked(pid, 0)))
	{
		errno = ESRCH;
		return(-1);
	}
	gid = proc->pgid;
	proc_release(proc);
	return(gid);
}

void proc_setcmdline(Pproc_t *proc, char *const argv[])
{
	register char*	s;
	register size_t	i;
	register size_t	j;
	register size_t	n;

	for (i = j = 0; s = *argv; argv++)
	{
		if (i < sizeof(proc->cmd_line))
		{
			n = strlen(s);
			if (n < (sizeof(proc->cmd_line)-1)-i)
			{
				memcpy(&proc->cmd_line[i], s, n);
				i += n;
				proc->cmd_line[i++] = ' ';
			}
			else
			{
				memcpy(&proc->cmd_line[i], s, (sizeof(proc->cmd_line)-1)-i);
				i = sizeof(proc->cmd_line);
				if (j >= sizeof(proc->args)-2)
					break;
			}
		}
		if (j < (sizeof(proc->args)-1))
		{
			n = strlen(s) + 1;
			if (n < (sizeof(proc->args)-2)-j)
			{
				memcpy(&proc->args[j], s, n);
				j += n;
			}
			else
			{
				memcpy(&proc->args[j], s, (sizeof(proc->args)-3)-j);
				j = sizeof(proc->args)-3;
				proc->args[j++] = 0;
				if (i >= sizeof(proc->cmd_line))
					break;
			}
		}
	}
	proc->cmd_line[i-1] = 0;
	proc->args[j++] = 0;
	proc->argsize = (unsigned char)j;
}


/*
 * allocate and initialize a process structure using parent data
 * ntpid==2 for fork()
 */
Pproc_t *proc_init(DWORD ntpid, int inexec)
{
	register Pproc_t *proc, *pproc=P_CP;
	Pfd_t *fdp;
	int pid,fd,fdi;
	HANDLE me;

	if (!(proc=proc_create(ntpid)))
		return(NULL);
	pid = proc->pid = (pid_t)ntpid;
	if(inexec)
	{
		proc->ppid = pproc->ppid;
		proc->parent = pproc->parent;
	}
	else
	{
		proc->ppid = pproc->pid;
		proc->parent = proc_slot(pproc);
	}
	proc->origph = 0;
	proc->gid = pproc->gid;
	proc->egid = pproc->egid;
	proc->pgid = pproc->pgid;
	proc->sid = pproc->sid;
	proc->priority = pproc->priority;
	proc->console = pproc->console;
	memcpy((void*)proc->cur_limits,(void*)pproc->cur_limits,2*sizeof(pproc->cur_limits));
	if(pproc->traceflags&UWIN_TRACE_INHERIT)
	{
		proc->trace = pproc->trace;
		proc->traceflags = pproc->traceflags;
	}
	if(ntpid>=3)
		me = GetCurrentProcess();
	proc->etoken = pproc->etoken;
	proc->umask = pproc->umask;
	proc->maxfds = pproc->maxfds;
	proc->procdir = pproc->procdir;
	proc->regdir = pproc->regdir;
	proc->inexec = (pproc->inexec&PROC_DAEMON);
	proc->debug = pproc->debug;
	proc->test = pproc->test;
	proc->wow = pproc->wow;
	proc->type = pproc->type & PROCTYPE_ISOLATE;
	proc->ntpid = ntpid; /* needed for proc_ununit() */
	if((proc->curdir=pproc->curdir) >=0)
	{
		int ref,ocurdir;
		if((ref=Pfdtab[ocurdir=proc->curdir].refcount)<0)
		{
			/* This should not happen, so log it */
			pproc->curdir = -1;
			setcurdir(-1, 0, 0, 0);
			if((proc->curdir=pproc->curdir) >=0)
				InterlockedIncrement(&Pfdtab[proc->curdir].refcount);
			logmsg(0,"proc_init oref=%d ocurdir=%d,'%s' curdir=%d,'%s' ref=%d ppcurdir=%d'%s' ref=%d",ref,ocurdir,((ocurdir>0&&ocurdir<Share->nfiles)?(fdname(ocurdir)):"bad_ocurdir"),proc->curdir,fdname(proc->curdir),Pfdtab[proc->curdir].refcount,pproc->curdir,((pproc->curdir>0&&pproc->curdir<Share->nfiles)?(fdname(pproc->curdir)):"bad_pproc->curdir"),Pfdtab[pproc->curdir].refcount);
		}
		else
		{
			Pfdtab[proc->curdir].type = FDTYPE_DIR;
			InterlockedIncrement(&Pfdtab[proc->curdir].refcount);
		}
	}
	if((proc->rootdir=pproc->rootdir) >=0)
		InterlockedIncrement(&Pfdtab[proc->rootdir].refcount);
	if(ntpid>=2)
	{
		size_t size = P_CP->cur_limits[RLIMIT_NOFILE]*sizeof(Pprocfd_t);
		proc->fdtab = (Pprocfd_t*)malloc(size);
		ZeroMemory(proc->fdtab,size);
	}
	for (fd = 0; fd < proc->maxfds; fd++)
	{
		if(!iscloexec(fd) || ntpid>=2)
		{
			setfdslot(proc,fd,fdslot(pproc,fd));
			if((fdi=fdslot(proc,fd))<0)
				continue;
			fdp = &Pfdtab[fdi];
			if(ntpid>=3)
			{
				BOOL inherit;
				HANDLE hp;
				inherit = !iscloexec(fd);
				if ((hp = Phandle(fd)) && !DuplicateHandle(me,hp,me,&proc->fdtab[fd].phandle,0,inherit,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				if ((hp = Xhandle(fd)) && !DuplicateHandle(me,hp,me,&proc->fdtab[fd].xhandle,0,inherit,DUPLICATE_SAME_ACCESS))
					logerr(0, "DuplicateHandle");
				if(inherit)
					Phandle(fd) = Fs_dup(fd,fdp,&Xhandle(fd),2);
				proc->fdtab[fd].cloexec = iscloexec(fd);
				incr_refcount(fdp);
			}
		}
	}
	proc->ipctab = pproc->ipctab;
	proc->siginfo.sigmask = pproc->siginfo.sigmask;
	proc->siginfo.sigign  = pproc->siginfo.sigign;
	proc->posixapp = POSIX_PARENT;	/* parent process is a posix application */
	return(proc);
}

static int handle_type(int type)
{
	switch(type)
	{
	case FDTYPE_FIFO: case FDTYPE_EPIPE: case FDTYPE_EFIFO:
		return HT_PIPE; break;
	case FDTYPE_NPIPE:
		return HT_NPIPE; break;
	case FDTYPE_CONSOLE:
		return HT_OCONSOLE; break;
	case FDTYPE_CONNECT_UNIX: case FDTYPE_CONNECT_INET:
		return HT_SOCKET; break;
	case FDTYPE_DIR:
		return HT_DIR; break;
	case FDTYPE_FILE: case FDTYPE_TFILE: case FDTYPE_XFILE:
		return HT_FILE; break;
	case FDTYPE_AUDIO: case FDTYPE_MODEM: case FDTYPE_MOUSE:
		return HT_COMPORT; break;
	default:
		return HT_UNKNOWN; break;
	}
}

/*
 * Undo operations done in proc_init()
 */
void
proc_uninit(Pproc_t *proc)
{
	int	fd;

	if(proc->curdir>=0)
		fdpclose(&Pfdtab[proc->curdir]);
	if(proc->rootdir>=0)
		fdpclose(&Pfdtab[proc->rootdir]);
	for(fd=0; fd < proc->maxfds; fd++)
		if(fdslot(proc,fd)>=0 && !iscloexec(fd))
			decr_refcount(getfdp(proc,fd));
}

/*
 * see if slot is on the group list
 */
static int proc_checklist(Pproc_t *pp, Pproc_t *proc)
{
	char *cp = "already on list";
	unsigned short s, *next, me=proc_slot(pp);
	int sigstate,count = Share->block;
	sigstate = P(3);
	next = &(proc->group);
	if(IsBadReadPtr((void*)next,sizeof(short)))
	{
		logmsg(0,"badsptr proc=%p next=%p",proc,next);
		V(3,sigstate);
		return(1);
	}
	while((s=*next) && s!=me && count-->0)
	{
		if(!is_slotvalid(s))
		{
			int nslot = proc_slot(next);
			Pproc_t *pp = proc_ptr(nslot);
			logmsg(0,"checklist me=%d slot=%d pid=%x-%d gent=%d state=%(state)s nslot=%d count=%d name=%s type=0x%x btype=0x%x pid=%x-%d,%(state)s",me,proc_slot(proc),proc->ntpid,proc->pid,proc->group,proc->state,nslot,count,proc->prog_name,Blocktype[nslot],Blocktype[nslot],pp->ntpid,pp->pid,pp->state);
			count = -1;
			break;
		}
		next = &(proc_ptr(s)->group);
	}
	V(3,sigstate);
	if(*next==0 || count<0)
		return(1);
	if(count==0)
		cp  = "recursive link";
	logmsg(LOG_PROC+2, "proc_checklist %s: pid=%x-%d pgid=%d slot=%d proc_slot=%d", cp, pp->ntpid, pp->pid, pp->pgid, me, proc_slot(proc));
	return(0);
}

/* put process on hash list */
void proc_addlist(register Pproc_t *pp)
{
	register Pproc_t *proc;
	register unsigned slot =  proc_hash(pp->pid);
	short s,*next;
	int sigstate;
retry:
	sigstate = P(3);
	/* check for duplicate process */
	for(next= &Pprochead[slot];s=*next; next = &proc->next)
	{
		if(!is_slotvalid(s))
		{
			V(3,sigstate);
			return;
		}
		proc = proc_ptr(s);
		if(proc->pid==pp->pid)
		{
			if(proc==pp)
			{
				V(3,sigstate);
				logmsg(LOG_PROC+2, "duplicate process name=%s pid=%x-%d slot=%d existing name=%s pid=%x-%d slot=%d", pp->prog_name, pp->ntpid, pp->pid, proc_slot(pp), proc->prog_name, proc->ntpid, proc->pid, *next);
				return;
			}
			if(proc->ntpid==pp->ntpid)
			{
				if(!procexited(proc))
				{
					V(3,sigstate);
					proc_release(proc);
					logmsg(LOG_PROC+2, "duplicate process deleted name=%s pid=%x-%d slot=%d existing name=%s pid=%x-%d slot=%d", pp->prog_name, pp->ntpid, pp->pid, proc_slot(pp), proc->prog_name, proc->ntpid, proc->pid, *next);
					goto retry;
				}
			}
		}
	}
	pp->next = Pprochead[slot];
	Pprochead[slot] = proc_slot(pp);
	slot =  proc_hash(pp->ntpid);
	pp->ntnext = Pprocnthead[slot];
	Pprocnthead[slot] = proc_slot(pp);
	if(pp->ppid!=1)
	{
		if(proc = proc_ptr(pp->parent))
		{
			pp->sibling = proc->child;
			proc->child = proc_slot(pp);
			InterlockedIncrement(&proc->nchild);
#ifdef DEBUG_CHILD_LIST
			logmsg(LOG_PROC+5, "proc_addlist: added slot=%d inuse=%d nchild=%d pid=%x-%d name=%s to parent %d", proc_slot(pp), proc->inuse, proc->nchild, pp->ntpid, pp->pid, pp->prog_name, proc->pid);
			dump_chain("addlist child",proc->child,&proc->child,offsetof(Pproc_t,sibling));
#endif
		}
		else
			logmsg(LOG_PROC+2, "proc_addlist: cannot find parent=%x pid=%x-%d slot=%d inuse=%d name=%s", pp->pid, pp->ntpid, pp->pid, proc_slot(pp), pp->inuse, pp->prog_name);
	}
	pp->group = 0;
	if(pp->pgid!=1 && pp->pgid!=pp->pid)
	{
		if(proc = proc_findlist(pp->pgid))
		{
			if(s=proc_checklist(pp,proc))
			{
				pp->group = proc->group;
				proc->group = proc_slot(pp);
			}
		}
	}
	V(3,sigstate);
#if 0
	slot = proc_hash(pp->pid);
	dump_chain("addlist pid",slot,&Pprochead[slot],offsetof(Pproc_t,next));
	slot = proc_hash(pp->ntpid);
	dump_chain("addlist ntpid",slot,&Pprocnthead[slot],offsetof(Pproc_t,ntnext));
#endif
}

/* delete process from hash list */
int proc_deletelist(Pproc_t *pp)
{
	register Pproc_t *proc;
	unsigned slot =  proc_hash(pp->pid);
	short s,*next = &Pprochead[slot];
	int me = proc_slot(pp);
	int count;
	HANDLE hp;
	logmsg(LOG_PROC+5,"proc_deletelist pid=%d pgid=%d slot=%d inuse=%d gent=%d ppid=%d parent=%d",pp->pid,pp->pgid,me,pp->inuse,pp->group, pp->ppid, pp->parent);
	if(pp->pgid==pp->pid && pp->group)
	{
		/*
		 * process group leader died before group
		 * mark as terminate
		 * process handle should have been duped into init
		 */
		pp->state = PROCSTATE_EXITED;
		proc= proc_ptr(pp->group);
		logmsg(LOG_PROC+5,"deletelist group leader slot=%d gent=%d posix=0x%x phand=%p ph=%p",proc_slot(pp),pp->group,pp->posixapp,pp->phandle,pp->ph);
		if(pp->phandle)
			goto child;
		if(pp->posixapp & (UWIN_PROC|POSIX_PARENT))
			goto child;
		logmsg(LOG_PROC+5,"grp_ldr_native pid=%d pgid=%d slot=%d inuse=%d state=%(state)s phand=%04x gent=%d ppid=%d parent=%d name=%s cmd=%s",pp->pid,pp->pgid,me,pp->inuse,pp->state,pp->phandle,pp->group,pp->ppid,pp->parent,pp->prog_name,pp->cmd_line);
		/* must be a native process, not much that we can do */
		if(hp=OpenProcess(SYNCHRONIZE,FALSE,proc->ntpid))
		{
			DWORD n;
			if((n=WaitForSingleObject(hp,500))!=0)
			{
				if(n==WAIT_TIMEOUT)
				{
					logmsg(0, "WaitForSingleObject 500ms timeout grp_entry slot=%d pid=%x-%d inuse=%d ppid=%d parent=%d pgid=%d gent=%d posix=0x%x state=%(state)s name=%s cmd=%s",proc_slot(proc),proc->ntpid,proc->pid,proc->inuse,proc->ppid,proc->parent,proc->pgid,proc->group,proc->posixapp,proc->state,proc->prog_name,proc->cmd_line);
					logmsg(0, "WaitForSingleObject 500ms timeout grp_ldr slot=%d ph=%04x phand=%04x pid=%x-%d pgid=%d inuse=%d ppid=%d parent=%d gent=%d posix=0x%x state=%(state)s name=%s cmd=%s",proc_slot(pp),pp->ph,pp->phandle,pp->ntpid,pp->pid,pp->pgid,pp->inuse,pp->ppid,pp->parent,pp->group,pp->posixapp,pp->state,pp->prog_name,pp->cmd_line);
				}
				else
				{
					logerr(0, "WaitForSingleObject grp_entry slot=%d pid=%x-%d inuse=%d ppid=%d parent=%d gent=%d posix=0x%x state=%(state)s name=%s cmd=%s",proc_slot(proc),proc->ntpid,proc->pid,proc->inuse,proc->ppid,proc->parent,proc->group,proc->posixapp,proc->state,proc->prog_name,proc->cmd_line);
					logmsg(0, "WaitForSingleObject grp_ldr slot=%d ph=%04x phand=%04x pid=%x-%d inuse=%d ppid=%d parent=%d gent=%d posix=0x%x state=%(state)s name=%s cmd=%s",proc_slot(pp),pp->ph,pp->phandle,pp->ntpid,pp->pid,pp->inuse,pp->ppid,pp->parent,pp->group,pp->posixapp,pp->state,pp->prog_name,pp->cmd_line);
				}
			}
			closehandle(hp,HT_PROC);
		}
		next = &Pprochead[slot];
	}
	count=Share->nprocs;
	while((s= *next) && s!=me && count-->0)
	{
		if(!is_slotvalid(s))
			break;
		next = &(proc_ptr(s)->next);
	}
	if(s==me)
	{
		*next = pp->next;
		pp->next = 0;
	}
	else
		logmsg(0, "deletelist failed pid=%x-%d name=%s slot=%d inuse=%d next=%p nxt_slot=%d s=%d count=%d", pp->ntpid, pp->pid, pp->prog_name, me, pp->inuse, next, proc_slot(next), s, count);
	count=Share->nprocs;
	slot = proc_hash(pp->ntpid);
	next = &Pprocnthead[slot];
	while((s=*next) && s!=me && count-->0)
	{
		if(!is_slotvalid(s))
			break;
		next = &(proc_ptr(s)->ntnext);
	}
	if(s==me)
	{
		*next = pp->ntnext;
		pp->ntnext = 0;
	}
	else
		logmsg(0, "delete_ntlist pid=%x-%d name=%s slot=%d inuse=%d next=%p nxt_slot=%d s=%d count=%d", pp->ntpid, pp->pid, pp->prog_name, me, pp->inuse, next, proc_slot(next), s, count);
child:
	if(me && pp->ppid!=1 && !(pp->posixapp&NOT_CHILD))
	{
		/* delete from the child list */
		if(proc = proc_ptr(pp->parent))
		{
			next = &(proc->child);
			count = Share->nprocs;
			while((s= *next) && s!=me && count-->0)
			{
				if(!is_slotvalid(s))
					break;
				next = &(proc_ptr(s)->sibling);
			}
			if(s==me)
			{
				*next = pp->sibling;
				pp->sibling = 0;
				InterlockedDecrement(&proc->nchild);
				pp->posixapp |= NOT_CHILD;
#ifdef DEBUG_CHILD_LIST
				if (proc->nchild > 1)
					dump_chain("deletelist child",proc->child,&proc->child,offsetof(Pproc_t,sibling));
				logmsg(LOG_PROC+5, "deletelist remove slot=%d inuse=%d nchild=%d child %d from parent %d", proc_slot(pp), proc->inuse, proc->nchild, pp->pid, proc->pid);
#endif
			}
			else
			{
				logmsg(0, "deletechild list pid=%x-%d parent=%d sibling=%d name=%s slot=%d inuse=%d next=%p nxt_slot=%d s=%d count=%d", pp->ntpid, pp->pid, pp->parent, pp->sibling, pp->prog_name, me, pp->inuse, next, proc_slot(next), s, count);
				logmsg(0, "deletechild_parent inuse=%d pid=%x=%d nchild=%d child=%d state=%(state)s name=%s cmd=%s", proc->inuse, proc->ntpid, proc->pid, proc->nchild, proc->child, proc->state, proc->prog_name, proc->cmd_line);
				dump_chain("parent_list",proc->child,&proc->child,offsetof(Pproc_t,sibling));
			}
		}
	}
	if(pp->phandle && pp->pgid==pp->pid && pp->group)
		return(0);
	proc = 0;
	if(me && pp->pgid!=1 && pp->pgid!=pp->pid)
	{
		/* delete from the process group list */
		if(proc = proc_findlist(pp->pgid))
		{
			count = Share->block;
			next = &proc->group;
			while((s=*next) && s!=me && count-->0)
			{
				if(!is_slotvalid(s))
					break;
				next = &(proc_ptr(s)->group);
			}
			if(s==me)
			{
				*next = pp->group;
				pp->group = 0;
				pp->pgid = 1;
			}
			else
			{
				logmsg(0, "deletepgid pid=%x-%d slot=%d pgid=%d inexec=0x%x posix=0x%x name=%s gldr=%d gldr_gent=%d gldr_hand=%04x next=%p s=%d count=%d", pp->ntpid, pp->pid, proc_slot(pp), pp->pgid, pp->inexec, pp->posixapp, pp->prog_name, proc_slot(proc), proc->group, proc->phandle,next,s,count);
			}
			if(proc->group==0  && proc->phandle && !(proc->posixapp&IN_RELEASE))
			{
				HANDLE oph=proc->phandle;
				logmsg(LOG_PROC+3,"groupdone slot=%d pid=%x-%d phandle=%p phand=%p",proc_slot(proc),proc->ntpid,proc->pid,proc->phandle,proc->ph);
				if(P_CP->pid==1)
				{
					closehandle(proc->phandle,HT_PROC);
					clr_procbit((unsigned long)proc->phandle);
				}
				else
					if (dup_from_init(proc)==0)
						logmsg(0, "dup_from_init failed");
				if(procexited(proc) && !(proc->posixapp&ON_WAITLST) && (P_CP->pid==1 || proc->ppid!=P_CP->pid) || (proc->posixapp&NOT_CHILD))
					proc_release(proc);
			}
			proc = 0;

		}
		else
		{
		int pblk = Blocktype[pp->parent];
		Pproc_t *procp;
			logmsg(0, "group_leader not found slot=%d inuse=%d pid=%x-%d pgrp=%d gent=%d posix=0x%x ppid=%d ppslot=%d pp_blk=0x%x state=%(state)s name=%s cmd=%s", proc_slot(pp),pp->inuse,pp->ntpid,pp->pid,pp->pgid,pp->group,pp->posixapp,pp->ppid,pp->parent,pblk,pp->state,pp->prog_name,pp->cmd_line);

			if (is_slotvalid(pp->parent))
			{
				procp = proc_ptr(pp->parent);
				logmsg(0, "grp_ent_parent slot=%d inuse=%d pid=%x-%d pgrp=%d gent=%d nchild=%d child=%d posix=0x%x ppid=%d ppslot=%d state=%(state)s name=%s cmd=%s", proc_slot(procp),procp->inuse,procp->ntpid,procp->pid,procp->pgid,procp->group,procp->nchild,procp->child,procp->posixapp,procp->ppid,procp->parent,procp->state,procp->prog_name,procp->cmd_line);
			}
			if (procp=proc_getlocked(pp->pgid, 0))
			{
				logmsg(0, "grp_ent_ldr slot=%d inuse=%d pid=%x-%d pgrp=%d gent=%d nchild=%d child=%d posix=0x%x ppid=%d ppslot=%d state=%(state)s name=%s cmd=%s", proc_slot(procp),procp->inuse,procp->ntpid,procp->pid,procp->pgid,procp->group,procp->nchild,procp->child,procp->posixapp,procp->ppid,procp->parent,procp->state,procp->prog_name,procp->cmd_line);
				proc_release(procp);
			}
		}
	}
	if(proc)
		proc_release(proc);
#if 0
	slot = proc_hash(pp->pid);
	dump_chain("dellist pid",slot,&Pprochead[slot],offsetof(Pproc_t,next));
	slot = proc_hash(pp->ntpid);
	dump_chain("dellist ntpid",slot,&Pprocnthead[slot],offsetof(Pproc_t,ntnext));
#endif
	if(!me)
		logmsg(0, "proc_deletelist !me");
	return(me?1:0);
}

Pproc_t *proc_findlist(pid_t pid)
{
	register Pproc_t *pp;
	unsigned slot =  proc_hash(pid);
	short s,*next;
	for(next= &Pprochead[slot];s=*next; next = &pp->next)
	{
		if(!is_slotvalid(s))
			break;
		pp = proc_ptr(s);
		if(pp->pid==pid)
			return(pp->pgid==pid?pp:0);
	}
	return(0);
}

int setpgid(pid_t pid, pid_t pgid)
{
	Pproc_t *proc, *proc1=NULL, *proc2;
	short s,*next;
	int sigstate, me, count;
	logmsg(LOG_PROC+2,"setpgid: pid=%d newpgid=%d oldpgid=%d",pid,pgid,P_CP->pgid);
	if(pgid<0)
	{
		errno = EINVAL;
		return(-1);
	}
	if (pid==0 || pid==P_CP->pid)
		proc = P_CP;
	else
	{
		sigstate = P(3);
		for(next= &P_CP->child; s= *next; next= &proc->sibling)
		{
			if(!is_slotvalid(s))
				break;
			proc = proc_ptr(s);
			if(proc->pid==pid)
				break;
		}
		V(3,sigstate);
		if(*next==0)
		{
			errno = ESRCH;
			return(-1);
		}
		if(proc->pid!=uwin_nt2unixpid(proc->ntpid))
		{
			errno = EACCES;
			return(-1);
		}
		if(P_CP->sid!=proc->sid)
		{
			errno = EPERM;
			return(-1);
		}
	}
	if(pgid==0)
		pgid = proc->pid;
	if(proc->pgid==pgid)
		return(0);
	if(proc->pid==proc->sid)
	{
		errno = EPERM;
		return(-1);
	}
	if(pgid!=proc->pid)
	{
		if(!(proc1=proc_findlist(pgid)) || proc1->sid!=P_CP->sid)
		{
			errno = EPERM;
			return(-1);
		}
	}
	sigstate = P(3);
	if(proc->pgid==proc->pid)
	{
		if(proc->group)
			logmsg(0, "setpgid for leader with non-empty group slot=%d pid=%d ngrp=%d name=%s", proc_slot(proc), proc->pid, pgid, proc->prog_name);
	}
	else
	{
		if(!(proc2 = proc_findlist(proc->pgid)))
		{
			errno = ESRCH;
			V(3,sigstate);
			return(-1);
		}
		me = proc_slot(proc);
		next = &(proc2->group);
		count = Share->block;
		while((s=*next) && s!=me && count-->0)
		{
			if(!is_slotvalid(s))
				break;
			if(proc_ptr(s)->pgid!=proc2->pgid)
			{
				Pproc_t *pp = proc_ptr(s);
				logmsg(0, "on wrong group list pid=%x-%d grp=%d grp should=%d name=%s", pp->ntpid, pp->pid, pp->pgid, proc2->pgid, pp->prog_name);
			}
			next = &(proc_ptr(s)->group);
		}
		if(next && s==me)
			*next = proc_ptr(s)->group;
		else
		{
			Pproc_t *gldr;
			gldr = proc_getlocked(proc->pgid, 0);
			logmsg(0, "not_in_grp_list slot=%d pid=%d pgid=%d next=%p s=%d count=%d", proc_slot(proc), proc->pid, proc->pgid, next, proc_slot(next), s, count);
			logmsg(0, "proc2 slot=%d pid=%d gent=%d name=%s", proc_slot(proc2), proc2->pid, proc2->group, proc2->prog_name);
			dump_chain("setpgid_p2",proc2->group,&proc2->group,offsetof(Pproc_t,group));
			if (gldr)
			{
				logmsg(0, "gldr slot=%d pid=%d gent=%d name=%s", proc_slot(gldr), gldr->pid, gldr->group, gldr->prog_name);
				dump_chain("setpgid_gl",gldr->group,&gldr->group,offsetof(Pproc_t,group));
				proc_release(gldr);
			}
		}
	}
	if(proc1)
	{
		Pproc_t *gldr;
		if(proc1->pgid!=pgid)
		{
			logmsg(0, "wrong group slot=%d pid=%x-%d ngrp=%d proc1_slot=%d proc1_pgid=%d name=%s", proc_slot(proc), proc->ntpid, proc->pid, pgid, proc_slot(proc1), proc1->pgid, proc->prog_name);
			gldr = proc_getlocked(proc->pgid, 0);
			if (gldr)
			{
				dump_chain("setpgid_proc_grp",gldr->group,&gldr->group,offsetof(Pproc_t,group));
				proc_release(gldr);
			}
			gldr = proc_getlocked(proc1->pgid, 0);
			if (gldr)
			{
				dump_chain("setpgid_proc1_grp",gldr->group,&gldr->group,offsetof(Pproc_t,group));
				proc_release(gldr);
			}
		}
		proc->group = proc1->group;
		proc1->group = proc_slot(proc);
	}
	else
		proc->group = 0;
	proc->pgid = pgid;
	V(3,sigstate);
	return(0);
}

pid_t setsid(void)
{
	int i;
	unsigned short slot;
	Pproc_t	*proc;
	/* Make sure we are not a process group leader */
	if (P_CP->pid == P_CP->pgid)
	{
		errno = EPERM;
		return(-1);
	}
	/* Make sure there is no process with pgrp == mypid */
	for(i= 0; i < Share->nprocs; i++)
	{
		for(slot=Pprochead[i]; slot>0; slot=proc->next)
		{
			if((proc=proc_ptr(slot))->pgid == P_CP->pid)
			{
				errno = EPERM;
				return(-1);
			}
		}
	}
	setpgid(0,0);
	P_CP->sid = P_CP->pgid = P_CP->pid;
	if(P_CP->console && dev_ptr(P_CP->console)->major==CONSOLE_MAJOR)
		FreeConsole();
	P_CP->console = 0;
	return(P_CP->pgid);
}

int setpgrp(void)
{
	return(setsid()<0?-1:0);
}

static int pagesize(void)
{
	HANDLE hp1,hp2;
	char *addr1,*addr2;
	hp1 = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READONLY,0,0x2000,NULL);
	if(!hp1 || hp1==INVALID_HANDLE_VALUE)
		return PAGESIZE;
	if(addr1 = (char*)MapViewOfFileEx(hp1,FILE_MAP_READ, 0,0,0, NULL))
		UnmapViewOfFile((char*)addr1);
	else
		return PAGESIZE;
	closehandle(hp1, HT_MAPPING);
	hp1 = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READONLY,0,0x100,NULL);
	if(!hp1 || hp1==INVALID_HANDLE_VALUE)
		return PAGESIZE;
	hp2= CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READONLY,0,0x100,NULL);
	if(!hp2 || hp2==INVALID_HANDLE_VALUE)
		return PAGESIZE;
	addr1 = (char*)MapViewOfFileEx(hp1,FILE_MAP_READ, 0,0,0, NULL);
	addr2 = (char*)MapViewOfFileEx(hp2,FILE_MAP_READ, 0,0,0, NULL);
	closehandle(hp1,HT_MAPPING);
	closehandle(hp2,HT_MAPPING);
	UnmapViewOfFile((char*)addr1);
	UnmapViewOfFile((char*)addr2);
	if(addr2>addr1 && (addr2-addr1) < PAGESIZE)
		return (int)(addr2-addr1);
	if(addr1>addr2 && (addr1-addr2) < PAGESIZE)
		return (int)(addr1-addr2);
	return PAGESIZE;
}

/*
 * returns the allocation granularity (not the page size) so that mmap() works
 */
int getpagesize(void)
{
#if 0
	static int psize;
	if(!psize)
		psize = pagesize();
	return(psize);
#else
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return(info.dwAllocationGranularity);
#endif
}

#ifndef PROCESSOR_ARCHITECTURE_INTEL
#define PROCESSOR_ARCHITECTURE_INTEL		0
#endif
#ifndef PROCESSOR_ARCHITECTURE_MIPS
#define PROCESSOR_ARCHITECTURE_MIPS		1
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA
#define PROCESSOR_ARCHITECTURE_ALPHA		2
#endif
#ifndef PROCESSOR_ARCHITECTURE_PPC
#define PROCESSOR_ARCHITECTURE_PPC		3
#endif
#ifndef PROCESSOR_ARCHITECTURE_SHX
#define PROCESSOR_ARCHITECTURE_SHX		4
#endif
#ifndef PROCESSOR_ARCHITECTURE_ARM
#define PROCESSOR_ARCHITECTURE_ARM		5
#endif
#ifndef PROCESSOR_ARCHITECTURE_IA64
#define PROCESSOR_ARCHITECTURE_IA64		6
#endif
#ifndef PROCESSOR_ARCHITECTURE_ALPHA64
#define PROCESSOR_ARCHITECTURE_ALPHA64		7
#endif
#ifndef PROCESSOR_ARCHITECTURE_MSIL
#define PROCESSOR_ARCHITECTURE_MSIL		8
#endif
#ifndef PROCESSOR_ARCHITECTURE_AMD64
#define PROCESSOR_ARCHITECTURE_AMD64		9
#endif
#ifndef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64	10
#endif

int uname(register struct utsname* ut)
{
	OSVERSIONINFO	osinfo;
	SYSTEM_INFO	sysinfo;
	DWORD		n;
	char*		name;
	char*		cp;
	char*		ep;
	char*		up;
	char		buf[32];
	char		sys[8];

	osinfo.dwOSVersionInfoSize = sizeof(osinfo);
	if(IsBadReadPtr((void*)ut,sizeof(struct utsname)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!GetVersionEx(&osinfo))
	{
		logerr(0, "GetVersionEx");
		return(-1);
	}
	name = 0;
	switch(osinfo.dwPlatformId)
	{
	case VER_PLATFORM_WIN32s:
		name = "32s";
		break;
	case 1: /* VER_PLATFORM_WIN32_WINDOWS: */
		if(osinfo.dwMinorVersion>=90)
			name = "ME";
		else if(osinfo.dwMinorVersion>=10)
			name = "98";
		else
			name = "95";
		break;
	case VER_PLATFORM_WIN32_NT:
		switch (osinfo.dwMajorVersion)
		{
		case 4:
			name = "2K";
			break;
		case 5:
			switch (osinfo.dwMinorVersion)
			{
			case 0:
				name = "NT";
				break;
			case 2:
				name = "2K3";
				break;
			default:
				name = "XP";
				break;
			}
			break;
		case 6:
			switch (osinfo.dwMinorVersion)
			{
			case 0:
				name = "VI";
				break;
			case 1:
			case 2:
			case 3:
				name = sys;
				name[0] = 'W';
				name[1] = (char)('6' + osinfo.dwMinorVersion);
				name[2] = 0;
				break;
			default:
				name = sys;
				name[0] = 'W';
				name[1] = '1';
				name[2] = (char)('1' + osinfo.dwMinorVersion - 4);
				name[3] = 0;
				break;
			}
			break;
		default:
			if (osinfo.dwMajorVersion >= 7)
				sfsprintf(name = sys, sizeof(sys), "X%d.%d", osinfo.dwMajorVersion + 2, osinfo.dwMinorVersion);
			break;
		}
		break;
	}
	sfsprintf(ut->sysname, sizeof(ut->sysname), SYSTEM_STRING "%s%s", name ? "-" : "", name);
	if(gethostname(ut->nodename, sizeof(ut->nodename)-1) <0)
		strncpy(ut->nodename, "local", sizeof(ut->nodename)-1);
	else if(cp = strchr(ut->nodename,'.'))
		*cp = 0;
	up = ut->release;
	for (n = 0; n < 6 && state.rel[n]; n++)
		*up++ = state.rel[n];
	if (up == ut->release)
	{
		ep = (char*)IDENT_TEXT(version_id);
		if (cp = strchr(ep, ')'))
			cp++;
		else
			cp = ep;
		while (*cp == ' ')
			cp++;
		while (up < &ut->release[sizeof(ut->release)-7] && *cp != ' ')
			*up++ = *cp++;
	}
	if ((n = (DWORD)sfsprintf(up, &ut->release[sizeof(ut->release)-1] - up, "/%d.%d", osinfo.dwMajorVersion, osinfo.dwMinorVersion)) > 0)
		up += n;
	*up = 0;

	cp = ut->version;
#ifdef DATE_STRING
	strncpy(ut->version, DATE_STRING, sizeof(ut->version)-2);
	ut->version[sizeof(ut->version)-1] = 0;
#else
	n=osinfo.dwBuildNumber&0xffff;
	while(n>=10)
	{
		*cp++ = '0' + (unsigned char)(n%10);
		n /= 10;
	}
	*cp++ = '0' + (unsigned char)n;
	*cp = 0;
	_strrev(ut->version);
#endif
	GetSystemInfo(&sysinfo);
	switch ((Share->Platform == VER_PLATFORM_WIN32_NT || osinfo.dwMinorVersion >= 10) ? sysinfo.wProcessorArchitecture : -1)
	{
	case PROCESSOR_ARCHITECTURE_INTEL:
	case PROCESSOR_ARCHITECTURE_AMD64:
		switch(sysinfo.dwProcessorType)
		{
		case PROCESSOR_INTEL_386:
			sysinfo.wProcessorLevel = 3;
			break;
		case PROCESSOR_INTEL_486:
			sysinfo.wProcessorLevel = 4;
			break;
		}
#if _X64_
		sfsprintf(name = buf, sizeof(buf), "i%d86%s", sysinfo.wProcessorLevel, P_CP && (P_CP->wow & WOW_ALL_32) ? "" : "-64");
#else
		sfsprintf(name = buf, sizeof(buf), "i%d86%s", sysinfo.wProcessorLevel, P_CP && (P_CP->wow & WOW_ALL_64) ? "-64" : "");
#endif
		break;
	case PROCESSOR_ARCHITECTURE_MIPS:
		name = "mips";
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
		name = "alpha";
		break;
	case PROCESSOR_ARCHITECTURE_PPC:
		name = "ppc";
		break;
	case PROCESSOR_ARCHITECTURE_SHX:
		name = "shx";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		name = "arm";
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		name = "ia64";
		break;
	case PROCESSOR_ARCHITECTURE_ALPHA64:
		name = "alpha64";
		break;
	case PROCESSOR_ARCHITECTURE_MSIL:
		name = "msil";
		break;
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
		name = "ia32";
		break;
	default:
#if _X64_
		name = P_CP && (P_CP->wow & WOW_ALL_32) ? "ix86" : "ix86-64";
#elif _X86_
		name = P_CP && (P_CP->wow & WOW_ALL_64) ? "ix86-64" : "ix86";
#else
		name = "unknown";
#endif
		break;
	}
	strncpy(ut->machine, name, sizeof(ut->machine)-1);
	ut->machine[sizeof(ut->machine)-1] = 0;
	return(0);
}

struct Priority
{
	int	process;
	int	thread;
} Prtable[42] =
{
/*-20*/	REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	REALTIME_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,

/*-10*/	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	HIGH_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,

/* 0 */	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,

	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	NORMAL_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_HIGHEST,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
/* 10 */IDLE_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,

	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_ABOVE_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_BELOW_NORMAL,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
	IDLE_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST,
/* 20 */IDLE_PRIORITY_CLASS, THREAD_PRIORITY_LOWEST
};

static int proc_setpriority(register Pproc_t *pp, int priority, int r)
{
	HANDLE hp=GetCurrentProcess();
	if(InterlockedIncrement(&pp->inuse))
	{
		if(priority<0)
		{
			if(pp->priority<r)
				r = pp->priority;
		}
		else if(pp==P_CP || (hp=OpenProcess(PROCESS_SET_INFORMATION,FALSE,pp->ntpid)))
		{
			r = pp->priority;
			if(!SetPriorityClass(hp,Prtable[priority].process))
				logerr(0, "SetPriorityClass");
			if(!SetThreadPriority(pp->thread,Prtable[priority].thread))
				logerr(0, "SetThreadPriority tid=%p", pp->thread);
			else
				r = pp->priority = priority-20;
			if(pp!=P_CP)
				closehandle(hp,HT_PROC);
		}
		else
			logerr(0, "OpenProcess");
		proc_release(pp);
	}
	return(r);
}

static int getandsetpriority(int which, id_t who, register int priority)
{
	register Pproc_t *pp;
	register int i,count;
	register int r=(priority<0?20:priority),slot;
	short *next;
	static int beenhere=0;
	if(!beenhere)
	{
		HANDLE hp=GetCurrentProcess();
		int pri = GetPriorityClass(hp);
		if(SetPriorityClass(hp,ABOVE_NORMAL_PRIORITY_CLASS))
		{
			SetPriorityClass(hp,pri);
			for(i=0; i < 5; i++)
			{
				Prtable[10+i].process = ABOVE_NORMAL_PRIORITY_CLASS;
				Prtable[26+i].process = BELOW_NORMAL_PRIORITY_CLASS;
			}
			Prtable[6].thread = THREAD_PRIORITY_ABOVE_NORMAL;
			Prtable[7].thread = THREAD_PRIORITY_NORMAL;
			Prtable[8].thread = THREAD_PRIORITY_BELOW_NORMAL;
			Prtable[9].thread = THREAD_PRIORITY_LOWEST;

			Prtable[10].thread = THREAD_PRIORITY_HIGHEST;
			Prtable[11].thread = THREAD_PRIORITY_ABOVE_NORMAL;
			Prtable[12].thread = THREAD_PRIORITY_NORMAL;
			Prtable[13].thread = THREAD_PRIORITY_BELOW_NORMAL;
			Prtable[14].thread = THREAD_PRIORITY_LOWEST;

			Prtable[27].thread = THREAD_PRIORITY_ABOVE_NORMAL;
			Prtable[28].thread = THREAD_PRIORITY_NORMAL;
			Prtable[29].thread = THREAD_PRIORITY_BELOW_NORMAL;
			Prtable[30].thread = THREAD_PRIORITY_LOWEST;

			Prtable[31].thread = THREAD_PRIORITY_HIGHEST;
			Prtable[32].thread = THREAD_PRIORITY_HIGHEST;
			Prtable[33].thread = THREAD_PRIORITY_ABOVE_NORMAL;
			Prtable[34].thread = THREAD_PRIORITY_ABOVE_NORMAL;
			Prtable[35].thread = THREAD_PRIORITY_NORMAL;
			Prtable[36].thread = THREAD_PRIORITY_NORMAL;
			Prtable[37].thread = THREAD_PRIORITY_BELOW_NORMAL;
			Prtable[38].thread = THREAD_PRIORITY_BELOW_NORMAL;
			Prtable[39].thread = THREAD_PRIORITY_LOWEST;
			Prtable[40].thread = THREAD_PRIORITY_LOWEST;
		}
		beenhere = 1;
	}
	switch(which)
	{
	    case PRIO_PROCESS:
		if(who==0)
			pp = P_CP;
		else if(!(pp = proc_getlocked((pid_t)who, 0)))
		{
			errno = ESRCH;
			return(-1);
		}
		r = proc_setpriority(pp,priority,r);
		if(who)
			proc_release(pp);
		break;
	    case PRIO_USER:
		if(who==0)
			who = (id_t)getuid();
		/* linear scan of process table */
		for(i= 0; i < Share->nprocs; i++)
		{
			for(slot=Pprochead[i]; slot>0; slot=pp->next)
			{
				pp = proc_ptr(slot);
				if(procexited(pp))
					continue;
				if(pp->uid != (uid_t)who)
					continue;
				r= proc_setpriority(pp,priority,r);
			}
		}
		break;
	    case PRIO_PGRP:
		if(who==0)
			who = (id_t)P_CP->pgid;
		count=Share->block;
		if (!(pp = proc_findlist(who)))
		{
			logmsg(0, "unknown process group id %d", who);
			errno = ESRCH;
			break;
		}
		r = proc_setpriority(pp,priority,r);
		for(next= &pp->group; *next && count-->0; next= &pp->group)
		{
			pp = proc_ptr(*next);
			if(!procexited(pp))
				continue;
			if(pp->gid != (uid_t)who)
			{
				logmsg(0, "getandsetpriority bad gid=%d", pp->gid);
				continue;
			}
			r= proc_setpriority(pp,priority,r);
		}
		break;
	    default:
		errno = EINVAL;
		return(-1);
	}
	if(r>20)
	{
		errno = ESRCH;
		return(-1);
	}
	return(r);
}

int setpriority(int which, id_t who, register int priority)
{
	if(priority<0 && !is_admin())
	{
		errno = EACCES;
		return(-1);
	}
	if(priority < -20)
		priority = -20;
	else if(priority > 19)
		priority = 19;
	priority += 20;
	if(getandsetpriority(which,who,priority)>=0)
		return(0);
	return(-1);
}

int getpriority(int which, id_t who)
{
	return(getandsetpriority(which,who,-1));
}

int nice(int incr)
{
	int r, err=errno;
	if(incr<0 && !is_admin())
	{
		errno = EPERM;
		return(-1);
	}
	errno = 0;
	r = setpriority(PRIO_PROCESS,0,P_CP->priority+incr);
	if(errno)
		return(-1);
	errno = err;
	return(0);
}

extern int getdtablesize(void)
{
	return(P_CP->cur_limits[RLIMIT_NOFILE]);
}

int getrlimit(int resource, struct rlimit *rlp)
{
	if(resource<0 || resource>=RLIM_NLIMITS)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadWritePtr((void*)rlp,sizeof(struct rlimit)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(resource < RLIM_NLIMITS)
	{
		rlp->rlim_max = P_CP->max_limits[resource];
		rlp->rlim_cur = P_CP->cur_limits[resource];
	}
	else
	{
		errno = ENOSYS;
		return(-1);
	}
	return(0);
}

int getrlimit64(int resource, struct rlimit64 *rlp)
{
	struct rlimit lim;
	if(IsBadWritePtr((void*)rlp,sizeof(struct rlimit64)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(getrlimit(resource,&lim)<0)
		return(-1);
	rlp->rlim_max = lim.rlim_max;
	rlp->rlim_cur = lim.rlim_cur;
	if(resource==RLIMIT_FSIZE)
	{
		rlp->rlim_max |= ((rlim64_t)P_CP->max_limits[RLIM_NLIMITS])<<32;
		rlp->rlim_cur |= ((rlim64_t)P_CP->cur_limits[RLIM_NLIMITS])<<32;
	}
	return(0);
}

int setrlimit(int resource, const struct rlimit *rlp)
{
	if(resource<0 || resource>=RLIM_NLIMITS)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadReadPtr((void*)rlp,sizeof(struct rlimit)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!is_admin() && (rlp->rlim_max>P_CP->max_limits[resource] || rlp->rlim_cur>P_CP->cur_limits[resource]))
	{
		errno = EPERM;
		return(-1);
	}
	P_CP->max_limits[resource] = rlp->rlim_max;
	P_CP->cur_limits[resource] = rlp->rlim_cur;
	return(0);
}

int setrlimit64(int resource, const struct rlimit64 *rlp)
{
	if(resource<0 || resource>=RLIM_NLIMITS)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadReadPtr((void*)rlp,sizeof(struct rlimit64)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!is_admin())
	{
		struct rlimit64 my;
		getrlimit64(resource,&my);
		if(rlp->rlim_max>my.rlim_max || rlp->rlim_cur>my.rlim_cur)
		{
			errno = EPERM;
			return(-1);
		}
	}
	P_CP->max_limits[resource] = (unsigned long)rlp->rlim_max;
	P_CP->cur_limits[resource] = (unsigned long)rlp->rlim_cur;
	if(resource==RLIMIT_FSIZE)
	{
		P_CP->max_limits[RLIM_NLIMITS] = (unsigned long)(rlp->rlim_max>>32);
		P_CP->cur_limits[RLIM_NLIMITS] = (unsigned long)(rlp->rlim_cur>>32);
	}
	return(0);
}

int vhangup(void)
{
	Pdev_t *pdev;

	if(!is_admin())
	{
		errno = EPERM;
		return(-1);
	}
	if(P_CP->console && (pdev=dev_ptr(P_CP->console)))
		kill(-pdev->tgrp,SIGHUP);
	return(0);
}

DWORD proc_ntparent(Pproc_t *pp)
{
	Pproc_t	*proc;
	DWORD	ntpid=Share->init_ntpid;
	if(pp->pid!=1)
	{
		int sigstate = P(3);
		if(proc=proc_getlocked(pp->ppid, 0))
		{
			ntpid = proc->ntpid;
			proc_release(proc);
		}
		else
			logmsg(0,"parent_proc missing slot=%d pid=%x-%d ppid=%d ppslot=%d state=%(state)s inuse=%d",proc_slot(pp),pp->ntpid,pp->pid,pp->ppid,pp->parent,pp->state,pp->inuse);
		V(3,sigstate);
	}
	return(ntpid);
}

static int regen_freelist(int oldtop)
{
	register Blockhead_t *blk,*oldblk=0;
	int i,n=0,top;
	int sigstate = P(1);
	if(!(top=Share->top)|| top>Share->block)
	{
		logmsg(0, "resources corrupted or unavailable freelist top=%x:%d nblocks=%d ops=%d", oldtop, oldtop, Share->nblocks, Share->nblockops);
#ifdef BLOCK_TRACE
		dumpit(255);
		//dumpit(50);
#endif
		get_uwin_slots(0, 0, state.log);
		//for(i=1; i < Share->block; i++ )
		for(i=1; i < Share->nfiles; i++ )
		{
			blk = (Blockhead_t*)block_ptr(i);
			if((Blocktype[i]&BLK_MASK) != BLK_FREE)
				continue;
			n++;
			if(oldblk)
				oldblk->next = i;
			else
				top = i;
			oldblk = blk;
		}
		Share->nblocks = n;
		logmsg(0, "freelist regenerated nblocks=%d", n);
	}
	Share->top = top;
	V(1,sigstate);
	return(top);
}

#ifdef BLK_CHECK
unsigned short Xblock_alloc(int type, const char *file, int line)
#else
unsigned short block_alloc(int type)
#endif
{
	register long old,next;
	register void *ptr;
#ifdef USE_SEMAPHORE
	int sigstate;
	if(USE_SEMAPHORE)
		 sigstate = P(1);
#endif
	while(1)
	{
		if(!(old= Share->top) || old > Share->block)
		{
			if(!(old=regen_freelist(old)))
			{
				logmsg(0, "out of resources" FMT_ATBLK " last=%d:%d" BLK_ARGS,Share->lastfile,Share->lastline);
				Sleep(1000);
				continue;
			}
		}
		ptr = block_ptr(old);
		next = ((Blockhead_t*)ptr)->next;
		if(!_comp(old,Share->top))
			continue;
		if(next==0 || next > Share->block)
		{
			int otype = Blocktype[old]>>4;
			logmsg(0, "block_alloc error" FMT_ATBLK " old=%d type=%d otype=%d btype=0x%x next=%x:%d otop=%d nblocks=%d" BLK_ARGS, old, Blocktype[old]&BLK_MASK, otype&BLK_MASK, Blocktype[old], next, next, Share->otop, Share->nblocks);
			regen_freelist(Share->top=next);
			continue;
		}
		Share->otop = Share->top;
#ifdef USE_SEMAPHORE
		if(USE_SEMAPHORE)
			Share->top =  next;
		else
#endif
		if(COMPARE_EXCHANGE(&Share->top,next,old)!=old)
		{
			logmsg(LOG_PROC+3, "block_alloc: concurrent access to head=%d", Share->top);
			Sleep(1);
			continue;
		}
		break;
	}
	if(type==BLK_VECTOR)
		ZeroMemory(block_ptr(old), BLOCK_SIZE);
	Blocktype[old] <<= 4;
	Blocktype[old] |= type;
	InterlockedDecrement(&Share->nblocks);
#ifdef BLOCK_TRACE
	if(!_save)
	{
		if(!Share->bitmap)
		{
			Share->bitmap = (unsigned short)old;
			_save = (unsigned int*)block_ptr(Share->bitmap);
			ZeroMemory(_save,BLOCK_SIZE);
			old = block_alloc(type);
			block_mark(Share->bitmap);
			Share->saveblk = block_alloc(BLK_VECTOR);
#ifdef USE_SEMAPHORE
			if(USE_SEMAPHORE)
				V(1,sigstate);
#endif
			return((unsigned short)old);
		}
		else
			_save = (unsigned int*)block_ptr(Share->bitmap);
	}
	if(!_save_blk && Share->saveblk)
		_save_blk = (unsigned short*)block_ptr(Share->saveblk);
	if(block_mark(old))
	{
		logmsg(0, "block_allocated but not freed=%d" FMT_FMBLK " prev=%s:%d" BLK_ARGS,Share->lastfile,Share->lastline);
		dumpit(20);
	}
#endif
#ifdef BLK_CHECK
	Share->lastline =  line;
	memcpy(Share->lastfile,file,5);
#endif
	InterlockedIncrement(&Generation[old]);
#ifdef USE_SEMAPHORE
	if(USE_SEMAPHORE)
		V(1,sigstate);
#endif
	return((unsigned short)old);
}

static DWORD WINAPI getntpid(void *arg)
{
	ExitThread(GetCurrentProcessId());
}

DWORD XGetProcessId(HANDLE ph)
{
	static DWORD (WINAPI *getprocid)(HANDLE);
	static int init;
	HANDLE th;
	DWORD ntpid=0,thid;
	if(!init)
	{
		if(!getprocid)
			getprocid = (DWORD(WINAPI*)(HANDLE))getsymbol(MODULE_kernel,"GetProcessId");
		init = 1;
	}
	if(getprocid)
		return((*getprocid)(ph));
	if(th=CreateRemoteThread(ph,sattr(0),0,getntpid,(void*)0,0,&thid))
		GetExitCodeThread(th,&ntpid);
	return(ntpid);
}
