#include	"scrhdr.h"


/*
**	Make the labels appeared changed
**
**	Written by Kiem-Phong Vo
*/
int slk_touch()
{
	reg SLK_MAP	*slk;
	reg int		i;

	if(!(slk = _curslk) )
		return ERR;

	for(i = 0; i < slk->_num; ++i)
		slk->_lch[i] = TRUE;
	slk->_changed = TRUE;

	return OK;
}
