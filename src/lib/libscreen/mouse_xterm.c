#include    "term.h"

/* Help Mouse Driver set change bits for Mouse Button Events;
**      bits used by  MOUSE_MOVED()  and  BUTTON_CHANGED(x)
**      are given in  term.h
*/
#define MOVED_BY_MOUSE()    	8
#define CHANGED_BY_MOUSE(x) 	(1 << ((x)-1))


/*	Mouse Handlers:
**
**		* de-instantiate the mouse escape sequence sent 
**		  by their mouse when a mouse event has occurred and
**
**		* update the public MOUSE data structure as required.
**
**	This mouse_xterm() is for the MIT X-11 Window System  xterm  mouse
**	     and may serve as a model for others
**
**	The X11  xterm  3-character mouse escape sequence is of the format:
**	         M_event X_loc Y_loc
**	where:
**		M_event:  ' ' means Button 1 Down
**		          '!' means Button 2 Down
**		          '"' means Button 3 Down
**		          '#' means Button Up (for the last one that went down)
**	    X_loc:  encoded X coordinate of mouse (see  mxy_xterm()  below)
**	    Y_loc:  encoded Y coordinate of mouse (see  mxy_xterm()  below)
**
**
**	mouse_xterm( MOUSE* mouse, int(*mgetc)() )
**
**		mouse:    address of the current mouse data structure to be updated
**
**		mgetc():  function to call to get the next character of the
**		          mouse escape sequence to be decoded or interpreted
**
**	J. J. Snyder,  AT&T Bell Labs 11267,  MH 3C-537,  9/93
**	      jjs@research.att.com,  908 582-4140
**
**	J. J. Snyder,  AT&T Labs HA1600000,   FP D-260,  12/99
**	      jjs@research.att.com,  973 360-8586
**	      Revised: relax 0177 WRAP rule for newer xterm windows that may be
**	               more than 94 characters wide or tall (see mxy_xterm(int c);
**	               now using 8 bits instead of 7 to encode x and y locations).
*/


#define 	MOUSE_TRACE 	1		/* for debug trace */
#undef 		MOUSE_TRACE				/* go silently */

#ifdef MOUSE_TRACE
#ifdef _SFIO_H
#include	<sfio.h>
#define 	PRINTF	sfprintf
#define 	STDERR	sfstderr
#else
#include	<stdio.h>
#define 	PRINTF	printf
#define 	STDERR	stderr
#endif /* _SFIO_H */
#endif


#define reg 	register


#define	BASE	 040	/* Blank ' ' char is base for coordinates */
#define	WRAP	0177	/* coordinates wrap after the DEL char */

static
#if __STD_C
int mxy_xterm(int c)    /* de-instantiate an Xterm mouse coordinate */
#else
int mxy_xterm(c)
int c;              /* current character of the mouse escape sequence */
#endif
{	reg 	int	d;

	if( c == ERR )
		return -1;

	/* decode an x or y screen distance encoded in 1 byte by X11 xterm.
	 * newer X11 xterms:  relax 7-bit 0177 WRAP rule on incoming 8-bit byte;
	 *                    let sending xterm window emulator manage any wrap.
	 */
	if( BASE < c )
		d = c - BASE;
	else
		d = 0;
	return d-1; 	/* convert upper-left corner from (1,1) base to (0,0) */
}


#if __STD_C
int mouse_xterm( MOUSE* mouse, int(*mgetc)() )
#else
int mouse_xterm(mouse, mgetc)
MOUSE*   mouse;
int 	(*mgetc)();
#endif
{	reg 	int 	c, i, rv;
	static	MOUSE	last_mouse;

	/* de-instantiate the 3-character X11  xterm  mouse escape sequence:
	**    M_event X_loc Y_loc
	*/

#ifdef MOUSE_TRACE
			PRINTF( STDERR, "mouse_xterm():\n" );
#endif /* MOUSE_TRACE */

	/* first, remember previous mouse status for detecting button changes */
	last_mouse.x = mouse->x;
	last_mouse.y = mouse->y;
	for( i=0; i < 3; i++)
		last_mouse.button[i] = mouse->button[i];
	last_mouse.changes = mouse->changes;

	/* get  M_event  */
	rv = OK;
	switch( (c=(*mgetc)()) )
	{
	case ' ' :
		mouse->button[0] = BUTTON_PRESSED;
		break;
	case '!' :
		mouse->button[1] = BUTTON_PRESSED;
		break;
	case '"' :
		mouse->button[2] = BUTTON_PRESSED;
		break;
	case '#' :
		if( last_mouse.button[0] == BUTTON_PRESSED )
			mouse->button[0] = BUTTON_RELEASED;
		else if( last_mouse.button[1] == BUTTON_PRESSED )
			mouse->button[1] = BUTTON_RELEASED;
		else if( last_mouse.button[2] == BUTTON_PRESSED )
			mouse->button[2] = BUTTON_RELEASED;
		else
		{	mouse->button[0] = BUTTON_RELEASED;
			mouse->button[1] = BUTTON_RELEASED;
			mouse->button[2] = BUTTON_RELEASED;
		}
		break;
	default :

#ifdef MOUSE_TRACE
		PRINTF(STDERR,
			"\nmouse_xterm( MOUSE_DATA ):  bad btn char >>%c<<\n", c );
#endif /* MOUSE_TRACE */

		rv = ERR;
	}

	/* get  X_loc  */
	mouse->x = mxy_xterm( (c=(*mgetc)()) );	/* may be <DEL> 0177 !! */

#ifdef MOUSE_TRACE
		if( MOUSE_X_POS < 0 )
			PRINTF( STDERR,
		         "\nmouse_xterm( MOUSE_DATA ):  bad x char >>%c<<\n", c );
#endif /* MOUSE_TRACE */

	/* get  Y_loc  */
	mouse->y = mxy_xterm( (c=(*mgetc)()) );   /* may be <DEL> 0177 !! */

#ifdef MOUSE_TRACE
		if( MOUSE_Y_POS < 0 )
			PRINTF( STDERR,
		         "\nmouse_xterm( MOUSE_DATA ):  bad x char >>%c<<\n", c );
		PRINTF( STDERR, "\nmouse_xterm( MOUSE_DATA ): y=%d x=%d\n",
		          MOUSE_Y_POS, MOUSE_X_POS );
		PRINTF( STDERR, "             cur_term at 0x%x\n",
				cur_term );
		PRINTF( STDERR, "             cur_term->mouse at 0x%x\n",
				cur_term->mouse );
		PRINTF( STDERR, "             cur_term->mouse->x at 0x%x\n",
				cur_term->mouse->x);
		PRINTF( STDERR, "             cur_term->mouse->y at 0x%x\n",
				cur_term->mouse->y);
#endif /* MOUSE_TRACE */

	/* now set change bits */
	mouse->changes = 0;
	if( rv == OK  &&  mouse->x >= 0  &&  mouse->y >= 0 )
	{
		if( mouse->x != last_mouse.x || mouse->y != last_mouse.y )
		    mouse->changes |= MOVED_BY_MOUSE();
		if( mouse->button[0] != last_mouse.button[0] )
		    mouse->changes |= CHANGED_BY_MOUSE( 1 );
		if( mouse->button[1] != last_mouse.button[1] )
		    mouse->changes |= CHANGED_BY_MOUSE( 2 );
		if( mouse->button[2] != last_mouse.button[2] )
		    mouse->changes |= CHANGED_BY_MOUSE( 3 );
		return KEY_MOUSE;
	}

	return ERR;
}
