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

void _STUB_vmclose(){}

#else

#include	"vmhdr.h"

/*	Close down a region.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
int vmclose(Vmalloc_t* vm)
#else
int vmclose(vm)
Vmalloc_t*	vm;
#endif
{
	Seg_t		*seg, *vmseg, *next;
	Vmalloc_t	*v, *last;
	Vmdata_t*	vd = vm->data;
	int		ev = 0;

	if(vm == Vmheap)
		return -1;

	if(!(vd->mode&VM_TRUST) && ISLOCK(vd,0))
		return -1;

	if(vm->disc->exceptf &&
	   (ev = (*vm->disc->exceptf)(vm,VM_CLOSE,NIL(Void_t*),vm->disc)) < 0 )
		return -1;

	/* make this region inaccessible until it disappears */
	vd->mode &= ~VM_TRUST;
	SETLOCK(vd,0);

	if((vd->mode&VM_MTPROFILE) && _Vmpfclose)
		(*_Vmpfclose)(vm);

	/* remove from linked list of regions	*/
	for(last = Vmheap, v = last->next; v; last = v, v = v->next)
	{	if(v == vm)
		{	last->next = v->next;
			break;
		}
	}

	if(ev == 0)
	{	vmseg = NIL(Seg_t*);
		for(seg = vd->seg; seg; seg = next)
		{	next = seg->next;
			if(seg->extent == seg->size)
				vmseg = seg;
			else	(*vm->disc->memoryf)(vm,seg->addr,seg->extent,0,vm->disc);
		}
		if(vmseg)
			(*vm->disc->memoryf)(vm,vmseg->addr,vmseg->extent,0,vm->disc);
	}
	else	CLRLOCK(vd,0);

	vmfree(Vmheap,vm);

	return 0;
}

#endif
