#include	"scrhdr.h"


/*
**	addch, then prefresh
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int pechochar(WINDOW* pad, chtype ch)
#else
int pechochar(pad,ch)
WINDOW	*pad;
chtype	ch;
#endif
{
	int	savimmed, rv;

	savimmed = pad->_immed;
	pad->_immed = FALSE;
	if((rv = waddch(pad,ch)) != ERR)
		rv = prefresh(pad,pad->_pminy,pad->_pminx,
			pad->_sminy,pad->_sminx,pad->_smaxy,pad->_smaxx);
	pad->_immed = savimmed;
	return rv;
}
