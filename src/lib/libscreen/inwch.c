#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Get a process code at (curx,cury).
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
chtype winwch(WINDOW* win)
#else
chtype winwch(win)
WINDOW	*win;
#endif
{
	chtype	a;

	a = ATTR(win->_y[win->_cury][win->_curx]);
	return a|_byte2code(wmbinch(win,win->_cury,win->_curx));
}

#else
void _mbunused(){}
#endif
