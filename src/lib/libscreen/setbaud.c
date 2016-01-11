#include	"termhdr.h"


/*
**	Set new baudrate
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int setbaud(int speed)
#else
int setbaud(speed)
int	speed;
#endif
{
	reg int	i;

	for(i = 0; i < _nbauds; ++i)
		if((unsigned short)speed <= _baudtab[i])
			break;
	if(i >= _nbauds)
		return ERR;

#if _hdr_termio
	_curtty.c_cflag |= i;
#else
	_curtty.sg_ospeed = _curtty.sg_ispeed = i;
#endif
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
