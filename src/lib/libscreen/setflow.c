#include	"termhdr.h"


/*
**	Set flow control
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _setflow(int yes)
#else
int _setflow(yes)
int	yes;
#endif
{
#if _hdr_termio
	if(yes)
		_curtty.c_iflag |= (IXON|IXOFF);
	else	_curtty.c_iflag &= ~(IXON|IXOFF);
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
#else
#ifdef SETCHARS
	if(yes)
	{	_curchars.t_startc = _orgchars.t_startc;
		_curchars.t_stopc = _orgchars.t_stopc;
	}
	else
	{	_curchars.t_startc = (char)(-1);
		_curchars.t_stopc = (char)(-1);
	}
	return SETCHARS(_curchars) < 0 ? ERR : OK;
#else
	return OK;
#endif
#endif
}
