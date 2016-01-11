#include	"termhdr.h"
#include	"curses.h"



/*
**	Retrieve the RGB values of a color number.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
int color_content( short c, short *red, short *green, short *blue )
#else
int color_content( c, red, green, blue )
short	c, *red, *green, *blue;
#endif
{	
	/* color check */
	if( COLORS < 1 || c < 0 || c >= COLORS )
		return ERR;

	/* table check */
	if( rgb_color_TAB == NIL(RGB_COLOR*) )
		return ERR;

	/* get RGB values of the color */
	*red   = rgb_color_TAB[c].r;
	*green = rgb_color_TAB[c].g;
	*blue  = rgb_color_TAB[c].b;

	return OK;
}
