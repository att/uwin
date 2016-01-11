#include	"scrhdr.h"


/*
**	Read a window that was stored by putwin
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
WINDOW	*getwin(FILE* filep)
#else
WINDOW	*getwin(filep)
FILE	*filep;
#endif
{
	short		maxy, maxx;
	reg WINDOW	*win;
	reg chtype	**ecp, **wcp;

	if(!(win = (WINDOW *) malloc(sizeof(WINDOW))) )
		return NIL(WINDOW*);

	if(fread((char*)win,sizeof(WINDOW),1,filep) != 1)
	{
		free((char *)win);
		return NIL(WINDOW*);
	}

#if _MULTIBYTE
	win->_nbyte = -1;
#endif

	maxy = win->_maxy;
	maxx = win->_maxx;
	win->_y = NIL(chtype**);
	win->_firstch = win->_lastch = NIL(short*);

	if(!(win->_y = (chtype **) malloc(maxy*sizeof(chtype *))) )
	{
		free((char *)win);
		return NIL(WINDOW*);
	}

	if(!(win->_firstch = (short *) malloc(2*maxy*sizeof(short))) )
	{
		free((char *) (win->_y));
		free((char *) win);
		return NIL(WINDOW*);
	}
	else	win->_lastch = win->_firstch + maxy;

	if(_image(win) == ERR)
		return NIL(WINDOW*);

	/* read the window image */
	ecp = win->_y + maxy;
	for(wcp = win->_y; wcp < ecp; ++wcp)
		if(fread((char*)(*wcp),sizeof(chtype),maxx,filep) != maxx)
			return NIL(WINDOW*);

	/* the change structure */
	if(fread((char*)(win->_firstch),sizeof(short),maxy,filep) != maxy ||
	   fread((char*)(win->_lastch),sizeof(short),maxx,filep) != maxx)
		return NIL(WINDOW*);

	return win;
}
