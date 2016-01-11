#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Add to 'win' a character at (curx,cury).
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int waddwch(WINDOW* win, chtype c)
#else
int waddwch(win,c)
WINDOW*	win;
chtype	c;
#endif
{
	reg int	i, width;
	uchar	buf[CSMAX];
	chtype	a;
	wchar_t	code;

	a = ATTR(c);
	code = (wchar_t)(c&A_CHARTEXT);

	/* translate the process code to character code */
	width = _code2byte(code,(char*)buf);

	/* now call waddch to do the real work */
	for(i = 0; i < width; ++i)
		if(waddch(win,a|buf[i]) == ERR)
			return ERR;
	return OK;
}

#else
void _mbunused(){}
#endif
