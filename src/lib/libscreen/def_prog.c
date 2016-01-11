#include	"termhdr.h"


/*
**	Save current terminal io-control discipline state
**
**	Written by Kiem-Phong Vo
*/
int def_prog_mode()
{
#ifdef TCHARS
	if(GETCHARS(_curchars) < 0)
		return ERR;
#endif
#ifdef LTCHARS
	if(GETLCHARS(_curlchars) < 0)
		return ERR;
#endif
#ifdef TAUXIL
	if(GETTAUXIL(_curtauxil) < 0)
		return ERR;
#endif
	return GETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
