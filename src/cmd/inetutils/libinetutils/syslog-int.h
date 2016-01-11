/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *
 *	@(#)syslog.h	8.1 (Berkeley) 6/2/93
 */

/* This is just the parts of syslog.h enabled by defining SYSLOG_NAMES, for
   systems that don't have that feature.  */

#ifndef __SYSLOG_NAMES_H__
#define __SYSLOG_NAMES_H__

#ifndef LOG_PRI
#define	LOG_PRI(p)	((p) & LOG_PRIMASK)
#endif
#ifndef LOG_MAKEPRI
#define	LOG_MAKEPRI(fac, pri)	(((fac) << 3) | (pri))
#endif
#ifndef LOG_FAC
#define	LOG_FAC(p)	(((p) & LOG_FACMASK) >> 3)
#endif

#ifdef SYSLOG_NAMES

#define	INTERNAL_NOPRI	0x10	/* the "no priority" priority */
				/* mark "facility" */
#define	INTERNAL_MARK	LOG_MAKEPRI(LOG_NFACILITIES, 0)
typedef struct _code {
	char	*c_name;
	int	c_val;
} CODE;

CODE prioritynames[] = {
#ifdef LOG_ALERT
	"alert",	LOG_ALERT,
#endif
#ifdef LOG_CRIT
	"crit",		LOG_CRIT,
#endif
#ifdef LOG_DEBUG
	"debug",	LOG_DEBUG,
#endif
#ifdef LOG_EMERG
	"emerg",	LOG_EMERG,
#endif
#ifdef LOG_ERR
	"err",		LOG_ERR,
	"error",	LOG_ERR,		/* DEPRECATED */
#endif
#ifdef LOG_INFO
	"info",		LOG_INFO,
#endif
	"none",		INTERNAL_NOPRI,		/* INTERNAL */
#ifdef LOG_NOTICE
	"notice",	LOG_NOTICE,
#endif
#ifdef LOG_EMERG
	"panic", 	LOG_EMERG,		/* DEPRECATED */
#endif
#ifdef LOG_WARNING
	"warn",		LOG_WARNING,		/* DEPRECATED */
	"warning",	LOG_WARNING,
#endif
	NULL,		-1,
};

CODE facilitynames[] = {
#ifdef LOG_AUTH
	"auth",		LOG_AUTH,
#endif
#ifdef LOG_AUTHPRIV
	"authpriv",	LOG_AUTHPRIV,
#endif
#ifdef LOG_CRON
	"cron", 	LOG_CRON,
#endif
#ifdef LOG_DAEMON
	"daemon",	LOG_DAEMON,
#endif
#ifdef LOG_FTP
	"ftp",		LOG_FTP,
#endif
#ifdef LOG_KERN
	"kern",		LOG_KERN,
#endif
#ifdef LOG_LPR
	"lpr",		LOG_LPR,
#endif
#ifdef LOG_MAIL
	"mail",		LOG_MAIL,
#endif
	"mark", 	INTERNAL_MARK,		/* INTERNAL */
#ifdef LOG_NEWS
	"news",		LOG_NEWS,
#endif
#ifdef LOG_AUTH
	"security",	LOG_AUTH,		/* DEPRECATED */
#endif
#ifdef LOG_SYSLOG
	"syslog",	LOG_SYSLOG,
#endif
#ifdef LOG_USER
	"user",		LOG_USER,
#endif
#ifdef LOG_UUCP
	"uucp",		LOG_UUCP,
#endif
#ifdef LOG_LOCAL0
	"local0",	LOG_LOCAL0,
#endif
#ifdef LOG_LOCAL1
	"local1",	LOG_LOCAL1,
#endif
#ifdef LOG_LOCAL2
	"local2",	LOG_LOCAL2,
#endif
#ifdef LOG_LOCAL3
	"local3",	LOG_LOCAL3,
#endif
#ifdef LOG_LOCAL4
	"local4",	LOG_LOCAL4,
#endif
#ifdef LOG_LOCAL5
	"local5",	LOG_LOCAL5,
#endif
#ifdef LOG_LOCAL6
	"local6",	LOG_LOCAL6,
#endif
#ifdef LOG_LOCAL7
	"local7",	LOG_LOCAL7,
#endif
	NULL,		-1,
};

#endif /* SYSLOG_NAMES */

#endif /* __SYSLOG_NAMES_H__ */
