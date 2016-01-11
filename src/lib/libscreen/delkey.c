#include	"termhdr.h"

/*
**	Delete keys matching pat or key from the key map
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _deletekey(char* pat, char* exp, int key)
#else
int _deletekey(pat,exp,key)
reg char	*pat;
reg char	*exp;
reg int		key;
#endif
{
	reg int		mask, cmp;
	reg KEY_MAP	*kp, *next;

	if(!_keymap)
		return ERR;

	/* for ease of determination of key to delete */
	mask = (pat ? 01 : 0) | (exp ? 02 : (key >= 0 ? 04 : 0));

	/* check each key */
	for(kp = _keymap; kp != NIL(KEY_MAP*); kp = next)
	{
		next = kp->_next;

		cmp = 0;
		if(pat && strcmp(pat,(char*)kp->_pat) == 0)
			cmp |= 01;
		if(exp)
		{	if(kp->_exp && strcmp(exp,(char*)kp->_exp) == 0)
				cmp |= 02;
		}
		else if(key >= 0 && kp->_key == key)
			cmp |= 04;

		/* found one to delete */
		if(cmp == mask)
		{	/* unlink it from the list */
			if(kp->_last)
				kp->_last->_next = next;
			else	_keymap = next;
			if(next)
				next->_last = kp->_last;

			/* free occupied space */
			if(kp->_user)
				free((char*)kp->_pat);
			if(kp->_exp)
				free((char*)kp->_exp);
			free((char*)kp);
		}
	}

	return OK;
}
