#include	"scrhdr.h"

/*
**	Move to (y,x) then do printw
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int mvprintw(int y, int x, char* form, ...)
#else
int mvprintw(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;
#if __STD_C
	va_start(args,form);
#else
	int	x, y;
	char	*form;
	va_start(args);
	y = va_arg(args,int);
	x = va_arg(args,int);
	form = va_arg(args,char*);
#endif

	if((rv = move(y,x)) == OK)
		rv = vwprintw(stdscr,form,args);

	va_end(args);
	return rv;
}


#if __STD_C
int mvwprintw(WINDOW* win, int y, int x, char* form, ...)
#else
int mvwprintw(va_alist)
va_dcl
#endif
{
	va_list	args;
	int	rv;
#if __STD_C
	va_start(args,form);
#else
	WINDOW	*win;
	int	y, x;
	char	*form;
	va_start(args);
	win = va_arg(args,WINDOW*);
	y = va_arg(args,int);
	x = va_arg(args,int);
	form = va_arg(args,char*);
#endif

	if((rv = wmove(win,y,x)) == OK)
		rv = vwprintw(win,form,args);

	va_end(args);

	return rv;
}
