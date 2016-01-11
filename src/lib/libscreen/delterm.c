#include	"termhdr.h"



/*
**	Delete a term structure
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int delterm(TERMINAL* term)
#else
int delterm(term)
TERMINAL*	term;
#endif
{
	reg TERMINAL*	savterm;

	if(term->_tc)
		_deltc(term->_tc);

	savterm = cur_term;
	cur_term = term;
	if(_delkey)
		(*_delkey)();
	if(_timeout >= 0)
		ttimeout(-1);
	cur_term = savterm;

	/* if not first terminal */
	if(term != &_f_term)
		free(term);
	else	_f_unused = TRUE;

	return OK;
}
