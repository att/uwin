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

/*	Internal printf engine to write to a string buffer
**	Written by Kiem-Phong Vo
*/

#if __STD_C
int vsnprintf(char* s, int n, const char* form, va_list args)
#else
int vsnprintf(s,n,form,args)
reg char*	s;
reg int		n;
reg char*	form;
va_list		args;
#endif
{
	return (s && form) ? sfvsprintf(s,n,form,args) : -1;
}

#if _lib___vsnprintf && !done_lib___vsnprintf && !defined(vsnprintf)
#define done_lib___vsnprintf 1
#define vsnprintf __vsnprintf
#include        "vsnprintf.c"
#endif

