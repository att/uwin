#include	"scrhdr.h"


/*
**	Reset line discipline to the program modes
**
**	Written by Kiem-Phong Vo
*/
int reset_prog_mode()
{
#ifdef TCHARS
	if(SETCHARS(_curchars) < 0)
		return ERR;
#endif
#ifdef LTCHARS
	if(SETLCHARS(_curlchars) < 0)
		return ERR;
#endif
#ifdef TAUXIL
	if(SETTAUXIL(_curtauxil) < 0)
		return ERR;
#endif

	/* for mouse  --  but without linking in any mouse functions! */
	if(MOUSE_ON())	/* if the mouse was grabbed before, grab it back */
	{	_puts( T_getm );
		_tflush();
	}

	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
