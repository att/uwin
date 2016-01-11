
#include	<windows.h>
#include	<stdio.h>

int main(int argc, char *argv[])
{
	DWORD	ntpid=strtol(argv[1],0,10);
	HANDLE	ph;
	if (!(ph = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ntpid)))
	{
		fprintf(stderr,"forkhelper: openprocess failed err=%d\n",GetLastError());
		exit(0);
	}
	WaitForSingleObject(ph,INFINITE);
	exit(0);
}
