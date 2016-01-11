/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
#pragma prototyped

/*
 * the u/win trace command
 */

static const char usage[] =
"[-?\n@(#)$Id: trace (AT&T Labs Research) 2012-04-29 $\n]"
USAGE_LICENSE
"[+NAME?trace - output a system call trace]"
"[+DESCRIPTION?By default \btrace\b runs \acommand\a with the specified "
	"arguments and displays a trace of each system call, it arguments, "
	"and the return value on standard output.  Arguments that "
	"are long are truncated by default.]"
"[a?Trace and then display a count for each system call.]"
"[c?Display only a count for each system call.]"
"[o?Write the trace output to \afile\a.]:[file]"
"[f?Equivalent to \b-i\b.]"
"[i?Tracing will be inherited by child processes.]"
"[p?Trace process\apid\a.]:[pid]"
"[t?Precede each call with the time in machine cycles that the call started.]"
"[v?The output will be more verbose by not truncating long arguments.]"
"\n"
"\ncommand [arg ...]\n"
"\n"
"[+EXIT STATUS?If successful, the exit status of \btrace\b will that "
	"of \acommand\a.  Otherwise, it will be one of the following]{"
        "[+126?\acommand\a was found but could not be invoked.]"
        "[+127?An error occurred in \btrace\b or \acommand\a could not "
		"be found.]"
"}"
"[+SEE ALSO?\btraceit\b(1)]"
;


#include <ast.h>
#include <stdio.h>
#include <error.h>
#include <ls.h>
#include <uwin.h>
#include <sys/wait.h>
#include <sys/stat.h>

int
main(int argc, char** argv)
{
	int			status;
	pid_t			pid;
	struct spawndata	proc;
	char			cmd[PATH_MAX];
	pid_t			trace_pid = 0;
	char*			trace_out = 0;

	error_info.id = "trace";
	memset(&proc, 0, sizeof(proc));
	proc.trace = 1;
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'a':
			proc.flags |= UWIN_TRACE_CALL|UWIN_TRACE_COUNT;
			continue;
		case 'c':
			proc.flags |= UWIN_TRACE_COUNT;
			continue;
		case 'f':
		case 'i':
			proc.flags |= UWIN_TRACE_INHERIT;
			continue;
		case 'o':
			if (!streq(opt_info.arg, "-"))
				trace_out = strdup(opt_info.arg);
			continue;
		case 'p':
			trace_pid = atoi(opt_info.arg);
			if (kill(trace_pid,0))
			{
				error(ERROR_SYSTEM|4, "%s not an active process", opt_info.arg);
				trace_pid=0;
			}
			continue;
		case 't':
			proc.flags |= UWIN_TRACE_TIME;
			continue;
		case 'v':
			proc.flags |= UWIN_TRACE_VERBOSE;
			continue;
		case '?':
			error(ERROR_USAGE|4, opt_info.arg);
			continue;
		case ':':
			error(2, opt_info.arg);
			continue;
		}
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || (!argv[0] && !trace_pid))
		error(ERROR_USAGE|4, optusage(NiL));
	if (!(proc.flags & (UWIN_TRACE_COUNT|UWIN_TRACE_CALL)))
		proc.flags |= UWIN_TRACE_CALL;
	status = 0;
	if (trace_pid)
	{
		char	trace_file[PATH_MAX];
		char	trace_cmd[PATH_MAX];
		ssize_t	n;
		int	trace_fd;

		sfsprintf(trace_file, sizeof(trace_file), "/proc/%d/trace", trace_pid);
		if ((trace_fd = open(trace_file, O_RDWR)) < 0)
			error(ERROR_SYSTEM|5, "%s: cannot open", trace_file);
		n = sfsprintf(trace_cmd, sizeof(trace_cmd), "%o %s", proc.flags, trace_out ? trace_out : ttyname(1)) + 1;
		if (write(trace_fd, trace_cmd, n) != n)
			error(ERROR_SYSTEM|6, "%s: trace initialization failed", trace_file);
		pause();
	}
	else
	{
		if (trace_out)
		{
			if ((proc.trace = open(trace_out, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
				error(ERROR_SYSTEM|3, "%s: cannot write", trace_out);
			else if (fcntl(proc.trace, F_SETFD, FD_CLOEXEC) < 0)
				error(ERROR_SYSTEM|1, "%s: cannot set close-on-exec", trace_out);
		}
		if (!pathpath(argv[0], NiL, PATH_REGULAR|PATH_EXECUTE, cmd, sizeof(cmd)))
			error(ERROR_SYSTEM|ERROR_NOENT+2, "%s: not found", argv[0]);
		if ((pid = uwin_spawn(cmd, argv, NiL, &proc)) < 0)
			error(ERROR_SYSTEM|ERROR_NOEXEC+2, "%s: cannot run", cmd);
		while (waitpid(pid, &status, 0) == -1)
			if (errno != EINTR)
				exit(EXIT_NOEXEC);
		status = WEXITSTATUS(status);
	}
	return status;
}
