/* Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include	"uwindef.h"
#include	<libutil.h>
#include	<uwin.h>

int ptym_open(char *ptyslave)
{
	int fdm;
	char *pslave;

	fdm = open("/dev/ptymx", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if(fdm < 0)
		return(fdm);
	if(pslave = ptsname(fdm))
		strcpy(ptyslave, pslave);
	else
	{
		close(fdm);
		errno = ENODEV;
		fdm = -1;
	}
	return(fdm);
}

int ptys_open(const char *ptyslave)
{
	return(open(ptyslave, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH));
}

int revoke (const char *path)
{
	errno = ENOSYS;
	return(-1);
}

int login_tty(int fd)
{
	setsid();
	if (ioctl(fd, TIOCSCTTY, (char *)NULL) == -1)
		return (-1);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if(fd > 2)
		close(fd);
	return (0);
}

int openpty(int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp)
{
	char line[256];
	register int master, slave, ttygid;
#if 0
	struct group *gr;
	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
#endif
		ttygid = -1;

	if((master = ptym_open(line)) <0)
		return(-1);
	chown(line, getuid(), ttygid);
	chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
	revoke(line);
	if((slave = ptys_open(line))>=0)
	{
		*amaster = master;
		*aslave = slave;
		if(name)
			strcpy(name, line);
		if (termp)
			tcsetattr(slave, TCSAFLUSH, termp);
#ifdef TIOCSWINSZ
		if (winp)
			ioctl(slave, TIOCSWINSZ, (char *)winp);
#endif
		return (0);
	}
	(void) close(master);
	errno = ENOENT;	/* out of ptys */
	return(-1);
}

int forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp)
{
	int master, slave, pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return (-1);
	switch (pid = fork())
	{
	case -1:
		return (-1);
	case 0:
		/*
		 * child
		 */
		close(master);
		login_tty(slave);
		return (0);
	}
	/*
	 * parent
	 */
	*amaster = master;
	close(slave);
	return (pid);
}

pid_t uwin_fork(register int flags)
{
	int fd;
	P_CP->inexec |= PROC_DAEMON;
	switch (fd=fork())
	{
	    case -1:
		return (-1);
	    case 0:
		break;
	    default:
		if(flags&UWIN_KILL_PARENT)
			_exit(0);
		return(fd);
	}
	if ((flags&UWIN_NEW_SESSION) &&  setsid() == -1)
		return (-1);
	if (flags&UWIN_CHDIR_ROOT)
		chdir("/");
	if ((flags&UWIN_DEVNULL) && (fd = open("/dev/null", O_RDWR, 0)) != -1)
	{
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close (fd);
	}
	return (0);
}

int daemon(int nochdir, int noclose)
{
	int flags = UWIN_NEW_SESSION|UWIN_KILL_PARENT;
	if(!nochdir)
		flags |= UWIN_CHDIR_ROOT;
	if(!noclose)
		flags |= UWIN_DEVNULL;
	return((int)uwin_fork(flags));
}

