#include	"scrhdr.h"

#if _MULTIBYTE


/*
**	Get a process code
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
wchar_t wgetwch(WINDOW* win)
#else
wchar_t wgetwch(win)
WINDOW	*win;
#endif
{
	int	c, n, type, width;
	uchar	buf[CSMAX];

	/* get the first byte */
	if((c = wgetch(win)) == ERR)
		return (wchar_t)ERR;

	type = TYPE(c);
	width = cswidth[type] - ((type == 1 || type == 2) ? 0 : 1);
	buf[0] = c;
	for(n = 1; n <= width; ++n)
	{
		if((c = wgetch(win)) == ERR)
			return (wchar_t)ERR;
		if(TYPE(c) != 0)
			return (wchar_t)ERR;
		buf[n] = c;
	}

	/* translate it to process code */
	return _byte2code((char*)buf);
}

#else
void _mbunused(){}
#endif
