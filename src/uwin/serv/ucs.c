#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <unistd.h>
#include <termios.h>
#include <netdb.h>
#include <uwin.h>
#include <errno.h>
#include <fcntl.h>
#include <uwin_serve.h>
#include "getinfo.h"

#define RTN_OK		0
#define RTN_ERROR	13

#define MAX_WAIT_TIME	3000

static char ucs_version_id[] = "\n@(#)ucs 2011-06-09\0\n";

SERVICE_STATUS status;
SERVICE_STATUS_HANDLE statushandle;

HANDLE event = NULL;

char servname[256], servdname[256]; 

void WINAPI csmain(DWORD argc, LPTSTR *argv);
void WINAPI csctrl(DWORD ctrlcode);

void getservname(void);

static int installservice(char *account,char *pass);
static int deleteservice(char *);

static int quiet;
char evename[32];
HANDLE evehan;
char buffer[80];
DWORD siz = 80;

void eventlog(char *msg);
BOOL scmstatus(DWORD state, DWORD exit, DWORD hint);

static void error(int level, char* fmt, ...) 
{
	DWORD	err = GetLastError();
	char*	cur;
	char*	end;
	char*	s;
	int	n;
	char	buf[512];
	va_list ap;

	va_start(ap, fmt);
	cur = buf;
	end = cur + sizeof(buf) - 1;
	cur += sfvsprintf(cur, end - cur, fmt, ap);
	va_end(ap);
	logerr(LOG_SYSTEM+level, "%s", buf);
	if (!quiet)
	{
		if ((level & LOG_SYSTEM) && err && (int)(end - cur) > 32)
		{
			cur += sfsprintf(cur, end - cur, "\r\n[%lu:", err);
			if (!(n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, err, 0, cur, (int)(end - cur), 0)))
				n = sfsprintf(cur, end - cur, "Unknown error code %d.\n", err);
			if (n >= (int)(end - cur))
				cur[n-1] = 0;
			if (s = strchr(cur, '\n'))
				n = (int)(s - cur);
			cur += n;
			if (*(cur-1) == '\r')
				cur--;
			if (*(cur-1) == '.')
				cur--;
			if (cur < end)
				*cur++ = ']';
		}
		*cur = 0;
		MessageBox(0, buf, "UWIN Setuid Service", 0x00200000L|0x00040000L|MB_OK|MB_ICONSTOP);

	}
}

static void eventlog(char *msg)
{
    	HANDLE	h;
	char*	sa[2];
	char	errmsg[256];

	if (h = RegisterEventSource(NULL, servname))
	{
		sfsprintf(errmsg, sizeof(errmsg), "%s error: %d", servname, GetLastError());
		sa[0] = errmsg;
		sa[1] = msg;
		ReportEvent(h, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, sa, NULL);
		DeregisterEventSource(h);
	}
}

int main(int argc, char** argv)
{
	char*			op;
	char*			s;
	int			n;
	char			name[80];
	char			passwd[PATH_MAX];
	SERVICE_TABLE_ENTRY	table[] = 
	{
		{NULL, (LPSERVICE_MAIN_FUNCTION)csmain},
		{NULL, NULL}
	};
	OSVERSIONINFO		osinfo;

	log_command = "ucs";
	log_level = 1;
	osinfo.dwOSVersionInfoSize = sizeof(osinfo); 
	if (!GetVersionEx(&osinfo) || osinfo.dwPlatformId != VER_PLATFORM_WIN32_NT) 
	{
		logmsg(LOG_STDERR+0, "not supported for this windows release");
		return 1;
	}
	logopen(log_command, 0);
	if ((s = *++argv) && (!_stricmp(s, "--quiet") || !_stricmp(s, "-q") || !_stricmp(s, "quiet")))
	{
		quiet = 1;
		s = *++argv;
	}
	if ((op = s) && _stricmp(op, "start"))
	{
		if (s = *++argv)
		{
			strncpy(name, s, sizeof(name));
			s = *++argv;
		}
		else
		{
			sfprintf(sfstderr, "Enter Account Name: ");
			sfscanf(sfstdin, "%s", name);
		}
		if (!_stricmp(op, "install"))
		{
			if (s && !strcmp(s, "-"))
			{
				passwd[0] = 0;
				if ((n = read(0, passwd, sizeof(passwd))) < 0)
				{
					logerr(LOG_STDERR+0, "read error");
					return 1;
				}
				passwd[n] = 0;
				s = passwd;
			}
			if (!installservice(name, s))
			{
				logerr(LOG_ALL+0, "install %s failed", name);
				return 1;
			}
			logmsg(1, "installed %s", name);
		}
		else if (!_stricmp(op, "delete"))
		{
			if (!deleteservice(name))
			{
				logerr(LOG_ALL+0, "delete %s failed", name);
				return 1;
			}
			logmsg(1, "deleted %s", name);
		}
		else
		{
			logmsg(LOG_STDERR+LOG_USAGE+0, "[ --quiet ] [ start | [ delete | install [ name [ password ] ] ] ]");
			return 2;
		}
	}
	else
	{
		getservname();
		table[0].lpServiceName = servname;
		if (!StartServiceCtrlDispatcher(table))
			eventlog("StartServiceCtrlDispatcher failed");
	}
	logclose();
	return 0;
}

int Check_UMS_Running(void)
{
	HANDLE hSerM, hSer;
	SERVICE_STATUS s_stat;
	int ret = 0;

	if ((hSerM = OpenSCManager(NULL,NULL,GENERIC_READ)) != NULL)
	{
		if ((hSer = OpenService(hSerM,UMS_NAME,SERVICE_QUERY_STATUS)) != NULL)
		{
			if (!QueryServiceStatus(hSer,&s_stat))
				logerr(1, "QueryServiceStatus in Check_UMS_Service");
			else if (s_stat.dwCurrentState == SERVICE_RUNNING)
				ret = 1;
			CloseServiceHandle(hSer);
		}
		else
			logerr(1, "OpenService in Check_UMS_Service");
		CloseServiceHandle(hSerM);
	}
	else
		logerr(1, "OpenSCManager in Check_UMS_Service");
	return(ret);
}

void WINAPI csmain(DWORD argc, LPTSTR *argv)
{
	HANDLE pipehandle, atok;
	DWORD len;
	UMS_slave_data_t sr;
	SECURITY_ATTRIBUTES sa;
	BOOL (PASCAL *duptok)(HANDLE,DWORD,LPSECURITY_ATTRIBUTES,SECURITY_IMPERSONATION_LEVEL,TOKEN_TYPE,PHANDLE);
	char c;
	HMODULE hp = NULL;

	log_command = "ucs";
	log_level = 1;
	logopen(log_command, 0);
	getservname();
	logmsg(1, "startup %s %s", servname, &ucs_version_id[9]);

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = 0;
	sa.bInheritHandle = FALSE;

	statushandle = RegisterServiceCtrlHandler(TEXT(servname), csctrl);

	if (statushandle == (SERVICE_STATUS_HANDLE)0)
	{
		eventlog("RegisterServiceCtrlHandler failed");
		return;
	}
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwServiceSpecificExitCode = 0;

	scmstatus(SERVICE_RUNNING, NO_ERROR, 0);

	if (Check_UMS_Running())
	{
		if (WaitNamedPipe(UWIN_PIPE_TOKEN, MAX_WAIT_TIME))
		{
			pipehandle = CreateFile(UWIN_PIPE_TOKEN, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (pipehandle != INVALID_HANDLE_VALUE)
			{
				sr.pid = GetCurrentProcessId();
				if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_DEFAULT|TOKEN_ADJUST_GROUPS|TOKEN_ADJUST_PRIVILEGES|TOKEN_EXECUTE|TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE|TOKEN_READ|TOKEN_WRITE|TOKEN_QUERY_SOURCE, &atok))
				{
					if(!(duptok = (BOOL (PASCAL*)(HANDLE,DWORD,SECURITY_ATTRIBUTES*,SECURITY_IMPERSONATION_LEVEL,TOKEN_TYPE,HANDLE*))getapi_addr("advapi32.dll","DuplicateTokenEx",&hp)))
						error(1, "DuplicateTokenEx function not found");
					else
					{
						if(duptok && (*duptok)(atok, MAXIMUM_ALLOWED, &sa, SecurityAnonymous, TokenPrimary, &sr.atok))
						{
							if (WriteFile(pipehandle, &sr, sizeof(sr), &len, NULL))
							{
								FlushFileBuffers(pipehandle);
								ReadFile(pipehandle,&c,sizeof(char),&len,NULL);
								Sleep(10);
							}
							else
								error(1, "WriteFile failed on TOKEN PIPE");
						}
						else
							error(1, "DuplicateTokenEx failed");
						CloseHandle(atok);
					}
				}
				else
				{
					GetUserName(buffer,&siz);
					error(1, "Unable to open my own process token");
				}
				CloseHandle(pipehandle);
			}
			else
				error(1, "Unable to connect to token pipe");
		}
		else
			error(1, "WaitNamedPipe failed");
	}
	else
		error(1, "UWIN Master service is not running");
	if (hp)
		FreeLibrary(hp);
	logclose();
	scmstatus(SERVICE_STOPPED, NO_ERROR, 0);
}

#define MAXNAME	80

void getservname(void)
{
	DWORD ret,i;
	HANDLE tok;
	char tmpbuf[1024], UserName[256], RefDomain[256];
	PTOKEN_USER UserToken = (PTOKEN_USER)tmpbuf;
	DWORD RetLen=1024, UserNameLen, RefDomainLen;
	SID_NAME_USE SidType;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok))
	{
		if ((ret = GetTokenInformation(tok, TokenUser, UserToken, RetLen, &RetLen)))
		{
			UserNameLen = sizeof(UserName);
			RefDomainLen = sizeof(RefDomain);
			if ((ret = LookupAccountSid(NULL,UserToken->User.Sid,UserName,&UserNameLen,RefDomain,&RefDomainLen,&SidType)))
			{
				strcat(RefDomain,"/");
				strcat(RefDomain,UserName);
				parse_username(RefDomain);
				sfsprintf(servname, sizeof(servname),"UWIN_CS%s", RefDomain);
				sfsprintf(servdname, sizeof(servdname),"UWIN Client(%s)", RefDomain);
				strcpy(evename,RefDomain);
				strcat(evename,"event");
				for(i=0;i<strlen(servname);i++)
					if(servname[i] == '/')
						servname[i]= '#';
				{
					unsigned char *ptr;
					for(ptr=(unsigned char *)evename; *ptr ; ptr++)
						*ptr=tolower(*ptr);
				}
			}
			else
				logerr(1, "LookupAccountSid failed in getservname");
		}
		else
			logerr(1, "GetTokenInformation failed in getservname");
	}
	else
		logerr(1, "OpenProcessToken failed in getservname");
}

void WINAPI csctrl(DWORD ControlCode)
{
	status.dwCurrentState = SERVICE_RUNNING;
	switch (ControlCode)
	{
		case SERVICE_CONTROL_STOP:
			scmstatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
			SetEvent(event);
			return;

		case SERVICE_CONTROL_PAUSE:
		case SERVICE_CONTROL_CONTINUE:
		case SERVICE_CONTROL_INTERROGATE:
		case SERVICE_CONTROL_SHUTDOWN:
			break;

		default:
			break;
	}

	scmstatus(status.dwCurrentState, NO_ERROR, 0);

	return;
}

int verifylogin(char *user, char *domain, char *passwd)
{
	HANDLE loginpipe;
	char info[257*3];
	int ret = 0;
	DWORD dwRead,dwWrite,value;

	if ((loginpipe = CreateFile(UWIN_PIPE_LOGIN,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL)) != INVALID_HANDLE_VALUE)
	{
		strcpy(info,user);
		strcat(info,"+");
		strcat(info,domain);
		strcat(info,"+");
		strcat(info,passwd);

		if (!WriteFile(loginpipe,(BYTE*)info,(DWORD)strlen(info),&dwWrite,NULL))
			logerr(LOG_STDERR+1, "WriteFile failed");
		else if (!ReadFile(loginpipe,(BYTE*)&value,sizeof(value),&dwRead,NULL) || dwRead == 0)
			logerr(LOG_STDERR+1, "ReadFile failed");
		else
			switch (value)
			{
			case -1:
				logmsg(LOG_STDERR+1, "Logon is not implemented in this version of windows");
				break;
			case 0:
				ret = 1;
				break;
			case ERROR_LOGON_TYPE_NOT_GRANTED:
				logmsg(LOG_STDERR+1, "Interactive logon for %s on %s prohibited by local policy", user, domain);
				break;
			default:
				logmsg(LOG_STDERR+1, "Incorrect Password");
				break;
			}
		CloseHandle(loginpipe);
	}
	else
		logmsg(LOG_STDERR+LOG_SYSTEM+0, "CreateFile %s failed", UWIN_PIPE_LOGIN);
	return ret;
}

static int installservice(char *account,char *pass)
{
	SC_HANDLE service, scm;
	TCHAR path[512], system[80], name[80], username[80];
	char userpass[256], user[256], dom[256]=".";
	int i,ret = 0;
	pid_t pid = -1;

	if(!GetModuleFileName(NULL, path, 512))
	{
		logmsg(LOG_STDERR+LOG_SYSTEM+0, "Unable to get executable name");
		return(0);
	}
	strcpy(name, account);
	if(!pass)
		pass = getpass("Password: ");
	strcpy(userpass,pass);
	strcpy(username, name);
	if (mangle(name, system,sizeof(system)))
	{
		int	sid[1024];
		int	sidlen=1024, domlen=256; 
		SID_NAME_USE puse;
		char *sep,*sysname=NULL,domain[256];
		int len;

		len = (int)strlen(userpass);
		sfsprintf(servname,sizeof(servname),"UWIN_CS%s", name);
		sfsprintf(servdname,sizeof(servdname),"UWIN Client(%s)", name);
		for(i=0;i<(signed)strlen(servname);i++)
			if(servname[i] == '/')
				servname[i]= '#';
	
		if ((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
		{
			if((service=OpenService(scm,TEXT(servname),SERVICE_QUERY_STATUS)) == NULL)
			{
				// If service does not exist then install.
				if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
				{
					strcpy(user,name);
					if((sep = strchr(name,'/')) != NULL)
					{
						*sep = '\0';
						strcpy(dom,name);
						strcpy(user,sep+1);
						*sep = '\\';
						setuid(1);							
					}

					if (system[0] != '\0')
					{
						sysname = system;
					}
					
					if (LookupAccountName(sysname,user,sid,&sidlen,domain,&domlen,&puse))
					{
						if (verifylogin(user,dom,userpass))
						{
							if ((pid = spawnl("/usr/etc/priv.exe","priv.exe",name,"1",0)) > 0)
								wait(&ret);
							ret = 0;
							strcat(dom,"\\");
							strcat(dom,user);
							if ((service = CreateService(scm,servname,servdname,SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path,NULL,NULL,NULL,dom,userpass)) != NULL)
							{
								logmsg(LOG_STDERR+1, "%s installed", servdname);
								CloseServiceHandle(service);
								ret=1;
							}
							else
								error(LOG_ALL+1, "CreateService failed for user %s", name);
						}
					}
					else
						logerr(1, "Invalid UserName %s", username);
				}
			}
			else
			{
				CloseServiceHandle(service);
				ret = 1;
			}
			CloseServiceHandle(scm);
		}
		else
			error(1, "OpenSCManager failed");
	}
	else
		logmsg(LOG_STDERR+0 ,"Invalid domain account %s", username);
	return(ret);
}

static int deleteservice(char *account)
{
	SC_HANDLE service, scm;
	int ret=1, i;
	char username[80];

	strcpy(username, account);
	
	parse_username(username);

	sfsprintf(servname, sizeof(servname),"UWIN_CS%s", username);
	sfsprintf(servdname, sizeof(servdname),"UWIN Client(%s)", username);

	for(i=0;i<(signed)strlen(servname);i++)
		if(servname[i] == '/')
			servname[i]= '#';

	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (scm)
	{
		service = OpenService(scm, servname, SERVICE_ALL_ACCESS);
		if (service)
		{
			if(DeleteService(service))
				logmsg(1, "%s deleted", servdname);
			else
			{
				logerr(LOG_STDERR+0, "delete %s failed", servdname);
				ret = 0;
			}
		}
		else
		{
			logerr(LOG_STDERR+0, "%s service open failed", servdname);
			ret = 0;
		}
		CloseServiceHandle(scm);
	}
	else
	{
		logerr(LOG_STDERR+0, "OpenSCManager failed");
		return(0);
	}

	return(ret);
}


BOOL scmstatus(DWORD State, DWORD ExitCode, DWORD Hint)
{
	static CheckPoint = 1;
	BOOL Result = TRUE;

	if (State == SERVICE_START_PENDING)
		status.dwControlsAccepted = 0;
	else
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	status.dwCurrentState = State;
	status.dwWin32ExitCode = ExitCode;
	status.dwWaitHint = Hint;

	if ((State == SERVICE_RUNNING) || (State == SERVICE_STOPPED))
		status.dwCheckPoint = 0;
	else
		status.dwCheckPoint = CheckPoint++;

	Result = SetServiceStatus(statushandle, &status);
	if (Result == FALSE)
		eventlog("SetServiceStatus failed");
	return(Result);
}

BOOL Win32Error(DWORD err, LPSTR str1)
{
	LPVOID temp;
	LPVOID local;
	if(FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
					NULL,
					err,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
					(LPTSTR) &temp,
					0,
					NULL))
	{
		if(IsBadStringPtr(str1,sizeof(str1)+1))
			return(-1);

		local = (char *)malloc(strlen(str1)+strlen(temp)+1);
		if(!local)
			return(-1);
		strcpy(local,str1);
		strcat(local,temp);
		MessageBox(NULL,local,"UWIN Setuid Service",0x00200000L| 0x00040000L |MB_OK|MB_ICONSTOP);
		LocalFree(temp);
		free(local);
		return(1);
	}
	else
		return(-1);
}
