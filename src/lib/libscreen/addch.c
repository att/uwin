#include	"scrhdr.h"

/*
**	Add the given character to the given window at (cury,curx).
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int waddch(WINDOW* win, chtype c)
#else
int waddch(win,c)
WINDOW*	win;
chtype	c;
#endif
{
	reg int		x, y, nx,
			savimmed, savsync, rv;
	reg chtype	*wcp, a;

	/* turn off these flags for any possible recursive calls */
	savimmed = win->_immed;
	savsync  = win->_sync;
	win->_immed = win->_sync = FALSE;

	a = ATTR(c);
	c &= A_CHARTEXT;

#if _MULTIBYTE
	win->_insmode = FALSE;
	if(_scrmax > 1 && _mbvalid(win) == ERR)
		return ERR;

	if(_mbtrue && (ISMBIT(c) || win->_index < win->_nbyte))
	{	rv = _mbaddch(win,a,c);
		goto done;
	}
	win->_nbyte = -1;
#endif

	x = win->_curx;
	y = win->_cury;
	rv = OK;

	switch(c)
	{
	case '\n' :
		wclrtoeol(win);
		win->_curx = 0;
		goto downone;
	case '\r' :
		win->_curx = 0;
		break;
	case '\b' :
		if(x > 0)
			win->_curx -= 1;
		break;
	case '\t' :
		nx = x + (TABSIZ - (x % TABSIZ));
		if(nx > win->_maxx)
			nx = win->_maxx;
		for(; x < nx; ++x)
		{	if(waddch(win,BLNKCHAR|a) == ERR)
			{	rv = ERR;
				goto done;
			}
		}
		break;
	default :
		/* control characters */
		if(iscntrl(c))
		{	/* make sure '^' and 'x' go together */
			if(x == win->_maxx-1 && waddch(win,'\n') == ERR)
			{	rv = ERR;
				goto done;
			}

			if(waddch(win,'^'|a) == ERR || waddch(win,UNCTRL(c)|a) == ERR)
				rv = ERR;
			goto done;
		}

		/* location to write into */
		wcp = win->_y[y]+x;
#if _MULTIBYTE
		/* clear any partial multi-column character */
		if(_scrmax > 1 && ISMBYTE(*wcp) && (rv = _mbclrch(win,y,x)) == ERR)
			goto done;
#endif
		/* normal char, do background translation */
		c = WCHAR(win,c)|a;
		if(*wcp != c)
		{	*wcp = c;

			if(x < win->_firstch[y])
				win->_firstch[y] = x;
			if(x > win->_lastch[y])
				win->_lastch[y] = x;
		}

		if((win->_curx += 1) >= win->_maxx)
		{	win->_curx = win->_maxx-1;
downone:
			if(y >= (win->_maxy-1) || y == win->_bmarg)
			{	if(win->_scroll)
					wscrl(win,1);
				else
				{	rv = ERR;
					goto done;
				}
			}
			else
			{	win->_curx = 0;
				win->_cury += 1;
			}
		}
		break;
	}

done :
	win->_sync = savsync;
	win->_immed = savimmed;

	/* sync with ancestor structures */
	if(win->_sync && win->_parent)
		wsyncup(win);

	return (win->_immed && rv == OK) ? wrefresh(win) : rv;
}
