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

void _STUB_vmregion(){}

#else

#include	"vmhdr.h"

/*	Return the containing region of an allocated piece of memory.
**	Beware: this only works with Vmbest, Vmdebug and Vmprofile.
**
**	10/31/2009: Add handling of shared/persistent memory regions.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
Vmalloc_t* vmregion(Void_t* addr)
#else
Vmalloc_t* vmregion(addr)
Void_t*	addr;
#endif
{
	Vmalloc_t	*vm;
	Vmdata_t	*vd;

	if(!addr)
		return NIL(Vmalloc_t*);

	vd = SEG(BLOCK(addr))->vmdt;
	for(vm = Vmheap; vm; vm = vm->next)
		if(vm->data == vd)
			break;

	return vm;
}

#endif
