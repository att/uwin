#include	"termhdr.h"


/*
**	Reset line discipline to the shell state
**
**	Written by Kiem-Phong Vo
*/
int reset_shell_mode()
{
#ifdef TCHARS
	if(SETCHARS(_orgchars) < 0)
		return ERR;
#endif
#ifdef LTCHARS
	if(SETLCHARS(_orglchars) < 0)
		return ERR;
#endif
#ifdef TAUXIL
	if(SETTAUXIL(_orgtauxil) < 0)
		return ERR;
#endif
	return SETTY(_orgtty) < 0 ? ERR : _tty_mode(&(_orgtty));
}
