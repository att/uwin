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

/*	Return the eof condition if any.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int feof(FILE* f)
#else
int feof(f)
FILE*	f;
#endif
{
	reg Sfio_t*	sf;
	
	if(!(sf = _sfstream(f)))
		return -1;

	_stdseterr(f,sf);
	return sfeof(sf);
}

#if _lib_feof_unlocked && !_done_feof_unlocked && !defined(feof)
#define _done_feof_unlocked	1
#define feof	feof_unlocked
#include	"feof.c"
#undef feof
#endif
