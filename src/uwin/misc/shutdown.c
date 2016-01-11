#include <ast.h>
#include <cmd.h>
#include <error.h>
#include <windows.h>

static const char usage[] =
"[-?@(#)shutdown (AT&T Labs Research) 2000-06-21\n]"
USAGE_LICENSE
"[+NAME?shutdown - shutdown system]"
"[+DESCRIPTION?\bshutdown\b can be use to shutdown the system "
	"and optionally to restart it.  The caller of this program "
	"needs to have the \bSHUTDOWN\b privilege.]"
"[g:grace:=30]#[seconds?\aseconds\a defines the grace period in seconds until "
	"the shutdown will occur.]"
"[i:init]:[init_state:=0?Currently \ainit_state\a can only be \b0\b or "
	"\b6\b.  A value of \b6\b is used to restart the system after boot.]"
"\n"
"\n[0|6]\n"
"\n"
"[+EXIT STATUS?If successful, the system will shutdown or optionally reboot."
	"Otherwise, a non-zero exit status will be returned.]"
;


static void shutdown(int reboot, int grace)
{
	HANDLE hToken;		/* handle to process token */ 
	TOKEN_PRIVILEGES *tkp;	/* pointer to token structure */ 
	char buf[4096];
 
	tkp=(TOKEN_PRIVILEGES*)buf;
 
	/* Get the current process token handle */
	if (!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken)) 
		error(ERROR_exit(1),"OpenProcessToken failed err=%d",GetLastError()); 
             
	/* Get the LUID for shutdown privilege. */
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,&tkp->Privileges[0].Luid); 
	tkp->PrivilegeCount = 1;
	tkp->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
 
	/* Get shutdown privilege for this process. */
	if(!AdjustTokenPrivileges(hToken,FALSE,tkp,0,(PTOKEN_PRIVILEGES)NULL,0))
		error(ERROR_exit(1),"AdjustTokenPrivileges enable failed err=%d",GetLastError()); 
     
	/* Display the shutdown dialog box and start the time-out countdown. */
	if(!InitiateSystemShutdown( NULL,"Click on the main window and press the Escape key to cancel shutdown.", grace, TRUE, reboot))
		error(ERROR_exit(1),"InitiateSystemShutdown failed err=%d",GetLastError()); 
	/* Disable shutdown privilege. */
	tkp->Privileges[0].Attributes = 0; 
	if(!AdjustTokenPrivileges(hToken,FALSE,tkp,0,(PTOKEN_PRIVILEGES)NULL,0))
		error(ERROR_exit(1),"AdjustTokenPrivileges disable failed err=%d",GetLastError()); 
	return;
}

int b_shutdown(int argc, char **argv, void *context)
{
	int n, reboot=0, grace=30;
#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv, context, NULL, 0);
#else
	cmdinit(argv, context, NULL, 0);
#endif
	NoP(argc);
	while((n=optget(argv,usage))) switch(n)
	{
	    case 'g':
		grace = opt_info.num;
		break;
	    case 'i':
		if(*opt_info.arg == '6') reboot=1;
		break;
	    case ':':
		error(2, opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
	if(error_info.errors)
		error(ERROR_usage(2),optusage(NULL));
	shutdown(reboot,grace);
	return(0);
}
