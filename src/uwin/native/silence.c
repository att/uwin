/*
 * silence!
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static const char	id[] = "silence";

#define H(x)		do{if(html)fprintf(stderr,"%s",x);}while(0)
#define T(x)		fprintf(stderr,"%s",x)

static void
help(int html)
{
H("<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n");
H("<HTML>\n");
H("<HEAD>\n");
H("<TITLE>silence man document</TITLE>\n");
H("</HEAD>\n");
H("<BODY bgcolor=white>\n");
H("<PRE>\n");
T("NAME\n");
T("  silence - terminate process given windows pid\n");
T("\n");
T("SYNOPSIS\n");
T("  silence [ pid ... ]\n");
T("\n");
T("DESCRIPTION\n");
T("  silence calls TerminateProcess(win32) for each pid operand.\n");
T("\n");
T("SEE ALSO\n");
T("  TerminateProcess(win32), kill(1)\n");
T("\n");
T("IMPLEMENTATION\n");
T("  version     silence (AT&T Research) 2010-12-06\n");
T("  author      Glenn Fowler <gsf@research.att.com>\n");
T("  copyright   Copyright (c) 2005-2010 AT&T Intellectual Property\n");
T("  license     http://www.opensource.org/licenses/cpl1.0.txt\n");
H("</PRE>\n");
H("</BODY>\n");
H("</HTML>\n");
}

int
main(int argc, char** argv)
{
	HANDLE	p;
	char*	s;
	char*	e;
	DWORD	n;
	int	r;

	r = 0;
	while (s = *++argv)
	{
		if (s[0] == '-')
		{
			help(s[1] == '-' && s[2] == 'h' && s[3] == 't');
			return 2;
		}
		n = strtol(s, &e, 0);
		if (*e)
		{
			fprintf(stderr, "%s: warning: %s: invalid native pid\n", id, s);
			r = 1;
		}
		else if (!(p = OpenProcess(PROCESS_DUP_HANDLE|PROCESS_TERMINATE, FALSE, n)))
		{
			fprintf(stderr, "%s: %u: cannot get process handle [error=%d]\n", id, n, GetLastError());
			r = 1;
		}
		else
		{
			if (!TerminateProcess(p, 256+9))
			{
				fprintf(stderr, "%s: %u: cannot terminate process [error=%d]\n", id, n, GetLastError());
				r = 1;
			}
			CloseHandle(p);
		}
	}
	return r;
}
