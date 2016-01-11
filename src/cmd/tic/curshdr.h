/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/curshdr.h	1.10"

#define	_NOHASH		(-1)	/* if the hash value is unknown */
#define	_REDRAW		(-2)	/* if line need redrawn */
#define	_BLANK		(-3)	/* if line is blank */
#define	_THASH		(123)	/* base hash if clash with other hashes */
#define	_KEY		(01)
#define	_MACRO		(02)

#define	_INPUTPENDING	cur_term->_iwait
#define	_PUTS(x, y)	tputs(x, y, _outch)
#define	_VIDS(na, oa)	(vidupdate((na), (oa), _outch), curscr->_attrs = (na))
#define	_ONINSERT()	(_PUTS(enter_insert_mode, 1), SP->phys_irm = TRUE)
#define	_OFFINSERT()	(_PUTS(exit_insert_mode, 1), SP->phys_irm = FALSE)

/*
 * IC and IL overheads and costs should be set to this
 * value if the corresponding feature is missing
 */
#define	LARGECOST	500

typedef	struct
{
    short	icfixed;		/* Insert char fixed overhead */
    short	dcfixed;		/* Delete char fixed overhead */
    short	Insert_character;
    short	Delete_character;
    short	Cursor_home;
    short	Cursor_to_ll;
    short	Cursor_left;
    short	Cursor_right;
    short	Cursor_down;
    short	Cursor_up;
    short	Carriage_return;
    short	Tab;
    short	Back_tab;
    short	Clr_eol;
    short	Clr_bol;
    short	Parm_ich;
    short	Parm_dch;
    short	Parm_left_cursor;
    short	Parm_up_cursor;
    short	Parm_down_cursor;
    short	Parm_right_cursor;
    short	Cursor_address;
    short	Row_address;
} COSTS;

#define	_COST(field)	(SP->term_costs.field)

/* Soft label keys */

#define	LABMAX	16	/* max number of labels allowed */
#define	LABLEN	8	/* max length of each label */

typedef	struct
{
    WINDOW	*_win;		/* the window to display labels */
    char	_ldis[LABMAX][LABLEN+1];/* labels suitable to display */
    char	_lval[LABMAX][LABLEN+1];/* labels' true values */
    short	_labx[LABMAX];	/* where to display labels */
    short	_num;		/* actual number of labels */
    short	_len;		/* real length of labels */
    bool	_changed;	/* TRUE if some labels changed */
    bool	_lch[LABMAX];	/* change status */
} SLK_MAP;

#ifdef _UWIN
#   include      <termios.h>
    struct  _str_struct
    {
	struct _strs1   _s1;
	struct _strs2   _s2;
	struct _strs3   _s3;
    };
#endif
struct	screen
{
    unsigned	fl_echoit : 1;	/* in software echo mode */
    unsigned	fl_endwin : 2;	/* has called endwin */
    unsigned	fl_meta : 1;	/* in meta mode */
    unsigned	fl_nonl : 1;	/* do not xlate input \r-> \n */
    unsigned	yesidln : 1;	/* has idln capabilities */
    unsigned	dmode : 1;	/* Terminal has delete mode */
    unsigned	imode : 1;	/* Terminal has insert mode */
    unsigned	ichok : 1;	/* Terminal can insert characters */
    unsigned	dchok : 1;	/* Terminal can delete characters */
    unsigned	sid_equal : 1;	/* enter insert and delete mode equal */
    unsigned	eid_equal : 1;	/* exit insert and delete mode equal */
    unsigned	phys_irm : 1;	/* in insert mode or not */
    long	baud;		/* baud rate of this tty */
    short	kp_state;	/* 1 iff keypad is on, else 0 */
    short	Yabove;		/* How many lines are above stdscr */
    short	lsize;		/* How many lines decided by newscreen */
    short	csize;		/* How many columns decided by newscreen */
    short	tsize;		/* How big is a tab decided by newscreen */
    WINDOW	*std_scr;	/* primary output screen */
    WINDOW	*cur_scr;	/* what's physically on the screen */
    WINDOW	*virt_scr;	/* what's virtually on the screen */
    int		*cur_hash;	/* hash table of curscr */
    int		*virt_hash;	/* hash table of virtscr */
    TERMINAL	*tcap;		/* TERMINFO info */
    FILE	*term_file;	/* File to write on for output. */
    FILE	*input_file;	/* Where to get keyboard input */
    SLK_MAP	*slk;		/* Soft label information */
    char	**_mks;		/* marks, only used with xhp terminals */
    COSTS	term_costs;	/* costs of various capabilities */
    SGTTY	save_tty_buf;	/* saved state of this tty */
    char	**_color_mks;	/* marks, only used with color xhp terminals */
    unsigned long  _trap_mbe;		/* trap these mouse button events    */
    unsigned long  _map_mbe_to_key;	/* map selected buttons on top of    */
					/* slk's to function keys */
};

extern	SCREEN	*SP;
extern	WINDOW	*_virtscr;

#ifdef	DEBUG
#ifndef	outf
extern	FILE	*outf;
#endif	/* outf */
#endif	/* DEBUG */

/* terminfo magic number */
#define	MAGNUM	0432

/* curses screen dump magic number */
#define	SVR2_DUMP_MAGIC_NUMBER	0433
#define	SVR3_DUMP_MAGIC_NUMBER	0434

/* Getting the baud rate is different on the two systems. */

#ifdef	SYSV
#define	_BR(x)	(x.c_cflag & CBAUD)
#include	<values.h>
#else	/* SYSV */
#define	BITSPERBYTE	8
#define	MAXINT		32767
#define	_BR(x)	(x.sg_ispeed)
#endif	/* SYSV */

#define	_BLNKCHAR	' '
#define	_CTRL(c)	(c | 0100)
#define	_ATTR(c)	((c) & A_ATTRIBUTES)
#define	_CHAR(c)	((c) & A_CHARTEXT)

/* combine CHAR par of the character with the attributes of the window.
   Two points: 1) If character is blank, usebackground instead
	       2) If character contains color, delete color from
		  window attribute.
*/
#define _WCHAR(w, c)    (_CHAR((c) == _BLNKCHAR ? (w)->_bkgd : (c))| \
				(((c) & A_COLOR) ? ((w)->_attrs & ~A_COLOR) : \
						   ((w)->_attrs)))

#define	_DARKCHAR(c)	((c) != _BLNKCHAR)
#define	_UNCTRL(c)	((c) ^ 0100)

/* blank lines info of curscr */
#define	_BEGNS		curscr->_firstch
#define	_ENDNS		curscr->_lastch

/* hash tables */
#define	_CURHASH	SP->cur_hash
#define _VIRTHASH	SP->virt_hash

/* top/bot line changed */
#define _VIRTTOP	_virtscr->_parx
#define _VIRTBOT	_virtscr->_pary

/* video marks */
#define	_MARKS		SP->_mks
#define	_COLOR_MARKS	SP->_color_mks

#define	_NUMELEMENTS(x)	(sizeof(x)/sizeof(x[0]))

#ifdef	_VR3_COMPAT_CODE
#define	_TO_OCHTYPE(x)		((_ochtype)(((x&A_ATTRIBUTES)>>9)|(x&0177)))
#define	_FROM_OCHTYPE(x)	((chtype) ((x&0177) | ((x&0177600)<<9)))
extern	void	(*_y16update)();
#endif	/* _VR3_COMPAT_CODE */

/* functions for screen updates */
extern	int	(*_setidln)(), (*_useidln)(), (*_quick_ptr)();

/* min/max functions */
#define	_MIN(a, b)	((a) < (b) ? (a) : (b))
#define	_MAX(a, b)	((a) > (b) ? (a) : (b))

extern	int	(*_do_slk_ref)(), (*_do_slk_tch)(), (*_do_slk_noref)();
extern	void	(*_slk_init)(), (*_rip_init)();
extern	WINDOW	*_makenew();
#ifndef _UWIN
extern	char	*getenv();

extern	char	*memset(), *realloc(), *malloc(), *calloc();
#endif
extern	void	memSset(), free(), perror(), exit(), _blast_keys(),
		_init_costs();
		
#ifndef _UWIN
#ifndef	memcpy
extern	char	*memcpy();
#endif	/* memcpy */
char	*strcpy(), *strcat();
unsigned	sleep();
#endif
