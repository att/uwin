/*
 * this initialization occurs before main
 * { environ stdin stdout stderr errno } are in program space
 * so that they can be used in static initialization
 */

#include "uwin_init.h"

#if defined(__BORLANDC__)
#   include	<astwin32.h>
#endif

#include	<limits.h>

extern "C"
{
#ifndef _UWIN_WINDOWS_H
	typedef void* HMODULE;
	typedef int (__stdcall *FARPROC)();
	__declspec(dllimport) FARPROC __stdcall GetProcAddress(void *, const char*);
	__declspec(dllimport) void *__stdcall GetModuleHandleA(const char*); 
#endif

	char**		environ;
#if defined(__DMC__) || defined(__BORLANDC__)
	extern int	errno;
        extern int	exit(int);
#   ifdef  __BORLANDC__
#	define __argv _argv
#   endif
	extern char**	__argv;
#else
#   ifndef errno
	int		errno;
#   endif
#endif
	int		h_errno;
	char		_Sfstdin[SFIO_T_SPACE];
	char		_Sfstdout[SFIO_T_SPACE];
	char		_Sfstderr[SFIO_T_SPACE];
	int		_Sfi;
	int		daylight;
	long		timezone;
	int		_daylight;
	long		_timezone;

	static char standard_time[TZNAME_MAX] = "EST";
	static char daylight_time[TZNAME_MAX] = "EDT";

	char*		tzname[2] =
	{
		standard_time,
		daylight_time
	};   

#ifdef __BORLANDC__

	struct file
	{
		int	dummy[6];
	};

	extern struct file _streams[];
	extern int sfsync(struct file*);

	int fflush(struct file *ptr)
	{
		int i;
		for(i=0;i < 3; i++)
		{
			if(ptr == &_streams[i])
				return(0);
		}
		return(sfsync(ptr));
	}

#endif

	void _uwin_main(void)
	{
		HMODULE hp;
		register struct _astdll *ap;
		struct _astdll *(*getdll)(void);
		int (*runprog)(void*,int, void *,char ***,int *);
		if(!(hp = GetModuleHandleA(POSIX_DLL)))
			return;
		if(!(getdll=(struct _astdll*(*)(void))GetProcAddress(hp,"_ast_getdll")))
			return;
		if(!(runprog=(int(*)(void*,int, void *,char ***,int *))GetProcAddress(hp,"_ast_runprog")))
			return;
		ap = getdll();
		ap->_ast_errno = &errno;
		ap->_ast_tzname = tzname;
		ap->_ast_timezone = &timezone;
		ap->_ast_daylight = &daylight;
		ap->_ast_stdin = (void*)(_Sfstdin);
		ap->_ast_stdout = (void*)(_Sfstdout);
		ap->_ast_stderr = (void*)(_Sfstderr);
		ap->_ast_herrno = &h_errno;    
#if defined(__DMC__) || defined(__BORLANDC__)
		ap->_ast__argv = __argv;    
		ap->_ast_exitfn = (int*)exit;    
#endif
		if(environ==0 && ap->_ast_environ)
			environ = *ap->_ast_environ;
		if(ap->_ast_libversion != 11)
		{
			ap->_ast_libversion = 11;
			ap->_ast_environ = &environ;
			runprog(0,0,0,&environ,&errno);
		}
		else
		{
			void (*ast_init)(void);
			if(!(hp=GetModuleHandleA(AST_DLL)))
				return;
			if(!(ast_init=(void(*)(void))GetProcAddress(hp,"_ast_init")))
				return;
			ast_init();
			if(ap->_ast_environ)
				environ = *ap->_ast_environ;
		}
	}
}

class Fake
{
public:
	Fake(void)
	{
		_uwin_main();
	}
private:
	int x;
};

static Fake _fake;
