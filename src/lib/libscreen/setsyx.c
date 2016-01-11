#include	"scrhdr.h"



/*
**	Functions to get and set the cursor of _stdbody
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _getsyx(int* yp, int* xp)
#else
int _getsyx(yp,xp)
int	*yp, *xp;
#endif
{
	*yp = _virtscr->_cury, *xp = _virtscr->_curx;
	*yp -= stdscr->_yoffset;
	return OK;
}

#if __STD_C
int setsyx(int y, int x)
#else
int setsyx(y,x)
int	y, x;
#endif
{
	if(y >= 0 && x >= 0)
	{
		_virtscr->_cury = y + stdscr->_yoffset;
		_virtscr->_curx = x;
		_virtscr->_leeve = FALSE;
	}
	return OK;
}
