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
#include <sys/types.h>

#undef st_mtime
#undef st_ctime
#undef st_atime


struct ostat
{
	mode_t st_mode;
	ino_t st_ino;
	dev_t st_dev;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	off_t st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
	long    st_blksize;
	long	st_blocks;
};

static void new2old(struct stat *np, struct ostat *op)
{
	op->st_mode = np->st_mode;
	op->st_ino = np->st_ino;
	op->st_dev = np->st_dev;
	op->st_nlink = np->st_nlink;
	op->st_uid = np->st_uid;
	op->st_gid = np->st_gid;
	op->st_size = np->st_size;
	op->st_atime = np->st_atim.tv_sec;
	op->st_mtime = np->st_mtim.tv_sec;
	op->st_ctime = np->st_ctim.tv_sec;
	op->st_blksize = np->st_blksize;
	op->st_blocks = np->st_blocks;
}

#define st_mtime	st_mtim.tv_sec
#define st_ctime	st_ctim.tv_sec
#define st_atime	st_atim.tv_sec
