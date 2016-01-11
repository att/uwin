#include	"scrhdr.h"


/*
**	Return unctrl codes
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
char* unctrl(unsigned int c)
#else
char* unctrl(c)
unsigned int	c;
#endif
{
	static char	code[64];

#if _MULTIBYTE
	c &= A_CHARTEXT;
	if(ISMBIT(c))
	{	c = _code2byte((wchar_t)c, code);
		code[c] = 0;
	}
	else
#endif
	if(iscntrl(c))
	{	code[0] = '^';
		code[1] = UNCTRL(c);
		code[2] = 0;
	}
	else if(c < 0177)
	{	code[0] = c;
		code[1] = 0;
	}
	else
	{	char	*sp, *cp, *ep;
		*(ep = &code[sizeof(code)-1]) = 0;
		for(sp = ep;; )
		{	*--sp = '0' + (c&07);
			if((c >>= 3) <= 0)
				break;
		}
		*--sp = '0';
		*--sp = '\\';
		for(cp = code; sp <= ep; )
			*cp++ = *sp++;
	}

	return code;
}
