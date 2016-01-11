#include	"termhdr.h"



/*
**	Restart a terminal after saving/restoring memory.
**	Iocontrol states are restored, new termcap is read.
**	name:	current terminal name
**	outd:	current output descriptor
**	errret:	to return error status
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
TERMINAL *restartterm(char* name, int outd, int* errret)
#else
TERMINAL *restartterm(name,outd,errret)
char	*name;
int	outd, *errret;
#endif
{
	reg int		savecho, savraw, savnonl;
	TERMINAL	*tp;

	/* save previous states */
	savecho = cur_term->_echo;
	savraw = cur_term->_raw;
	savnonl = cur_term->_nonl;

	/* get new terminal info */
	delterm(cur_term);
	_f_unused = TRUE;
	if(!(tp = setupterm(name,outd,errret)) )
		return NIL(TERMINAL*);

	/* reset iocontrol states */
	if(savecho)
		echo();
	else	noecho();
	if(savraw)
		raw();
	else	noraw();
	if(savnonl)
		nonl();
	else	nl();

	return tp;
}
