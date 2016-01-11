#include	"scrhdr.h"

/*
**	Input a character through a window.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int wgetch(WINDOW* win)
#else
int wgetch(win)
WINDOW	*win;
#endif
{
	reg int		c, doecho, inraw;

	/* can't be done */
	if(_echoit &&
	   ((!win->_scroll &&
	     win->_curx == win->_maxx-1 && win->_cury == win->_maxy-1) ||
	    (win->_cury+win->_begy) >= LINES ||
	    (win->_curx+win->_begx) >= COLS))
		return ERR;

	/* update the screen before user interaction */
	wrefresh(win);

	/* something already in buffer */
	if(_inputbeg < _inputcur)
	{
		doecho = FALSE;
		inraw = TRUE;
		c = tgetch(win->_keypad ? 1 : 0);
		goto done;
	}

	/* set appropriate ioctl states */
	if((doecho = _echoit) != 0)	/* assignment = */
		noecho();
	if(!(inraw = _rawmode))
		cbreak();

	/* set time-out period */
	if(win->_delay != _timeout)
		ttimeout(win->_delay);

	/* set appropriate keyboard states */
	if(win->_keypad != curscreen->_keypad)
	{
		_puts(win->_keypad ? T_smkx : T_rmkx);
		curscreen->_keypad = win->_keypad;
	}
	if(win->_meta != curscreen->_meta)
	{
		_puts(win->_meta ? T_smm : T_rmm);
		curscreen->_meta = win->_meta;
	}
	_tflush();

	/* ok, get the char */
	do
	{
		_wasstopped = FALSE;
		c = tgetch(!win->_keypad ? 0 : !win->_notimeout ? 1 : 2);
	} while(c == ERR && _wasstopped);

done:
	if(doecho)
		echo();
	if(!inraw)
		nocbreak();

	if(_echoit && c != ERR && c != '\0')
	{
		waddch(win,(chtype)c);
		if(!win->_immed)
			wrefresh(win);
	}

	return c;
}
