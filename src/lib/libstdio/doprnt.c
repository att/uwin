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
#define _in_doprnt	1
#include	"sfstdio.h"

/*	The internal printf engine in older stdios.
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int _doprnt(const char* form, va_list args, FILE* f)
#else
int _doprnt(form,args,f)
char*	form;          /* format to use */
va_list args;           /* arg list if argf == 0 */
FILE*	f;
#endif
{
	reg int		rv;
	reg Sfio_t*	sf;

	if(!(sf = _sfstream(f)) )
		return -1;

	if((rv = sfvprintf(sf,form,args)) < 0)
		_stdseterr(f,sf);
	return rv;
}
