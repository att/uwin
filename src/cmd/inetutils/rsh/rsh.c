/*-
 * Copyright (c) 1983, 1990, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rsh.c	8.4 (Berkeley) 4/29/95";
#endif /* not lint */

/*
 * $Source: /home/gd4/gnu/cvsroot/inetutils/rsh/rsh.c,v $
 * $Header: /home/gd4/gnu/cvsroot/inetutils/rsh/rsh.c,v 1.11 1997/02/17 04:31:46 miles Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#include <sys/file.h>
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

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <getopt.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm;
extern char *krb_realmofhost();
#endif

#include "version.h"

/*
 * rsh - remote shell
 */
int	rfd2;

char   *copyargs __P((char **));
void	sendsig __P((int));
void	talk __P((int, sigset_t *, pid_t, int));
void	usage __P((void));
void	warning __P((const char *, ...));

/* basename (argv[0]).  NetBSD, linux, & gnu libc all define it.  */
extern char *__progname;

static struct option long_options[] =
{
  { "debug", no_argument, 0, 'd' },
  { "no-input", no_argument, 0, 'n' },
  { "user", required_argument, 0, 'l' },
  { "encrypt", no_argument, 0, 'x' },
  { "realm", required_argument, 0, 'k' },
  { "help", no_argument, 0, '&' },
  { "version", no_argument, 0, 'V' },
  { 0 }
};

static void 
pusage (stream)
FILE *stream;
{
  fprintf(stream,
	  "Usage: %s [-nd%s]%s[-l USER] [USER@]HOST [COMMAND [ARG...]]\n",
	  __progname,
#ifdef KERBEROS
#ifdef CRYPT
	    "x", " [-k REALM] ");
#else
	    "", " [-k REALM] ");
#endif
#else
	    "", " ");
#endif
}

/* Print a help message describing all options to STDOUT and exit with a
   status of 0.  */
static void
help ()
{
  pusage (stdout);
  puts ("Execute COMMAND on remote system HOST\n\n\
  -d, --debug                Turn on socket debugging");
#ifdef KERBEROS
  puts ("\
  -k REALM, --realm=REALM    Obtain tickets for the remote host in REALM\n\
                             instead of the remote host's realm");
#endif
  puts ("\
  -l USER, --user=USER       Run as USER on the remote system");
  puts ("\
  -n, --no-input             Use /dev/null as input");
#ifdef CRYPT
  puts ("\
  -x, --encrypt              Encrypt all data using DES");
#endif
  puts ("\
      --help                 Give this help list\n\
  -V, --version              Print program version");
  fprintf (stdout, "\nSubmit bug reports to %s.\n", inetutils_bugaddr);
  exit (0);
}

/* Print a message saying to use --help to STDERR, and exit with a status of
   1.  */
static void
try_help ()
{
  fprintf (stderr, "Try `%s --help' for more information.\n", __progname);
  exit (1);
}

void
usage()
{
  pusage (stderr);
  try_help ();
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	struct servent *sp;
	sigset_t sigs, osigs;
	int argoff, asrsh, ch, dflag, nflag, one, rem;
	pid_t pid;
	uid_t uid;
	char *args, *host, *p, *user;

#ifndef HAVE___PROGNAME
	extern char *__progname;
	__progname = argv[0];
#endif

	argoff = asrsh = dflag = nflag = 0;
	one = 1;
	host = user = NULL;

	/* if called as something other than "rsh", use it as the host name */
	if (p = strrchr(argv[0], '/'))
		++p;
	else
		p = argv[0];
	if (strcmp(p, "rsh"))
		host = p;
	else
		asrsh = 1;

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#ifdef KERBEROS
#ifdef CRYPT
#define	OPTIONS	"8KLdek:l:nwx"
#else
#define	OPTIONS	"8KLdek:l:nw"
#endif
#else
#define	OPTIONS	"8KLdel:nw"
#endif
	while ((ch = getopt_long (argc - argoff, argv + argoff, OPTIONS,
				  long_options, 0))
	       != EOF)
		switch(ch) {
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
		case 'L':	/* -8Lew are ignored to allow rlogin aliases */
		case 'e':
		case 'w':
		case '8':
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			user = optarg;
			break;
#ifdef KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'n':
			nflag = 1;
			break;
#ifdef KERBEROS
#ifdef CRYPT
		case 'x':
			doencrypt = 1;
			des_set_key(cred.session, schedule);
			break;
#endif
#endif

		case '&':
			help ();
		case 'V':
			printf ("rsh (%s) %s\n",
				inetutils_package, inetutils_version);
			exit (0);

		case '?':
			try_help ();

		default:
			usage();
		}
	optind += argoff;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	/* if no further arguments, must have been called as rlogin. */
	if (!argv[optind]) {
		if (asrsh)
			*argv = "rlogin";
		execv(PATH_RLOGIN, argv);
		err(1, "can't exec %s", PATH_RLOGIN);
	}

	argc -= optind;
	argv += optind;

	if (!(pw = getpwuid(uid = getuid())))
		errx(1, "unknown user id");
	/* Accept user1@host format, though "-l user2" overrides user1 */
	p = strchr(host, '@');
	if (p) {
		*p = '\0';
		if (!user && p > host)
			user = host;
		host = p + 1;
		if (*host == '\0')
			usage();
	}
	if (!user)
		user = pw->pw_name;

#ifdef KERBEROS
#ifdef CRYPT
	/* -x turns off -n */
	if (doencrypt)
		nflag = 0;
#endif
#endif

	args = copyargs(argv);

	sp = NULL;
#ifdef KERBEROS
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "ekshell" : "kshell"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "ekshell" : "kshell");
		}
	}
#endif
	if (sp == NULL)
		sp = getservbyname("shell", "tcp");
	if (sp == NULL)
		errx(1, "shell/tcp: unknown service");

#ifdef KERBEROS
try_connect:
	if (use_kerberos) {
		struct hostent *hp;

		/* fully qualify hostname (needed for krb_realmofhost) */
		hp = gethostbyname(host);
		if (hp != NULL && !(host = strdup(hp->h_name)))
			err(1, NULL);

		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

#ifdef CRYPT
		if (doencrypt)
			rem = krcmd_mutual(&host, sp->s_port, user, args,
			    &rfd2, dest_realm, &cred, schedule);
		else
#endif
			rem = krcmd(&host, sp->s_port, user, args, &rfd2,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("shell", "tcp");
			if (sp == NULL)
				errx(1, "shell/tcp: unknown service");
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
		if (doencrypt)
			errx(1, "the -x flag requires Kerberos authentication");
		rem = rcmd(&host, sp->s_port, pw->pw_name, user, args, &rfd2);
	}
#else
	rem = rcmd(&host, sp->s_port, pw->pw_name, user, args, &rfd2);
#endif

	if (rem < 0)
		exit(1);

	if (rfd2 < 0)
		errx(1, "can't establish stderr");
	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, (char *) &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, (char *) &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
	}

	(void)setuid(uid);
#ifdef HAVE_SIGACTION
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGQUIT);
	sigaddset(&sigs, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
#else	
	sigs = sigmask (SIGINT) | sigmask (SIGQUIT) | sigmask (SIGTERM);
	osigs = sigblock (sigs);
#endif
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)signal(SIGTERM, sendsig);

	if (!nflag) {
		pid = fork();
		if (pid < 0)
			err(1, "fork");
	}

#ifdef KERBEROS
#ifdef CRYPT
	if (!doencrypt)
#endif
#endif
	{
		(void)ioctl(rfd2, FIONBIO, &one);
		(void)ioctl(rem, FIONBIO, &one);
	}

	talk(nflag, &osigs, pid, rem);

	if (!nflag)
		(void)kill(pid, SIGKILL);
	exit(0);
}

void
talk (nflag, osigs, pid, rem)
	int nflag;
        sigset_t *osigs;
	pid_t pid;
	int rem;
{
	int cc, wc;
	fd_set readfrom, ready, rembits;
	char *bp, buf[BUFSIZ];

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(0, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

rewrite:	
		FD_ZERO(&rembits);
		FD_SET(rem, &rembits);
		if (select(16, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR)
				err(1, "select");
			goto rewrite;
		}
		if (!FD_ISSET(rem, &rembits))
			goto rewrite;
#ifdef KERBEROS
#ifdef CRYPT
		if (doencrypt)
			wc = des_write(rem, bp, cc);
		else
#endif
#endif
			wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		bp += wc;
		cc -= wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
done:
		(void)shutdown(rem, 1);
		exit(0);
	}

#ifdef HAVE_SIGACTION
	sigprocmask (SIG_SETMASK, osigs, 0);
#else
	sigsetmask (*osigs);
#endif
	FD_ZERO(&readfrom);
	FD_SET(rfd2, &readfrom);
	FD_SET(rem, &readfrom);
	do {
		ready = readfrom;
		if (select(16, &ready, 0, 0, 0) < 0) {
			if (errno != EINTR)
				err(1, "select");
			continue;
		}
		if (FD_ISSET(rfd2, &ready)) {
			errno = 0;
#ifdef KERBEROS
#ifdef CRYPT
			if (doencrypt)
				cc = des_read(rfd2, buf, sizeof buf);
			else
#endif
#endif
				cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rfd2, &readfrom);
			} else
				(void)write(2, buf, cc);
		}
		if (FD_ISSET(rem, &ready)) {
			errno = 0;
#ifdef KERBEROS
#ifdef CRYPT
			if (doencrypt)
				cc = des_read(rem, buf, sizeof buf);
			else
#endif
#endif
				cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rem, &readfrom);
			} else
				(void)write(1, buf, cc);
		}
	} while (FD_ISSET(rfd2, &readfrom) || FD_ISSET(rem, &readfrom));
}

void
sendsig(sig)
	int sig;
{
	char signo;

	signo = sig;
#ifdef KERBEROS
#ifdef CRYPT
	if (doencrypt)
		(void)des_write(rfd2, &signo, 1);
	else
#endif
#endif
		(void)write(rfd2, &signo, 1);
}

#ifdef KERBEROS
/* VARARGS */
void
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
warning(const char * fmt, ...)
#else
warning(va_alist)
va_dcl
#endif
{
	va_list ap;
#if !(defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__)
	const char *fmt;
#endif

	fprintf(stderr, "%s: warning, using standard rsh: ", __progname);
#if defined(HAVE_STDARG_H) && defined(__STDC__) && __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	fmt = va_arg(ap, char *);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

char *
copyargs(argv)
	char **argv;
{
	int cc;
	char **ap, *args, *p;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if (!(args = malloc((u_int)cc)))
		err(1, NULL);
	for (p = args, ap = argv; *ap; ++ap) {
		(void)strcpy(p, *ap);
		for (p = strcpy(p, *ap); *p; ++p);
		if (ap[1])
			*p++ = ' ';
	}
	return (args);
}
