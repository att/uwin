#include	"scrhdr.h"


/*
**	Revised to  PROTOTYPE  mouse selection of a menu choice
*/


/*
**	Bring up a menu at the current cursor position of win.
**
**	Arguments:
**	win:	menu starts at (curx,cury) of win and uses memory of win
**	items:	list of items, NIL terminated
**	active:	list of active status of items, 0 for inactive, !0 for active
**	c:	first item to situate cursor at
**	top:	first top item in display.
**	bg, fg:	background and foreground video attributes.
**	ia:	attributes of inactive elements.
**	(*getuser)(): called as (*getuser)(win,cur,top,bot,repeat,input)
**		win:	window called with.
**		cur:	>= 0: the current menu item,
**			-1:	invalid.
**		top,bot: range of items in display.
**		repeat:	to return number of times to repeat the movement
**		input:	optionally, an input string can be returned
**			in the 'input' buffer. In this case,
**			the return value must be 0.
**		The return value can be 0, KEY_UP, KEY_DOWN, KEY_LEFT,
**		KEY_RIGHT, KEY_NPAGE, KEY_PPAGE to indicate accept or move.
**		The return value -1 terminates  wmenu without a choice.
**	kind:	or-ing of MENU_Attributes (see <curses.h>)
**
**	Return	-2: hard error, not enough display space
**		-1: no choice made or can be made
**		>=0: index of the item chosen
**
**	Written by Kiem-Phong Vo
*/


/* The environment of a menu */
typedef struct _m_
{
	WINDOW	*m_w;		/* original window */
	WINDOW	*m_mw;		/* menu window */
	WINDOW	*m_cw;		/* current item window */
	int	m_c, m_y, m_x;	/* current item position */
	int	m_ny, m_nx;	/* dimensions of menu matrix */
	int	m_oldtop, m_top, m_bot;		/* top and bottom items */
	int	m_len;		/* length of an item */
	chtype	m_fg, m_bg, m_ia;/* background, foreground, inactive attrs */
	int	m_row;		/* if menu is row oriented */
	int	m_mitems;	/* max number of items in a menu */
	int	m_nitems;	/* number of items */
	char	**m_items;	/* the items themselves */
	int	m_kind;		/* type of menu */
	int	m_move;		/* if menufg was called and curitem was moved */
	int	m_char0;	/* starting character position */
	char	*m_at;		/* active status */
} EVMENU;

static EVMENU	*Ev;		/* current environment */
#define Oldtop	(Ev->m_oldtop)
#define Top	(Ev->m_top)
#define Bot	(Ev->m_bot)
#define C	(Ev->m_c)
#define X	(Ev->m_x)
#define Y	(Ev->m_y)
#define NX	(Ev->m_nx)
#define NY	(Ev->m_ny)
#define Cwin	(Ev->m_cw)
#define Mwin	(Ev->m_mw)
#define Win	(Ev->m_w)
#define Length	(Ev->m_len)
#define Fg	(Ev->m_fg)
#define Bg	(Ev->m_bg)
#define Ia	(Ev->m_ia)
#define Row	(Ev->m_row)
#define Nitems	(Ev->m_nitems)
#define Mitems	(Ev->m_mitems)
#define Items	(Ev->m_items)
#define Kind	(Ev->m_kind)
#define Move	(Ev->m_move)
#define Char0	(Ev->m_char0)
#define Active	(Ev->m_at)


/*
	Print an item
*/
#if __STD_C
static void pr_item(WINDOW* win, int y, int x, int kind, char* it, chtype bg)
#else
static void pr_item(win,y,x,kind,it,bg)
reg WINDOW	*win;
reg int		y, x, kind;
char		*it;
chtype		bg;
#endif
{
	reg int		wx, c;
	reg uchar	*item = (uchar*)it;

	x *= (win->_maxx+1);
	mvderwin(win,y,x);

	if(!(kind&(MENU_ICENTER|MENU_IRIGHT)))
	{	wx = 0;
		for(x = 0; x < Char0; ++x)
		{	if(*item == 0)
				break;
#if _MULTIBYTE
			if(_mbtrue && ISMBIT(*item))
			{	reg int	m, k, ty;
				ty = TYPE(*item);
				m  = cswidth[ty] + (ty == 0 ? 0 : 1);
				for(k = 0; *item && k < m; ++k, ++item)
					;
			}
			else
#endif
				item++;
		}
	}
	else if((wx = strdisplen((char*)item,-1,(int*)0,(int*)0)) >= win->_maxx)
		wx = 0;
	else if(kind&MENU_ICENTER)
		wx = (win->_maxx - wx)/2;
	else	wx = (win->_maxx - wx);

	wbkgd(win,(bg&~A_ATTRIBUTES)|(winch(win)&A_ATTRIBUTES));
	wbkgd(win,bg);
	werase(win);
	scrollok(win,FALSE);
	wmove(win,0,wx);
	while(wx < win->_maxx && *item != 0)
	{
#if _MULTIBYTE
		if(_mbtrue && ISMBIT(*item))
		{	c = TYPE(*item);
			if((wx += scrwidth[c]) > win->_maxx)
				break;
			c = cswidth[c] + (c == 0 ? 0 : 1);
			while(c-- > 0 && *item)
				waddch(win,*item++);
		}
		else
#endif
		{	if((c = *item++) != '\t' && iscntrl(c) && (wx+2) > win->_maxx)
				break;
			waddch(win,c);
			if(c == '\t')
				wx = win->_curx;
			else 	wx += iscntrl(c) ? 2 : 1;
		}
	}
	wsyncup(win);
}

/*
	Check a set of match for a good one
*/
#if __STD_C
static int chkmatch(int m, int* list, int n_list, char* active, int c, int stay)
#else
static int chkmatch(m,list,n_list,active,c,stay)
reg int		m;
int*		list;
int		n_list;
char*		active;
int		c;
int		stay;
#endif
{
	reg int	i, first;

	if(m < 0 || n_list < 0)
		return -1;
	m = first = -1;
	for(i = 0; i < n_list; ++i)
		if(!active || active[list[i]])
		{
			if(first < 0)
				first = list[i];
			if((stay && list[i] == c) || (m < 0 && list[i] > c))
				m = list[i];
		}
	return m >= 0 ? m : first;
}

/*
	Check mouse match for a good one
*/
#if __STD_C
static int chkmouse(int top, int bot, char* active, int px, int py )
#else
static int chkmouse(top,bot,active,px,py)
int 	top,bot,px,py;
char	*active;
#endif
{	reg int 	i,ix;

	/* check that mouse is in window */
	if( px < 0 || py < 0)
		return -1;

	/* adjust for menu centered in window */
	px -= Ev->m_mw->_begx - Ev->m_w->_begx;
	py -= Ev->m_mw->_begy - Ev->m_w->_begy;

	/* compute index of choice that the mouse is on */
	ix = px/(Length+1);
	ix = (px+1)%(Length+1)==0 ? -1 : ix;
	if( ix < 0 || px < 0 || py < 0) 	/* mouse not on a choice */
		return -1;

	i  = Row ? ix + py*NX : ix*NY + py;
	i += top;

	return (top <= i && i <= bot && (!active || active[i])) ? i : -1;
}

/*
	Set top/bot of a new page
*/
#if __STD_C
static void settopbot(int m, int* top, int* bot)
#else
static void settopbot(m,top,bot)
reg int	m, *top, *bot;
#endif
{
	/* move enough to get the item into view */
	if(m < *top)
	{	*top = m;
		*bot = *top + Mitems;
	}
	else if(m >= *bot)
	{	*bot = m+1;
		*top = *bot - Mitems;
	}
	if(*top + Mitems >= Nitems)
		*top = Nitems - Mitems;
	if(*top < 0)
		*top = 0;
	*bot = *top + (Mitems < Nitems ? Mitems : Nitems);
}

#if __STD_C
static void redisplay(int top, int bot)
#else
static void redisplay(top,bot)
reg int	top, bot;
#endif
{
	reg int	i, mx, my;
	chtype	bg;
	char	*s;

	werase(Mwin);
	for(i = top; i < bot; ++i)
	{
		/* display string */
		my = Row ? (i-top)/NX : (i-top)%NY;
		mx = Row ? (i-top)%NX : (i-top)/NY;
		bg = Bg;
		/* don't display inactive elt attr if elt is blank */
		if (Active && !Active[i])
		{	for (s = Items[i]; *s==' ' || *s=='\t'; ++s)
				;
			bg = (*s)? Ia: Bg;
		}
		pr_item(Cwin,my,mx,Kind,Items[i],bg);
	}
	if(Kind&MENU_MORE)
	{
		wmove(Win,0,0);
		waddch(Win,top <= 0 ? ' ' :
			   ((Kind&MENU_ROW) ? ACS_LARROW : ACS_UARROW));
		wmove(Win,Win->_maxy-1,Win->_maxx-1);
		waddch(Win,bot >= Nitems ? ' ' :
			   ((Kind&MENU_ROW) ? ACS_RARROW : ACS_DARROW));
	}
}

/*
	Change the system idea of the current active item
*/
#if __STD_C
int menufg(reg char* s, int* top, int* bot)
#else
int menufg(s,top,bot)
reg char	*s;
int		*top, *bot;
#endif
{
	int	n, *list, n_list;

#define XPOS(c)		(Row ? (c-Top)%NX : (c-Top)/NY)
#define YPOS(c)		(Row ? (c-Top)/NX : (c-Top)%NY)

	if(!s || !s[0])
		n = C;
	else
	{
		n = _wstrmatch(s,Items,Nitems,&list,&n_list);
		n = chkmatch(n,list,n_list,Active,C,TRUE);
		if(n < 0)
			return -1;
	}

	if(Nitems > Mitems && (n < Top || n >= Bot))
	{
		settopbot(n,&Top,&Bot);
		Oldtop = Top;
		redisplay(Top,Bot);
	}
	else
	{
		/* set to background old foreground item */
		if(Cwin->_bkgd == Fg)
			wbkgd(Cwin,Bg);
		wsyncup(Cwin);
	}

	/* set to foreground new item */
	Move = TRUE;
	X = XPOS(n);
	Y = YPOS(n);
	C = n;
	mvderwin(Cwin,Y,X*(Length+1));
	wbkgd(Cwin,(Fg&~A_ATTRIBUTES)|(winch(Cwin)&A_ATTRIBUTES));
	wbkgd(Cwin,Fg);
	wsyncup(Cwin);
	wmove(Mwin,Y,X*(Length+1));
	wsyncup(Mwin);
	wmove(Win,(Mwin->_begy-Win->_begy)+Mwin->_cury,
		  (Mwin->_begx-Win->_begx)+Mwin->_curx);
	*top = Top;
	*bot = Bot;
	return n;
}


/*
	The menu function
*/
#if __STD_C
int wmenu(WINDOW* win, char** items, char* active,
	int c, int top, int char0, chtype bg, chtype fg, chtype ia,
	int (*getuser)(WINDOW*,int,int,int,int,int*,char**),
	int kind)
#else
int wmenu(win,items,active,c,top,char0,bg,fg,ia,getuser,kind)
WINDOW	*win;
char	**items;
char	*active;
int	c, top, char0;
chtype	bg, fg, ia;
int	(*getuser)(), kind;
#endif
{
	reg int		x, y, i, mx, my, ny, nx, sx, sy;
	reg WINDOW	*mwin, *cwin;
	int		rv, oldtop, bot, page, newpage,
			nitems, mitems, maxlen, length, oldrepeat, repeat;
	bool		isdone, isrow, hasactive, userscrl;
	char		*input;
	EVMENU		ev, *savev;

	/* handy inline functions */
#define MV_BACK(k)	((k) == KEY_LEFT || (k) == KEY_UP)
#define MV_FORW(k)	((k) == KEY_RIGHT || (k) == KEY_DOWN)
#define LEGAL(c)	(c >= 0 && c < nitems)
#define ACTIVE(c)	(!active || active[c])
#define GOOD(c)		(LEGAL(c) && ACTIVE(c))
#define ITEM(x,y)	((isrow ? y*nx + x : x*ny + y) + top)
#define XMENU(c)	(isrow ? (c-top)%nx : (c-top)/ny)
#define YMENU(c)	(isrow ? (c-top)/nx : (c-top)%ny)
#define VALID(x,y,c)	(x>=0 && x<nx && y>=0 && y<ny && GOOD(c))
#define RLEN(y)	(mitems < nitems ? nx : \
		 (!isrow ? ((y > (nitems-1)%ny) ? nx-1 : nx) :\
		  ((!(nitems%nx) || y < ((nitems-1)/nx)) ? nx : nitems%nx)))
#define CLEN(x)	(mitems < nitems ? ny : \
		 ( isrow ? ((x > (nitems-1)%nx) ? ny-1 : ny) :\
		 ((!(nitems%ny) || x < ((nitems-1)/ny)) ? ny : nitems%ny)))

	/* compute max length of a menu item */
	hasactive = active ? FALSE : TRUE;
	maxlen = 0;
	for(nitems = 0; items[nitems] ; ++nitems)
	{
		x = strdisplen(items[nitems],-1,(int*)0,(int*)0);
		maxlen  = x > maxlen ? x : maxlen;
		if(ACTIVE(nitems))
			hasactive = TRUE;
	}

	/* nothing to do */
	if(nitems == 0 || (length = maxlen) == 0)
		return -1;

	/* set row indicator */
	isrow = (kind&MENU_ROW) ? TRUE : FALSE;

	/* compute sizes of menu matrix */
	sx = win->_maxx;
	sy = win->_maxy;
	for(i = 0; i < 2; i++ )
	{
		if(kind&MENU_MORE)
		{	/* reserve space for more indicators */
			if(sx >= length+2)
				sx -= 2;
			else	kind &= ~MENU_MORE;
		}

		/* can display only sx length */
		if(length > sx)
			length = sx;

		/* number of elts per row and column */
		if(isrow)
		{
			if((nx = (sx+1)/(length+1)) > nitems)
				nx = nitems;
			ny = (nitems + nx - 1)/nx;
			if(ny > sy)
				ny = sy;
		}
		else
		{
			ny = nitems < sy ? nitems : sy;
			nx = (nitems + sy - 1)/sy;
			if(nx > (sx+1)/(length+1))
				nx = (sx+1)/(length+1);
		}

		/* max number of items that can be displayed at one time */
		if((mitems = ny*nx) <= 0)
			return -1;

		/* everything fits */
		if(nitems <= mitems || !(kind&MENU_MORE) )
			break;

		/* forget the more indicators */
		kind &= ~MENU_MORE;
		sx = win->_maxx;
	}

	/* sizes of menu window */
	mx = (nx * length) + nx - 1;
	my = ny;

	/* top-left corner of menu window */
	x = (kind&MENU_MORE) ? 1 : 0;
	y = 0;
	if(kind&MENU_XCENTER)
		x += (sx-mx)/2;
	else if(kind&MENU_XRIGHT)
		x += sx-mx;
	if(kind&MENU_YCENTER)
		y = (sy-my)/2;
	else if(kind&MENU_YBOTTOM)
		y = sy-my;

	/* make room for the cursor if possible */
	if(x == 0 && sx > mx)
		x = 1;

	/* make new windows for menu and choice */
	rv = -2;
	if((mwin = derwin(win,my,mx,y,x)) != NIL(WINDOW*) )
		cwin = derwin(mwin,1,length,0,0);
	if(!mwin || !cwin)
		goto done;
	scrollok(cwin,FALSE);

	/* set foreground, background, inactive attributes */
	fg = ((chtype)' ') | (fg&A_ATTRIBUTES);
	bg = ((chtype)' ') | (bg&A_ATTRIBUTES);
	ia = ((chtype)' ') | (ia&A_ATTRIBUTES);
	wbkgd(cwin,bg);

	/* item to situate cursor at */
	c = c < 0 ? 0 : c >= nitems ? nitems-1 : c;

	/* set initial top/bot items in display */
	if(nitems <= mitems)
	{
		top = 0;
		bot = nitems;
	}
	else
	{
		if(top < 0 || top >= nitems || c < top || c >= top+mitems)
			top = c;
		if((bot = top + mitems) > nitems)
		{
			top = nitems - mitems;
			bot = nitems;
		}
	}

	/* location of current item */
	x = XMENU(c);
	y = YMENU(c);

	rv = -1;
	oldrepeat = repeat = 1;
	page = mitems;
	oldtop = -1;
	userscrl = FALSE;
	isdone = (!(kind&MENU_PICK) || !getuser || !hasactive);

	/* menu environment */
	savev = Ev;
	Ev = &ev;
	X = x;
	Y = y;
	C = c;
	NX = nx;
	NY = ny;
	Fg = fg;
	Bg = bg;
	Ia = ia;
	Mwin = mwin;
	Cwin = cwin;
	Win = win;
	Oldtop = oldtop;
	Top = top;
	Bot = bot;
	Row = isrow;
	Length = length;
	Items = items;
	Nitems = nitems;
	Mitems = mitems;
	Kind = kind;
	Char0 = char0;
	Active = active;

	while(1)
	{
		/* reset previous foreground item to background */
		if(cwin->_bkgd == fg)
			wbkgd(cwin,bg);
		wsyncup(cwin);

		c = ITEM(x,y);

		/* make current item valid */
		newpage = FALSE;
		while(hasactive && !VALID(x,y,c))
		{
			switch(rv)
			{
			case KEY_RIGHT :
				if(++x >= RLEN(y))
				{
					x = 0;
					if(++y >= CLEN(x))
						newpage = TRUE;
				}
				break;
			case KEY_DOWN :
				if(++y >= CLEN(x))
				{
					y = 0;
					if(++x >= RLEN(y))
						newpage = TRUE;
				}
				break;
			case KEY_LEFT :
				if(--x < 0)
				{
					if(--y < 0)
						newpage = TRUE;
					else
						x = RLEN(y)-1;
				}
				break;
			case KEY_UP :
				if(--y < 0)
				{
					if(--x < 0)
						newpage = TRUE;
					else
						y = CLEN(x)-1;
				}
				break;
			default :
				if (isrow)
					rv = KEY_RIGHT;
				else
					rv = KEY_DOWN;
				break;
			}
			if (newpage)
				break;
			c = ITEM(x,y);
		}

		c = (c < 0)? (Nitems-1): ((c >= Nitems)? 0: c);
			
		if(MV_FORW(rv) && Active && !Active[c])
		{	/* current item must be active */
			i = c;
			while((c = (c += 1) >= Nitems ? 0 : c) != i)
				if(Active[c])
					break;
			if(c == i)
				return -2;
		}

		if(MV_BACK(rv) && Active && !Active[c])
		{	/* current item must be active */
			i = c;
			while((c = (c -= 1) < 0 ? (Nitems-1) : c) != i)
				if(Active[c])
					break;
			if(c == i)
				return -2;
		}

		if(c < top || c >= bot)
			settopbot(c,&top,&bot);
		if(top != oldtop)
		{
			oldtop = top;
			redisplay(top,bot);
		}

		x = XMENU(c);
		y = YMENU(c);

		/* highlight current item if appropriate */
		if(hasactive && (!isdone || !(kind&MENU_BKGD)))
		{
			mvderwin(cwin,y,x*(length+1));
			wbkgd(cwin,(fg&~A_ATTRIBUTES)|(winch(cwin)&A_ATTRIBUTES));
			wbkgd(cwin,fg);
			wsyncup(cwin);
			wmove(mwin,y,x*(length+1));
		}

		wsyncup(mwin);
		sy = (mwin->_begy-win->_begy) + mwin->_cury;
		sx = (mwin->_begx-win->_begx) + mwin->_curx;
		wmove(win,sy,sx > 0 ? sx-1 : 0);

		/* all done */
		if(isdone)
			goto done;

	getinput:
		input = NIL(char*);
		if(repeat)
		{
			oldrepeat = repeat;
			repeat = 0;
		}

		/* set current environment */
		Ev = &ev;
		X = x, Y = y, C = c, Oldtop = oldtop, Top = top, Bot = bot;
		Move = FALSE;
		rv = (*getuser)(win,c,top,bot,char0,&repeat,&input);
		Ev = &ev;
		if(C != c)	/* if user changed idea of current item */
			c = C, x = X, y = Y, oldtop = Oldtop, top = Top, bot = Bot;

		/* accept state, string input, or mouse event */
		if( rv == 0 || rv == -1 || rv == MOUSE_SEL || rv == MOUSE_HELP )
		{
			int n_list, *list, matching;
			int mustleave = (rv == -1) ? TRUE : FALSE;
			reg int mv=rv;

			rv = -1;
			if( mv == MOUSE_SEL || mv == MOUSE_HELP )	/* mouse */
			{	int 	px, py;		/* mouse pointer coordinates */
				matching = FALSE;
				wmouse_position( win, &py, &px );
				if( py>=0 && px>=0 && (rv=chkmouse(top,bot,active,px,py))>=0 )
				{	c = rv;
					matching = TRUE;
				}
				else
				{
					beep();
					goto getinput;
				}
			}
			else if(!input || !input[0]) 	/* accept */
			{
				matching = TRUE;
				rv = c;
			}
			else 						/* type-in */
			{
				matching = FALSE;
				repeat = oldrepeat;

				rv = _wstrmatch(input,items,nitems,&list,&n_list);
				rv = chkmatch(rv,list,n_list,active,c,Move);
				if(rv >= 0 &&
				   (n_list == 1 || (rv == c && !(kind&MENU_UNIQ))))
					matching = TRUE;
				if(rv < 0 && !mustleave)
				{
					beep();
					goto getinput;
				}
			}

			/* check for success */
			if(mustleave || (LEGAL(rv) && matching))
				isdone = TRUE;

			/* moving to a new item, reset the top */
			if(LEGAL(rv))
			{
				if(nitems > mitems && (rv < top || rv >= bot))
					settopbot(rv,&top,&bot);
			}
			else if(isdone)
				rv = c;

			x = XMENU(rv);
			y = YMENU(rv);

			/* skip movement code */
			continue;
		}

		if( rv == MOUSE_END )
			goto done;

		if( rv == KEY_MOUSE )	/* mouse event outside menu window */
		{	/* LATER:  check if on window's scroll bar */

			/* ignore and skip movement code */
			continue;
		}

		/* moving off page */
		if((MV_BACK(rv) && c == top) || (MV_FORW(rv) && c == (bot-1)))
		{
			c += MV_BACK(rv) ? -1 : 1;
			if(c < 0)
				c = nitems-1;
			else if(c >= nitems)
				c = 0;
			settopbot(c,&top,&bot);
			x = XMENU(c);
			y = YMENU(c);
			continue;
		}

		switch(rv)
		{
		case KEY_HOME :
			settopbot(0,&top,&bot);
			x = y = 0;
			rv = KEY_RIGHT;
			continue;
		case KEY_LEFT :
			if(!repeat)
				repeat = oldrepeat;
			x -= repeat;
			continue;
		case KEY_RIGHT :
			if(!repeat)
				repeat = oldrepeat;
			x += repeat;
			continue;
		case KEY_UP :
			if(!repeat)
				repeat = oldrepeat;
			y -= repeat;
			continue;
		case KEY_DOWN :
			if(!repeat)
				repeat = oldrepeat;
			y += repeat;
			continue;
		case KEY_NPAGE :
		case KEY_PPAGE :
			if(nitems <= mitems)
				continue;
			if(repeat > 0)
			{
				userscrl = TRUE;
				page = repeat;
				repeat = oldrepeat;
			}
			break;
		case KEY_SLEFT :
			if((char0 -= length) < 0)
				char0 = 0;
			Char0 = char0;
			redisplay(top,bot);
			continue;
		case KEY_SRIGHT :
			if((char0 += length) >= maxlen)
				char0 -= length;
			Char0 = char0;
			redisplay(top,bot);
			continue;
		default :
			beep();
			goto getinput;
		}

		/* this code can only be reached by a paging request */
		top += rv == KEY_NPAGE ? page : -page;
		if((top+mitems) > nitems)
			top = nitems-mitems; 
		if(top < 0)
			top = 0;
		bot = top + (mitems < nitems ? mitems : nitems);
		x = y = 0;
	} 

done :
	Ev = savev;
	if(cwin)
		delwin(cwin);
	if(mwin)
		delwin(mwin);

	if(!(kind&MENU_PICK) && win->_immed)
		wrefresh(win);

	return rv;
}
