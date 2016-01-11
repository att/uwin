#include	"scrhdr.h"



/*
**	Make a number of lines look like they have/have not been changed.
**	y: the start line
**	n: the number of lines affected
**	changed: 1 : changed
**		 0 : not changed
**		 -1: called internally.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wtouchln(WINDOW* win, int y, int n, int changed)
#else
int wtouchln(win,y,n,changed)
WINDOW	*win;
int	y, n, changed;
#endif
{
	reg int		maxy, maxx;
	reg short	*bch, *ech;

	maxy = win->_maxy;
	maxx = win->_maxx-1;
	bch = win->_firstch+y;
	ech = win->_lastch+y;

	for(; y < maxy && n > 0; ++y, --n, ++bch,++ech)
	{
		if(changed == 0)
			*bch = _INFINITY, *ech = -1;
		else if(*bch != _REDRAW || changed == -1)
			*bch = 0, *ech = maxx;
	}

	if(changed == 1 && win->_sync && win->_parent)
		wsyncup(win);

	return (changed == 1 && win->_immed) ? wrefresh(win) : OK;
}
