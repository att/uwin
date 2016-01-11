#include	"termhdr.h"


/*
**	Set/unset duplex mode.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _setecho(int yes)
#else
int _setecho(yes)
int	yes;
#endif
{
#if _hdr_termio
	if(yes)
		_curtty.c_lflag |= ECHO;
	else	_curtty.c_lflag &= ~ECHO;
#else
	if(yes)
		_curtty.sg_flags |= ECHO;
	else	_curtty.sg_flags &= ~ECHO;
#endif
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
