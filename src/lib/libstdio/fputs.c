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

/*	Output a string.
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int fputs(const char* s, FILE* f)
#else
int fputs(s, f)
reg char*	s;
reg FILE*	f;
#endif
{
	reg int		rv;
	reg Sfio_t*	sf;

	if(!s || !(sf = _sfstream(f)))
		return -1;

	if((rv = sfputr(sf,s,-1)) < 0)
		_stdseterr(f,sf);
	return rv;
}

#if _lib_fputs_unlocked && !_done_fputs_unlocked && !defined(fputs)
#define _done_fputs_unlocked	1
#define fputs	fputs_unlocked
#include	"fputs.c"
#undef fputs
#endif
