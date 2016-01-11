#include	"termhdr.h"

/*
**	Special string compare function see if one string is a prefix another
**	Return:	2	for different
**		1	if s1 contains s2
**		0	if equal
**		-1	if s1 is contained in s2
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
static int mystrcmp(reg uchar* s1, reg uchar* s2)
#else
static int mystrcmp(s1,s2)
reg char	*s1, *s2;
#endif
{
	while(*s1 && *s2)
		if(*s1++ != *s2++)
			return 2;
	if(*s1 != '\0')
		return 1;
	else	return *s2 == '\0' ? 0 : -1;
}

/*
**	Reorganize the key table so that kp is closer to the top.
*/
#if __STD_C
static int _movekey(KEY_MAP* kp)
#else
static int _movekey(kp)
KEY_MAP*	kp;
#endif
{
	reg KEY_MAP	*mv;

	/* no sense touching these */
	if(kp->_macro)
		return OK;

	/* move as far up as possible without violating containment */
	for(mv = kp; mv->_last; mv = mv->_last)
		if(mystrcmp(kp->_pat,mv->_last->_pat) < 0)
			break;
	if(mv == kp)
		return OK;

	/* remove kp from where it is */
	kp->_last->_next = kp->_next;
	if(kp->_next)
		kp->_next->_last = kp->_last;

	/* insert kp in front of mv */
	if(mv->_last)
		mv->_last->_next = kp;
	else	_keymap = kp;
	kp->_last = mv->_last;
	kp->_next = mv;
	mv->_last = kp;

	return OK;
}

/*
**	Add a new key to the key table
**	Macros always come after function keys.
**	If a key pattern is a prefix of another key,
**	it will come after the other one in the list.
**	This makes sure that tgetch() will match the longest possible key.
**
**	pat: the pattern identifying the key
**	exp: chars to expand into
**	key: the value to return when the key is recognized
**	isuser: if this was called from newkey(), ie, user did it
**	ismacro: if this is not a function key but a macro,
**		tgetch() will block on macros.
*/

#if __STD_C
int _addkey(char* argpat, char* argexp, int key, int isuser, int ismacro)
#else
int _addkey(argpat,argexp,key,isuser,ismacro)
char	*argpat, *argexp;
int	key, isuser, ismacro;
#endif
{
	reg KEY_MAP	*kp, *last;
	reg int		cmp;
	reg uchar	*pat = (uchar*)argpat, *exp = (uchar*)argexp;

	if(!pat || !pat[0] || (key <= 0 && !exp))
		return ERR;

	/* functions to adjust keymap */
	_delkey = delkeymap;
	_adjkey = _movekey;

	/* find a suitable place to insert */
	last = NIL(KEY_MAP*);
	for(kp = _keymap; kp; last = kp, kp = kp->_next)
	{	/* we want macro to comes last */
		if(!kp->_macro && ismacro)
			continue;

		/* function key always win */
		if(kp->_macro && !ismacro)
			cmp = -1;
		/* must be the case that kp->_macro == ismacro */
		else	cmp = mystrcmp(kp->_pat,pat);

		switch(cmp)
		{
		case 2 :
		case 1 :
			continue;

		case 0 : /* pattern exists, temporarily remove it */
			if(last)
				last->_next = kp->_next;
			else	_keymap = kp->_next;
			if(kp->_next)
				kp->_next->_last = last;
			if(kp->_exp)
				free(kp->_exp);
			goto doinsert;

		case -1 : /* insert before current pattern */
			kp = NIL(KEY_MAP*);
			goto doinsert;
		}
	}

doinsert:
	if(!kp)
	{	if(!(kp = (KEY_MAP*)malloc(sizeof(KEY_MAP))) )
			return ERR;
		if(!isuser)
			kp->_pat = pat;
		else if(!(kp->_pat = (uchar*)malloc(strlen((char*)pat)+1)) )
		{	free(kp);
			return ERR;
		}
		else	strcpy((char*)kp->_pat,(char*)pat);
		kp->_user = isuser;
	}

	kp->_macro = ismacro;
	if(!exp)
	{	kp->_key = key;
		kp->_exp = NIL(uchar*);
	}
	else
	{	kp->_key = strlen((char*)exp);
		if(!(kp->_exp = (uchar*)malloc(kp->_key+1)) )
		{	if(kp->_user)
				free((char*)kp->_pat);
			free(kp);
			return ERR;
		}
		else	strcpy((char*)kp->_exp,(char*)exp);
	}

	/* now hook it in */
	if(last)
	{	kp->_next = last->_next;
		last->_next = kp;
	}
	else
	{	kp->_next = _keymap;
		_keymap = kp;
	}
	kp->_last = last;
	if(kp->_next)
		kp->_next->_last = kp;

	return OK;
}
