#include	"scrhdr.h"


/*
**	Update the soft-label window
**
**	Written by Kiem-Phong Vo
*/
int slk_refresh()
{
	if(_slk_update())
	{
		wnoutrefresh(_curslk->_win);
		doupdate();
	}
	return OK;
}



/*
**	Update soft labels. Return TRUE if a window was updated.
*/
int _slk_update()
{
	reg WINDOW	*win;
	reg SLK_MAP	*slk;
	reg int		i;

	if(!(slk = _curslk) || !(slk->_changed))
		return FALSE;

	win = slk->_win;
	for(i = 0; i < slk->_num; ++i)
		if(slk->_lch[i])
		{
			if(win)	
				mvwaddstr(win,0,slk->_labx[i],slk->_ldis[i]);
			else	_puts(tstring(T_pln,i+1,slk->_ldis[i]));

			slk->_lch[i] = FALSE;
		}
	if(!win)
		_puts(T_smln);

	slk->_changed = FALSE;

	return win ? TRUE : FALSE;
}
