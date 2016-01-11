#include	"scrhdr.h"


/*
**	Set nodelay mode
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int nodelay(WINDOW* win, int bf)
#else
int nodelay(win,bf)
WINDOW	*win;
int	bf;
#endif
{
	win->_delay = bf ? 0 : -1;
	return bf;
}
