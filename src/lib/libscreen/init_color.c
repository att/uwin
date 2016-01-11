#include	"scrhdr.h"
#include	"termhdr.h"



/*
**	Redefine specified color number.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
int init_color( short c, short red, short green, short blue )
#else
int init_color( c, red, green, blue )
short	c, red, green, blue;
#endif
{	
	/* color check */
	if( COLORS < 1 || c < 0 || c >= COLORS )
		return ERR;

	/* rgb or hls check */
	if( T_hls )
	{	if( red < 0 || red > 360 || green < 0 || green > 100 ||
	        blue < 0 || blue > 100 )
			return ERR;
	} else
	{	if( red < 0 || red > 1000 || green < 0 || green > 1000 ||
	        blue < 0 || blue > 1000 )
			return ERR;
	}

	/* table check */
	if( rgb_color_TAB == NIL(RGB_COLOR*) )
		return ERR;

	/* reset color */
	if( T_hls )
	{	_rgb2hls( red, green, blue,
		    &rgb_color_TAB[c].r, &rgb_color_TAB[c].g, &rgb_color_TAB[c].b );
	} else
	{	rgb_color_TAB[c].r = red;
		rgb_color_TAB[c].g = green;
		rgb_color_TAB[c].b = blue;
	}

	/* put new color on screen */
	if( T_initc )
		putp( tparm(T_initc, (char*)c, (char*)red,(char*)green,(char*)blue) );

	return OK;
}
