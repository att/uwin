#include	"scrhdr.h"



/*
**	Change scrolling region
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wsetscrreg(WINDOW* win, int topy, int boty)
#else
int wsetscrreg(win,topy,boty)
WINDOW	*win;
int	topy, boty;
#endif
{
	if(topy < 0 || topy >= win->_maxy ||
	   boty < 0 || boty >= win->_maxy)
		return ERR;

	win->_tmarg = topy;
	win->_bmarg = boty;
	return OK;
}
