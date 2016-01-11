#include	"scrhdr.h"

/*
**	Move a derived window inside its parent window.
**	This routine does not change the screen relative
**	parameters begx and begy. Thus, it can be used to
**	display different parts of the parent window at
**	the same screen coordinate.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int mvderwin(WINDOW* win, int pary, int parx)
#else
int mvderwin(win,pary,parx)
WINDOW	*win;
int	pary, parx;
#endif
{
	reg int		y, maxy, maxx;
	reg WINDOW	*par;
	chtype		obkgd, **wc, **pc;
	short		*begch, *endch;

	if(!(par = win->_parent) )
		return ERR;
	if(pary == win->_pary && parx == win->_parx)
		return OK;

	maxy = win->_maxy-1;
	maxx = win->_maxx-1;
	if((parx+maxx) >= par->_maxx || (pary+maxy) >= par->_maxy)
		return ERR;

	/* save all old changes */
	wsyncup(win);

	/* rearrange pointers */
	win->_parx = parx;
	win->_pary = pary;
	wc = win->_y;
	pc = par->_y + pary;
	begch = win->_firstch;
	endch = win->_lastch;
	for(y = 0; y <= maxy; ++y, ++wc, ++pc, ++begch, ++endch)
	{
		*wc = *pc + parx;
		*begch = 0; *endch = maxx;
	}

	/* change background to our own */
	obkgd = win->_bkgd; win->_bkgd = par->_bkgd;
	return wbkgd(win,obkgd);
}
