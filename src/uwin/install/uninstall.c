/*
 * uninstall release
 *
 * uninstall UWIN release and switch to newest remaining release
 * if no other UWIN release then completely remove UWIN
 */

#include <windows.h>
#include <stdio.h>
#include <regstr.h>

#include "uwin_keys.c"

#define PATH_MAX	256

static const char id[] = "uwin_uninstall";

/*
 * remove dir and its subdirs
 */

static void
sear_rm_r(char* dir)
{
	WIN32_FIND_DATA	info;
	HANDLE		hp;

	if (!SetCurrentDirectory(dir))
		return;
	if ((hp = FindFirstFile("*.*", &info)) != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (info.dwFileAttributes&FILE_ATTRIBUTE_READONLY)
					SetFileAttributes(info.cFileName, info.dwFileAttributes&~FILE_ATTRIBUTE_READONLY);
				DeleteFile(info.cFileName);
			}
			else if (info.cFileName[0] != '.' || info.cFileName[1] != 0 && (info.cFileName[1] != '.' || info.cFileName[2] != 0))
				sear_rm_r(info.cFileName);
		} while(FindNextFile(hp, &info));
		FindClose(hp);
	}
	if (SetCurrentDirectory(".."))
		RemoveDirectory(dir);
}

static void
done(int code)
{
	Sleep(4000);
	exit(code);
}

static const char* sys[] = { UWIN_SYS_FILES, 0 };

int
main(int argc, char** argv)
{
	STARTUPINFO		sinfo;
	HKEY			key = 0;
	char			rootdir[PATH_MAX];
	char			urootdir[PATH_MAX];
	char			altrelease[PATH_MAX];
	char			altrootdir[PATH_MAX];
	char			cmd[PATH_MAX];
	char			buf[PATH_MAX];
	char*			release;
	char*			cp;
	int			alternate = 0;
	int			size = sizeof(rootdir);
	int			i;

	GetStartupInfo(&sinfo);
	if (sinfo.cbReserved2 > sizeof(int))
	{
		int	maxfd = *((int*)sinfo.lpReserved2);
		int	len;

		len = sizeof(int) + maxfd * (sizeof(unsigned char) + sizeof(HANDLE));
		if (sinfo.cbReserved2 >= len + sizeof(int))
		{
			if (cp = strrchr(argv[0],'\\'))
				cp++;
			else
				cp = argv[0];
			fprintf(stderr, "%s: %s cannot be run from UWIN\n", id, cp);
			done(1);
		}
	}
	if (argc != 2 || !(release = *++argv))
	{
		fprintf(stderr, "Usage: %s release\n", id);
		done(1);
	}
	if (uwin_getkeys(release, rootdir, sizeof(rootdir), 0, 0))
	{
		fprintf(stderr, "%s: %s: cannot determine UWIN %s root\n", id, UWIN_KEY, release);
		done(1);
	}
	if (!SetCurrentDirectory(rootdir))
	{
		fprintf(stderr, "%s: %s: cannot set current directory\n", id, rootdir);
		done(1);
	}
	for (cp = rootdir; *cp; cp++)
		if (*cp=='\\')
			*cp = '/';
	strcpy(urootdir, rootdir);
	if (urootdir[1]==':')
	{
		urootdir[1] = urootdir[0];
		urootdir[0] = '/';
	}
	if (!uwin_alternate(release, altrelease, sizeof(altrelease)) && !uwin_getkeys(altrelease, altrootdir, sizeof(altrootdir), 0, 0))
	{
		alternate = 1;
		for (cp = altrootdir; *cp; cp++)
			if (*cp == '\\')
				*cp = '/';
		if (altrootdir[1]==':')
		{
			altrootdir[1] = altrootdir[0];
			altrootdir[0] = '/';
		}
		sprintf(cmd, "usr\\bin\\ksh /var/uninstall/uninstall.sh %s \"%s\" %s \"%s\"", release, urootdir, altrelease, altrootdir);
	}
	else
		sprintf(cmd, "usr\\bin\\ksh /var/uninstall/uninstall.sh %s \"%s\"", release, urootdir);

	/*
	 * a successful uninstall ends with SIGKILL to all uwin processes
	 * the windows system() reports that as exit status 9
	 */

	if (system(cmd) != 9)
	{
		fprintf(stderr, "%s: %s: uninstall script failed\n", id, cmd);
		done(1);
	}
	GetSystemDirectory(buf, sizeof(buf));
	cp = buf + strlen(buf);
	*cp++ = '\\';
	for (i = 0; sys[i]; i++)
	{
		strcpy(cp, sys[i]);
		DeleteFile(buf);
		if (alternate)
		{
			sprintf(cmd, "%s\\var\\sys\\%s", altrootdir, sys[i]);
			CopyFile(cmd, buf, 0);
		}
	}
	SetCurrentDirectory("\\");
	sear_rm_r(rootdir);
	if (SetCurrentDirectory(rootdir))
	{
		SetCurrentDirectory("\\");
		sear_rm_r(rootdir);
	}

	if (alternate)
		fprintf(stderr, "UWIN %s uninstall completed successfully -- %s to UWIN %s.\n", release, strcmp(altrelease, release) < 0 ? "switching" : "falling back", altrelease);
	else
		fprintf(stderr, "UWIN completely removed from the system.\n");
	return 0;
}
