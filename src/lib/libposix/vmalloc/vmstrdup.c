/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#if defined(_UWIN) && defined(_BLD_ast)

void _STUB_vmstrdup(){}

#else

#include "vmhdr.h"

/*
 * return a copy of s using vmalloc
 */

#if __STD_C
char* vmstrdup(Vmalloc_t* v, register const char* s)
#else
char* vmstrdup(v, s)
Vmalloc_t*	v;
register char*	s;
#endif
{
	register char*	t;
	register size_t	n;

	return (s && (t = vmalloc(v, n = strlen(s) + 1))) ? (char*)memcpy(t, s, n) : (char*)0;
}

#endif
