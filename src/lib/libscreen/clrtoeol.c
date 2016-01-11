#include	"scrhdr.h"


/*
**	Clear to end-of-line
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wclrtoeol(WINDOW* win)
#else
int wclrtoeol(win)
WINDOW*	win;
#endif
{
	reg chtype	*wp, *ep, *wcp, wc;
	reg int		x, y, endx, m;
	reg uchar	*mkp;

	/* update the relevant window image */
	x = win->_curx;
	y = win->_cury;
	wcp = win->_y[y];
	wp = wcp + x;
	ep = wcp + win->_maxx-1;

#if _MULTIBYTE
	if(win != curscr)
	{	win->_nbyte = -1;
		if(_scrmax > 1)
		{	if(ISMBYTE(*wp))
			{	win->_insmode = TRUE;
				if(_mbvalid(win) == ERR)
					return ERR;
				x = win->_curx;
				wp = wcp + x;
			}

			if(ISMBYTE(*ep))
			{	reg chtype	*cp;
				for(cp = ep; cp >= wp; --cp)
					if(!ISCBIT(*cp))
						break;
				wc = RBYTE(*cp);
				if(cp+scrwidth[TYPE(wc)] > ep+1)
					ep = cp-1;
			}
		}
	}
#endif
		
	/* do the clear */
	wc = win->_bkgd;
	while(wp <= ep)
		*wp++ = wc;

	/* if curscr, reset blank structure */
	if(win == curscr)
	{	if(_begns[y] >= x)
			_begns[y] = win->_maxx;
		if(_endns[y] >= x)
			_endns[y] = _begns[y] > x ? -1 : x-1;
		if(x == 0)
			_curhash[y] = 0;

		if(_marks)
		{	mkp = _marks[y];
			endx = COLS/CHARSIZE + (COLS%CHARSIZE ? 1 : 0);
			for(m = x/CHARSIZE + 1; m < endx; ++m)
				mkp[m] = 0;
			mkp += x/CHARSIZE;
			if((m = x%CHARSIZE) == 0)
				*mkp = 0;
			else for(; m < CHARSIZE; ++m)
				*mkp &= ~(1 << m);
		}

		return OK;
	}

	/* regular window, update the change structure */
	if(win->_firstch[y] > x)
		win->_firstch[y] = x;
	win->_lastch[y] = ep-wcp;

	/* sync with ancestors structures */
	if(win->_sync && win->_parent)
		wsyncup(win);

	return win->_immed ? wrefresh(win) : OK;
}
