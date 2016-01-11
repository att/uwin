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
#pragma prototyped

#include "asohdr.h"

#if defined(_UWIN) && defined(_BLD_ast)

NoN(asolock)

#else

int
asolock(unsigned int volatile* lock, unsigned int key, int type)
{
	unsigned int	k;

	if (key)
		switch (type)
		{
		case ASO_UNLOCK:
			return *lock == 0 ? 0 : asocasint(lock, key, 0) == key ? 0 : -1;
		case ASO_TRYLOCK:
			return *lock == key ? 0 : asocasint(lock, 0, key) == 0 ? 0 : -1;
		case ASO_LOCK:
			if (*lock == key)
				return 0;
			/*FALLTHROUGH*/
		case ASO_SPINLOCK:
			for (k = 0; asocasint(lock, 0, key) != 0; ASOLOOP(k));
			return 0;
		}
	return -1;
}

#endif
