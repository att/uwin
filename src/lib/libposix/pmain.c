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
#include <windows.h>
#ifdef _DLL
#   include <stdlib.h>
#else
#   define _BLD_DLL 1
#   include     <astwin32.h>
#   undef _BLD_DLL
    extern int __argc;
    extern char **__argv;
    extern char **environ;
#endif
extern struct _astdll *_ast_getdll(void);
extern int main(int,char**, char**);

static void to_unix(char *arg)
{
	register char *cp = arg;
	if(cp[1]==':')
	{
		cp[1] = cp[0];
		cp[0] = '/';
		cp += 2;
	}
	do
	{
		if(*cp=='\\')
			*cp = '/';
	}
	while(*cp++);
}

int WINAPI _ast_WinMain(HINSTANCE me, HINSTANCE prev, LPSTR cmd, int flags)
{
	register struct _astdll *ap=_ast_getdll();
	register char **av= ap->_ast__argv;
	register int argc=0;
	to_unix(av[0]);
	while(*av++)
		argc++;
	return(main(argc,ap->_ast__argv,environ));
}

