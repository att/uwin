#include	"termhdr.h"


/*
**	Set the input channel for the current terminal
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int tinputfd(int fd)
#else
int tinputfd(fd)
int	fd;
#endif
{
	_inputd = fd;
	_timeout = -1;

	/* so that tgetch will reset it to be _inputd */
	_ahead = _AHEADPENDING;

	return OK;
}
