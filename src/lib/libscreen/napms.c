#include	"termhdr.h"

#define NAPINTERVAL	100
#define SECOND		1000
#define TIC_SEC		60


/*
**	Sleep for ms milliseconds.
**	Here are some reasonable ways to get a good nap.
**
**	(1) Use the select system call
**
**	(2) Install the ft (fast timer) device in your kernel.
**	    This is a pseudo-device to which an ioctl will wait n ticks
** 	   and then send you an alarm.
**
**	(3) Install the nap system call in your kernel.
**	    This system call does a timeout for the requested number of ticks.
**
**	(4) Write a routine that busy waits checking the time with ftime.
** 	    Ftime is not present on SysV systems, and since this busy waits,
**	    it will drag down response on your system.  But it works.
**
**	Written by Kiem-Phong Vo
*/


/*
**	Delay for a number of ticks.
*/
#ifndef HASNAP
#if _lib_select
#define HASNAP
#include	<sys/time.h>

#if __STD_C
static int nap(int ticks)
#else
static int nap(ticks)
int	ticks;
#endif
{
	struct timeval		d;

	d.tv_sec = 0;
	d.tv_usec = (ticks*SECOND)/TIC_SEC;
	select(20,0,0,0,&d);

	return OK;
}
#endif  /* _lib_select */
#endif	/* HASNAP */


#ifndef	HASNAP
#ifdef FTIOCSET
#define HASNAP

/*
**	The following code is adapted from the sleep code in libc.
**	It uses the "fast timer" device posted to USENET in Feb 1982.
**	nap is like sleep but the units are ticks (e.g. 1/60ths of
**	seconds in the USA).
*/

#include	<setjmp.h>
#include	<signal.h>
static jmp_buf	jmp;
static int	ftfd;

static napx()
{
	longjmp(jmp, 1);
}

#if __STD_C
static	nap(int ticks)
#else
static	nap(ticks)
int	ticks;
#endif
{
	unsignedint	altime;
	Signal_t	alsig = SIG_DFL;
	char		*ftname;
	struct requestbuf
	{
		short time;
		short signo;
	} rb;

	if(ticks <= 0)
		return OK;

	if(ftfd <= 0)
	{
		ftname = "/dev/ft0";
		while(ftfd <= 0 && ftname[7] <= '~')
		{
			ftfd = open(ftname, 0);
			if (ftfd <= 0)
				ftname[7]++;
		}
	}

	if(ftfd <= 0)
	{	/* Couldn't open a /dev/ft? */
		sleep((ticks+(TIC_SEC-1))/TIC_SEC);
		return ERR;
	}

	altime = alarm(1000);	/* time to maneuver */
	if(setjmp(jmp))
	{
		signal(SIGALRM, alsig);
		alarm(altime);
		return OK;
	}

	if(altime)
	{
		if(altime > ticks)
			altime -= ticks;
		else
		{
			ticks = altime;
			altime = 1;
		}
	}
	alsig = signal(SIGALRM,napx);
	rb.time = ticks;
	rb.signo = SIGALRM;
	ioctl(ftfd,FTIOCSET,&rb);
	for(;;)
		pause();

	/*NOTREACHED*/
	return OK;
}
#endif	/* FTIOCSET */
#endif	/* HASNAP */


/*
**	Desperate case
*/
#ifndef HASNAP
#if __STD_C
static	nap(int ticks)
#else
static	nap(ticks)
int	ticks;
#endif
{
	sleep((ticks+(TIC_SEC-1))/TIC_SEC);
	return OK;
}
#endif	/* HASNAP */

#if __STD_C
int napms(int ms)
#else
int napms(ms)
int ms;
#endif
{
	int	secs;

	if(ms <= 0)
		return OK;

	if((secs = ms/SECOND) > 0)
		sleep(secs);

	return nap(ms/(SECOND/TIC_SEC));
}
