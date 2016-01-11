#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Insert to 'win' at most n 'characters' of code starting at (cury,curx)
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winsnwstr(WINDOW* win, wchar_t* code, int n)
#else
int winsnwstr(win,code,n)
WINDOW	*win;
wchar_t	*code;
int	n;
#endif
{
	reg char	*sp;

	/* translate the process code to character code */
	if(!(sp = (char*)_strcode2byte(code,NIL(char*),n)) )
		return ERR;

	/* now call winsnstr to do the real work */
	return winsnstr(win,sp,-1);
}

#else
void _mbunused(){}
#endif
