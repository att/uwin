#pragma prototyped
/*
 * passwd.c
 * Written by David Korn
 * Wed Dec 18 09:01:52 EST 1996
 */

static const char usage[] =
"[-?@(#)passwd (AT&T Labs Research) 1999-11-10\n]"
USAGE_LICENSE
"[+NAME?passwd - change user password or shell or home directory]"
"[+DESCRIPTION?\bpasswd\b changes the password or shell associated with "
	"the user given by \aname\a or the current user if \aname\a "
	"is omitted.  By default, the current password is prompted for, "
	"and a new one must be entered and confirmed."
	"\bpasswd\b command can also be used to change the home directory "
	"where the \apath\a stands for the home directory.]"
"[s:shell?Change the login shell instead of the password.  The shell "
	"must be one of those listed in \b/etc/shells\b.]"
"[h:home]:[home?Change the home directory.  If the \ahome\a is an empty "
	"string the home directory will be cleared.   Otherwise \ahome\a "
	"can be in either the Windows format or Unix format.]"
"\n"
"\n[user_name]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Successful completion.]"
        "[+>1?An error occurred.]"
"}"
"[+SEE ALSO?\bid\b(1), \blogin\b(1)]"
;

#include	<cmd.h>
#include	<pwd.h>
#include	<windows.h>
#include	<lm.h>
#include	<errno.h>
#include	<uwin.h>

#define SHELL_FILE_PATH "/etc/shells"

#undef PASS_MAX
#define PASS_MAX	32
#define MAX_SHELLS	10

#include	<windows.h>
#define _FILE_DEFINED	1
#include	<wchar.h>

    static FARPROC getapi_addr(const char *lib, const char *sym)
    {
	char sys_dir[PATH_MAX];
	FARPROC addr;
	HMODULE hp;
	int len;
	len = GetSystemDirectory(sys_dir, PATH_MAX);
	sys_dir[len++] = '\\';
	strcpy(&sys_dir[len], lib);
	if(!(hp = LoadLibraryEx(sys_dir,0,0)))
		return(0);
	addr = GetProcAddress(hp,sym);
	return(addr);
    }

    static int pwchange(const char *name,const char *oldpass,const char *newpass)
    {
	int m,n;
	int status;
	const char *usr;
	wchar_t *domain=NULL,dbuf[256],user[32], oldp[PASS_MAX], newp[PASS_MAX];
	int (WINAPI *netfn)(wchar_t*,wchar_t*,wchar_t*,wchar_t*);
	netfn = (int (WINAPI*)(wchar_t*,wchar_t*,wchar_t*,wchar_t*))getapi_addr("netapi32.dll","NetUserChangePassword");
	if(!netfn)
		return(0);
	if(name && (usr = strrchr(name,'/')))
	{
		register char *cp = (char*)user;
		memcpy(cp,name,usr-name);
		cp[usr-name] = 0;
		usr++;
		for(; *cp; cp++)
		{
			if(*cp=='/')
				*cp= '\\';
		}
		mbstowcs(domain=dbuf,(char*)user,sizeof(dbuf));
	}
	else
		usr = name;
	if(usr)
	{
		mbstowcs(user, usr,sizeof(user));
		usr = (char*)user;
	}
	m=(int)mbstowcs(oldp, oldpass,sizeof(oldp));
	n=(int)mbstowcs(newp, newpass,sizeof(newp));
	status = (*netfn)(domain,(wchar_t*)usr,oldp,newp);
	if(status)
	{
		if(status==ERROR_INVALID_PASSWORD)
			error(ERROR_exit(1),"invalid passwd - password unchanged.");
		else
			error(ERROR_exit(1),"failed errno=%d - password unchanged.,status");
	}
	return(1);
    }

static int chhome(const char *homedir, char *name)
{
	int (WINAPI *netfn)(wchar_t*,wchar_t*,unsigned char**);
	int (WINAPI *netsetfn)(wchar_t*,wchar_t*,DWORD,unsigned char*,DWORD*);
	char user_name[64],username[64],machine_name[64],domain_name[64];
	char winhome[64];
	struct passwd *pwd;
	USER_INFO_1006 *uinfo=(USER_INFO_1006 *)NULL;  //home directory of the user
	NET_API_STATUS status;
	char *ptr;
	DWORD parm_err;
	wchar_t user[64],domain[64],*machine=(wchar_t *)NULL,home[256];

	netfn = (int (WINAPI*)(wchar_t*,wchar_t*,unsigned char**))getapi_addr("netapi32.dll","NetGetDCName");
	netsetfn = (int (WINAPI*)(wchar_t*,wchar_t*,DWORD,unsigned char*,DWORD*))getapi_addr("netapi32.dll","NetUserSetInfo");
	if(!netsetfn)
		return(0);
	strcpy(user_name,name);
	strcpy(username,user_name);
	if((pwd =  getpwnam(user_name)) == NULL)
	{
		sfprintf(sfstderr,"Unable to get password entry for %s. \n",name);
		return(1);
	}
	if((ptr = strchr(user_name,'/'))!=NULL)
	{
		strcpy(username,ptr+1);
		*ptr  = '\0';
		strcpy(domain_name,user_name);
		mbstowcs(domain,domain_name,sizeof(domain_name));
		//Get domain controller's name.
		if((*netfn)(NULL,(LPWSTR)domain,(LPBYTE *)&machine) != NERR_Success)
		{	
			sfprintf(sfstderr," Not able to get domain controller \n");
			return(1);
		}
		wcstombs(machine_name,machine,sizeof(machine_name));
	}
	else
	{	
		strcpy(machine_name,"");
		mbstowcs(machine,machine_name,sizeof(machine_name));
	}
	if(!_stricmp(pwd->pw_dir, homedir)) 
	{
		sfprintf(sfstderr," Home requested is the same as the old one \n");
		return (1);
	}
	uwin_path(homedir,winhome,sizeof(winhome));
	mbstowcs(home,winhome,sizeof(home));
	uinfo = (USER_INFO_1006 *)malloc(sizeof(USER_INFO_1006));
	uinfo->usri1006_home_dir = home;
	mbstowcs(user,username,sizeof(username));
	if(((status = (*netsetfn)(machine,(LPWSTR)user,1006,(LPBYTE)uinfo,&parm_err)) != NERR_Success)|| (parm_err!=0UL))
	{
		sfprintf(sfstderr,"NetUserSetInfo failed .... status=%d\n",status);
		switch(status)
		{
			case NERR_NotPrimary :
				sfprintf(sfstderr," Allowed only on primary domain controller \n");
				break;
			case NERR_BadPassword :
				sfprintf(sfstderr," Bad Password \n");
				break;
			case NERR_PasswordTooShort :
				sfprintf(sfstderr," Password too short \n");
				break;
			case NERR_LastAdmin :
				sfprintf(sfstderr," Operation not allowed on last administrator \n");
				break;
			case NERR_SpeGroupOp :
				sfprintf(sfstderr," Operation not allowed on specified special group \n");
			case ERROR_ACCESS_DENIED :
				sfprintf(sfstderr," Access denied \n");
				break;
			case NERR_InvalidComputer :
				sfprintf(sfstderr," Invalid Computer Name \n");
				break;
			case NERR_UserNotFound :
				sfprintf(sfstderr," User name Not found \n");
				break;
		}
		return (1);	
	}
	sfprintf(sfstdout,"%s home directory changed from %s to %s\n",name,pwd->pw_dir,homedir);
	return 0;
}

static int shell(const char *user)
{
	int (WINAPI *netfn)(wchar_t*,wchar_t*,unsigned char**);
	int (WINAPI *netgetfn)(wchar_t*,wchar_t*,DWORD,unsigned char**);
	int (WINAPI *netsetfn)(wchar_t*,wchar_t*,DWORD,unsigned char*,DWORD*);
	char *news,*olds;
	char user_name[64],shells[MAX_SHELLS][64];
	struct passwd *password;
	USER_INFO_10 *user_info;
	USER_INFO_1007 *uinfo;
	int num_of_shells,i=0;
	NET_API_STATUS status;
	FILE *fp;
	DWORD parm_err;
	char *ptr;
	char temp[256], temp_str[256],domain_name[64],machine_name[64];
	wchar_t username[64],comment[256],domain[64],*machine;

	netfn = (int (WINAPI*)(wchar_t*,wchar_t*,unsigned char**))getapi_addr("netapi32.dll","NetGetDCName");
	netgetfn = (int (WINAPI*)(wchar_t*,wchar_t*,DWORD,unsigned char**))getapi_addr("netapi32.dll","NetUserGetInfo");
	netsetfn = (int (WINAPI*)(wchar_t*,wchar_t*,DWORD,unsigned char*,DWORD*))getapi_addr("netapi32.dll","NetUserSetInfo");
	sfprintf(sfstdout,"\tShells available: \n");
	if(fp = fopen(SHELL_FILE_PATH,"r"))
	{
		/* File " /etc/shell "  found ... */
		for(i=0; EOF != fscanf(fp,"%s",shells[i]) ; i++)
			sfprintf(sfstdout,"\t\t%d. %s\n", i+1, shells[i]);
		num_of_shells = i;
	}
	else
	{	
		/* File " /etc/shell " not found ... */
		sfprintf(sfstdout,"\t\t 1. /usr/bin/ksh\n");
		strcpy(shells[0],"/usr/bin/ksh");
		num_of_shells = 0;
	}
	
	sfprintf(sfstdout," User : %s \n",user);
	if((password =  getpwnam(user)) == NULL)
	{
		sfprintf(sfstderr,"Unable to get password entry. \n");
		return(1);
	}
	sfprintf(sfstdout," Got Shell : %s \n",password->pw_shell);
	if(ptr = strchr(user,'/'))
	{
		strcpy(user_name,ptr+1);
		*ptr  = '\0';
		strcpy(domain_name,user);
		mbstowcs(domain,domain_name,sizeof(domain));
		sfprintf(sfstdout," Domain : %s Username : %s\n",domain_name,user_name);
		mbstowcs(domain,domain_name,sizeof(domain));
		/* Get domain controller's name. */
		if((*netfn)(NULL,(LPWSTR)domain,(LPBYTE *)&machine) != NERR_Success)
		{	
			sfprintf(sfstderr," Not able to get domain controller \n");
			return(1);
		}
		wcstombs(machine_name,machine,sizeof(machine_name));
	}
	else
	{	
		machine = domain;
		strncpy(user_name,user,sizeof(user_name)); 
		strcpy(machine_name,"");
		mbstowcs(machine,machine_name,sizeof(machine));
	}
	olds = password->pw_shell;
	sfprintf(sfstderr,"Old shell: %s\nNew shell: ",olds);
	news = sfgetr(sfstdin,'\n',1);
	if(!_stricmp(olds, news)) 
	{
		sfprintf(sfstderr," Shell requested is the same as old shell \n");
		return (1);
	}
	for(i=0;i<=num_of_shells;i++)
	{
		if(!_stricmp(shells[i], news)) 
			goto found;
	}
	error(ERROR_exit(1),"Cannot change shell to %s - shell not implemented.",news);
	return(1);
found:

	mbstowcs(username,user_name,sizeof(username));
	status=(*netgetfn)(machine,(LPWSTR)username,10,(LPBYTE *)&user_info);
	if(status != NERR_Success)
	{
		/*NetUserGetInfo failed... */
		sfprintf(sfstderr,"NetUserGetInfo failed... status=%d\n",status); 
		switch(status)
		{
			case ERROR_ACCESS_DENIED :
				sfprintf(sfstderr," Access denied \n");
				break;
			case NERR_InvalidComputer :
				sfprintf(sfstderr," Invalid Computer Name \n");
				break;
			case NERR_UserNotFound :
				sfprintf(sfstderr," User name Not found \n");
			default :
				sfprintf(sfstderr," Undefinable error \n");
				break;
		}
		return 1;
	}
	uinfo = (USER_INFO_1007 *)malloc(sizeof(USER_INFO_1007));
	wcstombs(temp,user_info->usri10_comment, sizeof(temp));
	if(!_stricmp(news,"/usr/bin/ksh"))
	{
		if((ptr=strstr(temp,"<UWIN_SHELL=")) != NULL)
			*ptr = '\0'; 
	} 
	// make an entry in the comment field of the user in the NT database.
	else
	{
		sfsprintf(temp_str, sizeof(temp_str),"<UWIN_SHELL=%s>", news);
		ptr = strstr(temp,"<UWIN_SHELL=");
		if(ptr == NULL)
			strcat(temp, temp_str);
		else
			strcpy(ptr,temp_str);
	}
	mbstowcs(comment,temp,sizeof(comment));
	uinfo->usri1007_comment = comment;
	if((status = (*netsetfn)(machine,(LPWSTR)username,1007,(LPBYTE)uinfo,&parm_err)) != NERR_Success)  
	{
		sfprintf(sfstderr,"NetUserSetInfo failed .... status=%d\n",status);
		switch(status)
		{
			case NERR_NotPrimary :
				sfprintf(sfstderr," Allowed only on primary domain controller \n");
				break;
			case NERR_BadPassword :
				sfprintf(sfstderr," Bad Password \n");
				break;
			case NERR_PasswordTooShort :
				sfprintf(sfstderr," Password too short \n");
				break;
			case NERR_LastAdmin :
				sfprintf(sfstderr," Operation not allowed on last administrator \n");
				break;
			case NERR_SpeGroupOp :
				sfprintf(sfstderr," Operation not allowed on specified special group \n");
			case ERROR_ACCESS_DENIED :
				sfprintf(sfstderr," Access denied \n");
				break;
			case NERR_InvalidComputer :
				sfprintf(sfstderr," Invalid Computer Name \n");
				break;
			case NERR_UserNotFound :
				sfprintf(sfstderr," User name Not found \n");
				break;
		}
		return(1);	
	}   
	return(0);
}

static int passwd(const char *name)
{
	char *pp,oldp[PASS_MAX], newp[PASS_MAX];
	if(!(pp = getpass("Old password:")))
		exit(1);
	strncpy(oldp,pp,PASS_MAX);
	if(!(pp = getpass("New password:")))
		exit(1);
	strncpy(newp,pp,PASS_MAX);
	if(!(pp = getpass("Retype new password:")))
		exit(1);
	if(strcmp(newp,pp))
		error(ERROR_exit(1),"Mismatch - password unchanged.");
	pwchange(name,oldp,newp);
	return(0);
}

main(int argc, char *argv[])
{
	register int n, sflag=0;
	register char *name=(char *)NULL;
	char *home=0;
	OSVERSIONINFO	osver;

	osver.dwOSVersionInfoSize = sizeof(osver);
	if(GetVersionEx(&osver))
	{
		if(osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		{
			sfprintf(sfstderr,"This program does not run on Windows 9x platforms\n");
			return(1);
		}
	}

#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,0, NULL, 0);
#else
	cmdinit(argv,0, NULL, 0);
#endif
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'h':
		home = opt_info.arg;
		break;
	    case 's':
		sflag=1;
		break;
	    case ']':
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	if(error_info.errors || argc>1)
		error(ERROR_usage(2),"%s", optusage((char*)0));
	if(home)
	{
		if(!(name= *argv) && !(name = getlogin()))
			error(ERROR_exit(1),"Cannot get user name.");
		if(name && geteuid()  && strcmp(getlogin(),name))
			error(ERROR_exit(1),"Permission denied.");
		n=chhome(home,name);
		return(n);
	}
	if(sflag)
	{
		if((name= *argv) && geteuid()  && strcmp(getlogin(),name))

			error(ERROR_exit(1),"Permission denied.");
		error(0,"Changing %s for %s",sflag?"shell":"passwd",name?name:getlogin());
		if(!name)
			n=shell(getlogin());
		else
			n=shell(name);
	}
	else
		n=passwd(name);
	return(n);
}

