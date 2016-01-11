#include <windows.h>
#include <uwin.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "getinfo.h"

const char*	log_command;
const char*	log_service;
int		log_level;

wk_getinfo_def wk_getinfo;
query_info_def query_info;
serv_enum_def serv_enum;
api_buffer_free_def api_buffer_free;
get_dcname_def get_dcname;

FARPROC getapi_addr(const char *lib, const char *sym,HMODULE *hp)
{
	char sys_dir[512];
	FARPROC addr;
	int len;
	if (*hp == NULL)
	{
		len = GetSystemDirectory(sys_dir, 512);
		sys_dir[len++] = '\\';
		strcpy(&sys_dir[len], lib);
		if(!(*hp = LoadLibraryEx(sys_dir,0,0)))
			return(0);
	}
	addr = GetProcAddress(*hp,sym);
	return(addr);
}

/*
 * This function returns TRUE if the Local System is a Domain Controller
 *It also retruns TRUE if the Local System belongs to a WorkGroup
 */ 
int getinfo(int option)
{
	NET_API_STATUS stat;
	SERVER_INFO_100 *serv_info;
	DWORD totalread, totalentry, i;
	DWORD mode = SV_TYPE_DOMAIN_CTRL|SV_TYPE_DOMAIN_BAKCTRL;
	int ret = 0;
	wchar_t ser[MAXNAME+2] = L"\\\\",ComName[MAXNAME];
	int comsize = sizeof(ComName);
	void *dcname;
	HMODULE hLibrary = NULL;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(osinfo); 
	if(!GetVersionEx(&osinfo)) 
		return(0);

	if(VER_PLATFORM_WIN32_NT==osinfo.dwPlatformId)
	{
		serv_enum = (serv_enum_def)getapi_addr("Netapi32.dll","NetServerEnum",&hLibrary);
		api_buffer_free = (api_buffer_free_def)getapi_addr("Netapi32.dll","NetApiBufferFree",&hLibrary);
		get_dcname = (get_dcname_def)getapi_addr("Netapi32.dll","NetGetDCName",&hLibrary);

		if (get_dcname==NULL || serv_enum==NULL || api_buffer_free==NULL)
		{
			if (hLibrary != NULL)
			{
				FreeLibrary(hLibrary);
			}
			return(0);
		}
	}
	else
	{
		return(0);
	}
	GetComputerNameW(ComName,&comsize);
	
	switch (option)
	{
	    case 3:  //IsDomain
		/* Determine if local computer is inside a domain.*/
		if((*get_dcname)(NULL, NULL, (LPBYTE *) &dcname)==NERR_Success)
		{
			(*api_buffer_free)(dcname);
			ret = 1;
		}
		return(ret);
	    case 1:
		mode = SV_TYPE_DOMAIN_CTRL;
		break;
	    case 2:
		mode = SV_TYPE_DOMAIN_BAKCTRL;
		break;
	}

	stat=(*serv_enum)(NULL,100,(unsigned char**)&serv_info,MAX_PREFERRED_LENGTH, &totalread,&totalentry,mode,NULL,0);

	if (stat == NERR_Success)
	{
		switch(option)
		{
		    case 1: //IsPDC
			if (totalentry != 1)
			{
				break;
			}
		    case 2: //IsBDC
		    case 4: //IsDC
			for(i=0;i<totalread;i++)
			{
				if(!_wcsicmp((wchar_t*)serv_info[i].sv100_name,ComName))
				{
					ret = 1;
					break;
				}
			}
			break;
		}
		(*api_buffer_free)(serv_info);
	}
	if (hLibrary != NULL)
	{
		FreeLibrary(hLibrary);
	}
	return(ret);
}

int parse_username(char *name)
{
	char *sep; 
	char temp[80];
	WKSTA_INFO_100 *wks_info;
	int local = 1;
	HMODULE hLibrary = NULL;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(osinfo); 
	if(!GetVersionEx(&osinfo)) 
		return(0);

	if(VER_PLATFORM_WIN32_NT==osinfo.dwPlatformId)
	{
		wk_getinfo = (wk_getinfo_def)getapi_addr("netapi32.dll","NetWkstaGetInfo",&hLibrary);
		api_buffer_free = (api_buffer_free_def)getapi_addr("netapi32.dll","NetApiBufferFree",&hLibrary);
		if (wk_getinfo == NULL || api_buffer_free == NULL)
		{
			if (hLibrary != NULL)
			{
				FreeLibrary(hLibrary);
			}
			return(1);
		}
	}
	else
	{
		return(1);
	}


	if ((sep = strchr(name,'/')) != NULL)
	{
		*sep = '\0';
		if (name[0] != '\0')
		{
			local = 0;

			if ((*wk_getinfo)(NULL,100,(LPBYTE*)&wks_info) == NERR_Success)
			{
				wcstombs(temp,(wchar_t*)wks_info->wki100_computername,sizeof(temp));
				if(_stricmp(temp,name) == 0)
				{
					local = 1;
				}
				else
				{
					wcstombs(temp,(wchar_t*)wks_info->wki100_langroup,sizeof(temp));
					if (IsDc() && _stricmp(temp,name) == 0)
					{
						local = 1;
					}
				}
				(*api_buffer_free)(wks_info);
			}
		}
	}

	if (local)
	{
		if (sep != NULL)
		{
			strcpy(name,sep+1);
		}
	}
	else
	{
		*sep = '/';
	}

	if (hLibrary != NULL)
	{
		FreeLibrary(hLibrary);
	}

	return(local);
}

int mangle(char *name, char *system,int size)
{
	char *sep; 
	wchar_t *data;
	int ret=0;
	wchar_t wdom[80];
	HMODULE hLibrary = NULL;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(osinfo); 
	system[0] = '\0';
	
	if(!GetVersionEx(&osinfo)) 
		return(0);

	if(VER_PLATFORM_WIN32_NT==osinfo.dwPlatformId)
	{
		get_dcname = (get_dcname_def)getapi_addr("netapi32.dll","NetGetDCName",&hLibrary);
		api_buffer_free = (api_buffer_free_def)getapi_addr("Netapi32.dll","NetApiBufferFree",&hLibrary);
		if((!get_dcname || !api_buffer_free) && hLibrary)
		{
			FreeLibrary(hLibrary);
			return(1);
		}
	}
	else
		return(1);

	if (parse_username(name))
		ret = 1;
	else
	{
		if ((sep = strchr(name,'/')) != NULL)
			*sep = '\0';
		mbstowcs(wdom,name,sizeof(wdom));
		*sep = '/';
		if ((*get_dcname)(NULL,wdom,(LPBYTE*)&data)== NERR_Success)
		{
			wcstombs(system,data,size);
			(*api_buffer_free)(data);
			ret = 1;
		}
	}
	if (hLibrary != NULL)
		FreeLibrary(hLibrary);
	return(ret);
}

SECURITY_DESCRIPTOR* nulldacl(void)
{
	static int beenhere;
	static SECURITY_DESCRIPTOR sd[1];
	if(!beenhere)
	{
	  	if(InitializeSecurityDescriptor(sd,SECURITY_DESCRIPTOR_REVISION))
		{
			if(SetSecurityDescriptorDacl(sd, TRUE, (ACL*)0,FALSE))
			{
				beenhere = 1;
				return(sd);
			}
		}
		else
			return(0);
	}
	return(sd);

}

static HANDLE log_handle = INVALID_HANDLE_VALUE;

void logopen(const char* name, int old)
{
	int	fd;
	int	oflags = O_WRONLY|O_APPEND|O_CREAT;
	char	log[PATH_MAX];
	char	buf[PATH_MAX];

	if (log_handle && log_handle != INVALID_HANDLE_VALUE)
		return;
	sfsprintf(log, sizeof(log), "%s%s", LOGDIR, name);
	if (old)
	{
		oflags |= O_TRUNC;
		sfsprintf(buf, sizeof(buf), "%s.old", log);
		rename(log, buf);
	}
	if ((fd = open(log, oflags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) >= 0)
	{
		chmod(log, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		chown(log, -1, 3);
		if (fd < 3)
		{
			int newfd;

			if ((newfd = fcntl(fd, F_DUPFD, 3)) > 0)
			{
				close(fd);
				fd = newfd;
			}
		}
		fcntl(fd, F_SETFD, 1);
		log_handle = uwin_handle(fd, 0);
	}
}

void logclose(void)
{
	if (log_handle && log_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(log_handle);
		log_handle = INVALID_HANDLE_VALUE;
	}
}

/*
 * append printf-style message line to the log
 * format should not contain newlines
 */

int logsrc(int level, const char* file, int line, const char* format, ...)
{
	va_list		ap;
	char		buf[2*1024];
	char*		cur;
	char*		end;
	char*		msg;
	char*		s;
	SYSTEMTIME	sys;
	DWORD		err;
	DWORD		w;
	int		n;

	if ((level & LOG_LEVEL) > log_level)
		return 0;
	err = GetLastError();
	cur = buf;
	end = buf + sizeof(buf) - 2;

	/*
	 * fixed message prefix
	 */

	if (!(level & (LOG_EVENT|LOG_STDERR)))
	{
		GetLocalTime(&sys);
		cur += sfsprintf(cur, end-cur, "%.4d-%02d-%02d+%02d:%02d:%02d %5d %d ",
				sys.wYear,
				sys.wMonth,
				sys.wDay,
				sys.wHour,
				sys.wMinute,
				sys.wSecond,
				GetCurrentThreadId(),
				level & LOG_LEVEL);
	}
	msg = cur;
	if (!(level & LOG_USAGE))
	{
		if (file)
		{
			if (s = strrchr(file, '/'))
				file = (const char*)s;
			cur += sfsprintf(cur, end-cur, "%s:%d: ", file, line);
		}
		else if (line)
			cur += sfsprintf(cur, end-cur, "%4d ", line);
	}

	/*
	 * the caller part
	 */

	va_start(ap, format);
	if (format && *format)
		cur += sfvsprintf(cur, end-cur, format, ap);
	va_end(ap);
	if (*(cur-1) == '\n')
		cur--;
	if (*(cur-1) == '\r')
		cur--;

	/*
	 * finally append to the log
	 */

	if (cur > buf)
	{
		if ((level & (LOG_SYSTEM|LOG_USAGE)) == LOG_SYSTEM && err && (end - cur) > 32)
		{
			cur += sfsprintf(cur, end - cur, " [%lu:", err);
			if (!(n = (int)FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, err, 0, cur, (DWORD)(end - cur), 0)))
				n = sfsprintf(cur, end - cur, "Unknown error code %d.\n", err);
			if (n >= (end - cur))
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
		if (cur > end)
			cur = end;
		if (level & LOG_EVENT)
		{
			char*	sa[2];
			HANDLE	hp;

			if (hp = RegisterEventSource(NULL, log_service))
			{
				*cur = 0;
				sa[0] = (char*)log_service;
				sa[1] = buf;
				ReportEvent(hp, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, sa, NULL);
				DeregisterEventSource(hp);
			}
		}
		if (level & (LOG_ALL|LOG_STDERR))
		{
			*cur = 0;
			if (log_command)
			{
				if (level & LOG_USAGE)
					sfprintf(sfstderr, "Usage: %s %s\n", log_command, msg);
				else
					sfprintf(sfstderr, "%s: %s\n", log_command, msg);
			}
			else
				sfprintf(sfstderr, "%s\n", msg);
			sfsync(sfstderr);
		}
		if (log_handle != INVALID_HANDLE_VALUE && !(level & LOG_STDERR))
		{
			*cur++ = '\r';
			*cur++ = '\n';
			WriteFile(log_handle, buf, (DWORD)(cur - buf), &w, 0);
			FlushFileBuffers(log_handle);
		}
	}
	SetLastError(err);
	return (int)(cur - buf);
}

void print(int level, char* format, ...) 
{
	va_list	ap;
	char	buf[1024];

	if (level > 0)
	{
		va_start(ap, format);
		sfvsprintf(buf, sizeof(buf), format, ap);
		va_end(ap);
		logsrc(level - 1, 0, 0, "%s", buf);
	}
}
