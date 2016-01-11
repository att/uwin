#include	"termhdr.h"

/*
**	Flush input of current terminal
**
**	Written by Kiem-Phong Vo
*/

int flushinp()
{
	_inputbeg = _inputcur = _inputbuf;

#ifdef TIOCSETP
	return ioctl(_outputd,TIOCSETP,&(_curtty)) < 0 ? ERR : OK;
#else
#ifdef TCFLSH
	return ioctl(_outputd,TCFLSH,0) < 0 ? ERR : OK;
#else
	return OK;
#endif /*TCFLSH*/
#endif /*TIOCSETP*/
}
