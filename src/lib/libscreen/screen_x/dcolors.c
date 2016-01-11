#include	<stdio.h>
#include	<curses.h>

/* dcolors:  Show default curses colors, if available
**
**      Works with X11R6 xterm and/or color_xterm;
**      set TERM=xterm or TERM=xterm-cd8 (colors with 8 defaults).
**
**      J.J. Snyder, jjs@research.att.com, 1/1999
*/

#define DEBUG		# debug info to stderr
#undef  DEBUG
#define DEBUG_X		# debug eXtra info to stderr
#undef  DEBUG_X

#ifdef _SFIO_H
#define         STDERR  sfstderr
#define         PRINTF  sfprintf
#else
#define         STDERR  stderr
#define         PRINTF  fprintf
#endif  /* _SFIO_H */

#define EXIT_OK 	 0
#define EXIT_FALSE	 1
#define EXIT_HELP	 2
#define EXIT_COLOR	 3
#define EXIT_NOCOLOR 4
#define EXIT_ERROR	 5

#define SPACE	' '
#define MCLINE	128

#define NDCOLORS	8		/* usual number of default colors */

static char   *Has_Color  = "Default Terminal Colors:";
static char   *Happy      = ":-)";
static char   *No_Colors  = "Sorry, Curses Found No Terminal Colors.";
static char   *Def_Colors = " K  R  G  Y  B  M  C  W ";

static WINDOW	*Awin = NIL(WINDOW*);
static int		 Curx=20, Cury=2;

#ifndef _UWIN
#if __STD_C
extern	int	exit(int);
#else
extern	int	exit();
#endif
#endif

#if __STD_C
static int a_msg( int y, int x, chtype a, char *msg )
#else
static int a_msg( y, x, a, msg )
int 	x, y;
chtype	a;
char	*msg;
#endif
{	char	*m;
	int 	 e;

	/* output message with attribute a */
	for( m = msg; *m != '\0'; m++ )
		e = mvwaddch( Awin, y, x++, (((chtype)(*m)) | a) );

	return e;
}


/* create a colorful line */
#if __STD_C
static int c_msg( int y, int x, short b, chtype a, char *msg )
#else
static int c_msg( y, x, b, a, msg )
int 	x, y, b;
short 	b;
chtype	a;
char	*msg;
#endif
{	
	int   e, nc;
	short   i;
	chtype	c;
	char *m;

	nc = COLORS > NDCOLORS ? NDCOLORS : COLORS;
	if( b > COLORS-1)
		b = COLORS-1;
	if( b < 0 )
		b = 0;

	i = 1;			/* color_pair 0 is pre-defined */
	e = init_pair( i++, COLOR_BLACK,   b );
	e = init_pair( i++, COLOR_RED,     b );
	e = init_pair( i++, COLOR_GREEN,   b );
	e = init_pair( i++, COLOR_YELLOW,  b );
	e = init_pair( i++, COLOR_BLUE,    b );
	e = init_pair( i++, COLOR_MAGENTA, b );
	e = init_pair( i++, COLOR_CYAN,    b );
	e = init_pair( i++, COLOR_WHITE,   b );
	if( e != OK )
		return ERR;

	for( m = msg; *m != '\0'; m++ )
	{	c = (((m-msg)/3) % (nc)) + 1;
#ifdef   DEBUG
		{	short fg, bg;
			pair_content( (short)c, &fg, &bg );
			PRINTF(STDERR,"    c=%d on b=%d; pair: %d %d\n", c, b, fg,bg );
		}
#endif /*DEBUG*/
		e = mvwaddch( Awin, y, x++, (((chtype)(*m)) | COLOR_PAIR(c) | a) );
	}
#ifdef   DEBUG
		PRINTF(STDERR,"\n" );
#endif /*DEBUG*/

	return e;
}


#if __STD_C
static void done( int e )
#else
static void done( e )
int 	e;
#endif
{	/* done */
	/* standend(); */
	refresh();
	endwin();
	exit( e );
}


#if __STD_C
main( int argc, char** argv )
#else
main( argc, argv )
int 	argc;
char**	argv;
#endif
{
	short	c;

	while(argc > 1 && argv[1][0] == '-')
	{	switch(argv[1][1])
		{	case '?' :
				PRINTF(STDERR,"Usage: %s\n", &argv[0][0] );
				exit( EXIT_HELP );
				break;
			default :
				PRINTF(STDERR,"Unknown argument: %s\n", &argv[1][0] );
				break;
		}
		argc -= 1;
		argv += 1;
	}

	/* start curses */
	if( !(Awin=initscr()) )
	{	PRINTF(STDERR,"Cannot start curses.\n");
		exit( EXIT_FALSE );
	}

	/* start color */
	if( start_color() == ERR )
	{	a_msg( Cury++, Curx, A_BOLD, No_Colors );
#ifdef   DEBUG
		PRINTF(STDERR,"Cannot start curses color.\n" );
#endif /*DEBUG*/
		done( EXIT_NOCOLOR );
	}

	a_msg( Cury++, Curx, A_BOLD, Has_Color );
#ifdef   DEBUG
	PRINTF(STDERR,"has_color; start_color OK.\n" );
#endif /*DEBUG*/


	/* show default colors */
	Cury++;
	for( c=COLORS-1; c >= 0; c-- )		/* white is last default color */
	{	c_msg( Cury++, Curx, c, A_BOLD, Def_Colors );
		refresh();
	}


	/* end */
	Cury++;
	a_msg( Cury++, Curx, A_BOLD, Happy );
#ifdef   DEBUG
	PRINTF(STDERR,"happy_msg\n" );
#endif /*DEBUG*/
	done( EXIT_OK );

	return 0;
}
