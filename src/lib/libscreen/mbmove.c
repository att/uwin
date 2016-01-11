#include	"scrhdr.h"

#if _MULTIBYTE


/*
**	Move (cury,curx) of win to (y,x).
**	It is guaranteed that the cursor is left at the start
**	of a whole character nearest to (y,x).
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wmbmove(WINDOW* win, int y, int x)
#else
int wmbmove(win,y,x)
WINDOW	*win;
int	y, x;
#endif
{
	reg chtype	*wcp, *wp, *ep;

	if(y < 0 || x < 0 || y >= win->_maxy || x >= win->_maxx)
		return ERR;

	if(_scrmax > 1)
	{
		wcp = win->_y[y];
		wp = wcp + x;
		ep = wcp+win->_maxx;

		/* make wp points to the start of a character */
		if(ISCBIT(*wp))
		{
			for(; wp >= wcp; --wp)
				if(!ISCBIT(*wp))
					break;
			if(wp < wcp)
			{
				wp = wcp+x+1;
				for(; wp < ep; ++wp)
					if(!ISCBIT(*wp))
						break;
			}
		}

		/* make sure that the character is whole */
		if(wp + scrwidth[TYPE(*wp)] > ep)
			return ERR;

		/* the new x position */
		x = wp-wcp;
	}

	if(y != win->_cury || x != win->_curx)
	{
		win->_nbyte = -1;
		win->_cury = y;
		win->_curx = x;
	}

	return win->_immed ? wrefresh(win) : OK;
}

#else
void _mbunused(){}
#endif
