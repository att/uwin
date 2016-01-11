#include	"scrhdr.h"

#if _MULTIBYTE


/*
**	Get the (y,x) character of a window and
**	return it in a 0-terminated string.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
char *wmbinch(WINDOW* win, int y, int x)
#else
char *wmbinch(win,y,x)
WINDOW	*win;
int	y, x;
#endif
{
	reg int		k, savx, savy;
	reg chtype	*wp, *ep, wc;
	static uchar	rs[CSMAX+1];

	k = 0;
	savx = win->_curx;
	savy = win->_cury;

	if(wmbmove(win,y,x) == ERR)
		goto done;
	wp = win->_y[win->_cury] + win->_curx;
	wc = RBYTE(*wp);
	ep = wp + scrwidth[TYPE(wc)];

	for(; wp < ep; ++wp)
	{
		if((wc = RBYTE(*wp)) == MBIT)
			break;
		rs[k++] = (uchar)wc;
		if((wc = LBYTE(*wp)) == MBIT)
			break;
		rs[k++] = (uchar)wc;
	}
done :
	win->_curx = savx;
	win->_cury = savy;
	rs[k] = '\0';
	return (char*)rs;
}

#else
void _mbunused(){}
#endif
