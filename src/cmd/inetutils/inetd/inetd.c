/* Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This file is part of GNU Inetutils.

   GNU Inetutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Inetutils is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Inetutils; see the file COPYING.  If not, write
   to the Free Software Foundation, Inc., 51 Franklin Street,
   Fifth Floor, Boston, MA 02110-1301 USA. */

/*
 * Copyright (c) 1983, 1991, 1993, 1994
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

#if 0
static char sccsid[] = "@(#)inetd.c	8.4 (Berkeley) 4/13/94";
#endif

/*
 * Inetd - Internet super-server
 *  
 * This program invokes all internet services as needed.  Connection-oriented
 * services are invoked each time a connection is made, by creating a process.
 * This process is passed the connection as file descriptor 0 and is expected
 * to do a getpeername to find out the source host and port.
 *  
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *  
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *  
 *	service name			must be in /etc/services or must
 *					name a tcpmux service
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	arguments starting with argv[0]
 *  
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *  
 * Comment lines are indicated by a `#' in column 1.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
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
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <grp.h>

#define	TOOMANY		40		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

#ifndef SIGCHLD
#define SIGCHLD	SIGCLD
#endif
#define	SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM))

char *program_name;

int	debug = 0;
int	nsock, maxsock;
fd_set	allsock;
int	options;
int	timingout;
int	toomany = TOOMANY;

struct	servtab {
  char	*se_service;		/* name of service */
  int	se_socktype;		/* type of socket to use */
  char	*se_proto;		/* protocol used */
  pid_t	se_wait;		/* single threaded server */
  short	se_checked;		/* looked at during merge */
  char	*se_user;		/* user name to run as */
  struct biltin *se_bi;		/* if built-in, description */
  char	*se_server;		/* server program */
  char	**se_argv;	        /* program arguments */
  size_t se_argc;               /* number of arguments */
  int	se_fd;			/* open descriptor */
  int	se_type;		/* type */
  sa_family_t se_family;	/* address family of the socket */
  char	se_v4mapped;		/* 1 = accept v4mapped connection, 0 = don't */
#ifdef IPV6
  struct sockaddr_storage se_ctrladdr;/* bound address */
#else
  struct sockaddr_in se_ctrladdr;/* bound address */
#endif
  int	se_count;		/* number started since se_time */
  struct	timeval se_time;	/* start of se_count */
  struct	servtab *se_next;
} *servtab;

#define NORM_TYPE	0
#define MUX_TYPE	1
#define MUXPLUS_TYPE	2
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) || \
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)


void		chargen_dg (int, struct servtab *);
void		chargen_stream (int, struct servtab *);
void		close_sep (struct servtab *);
void		config (int);
void		daytime_dg (int, struct servtab *);
void		daytime_stream (int, struct servtab *);
void		discard_dg (int, struct servtab *);
void		discard_stream (int, struct servtab *);
void		echo_dg (int, struct servtab *);
void		echo_stream (int, struct servtab *);
void		endconfig (FILE *);
struct servtab *enter (struct servtab *);
void		freeconfig (struct servtab *);
struct servtab *getconfigent (FILE *, const char *, size_t *);
void		machtime_dg (int, struct servtab *);
void		machtime_stream (int, struct servtab *);
char	       *newstr (const char *);
char	       *nextline (FILE *);
void		nextconfig (const char *);
void		print_service (const char *, const char *, struct servtab *);
void		reapchild (int);
void		retry (int);
FILE           *setconfig (const char *);
void		setup (struct servtab *);
void		tcpmux (int s, struct servtab *sep);
void		set_proc_title (char *, int);
void		initring (void);
long		machtime (void);
void            run_service (int ctrl, struct servtab *sep);
void            prepenv (int ctrl, struct sockaddr_in sa_client);

struct biltin {
  const char	*bi_service;	/* internally provided service name */
  int	bi_socktype;		/* type of socket supported */
  short	bi_fork;		/* 1 if should fork before call */
  short	bi_wait;		/* 1 if should wait for child */
  void	(*bi_fn) (int s, struct servtab *); /*function which performs it */
} biltins[] = {
  /* Echo received data */
  { "echo",	SOCK_STREAM,	1, 0,	echo_stream },
  { "echo",	SOCK_DGRAM,	0, 0,	echo_dg },

  /* Internet /dev/null */
  { "discard",	SOCK_STREAM,	1, 0,	discard_stream },
  { "discard",	SOCK_DGRAM,	0, 0,	discard_dg },

  /* Return 32 bit time since 1900 */
  { "time",	SOCK_STREAM,	0, 0,	machtime_stream },
  { "time",	SOCK_DGRAM,	0, 0,	machtime_dg },

  /* Return human-readable time */
  { "daytime",	SOCK_STREAM,	0, 0,	daytime_stream },
  { "daytime",	SOCK_DGRAM,	0, 0,	daytime_dg },

  /* Familiar character generator */
  { "chargen",	SOCK_STREAM,	1, 0,	chargen_stream },
  { "chargen",	SOCK_DGRAM,	0, 0,	chargen_dg },

  { "tcpmux",	SOCK_STREAM,	1, 0,	tcpmux },

  { NULL, 0, 0, 0, NULL }
};

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	**Argv;
char 	*LastArg;

char **config_files;


/* Signal handling */

#if defined(HAVE_SIGACTION)
# define SIGSTATUS sigset_t
# define sigstatus_empty(s) sigemptyset(&s)
# define inetd_pause(s) sigsuspend(&s)
#else
# define SIGSTATUS long
# define sigstatus_empty(s) s = 0
# define inetd_pause(s) sigpause (s)
#endif

void
signal_set_handler (int signo, RETSIGTYPE (*handler) ())
{
#if defined(HAVE_SIGACTION)
  struct sigaction sa;
  memset ((char *)&sa, 0, sizeof(sa));
  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, signo);
#ifdef SA_RESTART
  sa.sa_flags = SA_RESTART;
#endif
  sa.sa_handler = handler;
  sigaction (signo, &sa, NULL);
#elif defined(HAVE_SIGVEC)
  struct sigvec sv;
  memset (&sv, 0, sizeof(sv));
  sv.sv_mask = SIGBLOCK;
  sv.sv_handler = handler;
  sigvec (signo, &sv, NULL);
#else /* !HAVE_SIGVEC */
  signal (signo, handler);
#endif /* HAVE_SIGACTION */
}

void
signal_block (SIGSTATUS *old_status)
{
#ifdef HAVE_SIGACTION
  sigset_t sigs;
  
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGCHLD);
  sigaddset (&sigs, SIGHUP);
  sigaddset (&sigs, SIGALRM);
  sigprocmask (SIG_BLOCK, &sigs, old_status);
#else
  long omask = sigblock (SIGBLOCK);
  if (old_status)
    *old_status = omask;
#endif
}

void
signal_unblock (SIGSTATUS *status)
{
#ifdef HAVE_SIGACTION
  if (status)
    sigprocmask (SIG_SETMASK, status, 0);
  else
    {
      sigset_t empty;
      sigemptyset (&empty);
      sigprocmask (SIG_SETMASK, &empty, 0);
    }
#else
  sigsetmask (status ? *status : 0);
#endif
}


static void
usage (int err)
{
  if (err != 0)
    {
      fprintf (stderr, "Usage: %s [OPTION...] [CONF-FILE [CONF-DIR]] ...\n",
	       program_name);
      fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
    }
  else
    {
      fprintf (stdout, "Usage: %s [OPTION...] [CONF-FILE [CONF-DIR]] ...\n",
	       program_name);
      puts ("Internet super-server.\n\n\
  -d, --debug               Debug mode\n\
      --environment         Pass local and remote socket information in\n\
                            environment variables\n\
  -R, --rate NUMBER         Maximum invocation rate (per second)\n\
      --resolve             Resolve IP addresses when setting environment\n\
                            variables (see --environment)\n\
      --help                Display this help and exit\n\
  -V, --version             Output version information and exit");

      fprintf (stdout, "\nSubmit bug reports to %s.\n", PACKAGE_BUGREPORT);
    }
  exit (err);
}

static int env_option;      /* Set environment variables */
static int resolve_option;  /* Resolve IP addresses */

static const char *short_options = "dR:V";
static struct option long_options[] =
{
  { "debug", no_argument, 0, 'd' },
  { "rate", required_argument, 0, 'R' },
  { "help", no_argument, 0, '&' },
  { "version", no_argument, 0, 'V' },
  { "resolve", no_argument, &resolve_option, 1 },
  { "environment", no_argument, &env_option, 1 },
  { 0, 0, 0, 0 }
};

int
main (int argc, char *argv[], char *envp[])
{
  int option;
  struct servtab *sep;
  int dofork;
  pid_t pid;

  program_name = argv[0];

  Argv = argv;
  if (envp == 0 || *envp == 0)
    envp = argv;
  while (*envp)
    envp++;
  LastArg = envp[-1] + strlen (envp[-1]);

  while ((option = getopt_long (argc, argv, short_options,
				long_options, 0)) != EOF)
    {
      switch (option)
	{
	case 'd': /* Debug.  */
	  debug = 1;
	  options |= SO_DEBUG;
	  break;

	case 'R': /* Invocation rate.  */
	  {
	    char *p;
	    int number;
	    number = strtol (optarg, &p, 0);
	    if (number < 1 || *p)
	      syslog (LOG_ERR,
		      "-R %s: bad value for service invocation rate",
		      optarg);
	    else
	      toomany = number;
	    break;
	  }

	case '&': /* Usage.  */
	  usage (0);
	  /* Not reached.  */

	case 'V': /* Version.  */
	  printf ("inetd (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	  exit (0);
	  /* Not reached.  */

	case 0:
	  break;
	  
	case '?':
	default:
	  usage (1);
	  /* Not reached.  */
	}
    }

  if (resolve_option)
    env_option = 1;
  
  if (optind < argc)
    {
      int i;
      config_files = calloc (argc - optind + 1, sizeof (*config_files));
      for (i = 0; optind < argc; optind++, i++)
	{
	  config_files[i] = strdup (argv[optind]);
	}
    }
  else
    {
      config_files = calloc (3, sizeof (*config_files));
      config_files[0] = newstr (PATH_INETDCONF);
      config_files[1] = newstr (PATH_INETDDIR);
    }

  if (debug == 0)
    {
      daemon (0, 0);
    }

  openlog ("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);

  {
    FILE *fp = fopen (PATH_INETDPID, "w");
    if (fp != NULL)
      {
        fprintf (fp, "%d\n", getpid ());
        fclose (fp);
      }
    else
      syslog (LOG_CRIT, "can't open %s: %s\n", PATH_INETDPID,
	      strerror (errno));
  }

  signal_set_handler (SIGALRM, retry);
  config (0);
  signal_set_handler (SIGHUP, config);
  signal_set_handler (SIGCHLD, reapchild);
  signal_set_handler (SIGPIPE, SIG_IGN);

  {
    /* space for daemons to overwrite environment for ps */
#define	DUMMYSIZE	100
    char dummy[DUMMYSIZE];

    memset(dummy, 'x', DUMMYSIZE - 1);
    dummy[DUMMYSIZE - 1] = '\0';
    setenv("inetd_dummy", dummy, 1);
  }

  for (;;)
    {
      int n, ctrl;
      fd_set readable;

      if (nsock == 0)
	{
	  SIGSTATUS stat;
	  sigstatus_empty (stat);
	  
	  signal_block (NULL);
	  while (nsock == 0)
	    inetd_pause (stat);
	  signal_unblock (NULL);
	}
      readable = allsock;
      if ((n = select (maxsock + 1, &readable, NULL, NULL, NULL)) <= 0)
	{
	  if (n < 0 && errno != EINTR)
	    syslog (LOG_WARNING, "select: %m");
	  sleep (1);
	  continue;
	}
      for (sep = servtab; n && sep; sep = sep->se_next)
	if (sep->se_fd != -1 && FD_ISSET (sep->se_fd, &readable))
	  {
	    n--;
	    if (debug)
	      fprintf (stderr, "someone wants %s\n", sep->se_service);
	    if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
	      {
		struct sockaddr_in sa_client;
		socklen_t len = sizeof (sa_client);
		
		ctrl = accept (sep->se_fd, (struct sockaddr *)&sa_client,
			       &len);
		if (debug)
		  fprintf (stderr, "accept, ctrl %d\n", ctrl);
		if (ctrl < 0)
		  {
		    if (errno != EINTR)
		      syslog (LOG_WARNING, "accept (for %s): %m",
			      sep->se_service);
		    continue;
		  }
		if (env_option)
		  prepenv (ctrl, sa_client);
	      }
	    else
	      ctrl = sep->se_fd;

	    signal_block (NULL);
	    pid = 0;
	    dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
	    if (dofork)
	      {
		if (sep->se_count++ == 0)
		  gettimeofday (&sep->se_time, NULL);
		else if (sep->se_count >= toomany)
		  {
		    struct timeval now;

		    gettimeofday (&now, NULL);
		    if (now.tv_sec - sep->se_time.tv_sec > CNT_INTVL)
		      {
			sep->se_time = now;
			sep->se_count = 1;
		      }
		    else
		      {
			syslog (LOG_ERR,
				"%s/%s server failing (looping), service terminated",
				sep->se_service, sep->se_proto);
			close_sep (sep);
			signal_unblock (NULL);
			if (!timingout)
			  {
			    timingout = 1;
			    alarm (RETRYTIME);
			  }
			continue;
		      }
		  }
		pid = fork ();
	      }
	    if (pid < 0)
	      {
		syslog (LOG_ERR, "fork: %m");
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
		  close (ctrl);
		signal_unblock (NULL);
		sleep (1);
		continue;
	      }
	    if (pid && sep->se_wait)
	      {
		sep->se_wait = pid;
		if (sep->se_fd >= 0)
		  {
		    FD_CLR (sep->se_fd, &allsock);
		    nsock--;
		  }
	      }
	    signal_unblock (NULL);
	    if (pid == 0)
	      {
		if (debug && dofork)
		  setsid ();
		if (dofork)
		  {
		    int sock;
		    if (debug)
		      fprintf (stderr, "+ Closing from %d\n", maxsock);
		    for (sock = maxsock; sock > 2; sock--)
		      if (sock != ctrl)
			close (sock);
		  }
		run_service (ctrl, sep);
	      }
	    if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
	      close (ctrl);
	  }
    }
}

void
run_service (int ctrl, struct servtab *sep)
{
  struct passwd *pwd;
  char buf[50];

  if (sep->se_bi)
    {
      (*sep->se_bi->bi_fn) (ctrl, sep);
    }
  else
    {
      if (debug)
	fprintf (stderr, "%d execl %s\n", getpid (), sep->se_server);
      dup2 (ctrl, 0);
      close (ctrl);
      dup2 (0, 1);
      dup2 (0, 2);
      if ((pwd = getpwnam (sep->se_user)) == NULL)
	{
	  syslog (LOG_ERR, "%s/%s: %s: No such user",
		  sep->se_service, sep->se_proto, sep->se_user);
	  if (sep->se_socktype != SOCK_STREAM)
	    recv (0, buf, sizeof buf, 0);
	  _exit (1);
	}
      if (pwd->pw_uid)
	{
	  if (setgid (pwd->pw_gid) < 0)
	    {
	      syslog (LOG_ERR, "%s: can't set gid %d: %m",
		      sep->se_service, pwd->pw_gid);
	      _exit (1);
	    }
#ifdef HAVE_INITGROUPS
	  initgroups (pwd->pw_name, pwd->pw_gid);
#endif
	  if (setuid (pwd->pw_uid) < 0)
	    {
	      syslog (LOG_ERR, "%s: can't set uid %d: %m",
		      sep->se_service, pwd->pw_uid);
	      _exit (1);
	    }
	}
      execv (sep->se_server, sep->se_argv);
      if (sep->se_socktype != SOCK_STREAM)
	recv (0, buf, sizeof buf, 0);
      syslog (LOG_ERR, "cannot execute %s: %m", sep->se_server);
      _exit (1);
    }
}

void
reapchild (int signo)
{
  int status;
  pid_t pid;
  struct servtab *sep;

  signo; /* shutup gcc */
  for (;;)
    {
#ifdef HAVE_WAIT3
      pid = wait3 (&status, WNOHANG, NULL);
#else
      pid = wait (&status);
#endif
      if (pid <= 0)
	break;
      if (debug)
	fprintf (stderr, "%d reaped, status %#x\n", pid, status);
      for (sep = servtab; sep; sep = sep->se_next)
	if (sep->se_wait == pid)
	  {
	    if (status)
	      syslog (LOG_WARNING, "%s: exit status 0x%x",
		      sep->se_server, status);
	    if (debug)
	      fprintf (stderr, "restored %s, fd %d\n",
		       sep->se_service, sep->se_fd);
	    FD_SET (sep->se_fd, &allsock);
	    nsock++;
	    sep->se_wait = 1;
	  }
    }
}

void
config (int signo)
{
  int i;
  struct stat stats;
  struct servtab *sep;

  signo; /* Shutup gcc.  */

  for (sep = servtab; sep; sep = sep->se_next)
    sep->se_checked = 0;

  for (i = 0; config_files[i]; i++)
    {
      struct stat statbuf;

      if (stat (config_files[i], &statbuf) == 0)
	{
	  if (S_ISDIR (statbuf.st_mode))
	    {
	      DIR *dirp = opendir (config_files[i]);

	      if (dirp)
		{
		  struct dirent *dp;

		  while ((dp = readdir (dirp)) != NULL)
		    {
		      char *path = calloc (strlen (config_files[i])
					   + strlen (dp->d_name) + 2, 1);
		      if (path)
			{
			  sprintf (path,"%s/%s", config_files[i], dp->d_name);
			  if (stat (path, &stats) == 0
			      && S_ISREG(stats.st_mode))
			    {
			      nextconfig (path);
			    }
			  free (path);
			}
		    }
		  closedir (dirp);
		}
	    }
	  else if (S_ISREG (statbuf.st_mode))
	    {
	      nextconfig (config_files[i]);
	    }
	}
      else
	{
	  if (signo == 0)
	    fprintf (stderr, "inetd: %s, %s\n",
		     config_files[i], strerror(errno));
	  else
	    syslog (LOG_ERR, "%s: %m", config_files[i]);
	}
    }
}

void
nextconfig (const char *file)
{
#ifndef IPV6
  struct servent *sp;
#endif
  struct servtab *sep, *cp, **sepp;
  struct passwd *pwd;
  FILE * fconfig;
  SIGSTATUS sigstatus;
  
  size_t line = 0;
  
  fconfig = setconfig (file);
  if (!fconfig)
    {
      syslog (LOG_ERR, "%s: %m", file);
      return;
    }
  while ((cp = getconfigent (fconfig, file, &line)))
    {
      if ((pwd = getpwnam (cp->se_user)) == NULL)
	{
	  syslog(LOG_ERR, "%s/%s: No such user '%s', service ignored",
		 cp->se_service, cp->se_proto, cp->se_user);
	  continue;
	}
      /* Checking/Removing duplicates */
      for (sep = servtab; sep; sep = sep->se_next)
	if (strcmp (sep->se_service, cp->se_service) == 0
	    && strcmp (sep->se_proto, cp->se_proto) == 0
	    && ISMUX(sep) == ISMUX (cp))
	  break;
      if (sep != 0)
	{
	  signal_block (&sigstatus);
	  /*
	   * sep->se_wait may be holding the pid of a daemon
	   * that we're waiting for.  If so, don't overwrite
	   * it unless the config file explicitly says don't
	   * wait.
	   */
	  if (cp->se_bi == 0
	      && (sep->se_wait == 1 || cp->se_wait == 0))
	    sep->se_wait = cp->se_wait;
#define SWAP(a, b) { char *c = a; a = b; b = c; }
	  if (cp->se_user)
	    SWAP(sep->se_user, cp->se_user);
	  if (cp->se_server)
	    SWAP(sep->se_server, cp->se_server);
	  argcv_free(sep->se_argc, sep->se_argv);
	  sep->se_argc = cp->se_argc;
	  sep->se_argv = cp->se_argv;
	  cp->se_argc = 0;
	  cp->se_argv = NULL;
	  signal_unblock (&sigstatus);
	  freeconfig (cp);
	  if (debug)
	    print_service (file, "REDO", sep);
	}
      else
	{
	  sep = enter (cp);
	  if (debug)
	    print_service (file, "ADD ", sep);
	}
      sep->se_checked = 1;
      if (ISMUX (sep))
	{
	  sep->se_fd = -1;
	  continue;
	}
#ifndef IPV6 /* FIXME: This code is moved to setup() for IPV6, which is
	        wrong */
      sp = getservbyname (sep->se_service, sep->se_proto);
      if (sp == 0)
	{
	  static struct servent servent;
	  char *p;
	  unsigned long val;
	  unsigned short port;

	  val = strtoul (sep->se_service, &p, 0);
	  if (*p || (port = val) != val)
	    {
	      syslog (LOG_ERR, "%s/%s: unknown service",
		      sep->se_service, sep->se_proto);
	      sep->se_checked = 0;
	      continue;
	    }
	  servent.s_port = htons (port);
	  sp = &servent;
	}
      if (sp->s_port != sep->se_ctrladdr.sin_port)
	{
	  sep->se_ctrladdr.sin_family = AF_INET;
	  sep->se_ctrladdr.sin_port = sp->s_port;
	  if (sep->se_fd >= 0)
	    close_sep (sep);
	}
#endif
      if (sep->se_fd == -1)
	setup (sep);
    }
  endconfig (fconfig);
  /*
   * Purge anything not looked at above.
   */
  signal_block (&sigstatus);
  sepp = &servtab;
  while ((sep = *sepp))
    {
      if (sep->se_checked)
	{
	  sepp = &sep->se_next;
	  continue;
	}
      *sepp = sep->se_next;
      if (sep->se_fd >= 0)
	close_sep (sep);
      if (debug)
	print_service (file, "FREE", sep);
      freeconfig (sep);
      free ((char *)sep);
    }
  signal_unblock (&sigstatus);
}

void
retry (int signo)
{
  struct servtab *sep;

  signo; /* shutup gcc */
  timingout = 0;
  for (sep = servtab; sep; sep = sep->se_next)
    if (sep->se_fd == -1 && !ISMUX (sep))
      setup (sep);
}

void
setup (struct servtab *sep)
{
  int err;
  const int on = 1;
#ifdef IPV6
  const int off = 0;
  struct addrinfo *result, hints;
  struct protoent *proto;

 tryagain:
#endif
  sep->se_fd = socket (sep->se_family, sep->se_socktype, 0);
  if (sep->se_fd < 0)
    {
#ifdef IPV6
      /* If we don't support creating AF_INET6 sockets, create AF_INET
	 sockets.  */
      if (errno == EAFNOSUPPORT && sep->se_family == AF_INET6 && sep->se_v4mapped)
	{
	  /* Fall back to IPv6 silently.  */
	  sep->se_family = AF_INET;
	  goto tryagain;
	}
#endif
      
      if (debug)
	fprintf (stderr, "socket failed on %s/%s: %s\n",
		 sep->se_service, sep->se_proto, strerror (errno));
      syslog(LOG_ERR, "%s/%s: socket: %m", sep->se_service, sep->se_proto);
      return;
    }

#ifdef IPV6
  /* Make sure that tcp6 etc also work.  */
  if (strncmp (sep->se_proto, "tcp", 3) == 0)
    proto = getprotobyname ("tcp");
  else if (strncmp (sep->se_proto, "udp", 3) == 0)
    proto = getprotobyname ("udp");
  else
    proto = getprotobyname (sep->se_proto);
   
  if (!proto)
    {
      syslog (LOG_ERR, "%s: Unknown protocol", sep->se_proto);
      close (sep->se_fd);
      sep->se_fd = -1;
      return;
    }
  
  memset (&hints, 0, sizeof (hints));

  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = sep->se_family;
  hints.ai_socktype = sep->se_socktype;
  hints.ai_protocol = proto->p_proto;

  err = getaddrinfo (NULL, sep->se_service, &hints, &result);
  if (err)
    {
      const char *errmsg;

      if (err == EAI_FAMILY && sep->se_family == AF_INET6 && sep->se_v4mapped)
	{
	  /* Fall back to IPv6 silently.  */
	  sep->se_family = AF_INET;
	  close (sep->se_fd);
	  goto tryagain;
	}

      if (err == EAI_SYSTEM)
	errmsg = strerror (errno);
      else
	errmsg = gai_strerror (err);
      
      syslog (LOG_ERR, "%s/%s: getaddrinfo: %s",
	      sep->se_service, sep->se_proto, errmsg);
      
      close (sep->se_fd);
      sep->se_fd = -1;
      return;
    }

  memcpy (&sep->se_ctrladdr, result->ai_addr, result->ai_addrlen);

  freeaddrinfo (result);

  if (sep->se_family == AF_INET6)
    {
      if (sep->se_v4mapped)
	err = setsockopt (sep->se_fd, IPPROTO_IPV6, IPV6_V6ONLY,
			  (char *)&off, sizeof(off));
      else
	err = setsockopt (sep->se_fd, IPPROTO_IPV6, IPV6_V6ONLY,
			  (char *)&on, sizeof(on));
      
      if (err < 0)
	syslog (LOG_ERR, "setsockopt (IPV6_V6ONLY): %m");
    }
#endif
  
  if (strncmp (sep->se_proto, "tcp", 3) == 0 && (options & SO_DEBUG))
    {
      err = setsockopt(sep->se_fd, SOL_SOCKET, SO_DEBUG,
		       (char *)&on, sizeof (on));
      if (err < 0)
	syslog (LOG_ERR, "setsockopt (SO_DEBUG): %m");
    }
  
  err = setsockopt(sep->se_fd, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&on, sizeof (on));
  if (err < 0)
    syslog (LOG_ERR, "setsockopt (SO_REUSEADDR): %m");

  err = bind (sep->se_fd, (struct sockaddr *)&sep->se_ctrladdr,
	      sizeof (sep->se_ctrladdr));
  if (err < 0)
    {
#ifdef IPV6
      /* If we can't bind with AF_INET6 try again with AF_INET.  */
      if ((errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT)
	  && sep->se_family == AF_INET6 && sep->se_v4mapped)
	{	
	  /* Fall back to IPv6 silently.  */
	  sep->se_family = AF_INET;
	  close (sep->se_fd);
	  goto tryagain;
	}
#endif
      if (debug)
	fprintf (stderr, "bind failed on %s/%s: %s\n",
		 sep->se_service, sep->se_proto, strerror (errno));
      syslog(LOG_ERR, "%s/%s: bind: %m", sep->se_service, sep->se_proto);
      close (sep->se_fd);
      sep->se_fd = -1;
      if (!timingout)
	{
	  timingout = 1;
	  alarm (RETRYTIME);
	}
      return;
    }
  if (sep->se_socktype == SOCK_STREAM)
    listen (sep->se_fd, 10);
  FD_SET (sep->se_fd, &allsock);
  nsock++;
  if (sep->se_fd > maxsock)
    maxsock = sep->se_fd;
  if (debug)
    {
      fprintf(stderr, "registered %s on %d\n", sep->se_server, sep->se_fd);
    }
}

/*
 * Finish with a service and its socket.
 */
void
close_sep (struct servtab *sep)
{
  if (sep->se_fd >= 0)
    {
      nsock--;
      FD_CLR (sep->se_fd, &allsock);
      close (sep->se_fd);
      sep->se_fd = -1;
    }
  sep->se_count = 0;
  /*
   * Don't keep the pid of this running deamon: when reapchild()
   * reaps this pid, it would erroneously increment nsock.
   */
  if (sep->se_wait > 1)
    sep->se_wait = 1;
}

struct servtab *
enter (struct servtab *cp)
{
  struct servtab *sep;
  SIGSTATUS sigstatus;
  
  sep = (struct servtab *)malloc (sizeof (*sep));
  if (sep == NULL)
    {
      syslog (LOG_ERR, "Out of memory.");
      exit (-1);
    }
  *sep = *cp;
  sep->se_fd = -1;
  signal_block (&sigstatus);
  sep->se_next = servtab;
  servtab = sep;
  signal_unblock (&sigstatus);
  return sep;
}

struct	servtab serv;
#ifdef LINE_MAX
char	line[LINE_MAX];
#else
char 	line[2048];
#endif

FILE *
setconfig (const char *file)
{
  return fopen (file, "r");
}

void
endconfig (FILE *fconfig)
{
  if (fconfig)
    {
      fclose (fconfig);
    }
}

#define INETD_SERVICE      0   /* service name */
#define INETD_SOCKET       1   /* socket type */
#define INETD_PROTOCOL     2   /* protocol */
#define INETD_WAIT         3   /* wait/nowait */
#define INETD_USER         4   /* user */
#define INETD_SERVER_PATH  5   /* server program path */ 
#define INETD_SERVER_ARGS  6   /* server program arguments */

#define INETD_FIELDS_MIN   6   /* Minimum number of fields in entry */

struct servtab *
getconfigent (FILE *fconfig, const char *file, size_t *line)
{
  struct servtab *sep = &serv;
  size_t argc = 0, i;
  char **argv = NULL;
  char *cp;
  static char TCPMUX_TOKEN[] = "tcpmux/";
#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)

  memset ((caddr_t)sep, 0, sizeof *sep);
  
  while (1)
    {
      argcv_free (argc, argv);
      freeconfig (sep);
      memset ((caddr_t)sep, 0, sizeof *sep);
      
      while ((cp = nextline (fconfig)) && (*cp == '#' || *cp == '\0'))
	++*line;
      ++*line;
      if (cp == NULL)
	return NULL;
      
      if (argcv_get (cp, "", &argc, &argv))
	continue;
      
      if (argc < INETD_FIELDS_MIN)
	{
	  syslog (LOG_ERR, "%s:%lu: not enough fields",
		  file, (unsigned long) *line);
	  continue;
	}

      if (strncmp (argv[INETD_SERVICE], TCPMUX_TOKEN, MUX_LEN) == 0)
	{
	  char *c = argv[INETD_SERVICE] + MUX_LEN;
	  if (*c == '+')
	    {
	      sep->se_type = MUXPLUS_TYPE;
	      c++;
	    }
	  else
	    sep->se_type = MUX_TYPE;
	  sep->se_service = newstr (c);
	}
      else
	{
	  sep->se_service = newstr (argv[INETD_SERVICE]);
	  sep->se_type = NORM_TYPE;
	}
      
      if (strcmp (argv[INETD_SOCKET], "stream") == 0)
	sep->se_socktype = SOCK_STREAM;
      else if (strcmp (argv[INETD_SOCKET], "dgram") == 0)
	sep->se_socktype = SOCK_DGRAM;
      else if (strcmp (argv[INETD_SOCKET], "rdm") == 0)
	sep->se_socktype = SOCK_RDM;
      else if (strcmp (argv[INETD_SOCKET], "seqpacket") == 0)
	sep->se_socktype = SOCK_SEQPACKET;
      else if (strcmp (argv[INETD_SOCKET], "raw") == 0)
	sep->se_socktype = SOCK_RAW;
      else
	{
	  syslog (LOG_WARNING, "%s:%lu: bad socket type",
		  file, (unsigned long) *line);
	  sep->se_socktype = -1;
	}
      
      sep->se_proto = newstr (argv[INETD_PROTOCOL]);

#ifdef IPV6
      /* We default to IPv6, in setup() we'll fall back to IPv4 if
	 it doesn't work.  */
      sep->se_family = AF_INET6;
      sep->se_v4mapped = 1;
      
      if ((strncmp (sep->se_proto, "tcp", 3) == 0)
	  || (strncmp (sep->se_proto, "udp", 3) == 0))
	{
	  if (sep->se_proto[3] == '6')
	    {
	      sep->se_family = AF_INET6;
	      sep->se_v4mapped = 0;
	    }
	  else if (sep->se_proto[3] == '4')
	    {
	      sep->se_family = AF_INET;
	    }
	}
#else
      if ((strncmp (sep->se_proto, "tcp6", 4) == 0)
	  || (strncmp (sep->se_proto, "udp6", 4) == 0))
	{
	  syslog (LOG_ERR, "%s:%lu: %s: IPv6 support isn't eneabled",
		  file, (unsigned long) *line, sep->se_proto);
	  continue;
	}
      
      sep->se_family = AF_INET;
#endif
      
      if (strcmp (argv[INETD_WAIT], "wait") == 0)
	sep->se_wait = 1;
      else if (strcmp (argv[INETD_WAIT], "nowait") == 0)
	sep->se_wait = 0;
      else
	{
	  syslog (LOG_WARNING, "%s:%lu: bad wait type",
		  file, (unsigned long) *line);
	}
  
      if (ISMUX (sep))
	{
	  /*
	   * Silently enforce "nowait" for TCPMUX services since
	   * they don't have an assigned port to listen on.
	   */
	  sep->se_wait = 0;
	  
	  if (strncmp (sep->se_proto, "tcp", 3))
	    {
	      syslog (LOG_ERR, "%s:%lu: bad protocol for tcpmux service %s",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	  if (sep->se_socktype != SOCK_STREAM)
	    {
	      syslog (LOG_ERR,
		      "%s:%lu: bad socket type for tcpmux service %s",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	}
      
      sep->se_user = newstr (argv[INETD_USER]);
      sep->se_server = newstr (argv[INETD_SERVER_PATH]);
      if (strcmp (sep->se_server, "internal") == 0)
	{
	  struct biltin *bi;

	  for (bi = biltins; bi->bi_service; bi++)
	    if (bi->bi_socktype == sep->se_socktype
		&& strcmp (bi->bi_service, sep->se_service) == 0)
	      break;
	  if (bi->bi_service == 0)
	    {
	      syslog (LOG_ERR, "%s:%lu: internal service %s unknown",
		      file, (unsigned long) *line, sep->se_service);
	      continue;
	    }
	  sep->se_bi = bi;
	  sep->se_wait = bi->bi_wait;
	} else
	  sep->se_bi = NULL;

      sep->se_argc = argc - INETD_FIELDS_MIN + 1;
      sep->se_argv = calloc (sep->se_argc + 1, sizeof sep->se_argv[0]);
      if (!sep->se_argv)
	{
	  syslog (LOG_ERR, "%s:%lu: Out of memory.",
		  file, (unsigned long) *line);
	  exit (-1);
	}
      
      for (i = 0; i < sep->se_argc; i++)
	{
	  sep->se_argv[i] = argv[INETD_SERVER_ARGS + i];
	  argv[INETD_SERVER_ARGS + i] = 0;
	}
      sep->se_argv[i] = NULL;
      break;
    }
  argcv_free (argc, argv);
  return sep;
}

void
freeconfig (struct servtab *cp)
{
  if (cp->se_service)
    free (cp->se_service);
  if (cp->se_proto)
    free (cp->se_proto);
  if (cp->se_user)
    free (cp->se_user);
  if (cp->se_server)
    free (cp->se_server);
  argcv_free(cp->se_argc, cp->se_argv);
}

char *
nextline (FILE *fd)
{
  char *cp;

  if (fgets (line, sizeof line, fd) == NULL)
    return NULL;
  cp = strchr (line, '\n');
  if (cp)
    *cp = '\0';
  return line;
}

char *
newstr (const char *cp)
{
  char *s;
  if ((s = strdup (cp ? cp : "")))
    return s;
  syslog (LOG_ERR, "strdup: %m");
  exit (-1);
}

void
set_proc_title (char *a, int s)
{
  int size;
  char *cp;
#ifdef IPV6
  struct sockaddr_storage saddr;
#else
  struct sockaddr_in saddr;
#endif
  char buf[80];

  cp = Argv[0];
  size = sizeof saddr;
  if (getpeername (s, (struct sockaddr *)&saddr, &size) == 0)
    {
#ifdef IPV6
      int err;
      char buf2[80];

      err = getnameinfo ((struct sockaddr *) &saddr, sizeof (saddr), buf2,
			 sizeof (buf2), NULL, 0, NI_NUMERICHOST);
      if (!err)
	snprintf (buf, sizeof buf, "-%s [%s]", a, buf2);
      else
	snprintf (buf, sizeof buf, "-%s", a);
#else
      snprintf (buf, sizeof buf, "-%s [%s]", a, inet_ntoa (saddr.sin_addr));
#endif
    }
  else
    snprintf (buf, sizeof buf, "-%s", a);
  strncpy (cp, buf, LastArg - cp);
  cp += strlen (cp);
  while (cp < LastArg)
    *cp++ = ' ';
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	8192

/* ARGSUSED */
/* Echo service -- echo data back */
void
echo_stream (int s, struct servtab *sep)
{
  char buffer[BUFSIZE];
  int i;

  set_proc_title (sep->se_service, s);
  while ((i = read (s, buffer, sizeof buffer)) > 0
	 && write (s, buffer, i) > 0)
    ;
  exit (0);
}

/* ARGSUSED */
/* Echo service -- echo data back */
void
echo_dg (int s, struct servtab *sep)
{
  char buffer[BUFSIZE];
  int i, size;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif

  sep;
  size = sizeof sa;
  i = recvfrom (s, buffer, sizeof buffer, 0, (struct sockaddr *)&sa, &size);
  if (i < 0)
    return;
  sendto (s, buffer, i, 0, (struct sockaddr *) &sa, sizeof sa);
}

/* ARGSUSED */
/* Discard service -- ignore data */
void
discard_stream (int s, struct servtab *sep)
{
  int ret;
  char buffer[BUFSIZE];

  set_proc_title (sep->se_service, s);
  while (1)
    {
      while ((ret = read (s, buffer, sizeof buffer)) > 0)
	;
      if (ret == 0 || errno != EINTR)
	break;
    }
  exit (0);
}

/* ARGSUSED */
void
/* Discard service -- ignore data */
discard_dg (int s, struct servtab *sep)
{
  char buffer[BUFSIZE];
  sep; /* shutup gcc */
  read (s, buffer, sizeof buffer);
}

#include <ctype.h>
#define LINESIZ 72
char ring[128];
char *endring;

void
initring (void)
{
  int i;

  endring = ring;

  for (i = 0; i <= 128; ++i)
    if (isprint (i))
      *endring++ = i;
}

/* ARGSUSED */
/* Character generator */
void
chargen_stream (int s, struct servtab *sep)
{
  int len;
  char *rs, text[LINESIZ+2];

  set_proc_title (sep->se_service, s);

  if (!endring)
    {
      initring ();
      rs = ring;
    }

  text[LINESIZ] = '\r';
  text[LINESIZ + 1] = '\n';
  for (rs = ring;;)
    {
      if ((len = endring - rs) >= LINESIZ)
	memmove (text, rs, LINESIZ);
      else
	{
	  memmove (text, rs, len);
	  memmove (text + len, ring, LINESIZ - len);
	}
      if (++rs == endring)
	rs = ring;
      if (write (s, text, sizeof text) != sizeof text)
	break;
	}
  exit (0);
}

/* ARGSUSED */
/* Character generator */
void
chargen_dg (int s, struct servtab *sep)
{
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  static char *rs;
  int len, size;
  char text[LINESIZ+2];

  sep; /* shutup gcc */
  if (endring == 0)
    {
      initring ();
      rs = ring;
    }

  size = sizeof sa;
  if (recvfrom (s, text, sizeof text, 0, (struct sockaddr *)&sa, &size) < 0)
    return;

  if ((len = endring - rs) >= LINESIZ)
    memmove (text, rs, LINESIZ);
  else
    {
      memmove (text, rs, len);
      memmove (text + len, ring, LINESIZ - len);
    }
  if (++rs == endring)
    rs = ring;
  text[LINESIZ] = '\r';
  text[LINESIZ + 1] = '\n';
  sendto (s, text, sizeof text, 0, (struct sockaddr *)&sa, sizeof sa);
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

long
machtime (void)
{
  struct timeval tv;

  if (gettimeofday (&tv, NULL) < 0)
    {
      if (debug)
	fprintf (stderr, "Unable to get time of day\n");
      return 0L;
    }
#define	OFFSET ((u_long)25567 * 24*60*60)
  return (htonl ((long)(tv.tv_sec + OFFSET)));
#undef OFFSET
}

/* ARGSUSED */
void
machtime_stream (int s, struct servtab *sep)
{
  long result;

  sep; /* shutup gcc */
  result = machtime ();
  write (s, (char *) &result, sizeof result);
}

/* ARGSUSED */
void
machtime_dg (int s, struct servtab *sep)
{
  long result;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  int size;

  sep; /* shutup gcc */
  size = sizeof sa;
  if (recvfrom (s, (char *)&result, sizeof result, 0, 
		(struct sockaddr *)&sa, &size) < 0)
    return;
  result = machtime ();
  sendto (s, (char *) &result, sizeof result, 0, 
	  (struct sockaddr *)&sa, sizeof sa);
}

/* ARGSUSED */
void
/* Return human-readable time of day */
daytime_stream (int s, struct servtab *sep)
{
  char buffer[256];
  time_t lclock;

  sep; /*shutup gcc*/
  lclock = time ((time_t *) 0);

  sprintf (buffer, "%.24s\r\n", ctime(&lclock));
  write (s, buffer, strlen(buffer));
}

/* ARGSUSED */
/* Return human-readable time of day */
void
daytime_dg(int s, struct servtab *sep)
{
  char buffer[256];
  time_t lclock;
#ifdef IPV6
  struct sockaddr_storage sa;
#else
  struct sockaddr sa;
#endif
  int size;

  sep; /* shutup gcc */
  lclock = time ((time_t *) 0);

  size = sizeof sa;
  if (recvfrom (s, buffer, sizeof buffer, 0, (struct sockaddr *)&sa, &size) < 0)
    return;
  sprintf (buffer, "%.24s\r\n", ctime (&lclock));
  sendto (s, buffer, strlen(buffer), 0, (struct sockaddr *)&sa, sizeof sa);
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
void
print_service (const char *file, const char *action, struct servtab *sep)
{
  fprintf (stderr,
	   "%s:%s: %s proto=%s, wait=%d, user=%s builtin=%lx server=%s\n",
	   file, action, sep->se_service, sep->se_proto,
	   sep->se_wait, sep->se_user, (long)sep->se_bi, sep->se_server);
}

/*
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */


/* # of characters upto \r,\n or \0 */
static int
fd_getline (int fd, char *buf, int len)
{
  int count = 0, n;

  do {
    n = read (fd, buf, len-count);
    if (n == 0)
      return count;
    if (n < 0)
      return -1;
    while (--n >= 0)
      {
	if (*buf == '\r' || *buf == '\n' || *buf == '\0')
	  return count;
	count++;
	buf++;
      }
  } while (count < len);
  return count;
}

#define MAX_SERV_LEN	(256+2)		/* 2 bytes for \r\n */

#define strwrite(fd, buf)	write(fd, buf, sizeof(buf)-1)

void
tcpmux(int s, struct servtab *sep)
{
  char service[MAX_SERV_LEN+1];
  int len;

  /* Get requested service name */
  if ((len = fd_getline (s, service, MAX_SERV_LEN)) < 0)
    {
      strwrite (s, "-Error reading service name\r\n");
      _exit (1);
    }
  service[len] = '\0';

  if (debug)
    fprintf (stderr, "tcpmux: someone wants %s\n", service);

  /*
   * Help is a required command, and lists available services,
   * one per line.
   */
  if (!strcasecmp (service, "help"))
    {
      for (sep = servtab; sep; sep = sep->se_next)
	{
	  if (!ISMUX (sep))
	    continue;
	  write (s, sep->se_service, strlen (sep->se_service));
	  strwrite (s, "\r\n");
	}
      _exit (1);
    }

  /* Try matching a service in inetd.conf with the request */
  for (sep = servtab; sep; sep = sep->se_next)
    {
      if (!ISMUX (sep))
	continue;
      if (!strcasecmp (service, sep->se_service))
	{
	  if (ISMUXPLUS (sep))
	    {
	      strwrite (s, "+Go\r\n");
	    }
	  run_service (s, sep);
	  return;
	}
    }
  strwrite (s, "-Service not available\r\n");
  exit (1);
}

/* Set TCP environment variables, modelled after djb's ucspi-tcp tools:
   http://cr.yp.to/ucspi-tcp/environment.html
   FIXME: This needs support for IPv6
*/
void
prepenv (int ctrl, struct sockaddr_in sa_client)
{
  char str[16];
  char *ip;
  struct hostent *host;
  struct sockaddr_in sa_server;
  socklen_t len = sizeof (sa_server);

  setenv ("PROTO", "TCP", 1);
  unsetenv ("TCPLOCALIP");
  unsetenv ("TCPLOCALHOST");
  unsetenv ("TCPLOCALPORT");
  unsetenv ("TCPREMOTEIP");
  unsetenv ("TCPREMOTEPORT");
  unsetenv ("TCPREMOTEHOST");
  
  if (getsockname (ctrl, (struct sockaddr*)&sa_server, &len) < 0)
    syslog (LOG_WARNING, "getsockname(): %m");
  else
    {
      ip = inet_ntoa (sa_server.sin_addr);
      if (ip)
	{
	  if (setenv ("TCPLOCALIP", ip, 1) < 0)
	    syslog (LOG_WARNING, "setenv (TCPLOCALIP): %m");
	}

      snprintf (str, sizeof (str), "%d", ntohs (sa_server.sin_port));
      setenv ("TCPLOCALPORT", str, 1);

      if (resolve_option)
	{
	  if ((host = gethostbyaddr((char *) &sa_server.sin_addr,
				    sizeof (sa_server.sin_addr),
				    AF_INET)) == NULL)
	    syslog (LOG_WARNING, "gethostbyaddr: %m");
	  else if (setenv ("TCPLOCALHOST", host->h_name, 1) < 0)
	    syslog (LOG_WARNING, "setenv(TCPLOCALHOST): %m");
	}
    }

  ip = inet_ntoa (sa_client.sin_addr);
  if (ip)
    {
      if (setenv ("TCPREMOTEIP", ip, 1) < 0)
	syslog (LOG_WARNING, "setenv(TCPREMOTEIP): %m");
    }

  snprintf (str, sizeof (str), "%d", ntohs (sa_client.sin_port));
  if (setenv ("TCPREMOTEPORT", str, 1) < 0)
    syslog (LOG_WARNING, "setenv(TCPREMOTEPORT): %m");

  if (resolve_option)
    {
      if ((host = gethostbyaddr ((char *) &sa_client.sin_addr,
				 sizeof (sa_client.sin_addr),
				 AF_INET)) == NULL)
	syslog (LOG_WARNING, "gethostbyaddr: %m");
      else if (setenv ("TCPREMOTEHOST", host->h_name, 1) < 0)
	syslog (LOG_WARNING, "setenv(TCPREMOTEHOST): %m");
    }
}
