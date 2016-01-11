#include	"scrhdr.h"


/*
**	Draw a box around a window.
**
**	lc : the character and attributes used for the left side.
**	rc : right side.
**	tc : top side.
**	bc : bottom side.
**
**	Written by Kiem-Phong Vo
*/

#define	LC	0
#define	RC	1
#define TC	2
#define	BC	3
#define TL	4
#define TR	5
#define	BL	6
#define	BR	7

static char Crummy[8] =
	{	'|', '|', '-', '-',
		'+', '+', '+', '+'
	};
static char Beauty[8] =
	{	'x', 'x', 'q', 'q',
		'l', 'k', 'm', 'j'
	};

#if __STD_C
int wborder(WINDOW* win,
	chtype lc, chtype rc, chtype tc, chtype bc,
	chtype tl, chtype tr, chtype bl, chtype br)
#else
int wborder(win,lc,rc,tc,bc,tl,tr,bl,br)
WINDOW*	win;
chtype	lc, rc, tc, bc, tl, tr, bl, br;
#endif
{
	reg int		y, x, endx;
	reg chtype	*wcp, wc;
	chtype		ch[8];

	/* canonicalize border characters */
	ch[LC] = lc&A_CHARTEXT;
	ch[RC] = rc&A_CHARTEXT;
	ch[TC] = tc&A_CHARTEXT;
	ch[BC] = bc&A_CHARTEXT;
	ch[TL] = tl&A_CHARTEXT;
	ch[TR] = tr&A_CHARTEXT;
	ch[BL] = bl&A_CHARTEXT;
	ch[BR] = br&A_CHARTEXT;
	for(x = 0; x < 8; ++x)
	{
#if _MULTIBYTE	/* switch byte order for internal representation */
		if(ISMBYTE(ch[x]))
			ch[x] = (RBYTE(ch[x])<<8) | LBYTE(ch[x]);
#endif
		/* border char can't be multi-column */
		if(ch[x] == 0 || iscntrl(ch[x]) ||
#if _MULTIBYTE
		   scrwidth[TYPE(RBYTE(ch[x]))] > 1 ||
#endif
		   ch[x] == (chtype)Crummy[x])
			ch[x] = _acs_map[Beauty[x]];
	}

	lc = WCHAR(win,ch[LC]) | ATTR(lc) | ATTR(ch[LC]);
	rc = WCHAR(win,ch[RC]) | ATTR(rc) | ATTR(ch[RC]);
	tc = WCHAR(win,ch[TC]) | ATTR(tc) | ATTR(ch[TC]);
	bc = WCHAR(win,ch[BC]) | ATTR(bc) | ATTR(ch[BC]);
	tl = WCHAR(win,ch[TL]) | ATTR(tl) | ATTR(ch[TL]);
	tr = WCHAR(win,ch[TR]) | ATTR(tr) | ATTR(ch[TR]);
	bl = WCHAR(win,ch[BL]) | ATTR(bl) | ATTR(ch[BL]);
	br = WCHAR(win,ch[BR]) | ATTR(br) | ATTR(ch[BR]);

	/* left and right edges */
	endx = win->_maxx-1;
	for(y = win->_maxy-2; y >= 1; --y)
	{
#if _MULTIBYTE
		/* we can only overwrite single-width character */
		wc = win->_y[y][0];
		if(!ISCBIT(wc) && scrwidth[TYPE(RBYTE(wc))] == 1)
#endif
			win->_y[y][0] = lc;

#if _MULTIBYTE
		wc = win->_y[y][endx];
		if(!ISCBIT(wc) && scrwidth[TYPE(RBYTE(wc))] == 1)
#endif
			win->_y[y][endx] = rc;
	}

	/* do top and bottom edges */
	for(y = 0; y < win->_maxy; y += win->_maxy > 1 ? win->_maxy-1 : 1)
	{
		wcp = win->_y[y];
		x = 0;
		endx = win->_maxx-1;
#if _MULTIBYTE
		for(; x <= endx; ++x)
			if(!ISCBIT(wcp[x]))
				break;
		for(; endx >= x; --endx)
		{	if(!ISCBIT(wcp[endx]))
			{	reg int	m;

				wc = RBYTE(wcp[endx]);
				if((m = endx+scrwidth[TYPE(wc)]) > win->_maxx)
					endx -= 1;
				else	endx  = m-1;
				break;
			}
		}
#endif
		/* do corners */
		if(x == 0)
			wcp[x++] = y == 0 ? tl : bl;
		if(endx == win->_maxx-1)
			wcp[endx--] = y == 0 ? tr : br;

		/* do the rest */
		wc = y == 0 ? tc : bc;
		for(; x <= endx; ++x)
			wcp[x] = wc;
	}

	return touchwin(win);
}
