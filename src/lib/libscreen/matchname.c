#include	"termhdr.h"



/*
**	Match a name from a list of names separated by '|', and
**	ended by '\0' or ':'.
**	If found, return the pointer to the start of the name in
**	the list. If not found, return NULL.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
char 	*_matchname(char* term, char* list)
#else
char 	*_matchname(term,list)
char	*term, *list;
#endif
{
	reg char	*tp, *np;

	if(!term || !term[0] || !list || !list[0])
		return NIL(char*);

	while(*list != '\0' && *list != ':')
	{
		/* match term name */
		np = list;
		for(tp = term; *tp != '\0'; ++tp, ++np)
			if(*tp != *np)
				break;

		/* got it */
		if(*tp == '\0' && (*np == '|' || *np == '\0' || *np == ':'))
			return list;
			
		/* if got here, skip the rest of this name */
		while(*np != '\0' && *np != '|' && *np != ':')
			++np;

		list = *np == '|' ? np+1 : np;
	}

	return NIL(char*);
}
