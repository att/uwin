#include	"termhdr.h"


/*
**	Find the next name from a list of names separated by '|', and
** 	ended by '\0' or ':'.
**	Initialize list by providing non-null list in argument;
**	walk through rest of list by calling with null argument.
**	Return pointer to the start of the next name in the list,
**	omitting both leading and trailing white space.
**	If no more names found, return NIL(char*).
**
**	Written by Kiem-Phong Vo
*/


#ifndef	EOS
#define	EOS 	'\0'
#endif

#ifndef NIL
#define NIL(type)	((type)0)
#endif


#if __STD_C
char 	*_nextname(char* list)
#else
char 	*_nextname(list)
char	*list;
#endif
{	static	char 	*Rest=NIL(char*), *Name=NIL(char*);
	reg 	char	*np, *name, c;

	/* initialize Rest to list */
	if(list != NIL(char*))
	{
		Rest = list;
		if( Name != NIL(char*) )
			free( Name );
		Name = NIL(char*);
	}

	/* move Rest past previous name */
	if( Name != NIL(char*) )
	{	np = Name;
		while( *np != EOS )
			np++, Rest++;
		if( *Rest == EOS || *Rest == ':' )
			return NIL(char*);
		Rest++;		/* points to first char after Name */
		free( Name );
		Name = NIL(char*);
	}

	/* go to next name in Rest of list */
	while( *Rest == '|' || isblank(*Rest) )
		Rest++;
	if( *Rest == EOS || *Rest == ':' )
		return NIL(char*);
	name = Rest;

	/* go to end of name */
	np = name;
	while( *np != '|' && *np != ':' && *np != EOS )
		np++;		/* points to name field delimiter */
	/* trim any trailing whitespace */
	while( np > name && isblank(*(np-1)) )
		np--;		/* points to first whitespace char after name */

	/* copy next name into Name */
	c = *np;
	*np = EOS;   	/* end name with EOS */
	if( !(Name = (char *) malloc(strlen((char*)name)+1)) )
		return NIL(char*);
	strcpy((char*)Name,(char*)name);
	*np = c;		/* restore Rest of (original) list */

	return Name;
}
