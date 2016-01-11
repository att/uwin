#include	"scrhdr.h"



/*
**	Check if any line in window has been changed.
**		return TRUE if changed (1), otherwise FALSE
**
**	CAN REWRITE AS A MACRO
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int is_wintouched(WINDOW* win)
#else
int is_wintouched(win)
WINDOW	*win;
#endif
{
	reg int	y;

	for(y = win->_maxy-1; y >= 0; --y)
		if(win->_firstch[y] != _INFINITY)
			return TRUE;
	return FALSE;
}
