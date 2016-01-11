/*
 * run string operand via CreateProcess(win32)
 */

#include <windows.h>
#include <stdio.h>

static const char	id[] = "runcmd";

#define H(x)		do{if(html)fprintf(stderr,"%s",x);}while(0)
#define T(x)		fprintf(stderr,"%s",x)

static void
help(int html)
{
H("<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n");
H("<HTML>\n");
H("<HEAD>\n");
H("<TITLE>runcmd man document</TITLE>\n");
H("</HEAD>\n");
H("<BODY bgcolor=white>\n");
H("<PRE>\n");
T("NAME\n");
T("  runcmd - run command string via CreateProcess(win32)\n");
T("\n");
T("SYNOPSIS\n");
T("  runcmd [ options ] \"command\"\n");
T("\n");
T("DESCRIPTION\n");
T("  runcmd calls CreateProcess(win32) with the command operand as the\n");
T("  second argument, waits for the command to complete, and exits\n");
T("  with the command exit status.\n");
T("\n");
T("  To run command in an alternate uwin the calling process pwd must\n");
T("  be in the alternate uwin and PATH must only reference alternate\n");
T("  uwin directories.\n");
T("\n");
T("OPTIONS\n");
T("  -b, --background\n");
T("       Run command in the background and do not wait for completion.\n");
T("  -l, --log=file\n");
T("       Log the command standard output and error to file.\n");
T("  -w, --window\n");
T("       Run the command in a new window.\n");
T("\n");
T("SEE ALSO\n");
T("  CreateProcess(win32)\n");
T("\n");
T("IMPLEMENTATION\n");
T("  version     runcmd (AT&T Research) 2010-12-08\n");
T("  author      Glenn Fowler <gsf@research.att.com>\n");
T("  author      David Korn <dgk@research.att.com>\n");
T("  copyright   Copyright (c) 2005-2010 AT&T Intellectual Property\n");
T("  license     http://www.opensource.org/licenses/cpl1.0.txt\n");
H("</PRE>\n");
H("</BODY>\n");
H("</HTML>\n");
	exit(2);
}

static void
usage(int flag, char* msg)
{
	if (flag)
		fprintf(stderr, "%s: -%c: %s\n", id, flag, msg);
	fprintf(stderr, "Usage: %s [ --background ] [ --log=file ] \"command\"\n", id);
	exit(2);
}

int
main(int argc, char** argv)
{
	PROCESS_INFORMATION	pinfo;
	SECURITY_ATTRIBUTES	sattr;
	SECURITY_DESCRIPTOR	sdesc;
	STARTUPINFO		sinfo;
	char*			s;
	char*			v;
	int			n;

	int			background = 0;
	int			flags = NORMAL_PRIORITY_CLASS;
	char*			log = 0;

	while ((s = *++argv) && *s == '-')
	{
		v = 0;
		if ((n = *(s + 1)) == '-')
		{
			s += 2;
			if (!(n = *s++))
			{
				s = *++argv;
				break;
			}
			while (*s)
				if (*s++ == '=')
				{
					v = s;
					s = "";
					break;
				}
		}
		else if (!n)
			break;
		else
			s += 2;
		do
		{
			switch (n)
			{
			case '-':
				break;
			case 'b':
				background = 1;
				break;
			case 'h':
				help(1);
				break;
			case 'l':
				if (!v)
				{
					if (!*(v = s) && !(v = *++argv))
						usage(n, "argument expected");
					s = "";
				}
				log = v;
				break;
			case 'm':
				help(0);
				break;
			case 'w':
				flags |= CREATE_NEW_CONSOLE;
				break;
			default:
				usage(n, "unknown option");
				break;
			}
		} while (n = *s++);
	}
	if (!s)
		usage(0, 0);
	ZeroMemory(&sinfo, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);
	if (log)
	{
		sinfo.dwFlags = STARTF_USESTDHANDLES;
		InitializeSecurityDescriptor(&sdesc, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(&sdesc, TRUE, 0, FALSE);
		sattr.bInheritHandle = TRUE;
		sattr.lpSecurityDescriptor = &sdesc;
		sattr.nLength = sizeof(sattr);
		if (!(sinfo.hStdOutput = CreateFile(log, GENERIC_WRITE, FILE_SHARE_WRITE, &sattr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) || sinfo.hStdOutput == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "%s: %s: cannot write log file\n", id, log);
			exit(1);
		}
		sinfo.hStdError = sinfo.hStdOutput;
		n = CreateProcess(0, s, &sattr, &sattr, TRUE, flags, 0, 0, &sinfo, &pinfo);
	}
	else
		n = CreateProcess(0, s, 0, 0, TRUE, flags, 0, 0, &sinfo, &pinfo);
	if (!n)
	{
		fprintf(stderr, "%s: %s: cannot create process [error=%d]\n", id, s, GetLastError());
		exit(1);
	}
	if (background)
		n = 0;
	else
	{
		WaitForSingleObject(pinfo.hProcess, INFINITE);
		GetExitCodeProcess(pinfo.hProcess, &n);
		CloseHandle(pinfo.hProcess);
		CloseHandle(pinfo.hThread);
	}
	exit(n);
}
