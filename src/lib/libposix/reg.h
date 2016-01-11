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
#include <winreg.h>

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY	(0x0200)
#endif
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY	(0x0100)
#endif

#define WOWFLAGS(w)	((w)==32?KEY_WOW64_32KEY:(w)==64?KEY_WOW64_64KEY:0)
