#include	"scrhdr.h"
#include	"termhdr.h"

#define	M_ATTRS		(A_REVERSE|A_BLINK|A_DIM|A_BOLD|A_INVIS|A_PROTECT)


/*
**	Update video attributes. Here we overload the meaning of xhp for
**	termcap (or terminfo without sgr) to mean the video control style
**	of the HP-26* terminals. That is, any video sequence performs two
**	actions: (1) turn off all old video, (2) turn on the one sent.
**	In such a case, when multiple video attributes are desired, we
**	favor STANDOUT to UNDERLINE to REVERSE ...
**
**	na :	new attributes
**	oa :	old attributes
**	outc:	function to output a character.
**
**	Written by Kiem-Phong Vo
**	Revised by J. J. Snyder, 12/98, for color
*/


#if __STD_C
int vidupdate(chtype na, chtype oa, int(*outc)(int))
#else
int vidupdate(na,oa,outc)
chtype	na, oa;
int	(*outc)();
#endif
{	reg	short	ncp, ocp;

	/* no_color_video: video attr that cannot be used with color   */
	/*                 (because they are implemented using color)  */
	if(T_ncv > 0)
	{	na &= ~ATTR_NCV(T_ncv);
		oa &= ~ATTR_NCV(T_ncv);
	}

	/* nothing to do */
	if((na &= A_ATTRIBUTES) == (oa &= A_ATTRIBUTES))
		return OK;

	/* new set of attributes */
	_curattrs = na;
	
	/* get color pairs from attributes */
	ncp = PAIR_NUMBER( na );
	ocp = PAIR_NUMBER( oa );

	/* get non-color attributes */
	na &= ~A_COLOR;
	oa &= ~A_COLOR;

	/* catch-all case */
	if(T_sgr && T_xmc < 0 && !istermcap())
	{	tputs(tparm(T_sgr,
			    (char*)(na&A_STANDOUT),
			    (char*)(na&A_UNDERLINE),
			    (char*)(na&A_REVERSE),
			    (char*)(na&A_BLINK),
			    (char*)(na&A_DIM),
			    (char*)(na&A_BOLD),
			    (char*)(na&A_INVIS),
			    (char*)(na&A_PROTECT),
			    (char*)(na&A_ALTCHARSET)),
			1, outc);
		if( COLORS >= 0 && ncp != ocp )
			set_color( ncp, outc );
		return OK;
	}

	if(T_xmc >= 0 && (na&~oa))
	{	na &= ~oa;
		goto turnon;
	}

	/* turn off what is to be turned off first */
	if(T_rmso && (oa&A_STANDOUT) && !(na&A_STANDOUT))
	{
		tputs(T_rmso,1,outc);
		oa &= ~A_STANDOUT;
		if(T_seue)
			oa &= ~A_UNDERLINE;
		if(T_seme)
			oa &= ~M_ATTRS;
		if(T_seae)
			oa &= ~A_ALTCHARSET;
		if(T_opse && COLORS >=0 && ncp != 0 )
			ocp = 0;
		if(T_xmc >= 0)
			return OK;
	}

	if(T_rmul && (oa&A_UNDERLINE) && !(na&A_UNDERLINE))
	{
		tputs(T_rmul,1,outc);
		oa &= ~A_UNDERLINE;
		if(T_seue)
			oa &= ~A_STANDOUT;
		if(T_ueme)
			oa &= ~M_ATTRS;
		if(T_ueae)
			oa &= ~A_ALTCHARSET;
		if(T_opue && COLORS >=0 && ncp != 0 )
			ocp = 0;
		if(T_xmc >= 0)
			return OK;
	}

	if(T_sgr0 && (oa&M_ATTRS) && (na&M_ATTRS) != (oa&M_ATTRS))
	{
		tputs(T_sgr0,1,outc);
		oa &= ~M_ATTRS;
		if(T_seme)
			oa &= ~A_STANDOUT;
		if(T_ueme)
			oa &= ~A_UNDERLINE;
		if(T_meae)
			oa &= ~A_ALTCHARSET;
		if(T_opme && COLORS >=0 && ncp != 0 )
			ocp = 0;
		if(T_xmc >= 0)
			return OK;
	}

	if(T_rmacs && (oa&A_ALTCHARSET) && !(na&A_ALTCHARSET))
	{
		tputs(T_rmacs,1,outc);
		oa &= ~A_ALTCHARSET;
		if(T_seae)
			oa &= ~A_STANDOUT;
		if(T_ueae)
			oa &= ~A_UNDERLINE;
		if(T_meae)
			oa &= ~M_ATTRS;
		if(T_opae && COLORS >=0 && ncp != 0 )
			ocp = 0;
		if(T_xmc >= 0)
			return OK;
	}

	if( COLORS >= 0 && T_op && ncp != ocp && ncp == 0 )
	{	
		set_color( ncp, outc );
		if(T_opae)
			oa &= ~A_ALTCHARSET;
		if(T_opse)
			oa &= ~A_STANDOUT;
		if(T_opue)
			oa &= ~A_UNDERLINE;
		if(T_opme)
			oa &= ~M_ATTRS;
		if(T_xmc >= 0)
			return OK;
	}

turnon: /* turn on necessary attributes */
	if(T_smso && (na&A_STANDOUT) && !(oa&A_STANDOUT))
	{	na &= ~A_STANDOUT;
		tputs(T_smso,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_smul && (na&A_UNDERLINE) && !(oa&A_UNDERLINE))
	{	na &= ~A_UNDERLINE;
		tputs(T_smul,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_rev && (na&A_REVERSE) && !(oa&A_REVERSE))
	{	na &= ~A_REVERSE;
		tputs(T_rev,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_blink && (na&A_BLINK) && !(oa&A_BLINK))
	{	na &= ~A_BLINK;
		tputs(T_blink,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_dim && (na&A_DIM) && !(oa&A_DIM))
	{	na &= ~A_DIM;
		tputs(T_dim,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_bold && (na&A_BOLD) && !(oa&A_BOLD))
	{	na &= ~A_BOLD;
		tputs(T_bold,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_invis && (na&A_INVIS) && !(oa&A_INVIS))
	{	na &= ~A_INVIS;
		tputs(T_invis,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_prot && (na&A_PROTECT) && !(oa&A_PROTECT))
	{	na &= ~A_PROTECT;
		tputs(T_prot,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if(T_smacs && (na&A_ALTCHARSET) && !(oa&A_ALTCHARSET))
	{	na &= ~A_ALTCHARSET;
		tputs(T_smacs,1,outc);
		if(T_xmc >= 0)
			return OK;
	}
	if( COLORS >= 0 && ncp != ocp && ncp > 0 )
		set_color( ncp, outc );

	return OK;
}
