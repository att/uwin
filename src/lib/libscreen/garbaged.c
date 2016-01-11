#include	"scrhdr.h"

/*
**	Make sure that a number of lines in the window image
**	will be completely redrawn at the next refresh.
**	This is a more refined version of clearok
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int garbagedlines(WINDOW* win, int by, int nl)
#else
int garbagedlines(win,by,nl)
WINDOW	*win;
int	by, nl;
#endif
{
	reg short	*bch, *ech, *lch, ey;

	if(by < 0)
		by = 0;
	if((ey = by+nl) > win->_maxy)
		ey = win->_maxy;
	if(by >= ey)
		return ERR;

	bch = win->_firstch+by;
	ech = win->_firstch+ey;
	lch = win->_lastch +by;
	for(; bch < ech; ++bch, ++lch)
		*bch = _REDRAW, *lch = _INFINITY;

	return win->_immed ? wrefresh(win) : OK;
}
