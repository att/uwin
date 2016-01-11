#include	"scrhdr.h"

/*
**	Scroll the given window up/down n lines.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wscrl(WINDOW* win, int n)
#else
int wscrl(win,n)
WINDOW	*win;
#endif
{
	reg int		curx, cury, savimmed, savsync;

	if(!(win->_scroll))
		return ERR;

	savimmed = win->_immed;
	savsync = win->_sync;
	win->_immed = win->_sync = FALSE;

	curx = win->_curx; cury = win->_cury;

	win->_curx = 0;
	if(cury >= win->_tmarg && cury <= win->_bmarg)
		win->_cury = win->_tmarg;
	else	win->_cury = 0;

	winsdelln(win,-n);
	win->_curx = curx; win->_cury = cury;

	win->_immed = savimmed;
	win->_sync = savsync;

	if(win->_sync)
		wsyncup(win);

	return win->_immed ? wrefresh(win) : OK;
}
