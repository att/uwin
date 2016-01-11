/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
static char sccsid[] = "@(#)cmds.c	8.6 (Berkeley) 10/9/94";
#endif /* not lint */

/*
 * FTP User Program -- Command Routines.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <unistd.h>
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>

#include "ftp_var.h"

/* Returns true if STR is entirely lower case.  */
static int
all_lower (str)
	char *str;
{
	while (*str)
		if (isupper (*str++))
			return 0;
	return 1;
}

/* Returns true if STR is entirely upper case.  */
static int
all_upper (str)
	char *str;
{
	while (*str)
		if (islower (*str++))
			return 0;
	return 1;
}

/* Destructively converts STR to upper case.  */
static char *
strup (str)
	char *str;
{
	char *p;
	for (p = str; *p; p++)
		if (islower (*p))
			*p = toupper (*p);
	return str;
}

/* Destructively converts STR to lower case.  */
static char *
strdown (str)
	char *str;
{
	char *p;
	for (p = str; *p; p++)
		if (isupper (*p))
			*p = tolower (*p);
	return str;
}

jmp_buf	jabort;
char   *mname;
char   *home = "/";

char *mapin = 0;
char *mapout = 0;

/*
 * `Another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another(pargc, pargv, prompt)
	int *pargc;
	char ***pargv;
	char *prompt;
{
	int len = strlen(line), ret;

	if (len >= sizeof(line) - 3) {
		printf("sorry, arguments too long\n");
		intr();
	}
	printf("(%s) ", prompt);
	line[len++] = ' ';
	if (fgets(&line[len], sizeof(line) - len, stdin) == NULL)
		intr();
	len += strlen(&line[len]);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	makeargv();
	ret = margc > *pargc;
	*pargc = margc;
	*pargv = margv;
	return (ret);
}

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	char *host;
	short port;

	if (connected) {
		printf("Already connected to %s, use close first.\n",
			hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void) another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
		printf("usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	port = sp->s_port;
	if (argc > 2) {
		port = atoi(argv[2]);
		if (port <= 0) {
			printf("%s: bad port number-- %s\n", argv[1], argv[2]);
			printf ("usage: %s host-name [port]\n", argv[0]);
			code = -1;
			return;
		}
		port = htons(port);
	}
	host = hookup(argv[1], port);
	if (host) {
		int overbose;

		connected = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void) strcpy(typename, "ascii"), type = TYPE_A;
		curtype = TYPE_A;
		(void) strcpy(formname, "non-print"), form = FORM_N;
		(void) strcpy(modename, "stream"), mode = MODE_S;
		(void) strcpy(structname, "file"), stru = STRU_F;
		(void) strcpy(bytename, "8"), bytesize = 8;
		if (autologin)
			(void) login(argv[1]);

#if defined(unix) && NBBY == 8
/*
 * this ifdef is to keep someone form "porting" this to an incompatible
 * system and not checking this out. This way they have to think about it.
 */
		overbose = verbose;
		if (debug == 0)
			verbose = -1;
		if (command("SYST") == COMPLETE && overbose) {
			char *cp, c;
			cp = strchr(reply_string+4, ' ');
			if (cp == NULL)
				cp = strchr(reply_string+4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			printf("Remote system type is %s.\n",
				reply_string+4);
			if (cp)
				*cp = c;
		}
		if (!strncmp(reply_string, "215 UNIX Type: L8", 17)) {
			if (proxy)
				unix_proxy = 1;
			else
				unix_server = 1;
			/*
			 * Set type to 0 (not specified by user),
			 * meaning binary by default, but don't bother
			 * telling server.  We can use binary
			 * for text files unless changed by the user.
			 */
			type = 0;
			(void) strcpy(typename, "binary");
			if (overbose)
			    printf("Using %s mode to transfer files.\n",
				typename);
		} else {
			if (proxy)
				unix_proxy = 0;
			else
				unix_server = 0;
			if (overbose &&
			    !strncmp(reply_string, "215 TOPS20", 10))
				printf(
"Remember to set tenex mode when transfering binary files from this machine.\n");
		}
		verbose = overbose;
#endif /* unix */
	}
}

struct	types {
	char	*t_name;
	char	*t_mode;
	int	t_type;
	char	*t_arg;
} types[] = {
	{ "ascii",	"A",	TYPE_A,	0 },
	{ "binary",	"I",	TYPE_I,	0 },
	{ "image",	"I",	TYPE_I,	0 },
	{ "ebcdic",	"E",	TYPE_E,	0 },
	{ "tenex",	"L",	TYPE_L,	bytename },
	{ NULL }
};

/*
 * Set transfer type.
 */
void
settype(argc, argv)
	int argc;
	char *argv[];
{
	struct types *p;
	int comret;

	if (argc > 2) {
		char *sep;

		printf("usage: %s [", argv[0]);
		sep = " ";
		for (p = types; p->t_name; p++) {
			printf("%s%s", sep, p->t_name);
			sep = " | ";
		}
		printf(" ]\n");
		code = -1;
		return;
	}
	if (argc < 2) {
		printf("Using %s mode to transfer files.\n", typename);
		code = 0;
		return;
	}
	for (p = types; p->t_name; p++)
		if (strcmp(argv[1], p->t_name) == 0)
			break;
	if (p->t_name == 0) {
		printf("%s: unknown mode\n", argv[1]);
		code = -1;
		return;
	}
	if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
		comret = command ("TYPE %s %s", p->t_mode, p->t_arg);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void) strcpy(typename, p->t_name);
		curtype = type = p->t_type;
	}
}

/*
 * Internal form of settype; changes current type in use with server
 * without changing our notion of the type for data transfers.
 * Used to change to and from ascii for listings.
 */
void
changetype(newtype, show)
	int newtype, show;
{
	struct types *p;
	int comret, oldverbose = verbose;

	if (newtype == 0)
		newtype = TYPE_I;
	if (newtype == curtype)
		return;
	if (debug == 0 && show == 0)
		verbose = 0;
	for (p = types; p->t_name; p++)
		if (newtype == p->t_type)
			break;
	if (p->t_name == 0) {
		printf("ftp: internal error: unknown type %d\n", newtype);
		return;
	}
	if (newtype == TYPE_L && bytename[0] != '\0')
		comret = command("TYPE %s %s", p->t_mode, bytename);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE)
		curtype = newtype;
	verbose = oldverbose;
}

char *stype[] = {
	"type",
	"",
	0
};

/*
 * Set binary transfer type.
 */
/*VARARGS*/
void
setbinary(argc, argv)
	int argc;
	char **argv;
{

	stype[1] = "binary";
	settype(2, stype);
}

/*
 * Set ascii transfer type.
 */
/*VARARGS*/
void
setascii(argc, argv)
	int argc;
	char *argv[];
{

	stype[1] = "ascii";
	settype(2, stype);
}

/*
 * Set tenex transfer type.
 */
/*VARARGS*/
void
settenex(argc, argv)
	int argc;
	char *argv[];
{

	stype[1] = "tenex";
	settype(2, stype);
}

/*
 * Set file transfer mode.
 */
/*ARGSUSED*/
void
setftmode(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s mode, sorry.\n", modename);
	code = -1;
}

/*
 * Set file transfer format.
 */
/*ARGSUSED*/
void
setform(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s format, sorry.\n", formname);
	code = -1;
}

/*
 * Set file transfer structure.
 */
/*ARGSUSED*/
void
setstruct(argc, argv)
	int argc;
	char *argv[];
{

	printf("We only support %s structure, sorry.\n", structname);
	code = -1;
}

/*
 * Send a single file.
 */
void
put(argc, argv)
	int argc;
	char *argv[];
{
	char *cmd, *local, *remote;
	int loc = 0;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2 && !another(&argc, &argv, "local-file"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "remote-file")) {
usage:
		printf("usage: %s local-file remote-file\n", argv[0]);
		code = -1;
		return;
	}

	local = globulize (argv[1]);
	if (! local) {
		code = -1;
		return;
	}

	/*
	 * If "globulize" modifies argv[1], and argv[2] is a copy of
	 * the old argv[1], make it a copy of the new argv[1].
	 */
	if (loc)
		remote = strdup (local);
	else
		remote = strdup (argv[2]);

	cmd = (argv[0][0] == 'a') ? "APPE" : ((sunique) ? "STOU" : "STOR");
	if (loc && ntflag) {
		char *new = dotrans(remote);
		free (remote);
		remote = new;
	}
	if (loc && mapflag) {
		char *new = domap(remote);
		free (remote);
		remote = new;
	}
	sendrequest(cmd, local, remote,
		    strcmp (argv[1], local) != 0
		    || strcmp (argv[2], remote) != 0);
	free (local);
	free (remote);
}

/*
 * Send multiple files.
 */
void
mput(argc, argv)
	int argc;
	char **argv;
{
	int i;
	sig_t oldintr;
	int ointer;

	if (argc < 2 && !another(&argc, &argv, "local-files")) {
		printf("usage: %s local-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	if (proxy) {
		char *cp;

		while ((cp = remglob(argv,0)) != NULL) {
			if (*cp == 0)
				mflag = 0;
			if (mflag && confirm(argv[0], cp)) {
				char *tp = cp;

				if (mcase) {
					if (all_upper (tp))
						tp = strdown (strdup (tp));
				}
				if (ntflag) {
					char *new = dotrans(tp);
					if (tp != cp)
						free (tp);
					tp = new;
				}
				if (mapflag) {
					char *new = domap(tp);
					if (tp != cp)
						free (tp);
					tp = new;
				}
				sendrequest((sunique) ? "STOU" : "STOR",
				    cp, tp, cp != tp || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}

				if (tp != cp)
					free (tp);
			}

			free (cp);
		}
		(void) signal(SIGINT, oldintr);
		mflag = 0;
		return;
	}
	for (i = 1; i < argc; i++) {
		char **cpp, **gargs;
		glob_t gl;
		int flags;

		if (!doglob) {
			if (mflag && confirm(argv[0], argv[i])) {
				char *tp = argv[i];
				if (ntflag)
					tp = dotrans (tp);
				if (mapflag) {
					char *new = domap (tp);
					if (tp != argv[i])
						free (tp);
					tp = new;
				}
				sendrequest((sunique) ? "STOU" : "STOR",
				    argv[i], tp, tp != argv[i] || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}
				if (tp != argv[i])
					free (tp);
			}
			continue;
		}

		memset(&gl, 0, sizeof(gl));
		flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE;
#ifdef GLOB_QUOTE
		flags |= GLOB_QUOTE;
#endif
		if (glob(argv[i], flags, NULL, &gl) || gl.gl_pathc == 0) {
			warnx("%s: not found", argv[i]);
			globfree(&gl);
			continue;
		}
		for (cpp = gl.gl_pathv; cpp && *cpp != NULL; cpp++) {
			if (mflag && confirm(argv[0], *cpp)) {
				char *tp = *cpp;
				if (ntflag)
					tp = dotrans (tp);
				if (mapflag) {
					char *new = domap (tp);
					if (tp != *cpp)
						free (tp);
					tp = new;
				}
				sendrequest((sunique) ? "STOU" : "STOR",
				    *cpp, tp, *cpp != tp || !interactive);
				if (!mflag && fromatty) {
					ointer = interactive;
					interactive = 1;
					if (confirm("Continue with","mput")) {
						mflag++;
					}
					interactive = ointer;
				}
				if (tp != *cpp)
					free (tp);
			}
		}
		globfree(&gl);
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

void
reget(argc, argv)
	int argc;
	char *argv[];
{

	(void) getit(argc, argv, 1, "r+w");
}

void
get(argc, argv)
	int argc;
	char *argv[];
{

	(void) getit(argc, argv, 0, restart_point ? "r+w" : "w" );
}

/*
 * Receive one file.
 */
int
getit(argc, argv, restartit, mode)
	int argc;
	char *argv[];
	char *mode;
	int restartit;
{
	int loc = 0;
	char *local;

	if (argc == 2) {
		argc++;
		argv[2] = argv[1];
		loc++;
	}
	if (argc < 2 && !another(&argc, &argv, "remote-file"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "local-file")) {
usage:
		printf("usage: %s remote-file [ local-file ]\n", argv[0]);
		code = -1;
		return (0);
	}

	local = globulize (argv[2]);
	if (! local) {
		code = -1;
		return (0);
	}
	if (loc && mcase && all_upper (local))
		strdown (local);
	if (loc && ntflag) {
		char *new = dotrans(local);
		free (local);
		local = new;
	}
	if (loc && mapflag) {
		char *new = domap(local);
		free (local);
		local = new;
	}
	if (restartit) {
		struct stat stbuf;
		int ret;

		ret = stat(local, &stbuf);
		if (restartit == 1) {
			if (ret < 0) {
				warn("local: %s", local);
				free (local);
				return (0);
			}
			restart_point = stbuf.st_size;
		} else {
			if (ret == 0) {
				int overbose;

				overbose = verbose;
				if (debug == 0)
					verbose = -1;
				if (command("MDTM %s", argv[1]) == COMPLETE) {
					int yy, mo, day, hour, min, sec;
					struct tm *tm;
					verbose = overbose;
					sscanf(reply_string,
					    "%*s %04d%02d%02d%02d%02d%02d",
					    &yy, &mo, &day, &hour, &min, &sec);
					tm = gmtime(&stbuf.st_mtime);
					tm->tm_mon++;
					if (tm->tm_year > yy%100) {
						free (local);
						return (1);
					}
					if ((tm->tm_year == yy%100 &&
					    tm->tm_mon > mo) ||
					   (tm->tm_mon == mo &&
					    tm->tm_mday > day) ||
					   (tm->tm_mday == day &&
					    tm->tm_hour > hour) ||
					   (tm->tm_hour == hour &&
					    tm->tm_min > min) ||
					   (tm->tm_min == min &&
					    tm->tm_sec > sec)) {
						free (local);
						return (1);
					}
				} else {
					printf("%s\n", reply_string);
					verbose = overbose;
					free (local);
					return (0);
				}
			}
		}
	}

	recvrequest("RETR", local, argv[1], mode, strcmp (local, argv[2]) != 0);
	restart_point = 0;
	free (local);
	return (0);
}

/* ARGSUSED */
void
mabort(signo)
	int signo;
{
	int ointer;

	printf("\n");
	(void) fflush(stdout);
	if (mflag && fromatty) {
		ointer = interactive;
		interactive = 1;
		if (confirm("Continue with", mname)) {
			interactive = ointer;
			longjmp(jabort,0);
		}
		interactive = ointer;
	}
	mflag = 0;
	longjmp(jabort,0);
}

/*
 * Get multiple files.
 */
void
mget(argc, argv)
	int argc;
	char **argv;
{
	sig_t oldintr;
	int ch, ointer;
	char *cp, *tp, *tp2;

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv,proxy)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			tp = cp;
			if (mcase && ! all_lower (tp))
				tp = strdown (strdup (tp));
			if (ntflag) {
				char *new = dotrans (tp);
				if (tp != cp)
					free (tp);
				tp = new;
			}
			if (mapflag) {
				char *new = domap (tp);
				if (tp != cp)
					free (tp);
				tp = new;
			}
			recvrequest("RETR", tp, cp, "w",
				    tp != cp || !interactive);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with","mget")) {
					mflag++;
				}
				interactive = ointer;
			}
			if (tp != cp)
				free (tp);
		}
		free (cp);
	}
	(void) signal(SIGINT,oldintr);
	mflag = 0;
}

char *
remglob(argv,doswitch)
	char *argv[];
	int doswitch;
{
	static FILE *ftemp = NULL;
	static char **args;
	int buf_len = 0;
	char *buf = 0;
	int sofar = 0;
	int oldverbose, oldhash;
	char *cp, *mode, *end;

	if (!mflag) {
		if (!doglob) {
			args = NULL;
		}
		else {
			if (ftemp) {
				(void) fclose(ftemp);
				ftemp = NULL;
			}
		}
		return (NULL);
	}
	if (!doglob) {
		if (args == NULL)
			args = argv;
		if ((cp = *++args) == NULL)
			args = NULL;
		return cp ? 0 : strdup (cp);
	}
	if (ftemp == NULL) {
		char temp[sizeof PATH_TMP + sizeof "XXXXXX"];

		strcpy (temp, PATH_TMP);
		strcat (temp, "XXXXXX");
		mktemp (temp);

		oldverbose = verbose, verbose = 0;
		oldhash = hash, hash = 0;
		if (doswitch) {
			pswitch(!proxy);
		}
		for (mode = "w"; *++argv != NULL; mode = "a")
			recvrequest ("NLST", temp, *argv, mode, 0);
		if (doswitch) {
			pswitch(!proxy);
		}
		verbose = oldverbose; hash = oldhash;
		ftemp = fopen(temp, "r");
		(void) unlink(temp);
		if (ftemp == NULL) {
			printf("can't find list of remote files, oops\n");
			return (NULL);
		}
	}

	buf_len = 100;		/* Any old size */
	buf = malloc (buf_len + 1);

	sofar = 0;
	for (;;) {
		if (! buf) {
			printf ("malloc failure\n");
			return 0;
		}
		if (! fgets(buf + sofar, buf_len - sofar, ftemp)) {
			fclose(ftemp);
			ftemp = NULL;
			free (buf);
			return 0;
		}

		sofar = strlen (buf);
		if (buf[sofar - 1] == '\n') {
			buf[sofar - 1] = '\0';
			return buf;
		}

		/* Make more room and read some more... */
		buf_len += buf_len;
		buf = realloc (buf, buf_len);
	}
}

char *
onoff(bool)
	int bool;
{
	return (bool ? "on" : "off");
}

/*
 * Show status.
 */
/*ARGSUSED*/
void
status(argc, argv)
	int argc;
	char *argv[];
{
	int i;

	if (connected)
		printf("Connected to %s.\n", hostname);
	else
		printf("Not connected.\n");
	if (!proxy) {
		pswitch(1);
		if (connected) {
			printf("Connected for proxy commands to %s.\n", hostname);
		}
		else {
			printf("No proxy connection.\n");
		}
		pswitch(0);
	}
	printf("Mode: %s; Type: %s; Form: %s; Structure: %s\n",
		modename, typename, formname, structname);
	printf("Verbose: %s; Bell: %s; Prompting: %s; Globbing: %s\n",
		onoff(verbose), onoff(bell), onoff(interactive),
		onoff(doglob));
	printf("Store unique: %s; Receive unique: %s\n", onoff(sunique),
		onoff(runique));
	printf("Case: %s; CR stripping: %s\n",onoff(mcase),onoff(crflag));
	if (ntflag) {
		printf("Ntrans: (in) %s (out) %s\n", ntin,ntout);
	}
	else {
		printf("Ntrans: off\n");
	}
	if (mapflag) {
		printf("Nmap: (in) %s (out) %s\n", mapin, mapout);
	}
	else {
		printf("Nmap: off\n");
	}
	printf("Hash mark printing: %s; Use of PORT cmds: %s\n",
		onoff(hash), onoff(sendport));
	if (macnum > 0) {
		printf("Macros:\n");
		for (i=0; i<macnum; i++) {
			printf("\t%s\n",macros[i].mac_name);
		}
	}
	code = 0;
}

/*
 * Set beep on cmd completed mode.
 */
/*VARARGS*/
void
setbell(argc, argv)
	int argc;
	char *argv[];
{

	bell = !bell;
	printf("Bell mode %s.\n", onoff(bell));
	code = bell;
}

/*
 * Turn on packet tracing.
 */
/*VARARGS*/
void
settrace(argc, argv)
	int argc;
	char *argv[];
{

	trace = !trace;
	printf("Packet tracing %s.\n", onoff(trace));
	code = trace;
}

/*
 * Toggle hash mark printing during transfers.
 */
/*VARARGS*/
void
sethash(argc, argv)
	int argc;
	char *argv[];
{

	hash = !hash;
	printf("Hash mark printing %s", onoff(hash));
	code = hash;
	if (hash)
		printf(" (%d bytes/hash mark)", 1024);
	printf(".\n");
}

/*
 * Turn on printing of server echo's.
 */
/*VARARGS*/
void
setverbose(argc, argv)
	int argc;
	char *argv[];
{

	verbose = !verbose;
	printf("Verbose mode %s.\n", onoff(verbose));
	code = verbose;
}

/*
 * Toggle PORT cmd use before each data connection.
 */
/*VARARGS*/
void
setport(argc, argv)
	int argc;
	char *argv[];
{

	sendport = !sendport;
	printf("Use of PORT cmds %s.\n", onoff(sendport));
	code = sendport;
}

/*
 * Turn on interactive prompting
 * during mget, mput, and mdelete.
 */
/*VARARGS*/
void
setprompt(argc, argv)
	int argc;
	char *argv[];
{

	interactive = !interactive;
	printf("Interactive mode %s.\n", onoff(interactive));
	code = interactive;
}

/*
 * Toggle metacharacter interpretation
 * on local file names.
 */
/*VARARGS*/
void
setglob(argc, argv)
	int argc;
	char *argv[];
{

	doglob = !doglob;
	printf("Globbing %s.\n", onoff(doglob));
	code = doglob;
}

/*
 * Set debugging mode on/off and/or
 * set level of debugging.
 */
/*VARARGS*/
void
setdebug(argc, argv)
	int argc;
	char *argv[];
{
	int val;

	if (argc > 1) {
		val = atoi(argv[1]);
		if (val < 0) {
			printf("%s: bad debugging value.\n", argv[1]);
			code = -1;
			return;
		}
	} else
		val = !debug;
	debug = val;
	if (debug)
		options |= SO_DEBUG;
	else
		options &= ~SO_DEBUG;
	printf("Debugging %s (debug=%d).\n", onoff(debug), debug);
	code = debug > 0;
}

/*
 * Set current working directory
 * on remote machine.
 */
void
cd(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "remote-directory")) {
		printf("usage: %s remote-directory\n", argv[0]);
		code = -1;
		return;
	}
	if (command("CWD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("CWD command not recognized, trying XCWD\n");
		(void) command("XCWD %s", argv[1]);
	}
}

/*
 * Set current working directory
 * on local machine.
 */
void
lcd(argc, argv)
	int argc;
	char *argv[];
{
	char *dir;

	if (argc < 2)
		argc++, argv[1] = home;
	if (argc != 2) {
		printf("usage: %s local-directory\n", argv[0]);
		code = -1;
		return;
	}

	dir = globulize (argv[1]);
	if (! dir) {
		code = -1;
		return;
	}

	if (chdir(dir) < 0) {
		warn("dir: %s", dir);
		free (dir);
		code = -1;
		return;
	}

	free (dir);

#ifdef HAVE_GETCWD_ZERO_SIZE
	/* A size arg of zero means `as big as necessary.  */
	dir = getcwd (0, 0);
#else /* !HAVE_GETCWD_ZERO_SIZE */
#ifdef PATH_MAX
	dir = getcwd (0, PATH_MAX);
#else /* !PATH_MAX */
#ifdef MAXPATHLEN
	dir = getcwd (0, MAXPATHLEN);
#else /* !MAXPATHLEN */
	dir = getcwd (0, 2048);
#endif /* MAXPATHLEN */
#endif /* PATH_MAX */
#endif /* HAVE_GETCWD_ZERO_SIZE */

	if (dir) {
		printf("Local directory now %s\n", dir);
		free (dir);
	} else
		warnx("getcwd: %s", strerror (errno));
	code = 0;
}

/*
 * Delete a single file.
 */
void
delete(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "remote-file")) {
		printf("usage: %s remote-file\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("DELE %s", argv[1]);
}

/*
 * Delete multiple files.
 */
void
mdelete(argc, argv)
	int argc;
	char **argv;
{
	sig_t oldintr;
	int ointer;
	char *cp;

	if (argc < 2 && !another(&argc, &argv, "remote-files")) {
		printf("usage: %s remote-files\n", argv[0]);
		code = -1;
		return;
	}
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	while ((cp = remglob(argv,0)) != NULL) {
		if (*cp == '\0') {
			mflag = 0;
			continue;
		}
		if (mflag && confirm(argv[0], cp)) {
			(void) command("DELE %s", cp);
			if (!mflag && fromatty) {
				ointer = interactive;
				interactive = 1;
				if (confirm("Continue with", "mdelete")) {
					mflag++;
				}
				interactive = ointer;
			}
		}
		free (cp);
	}
	(void) signal(SIGINT, oldintr);
	mflag = 0;
}

/*
 * Rename a remote file.
 */
void
renamefile(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "from-name"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "to-name")) {
usage:
		printf("%s from-name to-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RNFR %s", argv[1]) == CONTINUE)
		(void) command("RNTO %s", argv[2]);
}

/*
 * Get a directory listing
 * of remote files.
 */
void
ls(argc, argv)
	int argc;
	char *argv[];
{
	char *cmd, *dest;

	if (argc < 2)
		argc++, argv[1] = NULL;
	if (argc < 3)
		argc++, argv[2] = "-";
	if (argc > 3) {
		printf("usage: %s remote-directory local-file\n", argv[0]);
		code = -1;
		return;
	}
	cmd = argv[0][0] == 'n' ? "NLST" : "LIST";

	if (strcmp(argv[2], "-") != 0) {
		dest = globulize(argv[2]);
		if (! dest) {
			code = -1;
			return;
		}
		if (*dest != '|' && !confirm("output to local-file:", dest)) {
			code = -1;
			goto out;
		}
	} else
		dest = 0;

	recvrequest(cmd, dest ? dest : "-", argv[1], "w", 0);
 out:
	if (dest)
		free (dest);
}

/*
 * Get a directory listing
 * of multiple remote files.
 */
void
mls(argc, argv)
	int argc;
	char **argv;
{
	sig_t oldintr;
	int ointer, i;
	char *cmd, mode[1], *dest;

	if (argc < 2 && !another(&argc, &argv, "remote-files"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "local-file")) {
usage:
		printf("usage: %s remote-files local-file\n", argv[0]);
		code = -1;
		return;
	}

	dest = argv[argc - 1];
	argv[argc - 1] = NULL;
	if (strcmp(dest, "-") && *dest != '|') {
		dest = globulize (dest);
		if (! dest) {
			code = -1;
			return;
		}
		if (! confirm("output to local-file:", dest)) {
			code = -1;
			free (dest);
			return;
		}
	} else
		dest = strdup (dest);

	cmd = argv[0][1] == 'l' ? "NLST" : "LIST";
	mname = argv[0];
	mflag = 1;
	oldintr = signal(SIGINT, mabort);
	(void) setjmp(jabort);
	for (i = 1; mflag && i < argc-1; ++i) {
		*mode = (i == 1) ? 'w' : 'a';
		recvrequest(cmd, dest, argv[i], mode, 0);
		if (!mflag && fromatty) {
			ointer = interactive;
			interactive = 1;
			if (confirm("Continue with", argv[0])) {
				mflag ++;
			}
			interactive = ointer;
		}
	}

	(void) signal(SIGINT, oldintr);
	mflag = 0;
	free (dest);
}

/*
 * Do a shell escape
 */
/*ARGSUSED*/
void
shell(argc, argv)
	int argc;
	char **argv;
{
	pid_t pid;
	sig_t old1, old2;
	char shellnam[40], *shell, *namep;

	old1 = signal (SIGINT, SIG_IGN);
	old2 = signal (SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		for (pid = 3; pid < 20; pid++)
			(void) close(pid);
		(void) signal(SIGINT, SIG_DFL);
		(void) signal(SIGQUIT, SIG_DFL);
		shell = getenv("SHELL");
		if (shell == NULL)
			shell = PATH_BSHELL;
		namep = strrchr(shell,'/');
		if (namep == NULL)
			namep = shell;
		(void) strcpy(shellnam,"-");
		(void) strcat(shellnam, ++namep);
		if (strcmp(namep, "sh") != 0)
			shellnam[0] = '+';
		if (debug) {
			printf ("%s\n", shell);
			(void) fflush (stdout);
		}
		if (argc > 1) {
			execl(shell,shellnam,"-c",altarg,(char *)0);
		}
		else {
			execl(shell,shellnam,(char *)0);
		}
		warn("%s", shell);
		code = -1;
		exit(1);
	}
	if (pid > 0)
		while (wait(0) != pid)
			;
	(void) signal(SIGINT, old1);
	(void) signal(SIGQUIT, old2);
	if (pid == -1) {
		warn("%s", "Try again later");
		code = -1;
	}
	else {
		code = 0;
	}
}

/*
 * Send new user information (re-login)
 */
void
user(argc, argv)
	int argc;
	char **argv;
{
	char acct[80];
	int n, aflag = 0;

	if (argc < 2)
		(void) another(&argc, &argv, "username");
	if (argc < 2 || argc > 4) {
		printf("usage: %s username [password] [account]\n", argv[0]);
		code = -1;
		return;
	}
	n = command("USER %s", argv[1]);
	if (n == CONTINUE) {
		if (argc < 3 )
			argv[2] = getpass("Password: "), argc++;
		n = command("PASS %s", argv[2]);
	}
	if (n == CONTINUE) {
		if (argc < 4) {
			printf("Account: "); (void) fflush(stdout);
			(void) fgets(acct, sizeof(acct) - 1, stdin);
			acct[strlen(acct) - 1] = '\0';
			argv[3] = acct; argc++;
		}
		n = command("ACCT %s", argv[3]);
		aflag++;
	}
	if (n != COMPLETE) {
		fprintf(stdout, "Login failed.\n");
		return;
	}
	if (!aflag && argc == 4) {
		(void) command("ACCT %s", argv[3]);
	}
}

/*
 * Print working directory.
 */
/*VARARGS*/
void
pwd(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	/*
	 * If we aren't verbose, this doesn't do anything!
	 */
	verbose = 1;
	if (command("PWD") == ERROR && code == 500) {
		printf("PWD command not recognized, trying XPWD\n");
		(void) command("XPWD");
	}
	verbose = oldverbose;
}

/*
 * Make a directory.
 */
void
makedir(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "directory-name")) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("MKD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("MKD command not recognized, trying XMKD\n");
		(void) command("XMKD %s", argv[1]);
	}
}

/*
 * Remove a directory.
 */
void
removedir(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "directory-name")) {
		printf("usage: %s directory-name\n", argv[0]);
		code = -1;
		return;
	}
	if (command("RMD %s", argv[1]) == ERROR && code == 500) {
		if (verbose)
			printf("RMD command not recognized, trying XRMD\n");
		(void) command("XRMD %s", argv[1]);
	}
}

/*
 * Send a line, verbatim, to the remote machine.
 */
void
quote(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "command line to send")) {
		printf("usage: %s line-to-send\n", argv[0]);
		code = -1;
		return;
	}
	quote1("", argc, argv);
}

/*
 * Send a SITE command to the remote machine.  The line
 * is sent verbatim to the remote machine, except that the
 * word "SITE" is added at the front.
 */
void
site(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "arguments to SITE command")) {
		printf("usage: %s line-to-send\n", argv[0]);
		code = -1;
		return;
	}
	quote1("SITE ", argc, argv);
}

/*
 * Turn argv[1..argc) into a space-separated string, then prepend initial text.
 * Send the result as a one-line command and get response.
 */
void
quote1(initial, argc, argv)
	char *initial;
	int argc;
	char **argv;
{
	int i, len;
	char buf[BUFSIZ];		/* must be >= sizeof(line) */

	(void) strcpy(buf, initial);
	if (argc > 1) {
		len = strlen(buf);
		len += strlen(strcpy(&buf[len], argv[1]));
		for (i = 2; i < argc; i++) {
			buf[len++] = ' ';
			len += strlen(strcpy(&buf[len], argv[i]));
		}
	}
	if (command(buf) == PRELIM) {
		while (getreply(0) == PRELIM)
			continue;
	}
}

void
do_chmod(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "mode"))
		goto usage;
	if (argc < 3 && !another(&argc, &argv, "file-name")) {
usage:
		printf("usage: %s mode file-name\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("SITE CHMOD %s %s", argv[1], argv[2]);
}

void
do_umask(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "SITE UMASK" : "SITE UMASK %s", argv[1]);
	verbose = oldverbose;
}

void
site_idle(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "SITE IDLE" : "SITE IDLE %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Ask the other side for help.
 */
void
rmthelp(argc, argv)
	int argc;
	char *argv[];
{
	int oldverbose = verbose;

	verbose = 1;
	(void) command(argc == 1 ? "HELP" : "HELP %s", argv[1]);
	verbose = oldverbose;
}

/*
 * Terminate session and exit.
 */
/*VARARGS*/
void
quit(argc, argv)
	int argc;
	char *argv[];
{

	if (connected)
		disconnect(0, 0);
	pswitch(1);
	if (connected) {
		disconnect(0, 0);
	}
	exit(0);
}

/*
 * Terminate session, but don't exit.
 */
void
disconnect(argc, argv)
	int argc;
	char *argv[];
{

	if (!connected)
		return;
	(void) command("QUIT");
	if (cout) {
		(void) fclose(cout);
	}
	cout = NULL;
	connected = 0;
	data = -1;
	if (!proxy) {
		macnum = 0;
	}
}

int
confirm(cmd, file)
	char *cmd, *file;
{
	char line[BUFSIZ];

	if (!interactive)
		return (1);
	printf("%s %s? ", cmd, file);
	(void) fflush(stdout);
	if (fgets(line, sizeof line, stdin) == NULL)
		return (0);
	return (*line != 'n' && *line != 'N');
}

void
fatal(msg)
	char *msg;
{

	errx(1, "%s", msg);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
char *
globulize(cp)
	char *cp;
{
	glob_t gl;
	int flags;

	if (!doglob)
		return strdup (cp);

	flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE;
#ifdef GLOB_QUOTE
	flags |= GLOB_QUOTE;
#endif

	memset(&gl, 0, sizeof(gl));
	if (glob(cp, flags, NULL, &gl) ||
	    gl.gl_pathc == 0) {
		warnx("%s: not found", cp);
		globfree(&gl);
		return (0);
	}

	cp = strdup(gl.gl_pathv[0]);
	globfree(&gl);

	return cp;
}

void
account(argc,argv)
	int argc;
	char **argv;
{
	char acct[50], *ap;

	if (argc > 1) {
		++argv;
		--argc;
		(void) strncpy(acct,*argv,49);
		acct[49] = '\0';
		while (argc > 1) {
			--argc;
			++argv;
			(void) strncat(acct,*argv, 49-strlen(acct));
		}
		ap = acct;
	}
	else {
		ap = getpass("Account:");
	}
	(void) command("ACCT %s", ap);
}

jmp_buf abortprox;

void
proxabort(sig)
  int sig;
{

	if (!proxy) {
		pswitch(1);
	}
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	longjmp(abortprox,1);
}

void
doproxy(argc, argv)
	int argc;
	char *argv[];
{
	struct cmd *c;
	sig_t oldintr;

	if (argc < 2 && !another(&argc, &argv, "command")) {
		printf("usage: %s command\n", argv[0]);
		code = -1;
		return;
	}
	c = getcmd(argv[1]);
	if (c == (struct cmd *) -1) {
		printf("?Ambiguous command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (c == 0) {
		printf("?Invalid command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (!c->c_proxy) {
		printf("?Invalid proxy command\n");
		(void) fflush(stdout);
		code = -1;
		return;
	}
	if (setjmp(abortprox)) {
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, proxabort);
	pswitch(1);
	if (c->c_conn && !connected) {
		printf("Not connected\n");
		(void) fflush(stdout);
		pswitch(0);
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	(*c->c_handler)(argc-1, argv+1);
	if (connected) {
		proxflag = 1;
	}
	else {
		proxflag = 0;
	}
	pswitch(0);
	(void) signal(SIGINT, oldintr);
}

void
setcase(argc, argv)
	int argc;
	char *argv[];
{

	mcase = !mcase;
	printf("Case mapping %s.\n", onoff(mcase));
	code = mcase;
}

void
setcr(argc, argv)
	int argc;
	char *argv[];
{

	crflag = !crflag;
	printf("Carriage Return stripping %s.\n", onoff(crflag));
	code = crflag;
}

void
setntrans(argc,argv)
	int argc;
	char *argv[];
{
	if (argc == 1) {
		ntflag = 0;
		printf("Ntrans off.\n");
		code = ntflag;
		return;
	}
	ntflag++;
	code = ntflag;
	(void) strncpy(ntin, argv[1], 16);
	ntin[16] = '\0';
	if (argc == 2) {
		ntout[0] = '\0';
		return;
	}
	(void) strncpy(ntout, argv[2], 16);
	ntout[16] = '\0';
}

char *
dotrans(name)
	char *name;
{
	char *new = malloc (strlen (name) + 1);
	char *cp1, *cp2 = new;
	int i, ostop, found;

	for (ostop = 0; *(ntout + ostop) && ostop < 16; ostop++)
		continue;
	for (cp1 = name; *cp1; cp1++) {
		found = 0;
		for (i = 0; *(ntin + i) && i < 16; i++) {
			if (*cp1 == *(ntin + i)) {
				found++;
				if (i < ostop) {
					*cp2++ = *(ntout + i);
				}
				break;
			}
		}
		if (!found) {
			*cp2++ = *cp1;
		}
	}
	*cp2 = '\0';
	return (new);
}

void
setpassive(argc, argv)
	int argc;
	char *argv[];
{

	passivemode = !passivemode;
	printf("Passive mode %s.\n", onoff(passivemode));
	code = passivemode;
}

void
setnmap(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;

	if (argc == 1) {
		mapflag = 0;
		printf("Nmap off.\n");
		code = mapflag;
		return;
	}
	if (argc < 3 && !another(&argc, &argv, "mapout")) {
		printf("Usage: %s [mapin mapout]\n",argv[0]);
		code = -1;
		return;
	}
	mapflag = 1;
	code = 1;
	cp = strchr(altarg, ' ');
	if (proxy) {
		while(*++cp == ' ')
			continue;
		altarg = cp;
		cp = strchr(altarg, ' ');
	}
	*cp = '\0';

	if (mapin)
		free (mapin);
	mapin = strdup (altarg);

	while (*++cp == ' ')
		continue;
	if (mapout)
		free (mapout);
	mapout = strdup (cp);
}

static int
cp_subst (from_p, to_p, toks, tp, te, tok0, buf_p, buf_len_p)
char **from_p, **to_p;
char *tp[9], *te[9];
int toks[9];
char *tok0;
char **buf_p;
int *buf_len_p;
{
	int toknum;
	char *src;
	int src_len;

	if (*++(*from_p) == '0') {
		src = tok0;
		src_len = strlen (tok0);
	}
	else if (toks[toknum = **from_p - '1']) {
		src = tp[toknum];
		src_len = te[toknum] - src;
	}
	else
		return 0;

	if (src_len > 2) {
		/* This subst will be longer than the original, so make room
		   for it.  */
		*buf_len_p += src_len - 2;
		*buf_p = realloc (*buf_p, *buf_len_p);
	}
	while (src_len--)
		*(*to_p)++ = *src++;

	return 1;
}

char *
domap(name)
	char *name;
{
	int buf_len = strlen (name) + 1;
	char *buf = malloc (buf_len);
	char *cp1 = name, *cp2 = mapin;
	char *tp[9], *te[9];
	int i, toks[9], toknum = 0, match = 1;

	for (i=0; i < 9; ++i) {
		toks[i] = 0;
	}
	while (match && *cp1 && *cp2) {
		switch (*cp2) {
			case '\\':
				if (*++cp2 != *cp1) {
					match = 0;
				}
				break;
			case '$':
				if (*(cp2+1) >= '1' && (*cp2+1) <= '9') {
					if (*cp1 != *(++cp2+1)) {
						toks[toknum = *cp2 - '1']++;
						tp[toknum] = cp1;
						while (*++cp1 && *(cp2+1)
							!= *cp1);
						te[toknum] = cp1;
					}
					cp2++;
					break;
				}
				/* FALLTHROUGH */
			default:
				if (*cp2 != *cp1) {
					match = 0;
				}
				break;
		}
		if (match && *cp1) {
			cp1++;
		}
		if (match && *cp2) {
			cp2++;
		}
	}
	if (!match && *cp1) /* last token mismatch */
	{
		toks[toknum] = 0;
	}
	cp1 = buf;
	*cp1 = '\0';
	cp2 = mapout;
	while (*cp2) {
		match = 0;
		switch (*cp2) {
			case '\\':
				if (*(cp2 + 1)) {
					*cp1++ = *++cp2;
				}
				break;
			case '[':
LOOP:
				if (*++cp2 == '$' && isdigit(*(cp2+1)))
					cp_subst (&cp2, &cp1,
						  toks, tp, te, name,
						  &buf, &buf_len);
				else {
					while (*cp2 && *cp2 != ',' &&
					    *cp2 != ']') {
						if (*cp2 == '\\') {
							cp2++;
						}
						else if (*cp2 == '$' &&
   						        isdigit(*(cp2+1)))
							if (cp_subst (&cp2,
								      &cp1,
								      toks,
								      tp, te,
								      name,
								      &buf,
								      &buf_len))
								match = 1;
						else if (*cp2) {
							*cp1++ = *cp2++;
						}
					}
					if (!*cp2) {
						printf("nmap: unbalanced brackets\n");
						return (name);
					}
					match = 1;
					cp2--;
				}
				if (match) {
					while (*++cp2 && *cp2 != ']') {
					      if (*cp2 == '\\' && *(cp2 + 1)) {
							cp2++;
					      }
					}
					if (!*cp2) {
						printf("nmap: unbalanced brackets\n");
						return (name);
					}
					break;
				}
				switch (*++cp2) {
					case ',':
						goto LOOP;
					case ']':
						break;
					default:
						cp2--;
						goto LOOP;
				}
				break;
			case '$':
				if (isdigit(*(cp2 + 1))) {
					if (cp_subst (&cp2, &cp1,
						      toks, tp, te, name,
						      &buf, &buf_len))
						match = 1;
					break;
				}
				/* intentional drop through */
			default:
				*cp1++ = *cp2;
				break;
		}
		cp2++;
	}
	*cp1 = '\0';

	if (! *buf)
		strcpy (buf, name);

	return buf;
}

void
setsunique(argc, argv)
	int argc;
	char *argv[];
{

	sunique = !sunique;
	printf("Store unique %s.\n", onoff(sunique));
	code = sunique;
}

void
setrunique(argc, argv)
	int argc;
	char *argv[];
{

	runique = !runique;
	printf("Receive unique %s.\n", onoff(runique));
	code = runique;
}

/* change directory to perent directory */
void
cdup(argc, argv)
	int argc;
	char *argv[];
{

	if (command("CDUP") == ERROR && code == 500) {
		if (verbose)
			printf("CDUP command not recognized, trying XCUP\n");
		(void) command("XCUP");
	}
}

/* restart transfer at specific point */
void
restart(argc, argv)
	int argc;
	char *argv[];
{

	if (argc != 2)
		printf("restart: offset not specified\n");
	else {
		restart_point = atol(argv[1]);
		printf((sizeof(restart_point) > sizeof(long)
			? "restarting at %qd. %s\n"
			: "restarting at %ld. %s\n"), restart_point,
		    "execute get, put or append to initiate transfer");
	}
}

/* show remote system type */
void
syst(argc, argv)
	int argc;
	char *argv[];
{

	(void) command("SYST");
}

void
macdef(argc, argv)
	int argc;
	char *argv[];
{
	char *tmp;
	int c;

	if (macnum == 16) {
		printf("Limit of 16 macros have already been defined\n");
		code = -1;
		return;
	}
	if (argc < 2 && !another(&argc, &argv, "macro name")) {
		printf("Usage: %s macro_name\n",argv[0]);
		code = -1;
		return;
	}
	if (interactive) {
		printf("Enter macro line by line, terminating it with a null line\n");
	}
	(void) strncpy(macros[macnum].mac_name, argv[1], 8);
	if (macnum == 0) {
		macros[macnum].mac_start = macbuf;
	}
	else {
		macros[macnum].mac_start = macros[macnum - 1].mac_end + 1;
	}
	tmp = macros[macnum].mac_start;
	while (tmp != macbuf+4096) {
		if ((c = getchar()) == EOF) {
			printf("macdef:end of file encountered\n");
			code = -1;
			return;
		}
		if ((*tmp = c) == '\n') {
			if (tmp == macros[macnum].mac_start) {
				macros[macnum++].mac_end = tmp;
				code = 0;
				return;
			}
			if (*(tmp-1) == '\0') {
				macros[macnum++].mac_end = tmp - 1;
				code = 0;
				return;
			}
			*tmp = '\0';
		}
		tmp++;
	}
	while (1) {
		while ((c = getchar()) != '\n' && c != EOF)
			/* LOOP */;
		if (c == EOF || getchar() == '\n') {
			printf("Macro not defined - 4k buffer exceeded\n");
			code = -1;
			return;
		}
	}
}

/*
 * get size of file on remote machine
 */
void
sizecmd(argc, argv)
	int argc;
	char *argv[];
{

	if (argc < 2 && !another(&argc, &argv, "filename")) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	(void) command("SIZE %s", argv[1]);
}

/*
 * get last modification time of file on remote machine
 */
void
modtime(argc, argv)
	int argc;
	char *argv[];
{
	int overbose;

	if (argc < 2 && !another(&argc, &argv, "filename")) {
		printf("usage: %s filename\n", argv[0]);
		code = -1;
		return;
	}
	overbose = verbose;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", argv[1]) == COMPLETE) {
		int yy, mo, day, hour, min, sec;
		sscanf(reply_string, "%*s %04d%02d%02d%02d%02d%02d", &yy, &mo,
			&day, &hour, &min, &sec);
		/* might want to print this in local time */
		printf("%s\t%02d/%02d/%04d %02d:%02d:%02d GMT\n", argv[1],
			mo, day, yy, hour, min, sec);
	} else
		printf("%s\n", reply_string);
	verbose = overbose;
}

/*
 * show status on reomte machine
 */
void
rmtstatus(argc, argv)
	int argc;
	char *argv[];
{

	(void) command(argc > 1 ? "STAT %s" : "STAT" , argv[1]);
}

/*
 * get file if modtime is more recent than current file
 */
void
newer(argc, argv)
	int argc;
	char *argv[];
{

	if (getit(argc, argv, -1, "w"))
		printf("Local file \"%s\" is newer than remote file \"%s\"\n",
			argv[2], argv[1]);
}
