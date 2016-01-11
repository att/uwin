#include	"scrhdr.h"



/*
**	set color pair on terminal.
**
**	Written by John J Snyder, 10/28/1998
*/


#if __STD_C
void set_color( int p, int(*outc)(int) )
#else
void set_color( p, outc )
int 	p, (*outc)();
#endif
{	short f, b;

	/* set original color pair */
	if( p == 0 && T_pairs == 0  && T_op )
	{	tputs( T_op, 1, outc );
		return;
	}

	/* set current color pair */
	if( T_scp )
	{	tputs( tparm(T_scp,(char*)p), 1, outc );
		return;
	}

	/* set foreground and background colors,
	 * using ANSI escape sequences if available
	 */
	pair_content( p, &f, &b );
	if( T_setab )
		tputs( tparm(T_setab,(char*)b), 1, outc);
	else
		tputs( tparm(T_setb,(char*)b), 1, outc);
	if( T_setaf )
		tputs( tparm(T_setaf,(char*)f), 1, outc);
	else
		tputs( tparm(T_setf,(char*)f), 1, outc);
}
