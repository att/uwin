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
int fputc(int c, FILE* f)
#else
int fputc(c, f)
reg int		c;
reg FILE*	f;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;

	if((c = sfputc(sf,c)) < 0)
		_stdseterr(f,sf);
	return c;
}

#if _lib__IO_putc && !_done_IO_putc && !defined(fputc)
#define _done_IO_putc	1
#define fputc	_IO_putc
#include	"fputc.c"
#undef fputc
#endif

#if _lib_fputc_unlocked && !_done_fputc_unlocked && !defined(fputc)
#define _done_fputc_unlocked	1
#define fputc	fputc_unlocked
#include	"fputc.c"
#undef fputc
#endif
