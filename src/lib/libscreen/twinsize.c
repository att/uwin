#include	"termhdr.h"

/* Get window sizes on windowing systems */
#if __STD_C
int twinsize(int fd, short* li, short* co)
#else
int twinsize(fd,li,co)
int	fd;
short*	li;
short*	co;
#endif
{
#if _typ_ttysize	/* SUN */
	{	struct ttysize w;
		if(ioctl(fd,TIOCGSIZE,&w) >= 0 && w.ts_lines > 0 && w.ts_cols > 0)
		{
			*li = w.ts_lines;
			*co = w.ts_cols;
			return OK;
		}
	}
#endif

#if _typ_winsize	/* 4.3bsd */
	{	struct winsize	w;
		if(ioctl(fd,TIOCGWINSZ,&w) >= 0 && w.ws_row > 0 && w.ws_col > 0)
		{
			*li = w.ws_row;
			*co = w.ws_col;
			return OK;
		}
	}
#endif
	return ERR;
}
