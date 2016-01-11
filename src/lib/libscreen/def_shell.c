#include	"termhdr.h"


/*
**	Save current terminal io-control discipline state
**
**	Written by Kiem-Phong Vo
*/
int def_shell_mode()
{
	if(GETTY(_orgtty) < 0 || GETTY(_curtty) < 0)
		return ERR;
#ifdef TCHARS
	if(GETCHARS(_orgchars) < 0 || GETCHARS(_curchars) < 0)
		return ERR;
#endif
#ifdef LTCHARS
	if(GETLCHARS(_orglchars) < 0 || GETLCHARS(_curlchars) < 0)
		return ERR;
#endif
#ifdef TAUXIL
	if(GETTAUXIL(_orgtauxil) < 0 || GETTAUXIL(_curtauxil) < 0)
		return ERR;
#endif
	return _tty_mode(&(_curtty));
}
