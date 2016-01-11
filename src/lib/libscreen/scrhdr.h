#ifndef _SCRHDR_H
#define _SCRHDR_H	1

#include	"termhdr.h"
#include	"curses.h"
#include	<signal.h>


/*	Data private to the screen library routines
**
**	Written by Kiem-Phong Vo
*/

#define DUMP_MAGIC_NUMBER	(MAGNUM+1)

#define LARGECOST	4096	/* very costly termcap */

#define _NOHASH		(unsigned long)(-1)	/* if the hash value is unknown */
#define _REDRAW		(-2)	/* if line need redrawn */
#define	_INFINITY	(32000)	/* no line can be longer than this */
#define _BLANK		(-3)	/* if line is blank */

#define CHARSIZE	(8)	/* minsize of a byte */


/* short-hand notation */
#define SLK_MAP		struct _slk_st
#define TC_COST		struct _tc_cost


/*
**	Soft label keys
*/
#define	LABMAX	16	/* max number of labels allowed */
#define LABLEN	16	/* max length of each label */
struct	_slk_st
{
	WINDOW	*_win;		/* the window to display labels */
	char	_ldis[LABMAX][LABLEN+1];/* labels suitable to display */
	char	_lval[LABMAX][LABLEN+1];/* labels' true values */
	short	_labx[LABMAX];	/* where to display labels */
	short	_num;		/* actual number of labels */
	short	_len;		/* real length of labels */
	bool	_changed;	/* TRUE if some labels changed */
	bool	_lch[LABMAX];	/* change status */
};

/*
**	Costs of various termcap entries
*/
struct	_tc_cost
{
	short	_c_cup,	/* costs of caps that we use */
		_c_home,
		_c_ll,
		_c_cud1,
		_c_cuu1,
		_c_cuf1,
		_c_cub1,
		_c_cud,
		_c_cuu,
		_c_cuf,
		_c_cub,
		_c_ht,
		_c_cbt,
		_c_cr,
		_c_hpa,
		_c_vpa,
		_c_dcfix,
		_c_dch1,
		_c_dch,
		_c_icfix,
		_c_ich1,
		_c_ich,
		_c_el,
		_c_el1;
};



/*
**	Screen structure
*/
struct screen
{
	TERMINAL *_term;	/* terminal infos */

	WINDOW	*_curscr;	/* screen image */
	WINDOW	*_stdscr;	/* for user to use */
	uchar	**_mks;		/* marks, only used with xhp terminals */

	FILE	*_infp;		/* input file pointer */
	FILE	*_outfp;	/* output file pointer */

	TC_COST	_tc;		/* costs of termcaps */

	SLK_MAP	*_slk;		/* soft label keys, if any */

	WINDOW	*_vrtscr;	/* for doupdate/wnoutrefresh */
	chtype	*_vhash;	/* hash table of virtscr */
	chtype	*_chash;	/* hash table of curscr */

	short	_lines;		/* number of lines available */
	short	_cols;		/* number of columns available */
	short	_tabsiz;	/* tab size */
	short	_yoffset;	/* current offset from the top */

	bool	_keypad,	/* keypad enable */
		_meta,		/* meta mode enable */
		_endscr,	/* did endwin() on it */
		_dch,		/* can delete chars */
		_ich,		/* can insert chars */
		_dmd,		/* has enter delete mode */
		_imd,		/* has enter insert mode */
		_sidmd,		/* enter insert/delete modes equal */
		_eidmd,		/* exit insert/delete modes equal */
		_insrt;		/* in insert mode */

	char	_cursor;	/* 0: invis, 1:normal, 2:very visible */

#ifndef _SFIO_H
	uchar	*_buf,		/* we have to do our own buffering because */
		*_end,		/* stdio is too stupid to handle interruption */
		*_ptr;		/* caused by signals */
#endif
};


/* min/max functions */
#define	MIN(a,b)	((a) < (b) ? (a) : (b))
#define	MAX(a,b)	((a) > (b) ? (a) : (b))


/* character functions */
#define BLNKCHAR	(040)
#define DARKCHAR(c)	((c) != BLNKCHAR)
#define CHAR(c)		((c) & A_CHARTEXT)
#define ATTR(c)		((c) & A_ATTRIBUTES)
#define WCHAR(w,c)	(CHAR((c) == BLNKCHAR ? (w)->_bkgd : (c))|(w)->_attrs)

#define	FULLWIN(w)	(w->_begy == 0 && w->_yoffset == 0 && \
			 w->_begx == 0 && \
			 w->_maxy >= curscr->_maxy && \
			 w->_maxx >= curscr->_maxx)

/* cursor state */
#define _cursstate	(curscreen->_cursor)

/* hash tables for line comparisons  */
#define _virthash	(curscreen->_vhash)
#define _curhash	(curscreen->_chash)

/* top/bottom lines that changed in virtscr */
#define _virttop	_virtscr->_parx
#define _virtbot	_virtscr->_pary

/* blank lines info of curscr */
#define	_begns		curscr->_firstch
#define _endns		curscr->_lastch

/* video marks */
#define _marks		curscreen->_mks

/* current offset from the top of the screen */
#define _curoffset	curscreen->_yoffset

/* terminal dependent things */
#define _LINES		curscreen->_lines
#define _COLS		curscreen->_cols
#define _TABSIZ		curscreen->_tabsiz
#define	_endwin		curscreen->_endscr
#define _input		curscreen->_infp
#define _output		curscreen->_outfp

/* insert/delete chars booleans */
#define _dchok		(curscreen->_dch)
#define _ichok		(curscreen->_ich)
#define _dmode		(curscreen->_dmd)
#define _imode		(curscreen->_imd)
#define _sidsame	(curscreen->_sidmd)
#define _eidsame	(curscreen->_eidmd)
#define _insert		(curscreen->_insrt)
#define _offinsert()	(_puts(T_rmir), _insert = FALSE)
#define _oninsert()	(_puts(T_smir), _insert = TRUE)

/* short-hand notations for the cost structure */
#define C_cup	curscreen->_tc._c_cup
#define C_home	curscreen->_tc._c_home
#define C_ll	curscreen->_tc._c_ll
#define C_cud1	curscreen->_tc._c_cud1
#define C_cuu1	curscreen->_tc._c_cuu1
#define C_cuf1	curscreen->_tc._c_cuf1
#define C_cub1	curscreen->_tc._c_cub1
#define C_cud	curscreen->_tc._c_cud
#define C_cuu	curscreen->_tc._c_cuu
#define C_cuf	curscreen->_tc._c_cuf
#define C_cub	curscreen->_tc._c_cub
#define C_ht	curscreen->_tc._c_ht
#define C_cbt	curscreen->_tc._c_cbt
#define C_cr	curscreen->_tc._c_cr
#define C_hpa	curscreen->_tc._c_hpa
#define C_vpa	curscreen->_tc._c_vpa
#define C_dcfix curscreen->_tc._c_dcfix
#define C_dch1	curscreen->_tc._c_dch1
#define C_dch	curscreen->_tc._c_dch
#define C_icfix curscreen->_tc._c_icfix
#define C_ich1	curscreen->_tc._c_ich1
#define C_ich	curscreen->_tc._c_ich
#define C_el	curscreen->_tc._c_el
#define C_el1	curscreen->_tc._c_el1

/* to do soft labels */
#define _curslk		curscreen->_slk

/* functions to use with termcap */
#define _puts(s)	tputs(s,1,_putbyte)
#define _vids(na,oa)	(vidupdate((int)(na),(int)(oa),_putbyte),curscr->_attrs = na)

/* functions to output to terminal */
#ifdef _SFIO_H
#define _putc(c)	sfputc(_output,c)
#define _tflush()	sfsync(_output)
#else
#define _curbuf		(curscreen->_buf)
#define _bptr		(curscreen->_ptr)
#define _bend		(curscreen->_end)
#define _putc(c)	((_bptr == _bend ? _tflush() : 0), *_bptr = (uchar)(c), \
			 _bptr += 1, (int)(c))
#endif

/* type of a signal handler */
typedef void(*Signal_t)_ARG_((int));

_BEGIN_EXTERNS_

#if _MULTIBYTE
extern short	_csmax,
		_scrmax;
extern bool	_mbtrue;
#endif

extern SCREEN	*_origscreen;
extern bool	_wasstopped;
extern WINDOW	*_virtscr;

#if !_hdr_unistd
extern char*	ttyname _ARG_((int));
#endif

extern int	(*_setidln)_ARG_((int,int));
extern int	(*_useidln)_ARG_(());
extern int	_insshift _ARG_((WINDOW*,int));
extern WINDOW*	_makenew _ARG_((int,int,int,int));
extern int	_image _ARG_((WINDOW*));
extern WINDOW*	_padref _ARG_((WINDOW*,int,int,int,int,int,int));
extern int	_sscanw _ARG_((WINDOW*, char*, va_list));
extern int	_putchar _ARG_((chtype));
extern int	_putbyte _ARG_((int));
#ifndef _tflush
extern int	_tflush _ARG_(());
#endif

extern Signal_t	signal _ARG_((int,Signal_t));

extern int	_slk_update _ARG_(());
extern int	(*_do_rip_init)_ARG_(());
extern int	(*_do_slk_init)_ARG_(());
extern int	(*_do_slk_ref)_ARG_(());
extern int	(*_do_slk_noref)_ARG_(());
extern int	(*_do_slk_tch)_ARG_(());

#if _MULTIBYTE
extern int	_mbclrch _ARG_((WINDOW*,int,int));
extern int	_mbvalid _ARG_((WINDOW*));
extern int	_mbaddch _ARG_((WINDOW*,chtype,chtype));
#endif

#ifndef _SFIO_H
#ifndef __STDIO_H__ 	/* MIPS IRIX 5.1 */
_astimport int	vsprintf _ARG_((char*,char*,va_list));
#endif /* MIPS */
_astimport int	vsscanf _ARG_((char*,char*,va_list));
#endif

_END_EXTERNS_

#endif /*_SCRHDR_H*/
