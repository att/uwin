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

/*	Closing a stream.
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int fclose(reg FILE* f)
#else
int fclose(f)
FILE*	f;
#endif
{
	reg int		rv;
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)))
		return -1;

	if((rv = sfclose(sf)) >= 0 && f != stdin && f != stdout && f != stderr)
	{	_sfunmap(f); /* remove the FILE to Sfio_t map */
		free(f);
	}

	return rv;
}
