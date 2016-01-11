/* This file is part of UWIN Telnetd source code */
#include <windows.h>
#include <lm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pwd.h>
#include <error.h>
#include <option.h>
#include <uwin.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "getinfo.h"

#define ETC_DENY 	"/etc/login.deny"
#define ETC_ALLOW 	"/etc/login.allow"

#define RTN_OK 0

static char astconfig[] = "_AST_FEATURES=UNIVERSE - att PATH_LEADING_SLASHES - 1 PATH_RESOLVE - metaphysical";
static char lognamebuff[PATH_MAX]      = "LOGNAME=";
static int gpid;

static char domainname[MAXNAME];

extern void utentpty(const char* name, const char* host);

void sighan(int signo)
{
	/* SIGHUP is not terminating the process so SIGKILL */
	kill(gpid,SIGKILL);
	exit(0);
}

int check_allow_file(const char *name)
{
	FILE *fp;
	char string[MAXNAME]="", *cp;
	char username[MAXNAME],userdomain[MAXNAME];
	int domain_wildcard=0, user_wildcard=0,token;
	int local_user_wildcard=0;
	int flag;
	char *cp1,myname[MAXNAME],mydom[MAXNAME]="";

	if((fp=fopen(ETC_ALLOW,"r"))==NULL)
		return -1; //file does not exist

	if((cp1=strchr(name,'/'))==NULL)	//string in username format
	{
		flag=1;
		strcpy(myname,name);
	}
	else
	{
		token = (int)(cp1-name);
		strcpy(mydom,name);
		mydom[token] = '\0';
		strcpy(myname,cp1+1);
		flag=0;
	}

	while(fscanf(fp,"%s",string) != EOF)
	{
		if((cp=strchr(string,'/'))==NULL)
		{
			//string in username format
			if(!_stricmp("*",string))
				local_user_wildcard = 1;
			if((local_user_wildcard || (!_stricmp(myname,string))) && !_stricmp(mydom,""))
			{
				fclose(fp);
				return 1;
			}
			else
				continue;
		}
		strcpy(username,cp+1);
		strcpy(userdomain,string);
		userdomain[cp-string]='\0';
		if(flag && _stricmp(mydom,userdomain))
			continue;

		if(!domain_wildcard && !_stricmp(userdomain,"*"))
			domain_wildcard=TRUE;
		if(!user_wildcard && !_stricmp(username,"*"))
			user_wildcard=TRUE;

		//check domain name: doesnt match
		if(!domain_wildcard && _stricmp(userdomain,mydom))
			continue;		//ignore entry

		//name match
		if(user_wildcard || !_stricmp(username,myname))
		{
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0; //match not found
}

int check_deny_file(const char *name)
{
	FILE *fp;
	char string[MAXNAME]="", *cp;
	char username[MAXNAME],userdomain[MAXNAME];
	int domain_wildcard=0, user_wildcard=0,token;
	int local_user_wildcard=0;
	int flag;
	char *cp1,myname[MAXNAME],mydom[MAXNAME]="";

	if((fp=fopen(ETC_DENY,"r"))==NULL)
		return -1;                      //file does not exist

	if((cp1=strchr(name,'/'))==NULL)	//string in username format
	{
		flag=1;
		strcpy(myname,name);
	}
	else
	{
		token = (int)(cp1-name);
		strcpy(mydom,name);
		mydom[token] = '\0';
		strcpy(myname,cp1+1);
		flag=0;
		if(!_stricmp(myname,"administrator") && !_stricmp(mydom,domainname))
		{
			fclose(fp);
			return 2;
		}
	}

	while(fscanf(fp,"%s",string) != EOF)
	{
		if((cp=strchr(string,'/'))==NULL)
		{
			//string in username format
			if(!_stricmp("*",string))
				local_user_wildcard = 1;
			if((local_user_wildcard || (!_stricmp(myname,string))) && !_stricmp(mydom,""))
			{
				fclose(fp);
				return 1;
			}
			else
				continue;
		}
		strcpy(username,cp+1);
		strcpy(userdomain,string);
		userdomain[cp-string]='\0';
		if(flag && _stricmp(mydom,userdomain))
				continue;

		if(!domain_wildcard && !_stricmp(userdomain,"*"))
			domain_wildcard=TRUE;
		if(!user_wildcard && !_stricmp(username,"*"))
			user_wildcard=TRUE;

		//check domain name: doesnt match
		if(!domain_wildcard && _stricmp(userdomain,mydom))
			continue;	//ignore entry

		//name match
		if(user_wildcard || !_stricmp(username,myname))
		{
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0; //match not found
}

int state_table(const char *name)
{
	int flag=FALSE;
	WKSTA_INFO_100 *wks_info;
	wk_getinfo_def wk_getinfo;
	api_buffer_free_def api_buffer_free;
	HMODULE hLib = NULL;
	
	wk_getinfo = (wk_getinfo_def)getapi_addr("netapi32.dll","NetWkstaGetInfo",&hLib);
	api_buffer_free = (api_buffer_free_def)getapi_addr("netapi32.dll","NetApiBufferFree",&hLib);
	if (wk_getinfo != NULL && api_buffer_free != NULL)
	{
		(*wk_getinfo)(NULL, 100, (unsigned char **)&wks_info);
		wcstombs(domainname, (const unsigned short*)wks_info->wki100_langroup, sizeof(domainname));
		(*api_buffer_free)((LPVOID)wks_info);
	}
	if (hLib != NULL)
	{
		FreeLibrary(hLib);
	}
		
	if(!wk_getinfo)
		return(1);

	//administrator always has access
	if(!_stricmp(name,"administrator"))
		return 1;

	switch(check_deny_file(name)) 	//check login.deny file
	{
		case 1:		//name found
			return 0;	//login denied
		case 2:	
			return 1;	//administrator... allowed
		case -1:		//file not found
		case 0:			//name not found
		{
			switch(check_allow_file(name)) //check login.allow file
			{
				case -1:	//file not found
				case  0: 	//name not found
					break;	//use DEFAULT condition
				case  1: 	//name found
					return 1;	//allow access
			}
		}
	}
	return 0;		//DEFAULT: deny access
}


HANDLE logon(char **ret, const char *peername, int win9x, char **passp)
{
	struct termios old, new;
	struct passwd *pwent;
	char *pass, *conv, system[MAXNAME],*sysname=NULL;
	static char buf[MAXNAME], username[MAXNAME], name[MAXNAME], dom[PATH_MAX];
	int ch, fromlen;
	HANDLE atok = INVALID_HANDLE_VALUE;
	int		sid[64];
	int		sidlen=sizeof(sid), domlen=sizeof(dom), namelen=sizeof(name);
	SID_NAME_USE puse;
	struct sockaddr_in from;

	tcgetattr(0, &old);
	new=old;
	new.c_lflag |= (ECHO|ECHOE|ECHOK|ECHONL);
	tcsetattr(0, TCSAFLUSH, &new);
	if(*ret)
		strncpy(name,*ret,namelen);
	else
	{
		*ret=name;
		sprintf(buf, "login: ");
		while(1)
		{
			write(1, buf, strlen(buf));
			conv=name;
			while ((ch=getchar()) != EOF)
			{
				if(ch == '\n')
					break;
				*conv++=ch;
			}
			*conv='\0';
			if(conv!=name)
				break;
			if(ch==EOF)
				exit(1);
		}
	}
	tcsetattr(0, TCSADRAIN, &old);
	pass=getpass("Password: ");
	if(passp)
		*passp = pass;

	if (mangle(name,system,sizeof(system)))
	{
		/*
		 * Code to check for user name in login.allow & login.deny
		 */
		if (state_table(name))
		{
			if (win9x)
			{
				atok = (HANDLE)1;
			}
			else
			{
				if (system[0] != '\0')
				{
					sysname = system;
				}

				strcpy(username,name);
				if ((conv = strchr(username,'/')) != NULL)
				{
					conv++;
				}
				else
				{
					if(pwent=getpwnam(name))
					{
						if(uwin_uid2sid(pwent->pw_uid,(Sid_t*)sid,sizeof(sid)))
						{
							sidlen = sizeof(username);
							if(!LookupAccountSid(NULL, sid, username, &sidlen, dom, &domlen, &puse))
							{
								strcpy(username,name);
							}
						}
					}
					conv = username;
					sidlen = sizeof(sid);
					domlen = sizeof(dom);
				}
				if(strchr(name,'/'))
					seteuid(1);            

				if (LookupAccountName(sysname, conv, sid, &sidlen, dom, &domlen, &puse))
				{
					if(!(atok=uwin_mktoken(username,pass,0)))
					{
						if(GetLastError()==ERROR_LOGON_TYPE_NOT_GRANTED)
							printf("Interactive logon for this user prohibited by local policy\n");
						atok = INVALID_HANDLE_VALUE;
					}
					else
					{
						if(!peername)
						{
							fromlen = sizeof(from);
							if (getpeername(20, (struct sockaddr *)&from, &fromlen) < 0)
							{
								printf("getpeername error: %ld\n",errno);
								exit(3);
							}
							peername = inet_ntoa(from.sin_addr);
						}
						setuid(0);
						utentpty(name,peername);
					}
				}
				else
				{
					printf("Invalid User\n");
				}
			}
		}
		else
		{
			printf("Access denied\n");
		}
	}
	else
	{
		printf("Invalid domain account\n");
	}

	return(atok);
}

extern char **environ;

int main(int argc, char *argv[])
{
	HANDLE atok=0;
	char *pass=0, *unixhomebuf, *myargv[]={"-ksh", NULL}, **myenv;
	char *name, *peername=0;
	char **envp = environ;
	struct passwd *pwent;
	struct utsname ut[1];
	int i, total,Iswin95, fflag=0;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(osinfo);
	
	fcntl(20,F_SETFD,FD_CLOEXEC);
	signal(SIGCHLD,SIG_DFL);
	signal(SIGINT,SIG_DFL);
	signal(SIGQUIT,SIG_DFL);
	for (total=0; envp[total]; ++total);
	/*
	 * Copy the environment variable to a separate buffer and
	 * allocate extra space for HOME
	*/
	while((i = optget(argv,"pfh:[host] [name]"))) switch(i)
	{
	    case 'p':
		break;
	    case 'h':
		peername = opt_info.arg;
		break;
	    case 'f':
		fflag = 1;
		break;
	    case ':':
		error(2, opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	name = argv[0];
	myenv = ( char ** )malloc( sizeof( char ** ) * ( total + 4 ) );
	for(i=0; envp[i]; i++)
	{
		myenv[i] = (char*)strdup(envp[i]);
	}
	myenv[total++] = astconfig;
	myenv[total++] = lognamebuff;
	myenv[total] = 0;
	uname(ut);
	signal(SIGHUP, sighan);


	if(!GetVersionEx(&osinfo))
	{
		printf("Unable to get System Information \n");
		exit(0);
	}

	if (VER_PLATFORM_WIN32_NT == osinfo.dwPlatformId)
	{
		Iswin95 = 0;
	}
	else
		Iswin95 = 1;

	if(!peername)
	{
		printf("Welcome to %s UWIN System \n", ut->sysname);
		printf("%s  Version %s (%s)\n", ut->sysname, ut->release,ut->nodename);
	}
	for (i=0; i<5; ++i)
	{
		alarm(60); //wait for 60 seconds for no response
		if (fflag || (atok=logon(&name,peername,Iswin95,&pass))!=INVALID_HANDLE_VALUE)
		{
			strcpy(&lognamebuff[8], name);
			if((pwent=getpwnam(name))==NULL)
			{
				printf("UWIN: Account entry missing in /etc/passwd for %s\n",name);
				continue;
			}
			unixhomebuf = ( char * )malloc(strlen(pwent->pw_dir) + 6);
			strcpy( unixhomebuf, "HOME=" );
		again:
			if (chdir(pwent->pw_dir) == -1)
			{
				if(pass && pwent->pw_dir[1]=='/' && pwent->pw_dir[0]=='/')
				{
					char remote[256],*cp,*dp=remote;
					for(cp=pwent->pw_dir; *cp; cp++,dp++)
					{
						if(*cp=='/')
							*dp = '\\';
						else
							*dp = *cp;
					}
					*dp = 0;
					seteuid(pwent->pw_uid);
					if(WNetAddConnection(remote,pass,NULL)==NO_ERROR)
						sleep(1);
					setuid(0);
					pass = 0;
					goto again;
				}
				strcat(unixhomebuf, "/tmp");
				chdir("/tmp");
				printf("UWIN: Could not change to home directory. Logging in with HOME=/tmp\n");
			}
			else
				strcat(unixhomebuf, pwent->pw_dir);
			/*
			 * Add unixhomebuf to environment
			 */
			myenv[ total] = (char*)strdup( unixhomebuf );
			myenv[ total+1] = (char*)NULL;
			alarm(0);
			if(Iswin95 || fflag)
			{
				if (atok && !Iswin95)
					CloseHandle(atok);
				setuid(0);
				utentpty(name,peername);
				if(fflag)
				{
					if(setuid(pwent->pw_uid)<0)
					{
						printf("Can't set uid to %d.\nCheck UCS service for user %s.\n",pwent->pw_uid,pwent->pw_name);
						fflush(stdout);
						return(126);
					}
				}
				execve(pwent->pw_shell, myargv, myenv);
			}
			else
			{
				struct spawndata sdata;
				memset(&sdata, '\0', sizeof(sdata));
				sdata.tok = atok;
				sdata.flags = UWIN_SPAWN_EXEC;
				uwin_spawn(pwent->pw_shell,myargv,myenv,&sdata);
			}
			fprintf(stderr,"UWIN: Could not exec %s\n", pwent->pw_shell);
			return(127);
			break;
		}
		else
			printf("Login incorrect\n");
		name = 0;
	}
	return(0);
}

