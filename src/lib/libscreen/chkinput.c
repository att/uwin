#include	"termhdr.h"


/*
**	Check to see if there is something to read in the input stream.
**	Return TRUE/FALSE if there is something/nothing.
**
**	Written by Kiem-Phong Vo
*/
#if _lib_select
#define TIMEB	struct timeval
#else
#define TIMEB	int
#if !defined(FNDELAY) && defined(O_NDELAY)
#define FNDELAY	O_NDELAY
#endif
#endif /*_lib_select*/

int chkinput()
{
	int	f;

	if(_macroerr)
		return FALSE;
	else if(_ahead < -1)
		return (_inputbeg < _inputcur);
	else if(_ahead < 0)
		return FALSE;
	else if(_inputbeg < _inputcur)
		return TRUE;

#if _lib_select
	{
	TIMEB	tmb;

	f = 1 << _ahead;
	tmb.tv_sec = tmb.tv_usec = 0;
	return select(20,(fd_set*)&f,0,0,&tmb) > 0 ? TRUE : FALSE;
	}
#else
#ifdef FIONREAD
	f = 0;
	return (ioctl(_ahead,FIONREAD,&f) >= 0 && f > 0) ? TRUE : FALSE;
#else
#ifdef FNDELAY
	{
	int	tmb;
	tmb = fcntl(_ahead,F_GETFL,0);
	fcntl(_ahead,F_SETFL,FNDELAY);
	if((f = read(_ahead,_inputcur,_inputend-_inputcur)) > 0)
		_inputcur += f;
	fcntl(_ahead,F_SETFL,tmb);
	return f > 0 ? TRUE : FALSE;
	}
#else
	return FALSE;
#endif /*FNDELAY*/
#endif /*FIONREAD*/
#endif /*_lib_select*/
}
