#include	"termhdr.h"

#if _MULTIBYTE


/*
**	Translate process code to byte-equivalent
**	Return the length of the byte-equivalent string
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int _code2byte(wchar_t code, char* bytes)
#else
int _code2byte(code,bytes)
wchar_t	code;
char	*bytes;
#endif
{
	reg int	left, right;

	left = LBYTE(code);
	right = RBYTE(code);

	if(left == 0)
	{
		if(!ISMBIT(right))
		{	/* ASCII */
			bytes[0] = right;
			return 1;
		}
		else
		{	/* SS2 1xxxxxxx */
			bytes[0] = SS2;
			bytes[1] = right;
			return 2;
		}
	}
	else if(left == MBIT)
	{
		if(!ISMBIT(right))
		{	/* SS3 1xxxxxxx */
			bytes[0] = SS3;
			bytes[1] = right|MBIT;
			return 2;
		}
		else
		{	/* 1xxxxxxxx */
			bytes[0] = right|MBIT;
			return 1;
		}
	}
	else if(ISMBIT(left))
	{
		if(!ISMBIT(right))
		{	/* SS3 1xxxxxx 1xxxxxxx */
			bytes[0] = SS3;
			bytes[1] = left;
			bytes[2] = right|MBIT;
			return 3;
		}
		else
		{	/* 1xxxxxxx 1xxxxxxx */
			bytes[0] = left;
			bytes[1] = right;
			return 2;
		}
	}
	else
	{	/* SS2 1xxxxxxx 1xxxxxxx */
		bytes[0] = SS2;
		bytes[1] = left|MBIT;
		bytes[2] = right|MBIT;
		return 3;
	}
}


/*
**	Translate a set of byte to a single process code
*/
#if __STD_C
wchar_t _byte2code(char* argbytes)
#else
wchar_t _byte2code(argbytes)
char	*argbytes;
#endif
{
	wchar_t	code;
	int	width;
	uchar	*bytes = (uchar*)argbytes;

	/* ASCII */
	if(!ISMBIT(bytes[0]))
		return RBYTE(bytes[0]);

	code = 0;
	width = cswidth[TYPE(bytes[0])];
	if(bytes[0] == SS2)
	{
		if(width == 1)
			code = RBYTE(bytes[1]);
		else /* if(width == 2) */
			code = ((bytes[1] & ~MBIT) << 8) | (bytes[2] | MBIT);
	}
	else if(bytes[0] == SS3)
	{
		if(width == 1)
			code = (MBIT << 8) | (bytes[1] & ~MBIT);
		else /* if(width == 2) */
			code = ((bytes[1] | MBIT) << 8) | (bytes[2] & ~MBIT);
	}
	else
	{
		if(width == 1)
			code = (MBIT << 8) | (bytes[0] | MBIT);
		else /* if(width == 2) */
			code = ((bytes[0] | MBIT) << 8) | (bytes[1] | MBIT);
	}
	return code;
}



/*
**	Translate a string of wchar_t to a byte string.
**	code: the input code string
**	byte: if not NULL, space to store the output string
**	n: maximum number of codes to be translated.
*/
#if __STD_C
char *_strcode2byte(wchar_t* code, char* argbyte, int n)
#else
char *_strcode2byte(code,argbyte,n)
wchar_t	*code;
char	*argbyte;
int	n;
#endif
{
	reg uchar	*bufp;
	reg wchar_t	*endcode;
	reg uchar	*byte = (uchar*)argbyte;
	static uchar	*buf;
	static int	bufsize;

	/* compute the length of the code string */
	if(n < 0)
		for(n = 0; code[n] != 0; ++n)
			;

	/* get space to store the translated string */
	if(!byte && (n*CSMAX+1) > bufsize)
	{
		if(buf)
			free((char*)buf);
		bufsize = n*CSMAX+1;
		if(!(buf = (uchar*)malloc(bufsize*sizeof(unsigned char))) )
			bufsize = 0;
	}

	/* no space to do it */
	if(!byte && !buf)
		return NIL(char*);

	/* start the translation */
	bufp = byte ? byte : buf;
	endcode = code+n;
	while(code < endcode && *code)
	{
		bufp += _code2byte(*code,(char*)bufp);
		++code;
	}
	*bufp = '\0';

	return (char*)(byte ? byte : buf);
}



/*
**	Translate a byte-string to a wchar_t string.
*/
#if __STD_C
wchar_t	*_strbyte2code(char* argbyte, wchar_t* code, int n)
#else
wchar_t	*_strbyte2code(argbyte,code,n)
char	*argbyte;
wchar_t		*code;
int		n;
#endif
{
	reg uchar	*endbyte;
	reg wchar_t	*bufp;
	reg uchar	*byte = (uchar*)argbyte;
	static wchar_t	*buf;
	static int	bufsize;

	if(n < 0)
		for(n = 0; byte[n] != '\0'; ++n)
			;

	if(!code && (n+1) > bufsize)
	{
		if(buf)
			free((char *)buf);
		bufsize = n+1;
		if(!(buf = (wchar_t *)malloc(bufsize*sizeof(wchar_t))) )
			bufsize = 0;
	}

	if(!code && !buf)
		return NIL(wchar_t*);

	bufp = code ? code : buf;
	endbyte = byte+n;

	while(byte < endbyte && *byte)
	{
		reg int type, width;

		type = TYPE(*byte);
		width = cswidth[type];
		if(type == 1 || type == 2)
			width++;

		if(byte + width <= endbyte)
			*bufp++ = _byte2code((char*)byte);

		byte += width;
	}
	*bufp = 0;

	return code ? code : buf;
}

#else
void _mbunused(){}
#endif
