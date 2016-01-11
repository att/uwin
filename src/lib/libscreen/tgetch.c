#include	"termhdr.h"


/*
**	Read a key typed from the terminal
**
**	cntl:	= 0 for single-char key only
**		= 1 for matching function key and macro patterns.
**		= 2 same as 1 but no time-out for funckey matching.
**
**	Written by Kiem-Phong Vo
*/


/* Read a set of bytes into the input buffer */
#if __STD_C
static int _rdinput(reg int waittm,reg int n)
#else
static int _rdinput(waittm,n)
reg int	waittm, n;
#endif
{	reg uchar	*cp, *bp, *ep;
	reg int		m;

	if(_inputcur+n > _inputend && _inputbeg > _inputbuf)
	{
		bp = _inputbuf;
		cp = _inputbeg;
		ep = _inputcur;
		while(cp < ep)
			*bp++ = *cp++;
		_inputcur = _inputbuf + (_inputcur - _inputbeg);
		_inputbeg = _inputbuf;
	}
	if(n > (m = _inputend - _inputcur))
		n = m;
	if((n = _rdtimeout(_inputd,waittm,_inputcur,n)) > 0)
		_inputcur += n;
	return n;
}

/* Insert a string into the input buffer */
#if __STD_C
static int ungetstr(reg uchar* s, reg int n)
#else
static int ungetstr(s,n)
reg uchar	*s;
reg int		n;
#endif
{	reg uchar	*cp, *ep, *ip;

	if(_inputbuf+n > _inputbeg)
	{	/* shift data to get space */
		reg int		m;

		m = _inputcur - _inputbeg;
		if((n+m) > _inputend - _inputbuf)
			return ERR;

		ip = _inputcur - 1;
		cp = ip + (n - (_inputbeg - _inputbuf));
		ep = _inputbeg;
		while(ip >= ep)
			*cp-- = *ip--;
		_inputbeg = cp+1;
		_inputcur = _inputbeg + m;
	}

	ip = s+n-1;
	cp = _inputbeg-1;
	while(ip >= s)
		*cp-- = *ip--;
	_inputbeg = cp+1;

	return n;
}


/* Read next character in escape sequence sent by terminal's mouse */
static int _mgetch()
{	reg int 	c;

	/* need more input */
	if(_inputcur > _inputend)
		if((_rdinput(-1,16)) <= 0)		/* get more characters */
			return ERR;

	/* return the next input char */
	c = *_inputcur++;				/* not multi-byte chars */
	return c;
}


static	char 	*M_es=NIL(char*);		/* data in last mouse escape sequence */

/* Remember current escape sequence just sent by terminal's mouse */
#if __STD_C
static void _mlog(uchar* beg, uchar* end)
#else
static void _mlog(beg,end)
uchar *beg, *end;
#endif
{	register	uchar 	*cp;
 	register	char 	*mp;

	if( M_es )
		free( M_es );

	if( !beg || !*beg || !end || !*end || end < beg ||
		!(M_es = (char *) malloc((end-beg+2))) )
	{	M_es = NIL(char*);
		return;
	}

	cp = beg;
	mp = M_es;
	while(cp <= end)
		*mp++ = (*cp++)&CMASK;
	*mp = '\0';
}

/* Share most-recent escape sequence just sent by terminal's mouse */
#if __STD_C
char* tdupmes(void)
#else
char* tdupmes()
#endif
{	return M_es;
}


/* fractions of a second measured in millisecs for read timeouts */
#define FAST        200
#define SLOW        400
#define STONE       1000


#if __STD_C
int tgetch(int cntl)
#else
int tgetch(cntl)
int	cntl;
#endif
{	reg int		waittm, i, n;
	reg KEY_MAP	*kp;
	reg int		do_read;

	if(_inputd < 0 || _inputeof)
		return ERR;

	if(_ahead == _AHEADPENDING)
		_ahead = _inputd;

	/* time-out period for fast reads (_MULTIBYTE or function keys) */
	waittm = baudrate() >= 9600 ? FAST : baudrate() >= 1200 ? SLOW : STONE;

	/* get first set of bytes */
	do_read = TRUE;
	if(_inputbeg >= _inputcur)
	{
		_inputbeg = _inputcur = _inputbuf;
#if _MULTIBYTE
		_literal = 0;
#endif
		if((n = _rdtimeout(_inputd,_timeout,_inputbeg,IBUFSIZE)) <= 0)
			return ERR;
		_inputcur += n;
	}
#if _MULTIBYTE	/* returning a byte from a multi-byte char */
	else if(_literal > 0)
		goto done;
#endif

	/* error or no key matching */
	if(_inputbeg >= _inputcur || cntl < 1 || cntl > 2 || !_keymap)
		goto done;

#if _MULTIBYTE	/* make sure that the whole char is in */
	i = *_inputbeg;
	if(ISMBIT(i))
	{	n = TYPE(i);
		n = (cswidth[n] - (n == 0 ? 1 : 0)) - (_inputcur - _inputbeg);
		while(n > 0)
		{	/* read the remaining bytes */
			if((i = _rdtimeout(_inputd,-1,_inputcur,n)) <= 0)
			{	_inputcur = _inputbeg;
				goto done;
			}
			n -= i;
		}
	}
#endif

	/* no time-out */
	if(cntl != 1)
		waittm = -1;

	/* walk thru the key map and matches keys or macros */
rematch:
	for(kp = _keymap; kp ; kp = kp->_next)
	{	/* see if the key or macro is a potential match */
		if(kp->_pat[0] != _inputbeg[0])
			continue;

		_macroerr = FALSE;
		for(i = 1;; ++i)
		{
			if(kp->_pat[i] == '\0')
			{	/* found a match, adjust key table */
				(*_adjkey)(kp);
				_inputbeg += i;

				if(!kp->_exp)
					if(kp->_key == KEY_MOUSE  &&  MOUSE_ON())
					{	/* de-instantiate the  mouse_info  input string
						** via the mouse driver function, which calls
						** _mgetch()  to consume characters from
						** the mouse escape sequence.
						*/
						reg int rv;
						reg uchar	*beg;
						beg = _inputcur = _inputbeg;
						rv = ((cur_term->_mouse_f) != NIL(Mdriver_f)) ?
						    (*(cur_term->_mouse_f))( curmouse, _mgetch ) : ERR;

						/* remember mouse escape sequence for input logging */
						_mlog(beg,_inputcur-1);
						_inputbeg = _inputcur;

						/* check if mouse button event occurred on top of
						** a screen label key mapped to a function key
						*/

							/**  ???  SOON  ???  **/

						return rv;
					}
					else
						return kp->_key;
				/* macro expansion */
				else if(ungetstr(kp->_exp,kp->_key) != ERR)
					goto rematch;
				/* too bad */
				else	return ERR;
			}

			/* need more inputs */
			if((_inputbeg+i) >= _inputcur)
			{	if(!do_read)
					break;
				n = (kp->_macro || waittm < 0) ? _timeout : waittm;
				if((n = _rdinput(n,16)) <= 0)
				{	do_read = FALSE;
					if(n < 0 && errno && kp->_macro)
						_macroerr = TRUE;
					break;
				}
			}

			/* see if the match is still good */
			if(kp->_pat[i] != _inputbeg[i])
				break;
		}

		/* try read again for macros */
		if(!do_read && !kp->_macro && kp->_next && kp->_next->_macro)
			do_read = TRUE;
	}

done:
	if(_inputbeg >= _inputcur || (!_inputeof && _macroerr) )
		return ERR;
	else
	{
		i = (*_inputbeg)&CMASK;
		_inputbeg += 1;
#if _MULTIBYTE
		if(_literal > 0)
			_literal -= 1;
		else if(ISMBIT(i))
		{	/* number of coming bytes to return as is */
			n = TYPE(i);
			_literal = cswidth[n] - (n == 0 ? 1 : 0);
		}
#endif
		return i;
	}
}
