#include	"scrhdr.h"


/*
**	Update all changes made by wnoutrefresh.
**	It's a real function because _virtscr
**	is not accessible to application programs.
**
**	Written by Kiem-Phong Vo
*/

int doupdate()
{
	return wrefresh(_virtscr);
}
