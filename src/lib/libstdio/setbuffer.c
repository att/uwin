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

/*	Change stream buffer
**	Written by Kiem-Phong Vo
*/


#if __STD_C
int setbuffer(reg FILE* f, char* buf, size_t size)
#else
int setbuffer(f,buf, size)
reg FILE*	f;
char*		buf;
size_t		size;
#endif
{
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;

	sfsetbuf(sf,(Void_t*)buf, buf ? size : 0);
	return 0;
}
