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

/*	Clear error status from a stream
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
void clearerr(FILE* f)
#else
void clearerr(f)
FILE*	f;
#endif
{
	reg Sfio_t*	sf;
	
	if(f && (sf = _sfstream(f)) )
	{	sfclrlock(sf);
		_stdclrerr(f);
	}
}

#if _lib_clearerr_unlocked && !_done_clearerr_unlocked && !defined(clearerr)
#define _done_clearerr_unlocked	1
#define clearerr	clearerr_unlocked
#include		"clearerr.c"
#undef clearerr
#endif
