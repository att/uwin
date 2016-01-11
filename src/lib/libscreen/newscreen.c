#include	"scrhdr.h"

/*
**	Start a new terminal structure
**	name: terminal type
**	lsiz, csiz, tabsiz: physical sizes
**	infp, outfp: input and output streams
**
**	Written by Kiem-Phong Vo
*/


/*
	Get real cost of a termcap string. The cost is usually
	parametrized by delays which is dependent on output speed.
*/
static int	_cost;
#if __STD_C
static int _count(int c)
#else
static int _count(c)
int	c;
#endif
{
	c = 0;
	return ++_cost;
}

#if __STD_C
static int _tcost(char* cp)
#else
static int _tcost(cp)
char	*cp;
#endif
{
	if(!cp)
		return LARGECOST;
	_cost = 0;
	tputs(cp,1,_count);
	return _cost;
}

static int _tccost()
{
	reg bool	ca;

	/* see if terminal can cursor address */
	ca = (T_cup ||
	        (T_vpa && T_hpa) ||
	        ((T_cuu1 || T_cuu) && (T_cud1 || T_cud) &&
		 (T_cuf1 || T_cuf) && (T_cub1 || T_cub)) ||
	        (T_home &&
		 (T_cuf1 || T_cuf || T_hpa) && (T_cud1 || T_cud || T_vpa)) ||
	        (T_ll &&
		 (T_cuf1 || T_cuf || T_hpa) && (T_cuu1 || T_cuu || T_vpa)));
	if(!ca)
		return ERR;

	/* set movement costs */
	C_cup  = _tcost(tstring(T_cup,10,10));
	C_vpa  = _tcost(tstring(T_vpa,10,-1));
	C_hpa  = _tcost(tstring(T_hpa,10,-1));
	C_cud  = _tcost(tstring(T_cud,10,-1));
	C_cuu  = _tcost(tstring(T_cuu,10,-1));
	C_cub  = _tcost(tstring(T_cub,10,-1));
	C_cuf  = _tcost(tstring(T_cuf,10,-1));
	C_cud1 = _tcost(T_cud1);
	C_cuu1 = _tcost(T_cuu1);
	C_cub1 = _tcost(T_cub1);
	C_cuf1 = _tcost(T_cuf1);
	C_ht   = _tcost(T_ht);
	C_cbt  = _tcost(T_cbt);
	C_cr   = _tcost(T_cr);
	C_home = _tcost(T_home);
	C_ll   = _tcost(T_ll);
	C_el   = _tcost(T_el);
	C_el1  = _tcost(T_el1);

	_dmode = (T_smdc && T_rmdc);
	_imode = (T_smir && T_rmir);
	_dchok = (T_dch1 || T_dch);
	_ichok = (_imode || T_ich || T_ich1);

	_sidsame = (_dmode && _imode && strcmp(T_smdc,T_smir) == 0);
	_eidsame = (_dmode && _imode && strcmp(T_rmdc,T_rmir) == 0);

	C_icfix = _imode ? _tcost(T_smir) + _tcost(T_rmir) : 0;
	C_ich  = _tcost(tstring(T_ich,10,-1));
	C_ich1 = _tcost(T_ich1);

	C_dcfix = _dmode ? _tcost(T_smdc) + _tcost(T_rmdc) : 0;
	C_dch1 = _tcost(T_dch1);
	C_dch  = _tcost(tstring(T_dch,10,-1));

	return OK;
}


#if __STD_C
SCREEN	*newscreen(char* name, int lsiz, int csiz, int tabsiz, FILE* outfp, FILE* infp)
#else
SCREEN	*newscreen(name,lsiz,csiz,tabsiz,outfp,infp)
char	*name;
int	lsiz, csiz, tabsiz;
FILE	*outfp, *infp;
#endif
{
	reg int		i, k, nc;
	reg uchar	**marks;
	reg SCREEN	*old, *nscr;

	/* make new entry */
	old = curscreen;
	if(!(nscr = (SCREEN*)malloc(sizeof(SCREEN))) )
		return NIL(SCREEN*);
	else
	{	curscreen = nscr;
		nscr->_term = NIL(TERMINAL*);
		nscr->_curscr = NIL(WINDOW*);
		nscr->_stdscr = NIL(WINDOW*);
		nscr->_mks = NIL(uchar**);

		nscr->_infp = NIL(FILE*);
		nscr->_outfp = NIL(FILE*);
		nscr->_slk = NIL(SLK_MAP*);

		nscr->_vrtscr = NIL(WINDOW*);
		nscr->_vhash = NIL(chtype*);
		nscr->_chash = NIL(chtype*);

		nscr->_lines = 0;
		nscr->_cols = 0;
		nscr->_tabsiz = 0;
		nscr->_yoffset = 0;

		nscr->_keypad = 0;
		nscr->_meta = 0;
		nscr->_endscr = 0;
		nscr->_dch = 0;
		nscr->_ich = 0;
		nscr->_dmd = 0;
		nscr->_imd = 0;
		nscr->_sidmd = 0;
		nscr->_eidmd = 0;
		nscr->_insrt = 0;
		nscr->_cursor = 0;
		
#ifndef _SFIO_H
		nscr->_buf = nscr->_end = nscr->_ptr = NIL(uchar*);
#endif
	}

	_curoffset = 0;
	_cursstate = 0;
	_dchok = _ichok = _dmode = _imode = _sidsame = _eidsame = _insert = FALSE;

	/* terminal state is normal */
	_endwin = TRUE;
	_cursstate = 1;

	/* get termcap table and current terminal line discipline */
#ifdef	TERMID
	_idinput = fileno(infp);
#endif
	if(!(curscreen->_term = setupterm(name,fileno(outfp),NIL(int*))) )
		goto err;

#if _MULTIBYTE
	/* the max length of a multi-byte character */
	_csmax = (cswidth[0] > cswidth[1]+1 ?
		  (cswidth[0] > cswidth[2]+1 ? cswidth[0] : cswidth[2]+1) :
		  (cswidth[1] > cswidth[2] ? cswidth[1]+1 : cswidth[2]+1));
	if(_csmax > CSMAX)
		goto err;

	/* the max length of a multi-column character */
	_scrmax = scrwidth[0] > scrwidth[1] ?
		  (scrwidth[0] > scrwidth[2] ? scrwidth[0] : scrwidth[2]) :
		  (scrwidth[1] > scrwidth[2] ? scrwidth[1] : scrwidth[2]);

	/* true multi-byte/multi-column case */
	_mbtrue = (_csmax > 1 || _scrmax > 1);
#endif

	/* various termcap costs */
	if(_tccost() == ERR)
		goto err;

	/* hardware sizes */
	_LINES = lsiz > 0 ? lsiz : T_lines;
	_COLS = csiz > 0 ? csiz : T_cols;
	_TABSIZ = tabsiz > 0 ? tabsiz : T_it;
	_LINES = _LINES <= 0 ? 24 : _LINES;
	_COLS = _COLS <= 0 ? 80 : _COLS;
	_TABSIZ = _TABSIZ <= 0 ? 8 : _TABSIZ;
	LINES = _LINES;
	COLS  = _COLS;
	TABSIZ = _TABSIZ;

	/* curscr, virtscr store what we think the screen looks like */
	if(!(curscreen->_curscr = newwin(LINES,COLS,0,0)) ||
	   !(curscreen->_vrtscr = newwin(LINES,COLS,0,0)) )
		goto err;

	/* so that refresh knows this screen is new */
	curscreen->_curscr->_sync = TRUE;
	curscreen->_vrtscr->_clear = FALSE;

	/* hash tables for line comparison */
	if(!(_virthash = (chtype *) malloc(2*LINES*sizeof(chtype))) )
		goto err;
	_curhash = _virthash + LINES;

	/* mark map for video attributes on cookie terminals */
	if(T_xmc < 0)
		_marks = NIL(uchar**);
	else
	{	if(!(marks = (uchar**)malloc(LINES*sizeof(uchar*))) )
			goto err;
		_marks = marks;
		nc = (COLS/CHARSIZE) + (COLS%CHARSIZE ? 1 : 0);
		for(i = LINES-1; i >= 0; --i, ++marks)
		{	if(!(*marks = (uchar*)malloc(nc*sizeof(uchar))) )
				goto err;
			for(k = nc-1; k >= 0; --k)
				marks[0][k] = 0;
		}
	}

	/* terminal io channels */
	_output = outfp;
	_input = infp;
	tinputfd(fileno(infp));
#ifndef _SFIO_H
	if(!(_curbuf = (uchar*)malloc(BUFSIZ)) )
		goto err;
	_bend = _curbuf + BUFSIZ;
	_bptr = _curbuf;
#endif

	/* so that slk_init and ripfuncs can do refresh */
	setcurscreen(nscr);

	/* take care of soft labels */
	_curslk = NIL(SLK_MAP*);
	if(_do_slk_init)
		(*_do_slk_init)();

	/* and rip-off lines */
	if(_do_rip_init)
		(*_do_rip_init)();

	/* remaining lines */
	_LINES = LINES;

	/* stdscr is for user to play with */
	if(!(curscreen->_stdscr = newwin(LINES,COLS,0,0)) )
		goto err;

	/* success */
	return setcurscreen(nscr);
err:
	delscreen(nscr);
	setcurscreen(old);
	return NIL(SCREEN*);
}
