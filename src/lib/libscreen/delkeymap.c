#include	"termhdr.h"


/*
**	Delete a key table
**
**	Written by Kiem-Phong Vo
*/
int delkeymap()
{
	reg KEY_MAP	*kp, *next;

	if(!_delkey)
		return ERR;
	_delkey = 0;

	if(!_keymap)
		return OK;

	/* free key slots */
	for(kp = _keymap; kp; kp = next)
	{	next = kp->_next;
		if(kp->_user && kp->_pat)
			free(kp->_pat);
		if(kp->_exp)
			free(kp->_exp);
		free(kp);
	}

	_keymap = NIL(KEY_MAP*);
	return OK;
}
