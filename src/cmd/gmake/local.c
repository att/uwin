/***********************************************************************
*                                                                      *
*                       Copyright (c) 1988-2006                        *
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
/* Local additions/intercepts. */

#include "config.h"

#define getopt		stdio_getopt
#include <stdio.h>
#undef	getopt

#define getopt		stdlib_getopt
#define system		stdlib_system
#include <stdlib.h>
#undef	getopt
#undef	system

#ifndef HAVE_ALLOCA
#include "alloca.c"
#endif

#ifdef _AMIGA
#include "amiga.c"
#endif

#if !defined HAVE_GETOPT || !defined HAVE_GETOPT_LONG
#include "getopt.c"
#include "getopt1.c"
#endif

#ifdef VMS
#include "vmsfunctions.c"
#include "vmsify.c"
#endif
