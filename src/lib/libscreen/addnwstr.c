#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Add to 'win' at most n 'characters' of code starting at (cury,curx)
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int waddnwstr(WINDOW* win, wchar_t* code, int n)
#else
int waddnwstr(win,code,n)
WINDOW*		win;
wchar_t*	code;
int		n;
#endif
{
	reg char	*sp;

	/* translate the process code to character code */
	if(!(sp = (char*)_strcode2byte(code,NIL(char*),n)) )
		return ERR;

	/* now call waddnstr to do the real work */
	return waddnstr(win,sp,-1);
}

#else
void _mbunused(){}
#endif
