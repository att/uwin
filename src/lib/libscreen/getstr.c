#include	"scrhdr.h"


/*
**	Read a string, stop at \n or \r
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wgetstr(WINDOW* win, char* s)
#else
int wgetstr(win,s)
WINDOW	*win;
char	*s;
#endif
{
	reg int	n, c;
	bool	inraw, doecho, savsync, savimmed, savleave;
	short	x[1024], y[1024];

	inraw = _rawmode;
	cbreak();
	doecho = _echoit;
	noecho();

	if(doecho)
	{
		savsync = win->_sync; win->_sync = FALSE;
		savimmed = win->_immed; win->_immed = TRUE;
		savleave = win->_leeve; win->_leeve = FALSE;
		wrefresh(win);
	}

	for(n = 0;;)
	{	/* save coordinates */
		if(doecho)
		{
			x[n] = win->_curx;
			y[n] = win->_cury;
		}

		c = wgetch(win);
			
		/* at end of string */
		if(c == ERR || c == '\n' || c == '\r')
			break;

		/* line discipline */
		if(c == killchar() || c == KEY_BREAK ||
		   c == erasechar() || c == KEY_LEFT || c == KEY_BACKSPACE)
		{
			n = (c == killchar() || c == KEY_BREAK) ? 0 :
				(n > 0 ? n-1 : 0);
			if(doecho)
			{
				wmove(win,y[n],x[n]);
				wclrtoeol(win);
			}
		}
		/* new character */
		else
		{
			s[n++] = c;
			if(doecho)
				waddch(win,(chtype)c);
		}
	}

	if(doecho)
	{
		waddch(win,(chtype)('\n'));
		echo();
		win->_sync = savsync;
		win->_immed = savimmed;
		win->_leeve = savleave;
	}
	if(!inraw)
		nocbreak();

	s[n] = '\0';
	return c == ERR ? ERR : OK;
}
