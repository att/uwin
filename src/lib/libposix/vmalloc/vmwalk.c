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

void _STUB_vmwalk(){}

#else

#include	"vmhdr.h"

/*	Walks all segments created in region(s)
**
**	Written by Kiem-Phong Vo, kpv@research.att.com (02/08/96)
*/

#if __STD_C
int vmwalk(Vmalloc_t* vm, int(*segf)(Vmalloc_t*, Void_t*, size_t, Vmdisc_t*) )
#else
int vmwalk(vm, segf)
Vmalloc_t*	vm;
int(*		segf)(/* Vmalloc_t*, Void_t*, size_t, Vmdisc_t* */);
#endif
{	
	reg Seg_t*	seg;
	reg int		rv;

	if(!vm)
	{	for(vm = Vmheap; vm; vm = vm->next)
		{	if(!(vm->data->mode&VM_TRUST) && ISLOCK(vm->data,0) )
				continue;

			SETLOCK(vm->data,0);
			for(seg = vm->data->seg; seg; seg = seg->next)
			{	rv = (*segf)(vm, seg->addr, seg->extent, vm->disc);
				if(rv < 0)
					return rv;
			}
			CLRLOCK(vm->data,0);
		}
	}
	else
	{	if(!(vm->data->mode&VM_TRUST) && ISLOCK(vm->data,0) )
			return -1;

		SETLOCK(vm->data,0);
		for(seg = vm->data->seg; seg; seg = seg->next)
		{	rv = (*segf)(vm, seg->addr, seg->extent, vm->disc);
			if(rv < 0)
				return rv;
		}
		CLRLOCK(vm->data,0);
	}

	return 0;
}

#endif
