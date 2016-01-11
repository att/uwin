#include	"termhdr.h"


/*
**	Switch to a new terminal
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
TERMINAL *setcurterm(TERMINAL* term)
#else
TERMINAL *setcurterm(term)
TERMINAL *term;
#endif
{
	if(!term)
		return NIL(TERMINAL*);

	cur_term = term;
	cur_tc = cur_term->_tc;
	_acs_map = cur_tc->_acs;

	return cur_term;
}
