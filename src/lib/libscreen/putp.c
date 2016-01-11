#include	<stdio.h>
#include	"termhdr.h"


/*
**	Output to stdout
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
static int _putstdout(int c)
#else
static int _putstdout(c)
int	c;
#endif
{
	return putchar(c&A_CHARTEXT);
}


/*
**	Output video attributes
*/
#if __STD_C
int vidattr(int newattrs)
#else
int vidattr(newattrs)
int	newattrs;
#endif
{
	return vidputs(newattrs,_putstdout);
}


/*
**	Output a string cap
*/
#if __STD_C
int putp(char* s)
#else
int putp(s)
char	*s;
#endif
{
	return tputs(s,1,_putstdout);
}
