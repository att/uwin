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
/*
 * Network display functions to access Windows functions without
 * causing problems by inclusion of UWIN posix.dll headers.
 */

#if _X64_ && !defined(_AMD64_)
#define _AMD64_		1
#endif

typedef void VOID;

#define sprintf_s		snprintf

#include <WinDef.h>
#include <WinBase.h>
#include <WinSock2.h>

#undef	FD_SET
#undef	FD_CLR
#undef	FD_ISSET
#undef	FD_ZERO

#define fd_set			_uwin_fd_set
#define gethostname		_uwin_gethostname
#define select			_uwin_select
#define timeval			_uwin_timeval

#include <windows.h>
#include <ws2tcpip.h>
#include <IPHlpapi.h>

#ifndef _TCPMIB_
#define TCP_TABLE_OWNER_PID_ALL	5
#endif

#ifndef _UDPMIB_
#define UDP_TABLE_OWNER_PID	1
#endif

#include "nethdr.h"
#include "dl.h"

#define logerr(d,a ...) 
#define logmsg(d,a ...) 

extern int sfsprintf(char*, size_t, const char*, ...);

#define UDP_REMOTE "*"

/*
 * local definition to avoid linking with winsock2 dll, used in net_disp
 */
extern unsigned long sock_ntohl(unsigned long);
extern unsigned short sock_ntohs(unsigned short);
extern const char *sock_inet_ntop(int af, const void *src, char *dst, size_t size);

static int ninit = 0;

static int (PASCAL *sGetExtendedTcpTable)(void *, int *, int, unsigned long, int, unsigned long);
static int (PASCAL *sGetExtendedUdpTable)(void *, int *, int, unsigned long, int, unsigned long);

#define NET_INIT(r)	do { if (!ninit) get_net_init(); if (ninit < 0) return r; } while (0)

static void get_net_init(void)
{
	if (!ninit)
	{
		if ((sGetExtendedTcpTable = (int (PASCAL*)(void *, int *, int, unsigned long, int, unsigned long))getsymbol(MODULE_iphlpapi, "GetExtendedTcpTable")) &&
		    (sGetExtendedUdpTable = (int (PASCAL*)(void *, int *, int, unsigned long, int, unsigned long))getsymbol(MODULE_iphlpapi, "GetExtendedUdpTable")))
			ninit = 1;
		else
			ninit = -1;
	}
}

#define FMT_tcpipv4	"%4d %8.8x:%5.5d %8.8x:%5.5d %.2x %8.8x %8.8x %6.6d %6.6d %6.6d %d\n"

int get_net_tcpipv4(void* pp, int fid, HANDLE hp)
{
	int i, n, ret;
	int ipv4_sz;
	MIB_TCPTABLE_OWNER_PID ent,*ipv4;
	char buf[512];

	NET_INIT(-1);
	/* Determine number of tcp table entries */
	ipv4_sz = 0;
	if (sGetExtendedTcpTable)
	{
		if ((*sGetExtendedTcpTable)(&ent, &ipv4_sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			ipv4 = (MIB_TCPTABLE_OWNER_PID *) malloc(ipv4_sz);
			ret = (*sGetExtendedTcpTable)(ipv4, &ipv4_sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
			if (ret != NO_ERROR)
			{
				logerr(0, "Could not get TCP v4 table");
				goto free_ret;
			}
		}
	}
	else
		return 0;

	ipv4_sz = ipv4_sz / sizeof(MIB_TCPTABLE_OWNER_PID);
logmsg(0, "disp_tcpipv4 size=%d", ipv4_sz);
	for (i=0; i < ipv4_sz; i++)
	{
		n=sfsprintf(buf, sizeof(buf), FMT_tcpipv4, i,
			sock_ntohl(ipv4->table[i].dwLocalAddr),
			sock_ntohs((u_short)ipv4->table[i].dwLocalPort),
			sock_ntohl(ipv4->table[i].dwRemoteAddr),
			sock_ntohs((u_short)ipv4->table[i].dwRemotePort),
			ipv4->table[i].dwState,
			/* Windows doesn't keep statistics by default */
			0,0,-1,
			ipv4->table[i].dwOwningPid,
			/* and UWIN proc info */
			ntpid2pid(ipv4->table[i].dwOwningPid),
			ntpid2uid(ipv4->table[i].dwOwningPid));
		WriteFile(hp, buf, n, &n, NULL);
	}
free_ret:
	free(ipv4);
	return 0;
}

#define FMT_tcpipv6	"%4d %32.32s:%5.5d %32.32s:%5.5d %.2x %8.8x %8.8x %6.6d %6.6d %6.6d %d\n"

int get_net_tcpipv6(void* pp, int fid, HANDLE hp)
{
	int i, n, ret;
	int ipv6_sz;
	MIB_TCP6TABLE_OWNER_PID ent,*ipv6;
	char buf[512];
	char addrbuf[128];

	NET_INIT(-1);
	/* Determine number of tcp table entries */
	ipv6_sz = 0;
	if (sGetExtendedTcpTable)
	{
		if ((*sGetExtendedTcpTable)(&ent, &ipv6_sz, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			ipv6 = (MIB_TCP6TABLE_OWNER_PID *) malloc(ipv6_sz);
			ret = (*sGetExtendedTcpTable)(ipv6, &ipv6_sz, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
			if (ret != NO_ERROR)
			{
				logerr(0, "Could not get TCP v4 table");
				goto free_ret;
			}
		}
	}
	else
		return 0;

	ipv6_sz = ipv6_sz / sizeof(MIB_TCP6TABLE_OWNER_PID);
logmsg(0, "disp_tcpipv6 size=%d", ipv6_sz);
	for (i=0; i < ipv6_sz; i++)
	{
		n=sfsprintf(buf, sizeof(buf), FMT_tcpipv6, i,
			sock_inet_ntop(AF_INET6, (void *)&(ipv6->table[i].ucLocalAddr), addrbuf, sizeof(addrbuf)),
			sock_ntohs((u_short)ipv6->table[i].dwLocalPort),
			sock_inet_ntop(AF_INET6, (void *)&(ipv6->table[i].ucRemoteAddr), addrbuf, sizeof(addrbuf)),
			sock_ntohs((u_short)ipv6->table[i].dwRemotePort),
			ipv6->table[i].dwState,
			/* Windows doesn't keep statistics by default */
			0,0,-1,
			ipv6->table[i].dwOwningPid,
			/* and UWIN proc info */
			ntpid2pid(ipv6->table[i].dwOwningPid),
			ntpid2uid(ipv6->table[i].dwOwningPid));
		WriteFile(hp, buf, n, &n, NULL);
		WriteFile(hp, buf, n, &n, NULL);
	}
free_ret:
	free(ipv6);
	return 0;
}

#define FMT_udpipv4	"%4d %8.8x:%5.5d        %s:%s     %.2x %8.8x %8.8x %6.6d %6.6d %6.6d %d\n"

int get_net_udpipv4(void* pp, int fid, HANDLE hp)
{
	int i, n, ret;
	int ipv4_sz;
	MIB_UDPTABLE_OWNER_PID ent,*ipv4;
	char buf[512];

	NET_INIT(-1);
	/* Determine number of tcp table entries */
	ipv4_sz = 0;
	if (sGetExtendedUdpTable)
	{
		if ((*sGetExtendedUdpTable)(&ent, &ipv4_sz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			ipv4 = (MIB_UDPTABLE_OWNER_PID *) malloc(ipv4_sz);
			ret = (*sGetExtendedUdpTable)(ipv4, &ipv4_sz, FALSE,AF_INET, UDP_TABLE_OWNER_PID, 0);
			if (ret != NO_ERROR)
			{
				logerr(0, "Could not get UDP v4 table");
				goto free_ret;
			}
		}
	}
	else
		return 0;

	ipv4_sz = ipv4_sz / sizeof(MIB_UDPTABLE_OWNER_PID);
logmsg(0, "disp_udpipv4 size=%d", ipv4_sz);
	for (i=0; i < ipv4_sz; i++)
	{
		n=sfsprintf(buf, sizeof(buf), FMT_udpipv4, i,
			sock_ntohl(ipv4->table[i].dwLocalAddr),
			sock_ntohs((u_short)ipv4->table[i].dwLocalPort),
			/* No remote addresses for UDP */
			UDP_REMOTE, UDP_REMOTE,
			/* UDP state is disconnected */
			1,
			/* Windows doesn't keep statistics by default */
			0,0,-1,
			ipv4->table[i].dwOwningPid,
			/* and UWIN proc info */
			ntpid2pid(ipv4->table[i].dwOwningPid),
			ntpid2uid(ipv4->table[i].dwOwningPid));
		WriteFile(hp, buf, n, &n, NULL);
	}
free_ret:
	free(ipv4);
	return 0;
}

#define FMT_udpipv6	"%4d %32.32s:%5.5d                                %s:%s     %.2x %8.8x %8.8x %6.6d %6.6d %6.6d %d\n"

int get_net_udpipv6(void* pp, int fid, HANDLE hp)
{
	int i, n, ret;
	int ipv6_sz;
	MIB_UDP6TABLE_OWNER_PID ent,*ipv6;
	char buf[512];
	char addrbuf[128];

#if 0
	sock_inet_ntop(AF_INET6,
		(void *)&(((struct sockaddr_in6 *)(cinfo->LocalAddr.lpSockaddr))->sin6_addr),
		addrbuf, sizeof(addrbuf)),
#endif
	NET_INIT(-1);
	/* Determine number of tcp table entries */
	ipv6_sz = 0;
	if (sGetExtendedUdpTable)
	{
		if ((*sGetExtendedUdpTable)(&ent, &ipv6_sz, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			ipv6 = (MIB_UDP6TABLE_OWNER_PID *) malloc(ipv6_sz);
			ret = (*sGetExtendedUdpTable)(ipv6, &ipv6_sz, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
			if (ret != NO_ERROR)
			{
				logerr(0, "Could not get UDP v4 table");
				goto free_ret;
			}
		}
	}
	else
		return 0;

	ipv6_sz = ipv6_sz / sizeof(MIB_UDP6TABLE_OWNER_PID);
logmsg(0, "disp_udpipv6 size=%d", ipv6_sz);
	for (i=0; i < ipv6_sz; i++)
	{
		n=sfsprintf(buf, sizeof(buf), FMT_udpipv6, i,
			sock_inet_ntop(AF_INET6, (void *)&(ipv6->table[i].ucLocalAddr), addrbuf, sizeof(addrbuf)),
			sock_ntohs((u_short)ipv6->table[i].dwLocalPort),
			/* No remote addresses for UDP */
			UDP_REMOTE, UDP_REMOTE,
			/* UDP state is disconnected */
			1,
			/* Windows doesn't keep statistics by default */
			0,0,-1,
			ipv6->table[i].dwOwningPid,
			/* and UWIN proc info */
			ntpid2pid(ipv6->table[i].dwOwningPid),
			ntpid2uid(ipv6->table[i].dwOwningPid));
		WriteFile(hp, buf, n, &n, NULL);
	}
free_ret:
	free(ipv6);
	return 0;
}
