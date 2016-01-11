#include	"termhdr.h"



/*
**	Match a names in a given list
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _match(char** list, short* idx, int n_elt, char* id)
#else
int _match(list,idx,n_elt,id)
char	**list;
short	*idx;
int	n_elt;
char	*id;
#endif
{
	reg int		i, l, u, cmp;

	if(!idx)
	{	/* linear search */
		for(i = 0; list[i]; ++i)
			if(strcmp(list[i],id) == 0)
				return i;
	}
	else
	{	/* binary search */
		l = 0, u = n_elt-1;
		while(l <= u)
		{
			i = (l+u)/2;
			if((cmp = strcmp(id,list[i])) == 0)
				return idx[i];
			else if(cmp < 0)
				u = i-1;
			else	l = i+1;
		}
	}

	return -1;
}
