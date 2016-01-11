#include	"scrhdr.h"


/*
**	Write a window to a file.
**
**	win:	the window to write out.
**	filep:	the file to write to.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int putwin(WINDOW* win, FILE* filep)
#else
int putwin(win,filep)
WINDOW	*win;
FILE	*filep;
#endif
{
	reg int		maxx, maxy;
	reg chtype	**wcp, **ecp;

	/* write the window skeleton */
	if(fwrite((char*)win,sizeof(WINDOW),1,filep) != 1)
		return ERR;

	/* write the character image */
	maxy = win->_maxy;
	maxx = win->_maxx;
	ecp = win->_y+maxy;
	for(wcp = win->_y; wcp < ecp; ++wcp)
		if(fwrite((char*)(*wcp),sizeof(chtype),maxx,filep) != maxx)
			return ERR;

	/* write the change structure */
	if(fwrite((char*)(win->_firstch),sizeof(short),maxy,filep) != maxy ||
	   fwrite((char*)(win->_lastch), sizeof(short),maxy,filep) != maxy)
		return ERR;

	return OK;
}
