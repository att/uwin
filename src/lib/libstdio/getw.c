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

/*	Get a word.
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int getw(FILE* f)
#else
int getw(f)
reg FILE*	f;
#endif
{
	reg Sfio_t*	sf;
	int		w;

	if(!(sf = _sfstream(f)))
		return -1;

	if(sfread(sf,(char*)(&w),sizeof(int)) != sizeof(int))
	{	_stdseterr(f,sf);
		return -1;
	}
	else	return w;
}
