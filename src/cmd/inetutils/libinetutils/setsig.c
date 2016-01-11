/* Set a signal handler, trying to turning on the SA_RESTART bit

   Copyright (C) 1997 Free Software Foundation, Inc.

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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <signal.h>

/* This is exactly like the traditional signal function, but turns on the
   SA_RESTART bit where possible.  */
sig_t
setsig (sig, handler)
     int sig;
     sig_t handler;
{
#ifdef HAVE_SIGACTION
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);
#ifdef SA_RESTART
  sa.sa_flags = SA_RESTART;
#endif
  sa.sa_handler = handler;
  sigaction (sig, &sa, &sa);
  return sa.sa_handler;
#else /* !HAVE_SIGACTION */
#ifdef HAVE_SIGVEC
  struct sigvec sv;
  sigemptyset (&sv.sv_mask);
  sv.sv_handler = handler;
  sigvec (sig, &sv, &sv);
  return sv.sv_handler;
#else /* !HAVE_SIGVEC */
  return signal (sig, handler);
#endif /* HAVE_SIGVEC */
#endif /* HAVE_SIGACTION */
}
