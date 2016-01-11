#include	"termhdr.h"


/*
**	Put a character back to the input queue.
**
**	c :	the character to push back
**	ishead:	if TRUE, the character is inserted at the queue head.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _ungetch(int c, int ishead)
#else
int _ungetch(c,ishead)
reg int	c;
reg int	ishead;
#endif
{
	reg uchar	*cp;
#if _MULTIBYTE
	reg int		r;

	if(ISCBIT(c))
	{
		r = RBYTE(c);
		c = LBYTE(c);
		if(ishead)
		{	/* do the right half first to maintain the byte order */
			if((r&0177) && _ungetch(r,TRUE) == ERR)
				return ERR;
		}
		else
		{
			if((c&0177) && _ungetch(c,FALSE) == ERR)
				return ERR;
			c = r;
		}
	}
#endif

	if(_inputd < 0 || (_inputbeg == _inputbuf && _inputcur == _inputend))
		return ERR;

	if(ishead)
	{
		if(_inputbeg > _inputbuf)
		{
			_inputbeg -= 1;
			*_inputbeg = c;
		}
		else
		{
			for(cp = _inputcur; cp > _inputbeg; --cp)
				*cp = *(cp-1);
			_inputcur += 1;
			*_inputbeg = c;
		}
	}
	else
	{
		if(_inputcur < _inputend)
		{
			*_inputcur = c;
			_inputcur += 1;
		}
		else
		{
			for(cp = _inputbeg; cp < _inputcur; ++cp)
				*(cp-1) = *cp;
			_inputbeg -= 1;
			*(_inputcur-1) = c;
		}
	}

	return OK;
}
