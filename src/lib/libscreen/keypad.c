#include	"scrhdr.h"



/*
**	Set keyboard modes to transmit or not
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int keypad(WINDOW* win, int bf)
#else
int keypad(win,bf)
WINDOW	*win;
int	bf;
#endif
{
	if(!_delkey && !_keymap && setkeymap() == ERR)
		return FALSE;
	if(!_keymap)
		return FALSE;

	return (win->_keypad = bf);
}
