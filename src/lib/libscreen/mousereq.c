#include	"scrhdr.h"

int request_mouse_pos() 	/* Tell device to report current mouse ptr position */
{	reg int 	rv;	/* Note:  some  xterms  report only on button click */

	if( MOUSE_OFF()  ||  cur_term->_mouse_f != NIL(Mdriver_f)  ||
	  	!T_reqmp || !T_reqmp[0] )	/* reqmp terminal capability not defined */
		return ERR;

	/* issue terminfo cmd:  req_mouse_pos */
	_puts(T_reqmp);		/* macro in  scrhdr.h */
	_tflush();

	/* update curmouse via tgetch() - decode key_mouse & mouse_info seqs */
	rv = tgetch( 2 );	/* blocking read of mouse escape sequence */
	return rv==ERR ? ERR : OK;
}
