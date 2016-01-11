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
#ifndef _DEBUG_CRT_H
#define _DEBUG_CRT_H	1

#if !DEBUG_CRT || __DMC__

#undef	DEBUG_CRT
#define DEBUG_CRT(m,v)

#else

#include <windows.h>

#define _STRING(x)	#x
#define STRING(x)	_STRING(x)

#undef	DEBUG_CRT
#define DEBUG_CRT(m,v)	debug_crt(__FILE__,STRING(__LINE__),m,v)

static void debug_crt(const char* file, const char* line, const char* msg, int val)
{
	const char*	s;
	char*		b;
	char*		t;
	HANDLE		f;
	DWORD		n;
	char		buf[128];
	char		num[16];

	b = buf;
	for (s = "DEBUG "; *s; *b++ = *s++);
	for (s = file; *s; *b++ = *s++);
	*b++ = '#';
	for (s = line; *s; *b++ = *s++);
	*b++ = ' ';
	for (s = msg; *s; *b++ = *s++);
	*b++ = ' ';
	t = num;
	if (val < 0)
	{
		val = -val;
		*t++ = '-';
	}
	do
	{
		*t++ = '0' + (val % 10);
	} while (val /= 10);
	while (t > num)
		*b++ = *--t;
	*b++ = '\n';
	n = b - buf;
	f = CreateFile("DEBUG_CRT.txt", FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	WriteFile(f, buf, n, &n, NULL);
	CloseHandle(f);
}

#endif

#endif
