#include	"scrhdr.h"


/*
**	Insert to 'win' at most n 'whole characters' of a string
**	starting at (cury,curx). If n <= 0, insert the entire string.
**	\n, \t, \r, \b are treated in the same way as other control chars.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winsnstr(WINDOW* win, char* isp, int n)
#else
int winsnstr(win,isp,n)
WINDOW	*win;
char	*isp;
int	n;
#endif
{
	reg chtype	*wcp;
	reg int		y, x, i, endx, maxx, len, rv;
	reg uchar	*sp = (uchar*)isp;

#if _MULTIBYTE
	bool		savscrl, savsync, savimmed;

	/* only insert at the start of a character */
	win->_nbyte = -1;
	win->_insmode = TRUE;
	if(_scrmax > 1 && (rv = _mbvalid(win)) == ERR)
		goto done;
#endif

	/* for convenience */
	n = n < 0 ? _INFINITY : n;
	maxx = win->_maxx;
	x = win->_curx;
	y = win->_cury;
	wcp = win->_y[y]+x;
	rv = OK;

	/* compute total length of the insertion */
	endx = x;
	for(i = 0; sp[i] != '\0' && i < n; ++i)
	{
		len = iscntrl(sp[i]) ? 2 : 1;
#if _MULTIBYTE
		if(ISMBIT(sp[i]))
		{
			reg int		m, k, ty;
			reg chtype	c;

			/* see if the entire character is defined */
			c = RBYTE(sp[i]);
			ty = TYPE(c);
			m = i + cswidth[ty] - (ty == 0 ? 1 : 0);
			for(k = i+1; sp[k] != '\0' && k <= m; ++k)
				;
			if(k <= m)
				break;

			/* recompute # of columns used */
			len = scrwidth[ty];

			/* skip an appropriate number of bytes */
			i = m;
		}
#endif
		if((endx += len) > maxx)
		{
			endx -= len;
			break;
		}
	}

	/* number of bytes insertible */
	n = i;

	/* length of inserted string */
	len = endx - x;

	/* shift the appropriate image */
	if((rv = _insshift(win,len)) == ERR)
		goto done;

#if _MULTIBYTE
	/* act as if we are adding characters */
	win->_insmode = FALSE;
	savscrl = win->_scroll;
	savimmed = win->_immed;
	savsync = win->_sync;
	win->_scroll = win->_immed = win->_sync = FALSE;
#endif

	/* insert new data */
	for(; n > 0; --n, ++sp)
	{
#if _MULTIBYTE
		/* multi-byte characters */
		if(_mbtrue && (ISMBIT(*sp) || win->_index < win->_nbyte))
		{
			_mbaddch(win,A_NORMAL,RBYTE(*sp));
			wcp = win->_y[y] + win->_curx;
			continue;
		}

		if(_scrmax > 1 && ISMBIT(*wcp))
			_mbclrch(win,y,win->_curx);

		/* single byte character */
		win->_nbyte = -1;
		win->_curx += iscntrl(*sp) ? 2 : 1;
#endif
		if(iscntrl(*sp))
		{
			*wcp++ = CHAR('^') | win->_attrs;
			*wcp++ = CHAR(UNCTRL(*sp)) | win->_attrs;
		}
		else	*wcp++ = WCHAR(win,*sp);
	}
done:

#if _MULTIBYTE
	win->_curx = x;
	win->_scroll = savscrl;
	win->_sync = savsync;
	win->_immed = savimmed;
#endif

	if(win->_sync && win->_parent)
		wsyncup(win);
	return win->_immed ? wrefresh(win) : OK;
}
