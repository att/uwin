#include	"termhdr.h"

int getmouse()		/* return mouse button events being passed to app */ 
{
	if( curmouse == NIL(MOUSE*) )
		return 0;

	return (int) curmouse->_mbapp;	/* returns an int using bits 1-15 */
}
