#include	"scrhdr.h"


/*
**	Add to 'win' at most n 'characters' of sp starting at (cury,curx).
**	If n is < 0, add all the string.
**	For multi-byte code sets, we assume that the string contains
**	entire characters.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int waddnstr(WINDOW* win, const char* argsp, int n)
#else
int waddnstr(win,argsp,n)
WINDOW*	win;
char*	argsp;
int	n;
#endif
{
	reg int		i, x, y, maxx;
	reg chtype	*wcp, c;
	reg short	*bch, *ech;
	reg uchar	*sp = (uchar*)argsp;
	int		savimmed, savsync, rv;

#if _MULTIBYTE
	/* throw away any current partial character */
	win->_nbyte = -1;
	win->_insmode = FALSE;
#endif

	/* turn these off in case waddch() is called */
	savimmed = win->_immed;
	savsync = win->_sync;
	win->_immed = win->_sync = FALSE;

	rv = OK;
	maxx = win->_maxx-1;
	n = n < 0 ? _INFINITY : n;
	wcp = NIL(chtype*);

	for(i = 0; *sp != '\0' && i < n; ++i)
	{
#if _MULTIBYTE
                if(_mbtrue)
                {       reg int m, k;
                        c = *sp;
                        m = TYPE(c);
                        m = cswidth[m] + ((m == 1 || m == 2) ? 1 : 0);
                        for(k = 0; k < m; ++k)
			{	if(waddch(win,c) == ERR)
					goto done;
				if((c = *++sp) == 0)
					goto done;
			}
                        continue;
                }
#endif

		/* normal processing of a single byte char */
		if(!wcp)
		{	y = win->_cury;
			x = win->_curx;
			bch = win->_firstch+y;
			ech = win->_lastch+y;
			wcp = win->_y[y]+x;
		}

		if(x == maxx || iscntrl(*sp))
		{	/* let waddch() do these cases */
			if((rv = waddch(win,(*sp)&CMASK)) == ERR)
				goto done;

			/* pointers may now be invalid */
			wcp = NIL(chtype*);
			++sp;
			continue;
		}	

		/* update the image */
		c = WCHAR(win,*sp);
		if(*wcp != c)
		{	*wcp = c;
			*bch = x < *bch ? x : *bch;
			*ech = x > *ech ? x : *ech;
		}
		win->_curx = ++x;
		++wcp;
		++sp;
	}

done :
	win->_sync = savsync;
	win->_immed = savimmed;
	if(win->_sync && win->_parent)
		wsyncup(win);
	return (rv == OK && win->_immed) ? wrefresh(win) : rv;
}
