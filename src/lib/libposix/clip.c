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
/*
 * code for the clipboard device
 */
#include	"uwindef.h"
#include	<shellapi.h>

static HANDLE clip_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	fdp->type = FDTYPE_CLIPBOARD;
	fdp->devno=0;
	return((HANDLE)1);
}

static int clipclose(int fd, Pfd_t* fdp,int noclose)
{
	return(0);
}

static int clipsize(HANDLE *hp, char **cp)
{
	int size = 0;
	if(!IsClipboardFormatAvailable(CF_TEXT))
		return(0);
	if(!OpenClipboard(NULL))
		return(0);
	if(!(*hp=GetClipboardData(CF_TEXT)))
		return(0);
	if((*cp=(char*)GlobalLock(*hp)) && (size=(int)strlen(*cp))==0)
		GlobalUnlock(*hp) ;
	CloseClipboard();
	return(size);
}

ssize_t clipread(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	HANDLE hp;
	int len,size,type;
	char *cp, **bf = (char **)buff;

	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (int)asize;
	if(IsClipboardFormatAvailable(CF_TEXT))
		type = CF_TEXT;
	else if(IsClipboardFormatAvailable(CF_OEMTEXT))
		type = CF_OEMTEXT;
	else if(IsClipboardFormatAvailable(CF_HDROP))
		type = CF_HDROP;
	else
	{
		logerr(LOG_DEV+5, "IsClipboardFormatAvailable");
		errno = unix_err(GetLastError());
		return(0);
	}

	if(!OpenClipboard(NULL))
	{
		logerr(LOG_DEV+5, "OpenClipboard");
		errno = unix_err(GetLastError());
		return(-1);
	}
	if(!(hp=GetClipboardData(type)))
	{
		errno = unix_err(GetLastError());
		logerr(LOG_DEV+5, "GetClipboardData");
		CloseClipboard();
		return(-1);
	}
	if(!(cp = (char*)GlobalLock(hp)))
	{
		errno = unix_err(GetLastError());
		logerr(LOG_DEV+5, "GlobalLock");
		CloseClipboard();
		return(-1);
	}
	if(type==CF_HDROP)
	{
		int s,r,n,i,nfiles=DragQueryFile(hp,0xFFFFFFFF,NULL,0);
		len = 3*nfiles+2;
		for (i = 0; i < nfiles; i++)
			len += DragQueryFile(hp, i, NULL,0);
		if(!(cp =(char *)malloc(len*sizeof(char))))
		{
			logmsg(0, "malloc out of space len=%d",len);
			errno = ENOMEM;
			return(-1);
		}
		for (n=0,i=0; i < nfiles; i++)
		{
			cp[n++] = '\'';
			r = DragQueryFile(hp, i, &cp[n],len-n);
			cp[n+r] = 0;
			s= unmapfilename(&cp[n], 0, 0);
			if(!s)
				cp[n-1] = ' ';
			n +=r;
			if(s)
				cp[n++] = '\'';
			cp[n++] = ' ';

		}
		cp[n] = 0;
	}
	else
		len = (int)strlen(cp);
	if(!fdp)
	{
		size = len+1;
		if(!(buff =(char *)malloc(size*sizeof(char))))
		{
			logmsg(0, "malloc out of space size=%d",size);
			errno = ENOMEM;
			return(-1);
		}
		*bf = buff;
	}
	/* fdp->sigio is current offset */
	if(fdp)
	{
		if(fdp->sigio >= len)
		{
			fdp->sigio = 0;
			return(0);
		}
		cp += fdp->sigio;
		len -= fdp->sigio;
	}
	len = (size>len)?len:size;
	if(fdp)
		fdp->sigio += len;
	memcpy(buff,cp,len);
	GlobalUnlock(hp) ;
	CloseClipboard();
	if(cp && type==CF_HDROP)
		free((void*)cp);
	return(len);
}

ssize_t clipwrite(int fd, register Pfd_t* fdp, const char *buff, size_t asize)
{
	HANDLE gp,hp;
	char *cp,*old;
	int i=0,len,size;

	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (int)asize;
	len = (int)strlen(buff);
	len = (size>len)?len:size;
	if(fdp)
	{
		i = clipsize(&hp,&old);
		if((fdp->oflag&O_APPEND) || fdp->sigio>i)
			fdp->sigio = i;
		i = fdp->sigio;
	}
	if(!(gp = GlobalAlloc (GHND,(DWORD)(i+len+1))))
	{
		errno = unix_err(GetLastError());
		logerr(LOG_DEV+5, "GlobalAlloc");
		return(-1);
	}
	if(!(cp = (char *)GlobalLock(gp)))
	{
		errno = unix_err(GetLastError());
		logerr(LOG_DEV+5, "GlobalLock");
		return(-1);
	}
	if(i>0)
	{
		memcpy(cp,old,i);
		GlobalUnlock(hp);
	}
	memcpy(&cp[i],buff,len);
	GlobalUnlock(gp);
	if(!OpenClipboard(NULL))
	{
		logerr(LOG_DEV+5, "OpenClipboard");
		errno = unix_err(GetLastError());
		return(-1);
	}
	EmptyClipboard();
	if(!SetClipboardData(CF_TEXT, gp))
	{
		logerr(LOG_DEV+5, "SetClipboardData");
		errno = unix_err(GetLastError());
		len = -1;
	}
	CloseClipboard();
	if(fdp)
		fdp->sigio += len;
	return(len);
}

void init_clipboard(Devtab_t *dp)
{
	filesw(FDTYPE_CLIPBOARD,clipread,clipwrite,nulllseek,pipefstat,clipclose,0);
	dp->openfn = clip_open;
}

