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
#ifndef _UWIN_MOUNT_H
#define _UWIN_MOUNT_H		1

#ifndef BLOCK_SIZE
#define BLOCK_SIZE		1024
#endif

#define UWIN_MOUNT_root_path	0x01	/* Uwin_mount_t.path has state.root prefix */
#define UWIN_MOUNT_root_onpath	0x02	/* Uwin_mount_t.path+Uwin_mount_t.offset has state.root prefix */
#define UWIN_MOUNT_usr_path	0x04	/* Uwin_mount_t.path has state.root+/usr prefix */

typedef struct Uwin_mount_s
{
	unsigned long	refcount;	/* reference count */
        unsigned int	attributes;	/* my attributes */
	unsigned int	pattributes;	/* parent attributes */
        unsigned short	type;		/* mount type */
	short		flags;		/* UWIN_MOUNT_* */
        short		server;		/* cs server string */
        short		offset;		/* offset for mounted on path */
	short		len;		/* length of mounted on path */
        char		path[BLOCK_SIZE-4*sizeof(short)-3*sizeof(long)];
} Uwin_mount_t;

#endif
