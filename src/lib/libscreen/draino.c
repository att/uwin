#include	"termhdr.h"


/*
**	Wait until output has drained enough so that only 'ms'
**	more millisecs are needed for it to drain completely.
**
**	Written by Kiem-Phong Vo
*/

#define NAPINTERVAL	100

#if __STD_C
int draino(int ms)
#else
int draino(ms)
int	ms;
#endif
{
#ifdef TIOCOUTQ
	int	ncneeded, ncthere;
	/* 10 bits/char, 1000 ms/sec and baudrate is bits/sec */
	ncneeded = baudrate()*ms/(10*1000);
	for(;;)
	{	if(ioctl(_outputd,TIOCOUTQ,&ncthere) < 0)
			return ERR;
		if(ncthere < ncneeded)
			return OK;
		napms(NAPINTERVAL);
	}
#else /* TICOUTQ */
#ifdef TCSETAW
	if(ms > 0)
		return ERR;
	else
	{	ioctl(_outputd,TCSETAW,cur_term->Nttyb);
		return OK;
	}
#else /* TCSETAW */
	return ERR;
#endif /* TCSETAW */
#endif /* TIOCOUTQ */
}
