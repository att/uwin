/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
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
#ifndef _ASOHDR_H
#define _ASOHDR_H	1

#if _PACKAGE_ast

#include	<ast.h>
#include	<error.h>
#include	<fnv.h>

#else

#include	<errno.h>

#ifndef elementsof
#define elementsof(x)	(sizeof(x)/sizeof(x[0]))
#endif
#ifndef integralof
#define integralof(x)	(((char*)(x))-((char*)0))
#endif
#ifndef FNV_MULT
#define FNV_MULT	0x01000193L
#endif
#ifndef NiL
#define NiL		((void*)0)
#endif
#ifndef NoN 
#if defined(__STDC__) || defined(__STDPP__)
#define NoN(x)		void _STUB_ ## x () {}
#else
#define NoN(x)		void _STUB_/**/x () {}
#endif
#if !defined(_STUB_)
#define _STUB_
#endif
#endif

#endif

#include	<unistd.h>

#include	"FEATURE/aso"

#if _UWIN
#undef	_aso_fcntl
#undef	_aso_semaphore
#endif

#include	"aso.h"

#define HASH(p,z)	((integralof(p)*FNV_MULT)%(z))

#endif
