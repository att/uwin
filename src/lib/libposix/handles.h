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
#ifndef _HANDLES_H
#define _HANDLES_H	1

#define HTAB_OPEN	0
#define HTAB_CREATE	1
#define HTAB_USE	2
#define HTAB_CLOSE	3

#ifdef HANDLEWATCH
extern void htab_init(HANDLE);
extern void htab_modify(const char*,int,HANDLE,int,int,int,const char*);
extern void htab_list(Pproc_t*,HANDLE);

#define htab_socket(a,b,c,d)	htab_modify(__FILE__,__LINE__,a,b,HT_SOCKET,c,d)

extern BOOL WINAPI SetHandleInformationX(const char*, int, HANDLE, DWORD , DWORD);
extern HANDLE WINAPI CreateThreadX(const char *, int, SECURITY_ATTRIBUTES *, DWORD, LPTHREAD_START_ROUTINE, void *, DWORD, DWORD *);
extern BOOL WINAPI CreatePipeX(const char*, int,  HANDLE*, HANDLE*, SECURITY_ATTRIBUTES*, DWORD);
extern HANDLE WINAPI CreateMutexX(const char*, int, SECURITY_ATTRIBUTES*, BOOL, const char*);
extern HANDLE WINAPI CreateEventX(const char*, int, SECURITY_ATTRIBUTES*,BOOL,BOOL,const char*);
extern HANDLE WINAPI CreateFileMappingX(const char*, int, HANDLE, SECURITY_ATTRIBUTES*, DWORD, DWORD , DWORD , const char*);
extern BOOL WINAPI CreateProcessX(const char*, int, const char*, char*, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, BOOL, DWORD, void*, const char*, STARTUPINFOA*, PROCESS_INFORMATION *);
extern HANDLE WINAPI CreateFileX(const char*, int, const char*, DWORD , DWORD , SECURITY_ATTRIBUTES *, DWORD , DWORD , HANDLE );
extern HANDLE WINAPI OpenProcessX(const char*, int, DWORD , BOOL , DWORD );
extern HANDLE WINAPI OpenEventX(const char*, int, DWORD , BOOL , const char*);
extern HANDLE WINAPI OpenFileMappingX(const char*, int, DWORD , BOOL , const char*);
extern BOOL WINAPI OpenProcessTokenX(const char*, int, HANDLE, DWORD , HANDLE*);
extern BOOL WINAPI DuplicateHandleX(const char*, int, HANDLE , HANDLE , HANDLE , HANDLE , DWORD , BOOL , DWORD );
#if 0
extern BOOL WINAPI CloseHandleX(const char*, int, HANDLE);
#else
extern int closehandleX(const char*, int, HANDLE, int);
#endif

#undef CreateMutex
#undef CreateEvent
#undef CreateFileMapping
#undef CreateFile
#undef CreateProcess
#undef OpenFile
#undef OpenFileMapping
#undef OpenEvent
#undef CloseHandle

#define SetHandleInformation(a,b,c)		SetHandleInformationX(__FILE__,__LINE__,a,b,c)
#define CreateThread(a,b,c,d,e,f)	CreateThreadX(__FILE__,__LINE__,a,b,c,d,e,f)
#define CreatePipe(a,b,c,d)		CreatePipeX(__FILE__,__LINE__,a,b,c,d)
#define CreateMutex(a,b,c)		CreateMutexX(__FILE__,__LINE__,a,b,c)
#define CreateEvent(a,b,c,d)		CreateEventX(__FILE__,__LINE__,a,b,c,d)
#define CreateFileMapping(a,b,c,d,e,f)	CreateFileMappingX(__FILE__,__LINE__,a,b,c,d,e,f)
#define CreateProcess(a,b,c,d,e,f,g,h,i,j)		CreateProcessX(__FILE__,__LINE__,a,b,c,d,e,f,g,h,i,j)
#define CreateFile(a,b,c,d,e,f,g)		CreateFileX(__FILE__,__LINE__,a,b,c,d,e,f,g)
#define OpenProcess(a,b,c)		OpenProcessX(__FILE__,__LINE__,a,b,c)
#define OpenEvent(a,b,c)		OpenEventX(__FILE__,__LINE__,a,b,c)
#define OpenFileMapping(a,b,c)		OpenFileMappingX(__FILE__,__LINE__,a,b,c)
#define OpenProcessToken(a,b,c)	OpenProcessTokenX(__FILE__,__LINE__,a,b,c)
#define DuplicateHandle(a,b,c,d,e,f,g)		DuplicateHandleX(__FILE__,__LINE__,a,b,c,d,e,f,g)
#if 0
#define CloseHandle(a)		CloseHandleX(__FILE__,__LINE__,(a))
#else
#define closehandle(a,b)	closehandleX(__FILE__,__LINE__,(a),(b))
#endif

#else

#define htab_socket(a,b,c,d)

#endif

#ifndef CreateThread

#define CreateThread(a,b,c,d,e,f)	CreateThreadL(#c,__FILE__,__LINE__,a,b,c,d,e,f)

extern HANDLE WINAPI CreateThreadL(const char*, const char*, int, SECURITY_ATTRIBUTES*, DWORD, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*);

#endif

#endif
