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
#ifndef _HEADERS_H
#define _HEADERS_H	1
/*
 * standard list of include files
 */

#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <termios.h>
#include <grp.h>
#include <pwd.h>
#include <dlfcn.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <strings.h>
#include <mnt.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/sem.h>
#include <setjmp.h>
#include <sys/mount.h>
#include <uwin.h>


#ifndef O_BINARY
#   define O_BINARY	0
#endif /* O_BINARY */
#ifndef OPEN_MAX
#   define OPEN_MAX	64
#endif /* OPEN_MAX */
#ifndef PATH_MAX
#   define PATH_MAX	256
#endif /* PATH_MAX */
#endif /* _HEADERS_H */
