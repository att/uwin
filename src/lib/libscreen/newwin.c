#include	"scrhdr.h"

/*
**	Make a new window
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
WINDOW *newwin(int nl, int nc, int by, int bx)
#else
WINDOW *newwin(nl,nc,by,bx)
int	nl, nc, by, bx;
#endif
{
	reg WINDOW	*win;
	reg chtype	*cp, *ep;
	reg int		i;

	if(nl <= 0)
		nl = LINES - by;
	if(nc <= 0)
		nc = COLS - bx;

	if(!(win = _makenew(nl,nc,by,bx)) )
		return NIL(WINDOW*);
	if(_image(win) == ERR)
		return NIL(WINDOW*);

	cp = win->_y[0];
	for(ep = cp+nc-1; ep >= cp; --ep)
		*ep = BLNKCHAR;
	for(i = 1; i < nl; i++)
		memcpy((char*)(win->_y[i]),(char*)cp,nc*sizeof(chtype));

	win->_yoffset = _curoffset;
	if(!curscr)
		win->_clear = TRUE;
	else	win->_clear = FULLWIN(win);

	return win;
}


/*
**	Prepare a new window image
*/
#if __STD_C
WINDOW	*_makenew(int nl, int nc, int by, int bx)
#else
WINDOW	*_makenew(nl,nc,by,bx)
int	nl, nc, by, bx;
#endif
{
	reg int		i;
	reg WINDOW	*win;
	reg short	*begch, *endch;

	if(!(win = (WINDOW*) malloc(sizeof(WINDOW))) )
		return NIL(WINDOW*);

	if(!(win->_y = (chtype **) malloc(nl*sizeof(chtype *))) )
	{	free((char*) win);
		return NIL(WINDOW*);
	}

	if(!(win->_firstch = (short *) malloc(2*nl*sizeof(short))) )
	{	free((char*)(win->_y));
		free((char*) win);
		return NIL(WINDOW*);
	}
	else	win->_lastch = win->_firstch + nl;
	begch = win->_firstch; endch = win->_lastch;
	for(i = 0; i < nl; i++, ++begch, ++endch)
	{	*begch = _INFINITY;
		*endch = -1;
	}

	win->_curx = win->_cury = 0;
	win->_maxy = nl;
	win->_maxx = nc;
	win->_begy = by;
	win->_begx = bx;
	win->_attrs = A_NORMAL;

	win->_clear = win->_leeve = win->_scroll = win->_idln = win->_keypad =
	win->_meta = win->_immed = win->_sync = win->_notimeout = FALSE;

	win->_tmarg = 0;
	win->_bmarg = nl-1;
	win->_pminy = win->_pminx =
	win->_sminy = win->_sminx =
	win->_smaxy = win->_smaxx = -1;

	win->_yoffset = 0;

	win->_bkgd = BLNKCHAR;

	win->_delay = -1;

	win->_ndescs = 0;
	win->_parx = win->_pary = -1;
	win->_parent = NIL(WINDOW*);

	win->_ybase = NIL(chtype*);

#if _MULTIBYTE
	win->_nbyte = -1;
	win->_index = 0;
	win->_insmode = FALSE;
#endif

	return win;
}


/*
**	Make the character image.
*/
#if __STD_C
int _image(WINDOW* win)
#else
int _image(win)
WINDOW	*win;
#endif
{
	reg int		i, nl, nc;

	nl = win->_maxy;
	nc = win->_maxx;
	if(!(win->_ybase = (chtype*) malloc(nc*nl*sizeof(chtype))) )
	{
		free((char*)(win->_firstch));
		free((char*)(win->_y));
		free((char*)win);
		return ERR;
	}
	win->_y[0] = win->_ybase;
	for(i = 1; i < nl; ++i)
		win->_y[i] = win->_y[i-1]+nc;

	return OK;
}
