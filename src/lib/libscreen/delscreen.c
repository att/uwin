#include	"scrhdr.h"

/*
**	Delete a screen entry
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int delscreen(SCREEN* scr)
#else
int delscreen(scr)
SCREEN	*scr;
#endif
{
	reg SCREEN	*savscr;
	reg int		i;
	reg uchar	**mks;

	/* restore terminal to original mode */
	savscr = curscreen;
	curscreen = scr;
	if(!_endwin)
		endwin();
	curscreen = savscr;

	/* if curscr exists, it has the right number of lines */
	if(scr->_curscr)
		scr->_lines = scr->_curscr->_maxy;

	/* release space used */
#ifndef _SFIO_H
	if(scr->_buf)
		free(scr->_buf);
#endif
	if(scr->_vrtscr)
		delwin(scr->_vrtscr);
	if(scr->_stdscr)
		delwin(scr->_stdscr);
	if(scr->_curscr)
		delwin(scr->_curscr);
	if(scr->_vhash)
		free(scr->_vhash);
	if(scr->_mks)
	{	mks = scr->_mks;
		for(i = scr->_lines-1; i >= 0; --i, ++mks)
			if(*mks)
				free(*mks);
		free(scr->_mks);
	}
	if(scr->_term)
		delterm(scr->_term);
	if(scr->_slk)
		free(scr->_slk);
	free(scr);

	return OK;
}
