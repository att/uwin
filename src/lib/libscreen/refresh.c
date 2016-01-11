#include	"scrhdr.h"

/*	Make the screen look like "win" over the area covered by win.
**	This routine may use insert/delete char/line and scrolling-region.
**	win :	the window being updated
**
**	Written by Kiem-Phong Vo
*/

static short	cy, cx,		/* current cursor coord */
		scrli, scrco;	/* actual screen size */
static uchar	**marks;	/* the mark table for cookie terminals */
#define 	_ismark(y,x)	(marks[y][x/CHARSIZE] & (1 << (x%CHARSIZE)))

/* 	Use hardware clear-to-bottom.  */
#if __STD_C
static void _useceod(int topy, int boty)
#else
static	_useceod(topy,boty)
int	topy, boty;
#endif
{
	reg short	*begns, *begch;

	/* skip lines already blanked */
	begch = _virtscr->_firstch + topy;
	begns = _begns + topy;
	for(; topy < boty; ++topy, ++begns, ++begch)
	{	if(*begns < scrco || *begch == _REDRAW)
			break;
		else	*begch = _INFINITY;
	}

	/* nothing to do */
	if(topy+1 >= boty)
		return;

	/* see if bottom is clear */
	for(begns = _begns+boty; boty < scrli; ++boty, ++begns)
		if(*begns < scrco)
			return;

	if(topy == 0)
	{	/* use clear-screen if appropriate */
		_puts(T_clear);
		cy = 0; cx = 0;
		werase(curscr);
	}
	else if(T_ed || (T_dl && !T_db))
	{	/* use clear-to-end-of-display or delete lines */
		mvcur(cy,cx,topy,0);
		cy = topy; cx = 0;
		_puts(T_ed ? T_ed : tstring(T_dl,scrli-topy,-1));

		/* update curscr */
		curscr->_cury = topy;
		wclrtobot(curscr);
	}

	/* no hardware support */
	else	return;

	/* correct the update structure */
	wtouchln(_virtscr,topy,scrli,FALSE);
}


/*	Find the top-most line to do clear-to-eod.
**	topy, boty: the region to consider
*/
#if __STD_C
static int _getceod(int topy, int boty)
#else
static int _getceod(topy,boty)
int	topy, boty;
#endif
{
	reg chtype	*wcp, *ecp;
	reg int		wy;
	short		*begch, *endch, *begns;

	/* do nothing */
	if((topy+1) >= boty)
		return boty;

	wy    = boty - 1;
	begch = _virtscr->_firstch + wy;
	endch = _virtscr->_lastch + wy;
	begns = _begns + wy;

	for(; wy >= topy; --wy, --begch, --endch, --begns)
	{
		if(*endch == _BLANK || (*begch >= scrco && *begns >= scrco))
			continue;

		wcp = _virtscr->_y[wy];
		ecp = wcp + scrco;
		for(; wcp < ecp; ++wcp)
			if(DARKCHAR(*wcp))
				break;
		if(wcp != ecp)
			break;

		*endch = _BLANK;
	}

	return wy+1;
}


/*	Set/Unset markers for cookie terminal.  */
#if __STD_C
static void _setmark(int y,int x, chtype* s, int setting)
#else
static	_setmark(y,x,s,setting)
int	y, x;
chtype	*s;
int	setting;
#endif
{
	reg int		i;
	reg chtype	a;

	/* set the mark map */
	if(setting)
		marks[y][x/CHARSIZE] |= (1 << (x%CHARSIZE));
	else	marks[y][x/CHARSIZE] &= ~(1 << (x%CHARSIZE));

	if(s)
	{
		a = ATTR(curscr->_attrs);

		if(T_xmc == 0 || !setting)
		{	*s = CHAR(*s)|a;
			s += 1;
			x += 1;
		}
		else for(i = T_xmc; i > 0; --i)
		{	*s = BLNKCHAR|a;
			s += 1;
			x += 1;
		}

		/* now the video attr of the rest of the affected interval */
		for(; x < scrco; ++x, ++s)
		{	if(_ismark(y,x))
				break;
			*s = CHAR(*s) | a;
		}
	}
}


/*	At the right margin various weird things can happen.
**	We treat them here.
*/
#if __STD_C
static void _rmargin(chtype* wcp, chtype* scp, int wx)
#else
static	_rmargin(wcp, scp, wx)
chtype	*wcp, *scp;
int	wx;
#endif
{
	reg int		x, ix, w;
	reg chtype	wc;

	/* screen may scroll */
	if(cy == scrli-1)
	{	/* can't do anything */
		if(!_ichok)
			return;
#if _MULTIBYTE
		/* the width of the new character */
		wc = RBYTE(wcp[wx]);
		w = scrwidth[TYPE(wc)];

		/* the place to put it without causing scrolling */
		for(ix = wx-1; ix > 0; --ix)
			if(!ISCBIT(scp[ix]))
				break;
#else
		w  = 1;
		ix = wx-1;
#endif
		/* add the character to the screen image */
		mvcur(cy,cx,cy,ix);
		_putchar(T_hz && CHAR(wcp[wx]) == '~' ? '`' : wcp[wx]);
		scp[wx] = wcp[wx];
#if _MULTIBYTE
		for(x = wx+1; x < scrco; ++x)
			_putchar(wcp[x]), scp[x] = wcp[x];
#endif
		/* push the new character to its rightful place */
		mvcur(cy,ix+w,cy,ix);
		if(_imode && !_insert)
			_oninsert();
#if _MULTIBYTE
		/* width of the old character that was overwritten */
		wc = RBYTE(scp[ix]);
		w = scrwidth[TYPE(wc)];
#endif
		if(T_ich1)
			for(x = 0; x < w; ++x)
				_puts(T_ich1);
		else if(T_ich && !_insert)
			_puts(tstring(T_ich,w,-1));

		/* redraw the old character */
		wc = ATTR(scp[ix]);
		if(T_xmc == 0 || wc != curscr->_attrs)
		{	_vids(wc,curscr->_attrs);
			if(T_xmc == 0)
				_setmark(cy,ix,NIL(chtype*),TRUE);
		}
		wc = scp[ix];
		_putchar((T_hz && CHAR(wc) == '~') ? '`' : wc);
#if _MULTIBYTE
		for(w -= 1, x = ix+1; w > 0; --w, ++x)
			_putchar(scp[x]);
#endif
		/* put out the pad string */
		if(T_ip)
			_puts(T_ip);

		/* make sure the new char has the right video attrs */
		if(T_xmc == 0 && (wc = ATTR(scp[wx])) != A_NORMAL)
		{	_vids(wc,~wc);
			_setmark(cy,wx,NIL(chtype*),TRUE);
		}

		cx = wx;
	}
	else
	{
		if(_insert)
			_offinsert();

		/* put char out and update screen image */
		wc = wcp[wx];
		_putchar((T_hz && CHAR(wc) == '~') ? '`' : wc);
		scp[wx] = wcp[wx];
		if(T_xmc > 0 && _ismark(cy,wx))
		{	curscr->_attrs = wx > 0 ? ATTR(scp[wx-1]) : A_NORMAL;
			_setmark(cy,wx,scp+wx,FALSE);
		}
#if _MULTIBYTE
		for(x = wx+1; x < scrco; ++x)
		{	_putchar(wcp[x]);
			scp[x] = wcp[x];
			if(T_xmc > 0 && _ismark(cy,x))
			{	curscr->_attrs = ATTR(scp[x-1]);
				_setmark(cy,x,scp+x,FALSE);
			}
		}
#endif

		/* make sure that wrap-around happens */
		if(!T_am || T_xenl)
			_putchar('\r'), _putchar('\n');
		cx = 0;
		++cy;
	}
}


/*	Find a substring of s2 that match a prefix of s1.
**	The substring is such that:
**		1. it does not start with an element
**		   that is in perfect alignment with one in s1 and
**		2: it is at least as long as the displacement.
**
**	length:	the length of s1, s2.
**	maxs:	only search for match in [1,maxs]  of s2.
**	begm:	*begm returns where the match begins.
**
**	Return the number of matches.
*/
#if __STD_C
static int _prefix(chtype* s1, chtype* s2, int length, int maxs, int* begm)
#else
static int _prefix(s1,s2,length,maxs,begm)
chtype	*s1, *s2;
int	length, maxs, *begm;
#endif
{
	reg int		m, n, k;

	/**/ ASSERT(!ISCBIT(s1[0]) && !ISCBIT(s2[0]));

	/* condition 2 only allows matching up to length/2 */
	if(maxs > length/2)
		maxs = length/2;

	n = 0;
	for(m = 1; m <= maxs; ++m)
	{	/* testing for s1[m] != s2[m] is condition 1 */
		if(s1[0] == s2[m] && s1[m] != s2[m])
		{
			/* see if it's long enough (condition 2) */
			for(k = 2*m-1; k > m; --k)
				if(s1[k-m] != s2[k])
					break;

			/* found a good match */
			if(k == m)
			{	*begm = m;

				/* count the # of matches */
				s2 += m;
				length -= m;
				for(n = m; n < length; ++n)
					if(s1[n] != s2[n])
						break;
				break;
			}
		}
	}

	return n;
}


/*	See if a left or right shift is apppropriate
**	This routine is called only if !cookie_glitch or no video attributes
**	are used in the affected part.
**	The main idea is to find a longest common substring which is a
**	prefix of one of 'wcp' or 'scp', then either delete or
**	insert depending on where the prefix is.
**
**	wcp : what we want the screen to look like
**	scp : what the screen looks like now
**	length: the length to be updated
**	maxi: maximum possible insert amount
**	id: *id returns the amount of insert/delete
**
**	Return the number of chars matched after the shift.
*/
#if __STD_C
static int _useidch(chtype* wcp, chtype* scp, int length, int maxi, int* id)
#else
static int _useidch(wcp,scp,length,maxi,id)
chtype	*wcp, *scp;
int	length, maxi, *id;
#endif
{
	reg int		x1, x2, blnk;
	reg chtype	wc;
	int		cost, costich1, match, idch;

	/* try deletion */
	if(_dchok && CHAR(*wcp) != BLNKCHAR)
	{
		if((match = _prefix(wcp,scp,length,length/4,&idch)) > 0)
			cost = C_dcfix + (T_dch ? C_dch : C_dch1*idch);
		else	cost = _INFINITY;

		if(match >= cost)
		{	if(_dmode)
			{	if(_sidsame)
				{	if(!_insert)
						_oninsert();
				}
				else
				{	if(_insert)
						_offinsert();
					_puts(T_smdc);
				}
			}

			if(T_dch)
				_puts(tstring(T_dch,idch,-1));
			else for(x1 = 0; x1 < idch; ++x1)
				_puts(T_dch1);

			if(_dmode)
			{	if(_eidsame)
					_insert = FALSE;
				_puts(T_rmdc);
			}

			/* update screen image */
			for(x1 = 0, x2 = idch; x2 < length; ++x1, ++x2)
				scp[x1] = scp[x2];
			for(; x1 < length; ++x1)
				scp[x1] = BLNKCHAR;

			*id = -idch;
			return match;
		}
	}

	/* no insertion wanted or possible */
	if(!_ichok || CHAR(*scp) == BLNKCHAR)
		return 0;

	/* see if insertion is possible */
	if(length/2 < maxi)
		maxi = length/2;
	if((match = _prefix(scp,wcp,length,maxi,&idch)) <= 0)
		return 0;

	/* see if inserting blanks only */
	for(blnk = 0; blnk < idch; ++blnk)
		if(wcp[blnk] != BLNKCHAR)
			break;
	if(blnk < idch)
		blnk = 0;

	/* see if doing insertion is worth it */
	costich1 = idch*C_ich1;
	if(_imode)
		cost =	(_insert ? 0 : C_icfix) +
			((blnk > C_ich && C_ich < costich1) ? C_ich :
			 (T_ich1 ? costich1 : 0));
	else	cost =	(T_ich && C_ich < costich1) ? C_ich : costich1;
	if(match < (cost-blnk))
		return 0;

	if(_imode)
	{	/* inserting blanks */
		if(!_insert)
			_oninsert();
		if(blnk > C_ich && C_ich < costich1)
			_puts(tstring(T_ich,idch,-1));
		else if(T_ich1)
			goto do_insert_char;
		else	blnk = 0;
	}
	else if(T_ich && C_ich < costich1)
		_puts(tstring(T_ich,idch,-1));
	else
	{
	do_insert_char:
		for(x1 = 0; x1 < idch; ++x1)
			_puts(T_ich1);
	}

	/* inserting our own characters */
	if(!blnk)
	{	for(x1 = 0; x1 < idch; ++x1)
		{	wc = wcp[x1];
			if(ATTR(wc) != curscr->_attrs)
				_vids(ATTR(wc),curscr->_attrs);
			_putchar(CHAR(wc) == '~' && T_hz ? '`' : wc);
			if(T_ip)
				_puts(T_ip);
			++cx;
		}
	}

	/* update the screen image */
	for(x1 = length-1, x2 = length-idch-1; x2 >= 0; --x1, --x2)
		scp[x1] = scp[x2];
	memcpy(scp,wcp,idch*sizeof(chtype));

	*id = idch;
	return match+idch;
}

/* 	Shift appropriate portions of a line to leave space for cookies.
**	This code is not multi-byte smart. Let's hope multi-byte terminal
**	vendors have better wisdom than the Televideo crowd.
*/
#if __STD_C
static chtype	*_shove(reg int wy)
#else
static chtype	*_shove(wy)
reg int		wy;
#endif
{
	reg chtype	*wcp, *wp, *sp, *endsp, *endwp, attrs;
	reg int		curx;
	reg chtype	*cleft, *cright;
	static chtype	*shift;
	static int	length;

	if(length < scrco)
	{	/* allocate space for shifted line */
		if(shift)
			free((char*)shift);
		shift = (chtype*)malloc(scrco*sizeof(chtype));
		length = shift ? scrco : 0;
	}

	/* no space to do it */
	if(!shift)
		return _virtscr->_y[wy];

	wcp = _virtscr->_y[wy];
	endsp = (sp = shift) + scrco;

	/* copy the initial normal attribute part */
	while(sp < endsp && ATTR(wcp[0]) == A_NORMAL)
		*sp++ = *wcp++;
	if(sp >= endsp)
		return shift;

	/* find region where cursor resides */
	curx = (!_virtscr->_leeve && _virtscr->_cury == wy) ? _virtscr->_curx : -1;
	wp = _virtscr->_y[wy];
	if(curx >= 0)
	{	attrs = ATTR(wp[curx]);
		for(cleft = wp+curx; cleft > wp; --cleft)
			if(ATTR(cleft[-1]) != attrs)
				break;
		for(cright = wp+curx, endwp = wp+scrco; cright < endwp; ++cright)
			if(ATTR(cright[0]) != attrs)
				break;
	}
	else	cleft = (cright = wp) + scrco;
		
	do
	{	/* the length of the segment to be processed */
		attrs = ATTR(wcp[0]);
		for(wp = wcp+1, endwp = wcp+(endsp-sp); wp < endwp; ++wp)
			if(ATTR(wp[0]) != attrs)
				break;

		if(wcp >= cleft && wcp < cright)
		{	/* within the cursor_region, no cookies are allowed */
			if(sp > shift && sp[-1] == BLNKCHAR)
				goto prev_cookie;
			else	goto do_shift;
		}
		else if(sp > shift && wcp != cright &&
			wcp[0] != BLNKCHAR && CHAR(sp[-1]) == BLNKCHAR)
		{	/* use previous blank as a cookie */
		prev_cookie:
			sp[-1] = BLNKCHAR|attrs;
			goto do_copy;
		}
		else if(CHAR(wcp[0]) == BLNKCHAR)
		{	/* use current char for cookie */
		do_copy:
			memcpy((char*)sp,(char*)wcp,(wp-wcp)*sizeof(chtype));
			sp += wp-wcp;
			wcp = wp;
		}
		else
		{	/* shift right 1 for cookie */
		do_shift:
			sp[0] = BLNKCHAR|attrs;
			sp += 1;
			if((wp-wcp) > (endsp-sp))
				wp -= 1;
			memcpy((char*)sp,(char*)wcp,(wp-wcp)*sizeof(chtype));

			sp += wp-wcp;

			if(wp != cleft && sp < endsp && wp[0] == BLNKCHAR)
			{	/* shifted into a blank, eat it */
				if(wcp == cleft && curx >= 0)
					curx += 1;
				wcp = wp+1;
			}
			else
			{	if(wcp <= cleft && curx >= 0)
					curx += 1;
				wcp = wp;
			}
		}
	} while(sp < endsp);

	/* make sure that the end of the line is normal */
	sp = shift + scrco - 1;
	if(wy == scrli-1 && ATTR(sp[-1]) != A_NORMAL)
		sp[-1] = sp[0] = BLNKCHAR;
	else if(ATTR(sp[0]) != A_NORMAL)
		sp[0] = BLNKCHAR;

	/* update cursor position */
	if(curx >= scrco)
		_virtscr->_curx = scrco-1;
	else if(curx >= 0)
		_virtscr->_curx = curx;

	return shift;
}


/*	Update a line.
**	Three schemes of coloring are allowed. The first is the usual
**	pen-up/pen-down model. The second is the HP26*-like model.
**	In this case, colorings are specified by intervals, the left
**	side of the interval has the coloring mark, the right side
**	has the end-coloring mark. We assume that clear sequences will
**	clear ALL marks in the affected regions. The second case is
**	signified by the boolean flag T_xhp or T_xmc == 0.
**	The third case is for terminals that leave visible cookies on
**	the screen. This last case can never be done right with the
**	<curses> model. The algorithm approximates it by shifting
**	hilited portions of text as necessary.
*/
#if __STD_C
static int _updateln(int wy)
#else
static int _updateln(wy)
int	wy;
#endif
{
	reg chtype	*wcp, *scp, *wp, *sp, *ep, wc;
	reg int		wx, lastx, x;
	int		mtch, idch, blnkx, idcx, normx, maxi, endns, begns;
	bool		redraw;

	redraw = (_virtscr->_firstch[wy] == _REDRAW);
	endns = _endns[wy];
	begns = _begns[wy];

	/* easy case */
	if(!redraw && _virtscr->_lastch[wy] == _BLANK && begns >= scrco)
		return OK;

	/* line images */
	scp = curscr->_y[wy];
	wcp = T_xmc <= 0 ? _virtscr->_y[wy] : _shove(wy);

	/* the interval to be updated */
	if(redraw || T_xmc >= 0)
	{	wx = 0;
		lastx = scrco;
	}
	else
	{	wx = _virtscr->_firstch[wy];
		lastx = _virtscr->_lastch[wy] == _BLANK ? scrco :
				_virtscr->_lastch[wy]+1;
	}

	if(!redraw)
	{	/* skip the starting equal part */
		ep = wcp+lastx;
		for(wp = wcp+wx, sp = scp+wx; wp < ep; ++wp, ++sp)
			if(*wp != *sp)
				break;
		if((wx = wp-wcp) >= lastx)
			return OK;
#if _MULTIBYTE
		/* start update at an entire character */
		for(sp = scp+wx; wp > wcp; --wp, --sp)
			if(!ISCBIT(*wp) && !ISCBIT(*sp))
				break;
		wx = wp-wcp;
#endif

		/* skip the ending equal part */
		ep = wcp+wx;
		for(wp = wcp+lastx-1, sp = scp+lastx-1; wp >= ep; --wp, --sp)
			if(*wp != *sp)
				break;
		++wp;
#if _MULTIBYTE
		/* end update on an entire character */
		++sp;
		ep = wcp+scrco;
		for(; wp < ep; ++wp, ++sp)
			if(!ISCBIT(*wp) && !ISCBIT(*sp))
				break;
#endif
		lastx = wp - wcp;
	}

	/* place to do clear-eol */
	if(!T_el || T_xmc > 0 || endns >= lastx)
		blnkx = scrco;
	else if(_virtscr->_lastch[wy] == _BLANK)
		blnkx = -1;
	else
	{	ep = wcp+wx;
		for(wp = wcp+lastx-1; wp >= ep; --wp)
			if(DARKCHAR(*wp))
				break;

		/* avoid erasing the magic cookie */
		if(T_xmc > 0 && ATTR(*wp) != A_NORMAL)
			wp += 1;
#if _MULTIBYTE
		/* only clear whole screen characters */
		ep = scp+scrco;
		for(sp = scp+(wp-wcp)+1; sp < ep; ++sp)
			if(!ISCBIT(*sp))
				break;
		wp = wcp + (sp-scp) - 1;
#endif

		blnkx = wp-wcp;
		if(blnkx+C_el >= lastx)
			blnkx = scrco;
	}

	if(T_xmc < 0)
		normx = -1;
	else
	{	/* on cookie terminals, we may need to work more */
		if(blnkx < scrco)
			normx = blnkx+1;
		else
		{	ep = wcp+lastx;
			for(wp = wcp+scrco-1; wp >= ep; --wp)
				if(ATTR(*wp) != A_NORMAL)
					break;
			lastx = normx = (wp-wcp)+1;
			if(normx >= scrco)
				normx -= 1;
			if(T_xmc > 0 && wy == scrli-1 && normx == scrco-1)
				normx -= 1;
#if _MULTIBYTE
			ep = wcp+wx;
			for(wp = wcp+normx; wp >= ep; --wp)
				if(!ISCBIT(*wp))
					break;
#endif
		}
	}

	/* place for insert/delete chars */
#define SLACK	4
	if(redraw || (!_dchok && !_ichok) ||
	   endns < wx || (endns >= lastx && (scrco-lastx) > SLACK))
		idcx = scrco;
	else if(!marks)
		idcx = -1;
	else
	{	/* on cookie term, only do idch where no attrs are used */
		for(idcx = scrco-1, wp = wcp+idcx; idcx >= wx; --idcx, --wp)
			if(ATTR(*wp) || _ismark(wy,idcx) || idcx <= normx+1)
				break;
#if _MULTIBYTE
		for(; idcx < scrco; ++idcx, ++wp)
			if(!ISCBIT(*wp))
				break;
#endif
		if(idcx > 0)
			idcx += 1;
		if(idcx >= scrco-SLACK)
			idcx = scrco;
	}
	if(idcx < lastx && endns >= lastx)
		lastx = scrco;

	/* max amount of insert allow */
	if(idcx == scrco || !_ichok)
		maxi = 0;
	else if(lastx == scrco)
		maxi = scrco;
	else	maxi = lastx - (endns+1);

	/* go */
	ep   = wcp+lastx;
	wcp += wx;
	scp += wx;

	/* use clr_bol - to be safe, assume that it is exclusive  */
	if(!marks && T_el1 && blnkx > wx && begns >= wx)
	{
		/* find the initial blank interval */
		for(wp = wcp; wp < ep; ++wp)
			if(DARKCHAR(*wp))
				break;
		x = wx + (wp-wcp);
#if _MULTIBYTE
		/* clearing only whole screen characters */
		for(sp = scp+(x-wx); x >= wx; --x, --sp)
			if(!ISCBIT(*sp))
				break;
#endif
		x -= 1;
		if((x - (redraw ? 0 : begns)) > C_el1)
		{	mvcur(cy,cx,wy,x);
			cy = wy, cx = x;
			_puts(T_el1);

			mtch = x-wx;
			memcpy(scp,wcp,mtch*sizeof(chtype));
			wcp += mtch;
			scp += mtch;
			wx = x;
		}
	}

	/* move the cursor to the right place */
	mvcur(cy,cx,wy,wx);
	cy = wy, cx = wx;

	while(wx < lastx)
	{	/**/ ASSERT(cy == wy && cx == wx);
		/**/ ASSERT(!ISCBIT(*wcp));

		if(wx > blnkx)
		{	/* clear rest of line */
			_puts(T_el);
			curscr->_curx = wx;
			curscr->_cury = wy;
			wclrtoeol(curscr);

			if(marks && wx > 0 && ATTR(*(scp-1)) != A_NORMAL)
			{	_vids(A_NORMAL,ATTR(*(scp-1)));
				_setmark(wy,wx,NIL(chtype*),TRUE);
				if(T_xmc > 0)
					cx += T_xmc;
			}
			goto done;
		}

		/* check video attributes */
		if(marks)
			curscr->_attrs = ATTR(*scp);
		if((wc = ATTR(*wcp)) != curscr->_attrs)
		{	if(T_xmc >= 0 && wc != A_NORMAL && wx < normx)
			{	/* prevent spilling too far */
				mvcur(wy,wx,wy,normx);
				_vids(A_NORMAL,~A_NORMAL);
				_setmark(wy,normx,scp+normx-wx,TRUE);
				mvcur(wy,normx+(T_xmc > 0 ? T_xmc : 0),wy,wx);
				curscr->_attrs = ATTR(*scp);
				normx = -1;
			}

			if(T_xmc <= 0 || (wx == 0 && wc != A_NORMAL) ||
			   (wx > 0 && wc != ATTR(wcp[-1])) )
			{	_vids(ATTR(*wcp),curscr->_attrs);
				if(T_xmc >= 0)
				{	_setmark(wy,wx,scp,TRUE);
					if(T_xmc > 0)
					{	scp += T_xmc;
						wcp += T_xmc;
						wx += T_xmc;
						cx += T_xmc;
						goto next_char;
					}
					else if(*wcp == *scp && !redraw)
						goto next_char;
				}
			}
		}

		/* check the right margin */
#if _MULTIBYTE
		x = 1;
		if(_scrmax > 1)
		{	wc = RBYTE(*wcp);
			x = scrwidth[TYPE(wc)];
		}
#define ATMARGIN (wx == scrco-x)
#else
#define ATMARGIN (wx == scrco-1)
#endif
		if(ATMARGIN)
		{	_rmargin(wcp-wx,scp-wx,wx);
			goto done;
		}

		/* try insert/delete chars */
		if(wx > idcx &&
#if _MULTIBYTE
		   !ISCBIT(*scp) &&
#endif
		   (mtch = _useidch(wcp,scp,lastx-wx,maxi,&idch)))
		{
			maxi -= idch;
			wx  += mtch;
			scp += mtch;
			wcp += mtch;
			if(wx < lastx)
				mvcur(cy,cx,wy,wx), cx = wx;
			continue;
		}

		/* about to output characters, turn off insert mode */
		if(_insert)
			_offinsert();

		if(T_ul && T_eo && CHAR(*wcp) == '_')
		{	_putchar(' ');
			mvcur(wy,wx+1,wy,wx);
		}

		/* put out the character */
		wc = CHAR(*wcp);
		_putchar((T_hz && wc == '~') ? '`' : wc);

		/* just killed the cookie, reset map */
		if(T_xmc > 0 && _ismark(wy,wx))
		{	curscr->_attrs = wx > 0 ? ATTR(scp[-1]) : A_NORMAL;
			_setmark(wy,wx,scp,FALSE);
		}

		*scp++ = *wcp++;
		wx++; cx++;

#if _MULTIBYTE	/* output all of a multi-byte char */
		while(wx < lastx && ISCBIT(*wcp))
		{	_putchar(*wcp);

			if(T_xmc > 0 && _ismark(wy,wx) )
			{	/* this code takes care of cases like the tvi950.
				   There is also an outside chance that other cookie
				   terminals may erase a video mark when multi-column
				   chars are output over it. When that time comes,
				   we'll figure out what to do.
				*/
				curscr->_attrs = ATTR(scp[-1]);
				_setmark(wy,wx,scp,FALSE);
			}
			*scp++ = *wcp++;
			wx++; cx++;
		}
#endif

	next_char:
		if(!redraw && wx < lastx)
		{	/* skip equal chars */
			for(wp=wcp, sp=scp, x = wx; wp < ep; ++wp, ++sp, ++x)
				if(*wp != *sp ||
				   /* might as well erase this mark */
				   (T_xmc > 0 && _ismark(wy,x) && 
				    ATTR(*wp) == curscr->_attrs) )
					break;
#if _MULTIBYTE
			/* back up to the start of a whole character */
			if(wp > wcp && wp < ep)
				for(; wp > wcp; --wp, --sp, --x)
					if(!ISCBIT(*wp))
						break;
#endif
			if(x > wx+1)
			{	/* move to the place to continue refreshing */
				mvcur(cy,cx,wy,x);
				cx = wx = x;
				wcp = wp; scp = sp;
			}
		}
	}

done:
	/* update the blank structure */
	scp = curscr->_y[wy];
	ep = scp + scrco;
	for(sp = scp; sp < ep; ++sp)
		if(DARKCHAR(*sp))
			break;
	_begns[wy] = sp-scp;

	if(sp == ep)
		_endns[wy] = -1;
	else
	{	for(sp = scp+scrco-1; sp >= scp; --sp)
			if(DARKCHAR(*sp))
				break;
		_endns[wy] = sp-scp;
	}

	/* update the hash structure */
	_curhash[wy] = _begns[wy] < scrco ? _NOHASH : 0;

	return OK;
}


#if __STD_C
int wrefresh(WINDOW* win)
#else
int wrefresh(win)
WINDOW	*win;
#endif
{
	reg short	*bnsch, *ensch;
	reg SLK_MAP	*slk;
	reg chtype	*hs;
	int		wx, wy, nc,
			boty, clby, idby, curwin;

	/* to simplify code in some cases */
	curwin = (win == curscr);
	marks = _marks;
	scrli = curscr->_maxy;
	scrco = curscr->_maxx;
	slk = _curslk;

	if(_endwin)
	{	/* make sure we're in program mode */
		reset_prog_mode();
		curs_set(_cursstate);
		_puts(T_smcup);
		_puts(T_enacs);
		if(slk)
			(*_do_slk_tch)();
		_endwin = FALSE;
	}

	/* don't allow curscr refresh if the screen was just created */
	if(curwin && curscr->_sync)
		return OK;

	/* go thru _stdbody */
	if(!curwin && win != _virtscr)
		wnoutrefresh(win);

	/* if there is typeahead */
	if((_inputpending = chkinput()) == TRUE)
	{	if(curwin)
			curscr->_clear = TRUE;
		return OK;
	}

	if(curwin || curscr->_clear)
		_virtscr->_clear = TRUE;

	/* save curscr cursor coordinates */
	cy = curscr->_cury;
	cx = curscr->_curx;

	/* clear the screen if required */
	if(_virtscr->_clear)
	{	_puts(T_clear);
		cy = cx = curscr->_curx = curscr->_cury = 0;

		/* _sync indicates that this a new screen */
		if(!curscr->_sync)
			werase(curscr);
		else
		{	nc = scrco/CHARSIZE - (scrco%CHARSIZE ? 0 : 1);
			wy = scrli-1;
			bnsch = _begns; ensch = _endns;
			hs = _curhash;
			for(; wy >= 0; --wy)
			{	*bnsch++ = scrco;
				*ensch++ = -1;
				*hs++ = 0;
				if(marks)
				{	for(wx = nc; wx >= 0; --wx)
						marks[wy][wx] = 0;
				}
			}
		}

		_virtscr->_clear = curscr->_sync = curscr->_clear = FALSE;
		if(slk)
			(*_do_slk_tch)();

		/* pretend _virtscr has been totally changed */
		wtouchln(_virtscr,0,scrli,-1);
		_virttop = 0;
		_virtbot = scrli-1;

		/* will not do clear-eod or ins/del lines */
		clby = idby = scrli;
	}
	else	clby = idby = -1;

	/* software soft labels */
	if(slk && slk->_win && slk->_changed)
		(*_do_slk_noref)();

	/* do line updating */
	_virtscr->_clear = FALSE;
	wy    = _virttop;
	boty  = _virtbot+1;
	bnsch = _virtscr->_firstch+wy;
	ensch = _virtscr->_lastch+wy;

	for(; wy < boty; ++wy, ++bnsch, ++ensch)
	{	/* this line is up-to-date */
		if(*bnsch >= scrco)
			goto next;

		if(!curwin && (_inputpending = chkinput()) == TRUE)
		{	/* there is type-ahead */
			_virttop = wy;
			goto done;
		}

		if(clby < 0)
		{	/* now we have to work, check for ceod */
			clby = _getceod(wy,boty);

			/* check for insert/delete lines */
			if(_virtscr->_idln)
				idby = (*_setidln)(wy,boty);
		}

		/* try clear-to-eod */
		if(wy == clby)
			_useceod(wy,boty);

		/* try ins/del lines */
		if(wy == idby)
		{	curscr->_cury = cy; curscr->_curx = cx;
			(*_useidln)();
			cy = curscr->_cury; cx = curscr->_curx;
		}

		if(*bnsch < scrco)
			_updateln(wy);

	next:
		*bnsch = _INFINITY, *ensch = -1;
	}

	/* do hardware soft labels */
	if(slk && slk->_changed && !(slk->_win))
		(*_do_slk_ref)();

	/* move cursor */
	wy = _virtscr->_cury;
	wx = _virtscr->_curx;
	if(wy != cy || wx != cx)
	{	mvcur(cy,cx,wy,wx);
		cy = wy, cx = wx;
	}

	/* reset the flags */
	curscr->_clear = FALSE;
	_virtscr->_idln = FALSE;
	_inputpending = FALSE;

	/* virtual image is now up-to-date */
	_virttop = scrli;
	_virtbot = -1;

done :
	curscr->_cury = cy; curscr->_curx = cx;
	_tflush();
	return OK;
}
