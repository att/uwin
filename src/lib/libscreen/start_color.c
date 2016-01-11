#include	"scrhdr.h"
#include	"termhdr.h"


/*
**	Enable use of colors;
**	call before any color manipulation functions.
**
**	Written by John J Snyder, 10/28/1998
*/

#define 	COLOR_TRACE 			/* info to stderr */
#undef   	COLOR_TRACE				/* go silently */

#ifdef COLOR_TRACE
#ifdef _SFIO_H
#define 	STDERR  sfstderr
#define 	PRINTF  sfprintf
#else
#define 	STDERR  stderr
#define 	PRINTF  fprintf
#endif  /* _SFIO_H */
#endif  /* COLOR_TRACE */

#if __STD_C
int start_color( void )
#else
int start_color()
#endif
{	reg int 	nc, ncp;

#ifdef COLOR_TRACE
		PRINTF( STDERR, "start_color():  SCREEN_VERSION=%ld\n", SCREEN_VERSION);
#endif

	/* check for sufficient color capabilities,
	 * including number of colors and color pairs.
	 */
	if( ! has_colors() )
		return ERR;
	
	/* get number of COLORS supported on terminal */
	COLORS = T_colors;

	/* get number of COLOR_PAIRS supported on terminal */
	ncp = T_pairs;
	/*  Tektronix-style terminals have a pallet of N predefined colors
	 *  (typically 8) from which an application may select foreground and
	 *  background colors for N*N 'virtual' color_pairs.
	 */
	if( T_scp == NIL(char*) )
	{	/* virtual color_pairs; size internal datastructure color_pair_TAB */
		ncp = COLORS * COLORS;
		if( ncp > A_MAXCOLORPAIRS )
			ncp = A_MAXCOLORPAIRS;
	}
	COLOR_PAIRS = ncp;

#ifdef COLOR_TRACE
		PRINTF( STDERR,
		  "      COLORS=%d  COLOR_MAX=%d  COLOR_PAIRS=%d  A_MAXCOLORPAIRS=%ld\n"
		       , COLORS,    COLOR_MAX,    COLOR_PAIRS,    A_MAXCOLORPAIRS );
#endif

	/* check any old tables */
	if( rgb_color_TAB )
		free( rgb_color_TAB );
	if( color_pair_TAB )
		free( color_pair_TAB );

	/* get table space */
	nc = COLORS < COLOR_MAX ? COLOR_MAX : COLORS;	/* min 8 for dflt colors */
	if( !(rgb_color_TAB  = (RGB_COLOR*)  malloc(nc*sizeof(RGB_COLOR))) ||
	    !(color_pair_TAB = (PAIR_COLOR*) malloc(COLOR_PAIRS*sizeof(PAIR_COLOR)))
	   )
		return ERR;

	/* load conventional 8 default colors */
	if( T_hls )
		memcpy( rgb_color_TAB, _hls_bas_TAB, sizeof(RGB_COLOR)*COLOR_MAX );
	else
		memcpy( rgb_color_TAB, _rgb_bas_TAB, sizeof(RGB_COLOR)*COLOR_MAX );

	/* load default initial color pair */
	if( COLOR_PAIRS > 0 )
	{	color_pair_TAB[0].f = COLOR_WHITE;	/* so we assume */
		color_pair_TAB[0].b = COLOR_BLACK;	/* safe to assume per X/Open */
	}

#ifdef COLOR_TRACE
	/* reset colors to the terminal-specific initial values;
	 * may assume background is black for all terminals per X/Open.
	 */
	if( T_oc )
		_puts( T_oc );
	if( T_op )
		_puts( T_op );

		PRINTF( STDERR, "      ret=OK\n" );
#endif

	return OK;
}
