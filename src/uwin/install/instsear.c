#include	<windows.h>
#include	<stdlib.h>
#include	<stdio.h>

#include	"uwin_keys.c"

static void error_exit(const char* string, const char* arg, int usage)
{
	if (usage)
		fprintf(stderr, "Usage: ");
	else
		fprintf(stderr, "uwin-sear: ");
	if (arg)
		fprintf(stderr, "%s: ", arg);
	fprintf(stderr, "%s\n", string);
	Sleep(3000);
	exit(1);
}

static void usage(void)
{
	error_exit("install.sh [ debug ] [ defaults ] [ remote ] [ yes ] -- package release version", 0, 1);
}

int main(int argc, char** argv)
{
	PROCESS_INFORMATION	pinfo;
	STARTUPINFO		sinfo;
	SECURITY_ATTRIBUTES	sattr;
	char			command[MAX_PATH];
	char			path[MAX_PATH];
	char			dir[MAX_PATH];
	char			buf[MAX_PATH];
	char			rel[MAX_PATH];
	char*			cp;
	char*			ap;
	char*			option[8];
	void*			nenv = 0;
	int			argn;
	int			c;
	int			n;
	int			pkg;
	int			debug = 0;
	int			defaults = 0;
	int			options = 0;
	int			remote = 0;
	int			show = 0;
	int			yes = 0;
	int			flags = NORMAL_PRIORITY_CLASS;

	if (argc < 2)
		usage();
	for (argn = 2; ap = argv[argn]; argn++)
	{
		if (*ap == '-' && *(ap+1) == '-' && !*(ap+2))
		{
			argn++;
			break;
		}
		if (!strcmp(ap, "debug"))
			debug = 1;
		else if (!strcmp(ap, "defaults"))
			defaults = 1;
		else if (!strcmp(ap, "remote"))
			remote = 1;
		else if (!strcmp(ap, "show"))
			show = 1;
		else if (!strcmp(ap, "yes"))
			yes = 1;
		else
			error_exit("unknown option", ap, 0);
	}
	if ((argc - argn) != 3)
		usage();
	cp = argv[argn];
	if (ap = strchr(cp, '-'))
		ap++;
	else
		ap = cp;
	if (uwin_release(0, 0, rel, sizeof(rel)))
		error_exit("cannot determine current UWIN release", UWIN_KEY, 0);
	if (memcmp(argv[argn+1], rel, strlen(argv[argn+1])) > 0)
	{
		if (!uwin_getkeys(argv[argn+1], 0, 0, 0, 0))
			sprintf(buf, "UWIN %s %s %s incompatible with current %s -- restart UWIN %s* or get UWIN %s %s", argv[argn+1], ap, argv[argn+2], rel, argv[argn+1], rel, ap);
		else
			sprintf(buf, "UWIN %s %s %s incompatible with current %s -- install UWIN %s or get UWIN %s %s", argv[argn+1], ap, argv[argn+2], rel, argv[argn+1], rel, ap);
		error_exit(buf, 0, 0);
	}
	argv[argn+1] = rel;
	if (!defaults && !remote && !yes)
	{
		fprintf(stderr, "Install UWIN %s %s %s %d bit ? [yes] ", argv[argn+1], ap, argv[argn+2], 8 * sizeof(char*));
		fflush(stderr);
		n = 1;
		while ((c = getchar()) != EOF && c != '\n')
			if (n && c != 'y' && c != 'Y' && c != '1')
				n = 0;
		if (!n || c == EOF)
			return 0;
	}
	if (debug)
		option[options++] = "--debug";
	if (defaults)
		option[options++] = "--defaults";
	if (remote)
	{
		option[options++] = "--remote";
		flags |= CREATE_NO_WINDOW;
	}
	if (uwin_getkeys(0, dir, sizeof(dir), 0, 0))
		error_exit("cannot find UWIN registry keys", UWIN_KEY, 0);
#if _X64_
	sprintf(path, "%s\\usr\\bin\\ksh.exe", dir);
#else
	sprintf(path, "%s\\u32\\bin\\ksh.exe", dir);
	if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
	{
		n = (int)strlen(dir) + 2;
		path[n] = 's';
		path[n+1] = 'r';
	}
#endif
	cp = command;
	cp += sprintf(cp, "ksh \"%s\"", argv[1]);
	for (n = 0; n < options; n++)
		cp += sprintf(cp, " %s", option[n]);
	for (pkg = argn; argn < argc && (ap = argv[argn]); argn++)
		cp += sprintf(cp, " \"%s\"", ap);
	if (show)
	{
		fprintf(stderr, "\"%s\" %s\n", path, command);
		return 0;
	}
	ZeroMemory(&sattr, sizeof(sattr));
	sattr.bInheritHandle = TRUE;
	ZeroMemory(&sinfo, sizeof(sinfo));
	if (CreateProcess(path, command, &sattr, &sattr, TRUE, flags, nenv, NULL, &sinfo, &pinfo))
	{
		CloseHandle(pinfo.hThread);
		WaitForSingleObject(pinfo.hProcess, INFINITE);
		if (!GetExitCodeProcess(pinfo.hProcess, &n))
			error_exit("cannot get exit status", argv[1], 0);
		CloseHandle(pinfo.hProcess);
		if (n)
		{
			sprintf(buf, "%s %s -- exit status %d", path, command, n);
			error_exit(buf, argv[1], 0);
		}
	}
	else 
	{
		sprintf(buf, "cannot create process [error=%d]", GetLastError());
		error_exit(buf, argv[1], 0);
	}
	if (uwin_setkeys(argv[pkg], argv[pkg+1], argv[pkg+2], 0, 0))
		error_exit("cannot set UWIN package registry keys", UWIN_KEY, 0);
	return 0;
}
