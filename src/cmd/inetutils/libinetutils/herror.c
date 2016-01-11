/* A replacement version of herror

   Copyright (C) 1997 Free Software Foundation, Inc.

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

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <netdb.h>

#ifdef HAVE_H_ERRNO
# ifndef HAVE_H_ERRNO_DECL
extern int h_errno;
# endif /* !HAVE_H_ERRNO_DECL */
#else /* !HAVE_H_ERRNO */
# ifdef HAVE_ERRNO_H
#  include <errno.h>
# endif
# ifndef HAVE_ERRNO_DECL
extern int errno;
# endif
#endif /* HAVE_H_ERRNO */

extern const char *hstrerror __P ((int));

/* Print an error message on stderror containing the latest host error.  */
void
herror (pfx)
  char *pfx;
{
#ifdef HAVE_H_ERRNO
  int herr = h_errno;
#else
  int herr = errno;		/* ???? */
#endif

  if (pfx)
    fprintf (stderr, "%s: %s\n", pfx, hstrerror (herr));
  else
    fprintf (stderr, "%s\n", hstrerror (herr));
}
