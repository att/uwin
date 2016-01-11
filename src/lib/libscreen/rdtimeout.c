#include	"termhdr.h" 	/* includes <errno.h> and <sys/types.h> */

#define SECOND	1000    	/* a second in milliseconds */

#if _lib_select
#define TIMEB		struct timeval
#else

static long		Tm;	/* origin of time */
#define LTIME(tm)	(((tm)-Tm)*SECOND)

#if _hdr_termio
#include		<sys/times.h>
#define TIMEB		struct tms
#define FNODELAY	O_NDELAY
#ifndef TIC_SEC
#define TIC_SEC		75
#endif
#define SETTIME(tb)	(Tm = times(&(tb)))
#define GETTIME(tm,tb)	((tm) = LTIME(times(&(tb)))/TIC_SEC)
#else
#include		<sys/timeb.h>
#define TIMEB		struct timeb
#define FNODELAY	FNDELAY
#define SETTIME(tb)	(ftime(&(tb)), Tm = (tb).time)
#define GETTIME(tm,tb)	(ftime(&(tb)), (tm) = LTIME((tb).time) + (tb).millitm)
#endif	/*_hdr_termio*/

#endif /*_lib_select*/

/*
**	Time-out read.
**
**	inputd:	input file descriptor
**	waittm: delay time in units of milli-second.
**	buf:	to read characters in
**	n_want: max amount to read.
*/
#if __STD_C
int _rdtimeout(int inputd, int waittm, uchar* buf, int n_want)
#else
int _rdtimeout(inputd,waittm,buf,n_want)
reg int		inputd, waittm;
reg uchar	*buf;
reg int		n_want;
#endif
{
	int		f;
	reg int		n_read;
	TIMEB		tmb;
#if !_lib_select
	long		begtm, curtm;
#if _hdr_termio
	unsigned int	lflag, iflag, vtime, vmin;
#endif
#endif

	if(_inputeof)
		return -1;

	errno = 0;
	if(waittm < 0)
	{	/* blocking read */
		n_read = read(inputd,(char*)buf,n_want);
		if(n_read < 0 && errno == EINTR)
				errno = 0;
		if(n_read == 0)
			_inputeof = TRUE;
		return n_read;
	}

#if _lib_select /* easy case */
	f = 1 << inputd;
	tmb.tv_sec = waittm/SECOND;
	tmb.tv_usec = (waittm%SECOND)*SECOND;
	if((n_read = select(20,(fd_set*)&f,0,0,&tmb)) > 0)
	{	while((n_read = read(inputd,(char*)buf,n_want)) < 0)
		{	if(errno == EINTR)
				errno = 0;
			else	break;
		}
		if(n_read == 0)
			_inputeof = TRUE;
	}
#else
#if _hdr_termio
	lflag = _curtty.c_lflag;
	iflag = _curtty.c_iflag;
	vmin  = _curtty.c_cc[VMIN];
	vtime = _curtty.c_cc[VTIME];
	if((f = (waittm+99)/100) > 10)
		f = 10;
	_curtty.c_lflag &= ~(ISIG|ICANON);
	_curtty.c_iflag &= ~(ISTRIP|IXON);
	_curtty.c_cc[VMIN] = 0;
	_curtty.c_cc[VTIME] = f; /* convert to 10th of sec */
	SETTY(_curtty);
	if(waittm > f*100)
	{	/* initialize time */
		SETTIME(tmb);
		GETTIME(begtm,tmb);
	}
#else
	f = fcntl(inputd,F_GETFL,0);
	fcntl(inputd,F_SETFL,FNODELAY);
	if(waittm > 0)
	{	/* initialize time */
		SETTIME(tmb);
		GETTIME(begtm,tmb);
	}
#endif

	/* this loop allows a resume after interrupts */
	for(;;)
	{
		if((n_read = read(inputd,(char*)buf,n_want)) <= 0)
		{
#ifdef EWOULDBLOCK
			if(errno == EWOULDBLOCK)
				n_read = errno = 0;
#endif
			if(errno == EINTR)
				n_read = errno = 0;
		}
		if(n_read != 0 ||
#if _hdr_termio
		   waittm <= f*100
#else
		   waittm == 0
#endif
		  )
			break;

		/* see if we're out of time yet */
		GETTIME(curtm,tmb);
		if(curtm < begtm || (curtm-begtm) >= waittm)
			break;
	}

#if _hdr_termio
	_curtty.c_lflag = lflag;
	_curtty.c_iflag = iflag;
	_curtty.c_cc[VMIN] = vmin;
	_curtty.c_cc[VTIME] = vtime;
	SETTY(_curtty);
#else
	fcntl(inputd,F_SETFL,f);
#endif
#endif /*_lib_select*/

	return n_read;
}
