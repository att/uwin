#include	"scrhdr.h"

/*
**	Check if a line in window has been changed.
**		y: the line number
**		return TRUE if changed (1), otherwise FALSE
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int is_linetouched(WINDOW* win, int y)
#else
int is_linetouched(win,y)
WINDOW	*win;
int	y;
#endif
{
	return (y >= 0 && y < win->_maxy && win->_firstch[y] != _INFINITY) ? TRUE:FALSE;
}
