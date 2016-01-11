#include	"termhdr.h"

/*
**	Decode a termcap entry
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
void tcdecode(char* tc, char** area)
#else
void tcdecode(tc,area)
char	*tc, **area;
#endif
{
	reg int		i;
	reg char	c, *cp, *ar, *special;

	/* special char translation */
	special = "E\033^^\\\\::n\nr\rt\tb\bf\f";

	/* storage area */
	ar = *area;

	while(*tc != '\0' && *tc != ':')
	{
		c = *tc++;
		switch(c)
		{
		case '^' :
			c = *tc++ & 037;
			break;

		case '\\' :
			/* special character */
			c = *tc++;
			for(cp = special; *cp != '\0'; cp += 2)
				if(*cp == c)
				{
					c = cp[1];
					goto next;
				}

			/* digital representation */
			if(isdigit(c))
			{
				c -= '0';
				for(i = 2; isdigit(*tc) && i > 0; --i)
				{
					c <<= 3;
					c |= *tc++ - '0';
				}
			}
			break;
		}
	next :
		*ar++ = c == '\0' ? 0200 : c;
	}

	*ar++ = '\0';
	*area = ar;
}
