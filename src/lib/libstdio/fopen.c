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

/*	Opening a stream
**	Written by Kiem-Phong Vo
*/

#if __STD_C
FILE* fopen(char* name,const char* mode)
#else
FILE* fopen(name,mode)
char*	name;
char*	mode;
#endif
{
	reg FILE*	f;
	reg Sfio_t*	sf;

	if(!(sf = sfopen(NIL(Sfio_t*), name, mode)) )
		f = NIL(FILE*);
	else if(!(f = _stdstream(sf, NIL(FILE*))))
		sfclose(sf);
	else
	{	int	uflag;
		_sftype(mode, NIL(int*), &uflag);
		if(!uflag)
			sf->flags |= SF_MTSAFE;
	}

	return(f);
}
