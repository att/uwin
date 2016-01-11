/* A version of bsd `logwtmp' that should be widely portable

   Copyright (C) 1996 Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.ai.mit.edu>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* If `KEEP_OPEN' is defined, then a special version of logwtmp is compiled,
   called logwtmp_keep_open, which keeps the wtmp file descriptor open
   between calls, and doesn't attempt to open the file after the first call. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <sys/types.h>
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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UTMP_H
#include <utmp.h>
#else
#ifdef  HAVE_UTMPX_H
#include <utmpx.h>
#define utmp utmpx		/* make utmpx look more like utmp */
#endif
#endif

#ifndef HAVE_ERRNO_DECL
extern int errno;
#endif

static void
_logwtmp (ut)
     struct utmp *ut;
{
#ifdef KEEP_OPEN
  static int fd = -1;

  if (fd < 0)
    fd = open (PATH_WTMP, O_WRONLY|O_APPEND, 0);
#else
  int fd = open (PATH_WTMP, O_WRONLY|O_APPEND, 0);
#endif

  if (fd >= 0)
    {
      struct stat st;

#ifdef HAVE_FLOCK
      if (flock (fd, LOCK_EX|LOCK_NB) < 0 && errno != ENOSYS)
	{
	  sleep (1);
	  flock (fd, LOCK_EX|LOCK_NB); /* ignore error */
	}
#endif

#ifdef HAVE_FTRUNCATE
      if (fstat (fd, &st) == 0
	  && write (fd, (char *)ut, sizeof *ut) != sizeof *ut)
	ftruncate (fd, st.st_size);
#else
      write (fd, (char *)ut, sizeof *ut);
#endif

#ifdef HAVE_FLOCK
      flock (fd, LOCK_UN);
#endif

#ifndef KEEP_OPEN
      close (fd);
#endif
    }
}

void
#ifdef KEEP_OPEN
logwtmp_keep_open (line, name, host)
#else
logwtmp (line, name, host)
#endif
     char *line, *name, *host;
{
  struct utmp ut;

  /* Set information in new entry.  */
  bzero (&ut, sizeof (ut));
#ifdef HAVE_UTMP_UT_TYPE
  ut.ut_type = USER_PROCESS;
#endif
  strncpy (ut.ut_line, line, sizeof ut.ut_line);
  strncpy (ut.ut_name, name, sizeof ut.ut_name);
#ifdef HAVE_UTMP_UT_HOST
  strncpy (ut.ut_host, host, sizeof ut.ut_host);
#endif

#ifdef HAVE_UTMP_UT_TV
  gettimeofday (&ut.ut_tv, NULL);
#else
  time (&ut.ut_time);
#endif

  _logwtmp (&ut);
}
