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

/*	Change buffer and set/unset line buffering.
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int setvbuf(reg FILE* f, char* buf, int flags, size_t size)
#else
int setvbuf(f, buf, flags, size)
reg FILE*	f;
char*		buf;
int		flags;
size_t		size;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;

	if(flags == _IOLBF)
		sfset(sf,SF_LINE,1);
	else if(flags == _IONBF)
	{	sfsync(sf);
		sfsetbuf(sf,NIL(Void_t*),0);
		sfset(sf,SF_LINE,0);
	}
	else if(flags == _IOFBF)
	{	if(size == 0)
			size = BUFSIZ;
		sfsync(sf);
		sfsetbuf(sf,buf,size);
		sfset(sf,SF_LINE,0);
	}
	return 0;
}
