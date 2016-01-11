#include	"scrhdr.h"


/*
**	Functions to put out one byte or one character.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _putbyte(int c)
#else
int _putbyte(c)
int c;
#endif
{
	return _putc(c) < 0 ? ERR : OK;
}

#if __STD_C
int _putchar(chtype c)
#else
int _putchar(c)
chtype	c;
#endif
{
	reg chtype	o;
	reg int		rv;

	o = c&CMASK;
#if _MULTIBYTE
	/* ASCII code */
	if(!ISMBYTE(c))
		rv = (int)_putc(o);
	/* international code */
	else if(o&0177)
	{	if((rv = (int)_putc(o)) >= 0 && _csmax > 1 && ((o = LBYTE(c))&0177) )
			rv = (int)_putc(o);
	}
#else
	rv = _putc(o);
#endif
	return rv < 0 ? ERR : OK;
}
