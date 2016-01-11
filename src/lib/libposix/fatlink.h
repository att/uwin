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
extern int fatlinkname(const char*, char*, int);
extern int fatlinktop(char*, unsigned long*, int*);
extern int fatunlink(const char*, unsigned long, int);
extern int fatislink(const char*, unsigned long);
extern int fatlink(const char*, unsigned long, const char*, unsigned long, int);
extern int fatrename(const char*, const char*);
extern int fatclean(int);
extern void fat_init(void);
#define FATLINK	1
