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
#include	"sfstdio.h"

/*
**	Cleanup actions before exiting
**	Written by Kiem-Phong Vo.
*/

#if !_PACKAGE_ast && !_lib_atexit && !_lib_on_exit && !_lib_onexit && _exit_cleanup
static int _Sfio_already_has_cleanup;
#else

#if __STD_C
void _cleanup(void)
#else
void _cleanup()
#endif
{
	if(_Sfcleanup)
		(*_Sfcleanup)();
}

#endif
