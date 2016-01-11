/*
 * UWIN master service
 */

static const char ums_version_id[] = "\n@(#)ums 2011-08-09\0\n";

#include <windows.h>
#include <winuser.h>
#include <sfio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <limits.h>
#include <signal.h>
#include <uwin.h>
#include <uwin_keys.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netdb.h>
#include <pwd.h>
#include <ntsecapi.h>
#define _STDSTREAM_DEFINED
#include <wchar.h>
#include <uwin_serve.h>
#include <aclapi.h>

#include "getinfo.h"

#if STANDALONE

#define PASSWDFILE		"passwd"
#define GROUPFILE		"group"

#undef	LOGDIR
#define LOGDIR			""

#else

#define PASSWDFILE		"/var/etc/passwd"
#define GROUPFILE		"/var/etc/group"

#endif

#define ADD_PASSWD		"/etc/passwd.add"
#define ADD_GROUP 		"/etc/group.add"

#define LOCK_FILE		"/var/uninstall/lock"

#define GEN_PASSWD		"/etc/passgen.sh"

#define EDMODES			(HDMODES|S_IWGRP)
#define HDMODES			(S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
#define RDMODES			(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define WRMODES			(RDMODES|S_IWGRP|S_IWOTH)

#ifndef elementsof
#define elementsof(x)		(int)(sizeof(x)/sizeof(x[0]))
#endif

#undef	swprintf
#define swprintf		_snwprintf

#define MAXI_CLIENT		100
#define MAXNAME			80
#define MAXSID			64
#define MAXUSER			100
#define MAXVAL			50
#define MAX_SIZE		256
#define PG_ENTRY_MAX		(16*LINE_MAX)
#define PG_EXTRA		50
#define RTN_ERROR		13
#define RTN_OK			0

#define DUP_MOVE		(DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE)

#define USER			1
#define GROUP			3

#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY		(0x0100)
#endif

#ifndef TOKEN_ADJUST_SESSIONID
#define TOKEN_ADJUST_SESSIONID	0
#endif

typedef BOOL (WINAPI* CPU_f)(HANDLE,const char*,char*,
	SECURITY_ATTRIBUTES*,SECURITY_ATTRIBUTES*,BOOL,DWORD,
	VOID*,const char*, STARTUPINFOA*,PROCESS_INFORMATION*);
typedef BOOL (WINAPI* LOG_f)(const char*,const char*,const char*,DWORD,DWORD,Handle_t*);

typedef struct Clitoken_s
{
	char*		cliname;
	HANDLE		clitok;
	BOOL		pending;
} Clitoken_t;

typedef struct Servpass_s
{
	char*		servname;
	int		fatal;
} Servpass_t;

static int	setuidflag = 1;
static int	passwdflag = 1;
static int	domainenumflag = 0;
static int	genpassflag = 1;
static int	IsDC = 0;
static int	IsDOM = 0;

static wchar_t*	wextragp[] = { L"Network", L"Interactive", L"Everyone", L"Creator Owner", L"Local" };
static char*	extragp[] = { "Network", "Interactive", "Everyone", "Creator Owner", "Local" };

static wchar_t			gszComName[MAXNAME]= L"";
static wchar_t			gszPdcName[MAXNAME]= L"\\\\";
static wchar_t			gszDomName[MAXNAME]= L"";

static HANDLE			admin;
static HANDLE			suidevent;
static HANDLE			request_pipe;
static HANDLE			serviceevent;
static HANDLE			msthreadhandle;
static LOG_f			log_user;
static int			root_len;
static char*			pdata[64 * 1024];
static char			unix_root[PATH_MAX];
static char			release[64];
static Clitoken_t 		clienttoken[MAXI_CLIENT];

static SERVICE_STATUS_HANDLE	statushandle;
static UMS_child_data_t		cr;

/*
 * run script as specified user but do not wait for completion
 */
static int runscript(char *script,HANDLE user)
{
	struct spawndata	sdata;
	char*			av[5];
	char*			name = strchr(script,'/');

	av[0] = "-ksh";
	av[1] = "-c";
	av[2] = script;
	av[3] = (name?name+1:script);
	av[4] = 0;
	ZeroMemory(&sdata,sizeof(struct spawndata));
	sdata.tok = user;
	signal(SIGCHLD,SIG_IGN);
	signal(SIGTERM,SIG_IGN);
	if(uwin_spawn("/usr/bin/ksh.exe", av, NULL,&sdata)<=0)
	{
		logmsg(0, "uwin_spawn() error on %s, errno=%d",av[2], errno);
		return -1;
	}
	logmsg(0, "%s started",av[2]);
	return 0;
}

static void _Sleep(unsigned long m, int line)
{
	logsrc(0, 0, line, "Sleep %lu begin", m);
	Sleep(m);
	logsrc(0, 0, line, "Sleep %lu end", m);
}

#define Sleep(m)		_Sleep(m,__LINE__)

static BOOL _createthread(LPTHREAD_START_ROUTINE start, const char* name, LPVOID param, HANDLE* handle, int line)
{
	DWORD	t;
	HANDLE	h;

	if (!(h = CreateThread(NULL, 0, start, param, 0, &t)))
	{
		logsrc(LOG_SYSTEM, 0, line, "CreateThread %s failed", name);
		return FALSE;
	}
	if (handle)
		*handle = h;
	else
		CloseHandle(h);
	logsrc(0, 0, line, "CreateThread %04d %s", t, name);
	return TRUE;
}

#define createthread(f,p,h)	_createthread((LPTHREAD_START_ROUTINE)f,#f,p,h,__LINE__)

static HANDLE _createnamedpipe(const char* label, const char* name, DWORD mode, DWORD instances, DWORD osize, DWORD isize, DWORD timeout, int line)
{
	HANDLE				hp;

	static SECURITY_ATTRIBUTES	sa;
	static PACL			pACL = 0;
	static PACL			pNewACL = 0;
	static EXPLICIT_ACCESS		explicit_access_list[1];
	static TRUSTEE			trustee[1];

	if (sa.nLength != sizeof(sa))
	{
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = FALSE;
		sa.lpSecurityDescriptor = nulldacl();
	}
	if (!(hp = CreateNamedPipe(name, PIPE_ACCESS_DUPLEX|PIPE_ACCESS_OUTBOUND|WRITE_DAC, mode, instances, osize, isize, timeout, &sa)) || hp == INVALID_HANDLE_VALUE)
	{
		logsrc(LOG_SYSTEM, 0, line, "CreateNamedPipe %s %s failed", label, name);
		return 0;
	}
	if (!trustee[0].ptstrName)
	{
		/*
		 * from the microsoft CreateNamedPipe() online docs:
		 *
		 * An ACE must be added to the pipe DACL so that client processes
		 * running under low-privilege accounts are also able to change state
		 * of client end of this pipe to Non-Blocking and/or Message-Mode.
		 */

		GetSecurityInfo(hp, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, &pACL, 0, 0);
		trustee[0].TrusteeForm = TRUSTEE_IS_NAME;
		trustee[0].TrusteeType = TRUSTEE_IS_GROUP;
		trustee[0].ptstrName = "Everyone";
		trustee[0].MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		trustee[0].pMultipleTrustee = 0;
		ZeroMemory(&explicit_access_list[0], sizeof(EXPLICIT_ACCESS));
		explicit_access_list[0].grfAccessMode = GRANT_ACCESS;
		explicit_access_list[0].grfAccessPermissions = FILE_GENERIC_READ|FILE_GENERIC_WRITE;
		explicit_access_list[0].grfInheritance = NO_INHERITANCE;
		explicit_access_list[0].Trustee = trustee[0];
		SetEntriesInAcl(1, explicit_access_list, pACL, &pNewACL);
	}
	SetSecurityInfo(hp, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, pNewACL, 0);
	logsrc(0, 0, line, "CreateNamedPipe %s %s", label, name);
	return hp;
}

#define createnamedpipe(l,n,m,c,o,i,t)	_createnamedpipe(#l,n,m,c,o,i,t,__LINE__)

static BOOL _connectnamedpipe(HANDLE* handle, const char* name, int line)
{
	if (!ConnectNamedPipe(handle, NULL) && GetLastError() != ERROR_PIPE_CONNECTED)
	{
		logsrc(LOG_SYSTEM, 0, line, "ConnectNamedPipe %s failed", name);
		return FALSE;
	}
	logsrc(0, 0, line, "ConnectNamedPipe %s connected", name);
	return TRUE;
}

#define connectnamedpipe(f)	_connectnamedpipe(f##_pipe,#f,__LINE__)

static BOOL _disconnectnamedpipe(HANDLE* handle, const char* name, int line)
{
	if (!DisconnectNamedPipe(handle))
	{
		logsrc(LOG_SYSTEM, 0, line, "DisconnectNamedPipe %s failed", name);
		return FALSE;
	}
	logsrc(0, 0, line, "DisconnectNamedPipe %s disconnected", name);
	return TRUE;
}

#define disconnectnamedpipe(f)	_disconnectnamedpipe(f##_pipe,#f,__LINE__)

static BOOL _scmstatus(DWORD State, DWORD ExitCode, DWORD Hint, int line)
{
	SERVICE_STATUS		status;

	static unsigned int	CheckPoint = 0;

	CheckPoint++;
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = State;
	status.dwControlsAccepted = State == SERVICE_START_PENDING ? 0 : (SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN);
	status.dwWin32ExitCode = ExitCode;
	status.dwServiceSpecificExitCode = 0;
	status.dwCheckPoint = (State == SERVICE_RUNNING || State == SERVICE_STOPPED) ? 0 : CheckPoint;
	status.dwWaitHint = Hint;
	logsrc(0, 0, line, "SetServiceStatus state=%d exit=%d hint=%d checkpoint=%d:%d", State, ExitCode, Hint, status.dwCheckPoint, CheckPoint);
	if (!SetServiceStatus(statushandle, &status))
	{
		logerr(LOG_EVENT+0, "SetServiceStatus failed");
		return FALSE;
	}
	return TRUE;
}

#define scmstatus(a,b,c)	_scmstatus(a,b,c,__LINE__)

static void GetComputerInfo(void)
{
	int n,ntry=10;
	WKSTA_INFO_100 *wkinfo;
	SERVER_INFO_100 *servinfo;
	NET_API_STATUS ret;
	unsigned char *pdc;
	DWORD hResume = 0,dwRead,dwTotal;

again:
	if ((n=NetWkstaGetInfo(NULL,100,(LPBYTE*)&wkinfo)) == NERR_Success)
	{
		wcscpy(gszComName,(wchar_t*)wkinfo->wki100_computername);
		wcscpy(gszDomName,(wchar_t*)wkinfo->wki100_langroup);
		NetApiBufferFree(wkinfo);
	}
	else
	{
		SetLastError(n);
		logerr(0, "NetWkstaGetInfo failed");
		if(n==RPC_S_CALL_FAILED_DNE && --ntry>=0)
		{
			Sleep(200);
			goto again;
		}
	}

	IsDC = IsDc();
	IsDOM = IsDomain();

	if (IsDOM)
	{
		if(!NetGetDCName(NULL, gszDomName, &pdc))
		{
			wcscpy(gszPdcName,(wchar_t*)pdc);
			NetApiBufferFree(pdc);
			return;
		}
		if ((ret=NetServerEnum(NULL,100,(LPBYTE*)&servinfo,MAX_PREFERRED_LENGTH,&dwRead,&dwTotal,SV_TYPE_DOMAIN_CTRL,NULL,&hResume)) == NERR_Success)
		{
			if (dwRead > 0)
			{
				wcscpy(&gszPdcName[2],(wchar_t*)servinfo[0].sv100_name);
			}
			NetApiBufferFree(servinfo);
		}
		else if (ret == ERROR_MORE_DATA)
			NetApiBufferFree(servinfo);
		else
		{
			SetLastError(ret);
			logerr(0, "NetServerEnum failed");
		}
	}

}

static int constructid(const wchar_t* sys, const wchar_t* name, wchar_t* domain, size_t domain_size, int* k)
{
	unsigned int own[256];
	DWORD size=sizeof(own), dsize=(DWORD)domain_size;
	SID_NAME_USE puse;
	int ret = -1,j;

	if(!LookupAccountNameW(sys, name, (SID*)&own, &size, domain, &dsize, &puse))
		logerr(0, "LookupAccountName sys=\"%ls\" name=\"%ls\" failed", sys, name);
	else
	{
		j = *GetSidSubAuthorityCount(own);
		*k = *GetSidSubAuthority(own, j-1);
		ret = uwin_sid2uid((SID*)&own);
		if (*domain && !wcschr(name, L'\\'))
			*domain = 0;
		logmsg(0, "constructid sys=\"%ls\" name=\"%ls\" domain=\"%ls\"", sys, name, domain);
	}
	return ret;
}

#ifndef SECURITY_NT_SID_AUTHORITY
#   define SECURITY_NT_SID_AUTHORITY	{0,0,0,0,0,5}
#endif

static SID *get_adminsid(void)
{
	static SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_SID_AUTHORITY;
	DWORD a[8];
	SID *sid;
	char *cp, keyname[256];
	int i,nsubs,n;
	long l;
	if(RegQueryInfoKey(HKEY_USERS, 0, 0, 0, &nsubs, 0, 0, 0, 0, 0, 0, 0))
		return 0;
	for(i=0; i < nsubs; i++)
	{
		n = sizeof(keyname);
		if(RegEnumKeyEx(HKEY_USERS,i,keyname,&n,NULL,NULL,NULL,NULL))
			continue;
		if(strncmp(keyname,"S-1-5-",6))
			continue;
		cp = &keyname[6];
		n=0;
		do
		{
			l=strtol(cp, &cp, 10);
			if(*cp && *cp!='-')
				continue;
			a[n++] = l;
		}
		while(*cp++);
		if(AllocateAndInitializeSid(&auth,(BYTE)n,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],&sid))
		{
			*GetSidSubAuthority(sid, n-1) = DOMAIN_USER_RID_ADMIN;
			return sid;
		}
	}
	return 0;
}

static void local_account(char *name, size_t size)
{
	NET_DISPLAY_USER	*userinfo=0;
	NET_API_STATUS 		r;
	DWORD			nread;
	r = NetQueryDisplayInformation(NULL,USER,0,1,MAX_PREFERRED_LENGTH,&nread,(void*)&userinfo);
	if(r!=ERROR_MORE_DATA && r!=NERR_Success)
	{
		SetLastError(r);
		logmsg(0,"NetQueryDisplayInformation failed err=%d",r);
		strcpy(name,"None");
	}
	else
	{
		wcstombs(name,userinfo[0].usri1_name,size);
		NetApiBufferFree((void*)userinfo);
	}
	return;
}

static char* name_mw2mu(register char* t, register const char* f, int n, char** d)
{
	register const char*	e = f + n;
	register int		c;
	char*			p = 0;

	while (f < e && (c = *f++))
	{
		if (c == ' ')
			c = '+';
		else if (c == '\\')
		{
			c = '/';
			p = t;
		}
		*t++ = c;
	}
	if (d)
		*d = p;
	*t = 0;
	return t;
}

static wchar_t* name_mu2ww(register wchar_t* t, register const char* f, int n, wchar_t** d)
{
	register wchar_t	c;
	wchar_t*		p = 0;

	mbstowcs(t, f, n);
	while (c = *t)
	{
		if (c == L'+')
			*t = L' ';
		else if (c == '/')
		{
			*t = L'\\';
			p = t;
		}
		t++;
	}
	if (d)
		*d = p;
	return t;
}

static wchar_t* name_ww2wu(register wchar_t* t, register const wchar_t* f, int n, wchar_t** d)
{
	register const wchar_t*	e = f + n;
	register wchar_t	c;
	wchar_t*		p = 0;

	while (f < e && (c = *f++))
	{
		if (c == L' ')
			c = L'+';
		else if (c == L'\\')
		{
			c = L'/';
			p = t;
		}
		*t++ = c;
	}
	if (d)
		*d = p;
	*t = 0;
	return t;
}

static wchar_t* name_wu2ww(register wchar_t* t, register const wchar_t* f, int n, wchar_t** d)
{
	register const wchar_t*	e = f + n;
	register wchar_t	c;
	wchar_t*		p = 0;

	while (f < e && (c = *f++))
	{
		if (c == L'+')
			c = L' ';
		else if (c == L'/')
		{
			c = L'\\';
			p = t;
		}
		*t++ = c;
	}
	if (d)
		*d = p;
	*t = 0;
	return t;
}

static void check_shell(char* comment, char* shell)
{
	char *ptr,buff[MAX_SIZE];
	ptr = strstr(comment,"<UWIN_SHELL=");
	if(ptr)
	{
		int x = 0;
		char *first = ptr;
		ptr += strlen("<UWIN_SHELL=");
		while(*ptr && *ptr!='>' && x < MAX_SIZE)
			buff[x++] = *ptr++;
		if(*ptr++ != '>')
			strcpy(shell, "/usr/bin/ksh");
		else
		{
			buff[x] = 0;
			strcpy(shell,buff);
			strcpy(first,ptr);
		}
	}
	else
		strcpy(shell, "/usr/bin/ksh");
}

/*
 * check to see if /home/user exists and if not create it
 */

static int gethomedir(char *buffer, int bsize, char *wname, int id, int gid)
{
	register int n;
	char user[256];
	register int len=0,length=0;
	char *cp;
	char username[256];

	buffer[0] = 0;
	n = uwin_path("/home",user,sizeof(user));
	user[n++] = '\\';
	strcpy(&user[n], wname);
	memcpy(buffer,"/home/",6);
	buffer[6] = 0;
	if(cp=strchr(&user[n],'/'))	//string in domain/username format
	{
		strcpy(username,cp+1);
		*cp = 0;
		length=(int)strlen(username);
		len=(int)strlen(user);
		strncpy(&buffer[6],&user[n],(len-n));	//Copy the domain name
		buffer[6+len-n] = 0;
		len=(int)strlen(buffer);
		strncpy(&buffer[len],"/",1);
		buffer[len+1] = 0;
		if(!mkdir(buffer, HDMODES))
		{
			if(chmod(buffer, HDMODES) < 0)
				logerr(0, "chmod %s failed", buffer);
		}
		else if(errno!=EEXIST)
		{
			logmsg(0, "mkdir %s failed: errno=%d using /tmp",buffer, errno);
			strcpy(buffer,"/tmp");
			return 0;
		}
		len=(int)strlen(buffer);
		strncat(buffer,username,length);
		buffer[len+length+1] = 0;
	}
	else
	{
		cp = &user[n];
		len=(int)strlen(buffer);
		strncpy(&buffer[len],cp,sizeof(user)-n);
	}
	n = uwin_path(buffer,username,sizeof(username));
	if(!mkdir(username, HDMODES))
	{
		if(chown(username, id, gid))
			logerr(0, "chown %s <%u,%u> failed", username, id, gid);
		if(chmod(username, HDMODES))
			logerr(0, "chmod %s failed", username);
	}
	else if(errno!=EEXIST)
	{
		logmsg(0, "mkdir %s failed -- using /tmp (errno=%d)", username, errno);
		strcpy(buffer, "/tmp");
		return 0;
	}
	return 1;
}

static int getpasswdstruct(wchar_t* sys_name, const wchar_t* name, const wchar_t* dom_name, char* passwd_str, int passwd_len, int type)
{
	DWORD		k;
	int		adjust = 0;
	int		retval = 0;
	int		id;
	int		len;
	char*		cp;
	USER_INFO_3*	ui;
	GROUP_INFO_2*	gi;
	void*		ugi;
	wchar_t*	ug_name;
	wchar_t*	ug_comment;
	wchar_t*	ug_home;
	DWORD		ug_id;
	NET_API_STATUS	ret;
	char		comment[256];
	char		usershell[MAX_SIZE];
	char		buffer[256];
	char		full_name[2*MAXNAME] = "";
	wchar_t		domain[MAXNAME];

	/*
	 * a bunch of disambiguating hacks here because
	 * a windows file owner can be a group
	 */

	if (type == 'u')
	{
		if ((ret = NetUserGetInfo(sys_name, name, 3, (LPBYTE*)&ui)) != NERR_Success)
		{
			SetLastError(ret);
			logerr(0, "NetUserGetInfo sys=\"%ls\" name=\"%ls\" failed", sys_name, name);
			if (ret == ERROR_MORE_DATA)
				NetApiBufferFree(ui);
			return retval;
		}
		ugi = ui;
		ug_id = ui->usri3_primary_group_id;
		ug_name = ui->usri3_full_name;
		ug_comment = ui->usri3_comment;
		ug_home = ui->usri3_home_dir;
	}
	else
	{
		if ((ret = NetGroupGetInfo(sys_name, name, 2, (LPBYTE*)&gi)) != NERR_Success)
		{
			SetLastError(ret);
			logerr(0, "NetGroupGetInfo sys=\"%ls\" name=\"%ls\" failed", sys_name, name);
			if (ret == ERROR_MORE_DATA)
				NetApiBufferFree(gi);
			return retval;
		}
		ugi = gi;
		ug_id = gi->grpi2_group_id;;
		ug_name = gi->grpi2_name;
		ug_comment = gi->grpi2_comment;
		ug_home = L"";
	}
	k = USER;
	id = constructid(sys_name, name, domain, elementsof(domain), &k);
	if (id >= 0)
	{
		adjust = k - id;
		if (dom_name)
		{
			wcstombs(full_name, dom_name, sizeof(full_name));
			strcat(full_name, "/");
		}
		len = (int)strlen(full_name);
		wcstombs(&full_name[len], name, sizeof(full_name)-len);
		name_mw2mu(passwd_str, full_name, passwd_len, 0);
		strcat(passwd_str, ":x:");
		k = ug_id - adjust;
		sfsprintf(&passwd_str[strlen(passwd_str)], -1, "%d:%ld:", id, k);
		len = (int)wcstombs(comment, ug_comment, sizeof(comment)-1);
		if (len < 0)
			len = 0;
		comment[len] = 0;
		if (*ug_name)
		{
			char	uname[2*MAXNAME];

			wcstombs(uname, ug_name, sizeof(uname));
			strcat(passwd_str, uname);
			if (*comment)
				strcat(passwd_str, ",");
		}
		for (cp = comment; *cp; cp++)
			if(*cp == ':')
				*cp=';';
		check_shell(comment, usershell);
		len = (int)strlen(passwd_str);
		strcat(passwd_str, comment);
		strcat(passwd_str, ":");
		wcstombs(buffer, ug_home, sizeof(buffer));
		if (buffer[0] && !sys_name)
		{
			if (buffer[1]==':' && buffer[0]>='a' && buffer[0]<='z')
				buffer[0] += ('A'-'a');
			uwin_unpath(buffer, comment, sizeof(comment));
			strcat(passwd_str, comment);
		}
		else
		{
			/* In domain controller, the local user account and domain
			 * user account both should point to same home directory */
			if (IsDC && dom_name && !_wcsicmp(dom_name, gszDomName))
				wcstombs(full_name, name, sizeof(full_name));
			gethomedir(buffer, sizeof(buffer), full_name, id, k);
			strcat(passwd_str, buffer);
		}
		strcat(passwd_str, ":");
		strcat(passwd_str, usershell);
		len = (int)strlen(passwd_str);
		passwd_str[len] = '\n';
		len++;
		passwd_str[len] = 0;
		retval = len;
		NetApiBufferFree(ugi);
	}
	return retval;
}

static void user_enum(int fd, int domain)
{
	DWORD			dwRead;
	DWORD			dwCnt;
	DWORD			dwLoop;
	NET_API_STATUS		ret;
	NET_DISPLAY_USER*	userinfo;
	wchar_t*		sys;
	wchar_t*		dom;
	char*			type;
	int			size;
	char			pass[1024];
	wchar_t			uname[MAXNAME];

	if (domain)
	{
		type = "domain";
		sys = gszPdcName;
		dom = gszDomName;
	}
	else
	{
		type = "local";
		sys = 0;
		dom = 0;
	}
	logmsg(1, "user_enum %s", type);
	dwCnt=0;
	userinfo = 0;
	for (;;)
	{
		ret = NetQueryDisplayInformation((LPCWSTR)sys,USER,dwCnt,100,MAX_PREFERRED_LENGTH,&dwRead,(PVOID*)&userinfo);
		logmsg(1, "user_enum %s sequence ret=%d dwRead=%ld", type, ret, dwRead);
		if(ret!=ERROR_MORE_DATA && ret!=NERR_Success)
		{
			SetLastError(ret);
			logerr(0, "user_enum %s NetQueryDisplayInformation sys=%s failed", type, sys);
			break;
		}
		for (dwLoop=0;dwLoop<dwRead;++dwLoop)
		{
			wcscpy(uname,userinfo[dwLoop].usri1_name);
			if ((size = getpasswdstruct(sys,uname,dom,pass,sizeof(pass),'u')) <= 0)
				logmsg(0, "user_enum %s unable to get the passwd entry of %ls", type, uname);
			else if (write(fd,pass,size) != size)
				logerr(0, "user_enum write failed");
		}
		if(dwRead)
			dwCnt = userinfo[dwRead-1].usri1_next_index;
		NetApiBufferFree((LPVOID)userinfo);
		if(!dwRead)
			break;
	}
	logmsg(1, "user_enum %s done", type);
}

static int EnumGlobalUsers(wchar_t* sys_name, wchar_t* grname, wchar_t* dname, wchar_t* group_str, wchar_t* last)
{
	NET_API_STATUS ret;
	GROUP_USERS_INFO_0 *ulist;
	DWORD dwRead,dwTotal,dwLoop,dnamelen,len;
	DWORD_PTR hResume=0;
	wchar_t *begin, *next=group_str,*name;
	int room = 1, chop;
	wchar_t buf[MAXNAME];

	logmsg(1, "EnumGlobalUsers sys=\"%ls\" name=\"%ls\" domain=\"%ls\" [%ld]", sys_name, grname, dname, last - next);
	dnamelen = (dname && *dname) ? ((int)wcslen(dname) + 1) : 0;
	chop = dnamelen && IsDC && !_wcsicmp(dname,gszDomName);
	do
	{
		/*
		 * servername==sys_name
		 * groupname==grname
		 * level==0
		 * bufptr==ulist
		 * prefmaxlen==MAX_PREFERRED_LENGTH
		 * entiresread==&dwRead
		 * totalentries=&dwTotal
		 * ResumeHandle==&hResume
		 */
		if (dnamelen)
		{
			swprintf(buf, elementsof(buf), L"%s\\%s", dname, grname);
			name = buf;
		}
		else
			name = grname;
		ulist = 0;
		ret = NetGroupGetUsers((LPCWSTR)sys_name,(LPCWSTR)name,0,(LPBYTE*)&ulist,MAX_PREFERRED_LENGTH,&dwRead,&dwTotal,&hResume);
		if(ret!=NERR_Success && ret!=ERROR_MORE_DATA)
		{
#if 0
			if (ret != NERR_GroupNotFound)
#endif
			{
				SetLastError(ret);
				logerr(0, "NetGroupGetUsers sys=\"%ls\" name=\"%ls\" failed -- resume=%d total=%d", sys_name, name, hResume, dwTotal);
			}
			if (ulist)
				NetApiBufferFree((LPVOID)ulist);
			return 0;
		}
		logmsg(1, "NetGroupGetUsers name=\"%s\" dwRead=%ld dwTotal=%ld", name, dwRead, dwTotal);
		for (dwLoop=0;dwLoop<dwRead;++dwLoop)
		{
			begin = ulist[dwLoop].grui0_name;
			len = (int)wcslen(begin);
			if (begin[len-1] == L'$')
				continue;
			if(dnamelen)
			{
				if((next + dnamelen + len) >= last)
				{
					room = 0;
					logmsg(0, "NetGroupGetUsers overflow attempting to add %ls:%ls:%ls/%ls", sys_name, grname, dname, begin);
					break;
				}
				next = name_ww2wu(next,dname,(int)(last-next),0);
				*next++ = L'/';
				next = name_ww2wu(next,begin,(int)(last-next),0);
				*next++ = L',';
				if (!chop)
					continue;
			}
			if((next + len) >= last)
			{
				room = 0;
				logmsg(0, "NetGroupGetUsers overflow attempting to add %ls:%ls:%ls", sys_name, grname, begin);
				break;
			}
			next = name_ww2wu(next,begin,(int)(last-next),0);
			*next++ = L',';
		}
		NetApiBufferFree((LPVOID)ulist);
	} while (room && ret != NERR_Success);
	*next = 0;
	return (int)(next-group_str);
}

static int mkgrline(wchar_t* sys, id_t id, wchar_t* group, wchar_t* dom, char* group_buf, int group_len)
{
	int				r = 0;
	int				room = 1;
	DWORD				dwRead;
	DWORD				dwTotal;
	DWORD_PTR			hResume = 0;
	DWORD				dwCnt = 0;
	DWORD				dwLoop;
	GROUP_USERS_INFO_0*		UserList = 0;
	LOCALGROUP_MEMBERS_INFO_2*	LocalUserList = 0;
	NET_API_STATUS			ret;
	int				k = GROUP;
	int				len;
	id_t				gid;
	wchar_t*			sep;
	wchar_t*			begin;
	wchar_t*			next;
	wchar_t*			last;
	wchar_t				group_str[PG_ENTRY_MAX];
	wchar_t				domain[MAXNAME];

	logmsg(1, "mkgrline sys=\"%ls\" name=\"%ls\" id=%d domain=\"%ls\" groups=\"%s\"", sys, group, id, dom, group_buf);
	gid = constructid(sys, group, domain, elementsof(domain), &k);
	if (*domain)
		dom = domain;
	if (dom && !_wcsicmp(dom, gszComName))
		dom = 0;
	if (gid >= 0 || id)
	{
		next = group_str;
		last = &group_str[elementsof(group_str)-1];
		if (dom && *dom)
		{
			next = name_ww2wu(next, dom, (int)(last-next), 0);
			*next++ = L'/';
		}
		next = name_ww2wu(next, group, (int)(last-next), 0);
		if ((len = swprintf(next, last-next, L":x:%d:", id ? id : gid)) > 0)
			next += len;
		if (gid == id)
		{
			if ((len = EnumGlobalUsers(sys, group, dom, next, last-PG_EXTRA)) > 0)
				next += len;
			else
			{
				/*
				 * servername==sys
				 * localgroupname==group
				 * level==2
				 * bufptr==&LocalUserList
				 * prefmaxlen==MAX_PREFERED_LENGTH
				 * entriesread==&dwRead
				 * totalentries=&dwTotal
				 * resumehandle==&hResume
				 */
				logmsg(0, "mkgrline \"%ls\" check for local group", group);
				if ((ret=NetLocalGroupGetMembers((LPCWSTR)sys,(LPCWSTR)group,2,(LPBYTE *)&LocalUserList,MAX_PREFERRED_LENGTH,&dwRead,&dwTotal,&hResume)) == NERR_Success)
				{
					logmsg(1, "LclGrp ret=%d dwRead=%ld dwTotal=%ld room=%d", ret, dwRead, dwTotal, room);
					while (dwCnt < dwTotal && room)	 //	loop to retrieve all members of local group
					{
						logmsg(1, "dwCnt=%d", dwCnt);
						for (dwLoop=0;dwLoop < dwRead;++dwLoop)
						{
							begin = LocalUserList[dwLoop].lgrmi2_domainandname;
							if (sep = wcschr(begin,L'\\'))
							{
								*sep = 0;
								if (dom && _wcsicmp(dom, begin) == 0 || _wcsicmp(gszComName, begin) == 0)
									begin = sep+1;
								else if (IsDC && _wcsicmp(begin, gszDomName) == 0)
								{
									len = (int)wcslen(sep+1);
									if ((next + len) >= last)
									{
										room = 0;
										break;
									}
									next = name_ww2wu(next, sep+1, (int)(last-next), 0);
									*next++ = L',';
								}
								*sep = L'/';
								len = (int)wcslen(begin);
								if ((next + len) >= last)
								{
									room = 0;
									break;
								}
								next = name_ww2wu(next, begin, (int)(last-next), 0);
								*next++ = L',';
							}
						}
						dwCnt += dwRead;
						NetApiBufferFree((LPVOID)LocalUserList);
						ret = NetLocalGroupGetMembers((LPCWSTR)sys,(LPCWSTR)group,2,(LPBYTE *)&LocalUserList,MAX_PREFERRED_LENGTH,&dwRead,&dwTotal,&hResume);
						logmsg(1, "LclGrpMbr ret=%d dwRead=%ld dwTotal=%ld", ret, dwRead, dwTotal);
					}
				}
				else if (ret==ERROR_MORE_DATA)
				{
					logmsg(1, "LclGrp ret=%d dwRead=%ld dwTotal=%ld room=%d", ret, dwRead, dwTotal, room);
					NetApiBufferFree((LPVOID)LocalUserList);
				}
				else
				{
					SetLastError(ret);
					logerr(0, "NetLocalGroupGetMembers sys=\"%ls\" name=\"%ls\" failed", sys, group);
				}
			}
			if (next > group_str && next[-1] == L',')
				next--;
		}
		*next++ = L'\n';
		*next = 0;
		if ((r = (int)wcstombs(group_buf, group_str, group_len)) < 0)
		{
			logerr(0, "wcstombs %ls failed", group_str);
			ret = 0;
		}
	}
	return r;
}

static void group_enum(int fd, int domain)
{
	NET_DISPLAY_GROUP*	groupinfo;
	LOCALGROUP_INFO_0*	localgroup;
	DWORD			dwRead, dwTotal, dwCnt=0, dwLoop;
	DWORD_PTR		hResume=0;
	NET_API_STATUS		ret;
	wchar_t*		sys;
	wchar_t*		dom;
	wchar_t			domain_name[MAXNAME];
	char*			type;
	int			i, k=GROUP, id, size, len, wrt;
	char			group_entry[PG_ENTRY_MAX];

	if (domain)
	{
		type = "domain";
		sys = gszPdcName;
		dom = gszDomName;
	}
	else
	{
		type = "local";
		sys = 0;
		dom = 0;
	}
	logmsg(0, "group_enum %s", type);
	if (!domain)
	{
		for (;;)
		{
			/*
			 * servername==NULL
			 * level== 0
			 * bufptr==&localgroup
			 * prefmaxlen==MAX_PREFERRRED_LENGTH
			 * entriesred=&dwRead
			 * totalentries=&dwTotal
			 * resumehandle=&hResume
			 */
			ret = NetLocalGroupEnum(NULL,0,(LPBYTE*)&localgroup,MAX_PREFERRED_LENGTH,&dwRead,&dwTotal,&hResume);
			logmsg(1, "group_enum %s lclgrpenum ret=%d dwRead=%ld dwTotal=%ld", type, ret, dwRead, dwTotal);
			if (ret!=ERROR_MORE_DATA && ret!=NERR_Success)
			{
				SetLastError(ret);
				logerr(0, "group_enum %s NetLocalGroupEnum failed", type);
				break;
			}
			group_entry[0] = 0;
			for (dwLoop=0;dwLoop<dwRead;++dwLoop)
			{
				id = constructid(0, localgroup[dwLoop].lgrpi0_name, domain_name, elementsof(domain_name), &k);
				if (!(size = mkgrline(0, id, localgroup[dwLoop].lgrpi0_name, gszComName, group_entry, sizeof(group_entry))))
					logmsg(0, "group_enum %s unable to get the entry of group name=%ls id=%d", type, localgroup[dwLoop].lgrpi0_name, id);
				else if ((wrt=write(fd,group_entry,size)) != size)
					logmsg(0, "group_enum %s write group %s ret=%d size=%d failed err=%d", type, group_entry, wrt, size, errno);
			}
			NetApiBufferFree((LPVOID)localgroup);
			if (ret == NERR_Success)
				break;
		}

		/* Enumerate default local groups */
		for (i=0;i<5;++i)
		{
			id = constructid(0, wextragp[i], domain_name, elementsof(domain_name), &k);
			len = sfsprintf(group_entry,sizeof(group_entry),"%s:x:%d:\n",extragp[i],id);
			write(fd,group_entry,len);
		}
		if (IsDC)
			dom = gszDomName;
	}
	dwCnt=0;
	logmsg(1, "group_enum %s account=%d sys=\"%ls\" domain=\"%ls\"", type, i, sys, dom);
	for (;;)
	{
		/*
		 * ServerName==sys
		 * Level==GROUP==3
		 * Index==dwCnt, initially==0
		 *   later dwCnt = groupinfo[dwRead-1].grpi3_next_index;
		 * EntriesRequested==100
		 * PreferredmaximumLength==MAX_PREFERRED_LENGTH
		 * ReturnedEntryCount==&dwRead
		 * SortedBuffer==&groupinfo
		 */
		ret = NetQueryDisplayInformation((LPCWSTR)sys,GROUP,dwCnt,100,MAX_PREFERRED_LENGTH,&dwRead,(PVOID*)&groupinfo);
		logmsg(1, "group_enum %s dispinfo ret=%d dwCnt=%ld dwRead=%ld", type, ret, dwCnt, dwRead);
		if(ret!=ERROR_MORE_DATA && ret!=NERR_Success)
		{
			SetLastError(ret);
			logerr(0, "group_enum %s NetQueryDisplayInformation failed", type);
			break;
		}
		group_entry[0] = 0;
		for (dwLoop=0;dwLoop<dwRead;++dwLoop)
		{
			id = constructid(0, groupinfo[dwLoop].grpi3_name, domain_name, elementsof(domain_name), &k);
			if (!(size = mkgrline(sys, id, groupinfo[dwLoop].grpi3_name, dom, group_entry, sizeof(group_entry))))
				logmsg(0, "group_enum %s unable to get the entry of group name=%ls id=%d", type, localgroup[dwLoop].lgrpi0_name, id);
			else if ((wrt=write(fd, group_entry, size)) != size)
				logmsg(0, "group_enum %s write group %s ret=%d size=%d failed err=%d", type, group_entry, wrt, size, errno);
		}
		if (dwRead)
			dwCnt = groupinfo[dwRead-1].grpi3_next_index;
		NetApiBufferFree((LPVOID)groupinfo);
		if (!dwRead)
			break;
	}
	logmsg(0, "group_enum %s done", type);
}

static int ums_rename(const char* fpath, const char* tpath)
{
	ssize_t	n;
	int	ffd;
	int	tfd;
	char	buffer[8*1024];

	if (!rename(fpath, tpath))
		return 0;
	if (!unlink(tpath) && !rename(fpath, tpath))
		return 0;
	if ((ffd = open(fpath, O_RDONLY)) < 0)
	{
		logerr(0, "cannot read %s", fpath);
		return -1;
	}
	if ((tfd = open(tpath, O_WRONLY|O_CREAT|O_TRUNC, RDMODES)) < 0)
	{
		logerr(0, "cannot write %s", tpath);
		return -1;
	}
	while ((n = read(ffd, buffer, sizeof(buffer))) > 0)
		if (write(tfd, buffer, n) != n)
			logerr(0, "write %s failed", tpath);
	close(ffd);
	close(tfd);
	if (n < 0)
	{
		logerr(0, "read %s failed", fpath);
		RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
		return -1;
	}
	if (chmod(tpath, RDMODES))
		logerr(0, "cannot chmod %s", fpath);
	if (unlink(fpath))
		logerr(0, "unlink %s failed", fpath);
	return 0;
}

static void addfile(const char *xfile,const char *file_to_append)
{
	int  fd,fd1,len;
	char arr[1024];

	if ((fd1 = open(file_to_append, O_RDONLY)) >= 0)
	{
		if ((fd = open(xfile, O_WRONLY|O_CREAT|O_TRUNC, RDMODES)) >= 0)
		{
			while ((len = read(fd1, arr, sizeof(arr))) > 0)
				write(fd, arr, len);
			close(fd);
		}
		else
			logerr(0, "cannot append %s", xfile);
		close(fd1);
	}
	else
		logerr(0, "cannot read %s", file_to_append);
}

/*
 * generate the passwd and group files
 * call once in main thread with domain=0 to initialize local accounts
 * optionally call in thread with domain!=0 to check domain accounts
 */

static int genpg(int domain)
{
	int	from;
	int	o_flags;
	int	pw_fd;
	int	gr_fd;

	logmsg(0, "genpg %s begin", domain ? "domain" : "local");
	o_flags = O_WRONLY|O_CREAT;
	if (domain || passwdflag)
	{
		from = SEEK_END;
		addfile(PASSWDFILE".uwin", domain ? PASSWDFILE : ADD_PASSWD);
		addfile(GROUPFILE".uwin", domain ? GROUPFILE : ADD_GROUP);
	}
	else
	{
		from = SEEK_SET;
		o_flags |= O_TRUNC;
	}
	if ((pw_fd = open(PASSWDFILE".uwin", o_flags, RDMODES)) >= 0)
	{
		lseek(pw_fd, 0, from);
		user_enum(pw_fd, domain);
		close(pw_fd);
		if (chmod(PASSWDFILE".uwin", RDMODES))
			logerr(0, "cannot chmod %s", PASSWDFILE".uwin");
		if (domain)
			ums_rename(PASSWDFILE, PASSWDFILE".local");
		ums_rename(PASSWDFILE".uwin", PASSWDFILE);
	}
	else
		logmsg(0, "genpg: open(%s) failed with error %d", PASSWDFILE".uwin", errno);
	if ((gr_fd = open(GROUPFILE".uwin", o_flags, RDMODES)) >= 0)
	{
		lseek(gr_fd, 0L, from);
		group_enum(gr_fd, domain);
		close(gr_fd);
		if (chmod(GROUPFILE".uwin", RDMODES))
			logerr(0, "cannot chmod %s", GROUPFILE".uwin");
		if (domain)
			ums_rename(GROUPFILE, GROUPFILE".local");
		ums_rename(GROUPFILE".uwin", GROUPFILE);
	}
	else
		logmsg(0, "genpg: open(%s) failed with error %d", GROUPFILE".uwin", errno);
	if (!domain && !access(GEN_PASSWD, X_OK))
		runscript(GEN_PASSWD, admin);
	logmsg(0, "genpg %s done", domain ? "domain" : "local");
	return 1;
}

static DWORD WINAPI creatpg(LPVOID param)
{
	return genpg(1);
}

static int mkpwline(id_t id, char* name, char* pass, int pass_len)
{
	wchar_t*		user;
	wchar_t*		dom;
	wchar_t			buf[MAXNAME],ser_name[256]=L"\\\\",*p=NULL;
	NET_API_STATUS		stat;
	SERVER_INFO_100*	serv_info;
	DWORD			i=0,totalread=0, totalentry, ssize, dsize;
	BOOLEAN			bLocal=TRUE,bErr=FALSE;
	int			r = 0;
	int			type;
	wchar_t			domain[MAXNAME];
	SID_NAME_USE		use;
	DWORD			sid[MAXSID];
	DWORD			dis[MAXSID];

	name_mu2ww(buf, name, sizeof(buf), &user);
	ssize = sizeof(sid);
	dsize = elementsof(domain);
	if (!LookupAccountNameW(NULL, buf, sid, &ssize, domain, &dsize, &use))
	{
		logerr(0, "LookupAccountName %s failed", name);
		if (!id)
			return r;
		use = SidTypeUnknown;
		goto fabricate;
	}
	else if (id && uwin_uid2sid(id, (SID*)dis, sizeof(dis)) && !EqualSid(sid, dis))
		goto fabricate;
	switch (use)
	{
	case SidTypeUser:
		type = 'u';
		break;
	case SidTypeGroup:
		type = 'g';
		break;
	default:
		/* fabricate passwd entry for non-posix use type */
		id = uwin_sid2uid((Sid_t*)sid);
	fabricate:
		return sfsprintf(pass, pass_len, "%s:x:%d:%d:account use %d:/:/dev/null\n", name, id, id, use);
	}
	if (user)
	{
		/* *user=='/' so it may be a domain name -- rule out "local" domains */
		dom = buf;
		*user++ = 0;
		if (!dom[0])
			bErr = TRUE;
		else if (_wcsicmp(dom, gszComName) && (!IsDC || _wcsicmp(dom, gszDomName)))
		{
			if (!_wcsicmp(dom, L"BUILTIN"))
			{
				*--user = '\\';
				user = buf;
			}
			else
				bLocal = FALSE;
		}
	}
	else
		user = buf;
	if (bErr)
		logmsg(0, "invalid local account");
	else if (bLocal)
		r = getpasswdstruct(NULL, user, NULL, pass, pass_len, type);
	else
	{
		if (IsDOM)
		{
			unsigned char *pdc;
			if(!NetGetDCName(NULL,dom,&pdc))
			{
				wcscpy(ser_name,(wchar_t*)pdc);
				NetApiBufferFree(pdc);
				if ((r = getpasswdstruct(ser_name,user,dom,pass,pass_len,type)) > 0)
					return r;
			}
			stat=NetServerEnum(NULL,100,(unsigned char**)&serv_info,MAX_PREFERRED_LENGTH, &totalread,&totalentry,SV_TYPE_DOMAIN_CTRL|SV_TYPE_DOMAIN_BAKCTRL,dom,0);

			if (stat == NERR_Success)
			{
				for (i=0;i<totalread;++i)
				{
					wcscpy(&ser_name[2],(wchar_t*)serv_info[i].sv100_name);
					if ((r = getpasswdstruct(ser_name,user,dom,pass,pass_len,type)) > 0)
						break;
				}
				NetApiBufferFree(serv_info);
			}
			else
			{
				if (stat == ERROR_MORE_DATA)
					NetApiBufferFree(serv_info);
				SetLastError(stat);
				logerr(0, "NetServerEnum failed");
			}
		}
		/* Not able to find the domain server  */
		if(i == totalread)
			logmsg(0, "invalid domain account");
	}
	return r;
}

#define NUM_PASSWD_PIPES	20
#define NUM_GRP_PIPES		20

#define MAX_DATA		4096

static int			curr_passwd_thrd_cnt = 0;

static DWORD WINAPI proc_passwdThread(LPVOID pipe)
{
	HANDLE			hPipe;
	DWORD			read;
	DWORD			size;
	int			fd;
	int			ret;
	UMS_domainentry_t	request;
	char			ac_entry[PG_ENTRY_MAX];

	hPipe = (HANDLE)pipe;

	__try
	{
		size = 0;
		if (ReadFile(hPipe, &request, sizeof(request), &read, NULL) && read > 0)
		{
			logmsg(0, "proc_passwdThread %s name=%s id=%d started", "USER", request.name, request.id);
			size = mkpwline(request.id, request.name, ac_entry, sizeof(ac_entry));
			if (!(ret = WriteFile(hPipe, &size, sizeof(DWORD), &read, NULL)))
				logerr(0, "passwd write size sz=%d ret=%d read=%d", size, ret, read);
			if (size > 0)
			{
				if (!(ret = WriteFile(hPipe, ac_entry, size, &read, NULL)))
					logerr(0, "passwd data ret=%d read=%d", ret, read);
				if ((fd = open(PASSWDFILE, O_WRONLY|O_APPEND)) >= 0)
				{
					if (write(fd, ac_entry, size) != size)
						logerr(0, "%s append failed", PASSWDFILE);
					close(fd);
				}
			}
		}
		else
			logerr(0, "passwd pipe read failed:");
		if (size && !access(GEN_PASSWD, X_OK))
			sfsprintf(ac_entry, sizeof(ac_entry), "%s %s %s", GEN_PASSWD, "-u", request.name);
		FlushFileBuffers(hPipe);
		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);
	}

	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		logmsg(0, "proc_passwdThread exception");
		size = 0;
	}

	curr_passwd_thrd_cnt--;
	logmsg(0, "proc_passwdthread terminating size=%d cnt=%d", size, curr_passwd_thrd_cnt);
	return size;
}

static DWORD WINAPI passwdThread(LPVOID param)
{
	char*	pipe_name;
	HANDLE	passwd_pipe;
	int	max_thread = 0;

	pipe_name = UWIN_PIPE_PASSWORD;
	for (;;)
	{
		if (!(passwd_pipe = createnamedpipe(passwd, pipe_name, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, NUM_PASSWD_PIPES, MAX_DATA, MAX_DATA, NMPWAIT_USE_DEFAULT_WAIT)))
		{
			Sleep(50);
			continue;
		}
		if (!connectnamedpipe(passwd) || !createthread(proc_passwdThread, passwd_pipe, 0))
		{
			CloseHandle(passwd_pipe);
			continue;
		}
		if (++curr_passwd_thrd_cnt > max_thread)
			max_thread = curr_passwd_thrd_cnt;
		logmsg(0, "proc_passwdThread count %d/%d", curr_passwd_thrd_cnt, max_thread);
	}
	return 0;
}

static int	curr_grp_thrd_cnt = 0;

static DWORD WINAPI proc_grpThread(LPVOID pipe)
{
	wchar_t			winfo[MAX_SIZE];
	wchar_t*		domain;
	wchar_t*		name;
	HANDLE			hPipe;
	int			fd;
	int			ret;
	DWORD			read;
	DWORD			size=0;
	UMS_domainentry_t	request;
	char			ac_entry[PG_ENTRY_MAX];

	hPipe = (HANDLE) pipe;

	__try
	{
		if (ReadFile(hPipe, &request, sizeof(request), &read, NULL) && read > 0)
		{
			logmsg(0,  "proc_grpThread %s name=%s id=%d started", "GROUP", request.name, request.id);
			name_mu2ww(winfo, request.name, sizeof(winfo) - 1, &domain);
			if (domain)
			{
				*domain++ = 0;
				name = domain;
				domain = winfo;
			}
			else
			{
				name = winfo;
				domain = gszComName;
			}
			ac_entry[0] = 0;
			if (!(size = mkgrline(0, request.id, name, domain, ac_entry, sizeof(ac_entry))) && IsDOM)
			{
				/* If not local group and the system is in domain then
				 * try with Pdc system as global group */
				size = mkgrline(gszPdcName, request.id, name, domain == gszComName ? gszDomName : domain, ac_entry, sizeof(ac_entry));
			}
			if (!(ret = WriteFile(hPipe, &size, sizeof(DWORD), &read, NULL)))
				logerr(0, "grp write size sz=%d ret=%d read=%d", size, ret, read);
			if (size > 0)
			{
				if (!(ret = WriteFile(hPipe, ac_entry, size, &read, NULL)))
					logerr(0, "grp data ret=%d read=%d", ret, read);
				if ((fd = open(GROUPFILE, O_WRONLY|O_APPEND)) >= 0)
				{
					if (write(fd, ac_entry, size) != size)
						logerr(0, "%s append failed", GROUPFILE);
					close(fd);
				}
			}
		}
		else
			logerr(0, "grpPipe read failed");
		if (size && !access(GEN_PASSWD, X_OK))
		{
			sfsprintf(ac_entry, sizeof(ac_entry), "%s %s %s", GEN_PASSWD, "-g", request.name);
			runscript(ac_entry, admin);
		}
		FlushFileBuffers(hPipe);
		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);
	}

	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		logmsg(0, "proc_grpThread exception");
		size = 0;
	}

	curr_grp_thrd_cnt--;
	logmsg(0, "proc_grpThread terminating size=%d cnt=%d", size, curr_grp_thrd_cnt);
	return size;
}

static DWORD WINAPI grpThread(LPVOID param)
{
	char*	pipe_name;
	HANDLE	grp_pipe;
	int	max_thread = 0;

	pipe_name = UWIN_PIPE_GROUP;
	for (;;)
	{
		if (!(grp_pipe = createnamedpipe(grp, pipe_name, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, NUM_GRP_PIPES, MAX_DATA, MAX_DATA, NMPWAIT_USE_DEFAULT_WAIT)))
		{
			Sleep(100);
			continue;
		}
		if (!connectnamedpipe(grp) || !createthread(proc_grpThread, grp_pipe, 0))
		{
			CloseHandle(grp_pipe);
			continue;
		}
		if (++curr_grp_thrd_cnt > max_thread)
			max_thread = curr_grp_thrd_cnt;
		logmsg(0, "proc_grpThread count %d/%d", curr_grp_thrd_cnt, max_thread);
	}
	return 0;
}

static int change_group(HANDLE token,SID *group)
{
	TOKEN_PRIMARY_GROUP grp;
	grp.PrimaryGroup = group;
	if(SetTokenInformation(token,TokenPrimaryGroup,(TOKEN_PRIMARY_GROUP*)&grp,sizeof(grp)))
		return 1;
	logerr(0, "SetTokenInformation failed");
	return 0;
}

static DWORD WINAPI startservice(LPVOID param)
{
	Servpass_t*		serv = (Servpass_t*)param;
	char*			servname = serv->servname;
	char*			servid = servname + 7;
	SC_HANDLE		scm,service;
	SERVICE_STATUS		serstat;
	BOOL			ret = FALSE;
	DWORD			br,err;
	HANDLE 			hp,cphand,startservice_pipe,me=GetCurrentProcess();
	UMS_slave_data_t	sr;
	int			i;
	int			fatal = serv->fatal;

	if (!(startservice_pipe = createnamedpipe(startservice, UWIN_PIPE_TOKEN, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT)))
		ExitThread(0);
	if (scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))
	{
		if(service = OpenService(scm, servname, SERVICE_START|SERVICE_QUERY_STATUS))
		{
			if(StartService(service, 0, NULL))
			{
				err = 1;
				/* UMS will wait for 20 seconds UCS to come up */
				for (i=0;i<20 && err;++i)
					if(!QueryServiceStatus(service, &serstat))
					{
						logerr(0, "QueryServiceStatus %s failed", servid);
						break;
					}
					else
						switch (serstat.dwCurrentState)
						{
						case SERVICE_START_PENDING:
							Sleep(1000);
							break;
						case SERVICE_RUNNING:
							ret = TRUE;
						default:
							err = 0;
							break;
						}
			}
			else
			{
				logerr(0, "StartService %s failed", servid);
				RevertToSelf();
				CloseHandle(startservice_pipe);
				CloseServiceHandle(scm);
				ExitThread(0);
			}
			CloseServiceHandle(service);
			if (connectnamedpipe(startservice))
			{
				if(ReadFile(startservice_pipe, &sr, sizeof(sr), &br,NULL) && br > 0)
				{
					if(cphand = OpenProcess(PROCESS_DUP_HANDLE, FALSE, sr.pid))
					{
						if(DuplicateHandle(cphand, sr.atok, me, &hp, 0, TRUE, DUPLICATE_SAME_ACCESS))
						{
							logmsg(0, "%s service started tok=%p", servid,hp);
							CloseHandle(cphand);
							CloseServiceHandle(scm);
							CloseHandle(startservice_pipe);
							ExitThread((DWORD)hp);
						}
						logerr(0, "DuplicateHandle failed");
						CloseHandle(cphand);
					}
					else
						logerr(0, "unable to open process %d", sr.pid);
				}
				else
					logerr(0, "ReadFile failed");
				disconnectnamedpipe(startservice);
			}
		}
		else
		{
			err = GetLastError();
			logerr(0, "OpenService %s failed", servid);
			if(fatal && err == ERROR_SERVICE_DOES_NOT_EXIST)
				logmsg(0, "ucs service %s has not been installed -- install the service before any setuid() call can be made", servid);
		}
		CloseServiceHandle(scm);
	}
	else
		logerr(0, "OpenSCManager %s failed", servid);
	CloseHandle(startservice_pipe);
	ExitThread(0);
}

static BOOL gettoken(char* username, HANDLE* token, SID* group, int fatal)
{
	char servname[256]="UWIN_CS";
	int sidbuf[MAXSID];
	HANDLE cphand=NULL, me=GetCurrentProcess();
	DWORD br;
	int i,r;
	struct passwd *passwda;
	BOOL ret = FALSE;
	Servpass_t serv;

	serv.servname = servname;
	serv.fatal = fatal;
	if (!(passwda=getpwnam(username)))
	{
		logmsg(0, "error in getting the passwd structure for %s",username);
		return (FALSE);
	}
	if(!group && uwin_uid2sid(passwda->pw_gid,(SID*)sidbuf,sizeof(sidbuf)))
		group = (SID*)sidbuf;
	strcat(servname, username);
	for(i=0;i<(int)strlen(servname);i++)
		if(servname[i] == '/')
			servname[i]= '#';
	for(i=0;i<MAXI_CLIENT;i++)
	{
		if(!clienttoken[i].cliname)
			break;
		/* Token is already present */
		if (!_stricmp(servname,clienttoken[i].cliname))
		{
			if(clienttoken[i].pending)
			{
				cphand = clienttoken[i].clitok;
				goto harvest;
			}
			ret = TRUE;
			goto gotit;
		}
	}
	if (createthread(startservice, &serv, &cphand))
	{
	harvest:
		r = WaitForSingleObject(cphand,15000);
		if(r==WAIT_OBJECT_0)
		{
			GetExitCodeThread(cphand,&br);
			CloseHandle(cphand);
			logmsg(0,"ucs returned tok=%x",br);
			if(br)
			{
				ret = TRUE;
				clienttoken[i].clitok = (HANDLE)br;
				if(clienttoken[i].cliname=(char *)malloc(sizeof(char)*strlen(servname) + 1))
				{
					clienttoken[i].pending = FALSE;
					strcpy(clienttoken[i].cliname, servname);
					logmsg(0, "store index %d name=%s value=%p",i,clienttoken[i].cliname,clienttoken[i].clitok);
				}
				else
					logmsg(0,"malloc failed");
			}
		}
		else
		{
			logmsg(0,"ucs still running");
			if(clienttoken[i].cliname=(char *)malloc(sizeof(char)*strlen(servname) + 1))
			{
				strcpy(clienttoken[i].cliname, servname);
				clienttoken[i].clitok = cphand;
				clienttoken[i].pending = TRUE;
			}
		}
	}
gotit:
	if(ret)
	{
		if(!DuplicateHandle(me, clienttoken[i].clitok, me, token, 0, TRUE, DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle failed");
		logmsg(0, "token %d val %ld",clienttoken[i].clitok);
		if(group)
			change_group(*token,(SID*)group);
	}
	logmsg(0, "gettoken ret=x%x", ret);
	return ret;
}

/*  Query for the specified key and return the result */
static int getflag(const char *name, HKEY key, int dflt)
{
	long ret, type, datalen=MAXVAL;
	DWORD   data;
	datalen=MAXVAL;

	ret = RegQueryValueEx(key, name, NULL, &type, (LPBYTE)&data, &datalen);
	if(ret == ERROR_SUCCESS && type == REG_DWORD)
		return data;
	SetLastError(ret);
	logerr(0, "RegQueryValueEx %s failed", name);
	return dflt;
}

static void get_global_config(void)
{
	long ret;
	HKEY   key;
	char subkey[256];

	if(!*release)
	{
		logmsg(0, "registry release key not found -- default config assumed");
		return;
	}
	/*  Open the key if it exists */
	sfsprintf(subkey, sizeof(subkey), "%s\\%s\\UMS", UWIN_KEY, release);
	ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ|KEY_WOW64_64KEY, &key);
	if(ret != ERROR_SUCCESS)
	{
		if(ret != ERROR_FILE_NOT_FOUND)
		{
			SetLastError(ret);
			logerr(0, "cannot read registry key %s -- default config assumed", subkey);
		}

		/* keys are inaccessible or absent -- assume config defaults */
		setuidflag = 1;
		passwdflag = 1;
		domainenumflag = 0;
		genpassflag = 1;
		return;
	}
	setuidflag =  getflag("setuidFlag", key, 1);
	passwdflag = getflag("pswdFlag",key,1);
	domainenumflag = getflag("domainEnumFlag",key,0);
	genpassflag = getflag("genLocalPasswd",key,1);
	RegCloseKey(key);
}

static char *getname(id_t id, char *name, size_t size)
{
	SID_NAME_USE puse;
	wchar_t domain[PATH_MAX];
	wchar_t uname[PATH_MAX];
	wchar_t buff[2*PATH_MAX+2] = L"";
	char *ret = NULL;
	int dsize=sizeof(domain), sbuff[64],usize=sizeof(uname);
	if(uwin_uid2sid(id,(SID*)sbuff,sizeof(sbuff))<=0)
		return ret;
	if(!LookupAccountSidW(NULL, (SID*)sbuff,uname,&usize, domain, &dsize, &puse))
	{
		logerr(0, "LookupAccountSid failed");
		return ret;
	}
	if (_wcsicmp(domain,gszComName) && (!IsDC || _wcsicmp(domain,gszDomName)))
		swprintf(buff,sizeof(buff),L"%ls/",domain);
	wcscat(buff,uname);
	if (wcslen(buff) <= size)
	{
		wcstombs(name,buff,size);
		ret = name;
	}
	return ret;

}

/*
 *  handle setuid() and setgid() type requests
 */
static void setuidcall(void* dummy)
{
	HANDLE setuidcall_pipe,event,atok=0,ph,me=GetCurrentProcess();
	struct passwd *pwd;
	UMS_slave_data_t ret;
	int maskval,nr, sidbuf[24];
	SID *group;
	int data[(sizeof(UMS_setuid_data_t)+NGROUPS_MAX*sizeof(gid_t))/sizeof(int)];
	UMS_setuid_data_t *sdata= (UMS_setuid_data_t*)data;

	maskval=umask(0077);
	if (!log_user && !(log_user = (LOG_f)getapi_addr("advapi32.dll","LogonUserA",(HMODULE*)&atok)))
	{
		logerr(0, "LogonUserA failed");
		if (atok)
			FreeLibrary(atok);
		ExitThread(1);
	}
	if (!(setuidcall_pipe = createnamedpipe(setuidcall, UWIN_PIPE_SETUID, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT)))
		return;
	umask(maskval);
	for (;;)
	{
		ret.atok = event = ph = atok = 0;
		SetLastError(0);
		if(!connectnamedpipe(setuidcall))
			continue;
		if(!ReadFile(setuidcall_pipe,data,sizeof(data),&nr,NULL) || nr<sizeof(UMS_setuid_data_t))
		{
			logerr(0, "ReadFile failed");
			goto err;
		}
		if(!(ph=OpenProcess(PROCESS_DUP_HANDLE,FALSE,sdata->pid)))
		{
			logerr(0, "OpenProcess failed");
			goto err;
		}
		if(!DuplicateHandle(ph,sdata->event,me,&event,PROCESS_ALL_ACCESS,
			FALSE, DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateProcess failed");
		group = 0;
		logmsg(0, "setuidcall uid=%d gid=%d dup=%d",sdata->uid,sdata->gid,sdata->dupit);
		if(sdata->gid==UMS_MK_TOK && (nr> sizeof(UMS_setuid_data_t)))
		{
			char *passwd = (char*)data+sizeof(UMS_setuid_data_t);
			char *domain=passwd;
			char name[2*PATH_MAX+2];
			logmsg(0, "mktok call with id = %d",sdata->uid);
			while(*domain++);
			if(!(pwd=getpwuid(sdata->uid)))
				goto err;
			if (!getname(sdata->uid,name,sizeof(name)))
				strcpy(name,pwd->pw_name);
			if(!((*log_user)(name,domain,passwd, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &atok)))
			{
				logerr(0, "LogonUser %s failed", pwd->pw_name);
				goto err;
			}
		}
		else if(sdata->gid!=UMS_NO_ID)
		{
			logmsg(0, "setid call with gid = %d",sdata->gid);
			if(uwin_uid2sid(sdata->gid,(SID*)sidbuf,sizeof(sidbuf)))
				group = (SID*)sidbuf;
			else
				logmsg(0, "cannot find group %ld", sdata->gid);
		}
		if(sdata->uid==1)
		{
			if (!OpenProcessToken(me,TOKEN_ALL_ACCESS&~TOKEN_ADJUST_SESSIONID,&atok))
				logerr(0, "OpenProcessToken failed");
			sdata->dupit = 1;
		}
		else if(sdata->uid!=UMS_NO_ID)
		{
			if (sdata->gid != UMS_MK_TOK)
			{
				char name[2*PATH_MAX+2];
				logmsg(0, "setid call with uid = %d",sdata->uid);
				if ((pwd = getpwuid(sdata->uid)))
				{
					if (!getname(sdata->uid,name,sizeof(name)))
						strcpy(name,pwd->pw_name);
					if(!gettoken(name,&atok,group,1))
					{
						logmsg(0, "no uid serice for= %s",name);
						goto err;
					}
				}
				else
					goto err;
			}
			sdata->dupit = 1;
		}
		else if(group)
		{
			logmsg(0, "try to change gid to%ld", sdata->gid);
			if(!DuplicateHandle(ph,sdata->tok,me,&atok,PROCESS_ALL_ACCESS,
				FALSE, DUPLICATE_SAME_ACCESS))
			{
				logerr(0, "DuplicateHandle failed");
				goto err;
			}
			if(!change_group(atok,group))
			{
				logmsg(0, "change group to% ld failed", sdata->gid);
				CloseHandle(atok);
				atok = 0;
				goto err;
			}
			logmsg(0, "change group to% ld ok atok=%x dupit=%d", sdata->gid,atok,sdata->dupit);
			if(sdata->dupit)
			{
				HANDLE hp=atok;
				if(!DuplicateTokenEx(hp, MAXIMUM_ALLOWED, NULL,
					SecurityAnonymous, TokenPrimary, &atok))
				{
					logerr(0, "DuplicateToken failed");
					atok = 0;
				}
				CloseHandle(hp);
			}
			else
				ret.atok = atok;

		}
		if(sdata->gid!=UMS_MK_TOK && (nr> sizeof(UMS_setuid_data_t)))
		{
			/* handle setgrps call */
			int i=0;
			nr = (nr-sizeof(UMS_setuid_data_t))/sizeof(DWORD);
			for(i=0; i < nr ; i++)
			{
				logmsg(0, "setgid calls is  gid = %d",sdata->groups[i]);
			}
			ret.atok = 0;
		}
		if(sdata->dupit  && !DuplicateHandle(me, atok, ph, &ret.atok, PROCESS_ALL_ACCESS, TRUE, DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle failed");
	err:
		ret.pid = GetLastError();
		if(event)
		{
			if(!SetEvent(event))
				logerr(0, "SetEvent failed");
			CloseHandle(event);
		}
		if(!WriteFile(setuidcall_pipe, &ret, sizeof(ret), &nr, NULL))
			logerr(0, "WriteFile failed");
		else
			FlushFileBuffers(setuidcall_pipe);
		disconnectnamedpipe(setuidcall);
		if(ph)
			CloseHandle(ph);
		if(atok)
			CloseHandle(atok);
	}
}

static BOOL setprivilege(char *pri, BOOL flag)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES ptp;
	DWORD len=sizeof(TOKEN_PRIVILEGES);
	HANDLE myatok;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &myatok))
	{
		logerr(0, "OpenProcessToken failed");
		return FALSE;
	}

	if(!LookupPrivilegeValue(NULL, pri, &luid))
	{
		logerr(0, "LookupPrivilegeValue %s failed", pri);
		CloseHandle(myatok);
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	if (!AdjustTokenPrivileges(myatok, FALSE, &tp, len, &ptp, &len))
	{
		logerr(0, "AdjustTokenPrivileges failed");
		CloseHandle(myatok);
		return FALSE;
	}

	ptp.PrivilegeCount = 1;
	ptp.Privileges[0].Luid = luid;

	if(flag)
		ptp.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	else
		ptp.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & ptp.Privileges[0].Attributes);

	if (!AdjustTokenPrivileges(myatok, FALSE, &ptp, len, NULL, NULL))
	{
		logerr(0, "AdjustTokenPrivileges failed");
		CloseHandle(myatok);
		return FALSE;
	}

	CloseHandle(myatok);
	return TRUE;
}

static HANDLE getclient_token(HANDLE hpipe, DWORD ntpid, HANDLE *tok)
{
	HANDLE ph,hp;

	*tok = 0;
	if (!(ph = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ntpid)))
	{
		logerr(0, "OpenProcess failed");
		return 0;
	}
	if (!ImpersonateNamedPipeClient(hpipe))
	{
		logerr(0, "ImpersonateNamedPipeClient failed");
		CloseHandle(ph);
		return 0;
	}
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ALL_ACCESS, FALSE, &hp))
		logerr(0, "OpenThreadToken failed");
	else
		*tok = hp;
	RevertToSelf();
	return ph;
}

static char *startnewsuidproc(HANDLE* atok)
{
	static 	DWORD buffer[PATH_MAX/sizeof(DWORD)];
	char	*cmd=0;
	HANDLE	hp,cphand=0;
	DWORD	br;

	*atok = 0;
	cr.child = 0;
	if (!ReadFile(request_pipe, buffer, sizeof(buffer), &br, NULL))
	{
		logmsg(0, "read from request_pipe failed err=%d nread=%d",GetLastError(),br);
		return(0);
	}
	if (!(cphand = getclient_token(request_pipe, buffer[0], &hp)))
		logmsg(0, "getclient_token failed ntpid=%x");
	else
	{
		if (hp)
		{
			if (!DuplicateTokenEx(hp, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenPrimary, atok))
			{
				logerr(0, "unable to dup token");
				*atok = hp;
			}
			else
				CloseHandle(hp);
		}
		if (!WriteFile(request_pipe, &cr, sizeof(cr), &br, NULL))
			logerr(0, "WriteFile failed");
		else if (!ReadFile(request_pipe, (void *)&cr, sizeof(cr), &br, NULL))
			logerr(0, "ReadFile failed");
		else if (br == 0)
			logmsg(0, "read zero bytes from request_pipe");
		else if (!cr.child)
			logmsg(0, "remote thread returned NULL child handle");
		else
			cmd = (char*)&buffer[1];
	}
	if (!cr.child)
	{
		logmsg(0, "remote thread returned NULL child handle");
		cmd = 0;
	}
	else if(!DuplicateHandle(cphand, cr.child, GetCurrentProcess(), &cr.child, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateHandle failed");
		cmd = 0;
	}
	cr.err = 0;
	if (cphand)
		CloseHandle(cphand);
	if (!cmd && *atok)
	{
		CloseHandle(*atok);
		*atok = 0;
	}
	return(cmd);
}

static mode_t getfilemode(char *file)
{
	struct stat statb;
	char path[PATH_MAX];
	uwin_unpath(file,path,sizeof(path));
	if(stat(path,&statb) <0)
	{
		logmsg(0, "stat filed failed on %s err=%d",path,errno);
		return 0;
	}
	return statb.st_mode;
}

static BOOL getfileowner(const char *file, char *username, SID* sid, mode_t mode)
{
	int buf[1024],sidbuf[MAXSID];
	char refdom[256],domain[256];
	SECURITY_DESCRIPTOR *psd;
	DWORD sdsize=sizeof(buf),usize=MAXUSER,rsdsize,domsize=sizeof(domain);
	SID_NAME_USE siduse=SidTypeUser;
	SID * psid, *rsid=(SID*)sidbuf;
	DWORD type=0;
	BOOL owndef;
	id_t uid;
	if(mode&S_ISUID)
		type |= OWNER_SECURITY_INFORMATION;
	if(mode&S_ISGID)
		type |= GROUP_SECURITY_INFORMATION;

	psd = (SECURITY_DESCRIPTOR *)buf;

	if(!GetFileSecurity(file, type, psd, sdsize, &rsdsize))
	{
		logerr(0, "GetFileSecurity failed");
		return FALSE;
	}

	if(type&OWNER_SECURITY_INFORMATION)
	{
		if(!GetSecurityDescriptorOwner(psd, &psid, &owndef))
		{
			logerr(0, "GetSecurityDescriptorOwner failed");
			return FALSE;
		}

		rsdsize = sizeof(refdom);
		if(!LookupAccountSid(NULL, psid, username, &usize, refdom, &rsdsize, &siduse))
		{
			logerr(0, "LookupAccountSid failed");
			return FALSE;
		}
		sdsize = MAXSID;
		uid = uwin_sid2uid(psid);
		if(uid >= (1<<18))
		{
			memmove(&username[rsdsize+1],username,usize+1);
			memcpy(username,refdom,rsdsize);
			username[rsdsize]='/';
		}
		if (!_stricmp("Administrators", username))
			strcpy(username, "administrator");
	}
	else
		strcpy(username,"NotUsed");
	if(type&GROUP_SECURITY_INFORMATION)
	{
		if(!GetSecurityDescriptorGroup(psd, &psid, &owndef))
		{
			logerr(0, "GetSecurityDescriptorGroup failed");
			return FALSE;
		}
		CopySid(MAXSID,sid,psid);
	}
	return TRUE;
}

static void setuidbit_thread(void* arg1)
{
	char *cmdname,username[MAXUSER];
	DWORD bw;
	HANDLE catok, myphand, atok;
	int sidbuf[MAXSID];
	SID *group;
	mode_t mode;

	myphand = GetCurrentProcess();
	if (!setprivilege(SE_DEBUG_NAME, TRUE))
		logmsg(0, "setprivilege failed");
	signal(SIGQUIT, SIG_IGN);
	for (;;)
	{
		group = (SID*)sidbuf;
		if(!connectnamedpipe(request))
			continue;
		memset(&cr, 0, sizeof(cr));
		cr.err = (DWORD)-1;
		catok = 0;
		atok = 0;

		/*
		 * Setuid bit request has come.
		 * Check for the global variables. If set then proceed otherwise
		 * disconnect pipe and go back to wait.
		 */
		if (!setuidflag)
		{
			logmsg(0, "setuid flag disabled");
			goto loop;
		}
		logmsg(0, "received setuid request");
		if (!(cmdname=startnewsuidproc(&atok)))
		{
			logmsg(0, "startnewsuidproc failed");
			goto loop;
		}
		mode = getfilemode(cmdname);
		if (!getfileowner(cmdname, username, group, mode))
		{
			logmsg(0, "getfileowner failed");
			goto loop;
		}
		if (!(mode&S_ISGID))
		{
			int	buffer[256];
			int	rsize = sizeof(buffer);

			if(GetTokenInformation(atok, TokenPrimaryGroup, (TOKEN_PRIMARY_GROUP*)buffer, sizeof(buffer), &rsize))
				CopySid(sizeof(sidbuf), group, ((TOKEN_PRIMARY_GROUP*)buffer)->PrimaryGroup);
			else
			{
				logerr(0, "GetTokenInformation failed");
				group = 0;
			}
		}
		logmsg(0, "owner %s mode=%o", username, mode);
		if (mode&S_ISUID)
		{
			if(atok)
			{
				CloseHandle(atok);
				atok = 0;
			}
			if (!gettoken(username, &atok, group, 1))
			{
				logerr(0, "gettoken failed");
				goto loop;
			}
		}
		else if (!atok || !change_group(atok,group))
		{
			logerr(0, "unable to use token");
			goto loop;
		}
		logmsg(0, "successfully got the access token for %s", username);
		if (!DuplicateHandle(myphand, atok, cr.child, &catok, PROCESS_ALL_ACCESS, TRUE, DUPLICATE_SAME_ACCESS))
		{
			logerr(0, "DuplicateHandle failed");
			catok = 0;
			goto loop;
		}
		logmsg(0, "successfully duplicated the access token");
		cr.atok = catok;
loop:
		if (!WriteFile(request_pipe, &cr, sizeof(cr), &bw, NULL))
			logerr(0, "WriteFile failed");
		if (atok)
			CloseHandle(atok);
		if (cr.child)
			CloseHandle(cr.child);
		if (catok)
			CloseHandle(catok);
		DisconnectNamedPipe(request_pipe);
	}
}

/*
 * wait for init process to abort
 */
static DWORD WINAPI wait_init(LPVOID param)
{
	HANDLE		hpinit;
	DWORD		status, ntpid=uwin_ntpid(1);
	if(hpinit = OpenProcess(PROCESS_ALL_ACCESS,FALSE,ntpid))
	{
		WaitForSingleObject(hpinit,INFINITE);
		if(GetExitCodeProcess(hpinit,&status))
			logmsg(0, "init process aborts status=%x",status);
		else
			logerr(0, "init process aborts and GetExitCodeProcess failed");
	}
	return 0;
}

/*
 * duplicate event and token from requested process
 */
static HANDLE dupclient_token(HANDLE hpipe, UMS_remote_proc_t *rp)
{
	HANDLE ph,hp=0,me=GetCurrentProcess();
	if(!(ph=OpenProcess(PROCESS_ALL_ACCESS, FALSE, rp->ntpid)))
	{
		logerr(0, "OpenProcess failed");
		return 0;
	}
	if(!ImpersonateNamedPipeClient(hpipe))
	{
		logerr(0, "ImpersonateNamedPipe failed");
		CloseHandle(ph);
		return 0;
	}
	if(!DuplicateHandle(ph,rp->event,me,&rp->event,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateEvent failed");
		CloseHandle(ph);
		RevertToSelf();
		return 0;
	}
	if(!DuplicateHandle(ph,rp->tok,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateTok failed");
		CloseHandle(ph);
		ph = 0;
	}
	RevertToSelf();
	if(!ph)
		return 0;
	if(!DuplicateTokenEx(hp, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenPrimary, &rp->tok))
	{
		logerr(0, "DuplicateToken failed");
		CloseHandle(ph);
		ph = 0;
	}
	CloseHandle(hp);
	return ph;
}

static void dup_handles(HANDLE ph, STARTUPINFO *sp,int close)
{
	HANDLE hp,me=GetCurrentProcess();
	int i,n,maxfd = *((int*)(sp->lpReserved2));
	char *handle = (HANDLE)((char*)sp->lpReserved2+sizeof(int)+maxfd);
	for(n=0; n<2; n++)
	{
		for(i=0; i < maxfd; i++,handle +=sizeof(HANDLE))
		{
			memcpy(&hp,handle,sizeof(HANDLE));
			if(!hp || hp==INVALID_HANDLE_VALUE)
				continue;
			if(close)
			{
				CloseHandle(hp);
				continue;
			}
			if(DuplicateHandle(ph,hp,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS))
				memcpy(handle,&hp,sizeof(HANDLE));
			else
				logerr(0, "DuplicateHandle failed");
			if(!n)
			{
				if(!i)
					sp->hStdInput = hp;
				else if(i==1)
					sp->hStdOutput = hp;
				else if(i==2)
					sp->hStdError = hp;
			}
		}
		if(maxfd*sizeof(HANDLE)+maxfd+2*sizeof(int) <= sp->cbReserved2)
			return;
		handle += sizeof(int);
	}

}

/*
 * This thread will create a process with a given token
 * and make it appear as if the parent were the process
 * that asked to create it
 */

static DWORD WINAPI proc_thread(void *arg)
{
	static CPU_f CPU;
	HANDLE proc_thread_pipe,ph,me=GetCurrentProcess();
	PROCESS_INFORMATION pinfo;
	STARTUPINFO sinfo;
	SECURITY_ATTRIBUTES sattr;
	UMS_remote_proc_t pp;
	SIZE_T r;
	DWORD n;
	if(ph = GetModuleHandle("advapi32.dll"))
		CPU = (CPU_f)GetProcAddress(ph,"CreateProcessAsUserA");
	if(!CPU)
	{
		logerr(0, "CreateProcessAsUserA failed");
		ExitThread(1);
	}
	sattr.nLength = sizeof(sattr);
	sattr.bInheritHandle = TRUE;
	sattr.lpSecurityDescriptor = nulldacl();
	memset(&sinfo, 0, sizeof(sinfo));
	if (!(proc_thread_pipe = createnamedpipe(proc_thread, UWIN_PIPE_THREAD, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT)))
		ExitThread(1);
	for (;;)
	{
		ph = 0;
		SetLastError(0);
		if(!connectnamedpipe(proc_thread))
			goto fail;
		if(!ReadFile(proc_thread_pipe,&pp,sizeof(pp),&n,NULL) || n!=sizeof(pp))
			goto fail;
		if(!(ph = dupclient_token(proc_thread_pipe,&pp)))
			goto fail;
		if(pp.len > sizeof(pdata))
			goto fail;
		if(!ReadProcessMemory(ph,pp.addr,pdata,pp.len,&r))
		{
			logerr(0, "ReadProcessMemory failed");
			goto fail;
		}
		sinfo.lpTitle = ODDR(pdata,pp.title);
		sinfo.cbReserved2 = pp.rsize;
		sinfo.lpReserved2 = ODDR(pdata,pp.reserved);
		if(pp.rsize)
			dup_handles(ph,&sinfo,0);
		logmsg(0, "execute %s",ODDR(pdata,pp.cmdname));
		pinfo.hProcess = pp.pinfo.hProcess;
		pinfo.hThread = pp.pinfo.hThread;
		pinfo.dwProcessId = pp.pinfo.dwProcessId;
		pinfo.dwThreadId = pp.pinfo.dwThreadId;
		if(!((*CPU)(pp.tok, ODDR(pdata, pp.cmdname), ODDR(pdata, pp.cmdline), &sattr, &sattr, 1, pp.cflags, ODDR(pdata, pp.env), ODDR(pdata, pp.curdir), &sinfo, &pinfo)))
		{
			logerr(0, "createproc failed");
			goto fail;
		}
		else logmsg(0, "execute ok");
		if(!DuplicateHandle(me,pp.pinfo.hProcess,ph,&pp.pinfo.hProcess,0,FALSE,DUP_MOVE))
		{
			logerr(0, "pduphandle failed");
			goto fail;
		}
		if(!DuplicateHandle(me,pp.pinfo.hThread,ph,&pp.pinfo.hThread,0,FALSE,DUP_MOVE))
			logerr(0, "tduphandle failed");
			goto fail;
		if(pp.rsize)
			dup_handles(ph,&sinfo,1);
fail:
		pp.err = GetLastError();
		if(ph)
			CloseHandle(ph);
		if(!SetEvent(pp.event))
			logerr(0, "SetEvent failed");
		if(!WriteFile(proc_thread_pipe,&pp,sizeof(pp),&n,NULL))
			logerr(0, "WriteFile failed");
		CloseHandle(pp.event);
		CloseHandle(pp.tok);
		disconnectnamedpipe(proc_thread);
	}
	CloseHandle(proc_thread_pipe);
	ExitThread(0);
	return 0;
}

static void msthread(void* arg1)
{
	HANDLE  hEvent;
	HKEY keyhandle1;
	long ret;
	char subkey[256];

	createthread(setuidcall, 0, 0);
	createthread(setuidbit_thread, 0, 0);
	createthread(wait_init, 0, 0);
	createthread(proc_thread, 0, 0);
	if(!*release)
	{
		logmsg(0, "registry release key not found -- config changes ignored");
		return;
	}

	/* Create the event for change Notification. */
	if(!(hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		logerr(0, "CreateEvent failed");

	// Open the ums registry key
	sfsprintf(subkey, sizeof(subkey), "%s\\%s\\UMS", UWIN_KEY, release);
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ|KEY_WOW64_64KEY, &keyhandle1) == ERROR_FILE_NOT_FOUND)
	{
		/*
		 * Registry keys are absent.
		 * Exit the thread, as it need not wait for any kind
		 * of Notification.
		 */

		logmsg(0, "registry key %s not found -- config changes ignored", subkey);
		return;
	}
	for (;;)
	{
		ResetEvent(hEvent);
		// Set the handle for change notification
		ret = RegNotifyChangeKeyValue(keyhandle1,
			  FALSE, // only this key
			  REG_NOTIFY_CHANGE_LAST_SET,
			  hEvent,
			  TRUE);
		if(ret != ERROR_SUCCESS)
		{
			/* Error */
			SetLastError(ret);
			logerr(0, "RegNotifyChangeKeyValue failed");
		}

		// Wait for the Notification
		ret = WaitForSingleObject(hEvent,INFINITE);
		if(ret == WAIT_FAILED)
			logerr(0, "WaitForSingleObject failed");
		else if(ret == WAIT_OBJECT_0)	// successfully notified
		{
			//Query the registry for new values
			logmsg(0, "msthread config update notification");
			get_global_config();
		}

	}
}

static DWORD WINAPI LoginVerifyThread(LPVOID param)
{
	DWORD dwRead,dwWrite,Err=-1;
	HANDLE LoginVerifyThread_pipe=INVALID_HANDLE_VALUE;
	HANDLE atok=0;
	char user[2576*3],*domain,*passwd;

	if(!log_user && !(log_user = (LOG_f)getapi_addr("advapi32.dll","LogonUserA",(HMODULE*)&atok)))
	{
		logmsg(0, "LogonUser is not supported in the current OS. LoginVerify thread is aborting");
		if (atok)
			FreeLibrary(atok);
		return 0;
	}
	if (LoginVerifyThread_pipe = createnamedpipe(LoginVerifyThread, UWIN_PIPE_LOGIN, PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT, 1, 256, 256, NMPWAIT_WAIT_FOREVER))
	{
		logmsg(0, "waiting for LoginVerify connections");
		for (;;)
		{
			if(connectnamedpipe(LoginVerifyThread))
			{
				if(ReadFile(LoginVerifyThread_pipe,(BYTE*)user,sizeof(user),&dwRead,NULL) && dwRead > 0)
				{
					user[dwRead] = 0;
					if (domain = strchr(user,'+'))
					{
						*domain = 0;
						domain++;
						if (passwd = strchr(domain,'+'))
						{
							*passwd = 0;
							passwd++;
						}
						if (!(*log_user)(user, domain, passwd, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, (HMODULE*)&atok))
						{
							Err = GetLastError();
							CloseHandle(atok);
						}
						else
						{
							Err = 0;
						}

					}
					if (WriteFile(LoginVerifyThread_pipe,&Err,sizeof(Err),&dwWrite,NULL))
						FlushFileBuffers(LoginVerifyThread_pipe);
				}
				else
					logerr(0, "ReadFile %s failed", UWIN_PIPE_LOGIN);
				disconnectnamedpipe(LoginVerifyThread);
			}
		}
		logmsg(0, "LoginVerifyThread terminating");
		CloseHandle(LoginVerifyThread_pipe);
	}
	return 1;
}

static BOOL initservice(void)
{
	int rt;
	SID_NAME_USE puse;
	int i,val;
	int sidbuff[64];
	SID *sid = (SID*)sidbuff;
	char subkey[PATH_MAX], domain[PATH_MAX];
	HKEY uwin_key;
	DWORD type, rel_len = sizeof(release);
	char name[50];

	scmstatus(SERVICE_START_PENDING, NO_ERROR, 5000);

	/* make sure that /etc/passwd and /etc/group are symlinks */
	if(readlink("/etc/passwd",subkey,sizeof(subkey)) < 0)
	{
		logerr(0, "readlink %s failed", "/etc/passwd");
		mkdir("/var/etc", EDMODES);
		chown("/var/etc", 0, 1);
		unlink("/etc/passwd");
		unlink("/etc/group");
		if (symlink(PASSWDFILE,"/etc/passwd"))
			logerr(0, "symlink %s failed", "/etc/passwd");
		if (symlink(GROUPFILE,"/etc/group"))
			logerr(0, "symlink %s failed", "/etc/group");
	}
	/* /etc/pass and /etc/group cannot have ugo+x -- it foils mmap() */
	if (chmod(PASSWDFILE, RDMODES))
		logmsg(0, "chmod %s failed", PASSWDFILE);
	if (chmod(GROUPFILE, RDMODES))
		logmsg(0, "chmod %s failed", GROUPFILE);
	logmsg(0, "create " LOGDIR "ucs");
	if((i=creat(LOGDIR "ucs", WRMODES))>=0)
	{
		chmod(LOGDIR "ucs", WRMODES);
		close(i);
	}
	else
		logmsg(0, "create %s failed", LOGDIR "ucs");
	root_len = uwin_path("/",unix_root,sizeof(unix_root));

	//initializing Local Token storage to NULL
	for(i=0;i<MAXI_CLIENT;i++)
		clienttoken[i].cliname = NULL;
	/* get a token for Administrator */
	strcpy(subkey,"Administrator");
	rt =  sizeof(sidbuff);
	val = sizeof(domain);
	scmstatus(SERVICE_START_PENDING, NO_ERROR, 5000);
	local_account(name,sizeof(name));
	logmsg(0,"local account name %s",name);
	if(!LookupAccountName(NULL, name, (SID*)sid, &rt, domain, &val, &puse))
	{
		logerr(0, "LookupAccountName %s failed", name);
		sid = get_adminsid();
	}
	if(sid)
	{
		val = sizeof(domain);
		rt =  sizeof(subkey);
		i = *GetSidSubAuthorityCount(sid);
		*GetSidSubAuthority((SID*)sid, i-1) = DOMAIN_USER_RID_ADMIN;
		if(!LookupAccountSid(NULL, (SID*)sid, subkey, &rt, domain, &val, &puse))
			logerr(0, "LookupAccountSid %s failed", subkey);
	}
	else
		logerr(0, "get_adminsid failed");
	scmstatus(SERVICE_RUNNING, NO_ERROR, 0);
	logmsg(0, "Administrator account name %s", subkey);
	if(gettoken(subkey, &admin, NULL, 0))
		logmsg(0, "Administrator token=%x", admin);
	else
	{
		struct stat statb;
		rt =  sizeof(sidbuff);
		val = sizeof(domain);
		if(stat("/etc/ums.exe",&statb)>=0 && uwin_uid2sid(statb.st_uid, (SID*)sid, rt))
		{
			if(LookupAccountSid(NULL,(SID*)sid, subkey, &rt, domain, &val, &puse))
			{
				char *cp;
				if(statb.st_uid >= 0x10000)
				{
					cp = domain;
					cp[val++] = '/';
					strncpy(&cp[val],subkey,sizeof(domain)-val);
				}
				else
					cp = subkey;
				logmsg(0, "gettoken for %s",cp);
				if(gettoken(cp,&admin,NULL,0))
					logmsg(0, "running /etc/rc under account name=%s token=%x",cp, admin);
			}
			else
				logerr(0, "LookupAccountSid failed");
		}
	}
	setgid(3);

	/*
	 * determine the UWIN release
	 */
	sfsprintf(subkey, sizeof(subkey), "%s", UWIN_KEY);
	if(rt = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ|KEY_WOW64_64KEY, &uwin_key))
		logerr(0, "could not open key \"%s\"", subkey);
	else
	{
		/* Get current UWIN release */
		if(rt = RegQueryValueEx(uwin_key, UWIN_KEY_REL, NULL, &type, (LPBYTE)release, &rel_len))
			logerr(0, "could not query key \"%s\\...\\%s\"", subkey, UWIN_KEY_REL);
		else if(type != REG_SZ)
		{
			logmsg(0, "key \"%s\\...\\%s\" type=%d is not a 0-terminated string", subkey, UWIN_KEY_REL, type);
			*release = 0;
		}
		else
			logmsg(0, "UWIN release %s", release);
		RegCloseKey(uwin_key);
	}

	/*
	 * query the UWIN config registry and update the ums global config
	 */
	get_global_config();

	/*
	 * initialize the local passwd and group files
	 */
	if (genpassflag)
		genpg(0);

	/*
	 * create the service request pipe
	 */
	if(!(serviceevent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		logerr(0, "CreateEvent failed");
		return FALSE;
	}
	if (!(request_pipe = createnamedpipe(request, UWIN_PIPE_UMS, PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT, 1, 0, 0, NMPWAIT_USE_DEFAULT_WAIT)))
		return FALSE;

	/*
	 * start the service threads
	 */
	if (!createthread(msthread, 0, &msthreadhandle))
		return FALSE;
	if (!createthread(LoginVerifyThread, 0, 0))
		return FALSE;
	if (!createthread(passwdThread, 0, 0))
		return FALSE;
	if (!createthread(grpThread, 0, 0))
		return FALSE;

	/*
	 * enumerate the domain logins/groups if requested
	 */
	if (genpassflag && domainenumflag && !IsDC && IsDOM)
		createthread(creatpg, 0, 0);
	return TRUE;
}

/*
 * make sure that token has the privileges to perform
 * this operation
 */
static is_superuser(HANDLE atok)
{
	static int beenhere;
	static char buf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
	char prevstate[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
	TOKEN_PRIVILEGES *newpri=(TOKEN_PRIVILEGES *)buf;
	TOKEN_PRIVILEGES *oldpri=(TOKEN_PRIVILEGES *)prevstate;
	int dummy;
	if(!beenhere)
	{
		LUID bluid, rluid;
		if(!LookupPrivilegeValue(NULL, SE_CREATE_TOKEN_NAME, &bluid) ||
			!LookupPrivilegeValue(NULL, SE_ASSIGNPRIMARYTOKEN_NAME, &rluid))
		{
			logerr(0, "LookupPrivilegeValue failed");
			return 0;
		}
		newpri->PrivilegeCount = 2;
		newpri->Privileges[0].Luid = bluid;
		newpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		newpri->Privileges[1].Luid = rluid;
		newpri->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
		beenhere = 1;
	}
	if(!AdjustTokenPrivileges(atok, FALSE, newpri,sizeof(prevstate),oldpri,&dummy))
	{
		logerr(0, "AdjustTokenPrivileges failed");
		return 0;
	}
	if(!AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0))
		logerr(0, "AdjustTokenPrivileges failed");
	return oldpri->PrivilegeCount==2;
}

void WINAPI msctrl(DWORD op)
{
	switch (op)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		scmstatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		if (!SetEvent(serviceevent))
		{
			logerr(0, "SetEvent failed");
			scmstatus(SERVICE_STOPPED, NO_ERROR, 0);
			kill(0,SIGKILL);
			ExitProcess(0);
		}
		return;
	case SERVICE_CONTROL_PAUSE:
	case SERVICE_CONTROL_CONTINUE:
	case SERVICE_CONTROL_INTERROGATE:
		break;
	case UMS_PW_SYNC_MSG:
		if (domainenumflag && !IsDC && IsDOM)
			createthread(creatpg, 0, 0);
		if (clienttoken[0].cliname)
		{
			free(clienttoken[0].cliname);
			clienttoken[0].cliname = NULL;
		}
		break;
	default:
		logmsg(0, "msctrl unknown op %d", op);
		break;
	}
	scmstatus(SERVICE_RUNNING, NO_ERROR, 0);
}

static int installservice()
{
	SC_HANDLE service, scm;
	TCHAR path[512];
	char *depend = 0;
	int fail = 0;

	if(!GetModuleFileName(NULL, path, sizeof(path)))
	{
		logerr(LOG_STDERR+0, "unable to get executable name");
		return 1;
	}

	if(scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))
	{
		if(service = CreateService(scm,UMS_NAME,UMS_DISPLAYNAME,
			SERVICE_ALL_ACCESS|GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
			SERVICE_WIN32_SHARE_PROCESS, SERVICE_AUTO_START,
			SERVICE_ERROR_NORMAL, path, NULL, NULL, depend,
			NULL, NULL))
		{
			logerr(LOG_STDERR+0, "%s service is installed", UMS_DISPLAYNAME);
			CloseServiceHandle(service);
		}
		else
		{
			logerr(LOG_STDERR+0, "createService failed");
			fail = 1;
		}
		CloseServiceHandle(scm);
	}
	else
	{
		logerr(LOG_STDERR+0, "OpenSCManager failed");
		fail = 1;
	}
	return fail;
}

static int deleteservice(void)
{
	SC_HANDLE service, scm;
	int fail=0;

	if(scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))
	{
		if(service=OpenService(scm, UMS_NAME, SERVICE_ALL_ACCESS))
		{
			if(DeleteService(service))
				logerr(LOG_STDERR+0, "%s service is deleted", UMS_DISPLAYNAME);
			else
			{
				logerr(LOG_STDERR+0, "DeleteService failed");
				fail = 1;
			}
			CloseServiceHandle(service);
		}
		else
		{
			logerr(LOG_STDERR+0, "unable to open %s service", UMS_DISPLAYNAME);
			fail = 1;
		}
		CloseServiceHandle(scm);
	}
	else
	{
		logerr(LOG_STDERR+0, "OpenSCManager failed");
		fail = 1;
	}
	return fail;
}

void unmapfilename(char *path)
{
	register char *cp=path,c;
	int from_root = 0;
	if(root_len>0  && !_memicmp(path,unix_root,root_len))
	{
		int c;
		if(!(c=path[root_len]))
		{
			path[0] = '.';
			path[1] = 0;
			return;
		}
		if(c=='\\' || c=='/')
			from_root = 1;
	}
	if((c=*cp)!='/' && cp[1]==':')
	{
		cp[1] = c;
		*cp = '/';
		cp+=2;
	}
	while(c= *cp++)
	{
		if(c=='\\')
			cp[-1] = '/';
	}
	if(from_root)
	{
		cp = path;
		while(cp[root_len])
			*cp++ = cp[root_len];
		*cp = 0;
	}
}



void err_message(HANDLE hp)
{
	DWORD size=6;
	char data[]="error";
	DWORD written;

	if(!WriteFile(hp,&size,sizeof(DWORD),&written,NULL))
		logerr(0, "WriteFile failed");
	if(!WriteFile(hp,data,(DWORD)strlen(data)+1,&written,NULL))
		logerr(0, "WriteFile failed");
}

void WINAPI msmain(DWORD argc, LPTSTR* argv)
{
	int	fd;

	close(0);
	close(1);
	close(2);
	logmsg(0, "register service handler");
	if(!(statushandle=RegisterServiceCtrlHandler(UMS_NAME, msctrl)))
	{
		logerr(LOG_EVENT+0, "RegisterServiceCtrlHandler failed");
		return;
	}
	if(!initservice())
		logerr(LOG_EVENT+0, "service initialization failed");
	else
	{
		if(!ResetEvent(serviceevent))
		{
			logerr(0, "ResetEvent failed");
			serviceevent = CreateEvent(NULL, TRUE, FALSE, NULL);
		}
		scmstatus(SERVICE_RUNNING, NO_ERROR, 0);
		ResetEvent(serviceevent);

		/*
		 * start the rc services
		 * this is done as late as possible in the initialization
		 * because the rc services may make ums requests
		 */
		runscript("/etc/rc",admin);

		/*
		 * handle requests
		 */
		WaitForSingleObject(serviceevent, INFINITE);
		logmsg(0, "received service event");
		if((fd=open(LOCK_FILE, O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, RDMODES))>=0 || errno!=EEXIST)
		{
			if(fd>=0)
				unlink(LOCK_FILE);
			else
				logerr(0, "lock file create %s failed",LOCK_FILE);
			logmsg(0, "kill all UWIN processes");
			kill(-1,SIGKILL);
		}
		else
			logmsg(0, "termination request with uninstall lock set");
	}
	if(!scmstatus(SERVICE_STOPPED, NO_ERROR, 0))
	{
		Sleep(2000);
		logerr(0, "scmstatus failed");
		kill(0,SIGKILL);
		Sleep(1000);
		ExitProcess(0);
	}
	if (serviceevent)
		CloseHandle(serviceevent);
	if (request_pipe)
		CloseHandle(request_pipe);
	logclose();
}

int main(int argc, char *argv[])
{
#if STANDALONE
	char			subkey[PATH_MAX];
	char			domain[PATH_MAX];
#else
	SERVICE_TABLE_ENTRY	table[] =
	{
		{ UMS_NAME, (LPSERVICE_MAIN_FUNCTION)msmain },
		{ 0, 0 }
	};
#endif
	OSVERSIONINFO		info;

	umask(0);
	log_command = "ums";
	log_service = UMS_NAME;
	log_level = 1;
	logopen(log_command, argv[1] != 0);
	logmsg(0, "%s version %s %s server %d bit", log_command, &ums_version_id[9], argv[1] ? argv[1] : "start", 8 * sizeof(char*));
	if (GetVersionEx(&info) && info.dwPlatformId != VER_PLATFORM_WIN32_NT)
	{
		logmsg(LOG_STDERR+0, "requires Windows NT");
		exit(2);
	}
	GetComputerInfo();
	logmsg(0, "domain DC=%d DOM=%d com=%ls dom=%ls pdc=%ls", IsDC, IsDOM, gszComName, gszDomName, gszPdcName);
#if STANDALONE
	passwdflag = 0;
	logmsg(1, "ums running standalone (not a service)");
	genpg(0);
	logmsg(1, "genpg(0) done");
#else
	switch(argc)
	{
	case 1:
		if (!StartServiceCtrlDispatcher(table))
			logerr(LOG_EVENT+0, "StartServiceCtrlDispatcher failed");
		logmsg(0, "after dispatch");
		break;
	case 2:
		if (!_stricmp("install", argv[1]))
		{
			if (installservice())
			{
				logmsg(LOG_STDERR+0, "service install failed");
				return 1;
			}
			break;
		}
		else if (!_stricmp("delete", argv[1]))
		{
			if (deleteservice())
			{
				logmsg(LOG_STDERR+0, "service delete failed");
				return 1;
			}
			break;
		}
	default:
		logerr(LOG_STDERR+LOG_USAGE+0, "[install|delete]");
		break;
	}
#endif
	return 0;
}
