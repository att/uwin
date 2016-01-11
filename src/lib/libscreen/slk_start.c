#include	"scrhdr.h"



/*
**	Initialize usage of soft labels
**	This routine should be called before each call of newscreen
**	or initscr to initialize for the next terminal.
**
**	ng:	number of groupings. If gp is NULL, it denotes one
**		of two default groupings:
**		0:	- - -   - -   - - -
**		1:      - - - -     - - - -
**	gp:	groupings.
**
**	Written by Kiem-Phong Vo
*/

static int	_ngroups, _groups[LABMAX];

/*
**	Compute placements of labels. The general idea is to spread
**	the groups out evenly. This routine is designed for the day
**	when > 8 labels and other kinds of groupings may be desired.
**
**	The main assumption behind the algorithm is that the total
**	# of labels in all the groups is <= LABMAX.
**
**	ng:	number of groups
**	gp:	size of each group
**	len:	length of a label
**	labx:	to return the coords of the labels.
*/
#if __STD_C
static int _slk_setpos(int ng, int* gp, int len, short* labx)
#else
static int _slk_setpos(ng,gp,len,labx)
int	ng, *gp, len;
short	*labx;
#endif
{
	reg int		i, k, n, spread, left, begadd;
	int		grpx[LABMAX];

	/* compute starting coords for each group */
	grpx[0] = 0;
	if(ng > 1)
	{
		/* spacing between groups */
		for(i = 0, n = 0; i < ng; ++i)
			n += gp[i]*(len+1) - 1;
		if((spread = (COLS-(n+1))/(ng-1)) <= 0)
			return ERR;
		left = (COLS-(n+1))%(ng-1);
		begadd = ng/2 - left/2;

		/* coords of groups */
		for(i = 1; i < ng; ++i)
		{
			grpx[i] = grpx[i-1] + (gp[i-1]*(len+1)-1) + spread;
			if(left > 0 && i > begadd)
				grpx[i] += 1, left -= 1;
		}
	}

	/* now set coords of each label */
	n = 0;
	for(i = 0; i < ng; ++i)
		for(k = 0; k < gp[i]; ++k)
			labx[n++] = grpx[i] + k*(len+1);
	return OK;
}

static int _slk_init()
{
	reg int		i, len, num;
	reg SLK_MAP	*slk;
	reg char	*cp, *ep;
	reg WINDOW	*win;

	/* clear this out to ready for next time */
	_do_slk_init = NIL(int(*)());

	/* get space for slk structure */
	if(!(slk = (SLK_MAP *) malloc(sizeof(SLK_MAP))) )
		return ERR;

	/* compute actual number of labels */
	num = 0;
	for(i = 0; i < _ngroups; ++i)
		num += _groups[i];

	if(T_pln)
	{	/* set default label lengths */
		T_lh = T_lh <= 0 ? 1 : T_lh;
		T_lw = T_lw <= 0 ? 8 : T_lw;
		T_nlab = T_nlab <= 0 ? 8 : T_nlab;
	}

	/* max label length */
	if(T_pln && (T_lh*T_lw <= LABLEN) && T_nlab >= num)
		len = T_lh*T_lw;
	else if((len = (COLS-1)/(num+1)) > LABLEN)
		len = LABLEN;

	/* impossible to do */
	if(len <= 0 || num <= 0)
		goto err;

	slk->_num = num;
	slk->_len = len;

	if(T_pln && (T_lh*T_lw <= LABLEN) && T_nlab >= num)
		win = NIL(WINDOW*);
	else
	{	/* make window and compute positions for labels */
		if(_slk_setpos(_ngroups,_groups,len,slk->_labx) == ERR)
			goto err;
		if(!(win = newwin(1,COLS,LINES-1,0)) )
			goto err;
		win->_leeve = TRUE;
		wattrset(win,A_REVERSE|A_DIM);

		/* the labels will be 1 line */
		T_lh = 1;
		T_lw = len;

		/* remove one line from the screen */
		--LINES;
	}

	for(i = 0; i < num; ++i)
	{	/* all labels start out being blanks */
		cp = slk->_ldis[i]; ep = cp+len;
		for(; cp < ep; ++cp)
			*cp = ' ';
		*ep = '\0';
		slk->_lval[i][0] = '\0';
		slk->_lch[i] = TRUE;
	}

	slk->_changed = TRUE;
	slk->_win = win;

	_do_slk_ref = _slk_update;
	_do_slk_tch = slk_touch;
	_do_slk_noref = slk_noutrefresh;

	_curslk = slk;
	return OK;

err:
	free((char*)slk);
	return ERR;
}

#if __STD_C
int slk_start(int ng, int *gp)
#else
int slk_start(ng,gp)
int	ng, *gp;
#endif
{
	reg int		i, n;

	if(!gp )
	{
		if(ng <= 0 || ng > 1)
		{
			_ngroups = 3;
			_groups[0] = 3, _groups[1] = 2, _groups[2] = 3;
		}
		else
		{
			_ngroups = 2;
			_groups[0] = 4, _groups[1] = 4;
		}
	}
	else
	{
		for(i = 0, n = 0; i < ng; ++i)
		{
			if((n += gp[i]) > LABMAX)
				return ERR;
			_groups[i] = gp[i];
		}
		_ngroups = ng;
	}

	/* signal newscreen() */
	_do_slk_init = _slk_init;
	return OK;
}
