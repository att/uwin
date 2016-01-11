#include	"termhdr.h"


/*
**	Set/unset character break mode
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _setcbreak(int yes)
#else
int _setcbreak(yes)
int	yes;
#endif
{
#if _hdr_termio
	if(yes)
	{	_curtty.c_lflag &= ~ICANON;
		_curtty.c_lflag |= ISIG;
		_curtty.c_cc[VMIN] = 1;
		_curtty.c_cc[VTIME] = 0;
	}
	else
	{	_curtty.c_lflag |= ICANON;
		_curtty.c_cc[VEOF] = 0x04;
		_curtty.c_cc[VEOL] = 0x00;
	}
#else
	if(yes)
	{	_curtty.sg_flags |= CBREAK;
		_curtty.sg_flags &= ~CRMOD;
	}
	else
	{	_curtty.sg_flags &= ~CBREAK;
		_curtty.sg_flags |= CRMOD;
	}
#endif
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
