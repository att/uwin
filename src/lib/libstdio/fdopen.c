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

/*	Opening a stream given a file descriptor.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
FILE* fdopen(int fd, const char* mode)
#else
FILE* fdopen(fd,mode)
int	fd;
char*	mode;
#endif
{
	reg Sfio_t*	sf;
	reg FILE*	f;
	int		flags, uflag;

	if((flags = _sftype(mode,NIL(int*),&uflag)) == 0)
		return NIL(FILE*);
	if(!uflag)
		flags |= SF_MTSAFE;

	if(!(sf = sfnew(NIL(Sfio_t*), NIL(Void_t*), (size_t)SF_UNBOUND, fd, flags)))
		return NIL(FILE*);
	if(!(f = _stdstream(sf, NIL(FILE*))))
	{	sfclose(sf);
		return NIL(FILE*);
	}

	return(f);
}
