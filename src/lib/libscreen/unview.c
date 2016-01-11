#include	"scrhdr.h"



/*
**	Dump the image of a window into a readable format
**	Stand-out, Inverse Video and Bold are output as overstriked,
**	other video attributes are output as underlined.
**
**	win:	the window to be dumped.
**	putln:	function to write a line of text.
**	kind:	0	for line printer mode
**		1	for nroff/troff mode
**		2	for \# mode
**		3	for plain-text mode  (attributes ignored)
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wunview(WINDOW* win, int(*putln)(char*), int kind)
#else
int wunview(win,putln,kind)
WINDOW	*win;
int	(*putln)();
int	kind;
#endif
{
	reg int		x, y, bufn;
	reg chtype	curattrs, c, a, *wcp;
	char		buf[BUFSIZ];
#if _MULTIBYTE
	char		*sp;
	reg int		k, n;
#endif

	/* write a character into buf which is hopefully big enough */
#define	_putbuf(c)	(buf[bufn == (BUFSIZ-1) ? bufn : bufn++] = c)

	if(kind == 1)
		(*putln)(".nf\n");

	curattrs = A_NORMAL;
	for(y = 0; y < win->_maxy; ++y)
	{
		wcp = win->_y[y];
		bufn = 0;
		for(x = 0; x < win->_maxx;)
		{
			a = *wcp & A_ATTRIBUTES;

			/* wview format */
			if(kind == 2)
			{
				if(curattrs != a)
				{
					if(curattrs != A_NORMAL)
						(_putbuf('\\'),_putbuf('0'));
					if(a&A_STANDOUT)
						(_putbuf('\\'),_putbuf('1'));
					if(a&A_UNDERLINE)
						(_putbuf('\\'),_putbuf('2'));
					if(a&A_REVERSE)
						(_putbuf('\\'),_putbuf('3'));
					if(a&A_BLINK)
						(_putbuf('\\'),_putbuf('4'));
					if(a&A_DIM)
						(_putbuf('\\'),_putbuf('5'));
					if(a&A_BOLD)
						(_putbuf('\\'),_putbuf('6'));
					if(a&A_INVIS)
						(_putbuf('\\'),_putbuf('7'));
					if(a&A_PROTECT)
						(_putbuf('\\'),_putbuf('8'));
					if(a&A_ALTCHARSET)
						(_putbuf('\\'),_putbuf('9'));
					curattrs = a;
				}
			}
			/* nroff format */
			else if(kind == 1)
			{
				if(a & (A_STANDOUT|A_REVERSE|A_BOLD))
				{
					if(curattrs != A_BOLD)
					{
						curattrs = A_BOLD;
						_putbuf('\\');
						_putbuf('f');
						_putbuf('B');
					}
				}
				else if(a != A_NORMAL)
				{
					if(curattrs != A_UNDERLINE)
					{
						curattrs = A_UNDERLINE;
						_putbuf('\\');
						_putbuf('f');
						_putbuf('I');
					}
				}
				else
				{
					if(curattrs != A_NORMAL)
					{
						curattrs = A_NORMAL;
						_putbuf('\\');
						_putbuf('f');
						_putbuf('R');
					}
				}
			}

#if _MULTIBYTE
			/* get the character */
			sp = wmbinch(win,y,x);

			/* number of columns used */
			c = RBYTE(*sp);
			n = scrwidth[TYPE(c)];

			if(kind == 0)
			{
				/* overstrike */
				if(a & (A_STANDOUT|A_REVERSE|A_BOLD))
					for(k = 0; sp[k] != '\0'; ++k)
						_putbuf(sp[k]);
				/* underline */
				else if(a)
					for(k = 0; k < n; ++k)
						_putbuf('_');

				/* move the cursor back */
				if(a)
					for(k = 0; k < n; ++k)
						_putbuf('\b');
			}
			for(k = 0; sp[k] != '\0'; ++k)
				_putbuf(sp[k]);

			x += n;
			wcp += n;
#else
			c = *wcp & A_CHARTEXT;
			if(kind != 0 || a == 0)
			{	if(kind == 3 && *wcp & A_ALTCHARSET)
				{	if( *wcp == ACS_ULCORNER || *wcp == ACS_LLCORNER ||
					    *wcp == ACS_URCORNER || *wcp == ACS_LRCORNER  )
						_putbuf('+');
					else if( *wcp == ACS_HLINE )
						_putbuf('-');
					else if( *wcp == ACS_VLINE )
						_putbuf('|');
					else if( *wcp == ACS_LARROW )
						_putbuf('<');
					else if( *wcp == ACS_RARROW )
						_putbuf('>');
					else if( *wcp == ACS_DARROW )
						_putbuf('v');
					else if( *wcp == ACS_UARROW )
						_putbuf('^');
					else 		/* rare  :-) */
						_putbuf(c);
				}
				else
					_putbuf(c);
			}
			else
			{
				if(c != ' ')
				{
					_putbuf(c);
					_putbuf('\b');
				}
				_putbuf('_');
				if(a & (A_STANDOUT|A_REVERSE|A_BOLD))
					_putbuf(c);
				else	_putbuf('_');
			}

			++x;
			++wcp;
#endif
		}

		_putbuf('\n');
		_putbuf('\0');
		(*putln)(buf);
	}

	if(kind == 1)
		(*putln)(".fi\n");

	return OK;
}
