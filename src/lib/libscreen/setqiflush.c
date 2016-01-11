#include	"termhdr.h"


/*
**	Set/unset flushing the input queue on interrupts or quits.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _setqiflush(int yes)
#else
int _setqiflush(yes)
int	yes;
#endif
{
#if _hdr_termio
	if(yes)
		_curtty.c_lflag &= ~NOFLSH;
	else	_curtty.c_lflag |= NOFLSH;
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
#else
	return yes ? ERR : OK;
#endif
}
