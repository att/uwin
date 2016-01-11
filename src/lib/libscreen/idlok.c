#include	"scrhdr.h"

#define CHAR_BITS	(8)	/* 8-bit per char */
#define CHAR_MASK	(0377)

#define IDST	struct _id_st
static struct _id_st
{
	short	_wy;		/* matching lines */
	short	_sy;
}	*id, *sid, *eid;	/* list of idln actions */
static int	**mt;			/* the match length matrix */
static int	idlen, scrli, scrco,	/* screen dimensions */
		cy, cx;			/* current cursor positions */
static chtype	*hw, *hs;		/* the fingerprints of lines */
static bool	didcsr;
static long	chtype_mask;		/* chtype bits, used in hash_ln */


/*
**	Compute the largest set of matched lines.
**	This algorithm is an extended version of
**	the Longest Common Subsequence algorithm in which
**	we compute the largest set of matched lines (hence LCS)
**	with minimal distance between matched lines.
**	This currently uses dynamic programming since we don't
**	really match very large sets of lines. If this becomes
**	a resource hog some day, we'll switch to the
**	Jacobson-Vo's Heaviest Common Subsequence algorithm.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
static void _find_idln(int top, int bot)
#else
static	_find_idln(top,bot)
int	top, bot;
#endif
{
	reg int		*mw, *m1, w, s, wy, sy, n, nn;
	reg unsigned long	hwv;

	if((n = bot-top) <= 0)
		return;
	nn = n*n + n;

	/* compute the max match length matrix */
	for(w = 1, wy = top; w <= n; ++w, ++wy)
	{
		m1 = mt[w-1];
		mw = mt[w]+1;
		hwv = hw[wy];
		for(s = 1, sy = top; s <= n; ++s, ++sy, ++m1, ++mw)
		{	reg int		d, u, l;

			/* diagonal value */
			d = hwv != hs[sy] ? 0 : *m1 + nn - (s > w ? s-w : w-s);

			/* up value */
			u = *(m1+1);

			/* left value */
			l = *(mw-1);

			*mw = d > u ? MAX(d,l) : MAX(u,l);
		}
	}

	/* now find the matches */
	for(w = s = n, wy = sy = bot-1; w > 0 && s > 0; --w,--s, --wy,--sy)
	{	for(mw = mt[w]+s; s > 0; --s, --sy, --mw)
			if(*(mw-1) < *mw)
				break;

		for(; w > 0; --w, --wy)
			if(mt[w-1][s] < mt[w][s])
				break;

		if(w > 0 && s > 0 && w != s)
		{	--sid;
			sid->_wy = wy;
			sid->_sy = sy;
		}
	}
}


/*
**	Get the hash value of a line
*/
#define HPART(h,c)	(h = ((h)<<8) + ((h)<<2) + (h) + (c))

#if __STD_C
static chtype	_hash_ln(chtype* sc, chtype* ec)
#else
static chtype	_hash_ln(sc,ec)
chtype	*sc, *ec;
#endif
{
	reg unsigned long	h;
	reg bool		blank;
	reg unsigned char	*s, *e;

	h = 0;
	blank = TRUE;
	for(; sc <= ec; ++sc)
	{	if(*sc == BLNKCHAR)
			continue;

		for(s = (unsigned char*)sc, e = s + sizeof(chtype); s < e;)
			HPART(h,*s++);
		blank = FALSE;
	}

	if(blank)
		return 0;

	if(sizeof(long) > sizeof(chtype))
		h = (h >> (sizeof(long)-sizeof(chtype))/2) & chtype_mask;

	return (chtype)(h == 0 ? 1 : ((long)h == _NOHASH ? 2 : h));
}



/*
**	Set the places to do insert/delete lines
**	Return the start line for such action.
*/
#if __STD_C
static int _set_idln(int topy, int boty)
#else
static int _set_idln(topy,boty)
int	topy, boty;
#endif
{
	reg int		wy, bot, n, blnkbot;
	reg short	*begch, *endch, *begns, *endns;
	reg chtype	**wcp, **scp;

	if(topy+1 >= boty ||
	   !(T_dl1 || T_dl || (T_cup && T_csr && (T_ind || T_indn))) ||
	   !(T_il1 || T_il || (T_cup && T_csr && (T_ri || T_rin))))
		return curscr->_maxy;

	scrli = curscr->_maxy;
	scrco = curscr->_maxx;

	/* set up space for computing idln actions */
	if(idlen < scrli)
	{
		if(mt && mt[0])
			free((char *)(mt[0]));
		if(mt)
			free((char *)mt);
		if(id)
			free((char *)id);

		n = scrli+1;
		if(!(mt = (int **) malloc(n*sizeof(int *))) ||
		   !(mt[0] = (int *) malloc(n*n*sizeof(int))) ||
		   !(id = (IDST *) malloc(scrli*sizeof(IDST))))
		{
			if(mt && mt[0])
				free((char *)(mt[0]));
			if(mt)
				free((char *)mt);
			idlen = 0;
			return scrli;
		}
		idlen = scrli;
		for(wy = 1; wy < n; ++wy)
			mt[wy] = mt[wy-1] + n;
		for(wy = 0; wy < n; ++wy)
			mt[0][wy] = mt[wy][0] = 0;
	}

	/* compute fingerprints for fast line comparisons */
	begch = _virtscr->_firstch + topy;
	endch = _virtscr->_lastch + topy;
	wcp   = _virtscr->_y + topy;
	hw    = _virthash;

	begns = _begns + topy;
	endns = _endns + topy;
	scp   = curscr->_y + topy;
	hs    = _curhash;

	for(wy = topy; wy < boty;
	    ++wy, ++begch, ++endch, ++wcp, ++begns, ++endns, ++scp)
	{
		if(*begch == _REDRAW || *begch == _INFINITY)
			hw[wy] = _NOHASH;
		else if(*endch == _BLANK)
			hw[wy] = 0;
		else if(hw[wy] == _NOHASH)
		{	hw[wy] = _hash_ln(*wcp,*wcp+scrco-1);
			if(hw[wy] == 0)
				*endch = _BLANK;
		}

		if(hs[wy] == _NOHASH)
			hs[wy] = _hash_ln(*scp + *begns, *scp + *endns);
	}

	/* the bound of blank lines */
	for(blnkbot = scrli-1; blnkbot >= 0; --blnkbot)
		if(hs[blnkbot] != 0)
			break;
	for(; blnkbot >= 0; --blnkbot)
		if(hs[blnkbot] == hw[blnkbot])
			break;

	/* go get them */
	sid = eid = id + scrli;
	while(topy < boty)
	{
		reg IDST	*oldsid;

		/* find a validly changed interval of lines */
		for(; topy < boty; ++topy)
			if(hw[topy] != _NOHASH)
				break;
		for(bot = topy; bot < boty; ++bot)
			if(hw[bot] == _NOHASH)
				break;

		/* skip starting and trailing matching blanks */
		for(wy = topy; wy < bot; ++wy)
			if(hw[wy] != 0 || hs[wy] != 0)
				break;
		for(n = bot-1; n >= wy; --n)
			if(hw[n] != 0 || hs[n] != 0)
				break;

		/* don't do this, it's too annoying */
		if(n < blnkbot && baudrate() < 4800 && !T_csr && !(T_dl && T_il))
			break;

		oldsid = sid;
		_find_idln(wy,n+1);

		/* don't insert/delete if too much screen jumping */
		if(!T_csr && n < blnkbot && (oldsid-sid) < (scrli-wy)/2)
			sid = oldsid;

		topy = bot+1;
	}
		
	return eid > sid ? MIN(sid->_wy,sid->_sy) : scrli;
}


/*
**	Do the actual insert/delete lines
*/
#if __STD_C
static void _do_idln(int tsy, int bsy, int idn, int doinsert)
#else
static	_do_idln(tsy,bsy,idn,doinsert)
int	tsy, bsy, idn, doinsert;
#endif
{
	reg int		y, usecsr, yesscrl;
	reg short	*begns;

	/* change scrolling region */
	yesscrl = usecsr = FALSE;
	if(tsy > 0 || bsy < scrli)
	{	if(T_csr)
		{	_puts(tstring(T_csr,tsy,bsy-1));
			cy = cx = -1;
			yesscrl = usecsr = didcsr = TRUE;
		}
	}
	else
	{	if(didcsr)
		{	_puts(tstring(T_csr,0,scrli-1));
			cy = cx = -1;
			didcsr = FALSE;
		}
		yesscrl = TRUE;
	}

	if(doinsert)
	{	/* memory below, clobber it now */
		if(T_db && T_el && ((usecsr && T_ndscr) || bsy == scrli))
		{	for(y = bsy-idn, begns = _begns+y; y < bsy; ++y, ++begns)
			{	if(*begns >= scrco)
					continue;

				mvcur(cy,cx,y,0);
				cy = y, cx = 0;
				_puts(T_el);
			}
		}

		/* if not T_csr, the trick is to delete, then insert */
		if(!usecsr && bsy < scrli)
		{	/* delete appropriate number of lines */
			mvcur(cy,cx,bsy-idn,0);
			cy = bsy-idn, cx = 0;
			if(T_dl && (idn > 1 || !T_dl1))
				_puts(tstring(T_dl,idn,-1));
			else for(y = 0; y < idn; ++y)
				_puts(T_dl1);
		}

		/* now do insert */
		mvcur(cy,cx,tsy,0);
		cy = tsy, cx = 0;
		if(yesscrl)
		{	/* try scrolling if advantageous */
			if(!T_rin && (!T_ri || (T_il && idn > 1)))
				goto hardinsert;
			if(T_rin && (idn > 1 || !T_ri))
				_puts(tstring(T_rin,idn,-1));
			else for(y = 0; y < idn; ++y)
				_puts(T_ri);
		}
		else
		{
		hardinsert:
			if(T_il && (idn > 1 || !T_il1))
				_puts(tstring(T_il,idn,-1));
			else	for(y = 0; y < idn; ++y)
					_puts(T_il1);
		}
	}
	else
	{ /* doing deletion */
		/* memory above, clobber it now */
		if(T_da && T_el && ((usecsr && T_ndscr) || tsy == 0))
		{	for(y = 0, begns = _begns+y+tsy; y < idn; ++y, ++begns)
			{	if(*begns >= scrco)
					continue;

				mvcur(cy,cx,tsy+y,0);
				cy = tsy+y; cx = 0;
				_puts(T_el);
			}
		}

		/* ok, do delete */
		if(yesscrl)
		{
			if(!T_indn && (!T_ind || (T_dl && idn > 1)))
				goto harddelete;
			mvcur(cy,cx,bsy-1,0);
			cy = bsy-1, cx = 0;
			if(T_indn && (idn > 1 || !T_ind))
				_puts(tstring(T_indn,idn,-1));
			else for(y = 0; y < idn; ++y)
				_puts(T_ind);
		}
		else
		{
		harddelete:
			mvcur(cy,cx,tsy,0);
			cy = tsy, cx = 0;
			if(T_dl && (idn > 1 || !T_dl1))
				_puts(tstring(T_dl,idn,-1));
			else for(y = 0; y < idn; ++y)
				_puts(T_dl1);
		}

		/* if not T_csr, do insert to restore bottom */
		if(!usecsr && bsy < scrli)
		{
			y = scrli-1;
			begns = _begns + y;
			for(; y >= bsy; --y, --begns)
				if(*begns < scrco)
					break;
			if(y >= bsy)
			{	mvcur(cy,cx,bsy-idn,0);
				cy = bsy-idn; cx = 0;
				if(T_il && (idn > 1 || !T_il1))
					_puts(tstring(T_il,idn,-1));
				else for(y = 0; y < idn; ++y)
					_puts(T_il1);
			}
		}
	}
}


/*
**	Use hardware line delete/insert
*/
static int _use_idln()
{
	reg int		tsy, bsy, idn, dir, nomore;
	IDST		*ip, *ep, *eip;

	cy = curscr->_cury;
	cx = curscr->_cury;
	didcsr = FALSE;

	/* first cycle do deletions, second cycle do insertions */
	for(dir = 1; dir > -2; dir -= 2)
	{
		if(dir > 0)
			ip = sid,   eip = eid;
		else	ip = eid-1, eip = sid-1;

		nomore = TRUE;
		while(ip != eip)
		{	/* skip deletions or insertions */
			if((dir > 0 && ip->_wy > ip->_sy) ||
			   (dir < 0 && ip->_wy < ip->_sy))
			{	nomore = FALSE;
				ip += dir;
				continue;
			}

			/* find a contiguous block */
			for(ep = ip+dir; ep != eip; ep += dir)
				if(ep->_wy != (ep-dir)->_wy+dir ||
				   ep->_sy != (ep-dir)->_sy+dir)
					break;
			ep -= dir;

			/* top and bottom lines of the affected region */
			if(dir > 0)
			{	tsy = MIN(ip->_wy,ip->_sy);
				bsy = MAX(ep->_wy,ep->_sy) + 1;
			}
			else
			{	tsy = MIN(ep->_wy,ep->_sy);
				bsy = MAX(ip->_wy,ip->_sy) + 1;
			}

			/* amount to insert/delete */
			if((idn = ip->_wy - ip->_sy) < 0)
				idn = -idn;

			/* do the actual output */
			_do_idln(tsy,bsy,idn,dir == -1);

			/* update change structure */
			wtouchln(_virtscr,tsy,bsy-tsy,-1);

			/* update screen image */
			curscr->_tmarg = tsy, curscr->_bmarg = bsy-1;
			curscr->_cury = tsy;
			winsdelln(curscr,dir > 0 ? -idn : idn);
			curscr->_tmarg = 0, curscr->_bmarg = scrli-1;

			/* for next while cycle */
			ip = ep+dir;
		}

		if(nomore)
			break;
	}

	/* reset scrolling region */
	if(didcsr)
	{	_puts(tstring(T_csr,0,scrli-1));
		cy = cx = -1;
	}

	curscr->_cury = cy, curscr->_curx = cx;

	return OK;
}

/*
**	Set insert/delete line mode for win
*/
#if __STD_C
int idlok(WINDOW* win, int bf)
#else
int idlok(win,bf)
WINDOW	*win;
int	bf;
#endif
{
	/* set chtype_mask */
	if(sizeof(long) > sizeof(chtype) && !chtype_mask)
	{	reg int	i;
		for(i = 0; i < sizeof(chtype); ++i)
			chtype_mask |= CHAR_MASK << (i*CHAR_BITS);
	}

	/* function for insert/delete lines */
	_useidln = _use_idln;
	_setidln = _set_idln;

	return (win->_idln = bf);
}
