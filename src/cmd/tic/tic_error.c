/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/tic_error.c	1.6"
/*********************************************************************
*                         COPYRIGHT NOTICE                           *
**********************************************************************
*        This software is copyright (C) 1982 by Pavel Curtis         *
*                                                                    *
*        Permission is granted to reproduce and distribute           *
*        this file by any means so long as no fee is charged         *
*        above a nominal handling fee and so long as this            *
*        notice is always included in the copies.                    *
*                                                                    *
*        Other rights are reserved except as explicitly granted      *
*        by written permission of the author.                        *
*                Pavel Curtis                                        *
*                Computer Science Dept.                              *
*                405 Upson Hall                                      *
*                Cornell University                                  *
*                Ithaca, NY 14853                                    *
*                                                                    *
*                Ph- (607) 256-4934                                  *
*                                                                    *
*                Pavel.Cornell@Udel-Relay   (ARPAnet)                *
*                decvax!cornell!pavel       (UUCPnet)                *
*********************************************************************/

/*
 *	tic_error.c -- Error message routines
 *
 *  $Log:	RCS/tic_error.v $
 * Revision 2.1  82/10/25  14:45:31  pavel
 * Added Copyright Notice
 * 
 * Revision 2.0  82/10/24  15:16:32  pavel
 * Beta-one Test Release
 * 
 * Revision 1.3  82/08/23  22:29:31  pavel
 * The REAL Alpha-one Release Version
 * 
 * Revision 1.2  82/08/19  19:09:44  pavel
 * Alpha Test Release One
 * 
 * Revision 1.1  82/08/12  18:36:02  pavel
 * Initial revision
 * 
 *
 */

#include "compiler.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern char *string_table;
extern short term_names;
extern char *progname;

/* VARARGS1 */
#ifdef __STDC__
warning(char *fmt, ...)
#else
warning(va_alist)
va_dcl
#endif
{
#ifndef __STDC__
    register char *fmt;
#endif
    va_list args;

#ifdef __STDC__
    va_start(args, fmt);
#else
    va_start(args);
    fmt = va_arg(args, char *);
#endif
    fprintf (stderr, "%s: Warning: near line %d: ", progname, curr_line);
    fprintf (stderr, "terminal '%s', ", string_table+term_names);
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end(args);
}


/* VARARGS1 */
#ifdef __STDC__
err_abort(char *fmt, ...)
#else
err_abort(va_alist)
va_dcl
#endif
{
#ifndef __STDC__
    register char *fmt;
#endif
    va_list args;

#ifdef __STDC__
    va_start(args, fmt);
#else
    va_start(args);
    fmt = va_arg(args, char *);
#endif
    fprintf (stderr, "%s: Line %d: ", progname, curr_line);
    fprintf (stderr, "terminal '%s', ", string_table+term_names);
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end(args);
    exit(1);
}


/* VARARGS1 */
#ifdef __STDC__
syserr_abort(char *fmt, ...)
#else
syserr_abort(va_alist)
va_dcl
#endif
{
#ifndef __STDC__
    register char *fmt;
#endif
    va_list args;

#ifdef __STDC__
    va_start(args, fmt);
#else
    va_start(args);
    fmt = va_arg(args, char *);
#endif
    fprintf (stderr, "PROGRAM ERROR: Line %d: ", curr_line);
    fprintf (stderr, "terminal '%s', ", string_table+term_names);
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
	fprintf(stderr,"*** Possibly corrupted terminfo file ***\n");
    va_end(args);
    exit(1);
}
