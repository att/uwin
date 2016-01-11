#include	"scrhdr.h"

/*
**	Sync the changes in a window with its ancestors.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wsyncup(WINDOW* win)
#else
int wsyncup(win)
WINDOW	*win;
#endif
{
	reg short	*wbch, *wech, *pbch, *pech;
	reg int		wy, px, py, endy, bch, ech;
	reg WINDOW	*par;

	for(par = win->_parent; par ; win = par, par = par->_parent)
	{
		py = win->_pary;
		px = win->_parx;
		endy = win->_maxy;

		wbch = win->_firstch;
		wech = win->_lastch;
		pbch = par->_firstch+py;
		pech = par->_lastch+py;

		/* check each line */
		for(wy = 0; wy < endy; ++wy, ++wbch,++wech, ++pbch,++pech)
		{	if(*wbch == _INFINITY)
				continue;
			bch = px + *wbch;
			ech = px + *wech;
			if(*pbch > bch)
				*pbch = bch;
			if(*pech < ech)
				*pech = ech;
		}
	}
	return OK;
}


/*
**	Make the changes in ancestors visible in win.
*/
#if __STD_C
int wsyncdown(WINDOW* win)
#else
int wsyncdown(win)
WINDOW	*win;
#endif
{
	reg short	*wbch, *wech, *pbch, *pech;
	reg int		wy, px, py, endy, endx, bch, ech;
	reg WINDOW	*par;

	py = win->_pary;
	px = win->_parx;
	endy = win->_maxy;
	endx = win->_maxx-1;

	for(par = win->_parent; par ; par = par->_parent)
	{
		wbch = win->_firstch;
		wech = win->_lastch;
		pbch = par->_firstch+py;
		pech = par->_lastch+py;

		for(wy = 0; wy < endy; ++wy, ++wbch,++wech, ++pbch,++pech)
		{	if(*pbch == _INFINITY)
				continue;
			if((bch = *pbch - px) < 0)
				bch = 0;
			if((ech = *pech - px) > endx)
				ech = endx;
			if(bch > endx || ech < 0)
				continue;

			if(*wbch > bch)
				*wbch = bch;
			if(*wech < ech)
				*wech = ech;
		}

		py += par->_pary;
		px += par->_parx;
	}
	return OK;
}
