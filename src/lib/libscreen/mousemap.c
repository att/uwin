#include	"termhdr.h"

int getbmap()	/* return slk mouse button events being mapped to fun keys */
{
	if( curmouse == NIL(MOUSE*) )
		return ERR;

	return  (int) curmouse->_mbslk;	/* returns an int using bits 1-15 */
}
