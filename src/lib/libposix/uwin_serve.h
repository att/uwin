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
#ifndef _UWINSERV_H
#define _UWINSERV_H		1

#include <uwin_share.h>

/*
 * uwin service definitions
 * for UMS and forkhelper.exe
 */

#define UMS_NAME		"UWIN_MS"
#define UMS_DISPLAYNAME		"UWIN Master"

#define UWIN_PIPE_PREFIX	"\\\\.\\pipe\\UWIN."

#define UWIN_PIPE(b,t,p,c)	(sfsprintf(b,sizeof(b),UWIN_PIPE_PREFIX "pipe.%x.%x.%c",p,c,t),b)
#define UWIN_PIPE_FIFO(b,z,n)	(sfsprintf(b,z,UWIN_PIPE_PREFIX "fifo.%z%z",(n)[0],(n)[1]),b)
#define UWIN_PIPE_FORK(b,p)	(sfsprintf(b,sizeof(b),UWIN_PIPE_PREFIX "fork.%x",p),b)
#define UWIN_PIPE_GROUP		UWIN_PIPE_PREFIX "group"
#define UWIN_PIPE_INIT(b,p)	(sfsprintf(b,sizeof(b),UWIN_PIPE_PREFIX "init.%x",p),b)
#define UWIN_PIPE_LOGIN		UWIN_PIPE_PREFIX "login"
#define UWIN_PIPE_PASSWORD	UWIN_PIPE_PREFIX "passwd"
#define UWIN_PIPE_SETUID	UWIN_PIPE_PREFIX "setuid"
#define UWIN_PIPE_THREAD	UWIN_PIPE_PREFIX "thread"
#define UWIN_PIPE_TOKEN		UWIN_PIPE_PREFIX "token"
#define UWIN_PIPE_UMS		UWIN_PIPE_PREFIX "ums"

#define UMS_PW_SYNC_MSG		128
#define UMS_PW_CONTROL_MSG	129
#define UMS_GR_CONTROL_MSG	130

#define UMS_NO_ID		((DWORD)-1)
#define UMS_MK_TOK		((DWORD)-2)

typedef struct UMS_child_data_s
{
	WoW(HANDLE,		child);
	WoW(HANDLE,		atok);
	DWORD			err;
} UMS_child_data_t;

typedef struct UMS_proc_info_s
{
	WoW(HANDLE,		hProcess);
	WoW(HANDLE,		hThread);
	DWORD			dwProcessId;
	DWORD			dwThreadId;
} UMS_proc_info_t;

typedef struct UMS_domainentry_s
{
	id_t			id;
	char			name[NAME_MAX];
} UMS_domainentry_t;

typedef struct UMS_remote_proc_s
{
	WoW(HANDLE,		tok);
	WoW(HANDLE,		event);
	UMS_proc_info_t		pinfo;
	WoW(char*,		addr);
	DWORD			ntpid;
	DWORD			cflags;
	DWORD			err;
	unsigned short		slot;
	unsigned short		len;
	unsigned short		cmdline;
	unsigned short		cmdname;
	unsigned short		env;
	unsigned short		curdir;
	unsigned short		title;
	unsigned short		reserved;
	unsigned short		rsize;
} UMS_remote_proc_t;

typedef struct UMS_setuid_data_s
{
	WoW(HANDLE,		event);
	WoW(HANDLE,		tok);
	DWORD			uid;
	DWORD			pid;
	DWORD			gid;
	DWORD			ntpid;
	DWORD			dupit;
	DWORD			groups[1];
} UMS_setuid_data_t;

typedef struct UMS_slave_data_s
{
	WoW(HANDLE,		atok);
	DWORD			pid;
} UMS_slave_data_t;

#ifndef _UWIN_H
#define sfsprintf		snprintf
#endif

#define	ADDR(addr,offset)	(((char*)(addr))+((addr)->offset))
#define	ODDR(addr,offset)	(((char*)(addr))+(offset))

extern HANDLE		ums_setids(UMS_setuid_data_t*, int);

#endif
