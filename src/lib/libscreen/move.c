#include	"scrhdr.h"

/*
**	Move (cury,curx) of win to (y,x)
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int wmove(WINDOW* win, int y, int x)
#else
int wmove(win,y,x)
WINDOW	*win;
int	y, x;
#endif
{
	if(y < 0 || x < 0 || y >= win->_maxy || x >= win->_maxx)
		return ERR;

#if _MULTIBYTE
	if(y != win->_cury || x != win->_curx)
		win->_nbyte = -1;
#endif

	win->_cury = y;
	win->_curx = x;

	return win->_immed ? wrefresh(win) : OK;
}
