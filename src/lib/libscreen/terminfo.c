#include	"termhdr.h"
#include	"scrhdr.h"

/*
**	Get the value of a boolean cap
**	Return -1 if the name is invalid
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int tigetflag(char* id)
#else
int tigetflag(id)
char	*id;
#endif
{
	reg bool	*bs;
	reg int		i;

	bs = &(T_bw);
	return (i = _match(boolnames,NIL(short*),0,id)) >= 0 ? bs[i] : -1;
}



/*
**	Get the value of a num cap
**	Return -2 if the name is invalid
*/
#if __STD_C
int tigetnum(char* id)
#else
int tigetnum(id)
char	*id;
#endif
{
	reg short	*ns;
	reg int		i;

	ns = &(T_cols);
	return (i = _match(numnames,NIL(short*),0,id)) >= 0 ? ns[i] : -2;
}



/*
**	Get the value of a string cap
**	Return (char *)(-1) if the name is invalid
*/
#if __STD_C
char*	tigetstr(char* id)
#else
char*	tigetstr(id)
char	*id;
#endif
{
	reg char	**ss;
	reg int		i;

	ss = &(T_cbt);
	return (i = _match(strnames,NIL(short*),0,id)) >= 0 ? ss[i] : (char *)(-1);
}
