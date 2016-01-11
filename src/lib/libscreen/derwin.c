#include	"scrhdr.h"


/*
**	Make a  derived window of an existing window. The two windows
**	share the same character image.
**		orig:	the original window
**		nl, nc:	numbers of lines and columns
**		by, bx:	coordinates for upper-left corner of the derived
**			window in the coord system of the parent window.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
WINDOW* derwin(WINDOW* orig, int nl, int nc, int by, int bx)
#else
WINDOW* derwin(orig,nl,nc,by,bx)
WINDOW*	orig;
int	nl, nc, by, bx;
#endif
{
	reg int		y;
	reg WINDOW	*win, *par;

	/* make sure window fits inside the original one */
	if(by < 0 || bx < 0 || nc < 0 || nl < 0 ||
	   (by+nl) > orig->_maxy || (bx+nc) > orig->_maxx)
		return NIL(WINDOW*);
	if(nc == 0)
		nc = orig->_maxx - bx;
	if(nl == 0)
		nl = orig->_maxy - by;

	/* create the window skeleton */
	if(!(win = _makenew(nl,nc,by+orig->_begy,bx+orig->_begx)) )
		return NIL(WINDOW*);

	/* inheritance */
	win->_parx = bx;
	win->_pary = by;
	win->_bkgd = orig->_bkgd;
	win->_attrs = orig->_attrs;
	for (y = 0; y < nl; y++, by++)
		win->_y[y] = orig->_y[by] + bx;

	win->_yoffset = orig->_yoffset;
	win->_clear = FULLWIN(win);

	/* update descendant number of ancestors */
	win->_parent = orig;
	for(par = win->_parent; par != NIL(WINDOW*); par = par->_parent)
		par->_ndescs += 1;

	return win;
}
