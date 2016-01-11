#ifndef	_CURSES_H	/* protection against multiple #includes */
#define _CURSES_H	1

/*
**	Header file for programs that use the screen library.
**	This is suitable for ansi-C and C++ compilation.
**	Written by Kiem-Phong Vo (12/10/91)
**	Color added by John J. Snyder (10/20/98)
*/

#include	<stdio.h>
#include	<ast/term.h>
#include	<unctrl.h>
#include	<ast_common.h>
#ifndef _ast_va_list
#   define _ast_va_list void*
#endif


/* short-hand notations */
typedef struct _win_st	WINDOW;
typedef	struct screen	SCREEN;

/* WINDOW structure */
struct _win_st
{
	short		_cury, _curx;	/* current cursor position */
	short		_maxy, _maxx;	/* sizes of window */
	short		_begy, _begx;	/* start coords relative to screen (0,0) */
	short		_acp;		/* current color pair */
	chtype		_attrs;		/* current attributes */

	bool		_clear,
			_leeve,		/* should be _leave except for MSVC */
			_scroll,
			_idln,
			_keypad,
			_meta,
			_immed,
			_sync,
			_notimeout;

	chtype		**_y;		/* char image */
	short		*_firstch;	/* first changes of lines */
	short		*_lastch;	/* last changes of lines */

	short		_tmarg, _bmarg;	/* scrolling region */
	short		_pminy, _pminx,	/* for prefresh and echochar */
			_sminy, _sminx,
			_smaxy, _smaxx;

	short		_yoffset;	/* actual begy is _begy+_yoffset */

	chtype		_bkgd;		/* background, normally blank */
	short		_bcp;		/* background color pair, normally black */

	int		_delay;		/* delay period on wgetch
					   0:  for nodelay
					   <0: for infinite delay
					   >0: delay time in units of millisec
					*/

	short		_ndescs;	/* number of descendants */
	short		_parx, _pary;	/* coords relative to parent (0,0) */
	WINDOW		*_parent;	/* the parent if this is a subwin */

	chtype		*_ybase;	/* base of char image */

#if _MULTIBYTE
	short		_nbyte;		/* number of bytes to come */
	short		_index;		/* index to store coming char */
	unsigned char	_waitc[CSMAX];	/* array to store partial m-width char */
	bool		_insmode;	/* TRUE for inserting, FALSE for adding */
#endif
};


#ifndef MENU_ROW
/* menu attributes */
#define MENU_ROW	000001	/* menu is a row */
#define MENU_PICK	000002	/* picking something from it */
#define MENU_UNIQ	000004	/* picking matching item if unique */

/* justification of menu inside the window */
#define MENU_XLEFT	000010	/* X dir left justified (default) */
#define MENU_XCENTER	000020	/* centered */
#define MENU_XRIGHT	000040	/* right-justified */
#define MENU_YTOP	000100	/* Y dir to be top justified */
#define MENU_YCENTER	000200	/* centered */
#define MENU_YBOTTOM	000400	/* bottom justified */

/* justification of menu items */
#define MENU_ILEFT	001000	/* left-justified items (default) */
#define MENU_ICENTER	002000	/* center-justified items */
#define MENU_IRIGHT	004000	/* right-justified items */

/* use indicator >, <, ^ and v if necessary */
#define MENU_MORE	010000

/* unhighlight foreground item when exit */
#define MENU_BKGD	020000
#endif /*MENU*/


#ifndef EDIT_DONE
/* edit commands */
#define EDIT_DONE	01000
#define EDIT_UP		(EDIT_DONE+1)
#define EDIT_DOWN	(EDIT_DONE+2)
#define EDIT_LEFT	(EDIT_DONE+3)
#define EDIT_RIGHT	(EDIT_DONE+4)
#define EDIT_BEGLINE	(EDIT_DONE+5)
#define EDIT_ENDLINE	(EDIT_DONE+6)
#define EDIT_BEGWORD	(EDIT_DONE+7)
#define EDIT_ENDWORD	(EDIT_DONE+8)
#define EDIT_TOLINE	(EDIT_DONE+9)
#define EDIT_TOCHAR	(EDIT_DONE+10)
#define EDIT_SEARCH	(EDIT_DONE+11)
#define EDIT_SCROLL	(EDIT_DONE+12)
#define EDIT_ADDRESS	(EDIT_DONE+13)
#define EDIT_DELCHAR	(EDIT_DONE+14)
#define EDIT_ERASE	(EDIT_DONE+15)
#define EDIT_DELWORD	(EDIT_DONE+16)
#define EDIT_DELLINE	(EDIT_DONE+17)
#define EDIT_JOIN	(EDIT_DONE+18)
#define EDIT_CLRLINE	(EDIT_DONE+19)
#define EDIT_INSSTR	(EDIT_DONE+20)
#define EDIT_UNDO	(EDIT_DONE+21)
#define EDIT_MARK	(EDIT_DONE+22)
#define EDIT_YANKLINE	(EDIT_DONE+23)
#define EDIT_YANKRECT	(EDIT_DONE+24)
#define EDIT_YANKSPAN	(EDIT_DONE+25)
#define EDIT_PUT	(EDIT_DONE+26)
#define EDIT_ASKLINE	(EDIT_DONE+27)
#define EDIT_ASKCHAR	(EDIT_DONE+28)
#define EDIT_ASKYANK	(EDIT_DONE+29)
#define EDIT_HILITE	(EDIT_DONE+30)
#define EDIT_ASKHILITE	(EDIT_DONE+31)
#define EDIT_REPLACE	(EDIT_DONE+32)

/* mouse commands */
#define MOUSE_SEL	(EDIT_DONE+41)
#define MOUSE_HELP	(EDIT_DONE+42)
#define MOUSE_END	(EDIT_DONE+43)
#define MOUSE_G_SEL 	(EDIT_DONE+44)
#define MOUSE_G_HELP	(EDIT_DONE+45)
#define MOUSE_G_END 	(EDIT_DONE+46)
#define EDIT_ASKMOUSE	(EDIT_DONE+47)

#define EDIT_MAXYANK	128	/* max number of yank buffers */

/* some commands can have arguments */
typedef struct _edit_arg_
{
	int	type;		/* usually 1 (0) for returning (not) argument.
				   Defining the yank buffer for yanks/puts */
	bool	empty;		/* TRUE if edit buffer is empty */
	short	page;		/* page number */
	short	len;		/* length of current text line */
	short	topln;		/* top line number */
	short	botln;		/* bot line number */
	short	curln;		/* current line number */
	short	curch;		/* current position in line */
	union
	{
		int	n;	/* an integer value */
		char	*s;	/* a string value */
		char	*(*f)_ARG_((char*));/* a function address */
	}	arg;
}	EDIT_ARG;

/* type of editing */
#define EDIT_VIEW	0001	/* view-only */
#define EDIT_ATTR	0002	/* has embedded attributes, \digit convention */
#define EDIT_INVIS	0004	/* invisible mode */
#define EDIT_TEXT	0010	/* gettext is a char string */
#define EDIT_PAGE	0020	/* top line must be on page boundary */
#define EDIT_BREAK	0040	/* auto-line-break at end-of-line */
#define EDIT_BOUND	0100	/* line is bounded */
#define EDIT_1PAGE	0200	/* Can't fill in more than window size */
#define EDIT_CURLINE	0400	/* always return the current line */

#endif /*EDIT*/

_BEGIN_EXTERNS_ /* public data */
#if _BLD_screen && defined(__EXPORT__)
#define extern  __EXPORT__
#endif
#if !_BLD_screen && defined(__IMPORT__)
#define extern  extern __IMPORT__
#endif

#if _BLD_curses || defined(CURSES)

extern char*	boolcodes[];
extern char*	boolnames[];
extern char*	numcodes[];
extern char*	numnames[];
extern char*	strcodes[];
extern char*	strnames[];

#if defined(CURSES)

#define _boolfnames	boolfnames
#define _numfnames	numfnames
#define _strfnames	strfnames

#endif

extern char*	boolfnames[];
extern char*	numfnames[];
extern char*	strfnames[];

#endif

extern SCREEN	*curscreen;	/* current active terminal */
extern WINDOW	*stdscr, *curscr;
extern int	LINES, COLS, TABSIZ, COLORS, COLOR_PAIRS;

#undef extern
_END_EXTERNS_

/* prototypes of available functions at the screen level */
_BEGIN_EXTERNS_
#if _BLD_screen && defined(__EXPORT__)
#define extern __EXPORT__
#endif
#if !_BLD_screen && defined(__IMPORT__) && defined(__EXPORT__)
#define extern  __IMPORT__
#endif

extern int	waddch _ARG_((WINDOW*, chtype));
extern int	waddnstr _ARG_((WINDOW*,const char*,int));
extern int	wbkgd _ARG_((WINDOW*,chtype));
extern int	wborder _ARG_((WINDOW*,
			chtype,chtype,chtype,chtype,chtype,chtype,chtype,chtype));
extern int	wclrtobot _ARG_((WINDOW*));
extern int	wclrtoeol _ARG_((WINDOW*));
extern int	wcursyncup _ARG_((WINDOW*));
extern int	wdelch _ARG_((WINDOW*));
extern int	wechochar _ARG_((WINDOW*,chtype));
extern int	werase _ARG_((WINDOW*));
extern int	wgetch _ARG_((WINDOW*));
extern int	wgetstr _ARG_((WINDOW*,char*));
extern int	winsch _ARG_((WINDOW*,chtype));
extern int	winsdelln _ARG_((WINDOW*,int));
extern int	winsnstr _ARG_((WINDOW*,char*,int));
extern int	wline _ARG_((WINDOW*,chtype,int,int));
extern int	wmove _ARG_((WINDOW*,int,int));
extern int	wrefresh _ARG_((WINDOW*));
extern int	wnoutrefresh _ARG_((WINDOW*));
extern int	wscrl _ARG_((WINDOW*,int));
extern int	wsetscrreg _ARG_((WINDOW*,int,int));
extern int	wsyncup _ARG_((WINDOW*));
extern int	wsyncdown _ARG_((WINDOW*));
extern int	wtouchln _ARG_((WINDOW*,int,int,int));
extern void	wmouse_position _ARG_((WINDOW*,int*,int*));
extern int	winnstr _ARG_((WINDOW*, char*, int));
extern int	winchnstr _ARG_((WINDOW*, chtype*, int));
extern int	waddchnstr _ARG_((WINDOW*, chtype*, int));

extern int	is_linetouched _ARG_((WINDOW*,int));
extern int	is_wintouched _ARG_((WINDOW*));

extern int	mvderwin _ARG_((WINDOW*,int,int));
extern int	mvprintw _ARG_((int,int,char*,...));
extern int	mvwprintw _ARG_((WINDOW*,int,int,char*,...));
extern int	mvscanw _ARG_((int,int,char*,...));
extern int	mvwscanw _ARG_((WINDOW*,int,int,char*,...));
extern int	mvwin _ARG_((WINDOW*,int,int));
extern int	printw _ARG_((char*,...));
extern int	wprintw _ARG_((WINDOW*,char*,...));
extern int	vwprintw _ARG_((WINDOW*, const char*, _ast_va_list));
extern int	scanw _ARG_((char*,...));
extern int	wscanw _ARG_((WINDOW*,char*,...));
extern int	wunview _ARG_((WINDOW*,int(*)(char*),int));
extern int	wview _ARG_((WINDOW*,int,
			int(*)_ARG_((char**,int)),
			int(*)_ARG_((WINDOW*,int,int)),int));

extern char*	wedit _ARG_((WINDOW*, int,
			int(*)_ARG_((WINDOW*,EDIT_ARG*)),
			char*, char*(*)_ARG_((void)),
			int(*)_ARG_((char*,int)),
			int, int, int, int, int));
extern int	wmenu _ARG_((WINDOW*, char**, char*, int, int ,int,
			chtype, chtype, chtype,
			int(*)_ARG_((WINDOW*,int,int,int,int,int*,char**)),
			int));
extern int	menufg _ARG_((char*,int*,int*));

extern WINDOW*	newwin _ARG_((int,int,int,int));
extern WINDOW*	derwin _ARG_((WINDOW*,int,int,int,int));
extern WINDOW*	dupwin _ARG_((WINDOW*));
extern WINDOW*	getwin _ARG_((FILE*));
extern int	putwin _ARG_((WINDOW*,FILE*));
extern int	delwin _ARG_((WINDOW*));
extern int	copywin _ARG_((WINDOW*,WINDOW*,int,int,int,int,int,int,int));
extern int	garbagedlines _ARG_((WINDOW*, int by, int nl));

extern WINDOW*	initscr _ARG_(());
extern int	mvcur _ARG_((int,int,int,int));
extern SCREEN*	newscreen _ARG_((char*,int,int,int,FILE*,FILE*));
extern SCREEN*	setcurscreen _ARG_((SCREEN*));
extern int	delscreen _ARG_((SCREEN*));
extern int	endwin _ARG_(());
extern int	scr_dump _ARG_((char*));
extern int	scr_init _ARG_((char*));
extern int	scr_restore _ARG_((char*));
extern int	scr_reset _ARG_((FILE*,int));

extern int	doupdate _ARG_(());
extern int	curs_set _ARG_((int));
extern int	setsyx _ARG_((int,int));
extern int	_getsyx _ARG_((int*,int*));
extern int	delay_output _ARG_((int));

extern int	pechochar _ARG_((WINDOW*, chtype));
extern int	pnoutrefresh _ARG_((WINDOW*,int,int,int,int,int,int));
extern int	prefresh _ARG_((WINDOW*,int,int,int,int,int,int));

extern int	idlok _ARG_((WINDOW*,int));
extern int	intrflush _ARG_((WINDOW*,int));
extern int	keypad _ARG_((WINDOW*,int));
extern int	meta _ARG_((WINDOW*,int));
extern int	nodelay _ARG_((WINDOW*,int));
extern int	ring _ARG_((int));
extern int	ripoffline _ARG_((int, int(*)(WINDOW*, int)));

extern int	slk_start _ARG_((int,int*));
extern chtype	slk_attroff _ARG_((chtype));
extern chtype	slk_attron _ARG_((chtype));
extern chtype	slk_attrset _ARG_((chtype));
extern int	slk_set _ARG_((int,char*,int));
extern char*	slk_label _ARG_((int));
extern int	slk_clear _ARG_(());
extern int	slk_noutrefresh _ARG_(());
extern int	slk_restore _ARG_(());
extern int	slk_refresh _ARG_(());
extern int	slk_touch _ARG_(());

extern bool	can_change_color _ARG_(());
extern int	color_content _ARG_((short,short*,short*,short*));
extern bool	has_colors _ARG_(());
extern int	init_color _ARG_((short,short,short,short));
extern int	init_pair _ARG_((short,short,short));
extern int	pair_content _ARG_((short,short*,short*));
extern void	set_color _ARG_((int,int(*)(int)));
extern int	start_color _ARG_(());
extern void	_rgb2hls _ARG_((short,short,short,short*,short*,short*));

extern int	strdisplen _ARG_((char*,int,int*,int*));
extern void	initmatch _ARG_((int,int));
extern void	tstp _ARG_((int));
extern int	_wstrmatch _ARG_((char*,char**,int,int**,int*));

#if _MULTIBYTE
extern int	waddnwstr _ARG_((WINDOW*,wchar_t*,int));
extern int	waddwch _ARG_((WINDOW*,chtype));
extern wchar_t	wgetwch _ARG_((WINDOW*));
extern int	wgetwstr _ARG_((WINDOW*, wchar_t*));
extern int	winsnwstr _ARG_((WINDOW*,wchar_t*,int));
extern int	winswch _ARG_((WINDOW*,chtype));
extern chtype	winwch _ARG_((WINDOW*));
extern char*	wmbinch _ARG_((WINDOW*,int,int));
extern int	wmbmove _ARG_((WINDOW*,int,int));
extern int	mbcharlen _ARG_((char*));
extern int	mbdisplen _ARG_((char*));
#endif

extern char*	keyname _ARG_((int));

#undef extern
_END_EXTERNS_

#if _MULTIBYTE
#define	waddwstr(w,pp) 		waddnwstr((w),(pp),-1)
#define	winswstr(w,pp)		winsnwstr((w),(pp),-1)

#define	mbmove(y,x)		wmbmove(stdscr,(y),(x))
#define mbinch(y,x)		wmbinch(stdscr,(y),(x))
#define inwch()			winwch(stdscr)

#define addwch(cd)		waddwch(stdscr,(cd))
#define inswch(cd)		winswch(stdscr,(cd))
#define getwch()		wgetwch(stdscr)
#define addnwstr(pp,n)		waddnwstr(stdscr,(pp),(n))
#define addwstr(pp)		waddwstr(stdscr,(pp))
#define insnwstr(pp,n)		winsnwstr(stdscr,(pp),(n))
#define inswstr(pp)		winswstr(stdscr,(pp))
#define getwstr(pp)		wgetwstr(stdscr,(pp))

#define mvwaddwch(w,y,x,cd)	(wmove((w),(y),(x))==ERR ? ERR:waddwch((w),(cd)))
#define mvwinswch(w,y,x,cd)	(wmove((w),(y),(x))==ERR ? ERR:winswch((w),(cd)))
#define mvwgetwch(w,y,x)	(wmove((w),(y),(x))==ERR ? ERR:wgetwch(w))
#define mvwinwch(w,y,x)		(wmove((w),(y),(x))==ERR ? ERR:winwch(w))
#define mvwaddnwstr(w,y,x,pp,n) (wmove((w),(y),(x))==ERR ? ERR:waddnwstr((w),(pp),(n)))
#define mvwinsnwstr(w,y,x,pp,n) (wmove((w),(y),(x))==ERR ? ERR:winsnwstr((w),(pp),(n)))
#define mvwaddwstr(w,y,x,pp)	(wmove((w),(y),(x))==ERR ? ERR:waddwstr((w),(pp)))
#define mvwinswstr(w,y,x,pp)	(wmove((w),(y),(x))==ERR ? ERR:winswstr((w),(pp)))
#define mvwgetwstr(w,y,x,pp)	(wmove((w),(y),(x))==ERR ? ERR:wgetwstr((w),(pp)))

#define mvaddwch(y,x,cd)	mvwaddwch(stdscr,(y),(x),(cd))
#define mvinswch(y,x,cd)	mvwinswch(stdscr,(y),(x),(cd))
#define mvgetwch(y,x)		mvwgetwch(stdscr,(y),(x))
#define mvinwch(y,x)		mvwinwch(stdscr,(y),(x))
#define mvaddnwstr(y,x,pp,n)	mvwaddnwstr(stdscr,(y),(x),(pp),-1)
#define mvaddwstr(y,x,pp)	mvwaddwstr(stdscr,(y),(x),(pp))
#define mvinsnwstr(y,x,pp,n)	mvwinsnwstr(stdscr,(y),(x),(pp),-1)
#define mvinswstr(y,x,pp)	mvwinswstr(stdscr,(y),(x),(pp))
#define mvgetwstr(y,x,pp)	mvwgetwstr(stdscr,(y),(x),(pp))
#endif

/* functions to get at the window structure */
#define	getyx(w,y,x)	((y)=(w)->_cury, (x)=(w)->_curx)
#define getsyx(y,x)	_getsyx(&(y),&(x))
#define	getbegyx(w,y,x)	((y)=(w)->_begy, (x)=(w)->_begx)
#define	getmaxyx(w,y,x)	((y)=(w)->_maxy, (x)=(w)->_maxx)
#define	getparyx(w,y,x)	((y)=(w)->_pary, (x)=(w)->_parx)
#define	winch(w)	((w)->_y[(w)->_cury][(w)->_curx])
#define	wincp(w)	(PAIR_NUMBER(winch(w)))

#define wbkgdset(w,c)	((w)->_bkgd = (c))
#define wgetbkgd(w)	((w)->_bkgd)

/* functions for handling video attributes */
#define wattroff(w,a)	((w)->_attrs &= ~(a), (w)->_attrs |= (w)->_bkgd,\
			 (w)->_attrs &= A_ATTRIBUTES)
#define wattron(w,a)	((w)->_attrs |= (a), (w)->_attrs &= A_ATTRIBUTES)
#define wattrset(w,a)	((w)->_attrs = ((a) | (w)->_bkgd) & A_ATTRIBUTES)

/* functions for upward compatibility */
#define	wstandout(w)		wattrset((w),A_STANDOUT)
#define	wstandend(w)		wattrset((w),A_NORMAL)
#define whline(w,h,n)		wline((w), (h), (n), 0)
#define wvline(w,v,n)		wline((w), (v), (n), 1)
#define box(w,v,h)		wborder((w),(v),(v),(h),(h),0,0,0,0)
#define overlay(w1,w2)		copywin((w1),(w2),-1,-1,-1,-1,-1,-1,TRUE)
#define overwrite(w1,w2)	copywin((w1),(w2),-1,-1,-1,-1,-1,-1,FALSE)
#define scroll(w)		wscrl((w),1)
#define	winsertln(w)		winsdelln((w),1)
#define	wdeleteln(w)		winsdelln((w),-1)
#define untouchwin(w)		wtouchln((w),0,(w)->_maxy,FALSE)
#define touchwin(w)		wtouchln((w),0,(w)->_maxy,TRUE)
#define touchline(w,y,n)	wtouchln((w),(y),(n),TRUE)
#define newterm(type,fout,fin)	newscreen((type),0,0,0,(fout),(fin) )
#define set_term(new)		setcurscreen(new)
#define waddstr(w,sp)		waddnstr((w),(sp),-1)
#define winsstr(w,sp)		winsnstr((w),(sp),-1)
#define subwin(w,nl,nc,by,bx)	derwin((w),(nl),(nc),(by)-(w)->_begy,(bx)-(w)->_begx)
#define newpad(nl,nc)		newwin((nl),(nc),0,0)
#define subpad(w,nl,nc,by,bx)	derwin((w),(nl),(nc),(by),(bx))
#define slk_init(f)		slk_start((f),(int*)0)
#define traceon()		(OK)
#define traceoff()		(OK)
#define wclear(w)		((w)->_clear = TRUE, werase(w))
#define beep()			ring(TRUE)
#define flash()			ring(FALSE)

/* functions for setting time-out length on inputs */
#define wtimeout(w,tm)		((w)->_delay = (tm))

/* like winnstr but has no buffer size */
#define winstr(w,s)		winnstr((w),(s),-1)

/* pseudo functions for stdscr */
#define echochar(c)	wechochar(stdscr, (c))
#define timeout(tm)	wtimeout(stdscr, (tm))
#define setscrreg(t,b)	wsetscrreg(stdscr, (t), (b))
#define unview(wln,ty)	wunview(stdscr,(wln),(ty))
#define	inch()		winch(stdscr)
#define innstr(s,n)	winnstr(stdscr, (s), (n))
#define instr(s)	winstr(stdscr, (s))
#define	addch(c)	waddch(stdscr, (c))
#define	getch()		wgetch(stdscr)
#define	addstr(s)	waddstr(stdscr, (s))
#define	addnstr(s,n)	waddnstr(stdscr, (s), (n))
#define	insstr(s)	winsstr(stdscr, (s))
#define	insnstr(s,n)	winsnstr(stdscr, (s), (n))
#define	getstr(s)	wgetstr(stdscr,s)
#define	move(y,x)	wmove(stdscr, (y), (x))
#define	clear()		wclear(stdscr)
#define	erase()		werase(stdscr)
#define	clrtobot()	wclrtobot(stdscr)
#define	clrtoeol()	wclrtoeol(stdscr)
#define scrl(n)		wscrl(stdscr, (n))
#define insdelln(n)	winsdelln(stdscr, (n))
#define	insertln()	winsertln(stdscr)
#define	deleteln()	wdeleteln(stdscr)
#define	refresh()	wrefresh(stdscr)
#define	insch(c)	winsch(stdscr, (c))
#define	delch()		wdelch(stdscr)
#define standout()	wattron(stdscr,A_STANDOUT)
#define standend()	wattroff(stdscr,A_STANDOUT)
#define	attroff(a)	wattroff(stdscr, (a))
#define	attron(a)	wattron(stdscr, (a))
#define	attrset(a)	wattrset(stdscr, (a))
#define bkgd(c)		wbkgd(stdscr, (c))
#define bkgdset(c)	((stdscr)->_bkgd = (c))
#define getbkgd()	((stdscr)->_bkgd)
#define border(lc,rc,tc,bc,tl,tr,bl,br)	\
			wborder(stdscr,(lc),(rc),(tc),(bc),(tl),(tr),(bl),(br))
#define hline(h,n)	whline(stdscr, (h), (n))
#define vline(v,n)	wvline(stdscr, (v), (n))
#define inchnstr(ch,n)	winchnstr(stdscr, (ch), (n))
#define addchnstr(ch,n)	waddchnstr(stdscr, (ch), (n))

/* functions for move and update */
#define	mvwaddch(w,y,x,c)	(wmove((w),(y),(x))==ERR?ERR:waddch((w),(c)))
#define	mvwgetch(w,y,x)		(wmove((w),(y),(x))==ERR?ERR:wgetch(w))
#define	mvwaddstr(w,y,x,s)	(wmove((w),(y),(x))==ERR?ERR:waddstr((w),(s)))
#define	mvwaddnstr(w,y,x,s,n)	(wmove((w),(y),(x))==ERR?ERR:waddnstr((w),(s),(n)))
#define	mvwinsstr(w,y,x,s)	(wmove((w),(y),(x))==ERR?ERR:winsstr((w),(s)))
#define	mvwinsnstr(w,y,x,s,n)	(wmove((w),(y),(x))==ERR?ERR:winsnstr((w),(s),(n)))
#define	mvwgetstr(w,y,x,s)	(wmove((w),(y),(x))==ERR?ERR:wgetstr((w),(s)))
#define	mvwinch(w,y,x)		(wmove((w),(y),(x))==ERR?ERR:winch(w))
#define	mvwinnstr(w,y,x,s,n)	(wmove((w),(y),(x))==ERR?ERR:winnstr((w),(s),(n)))
#define	mvwinstr(w,y,x,s)	(wmove((w),(y),(x))==ERR?ERR:winstr((w),(s)))
#define	mvwdelch(w,y,x)		(wmove((w),(y),(x))==ERR?ERR:wdelch(w))
#define	mvwinsch(w,y,x,c)	(wmove((w),(y),(x))==ERR?ERR:winsch((w),(c)))
#define mvwline(w,y,x,c,n,v)	(wmove((w),(y),(x))==ERR?ERR:wline((w),(c),(n),(v)))
#define mvwvline(w,y,x,c,n)	(wmove((w),(y),(x))==ERR?ERR:wvline((w),(c),(n)))
#define mvwhline(w,y,x,c,n)	(wmove((w),(y),(x))==ERR?ERR:whline((w),(c),(n)))
#define mvwinchnstr(w,y,x,ch,n)	(wmove((w),(y),(x))==ERR?ERR:winchnstr((w), (ch), (n)) )
#define mvwaddchnstr(w,y,x,ch,n) (wmove((w),(y),(x))==ERR?ERR:waddchnstr((w), (ch), (n)) )

#define	mvaddch(y,x,c)		mvwaddch(stdscr,(y),(x),(c))
#define	mvgetch(y,x)		mvwgetch(stdscr,(y),(x))
#define	mvaddstr(y,x,s)		mvwaddstr(stdscr,(y),(x),(s))
#define	mvaddnstr(y,x,s,n)	mvwaddnstr(stdscr,(y),(x),(s),(n))
#define	mvinsstr(y,x,s)		mvwinsstr(stdscr,(y),(x),(s))
#define	mvinsnstr(y,x,s,n)	mvwinsnstr(stdscr,(y),(x),(s),(n))
#define	mvgetstr(y,x,s)		mvwgetstr(stdscr,(y),(x),(s))
#define	mvinch(y,x)		mvwinch(stdscr,(y),(x))
#define	mvinnstr(y,x,s,n)	mvwinnstr(stdscr,(y),(x),(s),(n))
#define	mvinstr(y,x,s)		mvwinstr(stdscr,(y),(x),(s))
#define	mvdelch(y,x)		mvwdelch(stdscr,(y),(x))
#define	mvinsch(y,x,c)		mvwinsch(stdscr,(y),(x),(c))
#define mvvline(y,x,c,n)	mvwvline(stdscr,(y),(x),(c),(n))
#define mvhline(y,x,c,n)	mvwhline(stdscr,(y),(x),(c),(n))
#define mvinchnstr(y,x,ch,n)	mvwinchnstr(stdscr,(y),(x),(ch),(n))
#define mvaddchnstr(y,x,ch,n)	mvwaddchnstr(stdscr,(y),(x),(ch),(n))

/* functions to define environment flags of a window */
#define	clearok(win,bf)		((win)->_clear = (bf))
#define	leaveok(win,bf)		((win)->_leeve = (bf))
#define	scrollok(win,bf)	((win)->_scroll = (bf))
#define immedok(win,bf)		((win)->_immed = (bf))
#define syncok(win,bf)		((win)->_sync = (bf))
#define notimeout(win,bf)	((win)->_notimeout = (bf))

#endif /* _CURSES_H */
