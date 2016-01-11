/*
 * Copyright (c) 1980, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)script.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: script.c,v 1.3.2.2 1997/08/29 05:29:52 imp Exp $";
#endif /* not lint */

#if _PACKAGE_ast
#   include <ast.h>
#   include <option.h>
#   include <cmd.h>
#   include <error.h>
#   define optarg	opt_info.arg
#   define optind	opt_info.index
static const char script_usage[] =
"[-?@(#)script (The Regents of the University of California) 1997-08-29\n]"
USAGE_LICENSE
"[+NAME?script - make typescript of terminal session]"
"[+DESCRIPTION?The \bscript\b utility makes a typescript of everything"
"	printed on the terminal.]"
"[+?If the argument file is given, \bscript\b saves all dialogue in file."
"	If no file name is given, the typescript is saved in the file"
"	\btypescript\b.]"
"[+?If the argument \bcommand ...\b is given, \bscript\b will run the"
"	specified command with an optional argument vector instead of an"
"	interactive shell.]"
"[a:append?Append the output to file or typescript, retaining the prior"
"	contents.]"
#if 0
"[k:keys?Log keys sent to program as well as output.]"
"[q:quiet?Run in quiet mode, omit the start and stop status messages.]"
"[t:time?Specify time interval in seconds between flushing script output"
"	file. A value of 0 causes script to flush for every character I/O"
"	event. The default interval is 30 seconds.]:[interval]"
#endif
"\n"
"\n[ file [ command ... ] ]\n"
"\n"
"[+BUGS?The \bscript\b utility places everything in the log file, including"
"	linefeeds and backspaces. This is not what the naive user expects.]"
"[+?It is not possible to specify a command without also naming the script"
"	file because of argument parsing compatibility issues.]"
"[+?When running in \b-k\b mode, echo cancelling is far from ideal. The slave"
"	terminal mode is checked for ECHO mode to check when to avoid manual"
"	echo logging. This does not work when in a raw mode where the program"
"	being run is doing manual echo.]"
;
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


FILE	*fscript;
int	master, slave;
int	child, subchild;
int	outcc;
char	*fname;

struct	termios tt;

void	done __P((void)) __dead2;
void	dooutput __P((void));
void	doshell __P((void));
void	fail __P((void));
void	finish __P((int));
void	scriptflush __P((int));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int cc;
	struct termios rtt;
	struct winsize win;
	int aflg, ch;
	char ibuf[BUFSIZ];

	aflg = 0;
#if _PACKAGE_ast
#   if _AST_VERSION >= 20060701L
	cmdinit(argc,argv,NULL,NULL,0);
#   else
	cmdinit(argv,NULL,NULL,0);
#   endif
	while ((ch = optget(argv, script_usage)))
#else
	while ((ch = getopt(argc, argv, "a")) !=  -1)
#endif
		switch(ch) {
		case 'a':
			aflg = 1;
			break;
#if _PACKAGE_ast
		case ':':
			error(2, opt_info.arg);
			break;
		case '?':
		default:
			error(ERROR_usage(2), "%s", opt_info.arg);
			break;
#else
		case '?':
		default:
			usage();
#endif
		}
	argc -= optind;
	argv += optind;
#if _PACKAGE_ast
        if(error_info.errors)
                error(ERROR_usage(2),"%s", optusage((char*)0));
#endif

	if (argc > 0)
		fname = argv[0];
	else
		fname = "typescript";

	if ((fscript = fopen(fname, aflg ? "a" : "w")) == NULL)
		err(1, "%s", fname);

	(void)tcgetattr(STDIN_FILENO, &tt);
	(void)ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	if (openpty(&master, &slave, NULL, &tt, &win) == -1)
		err(1, "openpty");

	(void)printf("Script started, output file is %s\n", fname);
	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);

	(void)signal(SIGCHLD, finish);
	child = fork();
	if (child < 0) {
		warn("fork");
		fail();
	}
	if (child == 0) {
		subchild = child = fork();
		if (child < 0) {
			warn("fork");
			fail();
		}
		if (child)
			dooutput();
		else
			doshell();
	}

	(void)fclose(fscript);
	while ((cc = read(STDIN_FILENO, ibuf, BUFSIZ)) > 0)
		(void)write(master, ibuf, cc);
	done();
}

static void
usage()
{
	(void)fprintf(stderr, "usage: script [-a] [file]\n");
	exit(1);
}

void
finish(signo)
	int signo;
{
	register int die, pid;
	union wait status;

	die = 0;
	while ((pid = wait3((int *)&status, WNOHANG, 0)) > 0)
		if (pid == child)
			die = 1;

	if (die)
		done();
}

void
dooutput()
{
	struct itimerval value;
	register int cc;
	time_t tvec;
	char obuf[BUFSIZ];

	(void)close(STDIN_FILENO);
	tvec = time(NULL);
	(void)fprintf(fscript, "Script started on %s", ctime(&tvec));

	(void)signal(SIGALRM, scriptflush);
	value.it_interval.tv_sec = 60 / 2;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	(void)setitimer(ITIMER_REAL, &value, NULL);
	for (;;) {
		cc = read(master, obuf, sizeof (obuf));
		if (cc <= 0)
			break;
		(void)write(1, obuf, cc);
		(void)fwrite(obuf, 1, cc, fscript);
		outcc += cc;
	}
	done();
}

void
scriptflush(signo)
	int signo;
{
	if (outcc) {
		(void)fflush(fscript);
		outcc = 0;
	}
}

void
doshell()
{
	char *shell;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	(void)close(master);
	(void)fclose(fscript);
	login_tty(slave);
	execl(shell, "sh", "-i", NULL);
	warn(shell);
	fail();
}

void
fail()
{

	(void)kill(0, SIGTERM);
	done();
}

void
done()
{
	time_t tvec;

	if (subchild) {
		tvec = time(NULL);
		(void)fprintf(fscript,"\nScript done on %s", ctime(&tvec));
		(void)fclose(fscript);
		(void)close(master);
	} else {
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
		(void)printf("Script done, output file is %s\n", fname);
	}
	exit(0);
}

