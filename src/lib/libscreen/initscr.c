#include	"scrhdr.h"

#if __STD_C
static void interrupt(int sig)
#else
static void interrupt(sig)
int	sig;
#endif
{

	if(sig == SIGINT || sig == SIGQUIT)
	{
#ifdef SIG_SETMASK
		sigset_t	set;
		(void)sigemptyset(&set);
		(void)sigaddset(&set, sig);
		(void)sigprocmask(SIG_UNBLOCK, &set, NIL(sigset_t*));
#endif

		signal(sig,SIG_DFL);
		endwin();
		kill(0,sig);
		pause();
		exit(1);
	}
}


/*
**	Start screen.
**
**	Written by Kiem-Phong Vo
*/
WINDOW	*initscr()
{
	reg char	*tname;
	Signal_t	sigf;

#ifdef _SFIO_H
	sfset(sfstdout,SF_LINE,0);
#else
	reg char	*buf;
	/* make stdout page buffering instead of line buffering */
	if((buf = (char *) malloc(BUFSIZ)) != NIL(char*))
		setbuf(stdout,buf);
#endif

	/* set terminal attributes */
	tname = (char *) getenv("TERM");
	curscreen = _origscreen = newscreen(tname,0,0,0,stdout,stdin);

	if(!curscreen)
		return NIL(WINDOW*);

	/* take care of stop and continue signals */
#ifdef	SIGTSTP
	if((sigf = signal(SIGTSTP,tstp)) != SIG_DFL)
		signal(SIGTSTP,sigf);
#endif

	if((sigf = signal(SIGINT,interrupt)) != SIG_DFL)
		signal(SIGINT,sigf);
	if((sigf = signal(SIGQUIT,interrupt)) != SIG_DFL)
		signal(SIGQUIT,sigf);

	return stdscr;
}
