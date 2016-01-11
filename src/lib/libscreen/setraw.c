#include	"termhdr.h"


/*
**	Set/unset raw mode
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int _setraw(int yes)
#else
int _setraw(yes)
int yes;
#endif
{
#if _hdr_termio
#define COOKED_INPUT	(IXON|IGNBRK|BRKINT|PARMRK)
	if(yes)
	{	_curtty.c_lflag &= ~(ICANON|ISIG|IEXTEN);
		_curtty.c_iflag &= ~(COOKED_INPUT|ICRNL|INLCR|IGNCR|ISTRIP);
		_curtty.c_oflag &= ~(OPOST);
		_curtty.c_cc[VMIN] = 1;
		_curtty.c_cc[VTIME] = 0;
	}
	else
	{	_curtty.c_lflag |= ICANON|ISIG|IEXTEN;
		_curtty.c_iflag |= COOKED_INPUT|ISTRIP;
		_curtty.c_oflag |= OPOST;
		_curtty.c_cc[VEOF] = 0x04;
		_curtty.c_cc[VEOL] = 0x00;
	}
#else
	if(yes)
	{	_curtty.sg_flags |= RAW|CBREAK;
		_curtty.sg_flags &= ~CRMOD;
	}
	else
	{	_curtty.sg_flags &= ~(RAW|CBREAK);
		_curtty.sg_flags |= CRMOD;
	}
#endif
#ifdef TCHARS
	if(yes)
	{	_curchars.t_eofc = 0;
		_curchars.t_brkc = 0;
	}
	else
	{	_curchars.t_eofc = _orgchars.t_eofc;
		_curchars.t_brkc = _orgchars.t_brkc;
	}
	if(SETCHARS(_curchars) < 0)
		return ERR;
#endif
#ifdef LTCHARS
	if(yes)
	{	_curlchars.t_rprntc = 0;
		_curlchars.t_flushc = 0;
		_curlchars.t_werasc = 0;
		_curlchars.t_lnextc = 0;
	}
	else
	{	_curlchars.t_rprntc = _orglchars.t_rprntc;
		_curlchars.t_flushc = _orglchars.t_flushc;
		_curlchars.t_werasc = _orglchars.t_werasc;
		_curlchars.t_lnextc = _orglchars.t_lnextc;
	}
	if(SETLCHARS(_curlchars) < 0)
		return ERR;
#endif
#ifdef TAUXIL
	if(yes)
		_curtauxil.tc_usest = (char)(-1);
	else	_curtauxil.tc_usest = _orgtauxil.tc_usest;
	if(SETTAUXIL(_curtauxil) < 0)
		return ERR;
#endif
	return SETTY(_curtty) < 0 ? ERR : _tty_mode(&(_curtty));
}
