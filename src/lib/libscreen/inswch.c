#include	"scrhdr.h"

#if _MULTIBYTE


/*
**	Insert to 'win' a process code at (curx,cury).
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winswch(WINDOW* win, chtype c)
#else
int winswch(win,c)
WINDOW	*win;
chtype	c;
#endif
{
	int	i, width;
	uchar	buf[CSMAX];
	chtype	a;
	wchar_t	code;

	a = ATTR(c);
	code = (wchar_t)(c&A_CHARTEXT);

	/* translate the process code to character code */
	width = _code2byte(code,(char*)buf);

	/* now call winsch to do the real work */
	for(i = 0; i < width; ++i)
		if(winsch(win,buf[i]) == ERR)
			return ERR;
	return OK;
}

#else
void _mbunused(){}
#endif
