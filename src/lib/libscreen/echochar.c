#include	"scrhdr.h"


/*
**	addch, then refresh
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wechochar(WINDOW* win, chtype ch)
#else
int wechochar(win,ch)
WINDOW*	win;
chtype	ch;
#endif
{
	if(waddch(win,ch) == ERR)
		return ERR;
	if(!win->_immed)
		return wrefresh(win);
	return OK;
}
