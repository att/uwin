#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Clear the space occupied by a multicolumn character
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _mbclrch(WINDOW* win, int y, int x)
#else
int _mbclrch(win,y,x)
WINDOW	*win;
int	y, x;
#endif
{
	reg chtype	*wcp, *ep, *wp, wc;

	/**/ ASSERT(_scrmax > 1);

	wcp = win->_y[y];
	wp = wcp + x;

	/* compute the bounds for the character */
	if(ISCBIT((unsigned long int)*wp))
	{
		for(; wp >= wcp; --wp)
			if(!ISCBIT((unsigned long int)*wp))
				break;
		if(wp < wcp)
			return ERR;
	}
	wc = RBYTE(*wp);
	ep = wp + scrwidth[TYPE(wc)];
	if(ep > wcp+win->_maxx)
		return ERR;

	/* update the change structure */
	if((x = wp-wcp) < win->_firstch[y])
		win->_firstch[y] = x;
	if((x = (ep-wcp)-1) > win->_lastch[y])
		win->_lastch[y] = x;

	/* clear the character */
	for(; wp < ep; ++wp)
		*wp = win->_bkgd;
	return OK;
}



/*
**	Make sure the window cursor point to a valid place.
**	If win->_insmode or isedge, the cursor has to
**	point to the start of a whole character; otherwise, the
**	cursor has to point to a part of a whole character.
*/
#if __STD_C
int _mbvalid(WINDOW* win)
#else
int _mbvalid(win)
WINDOW	*win;
#endif
{
	reg chtype	*wp, *wcp, *ecp, wc;
	reg int		x;
	reg bool	isedge;

	/**/ ASSERT(_scrmax > 1);

	x = win->_curx;
	wcp = win->_y[win->_cury];
	wp = wcp+x;
	if(!ISMBYTE(*wp))
		return OK;

	ecp = wcp+win->_maxx;
	isedge = FALSE;

	/* make wp points to the start column of a mb-character */
	if(ISCBIT(*wp))
	{
		for(; wp >= wcp; --wp)
			if(!ISCBIT(*wp))
				break;
		if(wp < wcp)
		{
			for(wp = wcp+x+1; wp < ecp; ++wp)
				if(!ISCBIT(*wp))
					break;
			if(wp >= ecp)
				return ERR;
			isedge = TRUE;
		}
	}

	/* make sure that wp points to a whole character */
	wc = RBYTE(*wp);
	if(wp+scrwidth[TYPE(wc)] > ecp)
	{
		for(wp -= 1; wp >= wcp; --wp)
			if(!ISCBIT(*wp))
				break;
		if(wp < wcp)
			return ERR;
		isedge = TRUE;
	}

	if(isedge || win->_insmode)
		win->_curx = wp-wcp;
	return OK;
}



/*
**	Add/insert multi-byte characters
*/
#if __STD_C
int _mbaddch(WINDOW* win, chtype a, chtype c)
#else
int _mbaddch(win,a,c)
WINDOW	*win;
chtype	a, c;
#endif
{
	reg int		n, x, y, nc, m, len, nbyte, ty;
	chtype		*wcp, wc;
	uchar		*wch, rc[2];

	/**/ ASSERT(_mbtrue);

	/* decode the character into a sequence of bytes */
	nc = 0;
	if(LBYTE(c)&0177)
		rc[nc++] = (uchar)LBYTE(c);
	if(RBYTE(c)&0177)
		rc[nc++] = (uchar)RBYTE(c);

	a |= (win->_attrs|win->_bkgd)&A_ATTRIBUTES;

	/* add the sequence to the image */
	for(n = 0; n < nc; ++n)
	{
		wc = RBYTE(rc[n]);
		wch = win->_waitc;

		/* first byte of a multi-byte character */
		if(win->_index >= win->_nbyte)
		{
			wch[0] = (uchar)wc;
			ty = TYPE(wc);
			win->_nbyte = cswidth[ty] + (ty == 0 ? 0 : 1);
			win->_index = 1;
		}
		/* non-first byte */
		else
		{
			wch[win->_index] = (uchar)wc;
			win->_index += 1;
		}

		/* if character is not ready to process */
		if(win->_index < win->_nbyte)
			continue;

		/* begin processing the character */
		nbyte = win->_nbyte;
		win->_nbyte = -1;
		wc = RBYTE(wch[0]);
		len = scrwidth[TYPE(wc)];

		/* window too small or char cannot be stored */
		if(len > win->_maxx || 2*len < nbyte)
			return ERR;

		/* if the character won't fit into the line */
		if((win->_curx+len) > win->_maxx &&
		   (win->_insmode || waddch(win,'\n') == ERR))
			return ERR;

		y = win->_cury;
		x = win->_curx;
		wcp = win->_y[y]+x;

		if(win->_insmode)
		{	/* perform the right shift */
			if(_insshift(win,len) == ERR)
				return ERR;
		}
		else if(_scrmax > 1)
		{	/* clear any multi-byte char about to be overwritten */
			for(m = 0; m < len; ++m)
				if(ISMBYTE(wcp[m]) && _mbclrch(win,y,x+m) == ERR)
					break;
			if(m < len)
				continue;
		}

		/* pack two bytes at a time */
		for(m = nbyte/2; m > 0; m -= 1, wch += 2)
			*wcp++ = (RBYTE(wch[1]) << 8) | RBYTE(wch[0]) | a | CBIT;

		/* do the remaining byte if any */
		if((nbyte%2) != 0)
			*wcp++ = (MBIT << 8) | RBYTE(wch[0]) | CBIT | a;

		/* fill-in for remaining display columns */
		for(m = (nbyte/2)+(nbyte%2); m < len; ++m)
			*wcp++ = (CBIT | (MBIT << 8) | MBIT) | a;

		/* the first column has Continue BIT off */
		win->_y[y][x] &= ~CBIT;

		if(win->_insmode == FALSE)
		{
			if(x < win->_firstch[y])
				win->_firstch[y] = x;
			if((x += len-1) >= win->_maxx)
				x = win->_maxx-1;
			if(x > win->_lastch[y])
				win->_lastch[y] = x;

			if((win->_curx += len) >= win->_maxx)
			{
				if(y >= (win->_maxy-1) || y == win->_bmarg)
				{
					win->_curx = win->_maxx-1;
					if(wscrl(win,1) == ERR)
						return ERR;
				}
				else
				{
					win->_cury += 1;
					win->_curx = 0;
				}
			}
		}
	}

	return OK;
}

#else
void _mbunused(){}
#endif
