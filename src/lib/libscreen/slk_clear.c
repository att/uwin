#include	"scrhdr.h"


/*
**	Clear the soft labels.
**
**	Written by Kiem-Phong Vo
*/
int slk_clear()
{
	reg SLK_MAP	*slk;
	reg int		i;

	if(!(slk = _curslk) )
		return ERR;

	if(slk->_win)
	{
		werase(slk->_win);
		wrefresh(slk->_win);
	}
	else
	{	/* send hardware clear sequences */
		char	blnk[2*LABLEN+1];

		for(i = 0; i < slk->_len; ++i)
			blnk[i] = ' ';
		blnk[i] = '\0';
		for(i = 0; i < slk->_num; ++i)
			_puts(tparm(T_pln,(char*)(i+1),blnk));
		_puts(T_rmln);
		_tflush();
	}

	for(i = 0; i < slk->_num; ++i)
		slk->_lch[i] = FALSE;
	slk->_changed = FALSE;

	return OK;
}
