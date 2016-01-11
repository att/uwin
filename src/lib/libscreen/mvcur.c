#include	"scrhdr.h"


/*
**	Move the cursor optimally from (cury,curx) to (newy,newx).
**	If (cury,curx) are bad, don't try relative cursor movement.
**
**	Written by Kiem-Phong Vo
*/

#define	H_UP		-1
#define	H_DO		1

#define Q_NOT		0
#define Q_HOMEUP	1
#define Q_HOMEDOWN	2
#define Q_HOMELEFT	3
#define Q_UP		4
#define Q_DOWN		5
#define	Q_LEFT		6
#define Q_RIGHT		7

static int	Newy;

/*
**	Move vertically
*/
#if __STD_C
static int mvvert(int cy, int ny, int doit)
#else
static int mvvert(cy,ny,doit)
int	cy, ny, doit;
#endif
{
	reg char	*ve;
	reg int		dy, st, cv, savnonl;

	if(cy == ny)
		return 0;

	/* cost of stepwise movement */
	if(cy < ny)
	{
		dy = ny-cy;
		ve = T_cud ? T_cud : T_cud1;
		st = T_cud ? C_cud : C_cud1 * dy;
	}
	else
	{
		dy = cy-ny;
		ve = T_cuu ? T_cuu : T_cuu1;
		st = T_cuu ? C_cuu : C_cuu1 * dy;
	}

	/* cost of using vertical move */
	cv = C_vpa;

	/* if calculating cost only */
	if(!doit)
		return st < cv ? st : cv;

	/* do it */
	if(cv < st)
		_puts(tstring(T_vpa,ny,-1));
	else if((cy < ny && T_cud) || (cy > ny && T_cuu))
		_puts(tstring(ve,dy,-1));
	else
	{
		if(_insert)
			_offinsert();
		if(!(savnonl = _nonlcr) && ve[0] == '\n')
			nonl();
		for(; dy > 0; --dy)
			_puts(ve);
		if(!savnonl && ve[0] == '\n')
			nl();
	}

	return 0;
}


/*
**	Move right.
*/
#if __STD_C
static int mvright(int cx, int nx, int doit)
#else
static int mvright(cx,nx,doit)
int	cx, nx, doit;
#endif
{
	reg uchar	*mks;
	reg chtype	*scp;
	reg int		nt, tx, x, stcost;

	if(!T_cuf1 && !T_cuf)
		return LARGECOST;

	/* number of tabs used in stepwise movement */
	if(T_cuf1)
	{
		nt = (!_xtab && !T_xt && T_ht) ? (nx/TABSIZ - cx/TABSIZ) : 0;
		tx = (nt > 0) ? (cx/TABSIZ + nt)*TABSIZ : cx;

		scp = curscr->_y[Newy];
		mks = _marks ? _marks[Newy] : NIL(uchar*);

		/* calculate stepwise cost */
#define _ismark(x)	(mks[x/CHARSIZE] & (1 << (x%CHARSIZE)))
		stcost = nt * C_ht;
		for(x = tx; x < nx; ++x)
		{
#if _MULTIBYTE
			stcost += C_cuf1;
#else
			if(T_xmc == 0 || (T_xmc < 0 && ATTR(scp[x]) == curscr->_attrs) )
				stcost += 1;
			else	stcost += C_cuf1;
#endif
		}
	}
	else	stcost = LARGECOST;

	if(!doit)
		return (C_cuf < stcost) ? C_cuf : stcost;

	/* actually move */
	if(C_cuf < stcost)
		_puts(tstring(T_cuf,nx-cx,-1));
	else
	{
		if(_insert)
			_offinsert();
		for(; nt > 0; --nt)
			_puts(T_ht);
		for(x = tx; x < nx; ++x)
		{
#if _MULTIBYTE
			_puts(T_cuf1);
#else
			if(T_xmc == 0 || (T_xmc < 0 && ATTR(scp[x]) == curscr->_attrs) )
				_putchar(CHAR(scp[x]));
			else	_puts(T_cuf1);
#endif
		}
	}

	return 0;
}



/*
**	Move left
*/
#if __STD_C
static int mvleft(int cx, int nx, int doit)
#else
static int mvleft(cx,nx,doit)
int	cx, nx, doit;
#endif
{
	reg int		tx, nt, x, stcost;

	if(!T_cub1 && !T_cub)
		return LARGECOST;

	/* stepwise cost */
	if(T_cub1)
	{
		tx = cx;
		nt = 0;
		if(!_xtab && !T_xt && T_cbt)
		{
			/* the TAB position >= nx */
			x = (nx%TABSIZ) ? (nx/TABSIZ + 1)*TABSIZ : nx;

			/* # of tabs used and position after using them */
			if(x < cx)
			{
				nt = (cx/TABSIZ - x/TABSIZ) +
				     ((cx%TABSIZ) ? 1 : 0);
				tx = x;
			}
		}
		stcost = nt*C_cbt + (tx-nx)*C_cub1;
	}
	else	stcost = LARGECOST;

	/* get cost only */
	if(!doit)
		return (C_cub < stcost) ? C_cub : stcost;

	/* doit */
	if(C_cub < stcost)
		_puts(tstring(T_cub,cx-nx,-1));
	else
	{
		if(_insert)
			_offinsert();
		for(; nt > 0; --nt)
			_puts(T_cbt);
		for(; tx > nx; --tx)
			_puts(T_cub1);
	}

	return 0;
}


/*
**	Move horizontally
*/
#if __STD_C
static int mvhor(int cx, int nx, int doit)
#else
static int mvhor(cx,nx,doit)
int	cx, nx, doit;
#endif
{
	reg int		st, ch, hl;

	if(cx == nx)
		return 0;

	/* cost using horizontal move */
	ch = C_hpa;

	/* cost doing stepwise */
	st = cx < nx ? mvright(cx,nx,FALSE) : mvleft(cx,nx,FALSE);
	if(st > LARGECOST)
		st = LARGECOST;

	/* cost homeleft first */
	hl = (_nonlcr && T_cr) ? C_cr + mvright(0,nx,FALSE) : LARGECOST;
	if(hl > LARGECOST)
		hl = LARGECOST;

	if(!doit)
		return (ch < st && ch < hl) ? ch : (hl < st ? hl : st);

	if(ch < st && ch < hl)
		_puts(tstring(T_hpa,nx,-1));
	else if(hl < st)
	{
		if(_insert)
			_offinsert();
		_puts(T_cr);
		mvright(0,nx,TRUE);
	}
	else
	{
		if(cx < nx)
			mvright(cx,nx,TRUE);
		else	mvleft(cx,nx,TRUE);
	}
	return 0;
}


/*
**	Move relatively
*/
#if __STD_C
static int mvrel(int cy, int cx, int ny, int nx, int doit)
#else
static int mvrel(cy,cx,ny,nx,doit)
int	cy, cx, ny, nx, doit;
#endif
{
	reg int		cv, ch;

	/* do in this order since mvhor may need the curscr image */
	cv = mvvert(cy,ny,doit);
	ch = mvhor(cx,nx,doit);

	return cv+ch;
}


/*
**	Move by homing first.
*/
#if __STD_C
static int homefirst(int ny, int nx, int type, int doit)
#else
static int homefirst(ny,nx,type,doit)
int	ny, nx, type, doit;
#endif
{
	reg char	*home;
	reg int		cy, cost;

	if(type == H_UP)
	{
		home = T_home;
		cost = C_home;
		cy = 0;
	}
	else
	{
		home = T_ll;
		cost = C_ll;
		cy = curscr->_maxy-1;
	}

	if(!home)
		return LARGECOST;
	if(!doit)
		return cost + mvrel(cy,0,ny,nx,FALSE);

	_puts(home);
	return mvrel(cy,0,ny,nx,TRUE);
}


#if __STD_C
int mvcur(int cury,int curx, int newy, int newx)
#else
int mvcur(cury,curx,newy,newx)
int	cury, curx, newy, newx;
#endif
{
	reg int		hu, hd, rl, cm, maxy, maxx, quick;

	/* obvious case */
	if(cury == newy && curx == newx)
		return OK;

	maxy = curscr->_maxy-1;
	maxx = curscr->_maxx-1;
	Newy = newy;

	/* can't do cursor movement */
	if(_endwin ||
	   newy < 0 || newy > maxy || newx < 0 || newx > maxx)
		return ERR;

	/* flag for absolute movement only */
	if(curx < 0 || cury < 0 || curx > maxx || cury > maxy)
		curx = -1;

	/* must turn off video attributes */
	if(curscr->_attrs != A_NORMAL && (curx < 0 || (!T_msgr && !_marks)))
		_vids(A_NORMAL,curscr->_attrs);

	/* must turn off insert mode */
	if(_insert && (curx < 0 || !T_mir))
		_offinsert();

	/* cost of using cm */
	cm = C_cup;

	/* initialize quick to not try any quick move */
	quick = Q_NOT;

	/* must use absolute movement */
	if(curx < 0)
	{
		rl = LARGECOST;
		if(cm < LARGECOST)
			hu = hd = LARGECOST;
		else	goto fromhome;
	}

	/* at high baud rate, we don't need to get real good cost estimates */
	else if(cm < LARGECOST && baudrate() >= 2400)
	{
		hu = hd = LARGECOST;

		rl = 0;
		if(C_home < cm && newx == 0 && newy == 0)
			quick = Q_HOMEUP;
		else if(C_ll < cm && newx == 0 && newy == maxy)
			quick = Q_HOMEDOWN;
		else if(C_cr < cm && _nonlcr && newx == 0 &&
			(newy == cury ||
			 (newy == cury-1 && C_cuu1 < cm) ||
			 (newy == cury+1 && C_cud1 < cm) ) )
			quick = Q_HOMELEFT;
		else if(C_cud1 < cm && newx == curx && newy == (cury+1))
			quick = Q_DOWN;
		else if(C_cuu1 < cm && newx == curx && newy == (cury-1))
			quick = Q_UP;
		else if(cury == newy && newx >= curx-4 && newx <= curx+4)
		{
			rl = newx < curx ? mvleft(curx,newx,FALSE) :
					   mvright(curx,newx,FALSE);
			if(rl < cm)
				quick = newx < curx ? Q_LEFT : Q_RIGHT;
		}
		else	rl = LARGECOST;
	}
	else
	{
		/* cost using relative movements */
		rl = mvrel(cury,curx,newy,newx,FALSE);

	fromhome:
		/* cost of homing to upper-left corner first */
		hu = T_home ? homefirst(newy,newx,H_UP,FALSE) : LARGECOST;

		/* cost of homing to lower-left corner first */
		hd = T_ll ? homefirst(newy,newx,H_DO,FALSE) : LARGECOST;
	}

	/* can't do any one of them */
	if(cm >= LARGECOST && rl >= LARGECOST &&
	   hu >= LARGECOST && hd >= LARGECOST)
		return ERR;

	/* do the best one */
	if(cm <= rl && cm <= hu && cm <= hd)
	{
		if(istermcap())
			_puts(tstring(T_cup,newx,newy));
		else	_puts(tstring(T_cup,newy,newx));
	}
	else if(hu < rl || hd < rl)
		homefirst(newy,newx,hu <= hd ? H_UP : H_DO,TRUE);
	else switch(quick)
	{
	case Q_HOMEUP:
		_puts(T_home);
		break;
	case Q_HOMEDOWN:
		_puts(T_ll);
		break;
	case Q_HOMELEFT :
		if(_insert)
			_offinsert();
		_puts(T_cr);
		if(newy == cury-1)
			_puts(T_cuu1);
		else if(newy == cury+1)
			_puts(T_cud1);
		break;
	case Q_UP:
		_puts(T_cuu1);
		break;
	case Q_DOWN :
		if(_insert)
			_offinsert();
		_puts(T_cud1);
		break;
	case Q_LEFT :
		mvleft(curx,newx,TRUE);
		break;
	case Q_RIGHT :
		mvright(curx,newx,TRUE);
		break;
	default:
		mvrel(cury,curx,newy,newx,TRUE);
		break;
	}

	/* update cursor position */
	curscr->_curx = newx;
	curscr->_cury = newy;

	if(curx < 0)
		_tflush();

	return OK;
}
