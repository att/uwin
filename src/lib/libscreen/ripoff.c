#include	"scrhdr.h"


/*
**	Remove lines from the screen.
**	ripoffline() should be called before newscreen or initscr.
**
**	line:	>0 for removing a line from the top
**		<=0 for removing a line from the bottom.
**	initf:	a function called as initf(win,COLS) to
**		initialize the ripped-off line.
**
**	Written by Kiem-Phong Vo
*/


#define RIPMAX	5
static struct	_rip_st
{
	int	_line;
	int	(*_initf) _ARG_((WINDOW*,int));
} _rip[RIPMAX];

static int	_ripcount;

static int _rip_init()
{
	reg int		i;
	reg WINDOW	*win;

	/* ready for next time */
	_do_rip_init = NIL(int(*)());

	if(_ripcount > 0)
	{
		for(i = 0; i < _ripcount; ++i)
		{
			--LINES;
			win = newwin(1,COLS,_rip[i]._line > 0 ? 0 : LINES,0);
			if(!win)
				break;

			(*(_rip[i]._initf))(win,COLS);

			if(_rip[i]._line > 0)
				_curoffset += 1;
		}
		_ripcount = 0;
	}
	return OK;
}


#if __STD_C
int ripoffline(int line, int(*initf)(WINDOW*,int))
#else
int ripoffline(line,initf)
int	line, (*initf)();
#endif
{
	if(_ripcount < RIPMAX)
	{
		_rip[_ripcount]._line = line;
		_rip[_ripcount]._initf = initf;
		++_ripcount;
	}

	/* tell newscreen */
	_do_rip_init = _rip_init;
	return OK;
}
