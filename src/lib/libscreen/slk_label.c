#include	"scrhdr.h"


/*
**	Return the current label of key number 'n'.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
char *slk_label(int n)
#else
char *slk_label(n)
int	n;
#endif
{
	reg SLK_MAP	*slk;

	slk = _curslk;
	return (!slk || n < 1 || n > slk->_num) ? NIL(char*) : slk->_lval[n-1];
}
