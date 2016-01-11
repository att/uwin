#include	"termhdr.h"


/*
**	Set external variables that hold terminal mode information
**	The following table defines baudrate
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _tty_mode(SGTTYB* ttyp)
#else
int _tty_mode(ttyp)
SGTTYB	*ttyp;
#endif
{
#if _hdr_termio
	_ospeed = ttyp->c_cflag & CBAUD;
	_rawmode = !(ttyp->c_lflag&ICANON) ? TRUE : FALSE;
	_nonlcr = !(ttyp->c_oflag&(ONLCR|OCRNL)) ? TRUE : FALSE;
	_echoit = (ttyp->c_lflag&ECHO) ? TRUE : FALSE;
	_xtab = (ttyp->c_oflag&TAB3) ? TRUE : FALSE;
	_erasech = ttyp->c_cc[VERASE];
	_killch = ttyp->c_cc[VKILL];
#else
	_ospeed = ttyp->sg_ospeed;
	_rawmode = (ttyp->sg_flags&(RAW|CBREAK)) ? TRUE : FALSE;
	_nonlcr = ((ttyp->sg_flags&RAW) || !(ttyp->sg_flags&CRMOD)) ? TRUE : FALSE;
	_echoit = (ttyp->sg_flags&ECHO) ? TRUE : FALSE;
	_xtab = (ttyp->sg_flags&XTABS) ? TRUE : FALSE;
	_erasech = ttyp->sg_erase;
	_killch = ttyp->sg_kill;
#endif

	if(_ospeed < (short)_nbauds)
		_baudrate = _baudtab[_ospeed];

	return OK;
}
