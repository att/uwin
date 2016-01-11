#include <ast.h>
#include <error.h>
#include <uwin.h>

static const char usage[] =
"[-?\n@(#)$Id: dumpstack (AT&T Labs Research) 2011-07-22 $\n]"
USAGE_LICENSE
"[+NAME?dumpstack - dump the call stack of a running process]"
"[+DESCRIPTION?\bdumpstack\b writes the call stack, one call per line, "
    "of the process \apid\a to standard output.]"
    "pid "
"[s:separator?Set the call listing separator string to "
    "\asep\a.]:[sep:=\\n\\t]"
"[t:thread?Interpret the \apid\a operand as a thread id.]"
"[+EXIT STATUS?]"
    "{"
        "[+0?Success.]"
        "[+>0?An error occurred.]"
    "}"
"[+SEE ALSO?\btrace\b(1)]"
;


int
main(int argc, char** argv)
{
	char*		s;
	char*		t;
	pid_t		pid;
	ssize_t		n;
	ssize_t		(*dump)(pid_t, void*, const char*, char*, size_t) = uwin_stack_process;
	char*		sep = "\n\t";
	char		buf[16*1024];

	NoP(argc);
	error_info.id = "dumpstack";
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 's':
			sep = opt_info.arg;
			continue;
		case 't':
			dump = uwin_stack_thread;
			continue;
		case '?':
			error(ERROR_USAGE|4, "%s", opt_info.arg);
			break;
		case ':':
			error(2, "%s", opt_info.arg);
			break;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || !(s = *argv++) || *argv)
		error(ERROR_USAGE|4, "%s", optusage(NiL));
	if (!(pid = strtol(s, &t, 0)) || *t)
		error(3, "%s: pid argument expected", s);
	if ((n = (*dump)(pid, 0, sep, buf, sizeof(buf))) < 0)
		error(ERROR_SYSTEM|3, "%d: call stack dump failed", pid);
	if (write(1, buf, n) != n)
		error(ERROR_SYSTEM|3, "write to standard output failed");
	return error_info.errors != 0;
}
