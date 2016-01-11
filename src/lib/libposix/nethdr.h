/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2012 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#ifndef _NET_HDR_H
#define _NET_HDR_H	1

extern int	ntpid2pid(int);
extern int	ntpid2uid(int);

extern int	get_net_tcpipv4(void*, int, HANDLE);
extern int	get_net_tcpipv6(void*, int, HANDLE);
extern int	get_net_udpipv4(void*, int, HANDLE);
extern int	get_net_udpipv6(void*, int, HANDLE);

#endif
