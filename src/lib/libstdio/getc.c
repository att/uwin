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

/*	Get a byte
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int getc(reg FILE* f)
#else
int getc(f)
reg FILE* f;
#endif
{
	return fgetc(f);
}

#if _lib_getc_unlocked && !_done_getc_unlocked && !defined(getc)
#define _done_getc_unlocked	1
#define getc	getc_unlocked
#include	"getc.c"
#undef getc
#endif
