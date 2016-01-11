#include	"scrhdr.h"



/*
**	Set meta mode
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int meta(WINDOW* win, int bf)
#else
int meta(win,bf)
WINDOW	*win;
int	bf;
#endif
{
	if(!T_smm || !T_rmm)
		return FALSE;
	win->_meta = bf;
	return bf;
}
