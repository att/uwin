#include	"termhdr.h"

#if _MULTIBYTE

/*
**	Read a process code from the terminal
**	cntl:	= 0 for single-char key only
**		= 1 for matching function key and macro patterns.
**		= 2 same as 1 but no time-out for funckey matching.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
wchar_t tgetwch(int cntl)
#else
wchar_t tgetwch(cntl)
int	cntl;
#endif
{
	int	c, n, type, width;
	char	buf[CSMAX];

	/* get the first byte */
	if((c = tgetch(cntl)) == ERR)
		return (wchar_t)ERR;

	type = TYPE(c);
	width = cswidth[type] - ((type == 1 || type == 2) ? 0 : 1);
	buf[0] = c;
	for(n = 1; n < width; ++n)
	{
		if((c = tgetch(cntl)) == ERR)
			return (wchar_t)ERR;
		buf[n] = c;
	}

	/* translate it to process code */
	return _byte2code(buf);
}

#else
void _mbunused(){}
#endif
