#include	"termhdr.h"


/*
**	See if the terminal has insert/delete char capabilities
**
**	Written by Kiem-Phong Vo
*/
int has_ic()
{
	return (T_ich1 || T_ich || T_smir) && (T_dch1 || T_dch);
}
