#include	"scrhdr.h"


/*
**	Insert/delete lines
**	id < 0 : number of lines to delete
**	id > 0 : number of lines to insert
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int winsdelln(WINDOW* win, int id)
#else
int winsdelln(win,id)
WINDOW	*win;
int	id;
#endif
{
	reg int		x, y, endy, endx, to, fr, nl, dir;
	reg chtype	*sw, *hash;
	reg uchar	*mk;
	bool		quick, saveimmed, savesync;
	short		*begch, *endch;

	if(win->_cury >= win->_tmarg && win->_cury <= win->_bmarg)
		endy = win->_bmarg+1;
	else	endy = win->_maxy;

	if(id < 0)
	{	/* can't delete more than there is in the region */
		if((nl = win->_cury - endy) > id)
			id = nl;

		/* compute the area to be copied */
		to = win->_cury;
		fr = to - id;
		nl = endy - fr;
		dir = 1;
	}
	else
	{	/* can't insert more than there is in the region */
		if((nl = endy - win->_cury) < id)
			id = nl;

		/* compute the area to be copied */
		to = endy - 1;
		fr = to - id;
		nl = fr - (win->_cury - 1);
		dir = -1;
	}

	/* if can be update quickly */
	quick = (win->_ndescs <= 0 && !win->_parent );

	/* go for it */
	begch = win->_firstch;
	endch = win->_lastch;
	endx  = win->_maxx;
	hash  = win == curscr ? _curhash : win == _virtscr ? _virthash : NIL(chtype*);

	for(; nl > 0; --nl, to += dir, fr += dir)
	{	/* can be done quick */
		if(quick)
		{
			sw = win->_y[to];
			win->_y[to] = win->_y[fr];
			win->_y[fr] = sw;
			if(win == curscr && _marks )
			{
				mk = _marks[to];
				_marks[to] = _marks[fr];
				_marks[fr] = mk;
			}
		}

		/* slow update */
		else	memcpy((char*)(win->_y[to]),(char*)(win->_y[fr]),
				endx*sizeof(chtype));

		if(hash)
			hash[to] = hash[fr];

		/* if this is curscr, the firstch[] and lastch[]
		   arrays contain blank information.
		*/
		if(win == curscr)
		{
			begch[to] = begch[fr];
			endch[to] = endch[fr];
		}
		/* regular window, update the change structure */
		else
		{
			begch[to] = 0;
			endch[to] = endx-1;
		}
	}

	/* clear the insert/delete lines */
	if((nl = id < 0 ? endy - to : to - (win->_cury - 1)) > 0)
	{
		saveimmed = win->_immed;
		savesync = win->_sync;
		win->_immed = FALSE;
		win->_sync = FALSE;
		x = win->_curx;
		y = win->_cury;

		win->_curx = 0;
		for(; nl > 0; --nl, to += dir)
		{
			win->_cury = to;
			wclrtoeol(win);
		}

		win->_curx = x;
		win->_cury = y;
		win->_immed = saveimmed;
		win->_sync = savesync;
	}

	if(win->_sync && win->_parent)
		wsyncup(win);

	return (win != curscr && win->_immed) ? wrefresh(win) : OK;
}
