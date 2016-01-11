#include	"scrhdr.h"


/*
**	Like wrefresh() but does not output.
**	The change structure of _virtscr is guaranteed by this
**	routine to contain whole characters in case of multi-byte chars.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wnoutrefresh(WINDOW* win)
#else
int wnoutrefresh(win)
WINDOW	*win;
#endif
{
	reg short	*bch, *ech, *sbch, *sech;
	reg chtype	**wcp, **scp, *wc, *sc;
	reg chtype	*hash;
	int		y, x, xorg, yorg, scrli, scrco,
			sminy, smaxy, boty, minx, maxx, lo, hi;

	if(win->_parent)
		wsyncdown(win);

	scrli = curscr->_maxy;
	scrco = curscr->_maxx;
	sminy = curscreen->_yoffset;
	smaxy = sminy+LINES;

	yorg = win->_begy+win->_yoffset;
	xorg = win->_begx;

	/* save flags, cursor positions */
	if(!win->_leeve &&
	   (y = win->_cury+yorg) >= 0 && y < scrli &&
	   (x = win->_curx+xorg) >= 0 && x < scrco)
	{
		_virtscr->_cury = y;
		_virtscr->_curx = x;
	}
	if(win->_idln)
		_virtscr->_idln = TRUE;
	if(win->_clear)
		_virtscr->_clear = TRUE;
	win->_clear = FALSE;

	/* region to update */
	boty = win->_maxy+yorg;
	if(yorg >= sminy && yorg < smaxy && boty >= smaxy)
		boty = smaxy;
	else if(boty > scrli)
		boty = scrli;
	boty -= yorg;

	minx = 0;
	if((maxx = win->_maxx+xorg) > scrco)
		maxx = scrco;
	maxx -= xorg+1;

	/* update structure */
	bch = win->_firstch;
	ech = win->_lastch;
	wcp = win->_y;

	hash = _virthash + yorg;
	sbch = _virtscr->_firstch + yorg;
	sech = _virtscr->_lastch + yorg;
	scp  = _virtscr->_y + yorg;

	/* first time around, set proper top/bottom changed lines */
	if(curscr->_sync)
		_virttop = scrli, _virtbot = -1;

	/* update each line */
	for(y = 0; y < boty; ++y, ++hash, ++bch,++ech, ++sbch,++sech, ++wcp,++scp)
	{
		if(*bch == _INFINITY)
			continue;

		lo = (*bch == _REDRAW || *bch < minx) ? minx : *bch;
		hi = (*bch == _REDRAW || *ech > maxx) ? maxx : *ech;

		wc = *wcp;
		sc = *scp;

#if _MULTIBYTE
		/* adjust lo and hi so they contain whole characters */
		if(_scrmax > 1)
		{
			if(ISCBIT(wc[lo]))
			{
				for(x = lo-1; x >= minx; --x)
					if(!ISCBIT(wc[x]))
						break;
				if(x < minx)
				{
					for(x = lo+1; x <= maxx; ++x)
						if(!ISCBIT(wc[x]))
							break;
					if(x > maxx)
						goto nextline;
				}
				lo = x;
			}
			if(ISMBYTE(wc[hi]))
			{
				reg int		w;
				reg chtype	rb;

				for(x = hi; x >= lo; --x)
					if(!ISCBIT(wc[x]))
						break;
				rb = RBYTE(wc[x]);
				w = scrwidth[TYPE(rb)];
				hi = (x+w) <= maxx+1 ? x+w-1 : x-1;
			}
		}
#endif

		/* nothing to do */
		if(hi < lo)
			goto nextline;

#if _MULTIBYTE
		/* clear partial multi-chars about to be overwritten */ 
		if(_scrmax > 1)
		{
			if(ISMBYTE(sc[lo+xorg]))
				_mbclrch(_virtscr,y+yorg,lo+xorg);
			if(ISMBYTE(sc[hi+xorg]))
				_mbclrch(_virtscr,y+yorg,hi+xorg);
		}
#endif

		/* update the change structure */
		if(*bch == _REDRAW || *sbch == _REDRAW)
			*sbch =  _REDRAW;
		else
		{
			if(*sbch > lo+xorg)
				*sbch = lo+xorg;
			if(*sech < hi+xorg)
				*sech = hi+xorg;
		}
		if(y+yorg < _virttop)
			_virttop = y+yorg;
		if(y+yorg > _virtbot)
			_virtbot = y+yorg;

		/* update the image */
		memcpy(sc+lo+xorg,wc+lo,((hi-lo)+1)*sizeof(chtype));

		/* the hash value of the line */
		*hash = _NOHASH;

	nextline:
		*bch = _INFINITY, *ech = -1;
	}

	return OK;
}
