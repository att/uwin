#include	"scrhdr.h"
#include	"termhdr.h"



/*
**	Redefine a color pair to foreground and background colors.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
int init_pair( short p, short fg, short bg )
#else
int init_pair( p, fg, bg )
short	p, fg, bg;
#endif
{	
	/* check pair */
	if( COLORS < 1 || p < 0 || p >= COLOR_PAIRS )
		return ERR;

	/* check foreground and background */
	if( fg < 0 || fg >= COLORS || bg < 0 || bg >= COLORS  )
		return ERR;

	/* check table */
	if( color_pair_TAB == NIL(PAIR_COLOR*) )
		return ERR;

	/* reset color pair */
	color_pair_TAB[p].f = fg;
	color_pair_TAB[p].b = bg;

	/** SOON:
	 **     update any characters with old color-pair def and refresh screen
	 **/

	/* send new color pair to screen */
	if( T_initp )
	{	putp( tparm( T_initp, (char*)p,
				(char*)(rgb_color_TAB[fg].r),
				(char*)(rgb_color_TAB[fg].g),
				(char*)(rgb_color_TAB[fg].b),
				(char*)(rgb_color_TAB[bg].r),
				(char*)(rgb_color_TAB[bg].g),
				(char*)(rgb_color_TAB[bg].b) ) );
	}

	return OK;
}
