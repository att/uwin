#include	"scrhdr.h"


/*
**	Mouse off, on, and initialization functions.
**
**	J. J. Snyder,  AT&T,  09/09/93.
*/


extern	TERMINAL	*cur_term;


/*
**	Initialize mouse entries for  cur_term = curscreen->_term
**
**	  called by mouse_on if terminal has undefined mouse entries
**
*/
static int _mouse_init()
{	reg char    *mn=NIL(char*);
	reg MOUSE   *cm=NIL(MOUSE*);

	/* initialize a MOUSE data structure */
	if( tigetnum("btns") <= 0 || (long)tigetstr("kmous") <= 0L ||
		!(cm = (MOUSE*)malloc(sizeof(MOUSE))) )
		return ERR;

	cm->x = cm->y = 0;
	cm->button[0] = cm->button[1] = cm->button[2] = 0;
	cm->changes = 0;
	cm->_mbapp = cm->_mbslk = cm->_mgrab = 0;
	cur_term->mouse = cm;

	/* and look for the terminal's mouse driver */
	if( setmdriver( (getenv("TERM")) ) == OK )
		return OK;
	for( mn = _nextname(cur_tc->_names);	/* hunt through list of names */
	     mn != NIL(char*);
	     mn = _nextname(NIL(char*)) )
		if( setmdriver(mn) == OK )      	/* found & set mouse driver */
			return OK;

	/* sorry! */
	free((char*)cur_term->mouse);
	return ERR;
}



/*
**	Control which mouse button events are passed to the application
*/


#if __STD_C
int mouse_on(short mbe)		/* pass these mouse button events to app */ 
#else
int mouse_on(mbe)		/* called by application when ready to grab mouse */
short	mbe;
#endif
{	reg 	int 	 rv;

	/* check if mouse data structures have been initialized */
	rv = OK;
	if( curmouse == NIL(MOUSE*) )
		rv = _mouse_init();
	if( rv == ERR )
		return ERR;

	curmouse->_mbapp |= mbe & ALL_MOUSE_EVENTS;

	if((curmouse->_mbapp != 0 || curmouse->_mbslk != 0) && !(curmouse->_mgrab))
	{   _puts( T_getm );
		_tflush();
		_addkey(T_kmous,NIL(char*),KEY_MOUSE,FALSE,FALSE);
		curmouse->_mgrab = TRUE;
	}

	return OK;
}


#if __STD_C
int mouse_set(short mbe)	/* pass only these mouse button events to app */ 
#else
int mouse_set(mbe)
short	mbe;
#endif
{	if( curmouse == NIL(MOUSE*) )
		return ERR;

	curmouse->_mbapp = 0;
	return mouse_on( mbe );
}


#if __STD_C
int mouse_off(short mbe)	/* do not pass these mouse button events to app */ 
#else
int mouse_off(mbe)
short	mbe;
#endif
{	if( curmouse == NIL(MOUSE*) )
		return ERR;

	curmouse->_mbapp &= ~mbe & ALL_MOUSE_EVENTS;

	if( curmouse->_mbapp == 0  &&  curmouse->_mbslk == 0  &&  curmouse->_mgrab )
	{	_puts( T_relm );		/* release the mouse */
		_tflush();
		curmouse->_mgrab = FALSE;
	}

	return OK;
}



/*
**	Control which mouse button events may activate screen labeled keys
*/


#if __STD_C
int map_button(short mbe)	/* map these slk mouse button events to their fun key */
#else
int map_button(mbe)
short	mbe;
#endif
{	reg 	int 	 rv;

	/* check if mouse data structures have been initialized */
	rv = OK;
	if( curmouse == NIL(MOUSE*) )
		rv = _mouse_init();
	if( rv == ERR )
		return ERR;

	curmouse->_mbslk = mbe & ALL_MOUSE_EVENTS;

	if((curmouse->_mbapp != 0 || curmouse->_mbslk != 0) && !(curmouse->_mgrab))
		return mouse_on( curmouse->_mbapp );

	if( curmouse->_mbapp == 0  &&  curmouse->_mbslk == 0  &&  curmouse->_mgrab )
		return mouse_off( NO_MOUSE_EVENTS );

	return OK;
}



/*
**	Mouse driver utility functions.
*/


typedef struct _mouse_driver	MOUSE_DRIVER;

/* mouse driver structure */
struct _mouse_driver
{
	char        	*name;
	Mdriver_f   	fun;	/* addr of mouse driver function */
	MOUSE_DRIVER	*next;
};

extern	mouse_xterm _ARG_((int));

MOUSE_DRIVER 	MD_xterm =
            	{ "xterm", (Mdriver_f)mouse_xterm, NIL(MOUSE_DRIVER*) };

MOUSE_DRIVER 	*MD_Table = &MD_xterm;


/*
**	Dump mouse driver table
*/

#ifdef _SFIO_H
#define 	STDERR	sfstderr
#define 	PRINTF	sfprintf
#else
#define 	STDERR	stderr
#define 	PRINTF	fprintf
#endif /* _SFIO_H */

void dmpmdtable()
{	reg	MOUSE_DRIVER	*md;

	PRINTF( STDERR, "\ndmpmdtable():  cur_term=x%p\n", cur_term );
	PRINTF( STDERR, "  mouse data at cur_term->mouse=x%p = curmouse=x%p\n",
		cur_term->mouse, curmouse );

    for( md=MD_Table; md != NIL(MOUSE_DRIVER*); md=md->next )
	{	
		PRINTF( STDERR, "  %c  MD x%p:  name=%s funloc=x%p next=x%p",
			cur_term->_mouse_f != NIL(Mdriver_f) &&
			cur_term->_mouse_f == md->fun ? '*' : ' ',
			md, md->name ? md->name : "nil", md->fun, md->next );
		if( strcmp("xterm",md->name) == 0 )
			PRINTF( STDERR, "  fun=%s", "mouse_xterm()" );
		PRINTF( STDERR, "\n" );
	}
	if( cur_term->_mouse_f != NIL(Mdriver_f) )
		PRINTF( STDERR,"  *  current mouse driver function set to funloc=x%p\n",
			cur_term->_mouse_f );
}


/*
**	Add a new mouse driver
*/
#if __STD_C
int addmdriver(char* name, Mdriver_f fun )
#else
int addmdriver(name, fun)
char *name;
Mdriver_f fun;
#endif
{	reg	MOUSE_DRIVER	*md;
	    MOUSE_DRIVER	*new_md;

	if( !name || !fun )
		return ERR;
	if( MD_Table->fun == NIL(Mdriver_f) )	/* safety check */
		return ERR;

	if( !(new_md = (MOUSE_DRIVER *) malloc(sizeof(MOUSE_DRIVER))) ||
	    !(new_md->name = (char *) malloc(strlen((char*)name)+1)) )
		return ERR;

	for( md=MD_Table; md != NIL(MOUSE_DRIVER*); md=md->next )
		;
	md->next = new_md;

	strcpy((char*)new_md->name,(char*)name);
	new_md->fun = fun;
	new_md->next = NIL(MOUSE_DRIVER*);

	/* and set current mouse driver to this new function */
	cur_term->_mouse_f = fun;

	return OK;
}


/*
**	Use this mouse driver
*/
#if __STD_C
int setmdriver(char* name)
#else
int setmdriver(name)
char *name;
#endif
{	reg	MOUSE_DRIVER	*md;

	if( name == NIL(char*) )
		return ERR;

	for( md=MD_Table; md != NIL(MOUSE_DRIVER*); md=md->next )
		if( strcmp(name,md->name) == 0)
		{	cur_term->_mouse_f = md->fun;
			return OK;
		}

	return ERR;
}
