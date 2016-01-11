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

void _STUB_vmdisc(){}

#else

#include	"vmhdr.h"

/*	Change the discipline for a region.  The old discipline
**	is returned.  If the new discipline is NIL then the
**	discipline is not changed.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
Vmdisc_t* vmdisc(Vmalloc_t* vm, Vmdisc_t* disc)
#else
Vmdisc_t* vmdisc(vm, disc)
Vmalloc_t*	vm;
Vmdisc_t*	disc;
#endif
{
	Vmdisc_t*	old = vm->disc;

	if(disc)
	{	if(disc->memoryf != old->memoryf)
			return NIL(Vmdisc_t*);
		if(old->exceptf &&
		   (*old->exceptf)(vm,VM_DISC,(Void_t*)disc,old) != 0 )
			return NIL(Vmdisc_t*);
		vm->disc = disc;
	}
	return old;
}

#endif
