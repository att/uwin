#include	"scrhdr.h"



/*
**	Convert red-green-blue to hue-lightness-saturation.
**
**		Typical RGB values range from 0 - 1000 each.
**		Typical HLS values range from 0 - 360, 0-100, and 0-100.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
void _rgb2hls( short r, short g, short b, short *h, short *l, short *s )
#else
void _rgb2hls( r, g, b, h, l, s )
short	r, g, b, *h, *l, *s;
#endif
{	reg short	min, max;

	/* get min and max of rgb  (0-1000) */
	min = (r < g && r < b) ? r : (g < b) ? g : b;
	max = (r > g && r > b) ? r : (g > b) ? g : b;

	/* lightness  (1-100) */
	*l = (min + max) / 20;

	/* grays, black, and white */
	if( min == max )
	{	*h = 0;
		*s = 0;
		return;
	}

	/* saturation  (0-100) */
	*s = ( *l < 50 ) ? (((max-min)*100) / (max+min))
					 : (((max-min)*100) / (2000-max-min));

	/* hue  (0-360, mod 360) */
	*l = (r == max ) ? ((120 + ((g-b)*60)/(max-min)) % 360)
	   : (g == max ) ? ((240 + ((b-r)*60)/(max-min)) % 360)
	                 : ((360 + ((r-g)*60)/(max-min)) % 360);
}
