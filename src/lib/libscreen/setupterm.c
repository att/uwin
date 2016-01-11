#include	"termhdr.h"


/*
**	Set up a new terminal
**	name:	the name of the new terminal
**	outd:	the output descriptor for this terminal,
**		outd < 0 if only the termcap/terminfo entry
**		is required (for example, if called from tgetent()).
**	errret: to return an error status (1 ok, -1 bad)
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
TERMINAL *setupterm(char* name, int outd, int* errret)
#else
TERMINAL *setupterm(name,outd,errret)
char	*name;
int	outd, *errret;
#endif
{
	TERMINAL	*ntrm, *savterm;
	reg char	*env;

#if _MULTIBYTE
	reg int		i;

	/* get the byte and column sizes of character sets */
	if(cswidth[0] < 0)
	{
		cswidth[0] = 1;
		if((env = getenv("CSWIDTH")) )
		{
			for(i = 0; i < 3 && *env != '\0'; ++i)
			{
				/* set cswidth if it is defined */
				if(isdigit(*env))
				{
					cswidth[i] = *env - '0';
					++env;
				}

				/* set scrwidth if it is defined */
				scrwidth[i] = -1;
				if(*env == ':')
				{
					++env;
					if(isdigit(*env))
					{
						scrwidth[i] = *env - '0';
						++env;
					}
				}
				/* if not defined, it is the same as cswidth */
				if(scrwidth[i] == -1)
					scrwidth[i] = cswidth[i];

				/* skip to next field */
				for(; *env != '\0'; ++env)
					if(*env == ',' || *env == ';')
					{
						++env;
						break;
					}
			}
		}
	}
#endif

	savterm = cur_term;

	/* get new structure for terminal */
	if(_f_unused)
		ntrm = &_f_term;
	else if(!(ntrm = (TERMINAL*) malloc(sizeof(TERMINAL))) )
		goto err;
	else
	{	ntrm->_tc = NIL(TERMCAP*);
		ntrm->_delay = 0;
		ntrm->_inpd = ntrm->_ahd = 0;
		ntrm->_ibeg = ntrm->_icur = ntrm->_iend = NIL(uchar*);
#if _MULTIBYTE
		ntrm->_ilit = 0;
#endif
		ntrm->_outd = 0;
		ntrm->_speed = ntrm->_baud = 0;
		ntrm->_attrs = A_NORMAL;
		ntrm->_keys = NIL(KEY_MAP*);
		ntrm->_killc = ntrm->_erasec = 0;
		ntrm->_notab = ntrm->_echo = ntrm->_raw = ntrm->_nonl = FALSE;
		ntrm->_iwait = ntrm->_ieof = FALSE;
		ntrm->_mouse_f = NIL(Mdriver_f);	/* mouse driver */
		ntrm->mouse = NIL(MOUSE*);      	/* mouse data structure */
	}

	/* redirection, yuk! But we must be upward compatible, right? */
	if(outd == 1 && !isatty(1))
		outd = 2;

	/* save terminal infos */
	cur_term = ntrm;
	_outputd = outd;
	if(outd >= 0)
		def_shell_mode();

	/* no input channel initially */
	_ahead = _inputd = -1;
	_timeout = -1;

	/* get termcap info */
	if(!name || !name[0])
		name = getenv("TERM");
	if(name && name[0])
		ntrm->_tc = _settc(name);

	/* try using the termid database */
#ifdef	TERMID
	if(errret)
		*errret = -1;
	if(outd >= 0)
	{
		if(!(ntrm->_tc))
			name = termid(outd,_idinput);
		if(name && name[0])
			ntrm->_tc = _settc(name);
	}
#endif

	if(ntrm->_tc)
	{	/* success */
		cur_tc = cur_term->_tc;
		ttytype = cur_tc->_names;

		if(outd >= 0)
		{	twinsize(outd,&(T_lines),&(T_cols));

			/* lines, cols, tabsiz in the environment */
			if((env = getenv("LINES")) && env[0] != '\0')
				T_lines = atoi(env);
			if((env = getenv("COLUMNS")) && env[0] != '\0')
				T_cols = atoi(env);
			if((env = getenv("TABSIZE")) && env[0] != '\0')
				T_it = atoi(env);
		}

		_inputbeg = _inputcur = &(_inputbuf[0]);
		_inputend = _inputbeg + IBUFSIZE;
#if _MULTIBYTE
		_literal = 0;
#endif
		if(errret)
			*errret = 1;
		_f_unused = FALSE;

		return cur_term;
	}

err:
	if(ntrm)
		delterm(ntrm);
	cur_term = savterm;
	if(errret)
		*errret = 0;
	return NIL(TERMINAL*);
}
