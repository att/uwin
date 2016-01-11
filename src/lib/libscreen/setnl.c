#include	"termhdr.h"


/*
**	Set/unset translation of \n and \r.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _setnl(int yes)
#else
int _setnl(yes)
int	yes;
#endif
{
#if _hdr_termio
	if(yes)
	{	_curtty.c_oflag |= (ONLCR|OCRNL);
		_curtty.c_iflag |= ICRNL;
	}
	else
	{	_curtty.c_oflag &= ~(ONLCR|OCRNL);
		_curtty.c_iflag &= ~ICRNL;
	}
#else
	if(yes)
		_curtty.sg_flags |= CRMOD;
	else	_curtty.sg_flags &= ~CRMOD;
#endif
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}


/*
**	Done here because 'nl' is too popular a name
*/
int nl()
{
	return _setnl(TRUE);
}
