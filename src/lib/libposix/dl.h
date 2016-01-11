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
#ifndef _DL_H
#define _DL_H			1

#define MODULE_advapi		0
#define MODULE_ast		1
#define MODULE_dbghelp		2
#define MODULE_iphlpapi		3
#define MODULE_kernel		4
#define MODULE_main		5
#define MODULE_msi		6
#define MODULE_msvcrt		7
#define MODULE_netapi		8
#define MODULE_nt		9
#define MODULE_pdh		10
#define MODULE_psapi		11
#define MODULE_pthread		12
#define MODULE_setup		13
#define MODULE_tlhelp		14
#define MODULE_winmm		15
#define MODULE_winsock		16
#define MODULE_COUNT		17

extern FARPROC	getsymbol(int, const char*);

#endif
