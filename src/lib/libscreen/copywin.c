#include	"scrhdr.h"



/*
**	Overlay/overwrite the content of the source window 'src'
**	on that of the target window 'tar'.
**	If any of the coord arguments is negative, the area to copy
**	from and copy to are computed from the overlapping area as
**	defined by the screen coordinates.
**
**	sby, sbx: the start coords in srcw to copy
**	tby, tbx, tey, tex: the area to copy to in the target window.
**	flag : TRUE if only overlaying.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int copywin(WINDOW* src, WINDOW* tar,
	int sby, int sbx, int tby, int tbx, int tey, int tex, int flag)
#else
int copywin(src,tar,sby,sbx,tby,tbx,tey,tex,flag)
WINDOW	*src, *tar;
int	sby, sbx, tby, tbx, tey, tex, flag;
#endif
{
	reg chtype	**scp, **tcp, *sc, *tc, *se, *te;
	reg int		sey, sex, t, b, l, r, slowcpy;
	reg short	*begch, *endch;
#if _MULTIBYTE
	reg chtype	wc;
#endif

	if(sby < 0 || sbx < 0 || tby < 0 || tbx < 0 || tey < 0 || tex < 0)
	{	/* compute overlapping screen area */
		sby = src->_begy;	sey = sby + src->_maxy;
		sbx = src->_begx;	sex = sbx + src->_maxx;
		tby = tar->_begy;	tey = tby + tar->_maxy;
		tbx = tar->_begx;	tex = tbx + tar->_maxx;
		t   = MAX(sby,tby);	b   = MIN(sey,tey);
		l   = MAX(sbx,tbx);	r   = MIN(sex,tex);

		/* switch back to window coordinates */
		sey = b - sby;		sby = t - sby;
		sex = r - sbx;		sbx = l - sbx;
		tey = b - tby;		tby = t - tby;
		tex = r - tbx;		tbx = l - tbx;
	}
	else
	{	/* make sure target area is in bound */
		tey = tey < tar->_maxy ? tey+1 : tar->_maxy;
		tex = tex < tar->_maxx ? tex+1 : tar->_maxx;

		/* compute the other end of the source area */
		if((sey = sby + (tey-tby)) > src->_maxy)
		{	sey = src->_maxy;
			tey = tby + (sey-sby);
		}
		if((sex = sbx + (tex-tbx)) > src->_maxx)
		{	sex = src->_maxx;
			tex = tbx + (sex-sbx);
		}
	}

	/* error conditions */
	if(tar == curscr || sey <= sby || sex <= sbx || tey <= tby || tex <= tbx)
		return ERR;
		
	/* now do the copying */
	scp = src->_y + sby;
	tcp = tar->_y + tby;
	begch = tar->_firstch + tby;
	endch = tar->_lastch + tby;
	sex -= 1;
	tex -= 1;

	slowcpy = flag || (tar->_bkgd != BLNKCHAR);

	for(; sby < sey; ++sby, ++scp, ++tby, ++tcp, ++begch, ++endch)
	{	/* update the change structure */
		if(*begch  > tbx)
			*begch = tbx;
		if(*endch < tex)
			*endch = tex;

		/* the intervals to copy from and to */
		sc = *scp + sbx;
		se = *scp + sex;
		tc = *tcp + tbx;
		te = *tcp + tex;

#if _MULTIBYTE
		/* only copy into an area bounded by whole characters */
		for(; tc <= te; ++tc, ++sc)
			if(!ISCBIT(*tc))
				break;
		if(tc > te)
			continue;

		for(; te >= tc; --te, --se)
			if(!ISCBIT(*te))
				break;
		wc = RBYTE(*te);
		t = scrwidth[TYPE(wc)] - 1;
		if(te+t <= *tcp+tex)
			te += t, se += t;
		else	te -= 1, se -= 1;
		if(te < tc)
			continue;

		/* don't copy partial characters */
		for(; sc <= se; ++sc, ++tc)
			if(!ISCBIT(*sc))
				break;
		if(sc > se)
			continue;

		for(; se >= sc; --se, --te)
			if(!ISCBIT(*se))
				break;
		wc = RBYTE(*se);
		t = scrwidth[TYPE(wc)] - 1;
		if(se+t <= *scp+sex)
			se += t, te += t;
		else	se -= 1, te -= 1;
		if(se < sc)
			continue;

		/* make sure that the copied-to place is clean */
		if(ISMBYTE(*tc))
			_mbclrch(tar,tby,tc - *tcp);
		if(ISMBYTE(*te))
			_mbclrch(tar,tby,te - *tcp);
#endif

		/* copy data */
		if(slowcpy)
		{	for(; sc <= se; ++sc, ++tc)
			{	if(!flag || *sc != BLNKCHAR)
				{	chtype sa;
					sa = ATTR(*sc);
					*tc = WCHAR(tar,CHAR(*sc))|sa;
				}
			}
		}
		else	memcpy(tc,sc,((se-sc)+1)*sizeof(chtype));
	}

	if(tar->_sync && tar->_parent)
		wsyncup(tar);

	return tar->_immed ? wrefresh(tar) : OK;
}
