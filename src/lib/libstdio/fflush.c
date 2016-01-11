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

/*	Flushing buffered output data.
**	This has been modified to work with both input&output streams.
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int fflush(reg FILE* f)
#else
int fflush(f)
reg FILE*	f;
#endif
{
	reg Sfio_t*	sf;

	if(!f)
		return sfsync(NIL(Sfio_t*));
	if(!(sf = _sfstream(f)))
		return -1;

	if(sf->extent >= 0)
		(void)sfseek(sf, (Sfoff_t)0, SEEK_CUR|SF_PUBLIC);

	return (sfsync(sf) < 0 || sfpurge(sf) < 0) ? -1 : 0;
}

#if _lib_fflush_unlocked && !_done_fflush_unlocked && !defined(fflush)
#define _done_fflush_unlocked	1
#define fflush	fflush_unlocked
#include	"fflush.c"
#undef fflush
#endif
