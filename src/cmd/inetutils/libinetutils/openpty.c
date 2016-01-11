/* Slightly modified version of libutil. There's probably
   a better way to do this.
   Adapted to inetutils by Bernhard Rosenkraenzer <bero@startrek.in-trier.de> */
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
#include <errno.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include <grp.h>

int openpty(amaster, aslave, name, termp, winp)
	int *amaster, *aslave;
	char *name;
	struct termios *termp;
	struct winsize *winp;
{
	char * const line = "/dev/ptyXX";
	register const char *cp1, *cp2;
	register int master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrstuvwxyzPQRST"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdef"; *cp2; cp2++) {
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				chown(line, getuid(), ttygid);
				chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				revoke(line);
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave, 
							TCSAFLUSH, termp);
#ifdef TIOCSWINSZ
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ, 
							(char *)winp);
#endif
					return (0);
				}
				(void) close(master);
				line[5] = 'p';
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}
