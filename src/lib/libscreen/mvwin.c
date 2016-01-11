#include	"scrhdr.h"


/*
**	Move top-left corner of a window to a new point.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int mvwin(WINDOW* win, int by, int bx)
#else
int mvwin(win,by,bx)
WINDOW	*win;
int	by, bx;
#endif
{
	if(by == win->_begy && bx == win->_begx)
		return OK;

	if(by < 0 || by >= LINES || bx < 0 || bx >= COLS)
		return ERR;

	win->_begy = by;
	win->_begx = bx;

	wtouchln(win,0,win->_maxy,-1);
	return win->_immed ? wrefresh(win) : OK;
}
