#pragma prototyped

/*
 * the u/win trace command
 */

static const char id[] = "\n@(#)tracer (AT&T Research) 2000-08-08\0\n";

#include <ast.h>
#include <error.h>
#include <ls.h>
#include <uwin.h>
#include <sys/wait.h>

int main(int argc, char** argv)
{
	int			status;
	pid_t			pid;
	struct spawndata	proc;
	char			cmd[PATH_MAX];
	char			*name;
	int			fd;

	if(name=strrchr(argv[0],'/'))
		name++;
	else
		name = argv[0];
	error_info.id = name;
	memset(&proc, 0, sizeof(proc));
	proc.flags |= UWIN_TRACE_CALL|UWIN_TRACE_TIME;
	if((fd=open("/etc/traceflags",O_RDONLY))>=0)
	{
		if((status=read(fd,cmd,4))>0)
		{
			while(--status>=0)
			{
				if(cmd[status]=='c')
					proc.flags |= UWIN_TRACE_COUNT;
				else if(cmd[status]=='v')
					proc.flags |= UWIN_TRACE_VERBOSE;
				else if(cmd[status]=='i')
					proc.flags |= UWIN_TRACE_INHERIT;
			}
		}
		close(fd);
	}
	else
		proc.flags |= UWIN_TRACE_VERBOSE|UWIN_TRACE_INHERIT|UWIN_TRACE_COUNT;
	sfsprintf(cmd,sizeof(cmd),LOGDIR "trace/%s.log",name);
	if ((proc.trace = open(cmd, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
		error(ERROR_SYSTEM|3, "%s: cannot write", opt_info.arg);
	fcntl(proc.trace, F_SETFD, FD_CLOEXEC);
	if (error_info.errors || !argv[0])
		error(ERROR_USAGE|4, optusage(NiL));
	sfsprintf(cmd,sizeof(cmd),LOGDIR "trace/%s",name);
	if(access(cmd,X_OK)!=0)
		error(ERROR_SYSTEM|ERROR_NOENT, "%s: not found", cmd);
	if (!(proc.flags & (UWIN_TRACE_COUNT|UWIN_TRACE_CALL)))
		proc.flags |= UWIN_TRACE_CALL;
	if ((pid = uwin_spawn(cmd, argv, NiL, &proc)) < 0)
		error(ERROR_SYSTEM|ERROR_NOEXEC, "%s: cannot run", cmd);
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			exit(EXIT_NOEXEC);
	return(WEXITSTATUS(status));
}
