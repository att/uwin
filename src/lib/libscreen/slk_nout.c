#include	"scrhdr.h"


/*
**	wnoutrefresh for the softkey window
**
**	Written by Kiem-Phong Vo
*/
int slk_noutrefresh()
{
	if(!_curslk )
		return ERR;

	if(_curslk->_win && _slk_update())
		wnoutrefresh(_curslk->_win);

	return OK;
}
