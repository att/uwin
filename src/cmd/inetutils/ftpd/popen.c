/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
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
 *
 */

#ifndef lint
static char sccsid[] = "@(#)popen.c	8.3 (Berkeley) 4/6/94";
#endif /* not lint */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#undef _BSDCOMPAT
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Include glob.h last, because it may define "const" which breaks
   system headers on some platforms. */
#include <glob.h>

#include "extern.h"

/*
 * Special version of popen which avoids call to shell.  This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */

#define MAX_ARGC 100
#define MAX_GARGC 1000

struct file_pid {
  	FILE *file;
	pid_t pid;
	struct file_pid *next;
};

/* A linked list associating ftpd_popen'd FILEs with pids.  */
struct file_pid *file_pids = 0;

FILE *
ftpd_popen(program, type)
	char *program, *type;
{
	char *cp;
	FILE *iop;
	struct file_pid *fpid;
	int argc, gargc, pdes[2], pid;
	char **pop, *argv[MAX_ARGC], *gargv[MAX_GARGC];

	if (*type != 'r' && *type != 'w' || type[1])
		return (NULL);

	if (pipe(pdes) < 0)
		return (NULL);

	/* break up string into pieces */
	for (argc = 0, cp = program;; cp = NULL) {
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;

    if (argc > MAX_ARGC) return(NULL); /* AUSCERT */
  }
	/* glob each piece */
	gargv[0] = argv[0];
	for (gargc = argc = 1; argv[argc]; argc++) {
		glob_t gl;
		int flags = GLOB_NOCHECK;

#ifdef GLOB_BRACE
				flags |= GLOB_BRACE;
#endif
#ifdef GLOB_QUOTE
				flags |= GLOB_QUOTE;
#endif
#ifdef GLOB_TILDE
				flags |= GLOB_TILDE;
#endif

		memset(&gl, 0, sizeof(gl));
		if (glob(argv[argc], flags, NULL, &gl))
			gargv[gargc++] = strdup(argv[argc]);
		else
			for (pop = gl.gl_pathv; *pop; pop++)
				gargv[gargc++] = strdup(*pop);
		globfree(&gl);
	}
	gargv[gargc] = NULL;

	iop = NULL;
	switch(pid = vfork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto pfree;
		/* NOTREACHED */
	case 0:				/* child */
		if (*type == 'r') {
			if (pdes[1] != STDOUT_FILENO) {
				dup2(pdes[1], STDOUT_FILENO);
				(void)close(pdes[1]);
			}
			dup2(STDOUT_FILENO, STDERR_FILENO); /* stderr too! */
			(void)close(pdes[0]);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
			(void)close(pdes[1]);
		}
		execv(gargv[0], gargv);
		_exit(1);
	}
	/* parent; assume fdopen can't fail...  */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)close(pdes[0]);
	}

	fpid = (struct file_pid *) malloc (sizeof (struct file_pid));
	if (fpid) {
		fpid->file = iop;
		fpid->pid = pid;
		fpid->next = file_pids;
		file_pids = fpid;
	}

pfree:	for (argc = 1; gargv[argc] != NULL; argc++)
		free(gargv[argc]);

	return (iop);
}

int
ftpd_pclose(iop)
	FILE *iop;
{
	struct file_pid *fpid = file_pids, *prev_fpid = 0;
	int fdes, status;
#ifdef HAVE_SIGACTION
	sigset_t sigs, osigs;
#else
	int omask;
#endif
	pid_t pid;

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, or, if already `pclosed'.
	 */
	while (fpid && fpid->file != iop) {
	     prev_fpid = fpid;
	     fpid = fpid->next;
	}
	if (! fpid)
        	return -1;

	if (prev_fpid)
		prev_fpid->next = fpid->next;
	else
		file_pids = fpid->next;

	(void)fclose(iop);
#ifdef HAVE_SIGACTION
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGQUIT);
	sigaddset(&sigs, SIGHUP);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
#else
	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
#endif
	while ((pid = waitpid(fpid->pid, &status, 0)) < 0 && errno == EINTR)
		continue;

	free (fpid);

#ifdef HAVE_SIGACTION
	sigprocmask(SIG_SETMASK, &osigs, 0);
#else
	(void)sigsetmask(omask);
#endif
	if (pid < 0)
		return (pid);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (1);
}
