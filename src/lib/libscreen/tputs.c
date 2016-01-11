#include	"termhdr.h"



/*
**	The following array gives the number of tens of milliseconds per
**	character for each speed . For example, since 300 baud returns a 7,
**	there are 33.3 milliseconds per char at 300 baud.
**
**	Written by Kiem-Phong Vo
*/
static short	tmspc10[] =
{
    /*  0	50	75	110	134.5	150	200	300 baud */
	0,	2000,	1333,	909,	743,	666,	500,	333,

    /*	600	1200	1800	2400	4800	9600	19200	38400 baud */
	166,	83,	55,	41,	20,	10,	5,	2
};

#if __STD_C
int _delay(int d, int(*outc)(int))
#else
int _delay(d,outc)
int	d;
int	(*outc)();
#endif
{
	reg int		mspc10;
	reg char	pc;

	/* If no delay needed, or no comprehensible output speed */
	if(!T_pad || d <= 0 || _ospeed <= 0 ||
	   _ospeed >= (sizeof(tmspc10) / sizeof(tmspc10[0])))
		return OK;

	/* Round up by a half a character frame */
	mspc10 = tmspc10[_ospeed];
	d += mspc10 / 2;
	pc = T_pad[0];
	for(d /= mspc10; d > 0; --d)
		if((*outc)(pc) < 0)
			return ERR;

	return OK;
}


/*
**	Compute a delay length
*/
#if __STD_C
static int dodelay(char** sp, int affcnt)
#else
static int dodelay(sp,affcnt)
char	**sp;
int	affcnt;
#endif
{
	reg char	*cp;
	reg int		d;

	cp = istermcap() ? *sp : *sp + 2;

	/* convert the number representing the delay. */
	d = 0;
	while(isdigit(*cp))
		d = d * 10 + (*cp++ - '0');

	d *= 10;
	if(*cp == '.')
	{
		cp++;
		if(isdigit(*cp))
			d += *cp - '0';

		/* use only one digit to the right of '.' */
		while(isdigit(*cp))
			cp++;
	}

	/* multiply by the affected lines count. */
	if(*cp == '*')
	{
		cp++;
		d *= affcnt;
	}

	/* if this is terminfo */
	if(!istermcap())
		/* this wasn't at all a delay sequence */
		if(*cp != '>')
			return -1;
		/* this is */
		else	++cp;

	*sp = cp;
	return d;
}


/*
**	Put a character string out with padding.
**	cp : the string to be put out.
**	affcnt: number of lines affected.
**	outc: function to output characters.
*/
#if __STD_C
int tputs(char* outs, int affcnt, int(*outc)(int))
#else
int tputs(outs,affcnt,outc)
char	*outs;
int	affcnt;
int	(*outc)();
#endif
{
	reg int		d;

	if(!outs || !outs[0])
		return ERR;

	if(istermcap())
		d = dodelay(&outs,affcnt);

	/* put out the string */
	while(*outs)
	{
		/* possible terminfo delay style */
		if(!istermcap() && *outs == '$' && *(outs+1) == '<')
		{	/* if false alarm, output to avoid infinite loop */
			if((d = dodelay(&outs,affcnt)) == -1)
				if((*outc)(*outs++) < 0)
					return ERR;

			/* real delay */
			else if(_delay(d,outc) == ERR)
				return ERR;
		}
		/* just a regular character */
		else if((*outc)(*outs++) < 0)
			return ERR;
	}

	if(istermcap() && d > 0 && _delay(d,outc) == ERR)
		return ERR;

	return OK;
}
