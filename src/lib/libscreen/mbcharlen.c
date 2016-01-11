#include	"termhdr.h"


#if _MULTIBYTE

/*
**	Get the # of valid characters
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int mbcharlen(char* sp)
#else
int mbcharlen(sp)
char	*sp;
#endif
{
	reg int		n, m, k, ty;
	reg chtype	c;

	n = 0;
	for(; *sp != '\0'; ++sp, ++n)
		if(ISMBIT(*sp))
		{
			c = RBYTE(*sp);
			ty = TYPE(c);
			m  = cswidth[ty] - (ty == 0 ? 1 : 0);
			for(sp += 1, k = 1; *sp != '\0' && k <= m; ++k, ++sp)
			{
				c = RBYTE(*sp);
				if(TYPE(c) != 0)
					break;
			}
			if(k <= m)
				break;
		}
	return n;
}

#else
void _mbunused(){}
#endif
