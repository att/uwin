#include	"scrhdr.h"

/*
**	Change the background character and attributes of a window.
**	The background character must be single column.
**
**	nbkgd :	new background char and attributes.
**		Since the background character must be single column,
**		if it is multi-byte it can have at most two bytes.
**		In such a case, it is assumed that the two bytes are
**		or-ed into a 16-bit quantity occupying the lower 16 bits
**		of nbkgd.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int wbkgd(WINDOW* win, chtype nbkgd)
#else
int wbkgd(win,nbkgd)
WINDOW*	win;
chtype	nbkgd;
#endif
{
	reg int		x, y, maxx;
	reg chtype	*wcp, obkgda, obkgdc, nbkgda, nbkgdc, c;
	reg short	*begch, *endch;

	if(nbkgd == win->_bkgd)
		return OK;

	obkgdc = CHAR(win->_bkgd);
	obkgda = ATTR(win->_bkgd);

	nbkgdc = CHAR(nbkgd);
	nbkgda = ATTR(nbkgd);

	/* the new background character must be single-column */
#if _MULTIBYTE
	/* switch byte order if necessary */
	if(ISMBYTE(nbkgdc))
		nbkgdc = (RBYTE(nbkgdc)<<8) | LBYTE(nbkgdc);
	c = RBYTE(nbkgdc);
	if(iscntrl(nbkgdc) || scrwidth[TYPE(c)] > 1)
		nbkgdc = obkgdc;
	nbkgd = nbkgdc|nbkgda;
#else
	if(iscntrl(nbkgdc))
		nbkgdc = obkgdc, nbkgd = nbkgdc|nbkgda;
#endif

	win->_bkgd = nbkgd;
	win->_attrs = ATTR((win->_attrs & ~obkgda) | nbkgda);

	maxx = win->_maxx-1;
	begch = win->_firstch;
	endch = win->_lastch;
	for(y = win->_maxy-1; y >= 0; --y, ++begch, ++endch)
	{
		for(x = maxx, wcp = win->_y[y]; x >= 0; --x, ++wcp)
		{
#if _MULTIBYTE
			chtype	cbit = wcp[0]&CBIT;
#endif
			if((c = CHAR(*wcp)) == obkgdc)
				c = nbkgdc;
			*wcp = c | ATTR((*wcp & ~obkgda) | nbkgda);
#if _MULTIBYTE
			*wcp |= cbit;
#endif
		}
		*begch = 0;
		*endch = maxx;
	}

	if(win->_sync && win->_parent)
		wsyncup(win);

	return win->_immed ? wrefresh(win) : OK;
}
