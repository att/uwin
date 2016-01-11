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
#include	<mnt.h>
#include	<sys/mount.h>
#define FILE void
#include	<mntent.h>

#define Uwin_mount_t	Mount_t
#include	"uwin_mount.h"

struct mstate
{
	unsigned long    drives;
	int     drive;
	int     index;
	int	file;
	Mnt_t   instance;
	char    type[128];
	char    dname[400];
};

#define mnt_ptr(i)	((Mount_t*)block_ptr(i))
#define mnt_onpath(mp)	(&((mp)->path[(mp)->offset]))
#define mnt_ftype(x)	((unsigned int)x>>26)

Mount_t *mount_look(char*, size_t, int, int*);
void init_mount(void);
void init_mount_extra(void);
void *mntfopen(struct mstate*, const char*, const char*);
ssize_t mnt_print(char*, size_t, Mount_t*);
