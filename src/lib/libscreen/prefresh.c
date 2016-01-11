#include	"scrhdr.h"


/*
**	Pad refresh. These routines are provided for upward compatibility
**	with the 'pad' structure of Sys V.2. Since windows now can be of
**	arbitrary size and derived windows can be moved within their
**	parent windows effortlessly, a separate notion of 'pad' as
**	a larger-than-screen window is no longer necessary.
**
**	pby, pbx: the area (pby,pbx,maxy,maxx) of pad is refreshed
**	sby,sbx,sey,sex: the screen area to be affected.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int prefresh(WINDOW* pad, int pby, int pbx, int sby, int sbx, int sey, int sex)
#else
int prefresh(pad,pby,pbx,sby,sbx,sey,sex)
WINDOW	*pad;
int	pby, pbx, sby, sbx, sey, sex;
#endif
{
	WINDOW	*win;

	if(!(win = _padref(pad,pby,pbx,sby,sbx,sey,sex)) )
		return ERR;
	wrefresh(win);
	delwin(win);

	pad->_pminy = pby; pad->_pminx = pbx;
	pad->_sminy = sby; pad->_sminx = sbx;
	pad->_smaxy = sey; pad->_smaxx = sex;

	return OK;
}


#if __STD_C
WINDOW *_padref(WINDOW* pad, int pby, int pbx, int sby, int sbx, int sey, int sex)
#else
WINDOW *_padref(pad,pby,pbx,sby,sbx,sey,sex)
WINDOW	*pad;
int	pby, pbx, sby, sbx, sey, sex;
#endif
{
	reg WINDOW	*win;
	reg int		maxy, maxx, t;

	/* the area to be updated */
	if(pby < 0)	pby = 0;
	if(pbx < 0)	pbx = 0;
	maxy = pad->_maxy - pby;
	maxx = pad->_maxx - pbx;

	/* the screen area affected */
	if(sby < 0)
		sby = 0;
	if(sbx < 0)
		sbx = 0;
	if(sey < sby || sey >= LINES)
		sey = LINES - 1;
	if(sex < sbx || sex >= COLS)
		sex = COLS - 1;

	/* the area to be updated */
	if(maxy > (t = sey-sby+1))
		maxy = t;
	if(maxx > (t = sex-sbx+1))
		maxx = t;
	if(!(win = derwin(pad,maxy,maxx,pby,pbx)) )
		return NIL(WINDOW*);

	win->_clear = pad->_clear;
	win->_leeve = pad->_leeve;
	win->_begy = sby;
	win->_begx = sbx;
	if((win->_cury = pad->_cury-pby) < 0 || win->_cury >= maxy)
		win->_cury = 0;
	if((win->_curx = pad->_curx-pbx) < 0 || win->_curx >= maxx)
		win->_curx = 0;

	return win;
}
