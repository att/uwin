#include	"termhdr.h"


/*
**	See if the terminal has insert/delete line capabilities
**
**	Written by Kiem-Phong Vo
*/
int has_il()
{
	return ((T_il1 || T_il) && (T_dl1 || T_dl));
}
