#include "globals.h"
#include "stdafx.h"
#include <winsvc.h>
#include <windows.h>
#include <winbase.h>
#include <stdlib.h>
#include <lm.h>
#include <stdarg.h>
#include "UwinClient.h"
#include <uwin_keys.h>
#include "RegistryOps.h"

#define UWIN_MASTER   1
#define UWIN_TELNETD  2
#define SYSNAMELENGTH 80

#define STATE_SERVICE   1
#define TYPE_SERVICE    2
#define CONTROL_SERVICE 3

#define START_TYPE 1

char servname[256], servdname[256]; 
SERVICE_STATUS status;
SERVICE_STATUS_HANDLE statushandle;

void convert_state_to_string(char *str, DWORD code, int opt)
{
	switch(opt)
	{
		case STATE_SERVICE:
			switch(code)
			{
				case SERVICE_STOPPED:
						strcpy(str,"Stopped");
						break;
				case SERVICE_START_PENDING:
						strcpy(str,"Start Pending");
						break;
				case SERVICE_STOP_PENDING:
						strcpy(str,"Stop Pending");
						break;
				case SERVICE_RUNNING:
						strcpy(str,"Running");
						break;
				case SERVICE_CONTINUE_PENDING:
						strcpy(str,"Continue Pending");
						break;
				case SERVICE_PAUSE_PENDING:
						strcpy(str,"Pause Pending");
						break;
				case SERVICE_PAUSED:
						strcpy(str,"Paused");
						break;
			   default:
						break;
			}
			break;
			
		case TYPE_SERVICE:

			switch (code)
			{
			
				case SERVICE_WIN32_OWN_PROCESS:
						strcpy(str,"SERVICE_WIN32_OWN_PROCESS");
						break;
				case SERVICE_WIN32_SHARE_PROCESS:
						strcpy(str,"SERVICE_WIN32_SHARE_PROCESS");
						break;
				case SERVICE_KERNEL_DRIVER:
						strcpy(str, "SERVICE_KERNEL_DRIVER");
						break;
				case SERVICE_FILE_SYSTEM_DRIVER:
						strcpy(str, "SERVICE_FILE_SYSTEM_DRIVER");
						break;
				case SERVICE_INTERACTIVE_PROCESS:
						strcpy(str,"SERVICE_INTERACTIVE_PROCESS");
						break;
				default:
						break;

			}
			break;
		case CONTROL_SERVICE:
				if (code & SERVICE_ACCEPT_STOP)
					strcpy(str,"Stop");
				if (code & SERVICE_ACCEPT_SHUTDOWN)
					strcat(str,", Shutdown");
				if (code & SERVICE_ACCEPT_PAUSE_CONTINUE)
					strcat(str,", Pause & Continue");

	}
}

int getregpath(char *path) 
{
	char*		root;
	int		len;
	CRegistryOps	regOps;

	if(!(root = regOps.GetRoot()))
		return 0;
	strcpy(path, root);
	len = strlen(path);
	// Append a '\' character at the end if not present
	if (path[len-1] != '\\')
	{
		path[len] = '\\';
		path[len+1] = 0;
	}
	return 1;
}

// install UMS if serviceno=1 , and TELNETD if 2.
int installservice(int  serviceno, char *instPath)
{
	SC_HANDLE service, scm;
	char path[1024];
	int fail = 0, len;

	if(instPath == NULL)
	{
		if(!getregpath(path))
			return 0;
	}
	else
		strcpy(path, instPath);

	len = strlen(path);
	if(path[len-1] != '\\')
	{
		path[len] = '\\';
		path[len+1] = 0;
	}

	switch (serviceno)
	{
		case UWIN_MASTER ://UMS
				strcpy(servname,"uwin_ms");
				strcat(path,"usr\\etc\\ums.exe");
				strcpy(servdname,"UWIN Master");
				break;
		case UWIN_TELNETD://telnetd
				strcpy(servname,"uwin_telnetd");
				strcat(path,"usr\\etc\\telnetd.exe");
				strcpy(servdname,"UWIN Telnetd");
				break; 
		default:
				return 0;
	}
	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scm)
	{
		if(service=OpenService(scm,TEXT(servname),SERVICE_QUERY_STATUS))
		{
			if(GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
			{	
				CloseServiceHandle(scm);
				CloseServiceHandle(service);
				MessageBox(NULL,"    Service Exists   ",servname,MB_OK);
				return(0);
			}
		}
		service = CreateService(scm, 
								TEXT(servname), 
								TEXT(servdname), 
								SERVICE_ALL_ACCESS, 
								SERVICE_WIN32_SHARE_PROCESS, 
								SERVICE_AUTO_START, 
								SERVICE_ERROR_IGNORE, 
								path, 
								NULL, 
								NULL, 
								NULL, 
								NULL, 
								NULL);

		if (service)
		{
			CloseServiceHandle(service);
			MessageBox(NULL,"Service Successfully Installed",servname,MB_OK);
			fail =1;
		}
		else
		{
			DWORD err;
			err = GetLastError();
			MessageBox(NULL,"Create Service Failed",servname,MB_OK);
			fail = 0;
		}
		CloseServiceHandle(scm);
	} 
	else
		fail = 0;
	
	return(fail);
}

int deleteservice(char *servname)
{
	BOOL ret;
	SC_HANDLE service, scm;
	int fail=1;
	char buffer[80],temp[80];
	SERVICE_STATUS *stat;
	strcpy(buffer,servname);
	stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS)+1);

	if(!query_servicestatus(buffer,stat))
	{
		free(stat);
		return 0;
	}
	convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
	if(!stricmp(temp,"Running"))
	{
		if(!stopservice(buffer))
		{
			free(stat);
			return 0;
		}
			Sleep(500);
	}
	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scm)
	{
		service = OpenService(scm,TEXT(servname), SERVICE_ALL_ACCESS);
		if (service)
		{
			ret = DeleteService(service);
			if (ret)
			{
				MessageBox(NULL,"Service Deleted Successfully",servname,MB_OK);
				fail = 1;
			}
			else
			{
				ErrorBox("Service Delete Failed", servname, -1);
				fail = 0;
			}
			CloseServiceHandle(service);
		}
		else
		{
			ErrorBox("Unable to Open Service",servname,-1);
			fail = 0;
		}
		CloseServiceHandle(scm);
	} 
	else
	{
		MessageBox(NULL,"OpenSCManager failed",servname,MB_OK);
		fail = 0;
	}
	free(stat);
	return(fail);
}

extern void ErrorBox(const char* mesg, const char* item, int err)
{
	CRegistryOps	regOps;
	char*		b;
	char*		e;

	int		flags = MB_OK;

	char		buf[1024];

	if (!regOps.LogLevel)
		return;
	b = buf;
	e = b + sizeof(buf) - 1;
	if (item)
		b += _snprintf(b, e - b, "%s", item);
	if (!mesg)
		mesg = "Operation failed";
	if (b > buf)
		*b++ = '\n';
	b += _snprintf(b, e - b, "%s.", mesg);
	if (err < 0)
		err = GetLastError();
	if (err)
	{
		flags |= MB_ICONERROR;
		if (b > buf)
			*b++ = '\n';
		b += FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 
			NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), b, e - b, NULL);
	}
	else
		flags |= MB_ICONINFORMATION;
	::MessageBox(NULL, buf, "UWIN Config", flags);
}

int startservice(char *servname)
{
	DWORD ret;
	SERVICE_STATUS serstat;
	SC_HANDLE scm, service;
	int siz = SYSNAMELENGTH;

	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		ErrorBox("Cannot open service manager", servname, -1);
		return(0);
	}
	if((service = OpenService(scm,servname,SERVICE_USER_DEFINED_CONTROL|SERVICE_START|SERVICE_QUERY_STATUS))==NULL)
	{
		CloseServiceHandle(scm);
		ErrorBox("Cannot open service", servname, -1);
		return(0);
	}
	
	ret = StartService(service,0,NULL);
	if(!ret)
	{
		ErrorBox("Cannot start service", servname, -1);
		CloseServiceHandle(scm);
		CloseServiceHandle(service);
	    	return(0);
	}

	do
	{
		ret = QueryServiceStatus(service, &serstat);
		if(ret)
		{
			if(serstat.dwCurrentState== SERVICE_START_PENDING)
			{
				Sleep(500);
				continue;
			}
			else if(serstat.dwCurrentState== SERVICE_RUNNING)
			{
				CloseServiceHandle(service);
				CloseServiceHandle(scm);
				MessageBox(NULL,"Service started successfully ",servname,MB_OK);
				break;
			}
		}
		else
		{
			ErrorBox("Cannot query service", servname, -1);
			CloseServiceHandle(service);
			CloseServiceHandle(scm);
			return 0;
		}

	}while(1);
	return(1);
}

int stopservice(char *servname)
{
	DWORD ret;
	SERVICE_STATUS stat,serstat;
	SC_HANDLE scm, service;
	
	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		ErrorBox("Cannot open service manager", servname, -1);
		return(0);
	}

	if((service = OpenService(scm,servname,SERVICE_STOP|SERVICE_QUERY_STATUS))==NULL)
	{
		ErrorBox("Cannot open service", servname, -1);
		CloseServiceHandle(scm);
		return(0);
	}
	ret = ControlService(service,SERVICE_CONTROL_STOP,&stat);
	if(!ret)
	{
		ErrorBox("Cannot control service", servname, -1);
		CloseServiceHandle(scm);
		CloseServiceHandle(service);
		return(0);
	}
	Sleep(100);
	ret = QueryServiceStatus(service, &serstat);
	if(ret)
	{
		if(serstat.dwCurrentState== SERVICE_STOPPED)
			MessageBox(NULL,"Service stopped successfully ",servname,MB_OK);
	}
	else
	{
		ErrorBox("Cannot query service", servname, -1);
		CloseServiceHandle(scm);
		CloseServiceHandle(service);
		return 0;
	}

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
	return 1;
}

int getservice_displayname(char *servname,char *dispname)
{
	DWORD ret,size;
	SC_HANDLE scm;
	char *buffer;
	
	
	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		MessageBox(NULL,"Error: OpenSCManager failed",servname,MB_OK);
		return(0);
	}
	ret = GetServiceDisplayName(scm, servname, 0, &size);
	
	if(GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
	{
		CloseServiceHandle(scm);
		return (0);
	}
	
	buffer = (char *) malloc(size+1);
		
	if(GetServiceDisplayName(scm, servname, buffer, &size))
	{
		strcpy(dispname,buffer);
		CloseServiceHandle(scm);
		return(1);
	}
	
	CloseServiceHandle(scm);
	free(buffer);
	return 0;
	
}

int config_status_to_string(char *str, DWORD code, int opt)
{
	switch (opt)
	{
	 	case START_TYPE:
			switch(code)
			{
				case SERVICE_BOOT_START:
					strcpy(str,"Boot Start");
					break;
				case SERVICE_SYSTEM_START:
					strcpy(str,"System Start");
					break;
				case SERVICE_AUTO_START:
					strcpy(str,"Auto Start");
					break;
				case SERVICE_DEMAND_START:
					strcpy(str,"Demand Start");
					break;
				case SERVICE_DISABLED:
					strcpy(str,"Disabled");
					break;
			}
			break;

	}
	return 1;
}

int query_servicestatus(char *servname,SERVICE_STATUS *stat)
{
	SC_HANDLE scm, service;
	
	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		MessageBox(NULL,"Error: OpenSCManager failed",servname,MB_OK);
		return(0);
	}

	if(service=OpenService(scm,TEXT(servname),SERVICE_QUERY_STATUS))
	{
		if(GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
		{
			CloseServiceHandle(scm);
			CloseServiceHandle(service);
			return(0);
		}
	}
	else
	{
		CloseServiceHandle(scm);
		return(0);
	}

	if(!QueryServiceStatus(service, stat))
	{
		CloseServiceHandle(service);
		CloseServiceHandle(scm);
		return (0);
	}
	CloseServiceHandle(scm);
	CloseServiceHandle(service);
	return (1);
}

int uwin_service_status(char *servname, SERVICE_STATUS *stat)
{
	BOOL ret;
	SC_HANDLE scm;
	DWORD i;

	ENUM_SERVICE_STATUS *statusbuff=0;
	DWORD bytesrequired, totalentries, Rshandle=0;

	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (scm)
	{
		ret = EnumServicesStatus(scm, SERVICE_WIN32, SERVICE_STATE_ALL, statusbuff, 0, &bytesrequired, &totalentries, &Rshandle);

		if(GetLastError() == ERROR_MORE_DATA)
			statusbuff = (ENUM_SERVICE_STATUS *)malloc(bytesrequired+1);
		else
		{
			CloseServiceHandle(scm);
			return(0);
		}

			Rshandle =0;
		ret = EnumServicesStatus(scm, SERVICE_WIN32, SERVICE_STATE_ALL,statusbuff, bytesrequired, &bytesrequired, &totalentries, &Rshandle);
		if(!ret)
		{
			ErrorBox("Cannot enumerate services", servname, -1);
			CloseServiceHandle(scm);
			return 0;
		}

		for(i=0;i<totalentries;i++)
		{
			if(!_stricmp(servname,statusbuff[i].lpServiceName))
			{
				stat->dwCurrentState = statusbuff[i].ServiceStatus.dwCurrentState;
				stat->dwControlsAccepted = statusbuff[i].ServiceStatus.dwControlsAccepted;
			}
		}
		if(statusbuff)
		free(statusbuff);
	}
	else
		return 0;

	CloseServiceHandle(scm);
	return 1;
}

int query_service_config(char *servname,QUERY_SERVICE_CONFIG *lpqscBuf)
{
	//BOOL ret;
	SC_HANDLE service, scm;
	DWORD bytesrequired;

	scm = OpenSCManager(NULL, NULL, SERVICE_QUERY_CONFIG);
	if (scm)
	{
		if(service = OpenService(scm,TEXT(servname), SERVICE_QUERY_CONFIG))
		{
			if(!QueryServiceConfig(service, lpqscBuf,4096,&bytesrequired))
			{
				CloseServiceHandle(scm);
				CloseServiceHandle(service);
				return 0;
			}
			CloseServiceHandle(service);
		}
		else
		{
			CloseServiceHandle(scm);
			return 0;
		}
	}
	else
		return 0;

	CloseServiceHandle(scm);
	return 1;
}

int set_service_config(char *servname, QUERY_SERVICE_CONFIG *config)
{
	SC_HANDLE service, scm;
	
	scm = OpenSCManager(NULL, NULL, SERVICE_QUERY_CONFIG);
	if (scm)
	{
		service = OpenService(scm,TEXT(servname), SERVICE_CHANGE_CONFIG);
		if(service)
		{
			if(!ChangeServiceConfig(service,
							 config->dwServiceType,
							 config->dwStartType,
							 config->dwErrorControl,
							 config->lpBinaryPathName,
							 config->lpLoadOrderGroup,
							 NULL,
							 config->lpDependencies,
							 config->lpServiceStartName,
							 NULL,
							 config->lpDisplayName))
			{
				ErrorBox("Cannot reconfigure service", servname, -1);
				CloseServiceHandle(scm);
				CloseServiceHandle(service);
				return 0;
			}
			CloseServiceHandle(service);
		}
		CloseServiceHandle(scm);
	}
	else
		return 0;
	return 1;
}
