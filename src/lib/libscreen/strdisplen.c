#include	"scrhdr.h"


/*
**	Get the display length of a string.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int strdisplen(char* str, int count, int* nbytes, int* cntl)
#else
int strdisplen(str,count,nbytes,cntl)
char	*str;		/* chars to be measured */
int	count;		/* how many to measured, -1 if all should be considered */
int	*nbytes;	/* number of bytes subsumed */
int	*cntl;		/* type of embedded control characters */
#endif
{
	reg int		n, type;
	reg char	*sp;

	n = 0;
	type = 0;
	for(sp = str ? str : ""; *sp != '\0' && count != 0; count--)
	{
#if _MULTIBYTE
		if(ISMBIT(*sp))
		{	/* multi-byte char */
			reg int	c, m, k, ty;

			c = RBYTE(*sp);
			ty = TYPE(c);
			m  = cswidth[ty] - (ty == 0 ? 1 : 0);
			for(sp += 1, k = 1; *sp != '\0' && k <= m; ++k, ++sp)
				;
			/* this char is invalid, stop now */
			if(k <= m)
				break;
			n += scrwidth[ty];
		}
		else
#endif
		{
			if(iscntrl(*sp))
			{
				type = 1;
				n += *sp == '\t' ? (TABSIZ - (n%TABSIZ)) : 2;
			}
			else	n += 1;
			sp += 1;
		}
	}
	if(nbytes)
		*nbytes = sp-str;
	if(cntl)
		*cntl = type;
	return n;
}
