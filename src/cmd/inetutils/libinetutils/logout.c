/* A version of bsd `logout' that should be widely portable

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
#include <utmp.h>
#include <fcntl.h>

int
logout (line)
  char *line;
{
  int success = 0;

#ifdef HAVE_SETUTENT		/* sysvish version */
  struct utmp *ut, search;

#ifdef HAVE_UTMPNAME
  if (! utmpname (PATH_UTMP))
    return 0;
#endif

  setutent ();

#ifdef HAVE_UTMP_UT_TYPE
  search.ut_type = USER_PROCESS;
#endif
  strncpy (search.ut_line, line, sizeof search.ut_line);

  ut = getutline (&search);
  if (ut)
    {
#ifdef HAVE_UTMP_UT_HOST
      bzero (ut->ut_host, sizeof ut->ut_host);
#endif
#ifdef HAVE_UTMP_UT_TV
      gettimeofday (&ut->ut_tv, NULL);
#else
      time (&ut->ut_time);
#endif

      if (pututline (ut) >= 0)
	success = 1;

      endutent ();
    }

#else /* !HAVE_SETUTENT */
#ifdef HAVE_GETUXLINE
  /* Strange utmpx version */
  struct utmpx *ut, search;

  strncpy (search.ut_line, line, sizeof (search.ut_line));
  ut = getutxline (&search);
  if (ut)
    {
      ut->ut_type = DEAD_PROCESS;
      ut->ut_exit.e_termination = 0;
      ut->ut_exit.e_exit = 0;
#ifdef HAVE_GETTIMEOFDAY
      gettimeofday (&ut->ut_tv, NULL);
#else
      time (&ut->ut_tv.tv_sec);
      ut->ut_tv.tv_usec = 0;
#endif
      modutx (ut);
      success = 1;
  }

  endutxent();

#else /* !HAVE_GETUTXLINE */
  /* Old bsdish version */
  int ut_fd;

  ut_fd = open (PATH_UTMP, O_RDWR);
  if (ut_fd >= 0)
    {
      struct utmp *ut;
      struct utmp ut_buf[100];
      off_t pos = 0;		/* Position in file */
      int rd;

      while (!success && (rd = read (ut_fd, ut_buf, sizeof ut_buf)) > 0)
	{
	  struct utmp *ut_end = (struct utmp *)((char *)ut_buf + rd);

	  for (ut = ut_buf; ut < ut_end; ut++, pos += sizeof *ut)
	    if (ut->ut_name[0]
		&& strncmp (ut->ut_name, line, sizeof ut->ut_line) == 0)
	      /* Found the entry for LINE; mark it as logged out.  */
	      {
		/* Zero out entries describing who's logged in.  */
		bzero (ut->ut_name, sizeof ut->ut_name);
#ifdef HAVE_UTMP_UT_HOST
		bzero (ut->ut_host, sizeof ut->ut_host);
#endif
#ifdef HAVE_UTMP_UT_TV
		gettimeofday (&ut->ut_tv, NULL);
#else
		time (&ut->ut_time);
#endif
		
		/* Now seek back to the position in utmp at which UT occured,
		   and write the new version of UT there.  */
		if (lseek (ut_fd, pos, SEEK_SET) >= 0
		    && write (ut_fd, (char *)ut, sizeof (*ut)) == sizeof (*ut))
		  {
		    success = 1;
		    break;
		  }
	      }
	}

      close (ut_fd);
    }
#endif /* HAVE_GETUTXLINE */
#endif /* HAVE_SETUTENT */

  return success;
}
