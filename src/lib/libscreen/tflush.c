#include	<scrhdr.h>

#ifdef _SFIO_H
static int	_dummy;
#else

int _tflush()
{
	reg uchar	*buf;
	reg int		wn, n;

	buf = _curbuf;
	if((n = _bptr - buf) == 0)
		return 0;
	_bptr = buf;
	for(;;)
	{
		if((wn = write(fileno(_output),(char*)buf,n)) == 0)
			return -1;
		else if(wn > 0)
		{	/* progressing */
			buf += wn;
			if((n -= wn) <= 0)
				return 0;
		}
		else
		{	/* error, see if it was just a signal interrupt */
#ifdef EWOULDBLOCK
			if(errno == EWOULDBLOCK)
				errno = EINTR;
#endif
			if(errno != EINTR)
				return -1;
			errno = 0;
		}
	}
}

#endif
