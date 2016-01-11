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
#include	"fsnt.h"
#include	<time.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<sys/time.h>
#include	<sys/timeb.h>
#include	<sys/select.h>

#define EPOCH		((ULONGLONG)0x019db1ded53e8000)
#define	UQUAD(high,low)	((((ULONGLONG)((unsigned)(high)))<<32)|(unsigned)(low))
#define MIC_TIC		(1000000000/HZ)
#define UZ		1000000


#define APR_1_2001	986108400
#define APR_8_2001	986713200
#define ONE_DAY		(24*3600)
#define FOUR_YEARS	(4*365*ONE_DAY+ONE_DAY)

/*
 * compute file time given unix time
 */
void	winnt_time(const struct timespec *tp, FILETIME *fp)
{
	ULONGLONG qw;
	qw = tp->tv_sec;
	qw *= FZ;
	qw += (tp->tv_nsec+50)/100;
	qw += EPOCH;
	fp->dwLowDateTime = (unsigned long)qw;
	qw >>= 32;
	fp->dwHighDateTime = (unsigned long)qw;
}

/*
 * return unix time corresponding to give file time
 * if flag is 1, then file time is used.  Otherwise, relative time
 */
void unix_time(const FILETIME *fp, struct timespec *tp,int flag)
{
	ULONGLONG	qw;

	qw = UQUAD(fp->dwHighDateTime,fp->dwLowDateTime);
	if(flag)
		qw -= EPOCH;
	tp->tv_sec = (time_t)(qw/FZ);
	qw %= FZ;
	tp->tv_nsec = (int)(100*qw);
}

static char *abbrev(char *dest,register const char *src, int n)
{
	register int c;
	register char *dp = dest;
	while((c= *src++) && n >1)
	{
		if(c>='A' && c <='Z')
		{
			*dp++ = c;
			n--;
		}
	}
	if(dp==dest)
		return((char*)src);
	*dp = 0;
	return(dest);
}

void tzset(void)
{
	char**		ev = *_ast_environ();

	static int	(*putenv)(const char*);
	static char	day[64];
	static char	std[64];

	if (!putenv)
		putenv = (int(*)(const char*))getsymbol(MODULE_msvcrt, "_putenv");
	if (ev && putenv)
	{
		char*	cp;

		while (cp = *ev++)
			if (cp[0]=='T' && cp[1]=='Z' && cp[2]=='=')
			{
				(*putenv)(cp);
				break;
			}
	}
	if(!state.clone.hp)
		_tzset();
	if (dllinfo._ast_daylight)
		*dllinfo._ast_daylight = _daylight;
	if (dllinfo._ast_timezone)
		*dllinfo._ast_timezone = _timezone;
	if (strchr(_tzname[0], ' '))
	{
		_tzname[0] = abbrev(std, _tzname[0], sizeof(std));
		_tzname[1] = abbrev(std, _tzname[1], sizeof(std));
	}
	dllinfo._ast_tzname[0] = _tzname[0];
	dllinfo._ast_tzname[1] = _tzname[1];
}

int time_getnow(FILETIME* f)
{
	SYSTEMTIME		sys;

	static int		beenhere;
	static void		(WINAPI *getnow)(FILETIME*);

	if (!beenhere)
	{
		beenhere = 1;
		getnow = (void (WINAPI *)(FILETIME*))getsymbol(MODULE_kernel, "GetSystemTimeAsFileTime");
	}
	if (getnow)
		(*getnow)(f);
	else
	{
		GetSystemTime(&sys);
		SystemTimeToFileTime(&sys, f);
	}
	return 0;
}

void time_diff(FILETIME* d, FILETIME* a, FILETIME* b)
{
	FILETIME	now;
	ULARGE_INTEGER	di;
	ULARGE_INTEGER	bi;

	if (!a)
		time_getnow(a = &now);
	if (!b)
		b = &Share->boot;
	di.u.HighPart = a->dwHighDateTime;
	di.u.LowPart = a->dwLowDateTime;
	bi.u.HighPart = b->dwHighDateTime;
	bi.u.LowPart = b->dwLowDateTime;
	di.QuadPart -= bi.QuadPart;
	if (!d)
		d = a;
	d->dwHighDateTime = di.u.HighPart;
	d->dwLowDateTime = di.u.LowPart;
}

void time_sub(FILETIME* d, FILETIME* a, ULONG s)
{
	FILETIME	now;
	ULARGE_INTEGER	di;

	if (!a)
		time_getnow(a = &now);
	di.u.HighPart = a->dwHighDateTime;
	di.u.LowPart = a->dwLowDateTime;
	di.QuadPart -= s;
	if (!d)
		d = a;
	d->dwHighDateTime = di.u.HighPart;
	d->dwLowDateTime = di.u.LowPart;
}

#if 0

/*
 * performance data helper api
 * some weird interaction with posix.dll boot
 * adding logmsg() calls avoid memory fault
 * dropping them brings the fault back in different places
 * at this point checking the registry is more efficient and reliable
 */

#include	<pdh.h>

typedef PDH_STATUS (*Pdh_OpenQuery_f)(LPCTSTR, DWORD_PTR, PDH_HQUERY*);
typedef PDH_STATUS (*Pdh_AddCounter_f)(PDH_HQUERY, LPCTSTR, DWORD_PTR, PDH_HCOUNTER*);
typedef PDH_STATUS (*Pdh_CollectQueryData_f)(PDH_HQUERY);
typedef PDH_STATUS (*Pdh_GetFormattedCounterValue_f)(PDH_HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
typedef PDH_STATUS (*Pdh_CloseQuery_f)(PDH_HQUERY);

typedef struct Pdh_s
{
	Pdh_OpenQuery_f			OpenQuery;
	Pdh_AddCounter_f		AddCounter;
	Pdh_CollectQueryData_f		CollectQueryData;
	Pdh_GetFormattedCounterValue_f	GetFormattedCounterValue;
	Pdh_CloseQuery_f		CloseQuery;
} Pdh_t;

static Pdh_t* init_pdh(void)
{
	static int		pid;
	static Pdh_t		pdh;

	if (P_CP->pid == pid)
		return &pdh;
	if (pid < 0)
		return 0;
	pid = -1;
	if ((pdh.OpenQuery = (Pdh_OpenQuery_f)getsymbol(MODULE_pdh, "PdhOpenQueryA")) &&
	    (pdh.AddCounter = (Pdh_AddCounter_f)getsymbol(MODULE_pdh, "PdhAddCounterA")) &&
	    (pdh.CollectQueryData = (Pdh_CollectQueryData_f)getsymbol(MODULE_pdh, "PdhCollectQueryData")) &&
	    (pdh.GetFormattedCounterValue = (Pdh_GetFormattedCounterValue_f)getsymbol(MODULE_pdh, "PdhGetFormattedCounterValue")) &&
	    (pdh.CloseQuery = (Pdh_CloseQuery_f)getsymbol(MODULE_pdh, "PdhCloseQuery")))
	{
		pid = P_CP->pid;
		logmsg(LOG_INIT+1, "performance data api enabled");
		return &pdh;
	}
	return 0;
}

/*
 * get performance counter value by name
 * 0 returned if not found or access failed
 */

static LONGLONG getcounter(const char* name)
{
	Pdh_t*			pdh;
	PDH_FMT_COUNTERVALUE	value;
	PDH_HQUERY		query;
	PDH_HCOUNTER		counter;
	DWORD			type;
	int			status;

	status = -1;
	value.largeValue = 0;
	if ((pdh = init_pdh()) && !pdh->OpenQuery(0, 0, &query))
	{
		logmsg(LOG_INIT+1, "counter '%s' query", name);
		if (!pdh->AddCounter(query, name, 0, &counter) &&
		    !pdh->CollectQueryData(query) &&
		    !pdh->GetFormattedCounterValue(counter, PDH_FMT_LARGE|PDH_FMT_NOSCALE, &type, &value))
			status = 0;
		pdh->CloseQuery(query);
	}
	logmsg(LOG_INIT+1, "counter '%s' status %d value %lld", name, status, value.largeValue);
	return value.largeValue;
}

/*
 * get system boot time by subtracting system uptime from the current time
 * this is called once for the current uwin instance
 */

int time_getboot(FILETIME* f)
{
	LONG	n;

	n = (LONG)getcounter("\\System\\System Up Time");
	logmsg(LOG_INIT+1, "system uptime %ld seconds", n);
	time_sub(f, 0, n);
	logmsg(LOG_INIT+1, "system uptime %ld seconds", n);
	return 0;
}

#else

/*
 * get system boot time by checking
 * (1) the start time of the System process ntpid 4
 * (2) the mttime of a registry key known to be updated
 *     at boot time and otherwise not likely to change
 * this is called once for the current uwin instance
 */

int time_getboot(FILETIME* f)
{
	HANDLE		h;
	HKEY		k;
	FILETIME	t;
	char*		m;
	char*		p;

	m = 0;
	if (h = OpenProcess(state.process_query, FALSE, 4))
	{
		if (GetProcessTimes(h, f, &t, &t, &t) && f->dwHighDateTime)
			m = "GetProcessTimes";
		CloseHandle(h);
	}
	if (!m && !RegOpenKeyEx(HKEY_LOCAL_MACHINE, p = "SYSTEM\\CurrentControlSet\\Control\\ComputerName", 0, KEY_READ, &k))
	{
		if (!RegQueryInfoKey(k, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, f))
		{
			m = p;
			(*(ULONGLONG*)f) -= 5 * 60 * (ULONGLONG)FZ;
		}
		RegCloseKey(k);
	}
	if (!m && !time_getnow(f))
		m = "time_getnow";
	if (!m)
		m = "ERROR";
	logmsg(LOG_INIT+1, "system boot time %(filetime)s (%s)", f, m);
	return 0;
}

#endif

static TOKEN_PRIVILEGES *time_priv(void)
{
	static int beenhere;
	static LUID bluid;
	static char buf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
	TOKEN_PRIVILEGES *tokpri=(TOKEN_PRIVILEGES *)buf;
	if(!beenhere)
	{
		if(!LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &bluid))
			return(0);
		beenhere = 1;
	}
	tokpri->PrivilegeCount = 1;
	tokpri->Privileges[0].Luid = bluid;
	tokpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	return(tokpri);
}

int settimeofday(struct timeval *tp, struct timezone *dp)
{
	int prevstate[256];
	struct timespec tv;
	SYSTEMTIME sys;
	FILETIME ft;
	HANDLE atok=0;
	int r = 0;
	if(IsBadReadPtr((void*)tp,sizeof(struct timeval)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!tp)
	{
		errno = EINVAL;
		return(-1);
	}
	tv.tv_sec = tp->tv_sec;
	tv.tv_nsec = 1000*tp->tv_usec;
	if(Share->Platform==VER_PLATFORM_WIN32_NT && !(atok=setprivs((void*)prevstate,sizeof(prevstate),time_priv)))
	{
		errno = EPERM;
		return(-1);
	}
	winnt_time(&tv,&ft);
	if(FileTimeToSystemTime(&ft,&sys))
		r=SetSystemTime(&sys);
	if(r)
		errno = unix_err(GetLastError());
	if(atok)
	{
		AdjustTokenPrivileges(atok,FALSE,(TOKEN_PRIVILEGES*)prevstate,0,NULL,0);
		closehandle(atok,HT_TOKEN);
	}
	return(r?0:-1);
}

int stime(const time_t *tp)
{
	struct timeval tv;
	if(tp)
		tv.tv_sec = *tp;
	tv.tv_usec = 0;
	return(settimeofday(tp?&tv:NULL,NULL));
}

time_t time(time_t *tp)
{
	struct timespec tv;
	FILETIME ftime;
	sigdefer(1);
	time_getnow(&ftime);
	unix_time(&ftime,&tv,1);
	if(tp)
		*tp = tv.tv_sec;
	sigcheck(0);
	return(tv.tv_sec);
}

/*
 *  This is just a fill in, it needs work
 */
int gettimeofday(struct timeval *tp, struct timezone *dp)
{
	FILETIME ftime;
	if(tp)
	{
	    	struct timespec tv;
		if(IsBadWritePtr((void*)tp,sizeof(struct timeval)))
		{
			errno = EFAULT;
			return(-1);
		}
		time_getnow(&ftime);
		unix_time(&ftime,&tv,1);
		tp->tv_sec = tv.tv_sec;
		tp->tv_usec = tv.tv_nsec/1000;
	}
	if(dp)
	{
		if(IsBadReadPtr((void*)dp,sizeof(struct timezone)))
		{
			errno = EFAULT;
			return(-1);
		}
		_tzset();
		*dllinfo._ast_daylight = _daylight;
		*dllinfo._ast_timezone = _timezone;
		dp->tz_minuteswest = *dllinfo._ast_timezone;
		dp->tz_dsttime = *dllinfo._ast_daylight;
	}
	return(0);
}

int ftime(struct timeb *tb)
{
	FILETIME filetime;
	struct timespec tp;
	if(!tb)
	{
		errno = EFAULT;
		return(-1);
	}
	time_getnow(&filetime);
	unix_time(&filetime,&tp,1);
	tb->time = tp.tv_sec;
	tb->millitm = (unsigned short)(tp.tv_nsec+500)/1000000;
	_tzset();
	*dllinfo._ast_daylight = _daylight;
	*dllinfo._ast_timezone = _timezone;
	tb->timezone = (short)(*dllinfo._ast_timezone);
	tb->dstflag = *dllinfo._ast_daylight;
	return(0);
}

unsigned long vtick_count(void)
{
	FILETIME ct,et,kt,ut;
	struct timespec user,sys;
	if(GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))
	{
		unix_time(&ut,&user,0);
		unix_time(&kt,&sys,0);
		return (unsigned long)(1000*(sys.tv_sec+user.tv_sec)+(sys.tv_nsec+user.tv_nsec+500)/1000000);
	}
	logerr(LOG_PROC+5, "GetProcessTimes");
	return(0);
}

static int timer(const struct itimerval *tp, struct itimerval *oldtp,int which)
{
	unsigned long ctime,remaining;
	unsigned long milliseconds=0;
	unsigned long interval=0;
	Palarm_t *ap;
	char name[UWIN_RESOURCE_MAX];
	if(which==ITIMER_REAL)
		ap = &P_CP->treal;
	else
		ap = &P_CP->tvirtual;
	if(tp)
	{
		milliseconds = (unsigned long)(1000*tp->it_value.tv_sec + tp->it_value.tv_usec/1000);
		if(milliseconds==0 && tp->it_value.tv_usec)
			milliseconds = 1;
		interval = (unsigned long)(1000*tp->it_interval.tv_sec + tp->it_interval.tv_usec/1000);
	}
	if(oldtp)
	{
		oldtp->it_value.tv_sec = oldtp->it_value.tv_usec = 0;
		oldtp->it_interval.tv_sec = oldtp->it_interval.tv_usec = 0;
		if(ap->remain)
		{
			if(which==ITIMER_REAL)
				ctime = GetTickCount();
			else
				ctime = vtick_count();
			if (ap->remain > ctime)
			{
				remaining = ap->remain-ctime;
				oldtp->it_value.tv_sec = remaining/1000;
				oldtp->it_value.tv_usec = (suseconds_t)(1000*(remaining-1000*oldtp->it_value.tv_sec));
			}
		}
		if(ap->interval)
		{
			oldtp->it_interval.tv_sec = ap->interval/1000;
			oldtp->it_interval.tv_usec = (suseconds_t)(1000*(ap->interval-1000*oldtp->it_interval.tv_sec));
		}
	}
	if(tp)
	{
		if (milliseconds > 0)
		{
			if(which==ITIMER_REAL)
				ap->remain = GetTickCount() + milliseconds;
			else
				ap->remain = vtick_count() + milliseconds;
			/* PulseEvent(P_CP->sevent); */
			ap->interval = interval;
		}
		else
			ap->remain = 0;

		if(!P_CP->alarmevent)
			P_CP->alarmevent = mkevent(UWIN_EVENT_ALARM(name),NULL);

		if(!SetEvent(P_CP->alarmevent))
			logerr(0, "SetEvent");
	}
	return(0);
}

useconds_t ualarm(useconds_t useconds, useconds_t interval)
{
	struct itimerval tm;
	tm.it_value.tv_sec = useconds/UZ;
	tm.it_value.tv_usec = (suseconds_t)(useconds - UZ*tm.it_value.tv_sec);
	tm.it_interval.tv_sec = interval/UZ;
	tm.it_interval.tv_usec = (suseconds_t)(interval - UZ*tm.it_value.tv_sec);
	if(timer(&tm,&tm,ITIMER_REAL)<0)
		return 0;
	return (useconds_t)(UZ*tm.it_value.tv_sec+tm.it_value.tv_usec);
}

unsigned int alarm(unsigned int seconds)
{
	struct itimerval tm;
	tm.it_value.tv_sec = seconds;
	tm.it_value.tv_usec = 0;
	tm.it_interval.tv_sec = 0;
	tm.it_interval.tv_usec = 0;
	if(timer(&tm,&tm,ITIMER_REAL)<0)
		return (unsigned)-1;
	if(tm.it_value.tv_sec==0)
		return tm.it_value.tv_usec>0;
	return (int)(tm.it_value.tv_sec+(tm.it_value.tv_usec>500));
}

int getitimer(int which, struct itimerval *tp)
{
	if(which!=ITIMER_REAL && which!=ITIMER_VIRTUAL)
	{
		if(which==ITIMER_PROF)
			errno = ENOSYS;
		else
			errno = EINVAL;
		return(-1);
	}
	if(IsBadReadPtr((void*)tp,sizeof(struct itimerval)))
	{
		errno = EFAULT;
		return(-1);
	}
	return(timer(0,tp,which));
}

/*
 * doesn't handle intervals yet
 */
int setitimer(int which, const struct itimerval *newp, struct itimerval *oldp)
{
	if(IsBadReadPtr((void*)newp,sizeof(struct itimerval)) ||
		(oldp && IsBadReadPtr((void*)oldp,sizeof(struct itimerval))))
	{
		errno = EFAULT;
		return(-1);
	}
	if(which!=ITIMER_REAL && which!=ITIMER_VIRTUAL)
	{
		if(which==ITIMER_PROF)
			errno = ENOSYS;
		else
			errno = EINVAL;
		return(-1);
	}
	return(timer(newp,oldp,which));
}

int usleep(useconds_t usec)
{
	if(usec==0)
		return(0);
	else if(usec >= 1000000)
	{
		errno = EINVAL;
		return(-1);
	}
	P_CP->state = PROCSTATE_SIGWAIT;
	sigdefer(1);
	WaitForSingleObject(P_CP->sigevent,(usec + 500) / 1000);
	P_CP->state = PROCSTATE_RUNNING;
	sigcheck(0);
	return(0);
}

unsigned int sleep (unsigned int seconds)
{
	int milli = GetTickCount()+1000*seconds;
	P_CP->state = PROCSTATE_SIGWAIT;
	sigdefer(1);
	WaitForSingleObject(P_CP->sigevent,seconds * 1000);
	P_CP->state = PROCSTATE_RUNNING;
	if((milli -= GetTickCount()) <0)
		milli = 0;
	/* clear pending alarm if not previously set but blocked */
	sigcheck(0);
	return(milli/1000);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	int milli,count;
	if(IsBadReadPtr((void*)rqtp,sizeof(struct timespec)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(rqtp->tv_nsec<0 || rqtp->tv_nsec > 1000000000)
	{
		errno = EINVAL;
		return(-1);
	}
	if(rmtp)
	{
		if(IsBadWritePtr((void*)rmtp,sizeof(struct timespec)))
		{
			errno = EFAULT;
			return(-1);
		}
	}
	milli = (rqtp->tv_nsec + 500000)/1000000;
	if(milli==0 && rqtp->tv_sec==0)
		goto finish;
	count = GetTickCount();
	milli += (int)(1000*rqtp->tv_sec);
	P_CP->state = PROCSTATE_SIGWAIT;
	sigdefer(1);
	WaitForSingleObject(P_CP->sigevent,milli);
	P_CP->state = PROCSTATE_RUNNING;
	if((milli -= (GetTickCount()-count)) <0)
		milli = 0;
	if(milli<2)
		milli = 0;
	/* clear pending alarm if not previously set but blocked */
	sigcheck(0);
finish:
	if(rmtp)
	{
		rmtp->tv_sec = milli/1000;
		milli %= 1000;
		rmtp->tv_nsec = 1000000*milli;
	}
	return(milli?-1:0);
}

clock_t  proc_time(Pproc_t *pp, struct timespec *user, struct timespec *sys)
{
	int milli = GetTickCount();
	int ticks = (HZ*milli+HZ/2)/1000;
	if (user && sys)
	{
		FILETIME ct,et,kt,ut;
		HANDLE hp;
		if(pp==P_CP)
			hp = GetCurrentProcess();
		else if(pp->ppid==P_CP->pid)
			hp = pp->ph;
		else
			hp = OpenProcess(state.process_query,FALSE,(DWORD)pp->ntpid);
		if(hp && GetProcessTimes(hp,&ct,&et,&kt,&ut))
		{
			pp->start = *(ULONGLONG*)&ct >= *(ULONGLONG*)&Share->boot ? ct : Share->boot;
			unix_time(&ut,user,0);
			unix_time(&kt,sys,0);
		}
		else
		{
			if(hp)
				logerr(LOG_PROC+5, "GetProcessTimes");
			user->tv_sec = sys->tv_sec = 0;
			user->tv_nsec = sys->tv_nsec = 0;
		}
		if(pp!=P_CP && pp->ppid!=P_CP->pid && hp)
			closehandle(hp,HT_PROC);
	}
	return(ticks);
}

clock_t proc_times(Pproc_t *pp, struct tms *buffer)
{
	struct timespec user,sys;
	clock_t ticks;
	if(buffer)
	{
		ticks = proc_time(pp,&user,&sys);
		buffer->tms_utime =  (clock_t)(HZ*user.tv_sec + (user.tv_nsec+MIC_TIC/2)/MIC_TIC);
		buffer->tms_stime =  (clock_t)(HZ*sys.tv_sec + (sys.tv_nsec+MIC_TIC/2)/MIC_TIC);
		buffer->tms_cutime = (clock_t)pp->cutime;
		buffer->tms_cstime = (clock_t)pp->cstime;
	}
	else
		ticks = (HZ*GetTickCount()+HZ/2)/1000;
	return(ticks);
}

clock_t clock(void)
{
	struct timespec user,sys;
	proc_time(P_CP,&user,&sys);
	user.tv_sec += sys.tv_sec;
	user.tv_nsec += sys.tv_nsec;
	if(user.tv_sec==0 && user.tv_nsec==0)
	{
		/* GetProcTimes() may not always work */
		return 1000*GetTickCount();
	}
	return (clock_t)(1000000*user.tv_sec + user.tv_nsec/1000);
}

clock_t times(struct tms *buffer)
{
	if(IsBadWritePtr((void*)buffer,sizeof(struct tms)))
	{
		errno = EFAULT;
		return(-1);
	}
	return(proc_times(P_CP,buffer));
}


void clock2timeval(clock_t c, struct timeval *tp)
{
	tp->tv_sec = c/HZ;
	tp->tv_usec = (suseconds_t)(1000000*(c-HZ*tp->tv_sec))/HZ;
}

int getrusage(int who, struct rusage *rp)
{
	struct timespec ru_utime, ru_stime;
	if(who!=RUSAGE_SELF && who!=RUSAGE_CHILDREN)
	{
		errno = EINVAL;
		return(-1);
	}
	if(IsBadReadPtr((void*)rp,sizeof(struct rusage)))
	{
		errno = EFAULT;
		return(-1);
	}
	if(dllinfo._ast_libversion>9)
		ZeroMemory(rp,sizeof(struct rusage));
	proc_time(P_CP,&ru_utime,&ru_stime);
	rp->ru_utime.tv_sec = ru_utime.tv_sec ;
	rp->ru_stime.tv_sec = ru_stime.tv_sec ;
	rp->ru_utime.tv_usec = ru_utime.tv_nsec/1000 ;
	rp->ru_stime.tv_usec = ru_stime.tv_nsec ;
	if(who!=RUSAGE_SELF)
	{
		struct timeval tv;
		clock2timeval((clock_t)P_CP->cutime,&tv);
		rp->ru_utime.tv_sec += tv.tv_sec;
		rp->ru_utime.tv_usec += tv.tv_usec;
		clock2timeval((clock_t)P_CP->cstime,&tv);
		rp->ru_stime.tv_sec += tv.tv_sec;
		rp->ru_stime.tv_usec += tv.tv_usec;
	}
	return(0);
}

#if _X64_
#define WOWSYM(x)	"_" x "64"
#else
#define WOWSYM(x)	x
#endif

extern char* ctime(const time_t* tp)
{
	time_t	t;

	static  char *(*clib_ctime)(const time_t*);

	if (!clib_ctime && !(clib_ctime = (char*(*)(const time_t*))getsymbol(MODULE_msvcrt, WOWSYM("ctime"))))
		return 0;
	if (!tp)
	{
		t = time(0);
		tp = &t;
	}
	return (*clib_ctime)(tp);
}

extern char* asctime(const struct tm* tm)
{
	static  char *(*clib_asctime)(const struct tm*);

	if (!clib_asctime && !(clib_asctime = (char*(*)(const struct tm*))getsymbol(MODULE_msvcrt, WOWSYM("asctime"))))
		return 0;
	return (*clib_asctime)(tm);
}

extern struct tm* gmtime(const time_t* tp)
{
	time_t	t;

	static  struct tm *(*clib_gmtime)(const time_t*);

	if (!clib_gmtime && !(clib_gmtime = (struct tm*(*)(const time_t*))getsymbol(MODULE_msvcrt, WOWSYM("gmtime"))))
		return 0;
	if (!tp)
	{
		t = time(0);
		tp = &t;
	}
	return (*clib_gmtime)(tp);
}

/*
 * This code works around a bug in strftime() that is fixed
 * in ast5.3 by alternating between two static buffers
 */
extern struct tm* localtime(const time_t* tp)
{
	struct tm*		tm;
	struct tm		save;
	time_t			t = tp ? *tp : time(0);
	int			year_offset;

	static struct tm*	(*clib_loctime)(const time_t*);
	static struct tm	alternate;
	static struct tm*	old;
	if(state.clone.hp)
	{
		t -= (*dllinfo._ast_timezone- *dllinfo._ast_daylight*3600);
		return(gmtime(&t));
	}

	if (!clib_loctime && !(clib_loctime = (struct tm*(*)(const time_t*))getsymbol(MODULE_msvcrt, WOWSYM("localtime"))))
		return 0;
	if (t < 0)
	{
		year_offset = (int)(-t-1)/FOUR_YEARS + 1;
		t += year_offset*FOUR_YEARS;
	}
	else
		year_offset = 0;
	if (old)
		save = *old;
	if (tm = (*clib_loctime)(&t))
	{
		alternate = *tm;
		/* fixup MS C-library bug with DST on 2001-04-01 */
		if (t >= APR_1_2001 && t < APR_8_2001 && !tm->tm_isdst)
		{
			time_t t1 = APR_8_2001;
			tm = (*clib_loctime)(&t1);
			if (tm->tm_isdst)
			{
				if ((t += 3600) < APR_8_2001)
					tm = (*clib_loctime)(&t);
				else	/* 2-3 AM 2001/04/08 */
				{
					*tm = alternate;
					tm->tm_hour += 1;
				}
				tm->tm_isdst = 1;
				alternate = *tm;
			}
			else
				*tm = alternate;
		}
		if (old)
			*tm = save;
		tm = &alternate;
		old = tm;
		if (year_offset)
		{
			tm->tm_year -= 4*year_offset;
			tm->tm_wday = (tm->tm_wday+year_offset*2)%7;
		}
	}
	return tm;
}

char* asctime_r(const struct tm* tm, char* buf)
{
	char*	r;

	if (r = asctime(tm))
		return strcpy(buf, r);
	return 0;
}

char* ctime_r(const time_t* tp, char* buf)
{
	char*	r;

	if (r = ctime(tp))
		return strcpy(buf, r);
	return 0;
}

struct tm* gmtime_r(const time_t* tp, struct tm* result)
{
	struct tm*	tm;

	if (tm = gmtime(tp))
	{
		*result = *tm;
		return result;
	}
	return 0;
}

struct tm* localtime_r(const time_t* tp, struct tm* result)
{
	struct tm*	tm;

	if (tm = localtime(tp))
	{
		*result = *tm;
		return result;
	}
	return 0;
}

unsigned long trandom(void)
{
	static unsigned oldx;
	FILETIME now;
	unsigned long h=0,x;
	while(1)
	{
		time_getnow(&now);
		x = now.dwLowDateTime;
		if(x!=oldx)
			break;
		Sleep(1);
	}
	oldx = x;
	h = HASHPART(h,x>>24);
	h = HASHPART(h,x>>16);
	h = HASHPART(h,x>>8);
	return(h);
}
