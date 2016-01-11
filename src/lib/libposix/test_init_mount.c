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
 * test harness for init_mount_extra()
 */

#define TEST_INIT_MOUNT		1

#undef	__IMPORT__
#define __IMPORT__	extern

typedef long ssize_t;
#if 0
typedef int id_t;
typedef int gid_t;
typedef int pid_t;
typedef int uid_t;
#endif

static struct State_s
{
	int	wow;
} state;

static int	trace = 0;

#include <stdio.h>

#if 0
#include "dl.c"
#else
#include <windows.h>
#include <io.h>
#endif
#include "mnthdr.h"

#define F_OK			0
#define PATH_MAX		1024

#define access			_access
#define sfsprintf		_snprintf
#define strncasecmp		_strnicmp

#define uwin_pathmap(w,u,n,f)	strncpy(u,w,n)

#define elementsof(x)		((int)sizeof(x)/(int)sizeof(x[0]))
#undef	logmsg
#define logmsg(l,f,a ...)	do { if (trace) { char* __s__; printf("test_init_mount:%s:%d: ", (__s__ = strrchr(__FILE__, '/')) ? __s__ : __FILE__, __LINE__); printf(f,a); printf("\n"); } } while (0)

#undef	mnt_ptr
#define mnt_ptr(x)		mount_look(0,0,0,0)
#define mount_add(r,s,t,f,n)	(printf("%24s    %s\n", r, s), 0)

extern void	init_mount_extra(void);

#define LOGLEVEL(index)		0
#define MODULE_msi		0

static HANDLE init_module(const char* library)
{
	HANDLE			h;
	char*			f;
	char			buf[PATH_MAX];

	static const char	sys[] = "C:\\Windows\\System32";

	f = "GetModuleHandle";
	sfsprintf(buf, sizeof(buf), "%s\\%s.dll", sys, library);
	if (h = GetModuleHandle(buf))
		goto found;
	f = "LoadLibrary";
	sfsprintf(buf, sizeof(buf), "%s.dll", library);
	if (h = LoadLibrary(buf))
		goto found;
	sfsprintf(buf, sizeof(buf), "%s\\%s.dll", sys, library);
	if (h = LoadLibrary(buf))
		goto found;
	logmsg(LOGLEVEL(index), "library %s not found", library);
	return 0;
 found:
	logmsg(LOGLEVEL(index), "library %s bound to %s by %s", library, buf, f);
	return h;
}

static FARPROC getsymbol(int index, const char* symbol)
{
	FARPROC			sym;

	static HANDLE		handle;

	static const char	library[] = "msi";

	if (!handle && !(handle = init_module(library)))
	{
		logmsg(LOGLEVEL(index), "library %s not found", library);
		return 0;
	}
	if (!(sym = GetProcAddress(handle, symbol)))
		logmsg(LOGLEVEL(index), "symbol %s not found in library %s", symbol, library);
	return sym;
}

static Mount_t* mount_look(char *path, size_t len, int mode, int* chop)
{
	static Mount_t	test;

	strcpy(test.path, "/usr");
	test.offset = 4;
	return &test;
}

int main(int argc, char** argv)
{
	trace = argc > 1;
	init_mount_extra();
	return 0;
}

#include "mnt.c"
