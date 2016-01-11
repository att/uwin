#include	"scrhdr.h"


/*
**	Flush/noflush inputs on interrupts and quits.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int intrflush(WINDOW* win, int bf)
#else
int intrflush(win,bf)
WINDOW	*win;
int	bf;
#endif
{
	win = NIL(WINDOW*);
	if(bf)
		qiflush();
	else	noqiflush();

	return bf;
}
