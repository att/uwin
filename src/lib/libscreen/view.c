#include	"scrhdr.h"


/*
**	View texts in a window
**
**	win:	window to view file
**	topln:	first top line
**	getln:	function to read a line of text (delineated with \n).
**		Called as (*getln)(bufp,n)
**			bufp:	*bufp returns the buffer containing the line.
**			n:	a line number.
**		Return values of getln():
**			>= 0:	success
**			< 0:	no more line to display
**	getuser: function to get a response from the user and to update
**		the screen. It is called as (*getuser)(win,topn,botn)
**			topn, botn: inclusive range of lines in display.
**		
**		Return values of getuser():
**			>=0:	line number to start display
**			< 0:	quit
**		
**	kind:	1 for literal, 0 for interpreting \\ and \# code
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int wview(WINDOW* win, int topln,
	int(*getln)(char**,int), int(*getuser)(WINDOW*,int,int),
	int kind)
#else
int wview(win,topln,getln,getuser,kind)
WINDOW	*win;
int	topln, (*getln)(), (*getuser)();
int	kind;
#endif
{
	reg bool	didsome;
	reg int		pos;

	if(!getln )
		return ERR;

	didsome = FALSE;
	scrollok(win,FALSE);
	immedok(win,FALSE);

	pos = 0;
	for(;;)
	{
		reg int	 n, y;
		char	*lp;

		werase(win);
		for(n = topln, y = 0;; ++n, y = win->_cury+1)
		{
			bool	did_addch;

			/* read a line */
			if((*getln)(&lp,n) < 0)
			{
				--n;
				break;
			}

			didsome = TRUE;
			wmove(win,y,0);

			/* add the line into the window */
			did_addch = FALSE;
			for(lp += pos; *lp != '\n' && *lp != '\0'; ++lp, ++pos)
			{
				reg chtype	c;

				c = (chtype)(*lp);
				if(kind == 0 && c == '\\' &&
				   lp[1] >= '0' && lp[1] <= '9')
				{
					lp += 1;
					c = *lp;
					if(c == '0')
						wattrset(win,A_NORMAL);
					else if(c == '1')
						wattron(win,A_STANDOUT);
					else if(c == '2')
						wattron(win,A_UNDERLINE);
					else if(c == '3')
						wattron(win,A_REVERSE);
					else if(c == '4')
						wattron(win,A_BLINK);
					else if(c == '5')
						wattron(win,A_DIM);
					else if(c == '6')
						wattron(win,A_BOLD);
					else if(c == '7')
						wattron(win,A_INVIS);
					else if(c == '8')
						wattron(win,A_PROTECT);
					else if(c == '9')
						wattron(win,A_ALTCHARSET);
				}
				else
				{
					did_addch = TRUE;
					if(kind == 0 && c == '\\' && lp[1] == '\\')
					{	lp += 1;
						c = *lp;
					}
#if _MULTIBYTE
					c = RBYTE(c);
#endif
					if(waddch(win,c) == ERR)
					{
						if(lp[1] == '\n' || lp[1] == '\0')
							pos = 0;
						else
						{
							--n;
							goto dodisplay;
						}
					}
				}
			}

			if(lp[0] == '\0' || lp[0] == '\n')
				pos = 0;

			if(win->_cury == win->_maxy-1)
				break;

			/* this resulted from an auto-margin */
			if(did_addch && win->_curx == 0)
				wmove(win,win->_cury-1,0);
		}

	dodisplay:
		if(!didsome || !getuser)
			return OK;

		if((topln = (*getuser)(win,topln,n)) < 0)
			return OK;
	}
}
