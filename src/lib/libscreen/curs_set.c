#include	"scrhdr.h"


/*
**	Change cursor style.
**	Return the value of the previous style.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int curs_set(int vis)
#else
int curs_set(vis)
int	vis;
#endif
{
	int	prev;

	switch(vis)
	{
	case 0 :	/* cursor off */
		if(T_civis)
			_puts(T_civis);
		else if(T_cvvis && T_cnorm)
			(_puts(T_cnorm), vis = 1);
		break;
	case 1 :	/* cursor normal */
		if(T_cnorm)
			_puts(T_cnorm);
		else if(T_civis && T_cvvis)
			(_puts(T_cvvis), vis = 2);
		break;
	case 2 :	/* cursor very visible */
		if(T_cvvis)
			_puts(T_cvvis);
		else if(T_civis && T_cnorm)
			(_puts(T_cnorm), vis = 1);
		break;
	default :
		vis = _cursstate;
	}

	prev = _cursstate;
	_cursstate = (char)vis;
	return prev;
}
