#include	"scrhdr.h"

/*	Return a string of chtype starting at (cury,curx).
**	In a multibyte environment, we try to return whole characters.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int winchnstr(WINDOW *win, chtype *str, int n)
#else
int winchnstr(win, str, n)
WINDOW*	win;
chtype*	str;
int	n;
#endif
{
	chtype	*ch, *endch;

	ch = win->_y[win->_cury] + win->_curx;
#if _MULTIBYTE
	if(ISCBIT(*ch))
		return ERR;
#endif

	if(n < 0 || n >= (win->_maxx - win->_curx))
		endch = ch + (win->_maxx - win->_curx);
	else
	{	endch = ch + n;
#if _MULTIBYTE
		while(ISCBIT(*endch))
			endch -= 1;
#endif
	}

	n = endch - ch;
	while(ch < endch)
		*str++ = *ch++;
	*str = 0;

	return n;
}
