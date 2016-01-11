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
#include	"uwindef.h"
#include	"stropts.h"

extern int isastream(int fd)
{
	if (!isfdvalid (P_CP,fd))
	{
                errno = EBADF;
                return(-1);
	}
	return(0);
}

extern int getmsg(int fd, struct strbuf *cp, struct strbuf *dp, int *flgp)
{
	if(!isfdvalid (P_CP,fd))
                errno = EBADF;
	else
                errno = ENOSTR;
	return(-1);
}

extern int getpmsg(int fd,struct strbuf *cp,struct strbuf *dp,int *bp,int* flgp)
{
	if(!isfdvalid (P_CP,fd))
                errno = EBADF;
	else
                errno = ENOSTR;
	return(-1);
}

extern int putmsg(int fd,const struct strbuf *cp,const struct strbuf *dp,int flags)
{
	if(!isfdvalid (P_CP,fd))
                errno = EBADF;
	else
                errno = ENOSTR;
	return(-1);
}

extern int putpmsg(int fd, const struct strbuf *cp,const struct strbuf *dp, int band, int flags)
{
	if(!isfdvalid (P_CP,fd))
                errno = EBADF;
	else
                errno = ENOSTR;
	return(-1);
}

extern int fattach(int fd, const char *path)
{
	if(!isfdvalid (P_CP,fd))
                errno = EBADF;
	else
                errno = ENOSTR;
	return(-1);
}
extern int fdetatch(const char *path)
{
	struct stat statb;
	if(stat(path,&statb) < 0)
		return(-1);
	return(0);
}
