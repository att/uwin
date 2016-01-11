#include	"termhdr.h"


#ifndef _NFILE
#define _NFILE	20		/* max number of file descriptors allowed */
#endif
static TERMCAP	*tctab[_NFILE];	/* table of different termcaps in use */
static int	tcnum;

/* approximation in ASCII for some of the line drawing characters */
static unsigned char acs_approx[][2] =
{
	{ '`', '*' },	/* ACS_DIAMOND */
	{ 'a', ':' },	/* ACS_CKBOARD */
	{ 'f', '\'' },	/* ACS_DEGREE */ 
	{ 'g', '#' },	/* ACS_PLMINUS */
	{ 'o', '-' },	/* ACS_S1 */
	{ 'q', '-' },	/* ACS_HLINE */
	{ 's', '_' },	/* ACS_S9 */
	{ 'x', '|' },	/* ACS_VLINE */
	{ '~', 'o' },	/* ACS_BULLET */
	{ ',', '<' },	/* ACS_LARROW */
	{ '+', '>' },	/* ACS_RARROW */
	{ '.', 'v' },	/* ACS_DARROW */
	{ '-', '^' },	/* ACS_UARROW */
	{ 'h', '#' },	/* ACS_BOARD */
	{ 'i', '#' },	/* ACS_LANTERN */
	{ '0', '#' },	/* ACS_BLOCK */
};



/*
**	See if the termcap of a terminal is already here
*/
#if __STD_C
static TERMCAP *tc_find(char* term)
#else
static TERMCAP *tc_find(term)
char	*term;
#endif
{
	reg int		i;

	for(i = 0; i < tcnum; ++i)
		if(_matchname(term,tctab[i]->_names))
		{
			tctab[i]->_use += 1;
			return tctab[i];
		}

	return NIL(TERMCAP*);
}



/*
**	Get the actual termcap of a terminal
*/
#if __STD_C
static TERMCAP *tc_get(char* term)
#else
static TERMCAP *tc_get(term)
char	*term;
#endif
{
	reg int		i;
	TERMCAP		*tc, *savtc;
	reg chtype	*acsmap, toget, toput;

	/* save current termcap pointer */
	savtc = cur_tc;

	/* make space for the new entry */
	if(_f_unused)
		tc = &_f_tc;
	else if(!(tc = (TERMCAP*) malloc(sizeof(TERMCAP))) )
		return NIL(TERMCAP*);

	/* for the sake of T_ macros */
	cur_tc = tc;

	/* read in termcap or terminfo */
	if(_tiread(tc,term) == OK)
		tc->_istc = FALSE;
	else if(_tcread(tc,term) == OK)
		tc->_istc = TRUE;
	else
	{	if(tc != &_f_tc)
			free(tc);
		else	tc->_use = 0;
		cur_tc = savtc;
		return NIL(TERMCAP*);
	}

	/* possible different representations */
#ifdef	NOCTRL
	T_xt = T_xenl = TRUE;
	if(T_cub1 && T_cub1[0] == '\b')
		T_cub1 = NIL(char*);
	if(T_cud1 && T_cud1[0] == '\n')
		T_cud1 = NIL(char*);
#else
	T_cub1 = T_cub1 ? T_cub1 : (T_cup || T_hpa) ? 0 : "\b";
	T_ht = T_ht ? T_ht : "\t";
	T_cr = T_cr ? T_cr : "\r";
#endif	/* NOCTRL */

	/* if tabs, newlines, carriage returns can't be used */
	if(T_xt)
		T_ht = T_cbt = NIL(char*);
	if(T_xenl)
	{
		T_cr = 0;
		if(T_cud1 && T_cud1[0] == '\n')
			T_cud1 = NIL(char*);
	}

	/* set pad character */
	T_pad = T_npc ? NIL(char*) : T_pad ? T_pad : "";

	/* set a default standout mode */
	T_smso = T_smso ? T_smso : T_smul ? T_smul :
		 T_rev ? T_rev : T_bold ? T_bold :
		 T_blink ? T_blink : T_dim ? T_dim : NIL(char*);
	T_rmso = T_smso ? T_rmso : T_smul ? T_rmul :
		 T_sgr0 ? T_sgr0 : NIL(char*);

	/* hp terminals use magic cookie of size 0 */
	if(T_xhp && T_xmc < 0)
		T_xmc = 0;

	/* compute vector of available video attributes */
	if(!istermcap() && T_sgr)
		tc->_atvec = A_ATTRIBUTES;
	else
	{
		tc->_atvec = 0;
		if(T_smso)	tc->_atvec |= A_STANDOUT;
		if(T_smul)	tc->_atvec |= A_UNDERLINE;
		if(T_rev)	tc->_atvec |= A_REVERSE;
		if(T_blink)	tc->_atvec |= A_BLINK;
		if(T_dim)	tc->_atvec |= A_DIM;
		if(T_bold)	tc->_atvec |= A_BOLD;
		if(T_invis)	tc->_atvec |= A_INVIS;
		if(T_prot)	tc->_atvec |= A_PROTECT;
		if(T_smacs)	tc->_atvec |= A_ALTCHARSET;
	}

	/* set comparisons of video ending strings */
	T_seue = (T_rmso && T_rmul  && strcmp(T_rmso,T_rmul) == 0);
	T_seme = (T_rmso && T_sgr0  && strcmp(T_rmso,T_sgr0) == 0);
	T_seae = (T_rmso && T_rmacs && strcmp(T_rmso,T_rmacs) == 0);
	T_ueme = (T_rmul && T_sgr0  && strcmp(T_rmul,T_sgr0) == 0);
	T_ueae = (T_rmul && T_rmacs && strcmp(T_rmul,T_rmacs) == 0);
	T_meae = (T_sgr0 && T_rmacs && strcmp(T_sgr0,T_rmacs) == 0);
	T_opae = (T_op   && T_rmacs && strcmp(T_op,T_rmacs) == 0);
	T_opme = (T_op   && T_sgr0  && strcmp(T_op,T_sgr0) == 0);
	T_opse = (T_op   && T_rmso  && strcmp(T_op,T_rmso) == 0);
	T_opue = (T_op   && T_rmul  && strcmp(T_op,T_rmul) == 0);

	/* + is the default for line drawing characters */
	acsmap = tc->_acs;
	for(i = 0; i < 128; ++i)
		acsmap[i] = '+';

	/* approximation */
	for(i = sizeof(acs_approx)/2 - 1; i >= 0; --i)
		acsmap[acs_approx[i][0]] = acs_approx[i][1];

	/* Now do mapping for terminals own ACS, if any */
	if(T_acsc)
	{
		reg unsigned char	*cp;
		for(cp = (unsigned char*) T_acsc; *cp != '\0'; cp += 2)
		{
			toget = *cp;
			toput = *(cp+1);
			acsmap[toget] = toput | A_ALTCHARSET;
		}
	}

	/* set name */
	tc->_term = (char *) _matchname(term,tc->_names);

	cur_tc = savtc;
	tc->_use = 1;
	tctab[tcnum++] = tc;
	return tc;
}


/*
**	Get termcap entry.
**	term: terminal name
*/
#if __STD_C
TERMCAP	*_settc(char* term)
#else
TERMCAP	*_settc(term)
char	*term;
#endif
{
	TERMCAP*	tc;

	if(!(tc = tc_find(term)) )
		tc = tc_get(term);

	return tc;
}



/*
**	Delete termcap entry
*/
#if __STD_C
void _deltc(TERMCAP* tc)
#else
void _deltc(tc)
TERMCAP	*tc;
#endif
{
	reg int		i;

	if((tc->_use -= 1) <= 0)
	{
		/* if not first termcap */
		if(tc != &_f_tc)
		{
			free((char *)(tc->_area));
			free((char *) tc);
		}

		/* delete from table */
		--tcnum;
		for(i = 0 ; i < tcnum; ++i)
			if(tc == tctab[i])
			{
				tctab[i] = tctab[tcnum];
				break;
			}
	}
}
