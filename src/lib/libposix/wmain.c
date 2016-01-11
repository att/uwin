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
#include	<windows.h>
#ifdef _DLL
#   include	<stdlib.h>
#else
    extern char	**environ;
    extern int	errno;
#endif

extern int WINAPI _ast_WinMain(HINSTANCE,HINSTANCE,LPSTR,int);
extern int	_ast_runprog(int(WINAPI*)(HINSTANCE,HINSTANCE,LPSTR,int),int, char*[],char***,int*);


#undef WinMain

int WINAPI WinMain(HINSTANCE me, HINSTANCE prev, LPSTR cmdline, int show)
{
	char *argv[4];
	argv[0] = (char*)me;
	argv[1] = (char*)prev;
	argv[2] = (char*)cmdline;
	argv[3] = (char*)show;
	exit(_ast_runprog(_ast_WinMain,0,argv,&environ,&errno));
	return(0);
}
