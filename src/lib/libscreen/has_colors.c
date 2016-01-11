#include	"scrhdr.h"

/*
**	Find out if terminal supports color.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
bool has_colors( void )
#else
bool has_colors()
#endif
{
	/* check for sufficient color capabilities */
	return ( T_colors >= 0 && T_pairs >= 0 && (T_oc || T_op) && 
	         ((T_setaf && T_setab) || (T_setf && T_setb) || T_scp) );
}
