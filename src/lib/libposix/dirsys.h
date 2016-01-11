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
#ifndef _DIRSYS_H
#define _DIRSYS_H	1

typedef struct DIRSYS_s DIRSYS;

typedef int (*DIR_next_f)(DIRSYS*, struct dirent*);
typedef void (*DIR_close_f)(DIRSYS*);

struct DIRSYS_s
{
	HANDLE		handle;
	char*		fname;
	HGLOBAL		dirh; /* handle of this struct */
	DIR_close_f	closef;
	DIR_next_f	getnext;
	int		flen;
	int		view;
	int		v1;
	int		v2;
	int		uniq;
	unsigned long	hash;
	unsigned long	fileno;
	int		csflag;		/* set for case sensitive directories */
	short		checklink;	/* set to check for .lnk removal */
	short		version;
	pid_t		subdir;		/* set for process subdirectory */
	int		type;
	pid_t		pid;
	struct dirent  entry;
};

#endif
