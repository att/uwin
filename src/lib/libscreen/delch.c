#include	"scrhdr.h"


/*
**	Delete the character at curx, cury.
**	Even if (cury,curx) is in the middle of a multi-column character
**	the entire character is still deleted.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wdelch(WINDOW* win)
#else
int wdelch(win)
WINDOW	*win;
#endif
{
	reg chtype	*wp, *ep, *cp, *wcp, wc;
	reg int		x, y, s;

	y = win->_cury;
	x = win->_curx;
	wcp = win->_y[y];
	wp = wcp + x;
	ep = wcp + win->_maxx - 1;
	s = 1;	/* amount to shift */

#if _MULTIBYTE
	win->_nbyte = -1;
	if(_scrmax > 1)
	{	if(ISMBYTE(*wp))
		{	win->_insmode = TRUE;
			if(_mbvalid(win) == ERR)
				return ERR;
			x = win->_curx;
			wp = wcp+x;
		}
	
		/* only shift left up to a complete character */
		if(ISMBYTE(*ep))
		{	for(cp = ep; cp >= wp; --cp)
				if(!ISCBIT(*cp))
					break;
			wc = *cp;
			if(cp+scrwidth[TYPE(wc)] > ep+1)
				ep = cp-1;
		}
	
		if(ISMBYTE(*wp))
		{	wc = RBYTE(*wp);
			s  = scrwidth[TYPE(wc)];
		}
	
		/* end of shifted interval */
		ep -= s-1;
	}
#endif

	/* perform the shift */
	for(; wp < ep; ++wp)
		*wp = *(wp+s);

	/* fill in the rest of the line with background chars */
	wc = win->_bkgd;
	for(cp = ep + s-1; cp >= wp; --cp)
		*cp = wc;

	/* update change records */
	if(win->_firstch[y] > x)
		win->_firstch[y] = x;
	win->_lastch[y] = win->_maxx-1;

	if(win->_sync && win->_parent)
		wsyncup(win);

	return win->_immed ? wrefresh(win) : OK;
}
