#include	"scrhdr.h"

/*
**	Find out if terminal can change color.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
bool can_change_color( void )
#else
bool can_change_color()
#endif
{
	/* check for change color capabilities */
	if( T_colors < 1 )		/* per X/Open curses */
		return FALSE;
	return ( T_ccc );
}
