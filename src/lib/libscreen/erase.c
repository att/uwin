#include	"scrhdr.h"


/*
**	Erase everything on the window.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int werase(WINDOW* win)
#else
int werase(win)
WINDOW	*win;
#endif
{
	wmove(win,0,0);
	return wclrtobot(win);
}
