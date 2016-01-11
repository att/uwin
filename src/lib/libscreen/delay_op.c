#include	"scrhdr.h"


/*
**	Insert ms milliseconds delay in the output stream of the
**	current terminal. At high baud rate, this can be cpu expensive.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int delay_output(int ms)
#else
int delay_output(ms)
int	ms;
#endif
{
	return _delay(ms*10,_putbyte);
}
