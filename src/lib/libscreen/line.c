#include	"scrhdr.h"

/*
**	Draw a vertical line with given characters
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wline(reg WINDOW* win, reg chtype c, reg int n, int vert)
#else
int wline(win,c,n,vert)
reg WINDOW*	win;
reg chtype	c;
reg int		n;
int		vert;
#endif
{
	reg chtype	**img;
	reg short	*firstch, *lastch;
	reg int		y, x;
	reg chtype	a;

	/* canonicalize c, a */
	if(c == 0)
		a = 0;
	else
	{	a = ATTR(c);
		c &= A_CHARTEXT;
#if _MULTIBYTE
		if(ISMBYTE(c))
			c = (RBYTE(c)<<8) | LBYTE(c);
#endif
	}

	/* set to default values as appropriate */
	if(c == 0 ||
#if _MULTIBYTE
	   scrwidth[TYPE(RBYTE(c))] > 1 ||
#endif
	   iscntrl(c))
		c = vert ? ACS_VLINE : ACS_HLINE;

	c = WCHAR(win,c)|a;
	y = win->_cury;
	x = win->_curx;
	img = win->_y;
	firstch = win->_firstch;
	lastch = win->_lastch;

	if(vert && n > (win->_maxy-y))
		n = win->_maxy-y;
	else if(!vert && n > (win->_maxx-x))
		n = win->_maxx-x;
	while(n-- > 0)
	{
#if _MULTIBYTE	/* can only draw on single-column characters */
		chtype wc = img[y][x];
		if(!ISCBIT(wc) && scrwidth[TYPE(RBYTE(wc))] == 1)
#endif
		{	img[y][x] = c;
			if(x < firstch[y])
				firstch[y] = x;
			if(x > lastch[y])
				lastch[y] = x;
		}
		if(vert)
			y += 1;
		else	x += 1;
	}

	if(win->_sync)
		wsyncup(win);
	return win->_immed ? wrefresh(win) : OK;
}
