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

void _STUB_vmset(){}

#else

#include	"vmhdr.h"


/*	Set the control flags for a region.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 01/16/94.
*/
#if __STD_C
int vmset(reg Vmalloc_t* vm, int flags, int on)
#else
int vmset(vm, flags, on)
reg Vmalloc_t*	vm;	/* region being worked on		*/
int		flags;	/* flags must be in VM_FLAGS		*/
int		on;	/* !=0 if turning on, else turning off	*/
#endif
{
	reg int		mode, inuse;
	reg Vmdata_t*	vd = vm->data;

	if(flags == 0 && on == 0)
		return vd->mode;

	SETINUSE(vd, inuse);
	if(!(vd->mode&VM_TRUST) )
	{	if(ISLOCK(vd,0))
		{	CLRINUSE(vd, inuse);
			return 0;
		}
		SETLOCK(vd,0);
	}

	mode = vd->mode;

	if(on)
		vd->mode |=  (flags&VM_FLAGS);
	else	vd->mode &= ~(flags&VM_FLAGS);

	if(vd->mode&(VM_TRACE|VM_MTDEBUG))
		vd->mode &= ~VM_TRUST;

	CLRLOCK(vd,0);
	CLRINUSE(vd, inuse);

	return mode;
}

#endif
