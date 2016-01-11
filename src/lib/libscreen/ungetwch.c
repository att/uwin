#include	"termhdr.h"

#if _MULTIBYTE


/*
**	Push a process code back into the input stream
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int ungetwch(wchar_t code)
#else
int ungetwch(code)
wchar_t	code;
#endif
{
	int	i, n;
	uchar	buf[CSMAX];

	n = _code2byte(code,(char*)buf);
	for(i = n-1; i >= 0; --i)
		if(ungetch(buf[i]) == ERR)
		{
			/* remove inserted characters */
			for(i = i+1; i < n; ++i)
				tgetch(0);
			return ERR;
		}

	return OK;
}

#else
void _mbunused(){}
#endif
