#include	"termhdr.h"



/*
**	Set the current time-out period for getting input.
**
**	delay:	< 0 for infinite delay (blocking read)
**		= 0 for no delay read
**		> 0 to specify a period in millisecs to wait
**		    for input, then return '\0' if none comes
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int ttimeout(int delay)
#else
int ttimeout(delay)
int	delay;
#endif
{
	if(_inputd < 0)
		return ERR;

	if(delay < 0)
		delay = -1;

	_timeout = delay;
	return OK;
}
