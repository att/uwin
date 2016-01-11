#include	"scrhdr.h"

/*
**	Get the characters from current cursor position to right margin.
**
**	win:	the window to be dumped.
**	s, n:	buffer and length.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winnstr(WINDOW* win, char* s, int n)
#else
int winnstr(win, s, n)
WINDOW	*win;
char	*s;
int	n;
#endif
{
	reg int		x, y, w;
	reg chtype	c, *wcp;

	/* write a character into buf which is hopefully big enough */
#define	_putbuf(c)	{ *s++ = c; if((w += 1) > n) goto done; }

	if(n < 0)
		n = 4*win->_maxx + 1;

	w = 0;
	wcp = win->_y[y = win->_cury];
	for(x = win->_curx; x < win->_maxx;)
	{
#if _MULTIBYTE
		char		*sp;
		reg int		k;

		/* get the character */
		sp = wmbinch(win,y,x);

		/* number of columns used */
		c = RBYTE(*sp);
		k = scrwidth[TYPE(c)];
		x += k;
		wcp += k;

		for(; *sp; ++sp)
			{ _putbuf(*sp); }
#else
		c = *wcp & A_CHARTEXT;
		if(*wcp&A_ALTCHARSET)
		{	if(*wcp == ACS_ULCORNER || *wcp == ACS_LLCORNER ||
			   *wcp == ACS_URCORNER || *wcp == ACS_LRCORNER  )
				{ _putbuf('+'); }
			else if( *wcp == ACS_HLINE )
				{ _putbuf('-'); }
			else if( *wcp == ACS_VLINE )
				{ _putbuf('|'); }
			else if( *wcp == ACS_LARROW )
				{ _putbuf('<'); }
			else if( *wcp == ACS_RARROW )
				{ _putbuf('>'); }
			else if( *wcp == ACS_DARROW )
				{ _putbuf('v'); }
			else if( *wcp == ACS_UARROW )
				{ _putbuf('^'); }
			else 	{ _putbuf(c); }
		}
		else	{ _putbuf(c); }

		x += 1;
		wcp += 1;
#endif
	}

done:
	if(w < n)
		*s = 0;
	return w;
}
