/* Clean up the pty and frob utmp/wtmp accordingly after logout

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
#ifdef HAVE_UTMP_H
#include <utmp.h>
#else
#ifdef  HAVE_UTMPX_H
#include <utmpx.h>
#define utmp utmpx		/* make utmpx look more like utmp */
#endif
#endif

/* Update umtp & wtmp as necessary, and change tty & pty permissions back to
   what they should be.  */
void
cleanup_session (tty, pty_fd)
  char *tty;
  int pty_fd;
{
  char *line, *pty;

#ifdef PATH_TTY_PFX
  if (strncmp (tty, PATH_TTY_PFX, sizeof PATH_TTY_PFX - 1) == 0)
    line = tty + sizeof PATH_TTY_PFX - 1;
  else
#endif
    line = tty;

  if (logout (line))
    logwtmp (line, "", "");

  chmod (tty, 0666);
  chown (tty, 0, 0);
  fchmod (pty_fd, 0666);
  fchown (pty_fd, 0, 0);
}
