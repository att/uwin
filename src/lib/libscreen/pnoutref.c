#include	"scrhdr.h"


/*
**	wnoutrefresh for pads.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int pnoutrefresh(WINDOW* pad, int pby, int pbx, int sby, int sbx, int sey, int sex)
#else
int pnoutrefresh(pad,pby,pbx,sby,sbx,sey,sex)
WINDOW	*pad;
int	pby, pbx, sby, sbx, sey, sex;
#endif
{
	WINDOW	*win;

	if(!(win = _padref(pad,pby,pbx,sby,sbx,sey,sex)) )
		return ERR;
	win->_idln = pad->_idln;
	wnoutrefresh(win);
	delwin(win);

	pad->_pminy = pby; pad->_pminx = pbx;
	pad->_sminy = sby; pad->_sminx = sbx;
	pad->_smaxy = sey; pad->_smaxx = sex;

	return OK;
}
