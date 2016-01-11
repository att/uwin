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

/*	Reopening a stream
**	Written by Kiem-Phong Vo
*/


#if __STD_C
FILE* freopen(char*name, const char* mode, reg FILE* f)
#else
FILE* freopen(name,mode,f)
reg char*	name;
reg char*	mode;
reg FILE*	f;
#endif
{
	Sfio_t*	sf;

	if(f && (sf = _sfstream(f)) )
		_sfunmap(f);

	if(!(sf = sfopen(sf, name, mode)) )
		return NIL(FILE*);
	else
	{	int	uflag;
		_sftype(mode, NIL(int*), &uflag);	
		if(!uflag) 
			sf->flags |= SF_MTSAFE;

		if(_stdstream(sf,f) != f)
		{	sfclose(sf);
			return NIL(FILE*);
		}
		else	return f;
	}
}
