#include	"scrhdr.h"


/*
**	Printf on stdscr and windows
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int printw(char* form, ...)
#else
int printw(va_alist)
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
	rv = vwprintw(stdscr,form,args);
	va_end(args);
	return rv;
}

#if __STD_C
int wprintw(WINDOW* win, char* form, ...)
#else
int wprintw(va_alist)
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
	rv = vwprintw(win,form,args);
	va_end(args);
	return rv;
}

#if __STD_C
int vwprintw(WINDOW* win, const char* form, va_list args)
#else
int vwprintw(win,form,args)
WINDOW	*win;
char	*form;
va_list	args;
#endif
{
	char	buf[1024];

#ifdef _SFIO_H
	sfvsprintf(buf, sizeof(buf), form, args);
#else
	vsprintf(buf,form,args);
#endif

	return waddnstr(win,buf,-1);
}
