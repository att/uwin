/*
 * mkpasswd.c
 * This programs send a Service Control to the UMS
 * to re-create the Passwd file and Group File
 */

#include <stdio.h>
#include <windows.h>
#include <uwin_serve.h>

BOOL Win32Error(DWORD err, LPSTR str1);

int main(int argc, char *argv[])
{
	int ret;
	SERVICE_STATUS stat;
	SC_HANDLE scm, service;
	char *servname = UMS_NAME;

	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		Win32Error(GetLastError(),"OpenSCManager");
		return(FALSE);
	}

	if((service = OpenService(scm,servname,SERVICE_USER_DEFINED_CONTROL|SERVICE_START))==NULL)
	{
		if(GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
			fprintf(stderr, "UWIN Master Service has not been installed..\n");
		CloseServiceHandle(scm);
		return(FALSE);
	}

	if((ret = ControlService(service, UMS_PW_SYNC_MSG, &stat)) == FALSE)
	{
		if(GetLastError() == ERROR_SERVICE_NOT_ACTIVE) //Service is not running
		{
			fprintf(stderr, "UWIN Master Service is not running..\n");
			fprintf(stderr, "Attempting to start the service...\n");
			if(StartService(service, 0, NULL)== FALSE)
				Win32Error(GetLastError(), "StartService");
			else
				fprintf(stdout,"UWIN Service: Creating new /etc/passwd and /etc/group files..\n");
		}
	}
	else
		fprintf(stdout,"UWIN Service: Creating new /etc/passwd and /etc/group files..\n");

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

BOOL Win32Error(DWORD err, LPSTR str1)
{
	LPVOID temp;
	LPVOID local;
	if(FormatMessage( \
			FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			(LPTSTR) &temp,
			0,
			NULL))
	{
		local = (char *)malloc(strlen(str1)+strlen(temp)+1);
		if(!local)
			return(-1);
		sprintf(local,"%s: %s\n",str1, temp);
		fprintf(stderr, local);
		LocalFree(temp);
		free(local);
		return(1);
	}
	else
		return(-1);
}
