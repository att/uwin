#include	"scrhdr.h"

/*
**	Sync the cursor position up to all ancestor windows.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wcursyncup(WINDOW* win)
#else
int wcursyncup(win)
WINDOW*	win;
#endif
{
	reg WINDOW*	par;
	reg int		x, y;

	for(par = win->_parent; par; win = par, par = par->_parent)
	{	x = win->_parx + win->_curx;
		y = win->_pary + win->_cury;
		if(x >= 0 && x < par->_maxx && y >= 0 && y < par->_maxy)
		{	par->_curx = x;
			par->_cury = y;
		}
		else	break;
	}
	return OK;
}
