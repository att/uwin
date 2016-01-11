#include	<windows.h>
#include	<stdlib.h>
#include	<stdio.h>

#ifndef CPL
#define CPL	"test.cpl"
#endif

int
main(int argc, char** argv)
{
	PROCESS_INFORMATION	pinfo;
	STARTUPINFO		sinfo;
	SECURITY_ATTRIBUTES	sattr;
	int			n;
	char			cmd[MAX_PATH];

	ZeroMemory(&sattr, sizeof(sattr));
	sattr.bInheritHandle = TRUE;
	ZeroMemory(&sinfo, sizeof(sinfo));
	n = GetSystemDirectory(cmd, sizeof(cmd));
	strcpy(cmd + n, "\\cmd.exe");
	if (CreateProcess(cmd, "cmd /c control " CPL, &sattr, &sattr, TRUE, NORMAL_PRIORITY_CLASS|CREATE_NO_WINDOW, NULL, NULL, &sinfo, &pinfo))
	{
		CloseHandle(pinfo.hThread);
		WaitForSingleObject(pinfo.hProcess, INFINITE);
		if (!GetExitCodeProcess(pinfo.hProcess, &n))
			n = 1;
		CloseHandle(pinfo.hProcess);
	}
	else
		n = 126;
	return 0;
}
