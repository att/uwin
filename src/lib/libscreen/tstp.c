#include	"scrhdr.h"

#ifdef	SIGTSTP
/*
**	To be invoked upon stop signals
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
void tstp(int sig)
#else
void tstp(sig)
int	sig;
#endif
{
	reg int		curx, cury;
	SCREEN*		savscreen;
#ifdef SIG_SETMASK
	sigset_t	oldset, set;

	/* block timers and window changes */
	(void)sigemptyset(&set);
        (void)sigaddset(&set, SIGALRM);
        (void)sigaddset(&set, SIGWINCH);
        (void)sigprocmask(SIG_BLOCK, &set, &oldset);
#endif

	/* tell getch that we were interrupted by SIGSTOP */
	_wasstopped = TRUE;

	/* save current terminal states */
	if((savscreen = curscreen) != _origscreen)
		curscreen = setcurscreen(_origscreen);

	cury = curscr->_cury;
	curx = curscr->_curx;

	/* restore terminal state to normal */
	mvcur(curscr->_cury,curscr->_curx,LINES-1,0);
	endwin();

	/* put ourself to bed */
#ifdef SIG_SETMASK
	/* unblock SISTSTP */
	(void)sigemptyset(&set);
	(void)sigaddset(&set, SIGTSTP);
	(void)sigprocmask(SIG_UNBLOCK, &set, NIL(sigset_t*));

	signal(SIGTSTP,SIG_DFL);
	kill(0,SIGTSTP);
#else
	signal(SIGTSTP,SIG_DFL);
	kill(0,SIGSTOP);
#endif

	/* JUST CAME BACK, prepare for next signal */
	signal(SIGTSTP,tstp);

	/* restore terminal states to before receiving stop signal */
	curscr->_cury = cury;
	curscr->_curx = curx;
	wrefresh(curscr);

	if(savscreen != curscreen)
		curscreen = setcurscreen(savscreen);

#ifdef SIG_SETMASK
	sigprocmask(SIG_SETMASK, &oldset, NIL(sigset_t*));
#endif
}
#endif /*SIGTSTP*/
