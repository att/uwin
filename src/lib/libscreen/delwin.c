#include	"scrhdr.h"



/*
**	Delete a window.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int delwin(WINDOW* win)
#else
int delwin(win)
WINDOW*	win;
#endif
{
	reg WINDOW	*par;

	if(win->_ndescs > 0)
		return ERR;

	if(!win->_parent)
		free(win->_ybase);
	else for(par = win->_parent; par; par = par->_parent)
		par->_ndescs -= 1;

	free(win->_y);
	free(win->_firstch);
	free(win);

	return OK;
}
