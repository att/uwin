#include	"scrhdr.h"


/*
**	Restore screen labels
**
**	Written by Kiem-Phong Vo
*/
int slk_restore()
{
	if(_curslk)
	{
		slk_touch();
		slk_refresh();
	}
	return OK;
}
