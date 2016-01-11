#include	"termhdr.h"


/*
**	Get the display length of a string.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int mbdisplen(char* sp)
#else
int mbdisplen(sp)
char	*sp;
#endif
{
	reg int		n, m, k, ty;
	reg chtype	c;

	n = 0;
	for(; *sp != '\0'; ++sp)
	{
		if(!ISMBIT(*sp))
			++n;
		else
		{
			c = RBYTE(*sp);
			ty = TYPE(c);
			m  = cswidth[ty] - (ty == 0 ? 1 : 0);
			for(sp += 1, k = 1; *sp != '\0' && k <= m; ++k, ++sp)
				;
			if(k <= m)
				break;
			n += scrwidth[ty];
		}
	}
	return n;
}
