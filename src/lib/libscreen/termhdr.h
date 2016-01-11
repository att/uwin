#ifndef _TERMHDR_H
#define _TERMHDR_H	1
#define _BLD_screen	1

/* TCBASE defines the default path to the termcap database.
** TIBASE defines the default path to the terminfo database.
** Users can overide these paths by supplying environment
** variables TERMCAP and TERMINFO
*/
#define TCBASE	"/etc/termcap"
#define TIBASE	"/usr/lib/terminfo"

#include	<ast_common.h>
#include	"FEATURE/screen"
#include	<sys/types.h>

#if _PACKAGE_ast
#include	<ast.h>
#include	<ast_time.h>
#include	<ast_tty.h>
#endif /*_PACKAGE_ast*/

#if !_PACKAGE_ast
#include	<sys/time.h>
#include	<fcntl.h>
#if _hdr_unistd
#include	<unistd.h>
#endif
#if _hdr_stdlib
#include	<stdlib.h>
#endif
#if __STD_C
#include	<string.h>
#endif
#endif /*_PACKAGE_ast*/

#if !_hdr_termio && !_hdr_sgtty
#define SGTTYB  unknown_tty_struct_type
#else
#if _hdr_termio
#include	<termio.h>
#define SGTTYB	struct termio
#else
#include	<sgtty.h>
#define SGTTYB	struct sgttyb
#if _typ_tchars
#define TCHARS_STRUCT	struct tchars	Ochars, Nchars;
#endif
#if _typ_ltchars
#define LTCHARS_STRUCT	struct ltchars	Olchars, Nlchars;
#endif
#if _typ_tauxil
#define TAUXIL_STRUCT	struct tauxil	Otauxil, Ntauxil;
#endif
#endif /*_hdr_termio*/
#endif /*_hdr_termio*/

#ifndef TCHARS_STRUCT
#define TCHARS_STRUCT
#endif
#ifndef LTCHARS_STRUCT
#define LTCHARS_STRUCT
#endif
#ifndef TAUXIL_STRUCT
#define TAUXIL_STRUCT
#endif

#define _TERM_PRIVATE \
	SGTTYB		Ottyb, Nttyb;	/* original&current line disciplines	*/ \
	TCHARS_STRUCT \
	LTCHARS_STRUCT \
	TAUXIL_STRUCT

#include	"term.h"
#include	"term.short.h"

#if __STD_C
#include	<stdarg.h>
#else
#include	<varargs.h>
#endif

#include	<errno.h>

#if !_lib_memcpy && _lib_bcopy
#define memcpy(to,fr,n)	bcopy((fr),(to),(n))
#endif

#define uchar		unsigned char
#define reg		register
#define NIL(type)	((type)0)

#ifdef DEBUG
#define ASSERT(s)	if(!(s)) abort();
#else
#define ASSERT(s)
#endif

/* _MULTIBYTE macros */
#define CMASK		0377
#define MBIT		(1<<7)		/* indicator for a multi-byte char */
#if defined(_ast_int8_t) && defined(_CHARTYPE64) 
#   define CBIT		((_ast_int8_t)1LLU<<63)	/* indicator for a continuing col */
#else
#   define CBIT		((unsigned long)1L<<31)	/* indicator for a continuing col */
#endif  /* _ast_int8_t */
#define ISMBYTE(x)	((x) & ~0177)	/* a multibyte code */
#define RBYTE(x)	((x) & CMASK)
#define LBYTE(x)	(((x) >> 8) & CMASK)
#define ISMBIT(x)	((x) & MBIT)
#define SETMBIT(x)	((x) |= MBIT)
#define CLRMBIT(x)	((x) &= ~MBIT)
#define ISCBIT(x)	((x) & CBIT)
#define SETCBIT(x)	((x) |= CBIT)
#define CLRCBIT(x)	((x) &= ~CBIT)
#define TYPE(x)		((x) == SS2 ? 1 : (x) == SS3 ? 2 : ISMBIT(x) ? 0 : 3)


#define MAGNUM	(0432)	/* a magic number */


/* max size for a terminfo/termcap entry */
#define TCSIZE		3072

/* so we don't have to include <ctype.h> */
#define islower(x)	((x) >= 'a' && (x) <= 'z')
#define isupper(x)	((x) >= 'A' && (x) <= 'Z')
#define isalpha(x)	(islower(x) || isupper(x))
#define isdigit(x)	((x) >= '0' && (x) <= '9')
#define isalnum(x)	(isalpha(x) || isdigit(x) || (x) == '_')
#define iscntrl(x)	((x) < 040 || (x) == 0177)
#define isblank(x)	((x) == ' ' || (x) == '\t')
#define UNCTRL(c)	((c) == 0177 ? '?' : (c) == 036 ? '~' : (c)+'@')

/* short hand for tty attributes */
#define _AHEADPENDING	(-2)
#define _mickey		cur_term->_mouse
#define _inputbuf	cur_term->_ibuf
#define _inputbeg	cur_term->_ibeg
#define _inputcur	cur_term->_icur
#define _inputend	cur_term->_iend
#define _literal	cur_term->_ilit
#define _timeout	cur_term->_delay
#define _inputd		cur_term->_inpd
#define _ahead		cur_term->_ahd
#define _inputpending	cur_term->_iwait
#define _macroerr	cur_term->_mcerr
#define _inputeof	cur_term->_ieof
#define _outputd	cur_term->_outd
#define	_orgtty		cur_term->Ottyb
#define	_curtty		cur_term->Nttyb

#ifdef TCHARS
#define _orgchars	cur_term->Ochars
#define _curchars	cur_term->Nchars
#endif
#ifdef LTCHARS
#define _orglchars	cur_term->Olchars
#define _curlchars	cur_term->Nlchars
#endif
#ifdef TAUXIL
#define _orgtauxil	cur_term->Otauxil
#define _curtauxil	cur_term->Ntauxil
#endif

#define _baudrate	cur_term->_baud
#define _ospeed		cur_term->_speed
#define _erasech	cur_term->_erasec
#define _killch		cur_term->_killc
#define	_echoit		cur_term->_echo
#define _xtab		cur_term->_notab
#define	_rawmode	cur_term->_raw
#define	_nonlcr		cur_term->_nonl
#define _curattrs	cur_term->_attrs

/* key map */
#define _keymap		cur_term->_keys

/* short hand for comparisons of video ending sequences */
#define T_seue		cur_tc->_seue
#define T_seme		cur_tc->_seme
#define T_seae		cur_tc->_seae
#define T_ueme		cur_tc->_ueme
#define T_ueae		cur_tc->_ueae
#define T_meae		cur_tc->_meae
#define T_opae		cur_tc->_opae
#define T_opme		cur_tc->_opme
#define T_opse		cur_tc->_opse
#define T_opue		cur_tc->_opue

/* functions to actually get/set line discipline */
#if _hdr_termio
#define	GETTY(x)	ioctl(_outputd,TCGETA,&(x))
#define	SETTY(x)	ioctl(_outputd,TCSETAW,&(x))
#else
#define GETTY(x)	ioctl(_outputd,TIOCGETP,&(x))
#define SETTY(x)	ioctl(_outputd,TIOCSETN,&(x))
#endif

#ifdef TCHARS
#define GETCHARS(x)	ioctl(_outputd,TIOCGETC,&(x))
#define SETCHARS(x)	ioctl(_outputd,TIOCSETC,&(x))
#endif
#ifdef LTCHARS
#define GETLCHARS(x)	ioctl(_outputd,TIOCGLTC,&(x))
#define SETLCHARS(x)	ioctl(_outputd,TIOCSLTC,&(x))
#endif
#ifdef TAUXIL
#define GETTAUXIL(x)	ioctl(_outputd,TIOCGAUXC,&(x))
#define SETTAUXIL(x)	ioctl(_outputd,TIOCSAUXC,&(x))
#endif

_BEGIN_EXTERNS_

/* TERMID determines whether setupterm() will sense the terminal
** for its name if the name has not been determined by other means.
*/
#ifdef TERMID
extern int	_idinput;
#endif

#ifndef errno
_astimport int	errno;
#endif

/* space reserved for the first terminal */
extern TERMCAP		_f_tc;
extern TERMINAL		_f_term;
extern char		_f_strs[];
extern int		_f_unused;

/* baudrate table */
extern	unsigned short	_baudtab[];
extern int		_nbauds;

/* color stuff */
typedef struct _rgb_color_st	RGB_COLOR;
struct _rgb_color_st
{	
	short	r;		/* red   (or hue) */
	short	g;		/* green (or lightness) */
	short	b;		/* blue  (or saturation) */
};
typedef struct _pair_color_st	PAIR_COLOR;
struct _pair_color_st
{	
	short	f;		/* foreground color */
	short	b;		/* background color */
};
extern RGB_COLOR  *rgb_color_TAB;	/* color table */
extern PAIR_COLOR *color_pair_TAB;	/* color-pair table */
extern RGB_COLOR _rgb_bas_TAB[];	/* default basic rgb colors 0-7 */
extern RGB_COLOR _hls_bas_TAB[];	/* default basic hls colors 0-7 */

extern int	(*_delkey) _ARG_(());
extern int	(*_adjkey) _ARG_((KEY_MAP*));
extern int	_match _ARG_((char**,short*,int,char*));
extern char*	_matchname _ARG_((char*,char*));
extern int	_rdtimeout _ARG_((int, int, uchar*, int));
extern int	_tty_mode _ARG_((SGTTYB*));
extern int	_delay _ARG_((int,int(*)(int)));
extern int	_tiread _ARG_((TERMCAP*, char*));
extern int	_tcread _ARG_((TERMCAP*, char*));
extern TERMCAP*	_settc _ARG_((char*));
extern void	_deltc _ARG_((TERMCAP*));

#if _MULTIBYTE
extern int  	_code2byte _ARG_((wchar_t, char*));
extern wchar_t	_byte2code _ARG_((char*));
extern char*	_strcode2byte _ARG_((wchar_t*, char*, int));
extern wchar_t*	_strbyte2code _ARG_((char*, wchar_t*, int));
#endif

extern int  	_demouse _ARG_(());
extern char*	_nextname _ARG_((char*));

#if !_PACKAGE_ast

#if !_hdr_unistd
#ifndef _proto_open
_astimport int		open _ARG_((const char*, int, ...));
#endif
_astimport int		close _ARG_((int));
_astimport int		read _ARG_((int, char*, int));
_astimport int		write _ARG_((int, const char*,int));
_astimport int		sleep _ARG_((int));
_astimport int		isatty _ARG_((int));
#endif /*_hdr_unistd*/

#if !__STDC__ && !_hdr_stdlib
_astimport char*	getenv _ARG_((const char*));
_astimport Void_t*	malloc _ARG_((int));
_astimport Void_t*	calloc _ARG_((int,int));
_astimport Void_t*	realloc _ARG_((Void_t*,int));
_astimport void		free _ARG_((Void_t*));
_astimport char*	strcpy _ARG_((char*, const char*));
_astimport int		strcmp _ARG_((const char*, const char*));
_astimport size_t	strlen _ARG_((const char*));
_astimport int		atoi _ARG_((const char*));
#if _lib_memcpy
_astimport Void_t*	memcpy _ARG_((void*,const void*,size_t));
#endif
#endif /*!__STDC__ && !_hdr_stdlib*/

#if !_lib_memcpy && _lib_bcopy
_astimport void		bcopy _ARG_((const void*,void*,size_t));
#endif

_astimport char*	strcat _ARG_((char*, const char*));
_astimport int		sprintf _ARG_((char*, const char*, ...));

#endif /*!_PACKAGE_ast*/

_END_EXTERNS_

#endif /*_TERMHDR_H*/
