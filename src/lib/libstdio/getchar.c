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


/*	Get a byte from stdin.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int getchar(void)
#else
int getchar()
#endif
{
	return fgetc(stdin);
}

#if _lib_getchar_unlocked && !_done_getchar_unlocked && !defined(getchar)
#define _done_getchar_unlocked	1
#define getchar	getchar_unlocked
#include	"getchar.c"
#undef getchar
#endif
