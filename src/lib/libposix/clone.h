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
#ifndef _CLONE_H
#define _CLONE_H	0

typedef struct Clone_s
{
	char		path[128];		/* helper process path	*/
	HANDLE		hp;
	HANDLE		hps;
	HANDLE		ev;
	DWORD		msg;
	int		helper;			/* in helper process	*/
	int		on;			/* clone fork enabled	*/
	unsigned int	codepage;
} Clone_t;

#define	SOP_SOCKET	0
#define	SOP_SOCKOPT	1
#define	SOP_BIND	2
#define	SOP_LISTEN	3
#define	SOP_CONNECT	4
#define	SOP_ACCEPT	5
#define	SOP_EVENTS	6
#define	SOP_ESELECT	7
#define	SOP_SOCKET2	8
#define	SOP_BIND2	9
#define	SOP_CLOSE	10
#define	SOP_LISTEN2	11
#define	SOP_DUP		12
#define	SOP_NINFO	13
#define	SOP_AINFO	14
#define	SOP_NAME	15
#define	SOP_HNAME	16
#define	SOP_INIT	17
#define	SOP_GETHBYN	18
#define	SOP_GETHBYA	19
#define	SOP_GETSBYN	20
#define	SOP_GETSBYP	21
#define	SOP_GETPBYN	22
#define	SOP_GETPBY_	23

#define _CLONE_STATE_		Clone_t clone;

#define CLONE			"forkhelper"

#define UWIN_PIPE_CLONE(b,p)	(sfsprintf(b,sizeof(b),UWIN_PIPE_PREFIX "clone.%x",p),b)

extern DWORD WINAPI	fh_main(void*);

extern pid_t 		ntclone(void);
extern BOOL WINAPI	cl_createproc(HANDLE,const char*, char*, SECURITY_ATTRIBUTES*, SECURITY_ATTRIBUTES*, BOOL, DWORD, void*, const char*, STARTUPINFO*, PROCESS_INFORMATION*);
extern HANDLE WINAPI	cl_createthread(SECURITY_ATTRIBUTES*, SIZE_T, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*);
extern HANDLE WINAPI	cl_mutex(SECURITY_ATTRIBUTES*, DWORD, BOOL, const char*);
extern BOOL WINAPI	cl_threadpriority(HANDLE,int);
extern HANDLE WINAPI	cl_event(SECURITY_ATTRIBUTES*, DWORD, BOOL, BOOL, const char*);
extern HANDLE 		cl_fsopen(Pfd_t*, const char*, HANDLE*, int, mode_t);
extern int		cl_chstat(const char*, int, uid_t, gid_t, mode_t,int);
extern BOOL WINAPI	cl_pipe(HANDLE*, HANDLE*, SECURITY_ATTRIBUTES*, DWORD);
extern LONG WINAPI	cl_reg(HKEY, const char*, DWORD, REGSAM , HKEY*);
extern HANDLE WINAPI	cl_mapping(HANDLE,SECURITY_ATTRIBUTES*,DWORD,DWORD,DWORD,const char*);
extern BOOL WINAPI	cl_freeconsole(void);
extern HANDLE		cl_socket(void*, int, int, int, void*, int);
extern int		cl_setsockopt(void*,HANDLE, int, int,const void*, int);
extern HANDLE		cl_sockops(int,void*, HANDLE, void*, int*);
extern int              cl_socket2(int,int, int, int);
extern int              cl_bind2(int, void*, int);
extern int		cl_netevents(void*,HANDLE,HANDLE,void*,size_t);
extern int		cl_getsockinfo(void*,const void*,int,char*,int,char*,int,int,void*);
extern int		cl_getby(int, void*, const void*, size_t, void*, void*, size_t);
extern UINT WINAPI	cl_getconsoleCP(void);

#undef CreateThread
#define CreateThread		(state.clone.hp?cl_createthread:CreateThread)
#undef CreateMutex
#define CreateMutex(a,b,c)	(state.clone.hp?cl_mutex(a,0,b,c):CreateMutexA(a,b,c))
#undef OpenMutex
#define OpenMutex(a,b,c)	(state.clone.hp?cl_mutex(NULL,a,b,c):OpenMutexA(a,b,c))
#undef CreateProcess
#define CreateProcess(a,b,c,d,e,f,g,h,i,j)		(state.clone.hp?cl_createproc(NULL,a,b,c,d,e,f,g,h,i,j):CreateProcessA(a,b,c,d,e,f,g,h,i,j))
#define Fs_open			(state.clone.hp?cl_fsopen:Fs_open)
#undef CreateEvent
#define CreateEvent(a,b,c,d)	(state.clone.hp?cl_event(a,0,b,c,d):CreateEventA(a,b,c,d))
#undef OpenEvent
#define OpenEvent(a,b,c)	(state.clone.hp?cl_event(NULL,a,FALSE,b,c):OpenEventA(a,b,c))
#define SetThreadPriority	(state.clone.hp?cl_threadpriority:SetThreadPriority)
#define CreatePipe		(state.clone.hp?cl_pipe:CreatePipe)
#undef RegOpenKeyEx
#define RegOpenKeyEx		(state.clone.hp?cl_reg:RegOpenKeyExA)
#undef CreateFileMapping
#define CreateFileMapping	(state.clone.hp?cl_mapping:CreateFileMappingA)
#define FreeConsole		(state.clone.hp?cl_freeconsole:FreeConsole)
#define GetConsoleCP		(state.clone.hp?cl_getconsoleCP:GetConsoleCP)

#endif
