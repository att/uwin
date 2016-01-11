#include	"scrhdr.h"

/*
**	Shift right an interval of characters
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _insshift(WINDOW* win, int len)
#else
int _insshift(win,len)
WINDOW	*win;
int	len;
#endif
{
	reg int		x, y, maxx, mv;
	reg chtype	*wcp, *wp, *ep;

	y = win->_cury;
	x = win->_curx;
	maxx = win->_maxx;
	wcp  = win->_y[y];

#if _MULTIBYTE
	/**/ ASSERT(!ISCBIT(wcp[x]));

	/* shift up to a whole character */
	if(_scrmax > 1)
	{
		wp = wcp+maxx-1;
		if(ISMBYTE(*wp))
		{
			reg chtype	rb;

			for(; wp >= wcp; --wp)
				if(!ISCBIT(*wp))
					break;
			if(wp < wcp)
				return ERR;
			rb = RBYTE(*wp);
			if((wp+scrwidth[TYPE(rb)]) > (wcp+maxx))
				maxx = wp - wcp;
		}
	}
#endif

	/* see if any data need to move */
	if((mv = maxx - (x+len)) <= 0)
		return OK;

#if _MULTIBYTE
	/* the end of the moved interval must be whole */
	if(ISCBIT(wcp[x+mv]))
		_mbclrch(win,y,x+mv-1);
#endif

	/* move data */
	ep = wcp+x+len;
	for(wp = wcp+maxx-1; wp >= ep; --wp)
		*wp = *(wp-len);

#if _MULTIBYTE
	/* clear a possible partial multibyte character */
	if(ISMBYTE(*wp))
		for(ep = wp; ep >= wcp; --ep)
		{	chtype	ch;
			ch = ISCBIT(*ep);
			*ep = win->_bkgd;
			if(!ch)
				break;
		}
#endif

	/* update the change structure */
	if(x < win->_firstch[y])
		win->_firstch[y] = x;
	win->_lastch[y] = maxx-1;

	return OK;
}
