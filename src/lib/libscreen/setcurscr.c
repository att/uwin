#include	"scrhdr.h"


/*
**	Switch to a newscr screen to talk with
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
SCREEN	*setcurscreen(SCREEN* newscr)
#else
SCREEN	*setcurscreen(newscr)
SCREEN	*newscr;
#endif
{
	if(!newscr)
		return NIL(SCREEN*);

	curscreen = newscr;

	stdscr = curscreen->_stdscr;
	curscr = curscreen->_curscr;
	_virtscr = curscreen->_vrtscr;

	LINES = _LINES;
	COLS = _COLS;
	TABSIZ = _TABSIZ;

	setcurterm(curscreen->_term);

	return curscreen;
}
