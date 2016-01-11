#include	"scrhdr.h"

/*
**	Scanf on the standard screen.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int scanw(char* form, ...)
#else
int scanw(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;
#if __STD_C
	va_start(args,form);
#else
	char	*form;
	va_start(args);
	form = va_arg(args,char*);
#endif
	rv = _sscanw(stdscr,form,args);
	va_end(args);
	return rv;
}

/*
**	Scanf on the given window.
*/
#if __STD_C
int wscanw(WINDOW* win, char* form, ...)
#else
int wscanw(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;
#if __STD_C
	va_start(args,form);
#else
	WINDOW	*win;
	char	*form;
	va_start(args);
	win = va_arg(args,WINDOW*);
	form = va_arg(args,char*);
#endif
	rv = _sscanw(win,form,args);
	va_end(args);
	return rv;
}

#if __STD_C
int _sscanw(WINDOW* win, char* form, va_list args)
#else
int _sscanw(win,form,args)
WINDOW	*win;
char	*form;
va_list	args;
#endif
{
	char	buf[1024];

	if(wgetstr(win,buf) == ERR)
		return ERR;

#ifdef _SFIO_H
	return sfvsscanf(buf,form,args);
#else
	return vsscanf(buf,form,args);
#endif
}
