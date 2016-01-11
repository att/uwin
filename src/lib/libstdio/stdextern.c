/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#define _in_stdextern	1
#include	"sfstdio.h"

int	_Stdextern = 0;	/* symbol to force loading of this file	*/

#if _native_iob
#define _have_streams	1
#endif

#if !defined(_iob) && _do_iob && !_have_streams
#define _have_streams	1
FILE	_iob[_NFILE];
#endif

#if !defined(_sf) && _do_sf && !_have_streams	/* BSDI2.0 */
FILE	_sf[_NFILE];
#endif

#if _do_swbuf && _do_srget && !_have_streams	/*BSDI3.0*/
#define _have_streams	1
FILE	__sstdin, __sstdout, __sstderr;
#endif

#if _do_self_stdin && !_have_streams /*Linux Redhat 6.0*/
#define _have_streams	1
FILE	_Stdin_,  *stdin  = &_Stdin_;
FILE	_Stdout_, *stdout = &_Stdout_;
FILE	_Stderr_, *stderr = &_Stderr_;
#endif

#if _do_ampersand_stdin && !_have_streams /*Linux Redhat 5.2*/
#define _have_streams	1
FILE	_IO_stdin_, _IO_stdout_, _IO_stderr_;
#endif

#if _do_star_stdin && !_have_streams /*Linux Redhat 5.2*/
#define _have_streams	1
FILE	_Stdin_,  *_IO_stdin  = &_Stdin_;
FILE	_Stdout_, *_IO_stdout = &_Stdout_;
FILE	_Stderr_, *_IO_stderr = &_Stderr_;
#endif
