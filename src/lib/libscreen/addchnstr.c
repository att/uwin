#include	"scrhdr.h"

/*	Add a string of chtype to the window
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
waddchnstr(WINDOW* win, chtype* str, int n)
#else
waddchnstr(win, str, n)
WINDOW*	win;
chtype*	str;
int	n;
#endif
{
	chtype	*ch, *endch, *begch;

	begch = ch = win->_y[win->_cury] + win->_curx;
	endch = ch + win->_maxx;

#if _MULTIBYTE
	if(ISCBIT(*ch))
		return ERR;
	if(n != 0 && ISCBIT(*str))
		return ERR;
#endif

	if(n < 0)
	{	while(ch < endch && *str)
			*ch++ = *str++;
	}
	else
	{	if((ch+n) < endch)
			endch = ch+n;
		while(ch < endch)
			*ch++ = *str++;
	}

	return ch-begch;
}
