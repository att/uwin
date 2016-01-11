#include	"scrhdr.h"

/*
**	Duplicate a window
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
WINDOW* dupwin(WINDOW* win)
#else
WINDOW* dupwin(win)
WINDOW*	win;
#endif
{
	reg int		i, nl, nc;
	reg chtype	**wcp, **ncp;
	WINDOW		*w;

	nl = win->_maxy;
	nc = win->_maxx;
	if(!(w = _makenew(nl,nc,win->_begy,win->_begx)) )
		return NIL(WINDOW*);
	if(_image(w) == ERR)
		return NIL(WINDOW*);

	for(i = 0, wcp = win->_y, ncp = w->_y; i < nl; ++i, ++wcp, ++ncp)
	{
#if _MULTIBYTE
		reg chtype	*ws, *we, *ns, *ne, wc;
		reg int		n;

		ws = *wcp;
		we = ws + nc - 1;

		/* skip partial characters */
		for(; ws <= we; ++ws)
			if(!ISCBIT(*ws))
				break;
		for(; we >= ws; --we)
			if(!ISCBIT(*we))
				break;
		if(we >= ws)
		{
			wc = *we;
			n = scrwidth[TYPE(wc)];
			if((we + n) <= (*wcp + nc))
				we += n;

			ns = *ncp + (ws - *wcp);
			ne = *ncp + (we - *wcp);
			memcpy((char*)ns,(char*)ws,(ne-ns)*sizeof(chtype));
		}
		else	ns = ne = *ncp + nc;

		/* fill the rest with background chars */
		wc = win->_bkgd;
		for(ws = *ncp; ws < ns; ++ws)
			*ws = wc;
		for(ws = *ncp+nc-1; ws >= ne; --ws)
			*ws = wc;
#else

		memcpy((char*)(*ncp),(char*)(*wcp),nc*sizeof(chtype));
#endif
	}

	memcpy((char*)(w->_firstch),(char*)(win->_firstch),nl*sizeof(short));
	memcpy((char*)(w->_lastch),(char*)(win->_lastch),nl*sizeof(short));

	w->_curx = win->_curx; w->_cury = win->_cury;
	w->_tmarg = win->_tmarg; w->_bmarg = win->_bmarg;
	w->_pminy = win->_pminy; w->_pminx = win->_pminx;
	w->_sminy = win->_sminy; w->_sminx = win->_sminx;
	w->_smaxy = win->_smaxy; w->_smaxx = win->_smaxx;
	w->_yoffset = win->_yoffset;

	w->_attrs = win->_attrs;
	w->_bkgd = win->_bkgd;

	w->_delay = win->_delay;

	w->_clear = win->_clear;
	w->_leeve = win->_leeve;
	w->_scroll = win->_scroll;
	w->_immed = win->_immed;
	w->_idln = win->_idln;
	w->_meta = win->_meta;
	w->_keypad = win->_keypad;
	w->_notimeout = win->_notimeout;

#if _MULTIBYTE
	w->_index = win->_index;
	w->_nbyte = win->_nbyte;
	w->_insmode = win->_insmode;
	memcpy((char*)(w->_waitc),(char*)(win->_waitc),_csmax*sizeof(char));
#endif

	return w;
}
