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

/*	Write out a byte
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int putc(int c, FILE* f)
#else
int putc(c, f)
reg int		c;
reg FILE*	f;
#endif
{
	return fputc(c,f);
}

#if _lib_putc_unlocked && !_done_putc_unlocked && !defined(putc)
#define _done_putc_unlocked	1
#define putc	putc_unlocked
#include	"putc.c"
#undef putc
#endif
