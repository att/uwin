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
#define _set_abort_behavior	____set_abort_behavior

#include	"uwin_init.h"
#include	"fsnt.h"
#include	"dlls.h"

#if _DLL
#   include	<stdlib.h>
#   include	<unistd.h>
#   include	<errno.h>
#   include	<time.h>
#   undef tzname
#else
#   define _BLD_DLL 1
#   include	<astwin32.h>
#   undef _BLD_DLL
#   undef _DLL
    extern int		errno;
    extern char**	__argv;
    int 		_daylight;
    long		_timezone;
    char**		_environ;
#endif

#undef	_set_abort_behavior

#include	<limits.h>

extern char     _Sfstdin[],_Sfstdout[],_Sfstderr[];
extern char     *tzname[];
#undef		h_errno
int		h_errno;

char		**opt_argv;
char		_Sfstdin[SFIO_T_SPACE];
char		_Sfstdout[SFIO_T_SPACE];
char		_Sfstderr[SFIO_T_SPACE];
int		_Sfi;

static char standard_time[TZNAME_MAX] = "EST";
static char daylight_time[TZNAME_MAX] = "EDT";

char		*tzname[2] =
{
	standard_time,
	daylight_time
};

extern  int _ast_runprog(void*,int, void *,char ***,int *);
extern struct _astdll *_ast_getdll(void);

#if !defined(_NTSDK)
#   undef environ
    char**	environ;
#endif /* _NTSDK */

#if _DLL
    void _ast_initposix(void)
#else
    void init_libposix(void)
#endif
{
	void *hp, (*exitfn)(void);
	register struct _astdll *ap;
	ap = _ast_getdll();
	environ = _environ;
	ap->_ast_environ = &_environ;
	if(ap->_ast_libversion==10)
		return;
	ap->_ast_timezone = &_timezone;
	ap->_ast_daylight = &_daylight;
	ap->_ast_errno = &errno;
	ap->_ast_tzname = tzname;
	ap->_ast_stdin = (void*)(_Sfstdin);
	ap->_ast_stdout = (void*)(_Sfstdout);
	ap->_ast_stderr = (void*)(_Sfstderr);
	ap->_ast__exitfn = (int*)_exit;
	ap->_ast_exitfn = (int*)exit;
	ap->_ast__argv = __argv;
	ap->_ast_libversion = 10;
	ap->_ast_herrno = &h_errno;
	ap->_ast_msvc = (void*)atexit;
#if _DLL
	_ast_runprog(0,0,0,&_environ,&errno);
#else
	h_errno = (int)_pctype;
#endif
	if((hp = GetModuleHandleA(POSIX_DLL)) && (exitfn=(void(*)(void))GetProcAddress(hp,"_ast_atexit")))
	{
		logmsg(LOG_PROC+5, "_ast_initposix _ast_atexit %p", exitfn);
		atexit(exitfn);
	}
	else
		logmsg(LOG_PROC+5, "_ast_initposix GetModuleHandle hp %p", hp);
}

#if !_BLD_DLL

extern void *malloc(size_t);

void *_nh_malloc(size_t size)
{
	return(malloc(size));
}

#undef errno
int errno;

int _getenv_helper_nolock(void)
{
	return 0;
}

void abort(void)
{
	static void (*abortfn)(void);
	void *hp;
	if(!abortfn && (hp = GetModuleHandleA(POSIX_DLL)))
		abortfn = (void(*)(void))GetProcAddress(hp,"abort");
	if(abortfn)
		(*abortfn)();
}

unsigned int _set_abort_behavior(unsigned int flags, unsigned int mask)
{
	return 0;
}

#endif
