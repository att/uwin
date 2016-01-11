#include	"scrhdr.h"

/*
**	Turn terminal to normal mode.
**
**	Written by Kiem-Phong Vo
*/
int endwin()
{
	int	savahead;

	if(_inputpending)
	{	savahead = _ahead; _ahead = -1;
		doupdate();
		_ahead = savahead;
	}

	if(_insert)
		_offinsert();
	if(curscr)
	{	if(curscr->_attrs != A_NORMAL)
			_vids(A_NORMAL,curscr->_attrs);
		mvcur(curscr->_cury,curscr->_curx,curscr->_maxy-1,0);
	}
	if(_cursstate != 1)
		_puts(T_cnorm);
	if(curscreen->_meta)
		_puts(T_rmm), curscreen->_meta = FALSE;
	if(curscreen->_keypad)
		_puts(T_rmkx), curscreen->_keypad = FALSE;

	/* for mouse  --  but without linking in any mouse functions! */
	if( MOUSE_ON() )	/* leave mouse structure for any restoration */
		_puts( T_relm );	/* now release the mouse */

	/* for color */
	if( COLORS >= 0 )
	{	if( T_oc )
			_puts( T_oc );		/* restore colors */
		if( T_op )
			_puts( T_op );		/* restore original color pair */
	}

	if(T_rmcup)
		_puts(T_rmcup);
	if(_timeout >= 0)
		ttimeout(-1);
	_tflush();

	reset_shell_mode();
	_endwin = TRUE;

	return OK;
}
