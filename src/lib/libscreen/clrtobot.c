#include	"scrhdr.h"


/*
**	Clear from current line to end of scrolling region
**	or end of window.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wclrtobot(WINDOW* win)
#else
int wclrtobot(win)
WINDOW*	win;
#endif
{
	reg int	endy, cury, curx, savimmed, savsync;

	cury = win->_cury;
	curx = win->_curx;

	if(win != curscr)
	{	savimmed = win->_immed;
		savsync = win->_sync;
		win->_immed = win->_sync = FALSE;
	}

	/* set region to be clear */
	win->_curx = 0;
	if(cury >= win->_tmarg && cury <= win->_bmarg)
		endy = win->_bmarg+1;
	else	endy = win->_maxy;

	for(; win->_cury < endy; win->_cury += 1)
		wclrtoeol(win);

	win->_cury = cury;
	win->_curx = curx;

	if(win == curscr)
		return OK;

	/* not curscr */
	win->_immed = savimmed;
	win->_sync = savsync;

	if(win->_sync && win->_parent)
		wsyncup(win);
	
	return win->_immed ? wrefresh(win) : OK;
}
