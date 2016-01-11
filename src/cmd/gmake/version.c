/***********************************************************************
*                                                                      *
*                       Copyright (c) 1988-1999                        *
*                                                                      *
*        This is free software; you can redistribute it and/or         *
*     modify it under the terms of the GNU General Public License      *
*            as published by the Free Software Foundation;             *
*       either version 2, or (at your option) any later version.       *
*                                                                      *
*           This software is distributed in the hope that it           *
*              will be useful, but WITHOUT ANY WARRANTY;               *
*         without even the implied warranty of MERCHANTABILITY         *
*                 or FITNESS FOR A PARTICULAR PURPOSE.                 *
*         See the GNU General Public License for more details.         *
*                                                                      *
*                You should have received a copy of the                *
*                      GNU General Public License                      *
*           along with this software (see the file COPYING.)           *
*                    If not, a copy is available at                    *
*                 http://www.gnu.org/copyleft/gpl.html                 *
*                                                                      *
*                           Richard Stallman                           *
*                            Roland McGrath                            *
*                              Paul Smith                              *
*                                                                      *
***********************************************************************/
/* We use <config.h> instead of "config.h" so that a compilation
   using -I. -I$srcdir will use ./config.h rather than $srcdir/config.h
   (which it would do because make.h was found in $srcdir).  */
#include <config.h>

#ifndef MAKE_HOST
# define MAKE_HOST "unknown"
#endif

char *version_string = VERSION;
char *make_host = MAKE_HOST;

/*
  Local variables:
  version-control: never
  End:
 */
