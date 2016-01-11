#include	"termhdr.h"
#include	"scrhdr.h"


/*
**	These routines are for upward compatibility with the termcap
**	interface of Berkeley UNIX
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int tgetent(char* buf, char* name)
#else
int tgetent(buf,name)
char	*buf, *name;
#endif
{
	int	rv = 1;

	setupterm(name,0,&rv);
	return rv;
}



/*
**	Get the value of a boolean cap
**	Return -1 if the name is invalid
*/
#if __STD_C
int tgetflag(char* id)
#else
int tgetflag(id)
char	*id;
#endif
{
	reg bool	*bs;
	reg int		i;

/*---BOOLS---*/
	bs = &(T_bw);
	return (i = _match(boolcodes,NIL(short*),0,id)) >= 0 ? bs[i] : -1;
}



/*
**	Get the value of a num cap
**	Return -2 if the name is invalid
*/
#if __STD_C
int tgetnum(char* id)
#else
int tgetnum(id)
char	*id;
#endif
{
	reg short	*ns;
	reg int		i;

/*---NUMS---*/
	ns = &(T_cols);
	return (i = _match(numcodes,NIL(short*),0,id)) >= 0 ? ns[i] : -2;
}



/*
**	Get the value of a string cap
**	Return (char *)(-1) if the name is invalid
*/
#if __STD_C
char*	tgetstr(char* id, char** area)
#else
char*	tgetstr(id, area)
char	*id;
char**	area;
#endif
{
	reg char	**ss;
	reg int		i;

/*---STRS---*/
	ss = &(T_cbt);
	return (i = _match(strcodes,NIL(short*),0,id)) >= 0 ? ss[i] : (char *)(0);
}
