/*
 * Copyright (c) 1988, 1990, 1993
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
"@(#) Copyright (c) 1988, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.3 (Berkeley) 5/30/95";
#endif /* not lint */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

#include <getopt.h>

#include "ring.h"
#include "externs.h"
#include "defines.h"

#include "version.h"

/* These values need to be the same as defined in libtelnet/kerberos5.c */
/* Either define them in both places, or put in some common header file. */
#define OPTS_FORWARD_CREDS           0x00000002
#define OPTS_FORWARDABLE_CREDS       0x00000001

#if 0
#define FORWARD
#endif

/*
 * Initialize variables.
 */
    void
tninit()
{
    init_terminal();

    init_network();
    
    init_telnet();

    init_sys();

#if defined(TN3270)
    init_3270();
#endif
}

#define USAGE "Usage: %s [OPTION...] [HOST [PORT]]\n"

/* Print a help message describing all options to STDOUT and exit with a
   status of 0.  */
static void
help ()
{
  fprintf (stdout, USAGE, prompt);

  puts ("Login to remote system HOST (optionally, on service port PORT)\n\n\
  -8, --binary               Use an 8-bit data path\n\
  -a, --login                Attempt automatic login\n\
  -c, --no-rc                Don't read the user's .telnetrc file\n\
  -d, --debug                Turn on debugging\n\
  -e CHAR, --escape=CHAR     Use CHAR as an escape character\n\
  -E, --no-escape            Use no escape character\n\
  -K, --no-login             Don't automatically login to the remote system\n\
  -l USER, --user=USER       Attempt automatic login as USER\n\
  -L, --binary-output        Use an 8-bit data path for output only\n\
  -n FILE, --trace=FILE      Record trace information into FILE\n\
  -r, --rlogin               Use a user-interface similar to rlogin\n\
  -S TOS, --tos=TOS          Use the IP type-of-service TOS\n\
  -X ATYPE, --disable-auth=ATYPE   Disable type ATYPE authentication");

#ifdef ENCRYPTION
  puts ("\
  -x, --encrypt              Encrypt the data stream, if possible");
#endif

#ifdef AUTHENTICATION
  puts ("\n\
 When using Kerberos authentication:\n\
  -f, --fwd-credentials      Allow the the local credentials to be forwarded\n\
  -k REALM, --realm=REALM    Obtain tickets for the remote host in REALM\n\
                             instead of the remote host's realm");
#endif

#if defined(TN3270) && defined(unix)
  puts ("\n\
 TN3270 options (note non-standard option syntax):\n\
      -noasynch\n\
      -noasynctty\n\
      -noasyncnet\n\
  -t LINE, --transcom=LINE");
#endif

#if defined (ENCRYPTION) || defined (AUTHENTICATION) || defined (TN3270)
  putc ('\n');
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
  fprintf (stderr, "Try `%s --help' for more information.\n", prompt);
  exit (1);
}

/* Print a usage message to STDERR and exit with a status of 1.  */
static void
usage ()
{
  fprintf (stderr, USAGE, prompt);
  try_help ();
}

#if 0
static struct option long_options[] =
{
  { "binary", no_argument, 0, '8' },
  { "login", no_argument, 0, 'a' },
  { "no-rc", no_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "escape", required_argument, 0, 'e' },
  { "no-escape", no_argument, 0, 'E' },
  { "no-login", no_argument, 0, 'K' },
  { "user", required_argument, 0, 'l' },
  { "binary-output", no_argument, 0, 'L' },
  { "trace", required_argument, 0, 'n' },
  { "rlogin", no_argument, 0, 'r' },
  { "tos", required_argument, 0, 'S' },
  { "disable-auth", required_argument, 0, 'X' },
  { "encrypt", no_argument, 0, 'x' },
  { "fwd-credentials", no_argument, 0, 'f' },
  { "realm", required_argument, 0, 'k' },
  { "transcom", required_argument, 0, 't' },
  { "help", no_argument, 0, '&' },
  { "version", no_argument, 0, 'V' },
  { 0 }
};
#endif

/*
 * main.  Parse arguments, invoke the protocol or command parser.
 */
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;
	char *user;
#ifndef strrchr
	char *strrchr();
#endif
#ifdef	FORWARD
	extern int forward_flags;
#endif	/* FORWARD */

	tninit();		/* Clear out things */
#if	defined(CRAY) && !defined(__STDC__)
	_setlist_init();	/* Work around compiler bug */
#endif

	TerminalSaveState();

	if (prompt = strrchr(argv[0], '/'))
		++prompt;
	else
		prompt = argv[0];

	user = NULL;

	rlogin = (strncmp(prompt, "rlog", 4) == 0) ? '~' : _POSIX_VDISABLE;
	autologin = -1;

#if 0
	while ((ch = getopt_long (argc, argv, "8EKLS:X:acde:fFk:l:n:rt:x",
				  long_options, 0))
	       != EOF)
#else
	while ((ch = getopt (argc, argv, "8EKLS:X:acde:fFk:l:n:rt:x")) != EOF)
#endif
	{
		switch(ch) {
		case '8':
			eight = 3;	/* binary output and input */
			break;
		case 'E':
			rlogin = escape = _POSIX_VDISABLE;
			break;
		case 'K':
#ifdef	AUTHENTICATION
			autologin = 0;
#endif
			break;
		case 'L':
			eight |= 2;	/* binary output only */
			break;
		case 'S':
		    {
#ifdef	HAS_GETTOS
			extern int tos;

			if ((tos = parsetos(optarg, "tcp")) < 0)
				fprintf(stderr, "%s%s%s%s\n",
					prompt, ": Bad TOS argument '",
					optarg,
					"; will try to use default TOS");
#else
			fprintf(stderr,
			   "%s: Warning: -S ignored, no parsetos() support.\n",
								prompt);
#endif
		    }
			break;
		case 'X':
#ifdef	AUTHENTICATION
			auth_disable_name(optarg);
#endif
			break;
		case 'a':
			autologin = 1;
			break;
		case 'c':
			skiprc = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'e':
			set_escape_char(optarg);
			break;
		case 'f':
#if defined(AUTHENTICATION) && defined(KRB5) && defined(FORWARD)
			if (forward_flags & OPTS_FORWARD_CREDS) {
			    fprintf(stderr, 
				    "%s: Only one of -f and -F allowed.\n",
				    prompt);
			    help (0);
			}
			forward_flags |= OPTS_FORWARD_CREDS;
#else
			fprintf(stderr,
			 "%s: Warning: -f ignored, no Kerberos V5 support.\n", 
				prompt);
#endif
			break;
		case 'F':
#if defined(AUTHENTICATION) && defined(KRB5) && defined(FORWARD)
			if (forward_flags & OPTS_FORWARD_CREDS) {
			    fprintf(stderr, 
				    "%s: Only one of -f and -F allowed.\n",
				    prompt);
			    help (0);
			}
			forward_flags |= OPTS_FORWARD_CREDS;
			forward_flags |= OPTS_FORWARDABLE_CREDS;
#else
			fprintf(stderr,
			 "%s: Warning: -F ignored, no Kerberos V5 support.\n", 
				prompt);
#endif
			break;
		case 'k':
#if defined(AUTHENTICATION) && defined(KRB4)
		    {
			extern char *dest_realm, dst_realm_buf[], dst_realm_sz;
			dest_realm = dst_realm_buf;
			(void)strncpy(dest_realm, optarg, dst_realm_sz);
		    }
#else
			fprintf(stderr,
			   "%s: Warning: -k ignored, no Kerberos V4 support.\n",
								prompt);
#endif
			break;
		case 'l':
			autologin = 1;
			user = optarg;
			break;
		case 'n':
#if defined(TN3270) && defined(unix)
			/* distinguish between "-n oasynch" and "-noasynch" */
			if (argv[optind - 1][0] == '-' && argv[optind - 1][1]
			    == 'n' && argv[optind - 1][2] == 'o') {
				if (!strcmp(optarg, "oasynch")) {
					noasynchtty = 1;
					noasynchnet = 1;
				} else if (!strcmp(optarg, "oasynchtty"))
					noasynchtty = 1;
				else if (!strcmp(optarg, "oasynchnet"))
					noasynchnet = 1;
			} else
#endif	/* defined(TN3270) && defined(unix) */
				SetNetTrace(optarg);
			break;
		case 'r':
			rlogin = '~';
			break;
		case 't':
#if defined(TN3270) && defined(unix)
			transcom = tline;
			(void)strcpy(transcom, optarg);
#else
			fprintf(stderr,
			   "%s: Warning: -t ignored, no TN3270 support.\n",
								prompt);
#endif
			break;
		case 'x':
#ifdef	ENCRYPTION
			encrypt_auto(1);
			decrypt_auto(1);
#else	/* ENCRYPTION */
			fprintf(stderr,
			    "%s: Warning: -x ignored, no ENCRYPT support.\n",
								prompt);
#endif	/* ENCRYPTION */
			break;

		case '&':
			help ();
		case 'V':
			printf ("telnet (%s) %s\n",
				inetutils_package, inetutils_version);
			exit (0);

		case '?':
			try_help ();

		default:
			usage ();
			/* NOTREACHED */
		}
	}
	if (autologin == -1)
		autologin = (rlogin == _POSIX_VDISABLE) ? 0 : 1;

	argc -= optind;
	argv += optind;

	if (argc) {
		char *args[7], **argp = args;

		if (argc > 2)
			usage ();
		*argp++ = prompt;
		if (user) {
			*argp++ = "-l";
			*argp++ = user;
		}
		*argp++ = argv[0];		/* host */
		if (argc > 1)
			*argp++ = argv[1];	/* port */
		*argp = 0;

		if (setjmp(toplevel) != 0)
			Exit(0);
		if (tn(argp - args, args) == 1)
			return (0);
		else
			return (1);
	}
	(void)setjmp(toplevel);
	for (;;) {
#ifdef TN3270
		if (shell_active)
			shell_continue();
		else
#endif
			command(1, 0, 0);
	}
}
