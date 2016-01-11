#include	"scrhdr.h"


#if __STD_C
void
wmouse_position(WINDOW *w, int *y, int *x)
#else
void
wmouse_position(w, y, x)
WINDOW	*w;
int 	*y, *x;
#endif
{
	reg 	short	y1,x1, y2,x2;	/* for WINDOW structure */

	getbegyx( w, y1, x1 );
	getmaxyx( w, y2, x2 );

	/* Note:  some  xterms  report only on a button click */
	if( request_mouse_pos() == ERR)
		;	/* use position from most recent button click */

	*y = ( y1 <= curmouse->y && curmouse->y < y1+y2 ) ?
		curmouse->y - y1 : -1 ;
	*x = ( x1 <= curmouse->x && curmouse->x < x1+x2 ) ?
		curmouse->x - x1 : -1 ;

	if( *y < 0 )
		*x = -1;
	if( *x < 0 )
		*y = -1;
}
