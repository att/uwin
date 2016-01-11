#include	"scrhdr.h"
#include	"termhdr.h"



/*
**	Retrieve foreground and background colors of a color pair.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
int pair_content( short p, short *fg, short *bg )
#else
int pair_content( p, fg, bg )
short	p, *fg, *bg;
#endif
{	
	/* check pair */
	if( COLORS < 1 || p < 0 || p >= COLOR_PAIRS )
		return ERR;

	/* table pair */
	if( color_pair_TAB == NIL(PAIR_COLOR*) )
		return ERR;

	/* retrieve foreground and background colors */
	*fg = color_pair_TAB[p].f;
	*bg = color_pair_TAB[p].b;

	return OK;
}
