#include	"scrhdr.h"


/*
**	Insert a character at (curx,cury).
**	All characters starting at curx are shifted right.
**	It is guaranteed that only whole characters are moved.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winsch(WINDOW*win, chtype c)
#else
int winsch(win,c)
WINDOW	*win;
chtype	c;
#endif
{
	reg int		len, maxx, x, y, rv;
	reg chtype	*wcp, a;

	rv = OK;
	a = ATTR(c);
	c &= A_CHARTEXT;

#if _MULTIBYTE
	win->_insmode = TRUE;
	if(_scrmax > 1 && (rv = _mbvalid(win)) == ERR)
		goto done;

	/* take care of multi-byte characters */
	if(_mbtrue && (ISMBIT(c) || win->_index < win->_nbyte))
	{
		rv = _mbaddch(win,a,c);
		goto done;
	}
	win->_nbyte = -1;
#endif

	maxx = win->_maxx;
	y = win->_cury;
	x = win->_curx;

	if(c == '\r')
	{
		win->_curx = 0;
		goto done;
	}

	if(c == '\b')
	{
		if(x > 0)
			win->_curx -= 1;
		goto done;
	}

	/* with \n, in contrast to waddch, we don't clear-to-eol */
	if(c == '\n')
	{
		if(y >= (win->_maxy-1) || y == win->_bmarg)
		{
			bool savs, savi;

			savs = win->_sync;
			savi = win->_immed;
			win->_sync = win->_immed = FALSE;
			rv = wscrl(win,1);
			win->_sync = savs;
			win->_immed = savi;
		}
		else	win->_cury += 1;

		win->_curx = 0;
		goto done;
	}
	/* with tabs or control characters, we have to do more */
	if(c == '\t')
	{
		len = (TABSIZ - (x % TABSIZ));
		if((x + len) >= maxx)
			len = maxx - x;
		c = ' ';
	}
	else
	{
		if(iscntrl(c))
		{
			if(x >= maxx-1)
			{
				rv = ERR;
				goto done;
			}
			len = 2;
		}
		else	len = 1;
	}

	/* do the shift */
	if((rv = _insshift(win,len)) == ERR)
		goto done;
	
	/* inserting a control character */
	wcp = win->_y[y]+x;
	if(iscntrl(c))
	{
		*wcp++ = '^' | win->_attrs | a;
		*wcp = UNCTRL(c) | win->_attrs | a;
	}
	/* inserting normal characters */
	else
	{
		c = WCHAR(win,c) | a;
		for(; len > 0; --len)
			*wcp++ = c;
	}

done :
	if(win->_sync && win->_parent)
		wsyncup(win);

	return (rv == OK && win->_immed) ? wrefresh(win) : rv;
}
