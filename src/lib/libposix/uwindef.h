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
#ifndef _UWINDEF_H
#define _UWINDEF_H	1

#ifdef _MAC
#   include "fsmac.h"
#endif
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include "fsnt.h"
#   include <windows.h>
#endif
#if !defined(_MAC) && !defined(_WIN32)
#   include "fsdos.h"
#endif

#ifdef _X86_
#   define prog_pc(cp)		((void*)((cp)->Eip))
#   define stack_ptr(cp)	((DWORD*)((cp)->Esp))
#   define stack_base(cp)	((DWORD*)((cp)->Ebp))
#endif

#ifdef _X64_
#   define prog_pc(cp)		((void*)((cp)->Rip))
#   define stack_ptr(cp)	((DWORD*)((cp)->Rsp))
#   define stack_base(cp)	((DWORD*)((cp)->Rbp))
#endif

#ifdef _MIPS_
#   define prog_pc(cp)		((void*)((cp)->Fir))
#   define stack_ptr(cp)	((DWORD*)((cp)->IntSp))
#   define stack_base(cp)	((DWORD*)((cp)->IntRa))
#endif

#ifdef _ALPHA_
#   define prog_pc(cp)		((void*)((cp)->Fir))
#   define stack_ptr(cp)	((DWORD*)((cp)->IntSp))
#   define stack_base(cp)	((DWORD*)((cp)->IntRa))
#endif

extern DWORD XGetProcessId(HANDLE);
#define GetProcessId(x)	XGetProcessId(x)

#define UWIN_CONTEXT_FORK		CONTEXT_ALL
#define UWIN_CONTEXT_TRACE		CONTEXT_ALL

#endif
