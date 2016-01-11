#include	"scrhdr.h"

#if _MULTIBYTE

/*
**	Get a string consists of process code
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wgetwstr(WINDOW* win, wchar_t* code)
#else
int wgetwstr(win,code)
WINDOW	*win;
wchar_t	*code;
#endif
{
	char	buf[BUFSIZ];

	/* call getstr to get the characters */
	if(wgetstr(win,buf) == ERR)
		return ERR;

	/* translate it to process code */
	_strbyte2code(buf,code,-1);
	return OK;
}

#else
void _mbunused(){}
#endif
