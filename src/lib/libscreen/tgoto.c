#include	"termhdr.h"


/*
**	Perform cursor addressing.
**
**	The following escapes are defined for substituting row/column:
**	%d	as in printf
**	%2	like %2d
**	%3	like %3d
**	%.	gives %c hacking special case characters
**	%+x	like %c but adding x first
**
**	The codes below affect the state but don't use up a value.
**
**	%>xy	if value > x add y
**	%r	reverses row/column
**	%i	increments row/column (for one origin indexing)
**	%s	add a string
**	%%	gives %
**
**	all other characters are ``self-inserting''.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
char *tgoto(char* input, char* arg1, char* arg2)
#else
char *tgoto(input, arg1, arg2)
char	*input, *arg1, *arg2;
#endif
{
	reg int		c, oncol, val, first, second, asis, isfirst;
	reg char	*dp, *cp;
	char		added[16];
	static char	result[48];

	if(!input )
		return NIL(char*);

	if(!istermcap())
		return tparm(input,arg2,arg1);

	/* if no %. translation allowed */
	asis = (input != T_cup && input != T_mrcup &&
		input != T_vpa && input != T_hpa &&
		input != T_cuf && input != T_cub &&
		input != T_cuu && input != T_cud);

	/* initialize outputs */
	added[0] = '\0';
	dp = result;

	/* set to know which coord we're working on */
	oncol = (input == T_hpa || input == T_cuf || input == T_cub);
	isfirst = (input != T_cup && input != T_mrcup); 
	first = (int)arg1;
	second = (int)arg2;
	val = isfirst ? first : second;

	while((c = *input++))
	{
		/* regular character */
		if(c != '%')
		{
			*dp++ = c;
			continue;
		}

		/* % escapes */
		switch(c = *input++)
		{
			/* reverse order of lines and cols */
		case 'r':
reverse:
			oncol = !oncol;
			isfirst = !isfirst;
			val = isfirst ? first : second;
			continue;

			/* literal % */
		case '%':
			*dp++ = c;
			continue;

			/* conditionally addition */
		case '>':
			if(val > *input++)
				val += *input++;
			else	input++;
			continue;

		/* 1 indexing terminals */
		case 'i':
			first++; second++; val++;
			continue;

			/* printf notation */
		case 'd':
			/* one digit */
			if(val < 10)
				goto one;

			/* two digits */
			if(val < 100)
				goto two;

			/* fall into... */
		case '3':
			/* the highest digit */
			*dp++ = (val / 100) + '0';
			val %= 100;

			/* fall into... */
		case '2':
			/* the second highest digit */
two:
			*dp++ = (val / 10) + '0';

			/* the last digit */
one:
			*dp++ = (val % 10) + '0';
			goto reverse;

			/* print by adding to a char */
		case '+':
			val += *input++;

			/* fall into... */
		case '.':
			/* take care of special characters */
			if(!asis && ((oncol && T_cub1) || (!oncol && T_cuu1)))
				while(val == '\0' || val == '\004' ||
				      val == '\t' || val == '\n' ||
				      val == '\r')
				{
					strcat(added,oncol?T_cub1:T_cuu1);
					val++;
				}
			*dp++ = val;
			goto reverse;

			/* string */
		case 's':
			cp = isfirst ? arg1 : arg2;
			while(*cp)
				*dp++ = *cp++;
			goto reverse;

			/* error */
		default:
			return NIL(char*);
		}
	}

	/* return results */
	strcpy(dp,added);
	return result;
}
