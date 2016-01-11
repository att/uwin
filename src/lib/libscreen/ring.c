#include	"scrhdr.h"


/*
**	bf: TRUE for beep, FALSE for flash
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int ring(int bf)
#else
int ring(bf)
int	bf;
#endif
{
	char	*bel;

	if(bf)
		bel = T_bel ? T_bel : T_flash;
	else	bel = T_flash ? T_flash : T_bel;

	_puts(bel);
	_tflush();

	if(_inputpending)
		doupdate();
	return OK;
}
