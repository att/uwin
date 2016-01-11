#include	"termhdr.h"



/* _f_* are static areas used for the first terminal.
   This is necessary to support 'vi' which does not
   get along with malloc()
*/
int		_f_unused = 1;
char		_f_strs[TCSIZE];
TERMCAP		_f_tc;
TERMINAL	_f_term;

/*
	baudrate table
*/
unsigned short _baudtab[] =
{
	0, 50, 75, 110, 135, 150, 200, 300, 600,
	1200, 1800, 2400, 4800, 9600, 19200, 38400
};
int	_nbauds = sizeof(_baudtab)/sizeof(_baudtab[0]);

/* functions for keymap */
int		(*_delkey)_ARG_(());
int		(*_adjkey)_ARG_((KEY_MAP*));

/* default input descriptor, for usage of termid() */
#ifdef TERMID
int		_idinput = 0;
#endif

/* character width (in bytes) and size (in columns) */
#if _MULTIBYTE
short		_cswidth[4] = {-1, 1, 1, 1};
short		_scrwidth[4] = {1, 1, 1, 1};
__DEFINE__(short*, cswidth, _cswidth);
__DEFINE__(short*, scrwidth, _scrwidth);
#endif


__DEFINE__(TERMINAL*, cur_term, &_f_term);
__DEFINE__(TERMCAP*, cur_tc, &_f_tc);
__DEFINE__(char*, Def_term, "unknown");
__DEFINE__(char*, ttytype, NIL(char*));
__DEFINE__(chtype*, _acs_map, NIL(chtype*));
