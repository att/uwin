#include	"scrhdr.h"


/*
**	Set a soft label.
**
**	n:	label number
**	lab:	the string
**	f:	0,1,2 for left,center,right-justification
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int slk_set(int n, char* lab, int f)
#else
int slk_set(n,lab,f)
int	n;
char	*lab;
int	f;
#endif
{
	reg SLK_MAP	*slk;
	reg int		len, h, k, left;
	char		*cp, *ep, *sval, *val, *dis;

	if(!(slk = _curslk) || f < 0 || f > 2 || n < 1 || n > slk->_num)
		return ERR;

	if(!lab )
		lab = "";

	/* construct the label string */
	dis = slk->_ldis[n-1];
	val = slk->_lval[n-1];
	for(h = 0; h < T_lh; ++h)
	{	/* strip starting blanks */
		while(*lab == ' ')
			++lab;

		/* and trailing blanks */
		len = 0;
		sval = val;
		for(cp = lab, ep = lab+strlen(lab); cp < ep; )
		{
#if _MULTIBYTE
			reg int	m, ty;
			if(_mbtrue && ISMBIT(*cp))
			{	m = RBYTE(*cp);
				ty = TYPE(m);
				m = cswidth[ty] - (ty == 0 ? 1 : 0);
				if((cp+m) >= ep || (len+scrwidth[ty]) > T_lw)
					break;
				len += scrwidth[ty];
				while(m-- >= 0)
					*val++ = *cp++;
			}
			else
#endif
			{	if(*cp == '\n')
					break;
				len += 1;
				*val++ = iscntrl(*cp) ? '_' : *cp;
				cp += 1;
			}
		}
		for(ep = cp; ep > lab; --ep, --len)
			if(*(ep-1) != ' ')
				break;

		/* justification */
		left = f == 0 ? 0 : (f == 1 ? (T_lw-len)/2 : (T_lw-len));

		/* construct the display string */
		for(k = 0; k < left; ++k)
			*dis++ = ' ';
		for(; sval < val; ++k)
			*dis++ = *sval++;
		for(; k < T_lw; ++k)
			*dis++ = ' ';

		if(*cp == '\n')
		{	*val++ = '\n';
			lab = cp+1;
		}
		else
		{	*val = '\0';
			lab = cp;
		}
	}

	*dis = *val = '\0';
	slk->_changed = slk->_lch[n-1] = TRUE;
	return OK;
}
