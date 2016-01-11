#include	"termhdr.h"


/*
**	Return the long name of the terminal
**
**	Written by Kiem-Phong Vo
*/

char	*longname()
{
	register char	*cp;

	for(cp = cur_tc->_names; *cp != '\0'; ++cp)
		;
	for(; *cp != '|' && cp > cur_tc->_names; --cp)
		;
	return cp+1;
}



/*
**	Return the terminal name
*/

char	*termname()
{
	register char	*cp, *tp;
	static char	term[16];

	for(cp = cur_tc->_term, tp = term; *cp != '\0'; ++cp, ++tp)
		if(*cp == '|' || *cp == ':')
			break;
		else	*tp = *cp;
	*tp = '\0';
	return term;
}
