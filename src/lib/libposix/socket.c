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
#include	"socket.h"
#ifdef GTL_TERM
#   include	"raw.h"
#endif
#include	<alloca.h>
#include	<ast/swap.h>
#include	<poll.h>
#include	"WinBase.h"
#include	"uwin_serve.h"

#define FD_MOST_EVENTS	(FD_READ|FD_WRITE|FD_CLOSE|FD_CONNECT|FD_ACCEPT)
#define FROM_PROTOCOL_INFO	(-1)
#undef LOGIT

/*  DEBUGGING START
 *
 * The following definitions are for debugging purposes in this file.
 *
#define SOCKDEBUG	1
 */

/*
 * Uncomment the following definition to force checking for availability of Winsock 2.0
 * as is the case for the first process after a reboot.
 */
#define FORCE_WINSOCK_INIT

/*********************************************************************************
	From UNIX Network Programming,  Networking APIs: Sockets and XTI
	Volume 1, Second Edition
	W. Richard Stevens.

Summary of networking ioctl requests

category	request			Description								Datatype
---------------------------------------------------------------------------------
socket		SIOCATMARK		at out-of-band mark?					int
			SIOCSPGRP		set process / group ID of the socket 	int
			SIOCGPGRP		get process / group Id of the socket	int


interface	SIOCGIFCONF		get list of all interfaces				struct ifconf
			SIOCSIFADDR		set interface address					struct ifreq
			SIOCGIFADDR		get interface addresses					struct ifreq
			SIOCSIFFLAGS	set interface flags						struct ifreq
			SIOCGIFFLAGS	get interface flags						struct ifreq
			SIOCSIFDSTADDR	set point-to-point address				struct ifreq
			SIOCGIFDSTADDR	get point-to-point address				struct ifreq
			SIOCGIFBRDADDR	get broadcast address					struct ifreq
			SIOCSIFBRDADDR	set broadcast address					struct ifreq
			SIOCGIFNETMASK	get subnet mask							struct ifreq
			SIOCSIFNETMASK	set subnet mask							struct ifreq
			SIOCGIFMETRIC	get interface metric					struct ifreq
			SIOCSIFMETRIC	set interface metric					struct ifreq
**********************************************************************************/

/*
 * ioctlsocket data
 *
 *	Unix socket flags as defined in
 *	net/if.h
 *	We create two arrays: one for the unix implementation and one
 *	for the win32 semi-equivalents.....
 *	This is used in routines for getting/setting flags
 *
 */

#define PROTOENT 0
#define SERVENT 1
#define HOSTENT 2

static char hostbuff[256];
static char protobuff[256];
static char servbuff[256];

static unix_win32_iffmap ifFlagMap[] =
{
	{IFF_UP, SO_ACCEPTCONN},
	{IFF_BROADCAST, SO_BROADCAST},
	{IFF_DEBUG, SO_DEBUG},
	{IFF_LOOPBACK, -1},
	{IFF_POINTOPOINT, -1},
	{IFF_NOTRAILERS, -1},
	{IFF_RUNNING, -1},
	{IFF_NOARP, -1},
	{IFF_PROMISC -1},
	{-1, -1}	/* sentinel */
};

/*************************************************/

int		sinit;

static int	lasterror;
static WSADATA	wsadata;

static int (PASCAL *sWSAGetLastError)(void);
static int (PASCAL *sWSASetLastError)(int);
static int (PASCAL *sWSACleanup)();
static int (PASCAL *ssend)(HANDLE,const char*,int,int);
static int (PASCAL *srecv)(HANDLE,char*,int,int);
static int (PASCAL *sgetsockopt)(HANDLE,int,int,char*,int*);
static int (PASCAL *ssetsockopt)(HANDLE,int,int,const char*,int);
static int (PASCAL *sconnect)(HANDLE, struct sockaddr*,int);
static int (PASCAL *sshutdown)(HANDLE,int);
static HANDLE (PASCAL *ssocket)(int,int,int);
static int (PASCAL *sclose)(HANDLE);
static int (PASCAL *sWSAIoctl)(int, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
static int (PASCAL *sbind)(HANDLE,struct sockaddr*,int);
static HANDLE (PASCAL *sWSAJoinLeaf) (HANDLE, const struct sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS, DWORD);
static char * (PASCAL *sgai_strerror)(int);
static int (PASCAL *sgetsockname)(HANDLE,struct sockaddr*,int*);
static int (PASCAL *sgetnameinfo)(const struct sockaddr *, socklen_t, char *, DWORD, char *, DWORD, int);
static int (PASCAL *sgetaddrinfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
static int (PASCAL *sfreeaddrinfo)(struct addrinfo *);
static unsigned int (PASCAL *sif_nametoindex)(const char *);
static char * (PASCAL *sif_indextoname)(unsigned int, char *);
static struct if_nameindex * (PASCAL *sif_nameindex)(void);
static void (PASCAL *sif_freenameindex)(struct if_nameindex *);
static int (PASCAL *sinet_pton)(int, const char *, void *);
static const char *(PASCAL *sinet_ntop)(int, const void *, char *, size_t);

static HANDLE enable_multicast(int);
static void set_herrno(int);

#define h_error	(sWSAGetLastError?(*sWSAGetLastError)():EOPNOTSUPP)

/*
 * Global constants for IPv6
 */
const struct ipv6_info ipv6_info =
{
	IN6ADDR_ANY_INIT,
	IN6ADDR_LOOPBACK_INIT,
	IN6ADDR_INTFACELOCAL_ALLNODES_INIT,
	IN6ADDR_LINKLOCAL_ALLNODES_INIT,
	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
};

/*
 *	The following two objects for use in gethostname()
 */

static struct qparam ParamsNT =
{
	"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
	"Domain",
	"Hostname"
};

static struct qparam Params95 =
{
	"System\\CurrentControlSet\\Services\\VxD\\MSTCP",
	"Domain",
	"HostName"
};

static int is_socket(int fd, int socktype)
{
	Pfd_t *fdp=getfdp(P_CP,fd);
	if(fdp->type == socktype)
		return(1);
	return(0);
}

int is_anysocket(Pfd_t *fdp)
{
	return fdp->type == FDTYPE_SOCKET || fdp->type == FDTYPE_DGRAM || fdp->type == FDTYPE_CONNECT_INET || fdp->type == FDTYPE_CONNECT_UNIX || fdp->type == FDTYPE_PIPE || fdp->type == FDTYPE_EPIPE;

}

void setNetEvent(int fd,int event,int code)
{
	getfdp(P_CP,fd)->socknetevents |= event;
	SetEvent(Xhandle(fd));
}

void reset_winsock(void)
{
	if(sinit)
	{
		lasterror = GetLastError();
		(*sWSACleanup)();
	}
}

FARPROC getaddr(const char* name)
{
	FARPROC	fp = 0;
	int	sigsys;

#ifdef FORCE_WINSOCK_INIT
	Share->sockstate = WS_UNINITIALIZED;
#endif
	if (Share->sockstate == WS_NONE)
		return 0;
	if(state.clone.hp)
	{
		int n;
		n = (int)strlen(name)+1;
		if(fp=(FARPROC)cl_sockops(SOP_INIT, NULL, NULL, (char*)name, &n))
			sinit = 1;
		return(fp);
	}
	sigsys = sigdefer(1);
	if (!sinit)
	{
		int (PASCAL* sWSAStartup)(WORD, WSADATA*);

		Share->sockstate = WS_NONE;
		if (!(sWSAStartup = (int (PASCAL*)(WORD, WSADATA*))getsymbol(MODULE_winsock, "WSAStartup")) ||
		    !(sWSACleanup = (int (PASCAL*)(void))getsymbol(MODULE_winsock, "WSACleanup")) ||
		    !(sWSAGetLastError = (int (PASCAL*)(void))getsymbol(MODULE_winsock, "WSAGetLastError")) ||
		    !(sWSASetLastError = (int (PASCAL*)(int))getsymbol(MODULE_winsock, "WSASetLastError")))
		{
			errno = ESOCKTNOSUPPORT;
			goto done;
		}
		if (lasterror = (*sWSAStartup)(MAKEWORD(2, 2), &wsadata))
		{
			logmsg(0, "winsock2 startup failed err=%d", lasterror);
			goto done;
		}
		if (LOBYTE(wsadata.wVersion) < 2)
		{
			/* Could not find version 2.x. */
			(*sWSACleanup)();
			logmsg(0, "winsock2 startup failed err=%d", lasterror);
			goto done;
		}
		switch (HIBYTE(wsadata.wVersion))
		{
		case 0:
		case 1:
			Share->sockstate = WS_20;
			break;
		case 2:
			Share->sockstate = WS_22;
			break;
		}
		sinit = 1;
		logmsg(LOG_DEV+7, "Winsock 2.%d started", HIBYTE(wsadata.wVersion));
	}
	if (name && !(fp = getsymbol(MODULE_winsock, name)))
		logerr(0, "getsymbol %d %s failed", MODULE_winsock, name);
done:
	sigcheck(sigsys);
	return fp;
}


/* Given a file descriptor return a non-connected or a connected internet socket HANDLE,
   or a non-connected UNIX socket handle.
 */
static HANDLE gethandle(int fd)
{
	Pfd_t *fdp;
	if (!isfdvalid (P_CP,fd))
	{
		errno = EBADF;
		return(0);
	}
	fdp = getfdp(P_CP,fd);
	switch(fdp->type)
	{
	    case FDTYPE_PIPE:
		if(!fdp->index)
			break;
	    case FDTYPE_EPIPE:
	    case FDTYPE_SOCKET:
	    case FDTYPE_DGRAM:
	    case FDTYPE_CONNECT_UNIX:
	    case FDTYPE_CONNECT_INET:
		return(Phandle(fd));
	}
	errno = ENOTSOCK;
	return(0);
}

static int getnetevents(int fd, int* errorCodes, int reset, DWORD mask, int code)
{
	WSANETWORKEVENTS netevents;
	int	ret,event,old;
	Pfd_t	*fdp;
	static int (PASCAL *senum_network_events)(HANDLE, HANDLE, WSANETWORKEVENTS*);
	ZeroMemory((void*)&netevents, sizeof(netevents));
	if(!sinit || !senum_network_events)
		senum_network_events=(int (PASCAL *)(HANDLE, HANDLE, WSANETWORKEVENTS*))getaddr("WSAEnumNetworkEvents");

	if(state.clone.hp)
		ret = cl_netevents(senum_network_events,Phandle(fd),Xhandle(fd),&netevents,sizeof(WSANETWORKEVENTS));
	
	else
		ret = (*senum_network_events)(Phandle(fd), Xhandle(fd), &netevents);
	if(ret)
	{
		errno = unix_err(h_error);
		return (-1);
	}
	if(errorCodes)
	{
		memcpy((char*)errorCodes, (char*)netevents.iErrorCode, (FD_MAX_EVENTS+1) * sizeof(int));
		if(errorCodes[0] && errorCodes[FD_ACCEPT_BIT])
			logmsg(0, "accept error bit set=%d", errorCodes[FD_ACCEPT_BIT]);
	}
	fdp = getfdp(P_CP,fd);
	old = fdp->socknetevents;
	event = (netevents.lNetworkEvents & (FD_OOB|FD_READ|FD_WRITE|FD_CLOSE|FD_CONNECT|FD_ACCEPT)) | old;
	if((netevents.lNetworkEvents&FD_OOB) && fdp->sigio)
	{
		Pproc_t *pp=proc_ptr(fdp->sigio);
		kill1((fdp->type&O_EXCL)?-pp->pid:pp->pid,SIGURG);
	}
	ret = (event&mask);
	fdp->socknetevents |= (event&~ret);
	if(reset)
		fdp->socknetevents &= ~ret;
	else
		fdp->socknetevents |= ret;
	if(event & FD_CLOSE)
	{
		fdp->socknetevents |= FD_CLOSE;
		SetEvent(Xhandle(fd));
	}
#ifdef LOGIT
	logmsg(LOG_DEV+5, "getnetevents %d fd=%d slot=%d netevent=0x%x reset=%d mask=0x%x old=0x%x new=0x%x event=0x%x ret=0x%x handle=%p", code, fd, file_slot(fdp), netevents.lNetworkEvents, reset, mask, old, fdp->socknetevents, event, ret, Xhandle(fd));
#endif
	return(ret);
}

/* ================================================================
 *  This whole section of code is to get around the problem of
 *  socket termination in which if the process that created the
 *  socket closes, the socket no longer works
 *
 *  This code sends a message down a pipe to a thread in the init
 *  process to duplicate the socket.
 *  A message is sent to this thread to close the socket.
 */
struct Msock	/* inet socket creation message */
{
	DWORD		ntpid;
	unsigned short	fslot;
	WSAPROTOCOL_INFO info;
};

static DWORD WINAPI mksock_thread(void *arg)
{
	static HANDLE (PASCAL *sWSAsocket)(int,int,int,WSAPROTOCOL_INFO*,GROUP,DWORD);
	struct Msock	msock;
	HANDLE		hp=(void*)arg,sock,*table;
	DWORD		size;

	sWSAsocket = (HANDLE (PASCAL*)(int,int,int,WSAPROTOCOL_INFO*,GROUP,DWORD))getaddr("WSASocketA");
	sclose =  (int (PASCAL*)(HANDLE))getaddr("closesocket");
	sshutdown =  (int (PASCAL*)(HANDLE, int))getaddr("shutdown");
	if(!sWSAsocket || !sclose || !sshutdown)
		goto done;
	if(!(table = malloc(Share->nfiles*sizeof(HANDLE))))
		goto done;
	ZeroMemory((void*)table, (Share->nfiles*sizeof(HANDLE)));
	while(ReadFile(hp,(void*)&msock,sizeof(msock), &size,NULL))
	{
		if(size!=sizeof(struct Msock))
		{
			logmsg(0, "incorrect message received size=%d", size);
			continue;
		}
		if(msock.fslot >= Share->nfiles)
		{
			logmsg(0, "incorrect file slot number=%d", msock.fslot);
			continue;
		}
		if(!msock.ntpid)
		{
			if(sock=table[msock.fslot])
			{
				(*sshutdown)(sock,SD_SEND);
				htab_socket(sock,0,HTAB_CLOSE,"msock_thread");
				(*sclose)(sock);
			}
			continue;
		}
		sock = (*sWSAsocket)(AF_INET, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &msock.info,0,0);
		if(sock!=_INVALID_SOCKET)
			table[msock.fslot] = sock;
	}
done:
	closehandle(hp,HT_PIPE);
	closehandle(Share->init_sock,HT_PIPE);
	Share->init_sock = 0;
	return(0);
}

/*
 * start the thread in the init process
 */
void	start_sockgen(void)
{
	HANDLE			hpin,hpout;
	DWORD			id;
	if(!CreatePipe(&hpin, &hpout, sattr(0), 512))
		return;
	if(CreateThread(NULL,0,mksock_thread,(void*)hpin,0,&id))
		Share->init_sock = hpout;
	else
		logerr(0, "CreateThread");
}

/*
 * duplicate the socket to the init process
 */
static int sock_to_init(Pfd_t *fdp, HANDLE sock)
{
	static int (PASCAL *sdupsocket)(HANDLE, DWORD, WSAPROTOCOL_INFO*);
	struct Msock		msock;
	HANDLE			hp=0,ph,me=GetCurrentProcess();
	DWORD			ntpid;
	int			r=0,size;

	if(!sinit || !sdupsocket)
		sdupsocket =  (int (PASCAL*)(HANDLE, DWORD, WSAPROTOCOL_INFO*))getaddr("WSADuplicateSocketA");
	if(!sdupsocket)
		return(0);
	msock.ntpid = P_CP->ntpid;
	msock.fslot = file_slot(fdp);
	ntpid = proc_ptr(Share->init_slot)->ntpid;
	if(!(ph=open_proc(ntpid,0)))
		return(0);
	if(!DuplicateHandle(ph,Share->init_sock,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		logerr(0, "DuplicateHandle init_sock=%p", Share->init_sock);
		goto done;
	}
	if(state.clone.hp)
		r = (int)cl_sockops(SOP_DUP,sdupsocket,sock,&msock.info,&ntpid);
	else
		r = (*sdupsocket)(sock,ntpid,&msock.info);
	if(r<0)
	{
		r = 0;
		logerr(0, "DuplicateSocket ntpid=%d sock=%p", ntpid, sock);
		goto done;
	}
	if(!WriteFile(hp,(void*)&msock,sizeof(msock),&size,NULL) || size!=sizeof(msock))
	{
		logerr(0, "WriteFile");
		goto done;
	}
	fdp->oflag |= O_CDELETE;
	r = 1;
done:
	closehandle(ph,HT_PROC);
	if(hp)
		closehandle(hp,HT_PIPE);
	return(r);
}

static void sock_done(Pfd_t *fdp)
{
	struct Msock 	msock;
	HANDLE		hp,ph,me=GetCurrentProcess();
	int		size;

	msock.ntpid = 0;
	msock.fslot = file_slot(fdp);
	if(!(ph=open_proc(proc_ptr(Share->init_slot)->ntpid,0)))
		return;
	if(!DuplicateHandle(ph,Share->init_sock,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle init_sock=%p", Share->init_sock);
	else if(!WriteFile(hp,(void*)&msock,sizeof(msock),&size,NULL))
		logerr(0, "WriteFile");
	closehandle(hp,HT_PIPE);
	closehandle(ph,HT_PROC);
}
/* ================================================================*/


/*
 * This is called from Fs_dup() when a handle has been replaced
 */
int sock_mapevents(HANDLE hp, HANDLE event, int on)
{
	static int (PASCAL *sevent_select)(HANDLE, HANDLE, long);
	DWORD mask=0;
	int r;
	if(!sinit || !sevent_select)
		sevent_select=(int (PASCAL *)(HANDLE, HANDLE, long))getaddr("WSAEventSelect");
	if(on)
		mask = FD_OOB|FD_READ|FD_WRITE|FD_CLOSE|FD_ACCEPT|FD_CONNECT;
	if(!sevent_select )
		return(0);
	if(state.clone.hp)
		r = cl_netevents(sevent_select,hp,event,(void*)mask,0);
	else
		r = (*sevent_select)(hp,event,mask);
	if(r==0)
		return(1);
	if(!on)
	{
		if(state.clone.hp)
			r = cl_netevents(sevent_select,hp,NULL,(void*)mask,0);
		else
			r = (*sevent_select)(hp,NULL,mask);
		if(r==0)
			return(1);
	}
	errno = unix_err(h_error);
	return(0);
}

/*
 * make the handle inheritable and create an event  if <event> is zero
 */
static int init_socket(int fd,  HANDLE sock, HANDLE event)
{
	HANDLE hp=sock;
	Pfd_t *fdp;
	static int (PASCAL *sevent_select)(HANDLE, HANDLE, long);
	if(!event)
	{
		if(!(event = CreateEvent(sattr(1), TRUE, FALSE, NULL)))
			return(0);
	}
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		HANDLE me = GetCurrentProcess();
		if(!DuplicateHandle(me,sock,me,&hp,0,TRUE, DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			return(0);
	}
	else if(!SetHandleInformation(hp, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
		return(0);

	(fdp=getfdp(P_CP,fd))->socknetevents = 0;
	/*
	 * sock_mapevents calls WSAEventSelect, on success enables non-blocking
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if (sock_mapevents(hp,event,1) == 1)
		fdp->oflag |= O_NONBLOCK;
	Phandle(fd) = hp;
	Xhandle(fd) = event;
	sethandle(fdp,fd);
	if (fdp->type != FDTYPE_CONNECT_UNIX)
		setsocket(fd);
	return(1);
}

/*
 * this is the server call that creates the FDTYPE_CONNECT_UNIX socket
 * it uses an event for synchronization and a pipe for use by accept
 */
static int afunix_bind(int fd, char* path)
{
	HANDLE			hpin,hpout,hp,init_proc,me=GetCurrentProcess();
	char			ename[UWIN_RESOURCE_MAX];
	int			r,initpid;
	Pfd_t 			*fdp;

	if (!path[0]) 
		return 0;

	if(!CreatePipe(&hpin, &hpout, sattr(0), 512))
	{
		errno = unix_err(GetLastError());
		return(-1);
	}
	/* make hpin inheritable */
	if(Share->Platform!=VER_PLATFORM_WIN32_NT)
	{
		if(!DuplicateHandle(me,hpin,me,&hpin,0,TRUE, DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			logerr(0, "DuplicateHandle");
	}
	else if(!SetHandleInformation(hpin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
		logerr(0, "SetHandleInformation");
	initpid = proc_ptr(Share->init_slot)->ntpid;
	if(!(init_proc=open_proc(initpid,0)))
		initpid = P_CP->ntpid;
	else
	{
		if (DuplicateHandle(me,hpout,init_proc,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
			hpout = hp;
		closehandle(init_proc,HT_PROC);
	}
	UWIN_EVENT_SOCK(ename, initpid, hpout);
	if(!(Xhandle(fd) = CreateEvent(sattr(1),TRUE,FALSE,ename)))
		goto err;
    if (path[0]) 
	if((r=mksocket(path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, initpid, hpout))< 0)
	{
		/* If mksocket fails, the socket may already exist */
		closehandle(Xhandle(fd),HT_EVENT);
		goto err;
	}
	fdp=getfdp(P_CP,fd);
	if(fdp->index>0 && fdp->index < Share->nblocks)
	{
		logmsg(0, "afunix_bind fdp=%d block allocated=%d", file_slot(fdp), fdp->index);
		block_free(fdp->index);
	}
	if (r)
		fdp->index = r;
	else if (fdp->index = block_alloc(BLK_FILE))
		strcpy(block_ptr(fdp->index), path);
	Phandle(fd) = hpin;
	return(0);
err:
	closehandle(hpin,HT_PIPE);
	closehandle(hpout,HT_PIPE);
	return(-1);
}

static int afunix_name(int slot, struct sockaddr_un *addr, int *addrlen)
{
	int i,n;
	if (addr && addrlen && *addrlen >= (i = (int)((char*)&addr->sun_path[0] - (char*)addr)))
	{
		addr->sun_family = AF_UNIX;
		if (*addrlen > i)
		{
			n = (int)strlen(fdname(slot)) + 1;
			if (n > (*addrlen - i))
				n = *addrlen - i;
			memcpy(addr->sun_path,fdname(slot), n);
			addr->sun_path[n - 1] = 0;
			*addrlen = (int)((char*)&addr->sun_path[n] - (char*)addr);
			return(1);
		}
		else
			return(0);
	}
	return(0);
}

/*
 * called by the server to accept new connections on FDTYPE_CONNECT_UNIX
 * AF_UNIX sockets.  It uses two anonymous pipes, one created by
 *  sock_open() the other created here.  It interchanges the write handles
 */
static HANDLE afunix_accept(int fd, struct sockaddr_un *addr, int *addrlen, int nfd)
{
	HANDLE			objects[2], event, pp, hp=0, hpin=0, hpout=0;
	HANDLE			me=GetCurrentProcess();
	DWORD			buff[3];
	int			slot,n,sigsys;
	Pfd_t			*fdp = getfdp(P_CP,nfd);
	char			ename[UWIN_RESOURCE_MAX];

	objects[0] = P_CP->sigevent;
	objects[1] = Xhandle(fd);
	sigsys = sigdefer(1);
	while((n=WaitForMultipleObjects(2,objects,FALSE,INFINITE))==WAIT_OBJECT_0)
	{
		if(sigcheck(sigsys))
		{
			sigdefer(1);
			continue;
		}
		errno = EINTR;
		goto err;
	}
	if(!CreatePipe(&hpin, &hpout, sattr(1), PIPE_BUF+24))
	{
		logerr(0, "CreatePipe");
		errno = unix_err(GetLastError());
		goto err;
	}
	if(!ReadFile(Phandle(fd),(void*)buff,sizeof(buff),&n,NULL))
	{
		logerr(0, "ReadFile");
		errno = unix_err(GetLastError());
		goto err;
	}
	if(n!=sizeof(buff))
	{
		errno = ECONNREFUSED;
		goto err;
	}
	UWIN_EVENT_SOCK_CONNECT(ename, buff);
	slot = buff[0];
	if(!(pp = open_proc(buff[2],PROCESS_ALL_ACCESS)) ||
	    !DuplicateHandle(pp,(HANDLE)buff[1],me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
	{
		logerr(0, "DuplicateHandle");
		errno = ECONNREFUSED;
		goto err;
	}
	closehandle(pp,HT_PROC);
	buff[0] = file_slot(fdp);
	buff[1] = (DWORD)hpout;
	buff[2] = P_CP->ntpid;
	if(!WriteFile(hp,(void*)buff,sizeof(buff),&n,NULL))
	{
		logerr(0, "WriteFile");
		errno = unix_err(GetLastError());
		goto err;
	}
	if(!ResetEvent(objects[1]))
		logerr(0, "ResetEvent");
	if(event=OpenEvent(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,ename))
	{
		if(!SetEvent(event))
			logerr(0, "SetEvent");
		closehandle(event,HT_EVENT);
	}
	else
		logerr(0, "OpenEvent %s failed", ename);
	afunix_name(slot,addr,addrlen);
	sigcheck(sigsys);
	fdp->type = FDTYPE_EPIPE;
	fdp->sigio = slot+2;
	fdp->socknetevents = FD_WRITE;
	Xhandle(nfd) = hp;
	Phandle(nfd) = hpin;
	return(hpin);
err:
	if(hpin)
		closehandle(hpin,HT_PIPE);
	if(hpout)
		closehandle(hpout,HT_PIPE);
	if(hp)
		closehandle(hp,HT_EVENT);
	sigcheck(sigsys);
	return(0);
}

/*
 * This opens the socket path, creates an anonymous pipe, and
 * then signals the server to start exchanging write handles
 */
static int afunix_connect(int fd, char* pathname)
{
	Pfd_t		*fdp = getfdp(P_CP,fd);
	HANDLE		pp=0, hp=0, extra=0, objects[2],hpout;
	HANDLE		me=GetCurrentProcess();
	DWORD		buff[3];
	Path_t		info;
	int		r,sigsys,flags = P_FILE|P_EXIST|P_NOHANDLE;

	info.oflags = GENERIC_READ;
	if (fdp->index > 0 && fdp->index < Share->nblocks)
	{
		logmsg(0, "afunix_connect fdp=%d block allocated=%d", file_slot(fdp), fdp->index);
		block_free(fdp->index);
	}
	if(fdp->index = block_alloc(BLK_FILE))
	{
		info.path = fdname(file_slot(fdp));
		flags |= P_MYBUF;
	}
	if(!pathreal(pathname,flags,&info))
		return(-1);
	if(!S_ISSOCK(info.mode))
	{
		errno = ENOTSOCK;
		return(-1);
	}
	if(!(hp = sock_open(fdp,&info, flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH, &extra)))
		return(-1);
	Phandle(fd) = hp;
#if 0
	/*
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if(fdp->oflag&(O_NONBLOCK|O_NDELAY))
	{
		Xhandle(fd) = extra;
		errno = EINPROGRESS;
		return(-1);
	}
#endif
	objects[0] = P_CP->sigevent;
	objects[1] = extra;
	sigsys = sigdefer(1);
	while(WaitForMultipleObjects(2,objects,FALSE,INFINITE)==WAIT_OBJECT_0)
	{
		if(sigcheck(sigsys))
		{
			sigdefer(1);
			continue;
		}
		errno = EINTR;
		goto err;
	}
	closehandle(extra,HT_EVENT);
	extra = 0;
	if(!ReadFile(hp,(void*)buff,sizeof(buff),&r,NULL))
	{
		logerr(0, "afunix_connect ReadFile");
		errno = unix_err(GetLastError());
		goto err;
	}
	if(r!=sizeof(buff))
	{
		errno = ECONNREFUSED;
		goto err;
	}
	if(!(pp = open_proc(buff[2],PROCESS_ALL_ACCESS)) ||
	    !DuplicateHandle(pp,(HANDLE)buff[1],me,&hpout,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
	{
		logerr(0, "afunix_connect DuplicateHandle");
		errno = ECONNREFUSED;
		goto err;
	}
	closehandle(pp,HT_PROC);
	fdp->type = FDTYPE_EPIPE;
	Xhandle(fd) = hpout;
	fdp->sigio = (unsigned short)(buff[0]+2);
	fdp->socknetevents = FD_WRITE;
	return(0);
err:
	if(pp)
		closehandle(pp,HT_PROC);
	if(hp)
	{
		closehandle(hp,HT_PIPE);
		Phandle(fd) = _INVALID_SOCKET;
	}
	if(extra)
		closehandle(extra,HT_EVENT);
	return(-1);
}

int accept(int fd, struct sockaddr *addr, int *addrlen)
{
	static HANDLE (PASCAL *saccept)(HANDLE,struct sockaddr*,int*);
	HANDLE hp,hp1=_INVALID_SOCKET;
	int nfd, fdi;
	long events, wsa_error=-1;
	int errorCodes[FD_MAX_EVENTS+1];

	if(!(hp = gethandle(fd)))
		return(-1);
	if(is_socket(fd, FDTYPE_SOCKET))
	{
		errno = EISCONN;
		return(-1);
	}
	else if(is_socket(fd, FDTYPE_DGRAM))
	{
		errno = EOPNOTSUPP;
		return(-1);
	}
	if(addr && IsBadReadPtr((void*)addr,sizeof(char*)))
	{
		errno = EINVAL;
		return(-1);
	}
	if(addr && addrlen && IsBadReadPtr((void*)addrlen,sizeof(char*)))
	{
		errno = EINVAL;
		return(-1);
	}
	if((nfd = get_fd (0)) == -1)
	{
		errno = EBADF;
		return(-1);
	}
	if((fdi = fdfindslot ()) == -1)
	{
		errno = EBADF;
		return(-1);
	}
	setfdslot(P_CP,nfd,fdi);
	Pfdtab[fdi].oflag = O_RDWR;
	if(getfdp(P_CP,fd)->type == FDTYPE_CONNECT_UNIX)
	{
		if(afunix_accept(fd,(struct sockaddr_un*)addr,addrlen,nfd))
		{
			addr->sa_family = AF_UNIX;
			return(nfd);
		}
		fdpclose(&Pfdtab[fdi]);
		setfdslot(P_CP,nfd,-1);
		return(-1);
	}
	Pfdtab[fdi].type = FDTYPE_SOCKET;
	Xhandle(nfd) = 0;
	if(!sinit || !saccept)
		saccept = (HANDLE (PASCAL*)(HANDLE,struct sockaddr*,int*))getaddr("accept");
	if(saccept)
	{
		events = getnetevents(fd,errorCodes,1,FD_ACCEPT,1);
		if(!(events&FD_ACCEPT))
		{
			HANDLE objects[2];
			DWORD waitmessage;
#if 0
			/*
			 * WINDOWS has all sockets in Non blocking mode has default,
			 * because of WSAEventSelect(). NOT user changable.
			 */
			if(getfdp(P_CP,fd)->oflag&(O_NONBLOCK|O_NDELAY))
			{
				errno = EWOULDBLOCK;
				goto err;
			}
#endif
			objects[0] = P_CP->sigevent;
			objects[1] = Xhandle(fd);
			sigdefer(1);
		again:
			waitmessage=WaitForMultipleObjects(2,objects,FALSE,INFINITE);
			if(waitmessage==WAIT_OBJECT_0+1)
			{
				events = getnetevents(fd, errorCodes,1,FD_ACCEPT,2);
				if(!(events&FD_ACCEPT))
					goto again;
				if(errorCodes[0] && errorCodes[FD_ACCEPT_BIT])
				{
					errno = unix_err(wsa_error = h_error);
					goto err;
				}
			}
			else if ( waitmessage==WAIT_OBJECT_0 ) /* Signal */
			{
				logmsg(0, "accept() -- WAIT_OBJECT_0");
				if(sigcheck(0))
				{
					sigdefer(1);
					goto again;
				}
				errno = EINTR;
				goto err;
			}
			else if(waitmessage == 0xFFFFFFFF)
			{
				SetLastError(wsa_error = h_error);
				logerr(0, "WaitForMultipleObjects() failed in accept()");
				errno = unix_err(wsa_error);
				goto err;
			}
			sigcheck(0);
		}
		if(state.clone.hp)
			hp1 = cl_sockops(SOP_ACCEPT,saccept,hp,addr,addrlen);
		else
			hp1=(*saccept)(hp,addr,addrlen);
	}
	else
	{
		wsa_error = h_error;
		errno = EOPNOTSUPP;
		goto err;
	}
	if(hp1 && hp1 != _INVALID_SOCKET)
	{
		htab_socket(hp1,0,HTAB_CREATE,"accept");
		if(init_socket(nfd,hp1,NULL))
		{
			Pfd_t *fdp = getfdp(P_CP,nfd);
			sock_to_init(fdp,hp1);
			/* Set same blocking state as listening socket. */
			if(getfdp(P_CP, fd)->oflag & O_NONBLOCK)
				fdp->oflag |= O_NONBLOCK;
			if(getfdp(P_CP, fd)->oflag & O_NDELAY)
				fdp->oflag |= O_NDELAY;
			logmsg(LOG_DEV+4, "accept() normal return");
			return(nfd);
		}
	}
	SetLastError(wsa_error = h_error);
	errno = unix_err(wsa_error);
err:
	fdpclose(&Pfdtab[fdi]);
	setfdslot(P_CP,nfd,-1);
	logerr(0, "accept()  ERROR return fd=%d errno=%d wsa_error=%d", nfd, errno, wsa_error);
	return(-1);
}

int bind(int fd, struct sockaddr *addr, int addrlen)
{
	HANDLE hp;
	int r= -1;

	if(!(hp = gethandle(fd)))
		return(-1);
	if (hp == _INVALID_SOCKET)
	{
		if (addr->sa_family==AF_UNIX)
			return(afunix_bind(fd, ((struct sockaddr_un*)addr)->sun_path));
		errno = EAFNOSUPPORT;
		return(-1);
	}
	if(is_socket(fd, FDTYPE_SOCKET))
	{
		errno = EISCONN;
		return(-1);
	}
	if(IsBadReadPtr((void*)addr,addrlen))
	{
		errno = EINVAL;
		return(-1);
	}
#if 1
	if(state.clone.hp)
		return(cl_bind2(fdslot(P_CP,fd),addr,addrlen));
#endif
	sigdefer(1);
	if(!sinit || !sbind)
		sbind =  (int (PASCAL*)(HANDLE,struct sockaddr*,int))getaddr("bind");
	addr->sa_len = 0;
	if(!sinit || sbind)
	{
		if(state.clone.hp)
			r = (int)cl_sockops(SOP_BIND,sbind,hp,addr,&addrlen);
		else
			r = (*sbind)(hp,addr,addrlen);
	}
	if(r != 0)
	{
		if(h_error==WSAEFAULT)
			errno = EINVAL;
		else
			errno = unix_err(h_error);
		/* errno = EOPNOTSUPP;*/
	}
	/* If unix domain save bind name */
	if (getfdp(P_CP,fd)->type == FDTYPE_CONNECT_UNIX)
	{
		Pfd_t *fdp = getfdp(P_CP,fd);
		fdp->index = block_alloc(BLK_FILE);
		if (fdp->index)
		{
			memcpy(block_ptr(fdp->index), addr, addrlen);
			((char *)block_ptr(fdp->index))[addrlen+1] = (char)NULL;
		}
	}
	sigcheck(0);
	return(r);
}


static int sockclose(int fd, Pfd_t* fdp, int noclose)
{
	static int (PASCAL *seventclose)(HANDLE);
	register int r=0;
	HANDLE hp;
	if(fdp->refcount==0 && (fdp->oflag&O_CDELETE))
		sock_done(fdp);
	if(noclose>0)
		return(0);
	if(!sinit || !sclose || !seventclose)
	{
		sclose =  (int (PASCAL*)(HANDLE))getaddr("closesocket");
		seventclose =  (int (PASCAL*)(HANDLE))getaddr("WSACloseEvent");
	}
	if(!sinit || !sshutdown)
		sshutdown =  (int (PASCAL*)(HANDLE,int))getaddr("shutdown");

	hp =  Phandle(fd);
#if 1
	if(state.clone.hp && issocket(fd) && fdp->refcount==0 && sclose)
	{
		int len = (int)(fdp-Pfdtab);
		cl_sockops(SOP_CLOSE, sclose, NULL, NULL, &len);
	}
#endif
	if(fdp->refcount==0)
	{
		if(fdp->sigio)
			proc_release(proc_ptr(fdp->sigio));
		if(!(fdp->oflag & (O_NDELAY|O_NONBLOCK)))
		{
			if(sock_mapevents(hp, Xhandle(fd),0))
			{
				int disable=0;
				if(ioctlsocket(hp, FIONBIO, &disable) != 0)
				{
					SetLastError(h_error);
					logerr(LOG_DEV+2, "ioctlsocket");
					errno = unix_err(h_error);
				}
			}
			else
				logerr(LOG_DEV+2, "sock_mapevents fd=%d type=%(fdtype)s", fd, fdp->type);
		}
		(*seventclose)(Xhandle(fd));
		if((*sclose)(hp)==SOCKET_ERROR)
		{
			SetLastError(h_error);
			logerr(LOG_DEV+1, "closesocket fd=%d type=%(fdtype)s hp=%p", fd, fdp->type, hp);
		}
		htab_socket(hp,0,HTAB_CLOSE,"sockclose");
	}
	else
	{
		if(issocket(fd))
		{
			if((*sclose)(hp)==SOCKET_ERROR)
			{
				SetLastError(h_error);
				logerr(LOG_DEV+1, "closesocket fd=%d type=%(fdtype)s hp=%p", fd, fdp->type, hp);
			}
		}
		else if(!closehandle(hp,HT_SOCKET))
			logerr(LOG_DEV+1, "closehandle fd=%d type=%(fdtype)s hp=%p", fd, fdp->type, hp);
		if(Xhandle(fd) && !closehandle(Xhandle(fd),HT_EVENT))
			logerr(LOG_DEV+1, "closehandle fd=%d type=%(fdtype)s xhp=%p", fd, fdp->type, Xhandle(fd));
	}
	return(0);
}

int connect(int fd, struct sockaddr *addr, int addrlen)
{
	HANDLE hp;
	int r= -1;
	long events=0;
	Pfd_t *fdp;

	logmsg(LOG_DEV+3, "connect() enter");
	if(!(hp = gethandle(fd)))
		return(-1);
	if(is_socket(fd, FDTYPE_SOCKET))
	{
		errno = EISCONN;
		return(-1);
	}
	if(addrlen<=0 || IsBadReadPtr(addr, addrlen))
	{
		errno=EINVAL;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(!issocket(fd))
	{
		errno = (fdp->type==FDTYPE_SOCKET?EISCONN:ENOTSOCK);
		return(-1);
	}
	/* Check address family with address family when created */
	if(addr->sa_family == AF_UNIX)
	{
		if(fdp->type != FDTYPE_CONNECT_UNIX)
		{
			errno = EAFNOSUPPORT;
			return(-1);
		}
	}
	else
	{
		addr->sa_len = 0;
		if(addr->sa_family == AF_INET)
		{
			if(fdp->type!=FDTYPE_CONNECT_INET && fdp->type!=FDTYPE_DGRAM)
			{
				errno = EAFNOSUPPORT;
				return(-1);
			}
		}
	}
	if (hp == _INVALID_SOCKET)
	{

		if(addr->sa_family==AF_UNIX)
			return(afunix_connect(fd, ((struct sockaddr_un*)addr)->sun_path));
		errno = EAFNOSUPPORT;
		return(-1);
	}
	if(!sinit || !sconnect)
		sconnect = (int (PASCAL*)(HANDLE, struct sockaddr*,int))getaddr("connect");
	if(sconnect)
	{
		HANDLE        objects[2];
		int r,err,errorCodes[FD_MAX_EVENTS+1];
		objects[0] = P_CP->sigevent;
		objects[1] = Xhandle(fd);
		if(state.clone.hp)
			r = (int)cl_sockops(SOP_CONNECT,sconnect,hp, addr, &addrlen);
		else
			r = (*sconnect)(hp, addr, addrlen);
		if(r==0)
		{
			if(fdp->type == FDTYPE_CONNECT_INET)
				fdp->type = FDTYPE_SOCKET;
			getnetevents(fd,errorCodes,0,FD_CONNECT,18);
			return(0);
		}
		if(h_error!=WSAEWOULDBLOCK)
		{
			errno = unix_err(h_error);
			return(-1);
		}
#if 0
		/*
		 * WINDOWS has all sockets in Non blocking mode has default,
		 * because of WSAEventSelect(). NOT user changable.
		 *
		 * This causes ssh outgoing connections to fail. Previously
		 * wasn't a problem, since sockets didn't set oflag values
		 */
		if(fdp->oflag&(O_NONBLOCK|O_NDELAY))
		{
			errno = EINPROGRESS;
			return(-1);
		}
#endif
		sigdefer(1);
		while(1)
		{
			r = WaitForMultipleObjects(2, objects, FALSE,10000);
			if(r==WAIT_OBJECT_0+1)
			{
				if(!getnetevents(fd,errorCodes,1,FD_CONNECT,17))
					continue;
				if(errorCodes[0] && (err=errorCodes[FD_CONNECT_BIT]))
				{
					errno = unix_err(err);
					r = -1;
				}
				else
				{
					if(fdp->type == FDTYPE_CONNECT_INET)
						fdp->type = FDTYPE_SOCKET;
					r = 0;
					/* without this x11 breaks with sp4 */
					Sleep(1);
				}
			}
			else if(r==WAIT_OBJECT_0)
			{
				if(sigcheck(0))
				{
					sigdefer(1);
					continue;
				}
				errno = EINTR;
				r = -1;
			}
			else if(r==WAIT_TIMEOUT)
			{
				errno = ETIMEDOUT;
				r = -1;
			}
			else
			{
				SetLastError(h_error);
				logerr(0, "WaitForMultipleObjects() failed in connect()");
				errno = unix_err(h_error);
				r = -1;
			}
			break;
		}
		sigcheck(0);
		return(r);
	}
	errno = EOPNOTSUPP;
	return(-1);
}

#ifndef WSA_NOT_ENOUGH_MEMORY
#define WSA_NOT_ENOUGH_MEMORY	(ERROR_NOT_ENOUGH_MEMORY)
#endif /* ! WSA_NOT_ENOUGH_MEMORY */
#ifndef WSAEINVAL
#ifndef WSABASEERR
#  define WSABASEERR		10000
#endif /* !WSABASEERR */
#define WSAEINVAL		(WSABASEERR+22)
#define WSAESOCKTNOSUPPORT	(WSABASEERR+44)
#define WSAEAFNOSUPPORT		(WSABASEERR+47)
#define WSATYPE_NOT_FOUND	(WSABASEERR+109)
#define WSAHOST_NOT_FOUND	(WSABASERR+1001)
#define WSATRY_AGAIN		(WSABASERR+1002)
#define WSANO_RECOVERY		(WSABASEERR+1003)
#define WSANO_DATA		(WSABASEERR+1004)
#endif /* !WSAEINVAL */
struct err_map {
	int eai_err;
	int win_err;
	char *eai_str;
} err_map[] = {
	{ EAI_AGAIN, WSATRY_AGAIN, "temporary failure in name resolution" },
	{ EAI_BADFLAGS, WSAEINVAL , "invalid flags or parameters" },
	{ EAI_FAIL, WSANO_RECOVERY, "non-recoverable failure in name resolution" },
	{ EAI_FAMILY, WSAEAFNOSUPPORT, "family not supported" },
	{ EAI_MEMORY, WSA_NOT_ENOUGH_MEMORY, "memory allocation failed" },
	{ EAI_NONAME, WSAHOST_NOT_FOUND, "nodename/servname not found" },
	{ EAI_SERVICE, WSATYPE_NOT_FOUND, "servname not supported for socktype" },
	{ EAI_SOCKTYPE, WSAESOCKTNOSUPPORT, "requested socktype not supported" },
	{ EAI_NODATA, 0, "No data available" },
	{ EAI_ADDRFAMILY, 0 , "Address family for nodename not supported"},
	{ EAI_SYSTEM, 0, "System error" },
	{ EAI_BADHINTS, 0, "Invalid hints parameter" },
	{ EAI_OVERFLOW, 0, "data overflow" },
	{ EAI_PROTOCOL, 0, "resolved protocol, unknown" },
};
int err_sz = (sizeof(err_map) / sizeof(struct err_map));


static map_wsaerr(int wsa_err)
{
	int i;

	for (i=0; i < err_sz; i++)
	{
		if (err_map[i].win_err == wsa_err)
			return err_map[i].eai_err;
	}
	return EAI_PROTOCOL;
}

char *gai_strerror(int ecode)
{
	static init_strerror = 0;
	if (!init_strerror && !sgai_strerror)
	{
		sgai_strerror = (char *(PASCAL *)(int))getaddr("gai_strerror");
		init_strerror = 1;
	}
	if (sgai_strerror)
		return (*sgai_strerror)(ecode);
	else
	{
		int i;
		for (i=0; i < err_sz; i++)
		{
			if (err_map[i].eai_err == ecode)
				return err_map[i].eai_str;
		}
		return "unknown error code";
	}
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	static init_nameinfo = 0;
	int wsaapi;

	if (!init_nameinfo && !sgetnameinfo)
	{
		sgetnameinfo = (int (PASCAL *)(const struct sockaddr *, socklen_t, char *, DWORD, char *, DWORD, int))getaddr("getnameinfo");
		init_nameinfo = 1;
	}
	if (sgetnameinfo)
	{
		if(state.clone.hp)
			wsaapi = cl_getsockinfo(*sgetnameinfo,sa, salen, host, (int)hostlen, serv, (int)servlen, flags, NULL);
		else
			wsaapi = (*sgetnameinfo)(sa, salen, host, (int)hostlen, serv, (int)servlen, flags);
		if (wsaapi)
			return map_wsaerr(wsaapi);
		return 0;
	}
	errno = ENOSYS;
	return -1;
}

static int
res_size(struct addrinfo *list)
{
	int size=0;
	while (list)
	{
		size += sizeof(struct addrinfo);
		if (list->ai_canonname)
		{
			int len = (int)strlen(list->ai_canonname) + 1;
			if (len & 0x3)
				len += 4 - (len&0x3);
			size += len;
		}
		if (list->ai_addr)
			size += sizeof(struct sockaddr);
		list = list->ai_next;
	}
	return size;
}


static void
copy_res(struct addrinfo *res, void *copybuf, int bufsize)
{
	unsigned char *cp;
	struct addrinfo *list;

	list = (struct addrinfo *) copybuf;
	cp = (unsigned char *)copybuf;
	while (res)
	{
		*list = *res;
		cp += sizeof(*list);
		if (res->ai_canonname)
		{
			list->ai_canonname = cp;
			strcpy(list->ai_canonname, res->ai_canonname);
			cp += (int)strlen(res->ai_canonname) + 1;
			if ((int)cp & 0x3)
				cp += 4 - ((int)cp&0x3);
		}
		if (res->ai_addr)
		{
			list->ai_addr = (struct sockaddr *)cp;
			*(list->ai_addr) = *(res->ai_addr);
			cp += sizeof(struct sockaddr);
		}
		if (res->ai_next)
			list->ai_next = (struct addrinfo *) cp;
		res = res->ai_next;
		list = list->ai_next;
	}
}

static int init_addrinfo = 0;

int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	int wsaapi;

	if (!init_addrinfo && (!sgetaddrinfo || !sfreeaddrinfo))
	{
		sgetaddrinfo = (int (PASCAL*)(const char *, const char *,const struct addrinfo *, struct addrinfo **))getaddr("getaddrinfo");
		sfreeaddrinfo = (int (PASCAL*)(const struct addrinfo *))getaddr("freeaddrinfo");
		init_addrinfo = 1;
	}
	if (sgetaddrinfo)
	{
		int size;
		void *res_copy;
		if(state.clone.hp)
		{
			if(wsaapi = cl_getsockinfo(sgetaddrinfo,nodename,0,(char*)servname,0,(char*)hints,0,0,res))
				return map_wsaerr(wsaapi);
			return(0);
		}
		else
			wsaapi = (*sgetaddrinfo)(nodename, servname, hints, res);
		size = res_size(*res);
		res_copy = malloc(size);
		if (res_copy)
		{
			copy_res(*res, res_copy, size);
			if (sfreeaddrinfo)
				(*sfreeaddrinfo)(*res);
			*res = res_copy;
			if (wsaapi)
				return map_wsaerr(wsaapi);
			return 0;
		}
		else
		{
			errno = ENOSPC;
			return EAI_SYSTEM;
		}
	}
	else
	{
		errno = ENOSYS;
		return -1;
	}
}

void freeaddrinfo(struct addrinfo *ai)
{
	free(ai);
}

unsigned int if_nametoindex(const char *name)
{
	static init_ifnametoindex = 0;

	if (!sif_nametoindex && !init_ifnametoindex)
	{
		sif_nametoindex = (unsigned int (PASCAL *)(const char *))getaddr("if_nametoindex");
		init_ifnametoindex = 1;
	}
	if (sif_nametoindex)
		/* no error code information available */
		return (*sif_nametoindex)(name);
	else
	{
		errno = ENOSYS;
		return 0;
	}
}

char *if_indextoname(unsigned long ifindex, char *ifname)
{
	static init_indextoname = 0;

	if (!init_indextoname && !sif_indextoname)
	{
		sif_indextoname = (char * (PASCAL *)(unsigned int, char *))getaddr("if_indextoname");
		init_indextoname = 1;
	}
	if (sif_indextoname)
		return (*sif_indextoname)(ifindex, ifname);
	else
	{
		errno = ENOSYS;
		return NULL;
	}
}

static int init_nameindex = 0;
struct if_nameindex *if_nameindex(void)
{
	if (!init_nameindex && (!sif_nameindex || !sif_freenameindex))
	{
		sif_nameindex = (struct if_nameindex * (PASCAL*)(void))getaddr("if_nameindex");
		sif_freenameindex = (void (PASCAL*)(struct if_nameindex *))getaddr("if_freenameindex");
		init_nameindex = 1;
	}
	if (sif_nameindex)
		return (*sif_nameindex)();
	else
	{
		errno = ENOSYS;
		return NULL;
	}
}

void if_freenameindex(struct if_nameindex *n)
{
	if (!init_nameindex && (!sif_nameindex || !sif_freenameindex))
	{
		sif_nameindex = (struct if_nameindex * (PASCAL*)(void))getaddr("if_nameindex");
		sif_freenameindex = (void (PASCAL*)(struct if_nameindex *))getaddr("if_freenameindex");
		init_nameindex = 1;
	}
	if (sif_freenameindex)
		(*sif_freenameindex)(n);
}

int getpeername(int fd, struct sockaddr *addr, int *addrlen)
{
	static int (PASCAL *sgetpeername)(HANDLE,struct sockaddr*,int*);
	Pfd_t *fdp;
	HANDLE hp;
	int r= -1;
	if(!(hp = gethandle(fd)))
		return(-1);
	fdp=getfdp(P_CP,fd);
	if(fdp->type==FDTYPE_CONNECT_INET || fdp->type==FDTYPE_DGRAM)
	{
		errno = ENOTCONN;		/* getpeername() cannot operate on a non-connected socket. */
		return(-1);
	}
	if(IsBadWritePtr((void*)addrlen,sizeof(int*)) || IsBadWritePtr((void*)addr,*addrlen))
	{
		errno = EFAULT;
		return(-1);
	}
	addr->sa_len = 0;
	if(fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE)
	{
		if(afunix_name(file_slot(fdp),(struct sockaddr_un*)addr,addrlen))
		{
			addr->sa_family = AF_UNIX;
			return(0);
		}
		errno = ENOTSOCK;
		return(-1);
	}
	if(!sinit || !sgetpeername)
		sgetpeername = (int (PASCAL*)(HANDLE,struct sockaddr*,int*))getaddr("getpeername");

	if(sgetpeername)
		r = (*sgetpeername)(hp,addr,addrlen);
	if (r != 0 )
		errno = unix_err(h_error);
	return(r);
}


int getsockname(int fd, struct sockaddr *addr, int *addrlen)
{
	HANDLE hp = gethandle(fd);	/* socket only needs to be bound. */
	int r= -1;
	Pfd_t *fdp;

	if(!hp)
		return(-1);
	if(IsBadWritePtr((void*)addr,sizeof(struct sockaddr)) || IsBadWritePtr((void*)addrlen,sizeof(int)))
	{
		errno = EFAULT;
		return(-1);
	}
	addr->sa_len = 0;
	fdp=getfdp(P_CP,fd);
	if(fdp->type==FDTYPE_CONNECT_UNIX || fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE)
	{
		if(afunix_name(file_slot(fdp),(struct sockaddr_un*)addr,addrlen))
		{
			addr->sa_family = AF_UNIX;
			return(0);
		}
		errno = ENOBUFS;
		return(-1);
	}
	if(!sinit || !sgetsockname)
		sgetsockname = (int (PASCAL*)(HANDLE,struct sockaddr*,int*))getaddr("getsockname");

	if(sgetsockname)
	{
		if(state.clone.hp)
			r = (int)cl_sockops(SOP_NAME,sgetsockname,hp,addr,addrlen);
		else
			r = (*sgetsockname)(hp,addr,addrlen);
	}
	if (r != 0 )
	{
		if(h_error == WSAEINVAL)
		{
			ZeroMemory(addr, *addrlen);
			*addrlen = sizeof(struct sockaddr);
			r = 0;
		}
		else
		{
			errno = unix_err(h_error);
			logerr(0, "getsockname r=%d",r);
		}
	}
	addr->sa_family = AF_INET;
	return(r);
}


char *inet_ntoa(struct in_addr in)
{
	static char *(PASCAL *sinet_ntoa)(struct in_addr);
	static char cp[20];
	char *nt_cp;
	if(!sinit || !sinet_ntoa)
		sinet_ntoa = (char *(PASCAL*)(struct in_addr))getaddr("inet_ntoa");
	if(sinet_ntoa)
	{
		if (!cp)
		{
			errno = ENOMEM;
			return (NULL);
		}
		nt_cp = (*sinet_ntoa)(in);
		if (nt_cp )
			memcpy(cp, nt_cp, strlen(nt_cp)+1);
		else
		{
			errno = unix_err(h_error);
			return(NULL);
		}
	}
	return(cp);
}

unsigned long inet_lnaof(struct in_addr in)
{
	unsigned long i = (in.S_un.S_un_b.s_b3<<8) + in.S_un.S_un_b.s_b4;
	if(in.S_un.S_un_b.s_b1 < 128)
		return((in.S_un.S_un_b.s_b2<<16) + i);
	return(i);
}

unsigned long inet_netof(struct in_addr in)
{
	if(in.S_un.S_un_b.s_b1 < 128)
		return(in.S_un.S_un_b.s_b1);
	return((in.S_un.S_un_b.s_b1<<8) + in.S_un.S_un_b.s_b2);
}

unsigned long  inet_addr(const char *cp )
{
	static unsigned long (PASCAL *sinet_addr)(const char *);
	unsigned long r = 0;
	if(!sinit || !sinet_addr)
		sinet_addr =  (unsigned long (PASCAL*)(const char *))getaddr("inet_addr");
	if(sinet_addr)
		r = (*sinet_addr)(cp);
	return(r);
}

int inet_aton(const char *cp, struct in_addr *inp)
{
	unsigned long l = inet_addr(cp);
	int n;
	if(l==INADDR_NONE && ((n=strtol(cp,(char**)&cp,0))!=255 || *cp!='.'))
		return(0);
	inp->S_un.S_addr = l;
	return(1);
}


/* Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * For functions inet_pton4, inet_pton6, inet_ntop4, inet_ntop6
 *
 */
/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, u_char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	u_char tmp[INET_ADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != 0) {
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL) {
			u_int new = *tp * 10 + (int)(pch - digits);

			if (new > 255)
				return (0);
			if (! saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
			*tp = new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);

	memcpy(dst, tmp, INET_ADDRSZ);
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, u_char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
			  xdigits_u[] = "0123456789ABCDEF";
	u_char tmp[INET6_ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	int ch, saw_xdigit, count_xdigit;
	u_int val;

	memset((tp = tmp), 0, INET6_ADDRSZ);
	endp = tp + INET6_ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = count_xdigit = 0;
	val = 0;
	while ((ch = *src++) != 0) {
		const char *pch;

		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			if (count_xdigit >= 4)
				return (0);
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return (0);
			saw_xdigit = 1;
			count_xdigit++;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return (0);
				colonp = tp;
				continue;
			} else if (*src == 0) {
				return (0);
			}
			if (tp + sizeof(int16_t) > endp)
				return (0);
			*tp++ = (u_char) (val >> 8) & 0xff;
			*tp++ = (u_char) val & 0xff;
			saw_xdigit = 0;
			count_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + INET_ADDRSZ) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += INET_ADDRSZ;
			saw_xdigit = 0;
			count_xdigit = 0;
			break;	/* 0 was seen by inet_pton4(). */
		}
		return (0);
	}
	if (saw_xdigit) {
		if (tp + sizeof(int16_t) > endp)
			return (0);
		*tp++ = (u_char) (val >> 8) & 0xff;
		*tp++ = (u_char) val & 0xff;
	}
	if (colonp != NULL) {
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = (int)(tp - colonp);
		int i;

		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return (0);
	memcpy(dst, tmp, INET6_ADDRSZ);
	return (1);
}
int inet_pton(int af, const char *src, void *dst)
{
#ifdef USE_WINDOWS_INET_PTON
	static init_pton = 0;
	int wsaapi;
	if(!init_pton && !sinet_pton)
	{
		sinet_pton = (int (PASCAL*)(int, const char *, void *))getaddr("inet_pton");
		init_pton = 1;
	}
	if(sinet_pton)
	{
		wsaapi = (*sinet_pton)(af, src, dst);
		if (wsaapi < 0)
		{
			errno = unix_err(map_wsaerr(GetLastError()));
			return 0;
		}
		return wsaapi;
	}
	else if (init_pton)	/* Not implemented by windows do it ourselves */
#else
	{
		switch (af) {
		case AF_INET:
			return inet_pton4(src, dst);
		case AF_INET6:
			return inet_pton6(src, dst);
		default:
			errno = EAFNOSUPPORT;
			return -1;
		}
	}
#endif /* USE_WINDOWS_INET_PTON */
	errno = ENOSYS;
	return -1;
}

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address, more or less like inet_ntoa()
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a u_char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const u_char *src, char *dst, size_t size)
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];
	ssize_t l;

	l = sfsprintf(tmp, size, fmt, src[0], src[1], src[2], src[3]);
	if (l <= 0 || l >= (int)size) {
		errno = ENOSPC;
		return (NULL);
	}
	strncpy(dst, tmp, size);
	return (dst);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop6(const u_char *src, char *dst, size_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	char *tp, *ep;
	struct { int base, len; } best, cur;
	u_int words[INET6_ADDRSZ / sizeof(int16_t)];
	int i;
	ssize_t advance;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, 0, sizeof words);
	for (i = 0; i < INET6_ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	cur.base = -1;
	for (i = 0; i < (INET6_ADDRSZ / sizeof(int16_t)); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	ep = tmp + sizeof(tmp);
	for (i = 0; i < (INET6_ADDRSZ / sizeof(int16_t)) && tp < ep; i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len)) {
			if (i == best.base) {
				if (tp + 1 >= ep)
					return (NULL);
				*tp++ = ':';
			}
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0) {
			if (tp + 1 >= ep)
				return (NULL);
			*tp++ = ':';
		}
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 &&
		    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!inet_ntop4(src+12, tp, (size_t)(ep - tp)))
				return (NULL);
			tp += (int)strlen(tp);
			break;
		}
		advance = sfsprintf(tp, ep - tp, "%x", words[i]);
		if (advance <= 0 || advance >= ep - tp)
			return (NULL);
		tp += advance;
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (INET6_ADDRSZ / sizeof(int16_t))) {
		if (tp + 1 >= ep)
			return (NULL);
		*tp++ = ':';
	}
	if (tp + 1 >= ep)
		return (NULL);
	*tp++ = 0;

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t)(tp - tmp) > size) {
		errno = ENOSPC;
		return (NULL);
	}
	strncpy(dst, tmp, size);
	return (dst);
}
const char *inet_ntop(int af, const void *src, char *dst, size_t size)
{
#ifdef USE_WINDOWS_INET_NTOP
	static char cp[INET6_ADDRSTRLEN];
	static int init_ntop = 0;
	const char *nt_cp;
	if(!init_ntop || !sinet_ntop)
	{
		sinet_ntop = (const char *(PASCAL*)(int, const void *,char *, size_t))getaddr("inet_ntop");
		init_ntop = 1;
	}
	/* Don't trust windows to set errno correctly */
	if (af != AF_INET && af != AF_INET6)
	{
		errno = EAFNOSUPPORT;
		return NULL;
	}
	if ((size < INET_ADDRSTRLEN) || (af==AF_INET6 && size < INET6_ADDRSTRLEN))
	{
		errno = ENOSPC;
		return NULL;
	}
	if(sinet_ntop)
	{
		if ( !cp )
		{
			errno = ENOMEM;
			return (NULL);
		}
		nt_cp = (*sinet_ntop)(af, src, dst, size);
		if (nt_cp )
		{
			memcpy(cp, nt_cp, strlen(nt_cp)+1);
			if (dst)
				memcpy(dst, nt_cp, strlen(nt_cp)+1);
		}
		else
		{
			errno = unix_err(map_wsaerr(GetLastError()));
			return(NULL);
		}
	}
	else if (init_ntop)	/* do it ourselves */
#else
	{
		switch (af)
		{
		case AF_INET:
			return inet_ntop4(src, dst, size);
		case AF_INET6:
			return inet_ntop6(src, dst, size);
		default:
			errno = EAFNOSUPPORT;
			return NULL;
		}
	}
#endif /* USE_WINDOWS_INET_NTOP */
	errno = ENOSYS;
	return NULL;
}

/*
 * local definition to avoid linking with winsock2 dll, used in net_disp
 */
const char *sock_inet_ntop(int af, const void *src, char *dst, size_t size)
{
	return inet_ntop(af, src, dst, size);
}


struct in_addr inet_makeaddr(u_long net, u_long host)
{
	u_long addr;
	if (net < 128)
		addr = (net << IN_CLASSA_NSHIFT) | (host & IN_CLASSA_HOST);
	else if (net < 65536)
		addr = (net << IN_CLASSB_NSHIFT) | (host & IN_CLASSB_HOST);
	else if (net < 16777216L)
		addr = (net << IN_CLASSC_NSHIFT) | (host & IN_CLASSC_HOST);
	else
		addr = net | host;
	addr = htonl(addr);
	return (*(struct in_addr *)&addr);
}

int listen(int fd, int backlog)
{
	static int (PASCAL *slisten)(HANDLE,int);
	HANDLE hp;
	int r= -1;

	if(state.clone.hp)
		return(cl_socket2(SOP_LISTEN2,0,fdslot(P_CP,fd),backlog));
	if(!(hp = gethandle(fd)))
		return(-1);
	if(is_socket(fd, FDTYPE_SOCKET))
	{
		errno = EISCONN;	/* accept() cannot operate on a connected socket. */
		return(-1);
	}
	if(getfdp(P_CP,fd)->type == FDTYPE_CONNECT_UNIX)
		return 0;
	if(!sinit || !slisten)
		slisten =  (int (PASCAL*)(HANDLE,int))getaddr("listen");
	if(slisten)
	{
		if(state.clone.hp)
			r = (int)cl_sockops(SOP_LISTEN,slisten,hp,NULL,&backlog);
		else
			r = (*slisten)(hp,backlog);
	}
	if (r != 0 )
		errno = unix_err(h_error);
	return(r);
}

static ssize_t common_recv(int fd, char *buf, size_t len, int flags, struct sockaddr *from, int *fromlen)
{
	long events=0;
	int sigsys,r=-1, err, errorCodes[FD_MAX_EVENTS+1];
	int eventFlags=FD_READ|FD_ACCEPT|FD_CLOSE;
	int dont_wait = 0, bioval, opt_flags;
	HANDLE objects[2];
	DWORD timeout = 2;
	Pfd_t *fdp;
	static int (PASCAL *srecvfrom)(HANDLE,char*,int,int,struct sockaddr*,int*);
	if (len==0)
		return(0);
	fdp = getfdp(P_CP,fd);
 	opt_flags = flags;
	logmsg(LOG_DEV+4, "common_recv fd=%d buf=%p len=%d flags=%x from=%p fromlen=%p type=%(fdtype)s", fd, buf, len, flags, from, fromlen, fdp->type);

	/*
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if ((flags & MSG_DONTWAIT) && !(fdp->oflag&O_NONBLOCK))
	{
		dont_wait = 1;
		bioval = 1;
		if(ioctlsocket(Phandle(fd), FIONBIO, &bioval) != 0)
		{
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	/*
	 * POSIX requires support MSG_OOB, MSG_PEEK, and MSG_WAITALL
	 * Linux also supports MSG_CONFIRM, MSG_DONTROUTE, MSG_DONTWAIT,
	 *    MSG_MORE, and MSG_NOSIGNAL.
	 * OpenBSD also supports MSG_DONTROUTE, MSG_DONTWAIT.
	 */
	/* Mask off flags not supported in Windows */
	flags &= (MSG_OOB | MSG_PEEK | MSG_WAITALL);
	if(!sinit)
	{
		srecv = 0;
		srecvfrom = 0;
	}
	if(!from && !srecv && !(srecv=(int (PASCAL*)(HANDLE,char*,int,int))getaddr("recv")))
	{
		logerr(0, "MISSING SOCKET LIBRARY");
		errno = EOPNOTSUPP;
		return(-1);
	}
	if(from && !srecvfrom && !(srecvfrom=(int (PASCAL*)(HANDLE,char*,int,int,struct sockaddr*,int*))getaddr("recvfrom")))
	{
		logerr(0, "MISSING SOCKET LIBRARY");
		errno = EOPNOTSUPP;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(flags&MSG_OOB)
		eventFlags |= FD_OOB;
	events = getnetevents(fd,errorCodes,1,eventFlags,3);
	/*
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if(!events && (fdp->oflag&(O_NONBLOCK|O_NDELAY)))
		events = FD_READ;
	objects[0] = P_CP->sigevent;
	objects[1] = Xhandle(fd);
	sigsys = sigdefer(1);
	while(1)
	{
		if(!events) switch(WaitForMultipleObjects(2, objects, FALSE, timeout))
		{
		    case WAIT_TIMEOUT:
			break;
		    case 0xFFFFFFFF:
			errno = unix_err(h_error);
			goto done;
		    case WAIT_OBJECT_0:
			if(sigcheck(sigsys))
			{
				sigdefer(1);
				continue;
			}
			errno = EINTR;
			goto done;
		    case (WAIT_OBJECT_0 +1):
			if(!(events=getnetevents(fd,errorCodes,1,eventFlags,4)))
				continue;
		}
		/*
		 * If user requested any OOB data via flags, check if there
		 * is OOB data. If not clear the MSG_OOB flag, since windows
		 * doesn't always return when there is no OOB data and the flag
		 * is set.
		 */
		if (flags & MSG_OOB)
		{
			BOOL bool;
			ioctlsocket(Phandle(fd), SIOCATMARK, &bool);
			if (bool == FALSE)
			{
				flags &= ~MSG_OOB;
			}
		}
		if(from)
			r = (*srecvfrom)(Phandle(fd),buf,(int)len,flags,from,fromlen);
		else
			r = (*srecv)(Phandle(fd), buf, (int)len, flags);
		err=GetLastError();
		if(r>=0)
			break;
		if((events&FD_CLOSE) || err==WSAECONNRESET || err==WSAESHUTDOWN)
			r = 0;
		else if(err==WSAEWOULDBLOCK)
		{
			/*
			 * WINDOWS has all sockets in Non blocking mode has default,
			 * because of WSAEventSelect(). NOT user changable.
			 */
			if(!(fdp->oflag&(O_NONBLOCK|O_NDELAY)))
			{
				if((timeout *=2) > 2000)
					timeout = 2000;
				events = getnetevents(fd,errorCodes,1,eventFlags,3);
				events = 0;
				continue;
			}
			errno=EWOULDBLOCK;
			if(fdp->oflag&O_NDELAY)
				r=0;
		}
		else
		{
			SetLastError(err);
			logerr(LOG_DEV+1, "common_recv from=%d failed fd=%d dont_wait=%d flags=%x", from, fd, dont_wait, opt_flags);
			errno = unix_err(err);
		}
		break;
	}
done:
	if (dont_wait)
	{
		bioval = 0;
		ioctlsocket(Phandle(fd), FIONBIO, &bioval);
	}
	if(r>0 && (events&FD_CLOSE))
		setNetEvent(fd,FD_READ,6);
	logmsg(LOG_DEV+3, "common_recv() exit r=%d", r);
	sigcheck(sigsys);
	return(r);
}

/*
 * This function checks for input on the pipe and then waits
 * for an interrupt or a timeout.
 * It would be nice if you could wait on the pipe handle
 *  but this causes hanging in some systems
 */
ssize_t piperecv(HANDLE  hp, Pfd_t* fdp, char *buff, size_t size, int flags)
{
	long data=0,timeout=10;
	int waitval;
	Pfifo_t *ffp=0;
/*
 * flags:
 *  POSIX MSG_PEEK: transfer data, don't block or consume data
 *  MSG_DONTWAIT: non-blockocking operation if would block return EAGAIN
 *  MSG_ERRWUEUE: receive queued errors from socket error queue
 *  MSG_NOSIGNAL: don't generate SIGPIPE when remote disconnected
 *  POSIX MSG_OOB: request out-of-band data
 *  POSIX MSG_WAITALL: block until full request is satisfied
 */
	if(fdp->type==FDTYPE_FIFO || fdp->type==FDTYPE_EFIFO)
		ffp= &Pfifotab[fdp->devno-1];
	while(PeekNamedPipe(hp,buff,(DWORD)size,&data, NULL, NULL) && data==0)
	{
		/* see of other end of pipe is closed */
		if((fdp->sigio==0 && (fdp->type==FDTYPE_PIPE||fdp->type==FDTYPE_EPIPE))|| (ffp && ffp->nwrite<0))
		{
			errno = EPIPE;
			goto err;
		}
		if(flags&MSG_PEEK)
		{
			errno = EWOULDBLOCK;
			data = -1;
			goto err;
		}
		if(ffp)
			ffp->blocked = proc_slot(P_CP);
		waitval=WaitForSingleObject(P_CP->sigevent,timeout);
		if(waitval==WAIT_OBJECT_0)
		{
			ResetEvent(P_CP->sigevent);
			if(!processsig())
				timeout = 0;
		}
		else if(waitval!=WAIT_TIMEOUT)
		{
			logerr(0, "WaitForSingleObject");
			goto err;
		}
		else if(timeout==0)
		{
			errno = EINTR;
			goto err;
		}
		else if(timeout < 500)
			timeout <<= 1;
	}
	if(data || (flags & MSG_PEEK))
		goto done;
	errno = unix_err(GetLastError());
err:
	if(errno==EPIPE)
		return(0);
	else
		data=- 1;
done:
 	/* Need to move file pointer to consume the peeked data */
	if (!(flags & MSG_PEEK))
		ReadFile(hp, buff, (DWORD)size, &data, NULL);
	if(ffp)
		ffp->blocked = 0;
	return(data);
}


ssize_t recv(int fd, char *buf, size_t len, int flags)
{
	HANDLE hp=0;
	Pdev_t *pdev=0;
#ifdef GTL_TERM
	Pdev_t *pdev_c;
#endif
	Pfd_t *fdp;
	ssize_t r = -1;
	int forwin95 = 0;
	if (!isfdvalid (P_CP,fd))
	{
		logmsg(LOG_DEV+4, "recv() fd not valid fd=%d", fd);
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if(!(hp = Phandle(fd)))
	{
		logmsg(LOG_DEV+4, "recv() handle is null fd=%d", fd);
		errno = EBADF;
		return(-1);
	}
	if(fdp->devno>0)
		pdev = dev_ptr(fdp->devno);
	switch(fdp->type)
	{
	case FDTYPE_PTY:
		{
			unsigned short *blocks = devtab_ptr(Share->chardev_index,PTY_MAJOR);
#ifdef GTL_TERM
			if(!pdev || (!pdev->io.master.masterid && !blocks[pdev->minor-128]))
#else
			if(!pdev || (!pdev->Pseudodevice.master && !blocks[pdev->minor-128]))
#endif
			{
				errno = EIO; /* Is errno proper? */
				return(-1);
			}
		}
		/* fall thru */
	case FDTYPE_CONSOLE:
		if(!pdev)
		{
			errno = EIO;
			return(-1);
		}
#ifdef GTL_TERM
		hp = pdev->io.input.hp;
		pdev_c=pdev_lookup(pdev);
		hp=pdev_c->io.input.hp;
#else
		hp = dev_gethandle(pdev, pdev->SrcInOutHandle.ReadPipeInput);
#endif
		/* fall thru */
	case FDTYPE_PIPE:
	case FDTYPE_EPIPE:
	case FDTYPE_NPIPE:
	case FDTYPE_FIFO:
	case FDTYPE_EFIFO:
		r = piperecv(hp,fdp,buf,len,flags);
#ifndef GTL_TERM
		if(fdp->type==FDTYPE_CONSOLE)
			closehandle(hp,HT_ICONSOLE);
		else if(fdp->type==FDTYPE_PTY)
			closehandle(hp,HT_PIPE);
#endif
		return(r);

	case FDTYPE_CONNECT_INET:
	case FDTYPE_CONNECT_UNIX:
		logmsg(LOG_DEV+4, "recv() ENOTCONN cannot operate on a non-connected socket fd=%d", fd);
		errno = ENOTCONN;		/* recv() cannot operate on a non-connected socket. */
		return(-1);

	default:
		logmsg(LOG_DEV+4, "recv() EOPNOTSUPP fd=%d does not correspond to a connected socket", fd);
		errno = EOPNOTSUPP;	/* if we get here, fd is not a socket. */
		return(-1);

	case FDTYPE_SOCKET:
	case FDTYPE_DGRAM:
		return common_recv(fd, buf, len, flags, NULL, NULL);
	}
}

ssize_t recvfrom(int fd, char *buf, size_t len, int flags, struct sockaddr *from, int *fromlen)
{
	register Pfd_t *fdp;
	if (!isfdvalid (P_CP,fd))
	{
		logmsg(LOG_DEV+4, "recvfrom() fd not valid fd=%d", fd);
		errno = EBADF;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	if (fdp->type==FDTYPE_EPIPE)
		return piperecv(Phandle(fd),fdp,buf,len,flags);
	if(!issocket(fd))
	{
		errno = ENOTSOCK;
		return(-1);
	}
 	return common_recv(fd,buf,len,flags,from,fromlen);
}

static ssize_t common_send(int fd, const char *buf, size_t len, int flags, struct sockaddr *to, int tolen)
{
	ssize_t r = -1;
	long events=0;
	int sigsys, err, errorCodes[FD_MAX_EVENTS+1];
	int eventFlags=FD_CONNECT|FD_WRITE|FD_CLOSE;
	int dont_wait = 0, bioval, opt_flags=flags;
	HANDLE objects[2];
	Pfd_t *fdp;
	static int (PASCAL *ssendto)(HANDLE,const char*,int,int,struct sockaddr*,int);
	if (IsBadReadPtr(buf, len))
	{
		logmsg(LOG_DEV+4, "common_send() IsBadReadPtr() true buf=%p len=%d", buf, len);
		errno=EFAULT;
		return(-1);
	}
	if(!isfdvalid (P_CP,fd))
	{
		logmsg(LOG_DEV+4, "common_send() fd not valid fd=%d", fd);
		errno = EBADF;
		return(-1);
	}
	if(!Phandle(fd))
	{
		logmsg(LOG_DEV+4, "common_send() handle is null fd=%d", fd);
		errno = EBADF;
		return(-1);
	}
	if(is_socket(fd, FDTYPE_CONNECT_INET))
	{
		logmsg(LOG_DEV+4, "common_send() is_socket() returned true for FDTYPE_CONNECT_INET on fd=%d", fd);
		errno = ENOTCONN;
		return(-1);
	}
	fdp = getfdp(P_CP,fd);
	logmsg(LOG_DEV+3, "common_send fd=%d buf=%p len=%d flags=%x to=%p tolen=%d type=%(fdtype)s", fd, buf, len, flags, to, tolen, fdp->type);

	/*
	 * Emulate MSG_DONTWAIT, but enabling non-blocking mode for socket
	 * for this syscall.
	 *
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if (flags & MSG_DONTWAIT & !(fdp->oflag&O_NONBLOCK))
	{
		dont_wait = 1;
		bioval = 1;
		if(ioctlsocket(Phandle(fd), FIONBIO, &bioval) != 0)
		{
			errno = EOPNOTSUPP;
			return -1;
		}
	}
	/* Need to add support for posix required options, MSG_EOR */
	/*
	 * POSIX requires support MSG_EOR and MSG_OOB
	 * Linux also supports MSG_CONFIRM, MSG_DONTROUTE, MSG_DONTWAIT,
	 *    MSG_MORE, and MSG_NOSIGNAL.
	 * OpenBSD also supports MSG_DONROUTE, and MSG_DONTWAIT.
	 */
	/* Mask off flags not supported in Windows */
	flags &= (MSG_DONTROUTE | MSG_OOB);
	if(!sinit)
	{
		ssend = 0;
		ssendto = 0;
	}
	if(!to && !ssend && !(ssend= (int (PASCAL*)(HANDLE,const char*,int,int))getaddr("send")))
	{
		logerr(0, "socket library not found");
		errno = EOPNOTSUPP;
		return(-1);
	}
	if(to && !ssendto && !(ssendto =  (int (PASCAL*)(HANDLE, const char*, int, int, struct sockaddr*, int))getaddr("sendto")))
	{
		logerr(0, "MISSING SOCKET LIBRARY");
		errno = EOPNOTSUPP;
		return(-1);
	}
	objects[0] = P_CP->sigevent;
	objects[1] = Xhandle(fd);
	fdp = getfdp(P_CP,fd);
	events = getnetevents(fd,errorCodes,1,FD_CONNECT|FD_WRITE|FD_CLOSE,6);
	/*
	 * WINDOWS has all sockets in Non blocking mode has default,
	 * because of WSAEventSelect(). NOT user changable.
	 */
	if(!events && (fdp->oflag&(O_NONBLOCK|O_NDELAY)))
		events = FD_WRITE;
	sigsys = sigdefer(1);
	while(1)
	{
		if(!(events&FD_WRITE)) switch(WaitForMultipleObjects(2, objects, FALSE, INFINITE))
		{
		    case 0xFFFFFFFF:
			r = -1;
			errno = unix_err(h_error);
			goto done;
		    case WAIT_OBJECT_0:
			if(sigcheck(sigsys))
			{
				sigdefer(1);
				continue;
			}
			errno = EINTR;
			r=-1;
			goto done;
		    case WAIT_OBJECT_0 + 1:
			if(!(events = getnetevents(fd, errorCodes,1,FD_CONNECT|FD_WRITE|FD_CLOSE,7)))
				continue;
		}
		if(to)
			r = (*ssendto)(Phandle(fd), buf, (int)len, flags, to, (int)tolen);
		else
			r = (*ssend)(Phandle(fd), buf, (int)len, flags);
		if(r>=0)
		{
			if(r)
				setNetEvent(fd,FD_WRITE,7);
			break;
		}
		if((err=GetLastError())==WSAEWOULDBLOCK)
		{
			fdp->socknetevents &= ~FD_WRITE;
			/*
			 * WINDOWS has all sockets in Non blocking mode has default,
			 * because of WSAEventSelect(). NOT user changable.
			 */
			if(!(fdp->oflag&(O_NONBLOCK|O_NDELAY)))
			{
				events &= ~FD_WRITE;
				continue;
			}
		}
		errno = unix_err(err);
		break;
	}
done:
	/* Emulate MSG_DONTWAIT, restore blocking mode for socket, in needed */
	if (dont_wait)
	{
		bioval = 0;
		ioctlsocket(Phandle(fd), FIONBIO, &bioval);
	}
	logmsg(LOG_DEV+2, "common_send() normal exit r=%d", r);
	sigcheck(sigsys);
	return(r);
}

ssize_t send(int fd, const char *buf, size_t len, int flags)
{
	Pfd_t *fdp;
	fdp = getfdp(P_CP,fd);
	logmsg(LOG_DEV+5, "send() enter");
	if(fdp->type==FDTYPE_EPIPE)
	{
	    	ssize_t	ret;
	    	int	ign=0;

		/* Support MSG_NOSIGNAL mode on AF_UNIX domain */
		if (flags & MSG_NOSIGNAL && !SigIgnore(P_CP, SIGPIPE))
		{
			ign=1;
			sigsetignore(P_CP, SIGPIPE, 1);
		}
		ret = write(fd, buf, len);
		if (flags & MSG_NOSIGNAL & ign)
			sigsetignore(P_CP, SIGPIPE, 0);
		return ret;
	}
	return common_send(fd,buf, len, flags, NULL, 0);
}

ssize_t sendto(int fd, const char *buf, size_t len, int flags, struct sockaddr *to, int tolen)
{
	Pfd_t *fdp;
	fdp = getfdp(P_CP,fd);
	logmsg(LOG_DEV+5, "sendto() enter");
	if (fdp->type==FDTYPE_EPIPE)
	{
	    	ssize_t	ret;
	    	int	ign=0;

		/* Support MSG_NOSIGNAL mode on AF_UNIX domain */
		if (flags & MSG_NOSIGNAL && !SigIgnore(P_CP, SIGPIPE))
		{
			ign=1;
			sigsetignore(P_CP, SIGPIPE, 1);
		}
		ret = write(fd, buf, len);
		if (flags & MSG_NOSIGNAL & ign)
			sigsetignore(P_CP, SIGPIPE, 0);
		return ret;
 	}
	
	return common_send(fd,buf,len,flags,to,tolen);

}

int getsockopt(int fd, int level, int optname, void *optval, int *optlen)
{
	HANDLE hp = gethandle(fd);	/* connected or un-connected socket. */
	Pfd_t *fdp;
	int r= -1;
	if(!hp)
		return(-1);
	if(optval && IsBadWritePtr(optlen,sizeof(int)) || IsBadWritePtr(optval,*optlen))
	{
		errno = EFAULT;
		return(-1);
	}
	fdp=getfdp(P_CP,fd);
	if(fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE)
	{
		if(level==SOL_SOCKET)
		{
			switch(optname)
			{
			    case SO_SNDBUF:
			    case SO_RCVBUF:
				if(optval)
				{
					*((int*)optval) = PIPE_BUF;
					*optlen = sizeof(int);
				}
				break;
			    case SO_TYPE:
				if(optval)
				{
					*((int*)optval) = AF_UNIX;
					*optlen = sizeof(int);
				}
				break;
			    default:
				if(optval)
					ZeroMemory(optval,*optlen);
			}
			return(0);
		}
		errno = ENOPROTOOPT;
		return(-1);
	}
	if(!sinit || !sgetsockopt)
		sgetsockopt =  (int (PASCAL*)(HANDLE,int,int,char*,int*))getaddr("getsockopt");

	if(!sinit || !sWSAIoctl)
		sWSAIoctl = (int (PASCAL*)(int, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE))getaddr("WSAIoctl");

	if((level == IPPROTO_IP) && (Share->sockstate == WS_20))
	{
		switch(optname)	/* None of these can be mapped to WSAIoctl() calls. Hence, not supported.*/
		{
		case IP_MULTICAST_IF:
			errno = ENOPROTOOPT;
			return -1;

		case IP_MULTICAST_TTL:
			errno = ENOPROTOOPT;
			return -1;

		case IP_MULTICAST_LOOP:
			errno = ENOPROTOOPT;
			return -1;

		case IP_ADD_MEMBERSHIP:
			errno = ENOPROTOOPT;
			return -1;

		case IP_DROP_MEMBERSHIP:
			errno = ENOPROTOOPT;
			return -1;

		default:
			break;
		}
	}

	if(sgetsockopt)
		r = (*sgetsockopt)(hp,level,optname,(char*)optval,optlen);
	if (r != 0 )
		errno = unix_err(h_error);
	return(r);
}

typedef struct
{
	HANDLE hp;
	struct sockaddr_in addr;
} Join_leaf_data;

static void WINAPI afinet_join_leaf(Join_leaf_data* jd)
{
	HANDLE newsock;
	newsock = (*sWSAJoinLeaf)(jd->hp,	/* socket */
		(PSOCKADDR)&jd->addr,		/* multicast address */
		sizeof(jd->addr),			/* length of addr struct */
		NULL,						/* caller data buffer */
		NULL,						/* callee data buffer */
		NULL,						/* socket QOS setting */
		NULL,						/* socket group QOS */
		JL_BOTH);					/* do both: send and receive */

	if(newsock == _INVALID_SOCKET)
	{
		errno = unix_err(h_error);
		ExitThread((DWORD)-1);
	}
	else
		ExitThread(0);
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	HANDLE hp = gethandle(fd);	/* connected or un-connected socket. */
	Pfd_t *fdp;
	int r= 0;
	if(!hp)
		return(-1);
	if(IsBadReadPtr(optval,optlen))
	{
		errno=EFAULT;
		return(-1);
	}
	fdp=getfdp(P_CP,fd);
	if(fdp->type==FDTYPE_PIPE || fdp->type==FDTYPE_EPIPE)
	{
		/* who knows what do to about these -- just a guess */
		if(level==SOL_SOCKET)
		{
			switch(optname)
			{
			    case SO_DEBUG:
			    case SO_KEEPALIVE:
			    case SO_DONTROUTE:
			    case SO_USELOOPBACK:
			    case SO_LINGER:
			    case SO_SNDBUF:
			    case SO_RCVBUF:
				return(0);
			    default:
				break;
			}
		}
		errno = ENOPROTOOPT;
		return(-1);
	}

	if(!sinit || !ssetsockopt)
		ssetsockopt =  (int (PASCAL*)(HANDLE,int,int,const char*,int))getaddr("setsockopt");

	if(!sinit || !sWSAIoctl)
		sWSAIoctl = (int (PASCAL*)(int, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE)) getaddr("WSAIoctl");

	if(!sinit || !sWSAJoinLeaf)
		sWSAJoinLeaf = (HANDLE (PASCAL*)(HANDLE, const struct sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS, DWORD))getaddr("WSAJoinLeaf");

	if((level == IPPROTO_IP) && (Share->sockstate <= WS_20))
	{
		switch(optname)
		{
		case IP_MULTICAST_IF:/* Currently a no-op. No way to store this info.  Not supported by winsock 2.*/
			return 0;

		case IP_MULTICAST_TTL:
			if(optlen != sizeof(int) && optlen != sizeof(unsigned char))
			{
				errno = EFAULT;
				return -1;
			}
			if((hp = enable_multicast(fd)) == _INVALID_SOCKET)
				return -1;
			else
			{
				int ttl;
				DWORD cbytes;

				switch(optlen)
				{
				    case sizeof(int):
					ttl = atoi((char*)optval);
					break;
				    case sizeof(unsigned char):
					ttl = *((unsigned char*)optval);
					break;
				}
				r = (*sWSAIoctl)((SOCKET)hp,		/* socket */
					SIO_MULTICAST_SCOPE,			/* IP Time-To-Live */
					&ttl,							/* input */
					sizeof(ttl),					/* size */
					NULL,							/* output */
					0,								/* size */
					&cbytes,						/* bytes returned */
					NULL,							/* overlapped */
					NULL);							/* completion routine */
			}
			if(r < 0)
			{
				errno = unix_err(h_error);
				return -1;
			}
			else
			{
				return 0;
			}

		case IP_MULTICAST_LOOP:	/* NOTE:  This protocol is not supported by winsock 2.x */
			if(optlen != sizeof(int) && optlen != sizeof(unsigned char))
			{
				errno = EFAULT;
				return -1;
			}

			if((hp = enable_multicast(fd)) == _INVALID_SOCKET)
			{
				return -1;
			}
			else
			{
				int bflag;
				DWORD cbytes;

				switch(optlen)
				{
				case sizeof(int):
					bflag = atoi((char*)optval);
					break;
				case sizeof(unsigned char):
					bflag = *((unsigned char*)optval);
					break;
				}

				r = (*sWSAIoctl)((SOCKET)hp,		/* socket */
					SIO_MULTIPOINT_LOOPBACK,		/* turn loopBack off */
					&bflag,							/* input */
					sizeof(bflag),					/* size */
					NULL,							/* output */
					0,								/* size */
					&cbytes,						/* bytes returned */
					NULL,							/* overlapped */
					NULL);							/* completion routined */
			}
			if(r < 0)
			{
				errno = unix_err(h_error);
				return -1;
			}
			else
				return 0;

		case IP_ADD_MEMBERSHIP:
			if(optlen != sizeof(struct ip_mreq))
			{
				errno = EFAULT;
				return -1;
			}

			if((hp = enable_multicast(fd)) == _INVALID_SOCKET)
			{
				errno = unix_err(h_error);
				return -1;
			}
			else
			{
				Join_leaf_data jld;
				unsigned long enable;
				HANDLE objects[2];
				int tid;
				DWORD timeout;
				DWORD waitretn;
				struct ip_mreq* pmreq = (struct ip_mreq*)optval;
				int addrlength = sizeof(struct sockaddr_in);

				jld.hp = hp;

				/* We need to get the current sockaddr_in address, especially for the port
				   number added when the user's code called bind() earlier on this socket.*/
				if(getsockname(fd, (PSOCKADDR)&jld.addr, &addrlength) != 0)
					return -1;

				jld.addr.sin_addr.s_addr = pmreq->imr_multiaddr.s_addr; /* set multicast address*/

				objects[0] = P_CP->sigevent;

				/* Make the socket temporarily blocking (disable non-blocking) */
				enable = 0;
				/*
				 * WINDOWS has all sockets in Non blocking mode has default,
				 * because of WSAEventSelect(). NOT user changable.
				 */
				if((fdp->oflag&O_NONBLOCK) && (ioctlsocket(hp, FIONBIO, &enable) != 0))
				{
					errno = unix_err(h_error);
					return -1;
				}

				logmsg(LOG_DEV+4, "setsockopt() creating AF_INET join leaf thread");

				/* Now join the multicast group */

				if(!(objects[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)afinet_join_leaf, &jld, 0, &tid)))
				{
					errno = unix_err(GetLastError());
					r = -1;
					goto done;
				}
				/*
				 * WINDOWS has all sockets in Non blocking mode has default,
				 * because of WSAEventSelect(). NOT user changable.
				 */
				timeout = (getfdp(P_CP, fd)->oflag&(O_NONBLOCK|O_NDELAY)) ? 10 : INFINITE;
			sigdefer(1);
			again:
				waitretn = WaitForMultipleObjects(2, objects, FALSE, timeout);
				if(waitretn == WAIT_OBJECT_0 + 1)
				{
					if(!GetExitCodeThread(objects[1], &r))
						r = -1;

					closehandle(objects[1],HT_THREAD);
				}
				else
				{
					terminate_thread(objects[1],(DWORD)-1);
					if(waitretn == WAIT_OBJECT_0)
					{
						if(sigcheck(0))
						{
							sigdefer(1);
							goto again;
						}
						errno = EINTR;
						r = -1;
					}
					else if(waitretn = WAIT_TIMEOUT)
					{
						errno = ETIMEDOUT;
						r = -1;
					}
					else if(waitretn = 0xFFFFFFFF)
					{
						logerr(0, "WaitForMultipleObjects() failed in setsockopt() for IP_ADD_MEMBERSHIP");
						errno = unix_err(h_error);
						r = -1;
					}
				}
				sigcheck(0);

				/* Restore non-blocking mode of socket. */
done:
				enable = 1;
				/*
				 * WINDOWS has all sockets in Non blocking mode has default,
				 * because of WSAEventSelect(). NOT user changable.
				 */
				if((fdp->oflag & O_NONBLOCK) && (ioctlsocket(hp, FIONBIO, &enable) != 0))
				{
					errno = unix_err(h_error);
					return -1;
				}
				return r;
			}

		case IP_DROP_MEMBERSHIP:
			/* This is a no-op.  The only way to drop membership in winsock2 is to close the socket. */
			return 0;

		default:
			break;
		}
	}

	if(ssetsockopt)
	{
		if(state.clone.hp)
			r = cl_setsockopt(*ssetsockopt,hp,level,optname,(char*)optval,(int)optlen);
		else
			r = (*ssetsockopt)(hp,level,optname,(char*)optval,(int)optlen);
	}
	if(optname==IP_TOS) /* avoid warnings if not implemented */
		r = 0;
	if (r != 0 )
		errno = unix_err(h_error);
	return(r);
}

static HANDLE enable_multicast(int fd)
{
	/* This function replaces the socket referenced by the specified fd with one
	   that has pointcast flags set for multicast operations, and sets a bit in the
	   associated oflag to indicate that this has been done.  The oflag is tested for
	   this bit (MULTICAST_SET) when the function is first called and if already set,
	   the function returns success with no further action.  If upon call this bit is
	   not set in oflag, then a new socket will be created using the same fd slot.
	   If the original socket had been bound, then the new socket will be bound to the
	   same address.

	   Return value:  valid socket handle = success, INVALID_SOCKET = failure
	 */
	static SOCKET (PASCAL *wsasocket)(int,int,int, LPWSAPROTOCOL_INFO, GROUP, DWORD);
	static int (PASCAL *sclose)(HANDLE);
	HANDLE hp=_INVALID_SOCKET;
	Pfd_t *fdp;
	struct sockaddr_in addr;
	int addrlength = sizeof(struct sockaddr_in);
	int mustbind = 0;

	if(!is_socket(fd, FDTYPE_CONNECT_INET) && !is_socket(fd, FDTYPE_DGRAM))	/* verify fd is an un-connected INET socket */
	{
		errno = ENOTSOCK;
		return _INVALID_SOCKET;
	}

	fdp = getfdp(P_CP, fd);
	if(fdp->oflag & MULTICAST_SET)
		return Phandle(fd);	/* Socket already enabled for multicasting. */

	if(!sinit || !sclose)
		sclose =  (int (PASCAL*)(HANDLE))getaddr("closesocket");

	/* WSASocketA is loaded -- ASCII characters.*/
	if(!sinit || !wsasocket)
		wsasocket = (SOCKET (PASCAL*)(int,int,int, LPWSAPROTOCOL_INFO, GROUP, DWORD))getaddr("WSASocketA");

	if(!sinit || !ssetsockopt)
		ssetsockopt =  (int (PASCAL*)(HANDLE,int,int,const char*,int))getaddr("setsockopt");

	/* Before we close the original socket, we must first determine if it has been bound.
	If it has, then the new socket must be bound to the same address so as not to loose this
	information, and also to put the new socket in the same bound state as the original.*/

	if(!sinit || !sgetsockname)
		sgetsockname = (int (PASCAL*)(HANDLE,struct sockaddr*,int*))getaddr("getsockname");

	if((*sgetsockname)(Phandle(fd),(PSOCKADDR)&addr,&addrlength) != 0)
	{
		if((*sWSAGetLastError)() != WSAEINVAL)	/*If error is WSAEINVAL then socket not bound or bound to ADDR_ANY. This is not an error here.*/
		{
			errno = unix_err(h_error);
			return _INVALID_SOCKET;
		}
	}
	else	/*If we get here, then the socket has been bound previously. So, bind the new socket.*/
	{
		mustbind = 1;
	}

	hp = (HANDLE)((*wsasocket)(AF_INET, SOCK_DGRAM, 0, (LPWSAPROTOCOL_INFO)NULL, 0,
	        WSA_FLAG_MULTIPOINT_C_LEAF | WSA_FLAG_MULTIPOINT_D_LEAF));

	if(hp == _INVALID_SOCKET)
	{
		errno = EFAULT;
		return _INVALID_SOCKET;
	}

	(*sclose)(Phandle(fd));	/* Close the original socket  */
	htab_socket(Phandle(fd),0,HTAB_CLOSE,"multicast");
	htab_socket(hp,0,HTAB_CREATE,"multicast");
	if(init_socket(fd,hp,Xhandle(fd)))
	{
		int enable = 1;
		/*
		 * WINDOWS has all sockets in Non blocking mode has default,
		 * because of WSAEventSelect(). NOT user changable.
		 */
		if(!(fdp->oflag&O_NONBLOCK) && (ioctlsocket(Phandle(fd), FIONBIO, &enable) != 0))
		{
			errno = unix_err(h_error);
			return _INVALID_SOCKET;
		}
	}
	else
	{
		errno = EBADF;
		return _INVALID_SOCKET;
	}

	fdp->oflag |= MULTICAST_SET;	/* Set flag to indicate multicasting enabled for this socket fd. */

	if(mustbind)
	{
		int success;
		for(success = 10; success > 0; --success)
		{
			if(bind(fd, (PSOCKADDR)&addr, addrlength) >= 0)
				break;
			Sleep(10);
		}
		if(!success)
		{
			return _INVALID_SOCKET;
		}
	}

	return Phandle(fd);
}

int socket(int domain, int type, int protocol)
{
	HANDLE hp=_INVALID_SOCKET;
	Pfd_t *fdp;
	int fdi,fd;

	if ((fd = get_fd(0)) == -1)
		return(-1);
	if(state.clone.hp)
                return(cl_socket2(fd,domain,type,protocol));
	if ((fdi = fdfindslot ()) == -1)
		goto err;
	setfdslot(P_CP,fd,fdi);
	fdp= &Pfdtab[fdi];
	fdp->oflag = O_RDWR;
	fdp->socknetevents = 0;

	/* We support only two domains at present.  MWB  */
	switch(domain)
	{
	case AF_INET:
	case AF_INET6:
		switch(type)
		{
		case SOCK_STREAM:
			fdp->type = FDTYPE_CONNECT_INET;
			break;
		case SOCK_DGRAM:
		case SOCK_RAW:
			fdp->type = FDTYPE_DGRAM;
			break;
		default:
			errno = ESOCKTNOSUPPORT;
			goto err;
		}
		break;

	case AF_UNIX:
		setsocket(fd);
		fdp->type = FDTYPE_CONNECT_UNIX;
		Phandle(fd) = hp;
		return(fd);

	default:
		errno = EAFNOSUPPORT;
		goto err;
	}

	if(!sinit || !ssocket)
		ssocket =  (HANDLE (PASCAL*)(int,int,int))getaddr("socket");
	if(ssocket)
	{
		if(state.clone.hp)
		{
			static HANDLE (PASCAL *sWSAsocket)(int,int,int,WSAPROTOCOL_INFO*,GROUP,DWORD);
			if(!sWSAsocket)
				sWSAsocket = (HANDLE (PASCAL*)(int,int,int,WSAPROTOCOL_INFO*,GROUP,DWORD))getaddr("WSASocketA");
			hp =cl_socket(ssocket,domain,type,protocol,sWSAsocket,sizeof(WSAPROTOCOL_INFO));
		}
		else
			hp=(*ssocket)(domain, type, protocol);
	}
	if(ssocket && hp==_INVALID_SOCKET)
		errno = unix_err(h_error);
	if(hp==_INVALID_SOCKET)
		goto err;
	if(hp)
		htab_socket(hp,0,HTAB_CREATE,"socket");
	if(hp && init_socket(fd,hp,NULL))
	{
		/* init_socket calls WSAEventSelect, which sets non-blocking mode */
		if(type==SOCK_RAW)
			fdp->socknetevents |= FD_READ|FD_WRITE;
		else if(type==SOCK_DGRAM)
			fdp->socknetevents |= FD_WRITE;
		return(fd);
	}
	errno = EAFNOSUPPORT;
err:
	setfdslot(P_CP,fd,-1);
	fdpclose(&Pfdtab[fdi]);
	return(-1);
}

ssize_t recvmsg(int fd, struct msghdr* msg, int flags)
{
	Pfd_t*		fdp;
	ssize_t		datalen = 0;
	ssize_t		offset = 0;
	ssize_t		r = 0;
	ssize_t		n;
	int		i;
	char*		buf;
	Msg_rights_t	acc;

	if (!isfdvalid(P_CP, fd))
	{
		errno = EBADF;
		return -1;
	}
	fdp = getfdp(P_CP,fd);
	if (!is_anysocket(fdp))
	{
		errno = ENOTSOCK;
		return -1;
	}

	/*
	 * read the access header
	 */
	if (fdp->type == FDTYPE_EPIPE || fdp->type == FDTYPE_PIPE)
	{
		if (read(fd, &acc, sizeof(acc.header)) != sizeof(acc.header))
		{
			logerr(0, "recvmsg read header failed");
			errno = EBADMSG;
			return -1;
		}
		if (acc.header.nfds > 0)
		{
			n = (ssize_t)((char*)&acc.fd[acc.header.nfds] - (char*)&acc.fd[0]);
			if (read(fd, &acc.fd[0], n) != n);
			{
				logerr(0, "recvmsg read access fds failed");
				errno = EBADMSG;
				return -1;
			}
		}
	} else {
		acc.header.ntpid=acc.header.nfds=0;
	}
	for (i = 0; i < msg->msg_iovlen; i++)
		datalen += (int)msg->msg_iov[i].iov_len;
	if (datalen > 0)
	{
		if (!(buf = malloc(datalen)))
		{
			logmsg(LOG_DEV+4, "recvmsg() malloc failed");
			errno = ENOMEM;
			return -1;
		}
		if (acc.header.ntpid != 0 && datalen > acc.header.data_len)
			datalen=acc.header.data_len;
		if (fdp->type == FDTYPE_EPIPE || fdp->type == FDTYPE_PIPE)
			n = read(fd, buf, datalen);
		else
			n = common_recv(fd, (char *)buf, datalen, flags, (struct sockaddr*)msg->msg_name, (int *)&msg->msg_namelen);
		if (n < 0)
		{
			logerr(0, "recvmsg data read failed");
			free(buf);
			return -1;
		}
		r = n;
		offset = 0;
		for (i = 0; i < msg->msg_iovlen; i++)
		{
			if ((int)msg->msg_iov[i].iov_len > n)
				msg->msg_iov[i].iov_len = n;
			memcpy(msg->msg_iov[i].iov_base, buf + offset, msg->msg_iov[i].iov_len);
			offset += (int)msg->msg_iov[i].iov_len;
			if (!(n -= (int)msg->msg_iov[i].iov_len))
				break;
		}
		r -= n;
		free(buf);
		msg->msg_name = 0;
		msg->msg_namelen = 0;
	}
	if (msg->msg_accrightslen > 0 && acc.header.nfds > 0)
	{
		register Pproc_t*	pp;
		HANDLE			pph = 0;
		HANDLE			hp;
		int*			p=NULL;
		int			nfd;

		logmsg(LOG_DEV+4, "recvmsg rightslen=%d sock_type=%(fdtype)s ntpid=%d nfds=%d", msg->msg_accrightslen, fdp->type, acc.header.ntpid, acc.header.nfds);
		if (!(pp = proc_ntgetlocked(acc.header.ntpid,0)))
		{
			errno = ESRCH;
			return -1;
		}
		if (!(pph = open_proc(pp->ntpid,PROCESS_ALL_ACCESS)))
		{
			logerr(0, "recvmsg OpenProcess ntpid=x%x", pp->ntpid);
			errno = EACCES;
			goto badacc;
		}
		if (!ImpersonateNamedPipeClient(Phandle(fd)))
			logerr(0, "ImpersonateNamedPipeClient");
		p = (int*)msg->msg_accrights;
		n = msg->msg_accrightslen / sizeof(*p);
		if (n > acc.header.nfds)
		{
			n = acc.header.nfds;
			msg->msg_accrightslen = (int)(n * sizeof(*p));
		}
		for (i = 0; i < n; i++)
		{
			int rfd = acc.fd[i].fd;
			if (getfdp(pp, rfd)->type == FDTYPE_CONSOLE)
				hp = (HANDLE)1; /* bogus but its never used! */
			else if (!DuplicateHandle(pph, acc.fd[i].phandle, GetCurrentProcess(), &hp, 0, TRUE, DUPLICATE_SAME_ACCESS))
			{
				logerr(0, "DuplicateHandle");
				errno = EBADF;
				goto badacc;
			}
			if ((nfd = get_fd(0)) < 0)
			{
				CloseHandle(hp);
				goto badacc;
			}
			setfdslot(P_CP, nfd,fdslot(pp, rfd));
			Phandle(nfd) = hp;
			Xhandle(nfd) = 0;
			resetcloexec(nfd);
			incr_refcount(fdp=getfdp(P_CP,nfd));
			sethandle(fdp,nfd);
			*p++ = nfd;
			if (acc.fd[i].xhandle)
			{
				if (!DuplicateHandle(pph, acc.fd[i].xhandle, GetCurrentProcess(), &hp, 0, TRUE, DUPLICATE_SAME_ACCESS))
				{
					logerr(0, "DuplicateHandle");
					errno = EBADF;
					goto badacc;
				}
				Xhandle(nfd) = hp;
			}
		}
		RevertToSelf();
			proc_release(pp);
		closehandle(pph,HT_PROC);

		/*
		 * see the comment in sendmsg() for details on this write
		 */

		write(fd, "\n", 1);
		return r;
	badacc:
		RevertToSelf();
		if (pph)
			closehandle(pph,HT_PROC);
			proc_release(pp);
		while (p > (int*)msg->msg_accrights)
			close(*--p);
		return -1;
	}
	return r;
}

ssize_t sendmsg(int fd, struct msghdr* msg, int flags)
{
	Pfd_t*		fdp;
	int		i;
	int		d;
	ssize_t		datalen;
	ssize_t		offset;
	register int*	p;
	char*		buf = 0;
	Msg_rights_t	acc;

	if (!isfdvalid(P_CP, fd))
	{
		errno = EBADF;
		return -1;
	}
	fdp = getfdp(P_CP,fd);
	if (!is_anysocket(fdp))
	{
		errno = ENOTSOCK;
		return -1;
	}
	datalen = 0;
	for (i = 0; i < msg->msg_iovlen; i++)
		datalen += (int)msg->msg_iov[i].iov_len;
	if (datalen > 0 && !(buf = malloc(datalen)))
	{
		logmsg(LOG_DEV+4, "sendmsg() malloc failed");
		errno = ENOMEM;
		return -1;
	}

	/*
	 * access rights are file descriptors in all domains
	 * it usually only makes sense for AF_UNIX but nt
	 * may allow some handles to be passed across machines
	 */

	logmsg(LOG_DEV+4, "sendmsg rightslen=%d datalen=%d sock_type=%(fdtype)s", msg->msg_accrightslen, datalen, fdp->type);
	if (fdp->type == FDTYPE_EPIPE || fdp->type == FDTYPE_PIPE)
	{
		p = (int*)msg->msg_accrights;
		acc.header.nfds = msg->msg_accrightslen / sizeof(*p);
		acc.header.data_len = (int)datalen;
		if (acc.header.nfds > elementsof(acc.fd))
		{
			errno = EMSGSIZE;
			goto bad;
		}
		acc.header.ntpid = P_CP->ntpid;
		for (i = 0; i < acc.header.nfds; i++)
		{
			d = *p++;
			if (!isfdvalid(P_CP, d))
			{
				errno = EBADF;
				goto bad;
			}
			acc.fd[i].fd = d;
			acc.fd[i].phandle = Phandle(d);
			acc.fd[i].xhandle = Xhandle(d);
		}

		/*
		 * send the access info first
		 */

		i = (int)((char*)&acc.fd[acc.header.nfds] - (char*)&acc);
		if (write(fd, &acc, i) != i)
		{
			errno = EBADMSG;
			goto bad;
		}
	} else {
		acc.header.ntpid=acc.header.nfds=0;
	}
	if (datalen > 0)
	{
		offset = 0;
		for (i = 0; i < msg->msg_iovlen; i++)
		{
			memcpy(buf + offset, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
			offset += (int)msg->msg_iov[i].iov_len;
		}

		/*
		 * send the linearized data
		 */

		if (fdp->type == FDTYPE_EPIPE || fdp->type == FDTYPE_PIPE)
			datalen = write(fd, buf, datalen);
		else
			datalen = common_send(fd, (const char *)buf, datalen, flags, (struct sockaddr*)msg->msg_name, msg->msg_namelen);
	}
	if (acc.header.nfds > 0)
	{
		/*
		 * the file descriptor handles are held by this process
		 * and must be kept open until the other side grabs them
		 *
		 * a wait is not the nicest here
		 * but a third process would otherwise be required to
		 * keep the handles open
		 *
		 * the wait is done by reading one byte from the pipe
		 */

		HANDLE		objects[2];

		sigdefer(1);
		objects[0] = P_CP->sigevent;
		objects[1] = Phandle(fd);
	again:
		if (WaitForMultipleObjects(2, objects, FALSE, INFINITE) == WAIT_OBJECT_0)
		{
			if(sigcheck(0))
			{
				sigdefer(1);
				goto again;
			}
			errno = EINTR;
			goto bad;
		}
		datalen = read(fd, buf, 1);
		sigcheck(0);
	}
	free(buf);
	return datalen;
 bad:
	if (buf)
		free(buf);
	return -1;
}

int shutdown(int fd, int how)
{
	HANDLE hp = gethandle(fd);
	int r= -1;
	Pfd_t *fdp;

	fdp = getfdp(P_CP,fd);
	if(fdp->type == FDTYPE_PIPE)
	{
		errno = ENOTSOCK;
		return(-1);
	}
	else if(fdp->type==FDTYPE_EPIPE)
	{
		hp = Xhandle(fd);
		if(fdp->oflag&O_RDWR)
		{
			if(how==SD_RECEIVE)
			{
				closehandle(Phandle(fd),HT_PIPE);
				Phandle(fd) = hp;
			}
			else
				closehandle(hp,HT_PIPE);
			if(hp=Ehandle(fd))
			{
				/* make it inheritable */
				HANDLE me=GetCurrentProcess();
				if(!DuplicateHandle(me,hp,me,&hp,0,TRUE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
					logerr(0, "DuplicateHandle");
				Xhandle(fd) = hp;
				Ehandle(fd) = 0;
			}
			else
			{
				char name[UWIN_RESOURCE_MAX];
				int slot = fdp->sigio-2;
				if(file_slot(fdp) > slot)
					slot = file_slot(fdp);
				UWIN_EVENT_SOCK_IO(name, slot);
				Xhandle(fd) = hp = CreateEvent(sattr(1),TRUE,FALSE,name);
			}
		}
		fdp->oflag &= ~O_ACCMODE;
		if(how!=SD_RECEIVE)
			fdp->oflag |= O_RDONLY;
		if(how!=SD_SEND)
			fdp->oflag |= O_WRONLY;
		if(fdp->sigio)
		{
			Pfdtab[fdp->sigio-2].socknetevents |=FD_CLOSE;
			if(Pfdtab[fdp->sigio-2].socknetevents&(1<<7))
			{
				if(!SetEvent(hp))
					logerr(0, "SetEvent");
			}
		}
		return(0);
	}

	if(!hp)
	{	/* If we get here, errno will have been set by gethandle(). */
		return(-1);
	}
	if(fdp->type == FDTYPE_CONNECT_UNIX || fdp->type == FDTYPE_CONNECT_INET)
	{
		errno = ENOTCONN;
		return(-1);
	}

	if(!sinit || !sshutdown)
		sshutdown =  (int (PASCAL*)(HANDLE,int))getaddr("shutdown");
	if(sshutdown)
		r = (*sshutdown)(hp,how);
	if (r != 0 )
		errno = unix_err(h_error);
	else if(how==SD_BOTH)
	{
		/* prevent calling shutdown() twice on same socket */
		getfdp(P_CP,fd)->type = FDTYPE_CONNECT_INET;
		getfdp(P_CP,fd)->socknetevents |= FD_CLOSE;
	}
	return(r);
}

#if 1

static unsigned long pipecnt=0;

int socketpair(int d, int type, int protocol, int *sv)
{
	int fdi0, fdi1, cnt, ret=-1;
	HANDLE hpin1,hpout1;
	HANDLE hpin2,hpout2;
	DWORD pmode;
	char inname[64], outname[64];

/* This is just for the error checking */
	if ( d != AF_UNIX && d!=AF_INET )
	{
		errno = EAFNOSUPPORT;
		return(-1);
	}
	if(protocol)
	{
		if ( (protocol != IPPROTO_UDP ) && ( protocol != IPPROTO_TCP))
		{
			if(protocol==IPPROTO_ICMP || protocol==IPPROTO_RAW || protocol==IPPROTO_IP)
				errno = EOPNOTSUPP;
			else
				errno = EPROTONOSUPPORT;
			return (-1);
		}
		if ( (type == SOCK_DGRAM) && ( protocol != IPPROTO_UDP ))
		{
			errno = ESOCKTNOSUPPORT;
			return (-1);
		}
		if ( (type == SOCK_STREAM) && ( protocol != IPPROTO_TCP ))
		{
			errno = ESOCKTNOSUPPORT;
			return (-1);
		}
	}
	if ( (type != SOCK_STREAM ) && ( type != SOCK_DGRAM ))
	{
		if(type==SOCK_RAW)
			errno = ESOCKTNOSUPPORT;
		else
			errno = EINVAL;
		return (-1);
	}
	if (d==AF_INET)
	{
		errno = EOPNOTSUPP;
		return(-1);
	}
	if(IsBadWritePtr(sv,2*sizeof(int)))
	{
		errno = EFAULT;
		return(-1);
	}

/* Error checking code ends here */

	sigdefer(1);
	if((sv[0]=get_fd(0)) == -1 || (fdi0=fdfindslot()) < 0)
	{
		errno = EMFILE;
		goto done;
	}
	setfdslot(P_CP,sv[0],fdi0);

	if((sv[1]=get_fd(sv[0])) == -1 || (fdi1=fdfindslot()) < 0)
	{
		setfdslot(P_CP,sv[0],-1);
		fdpclose(&Pfdtab[fdi0]);
		errno = EMFILE;
		goto done;
	}
	setfdslot(P_CP,sv[1],fdi1);
	Pfdtab[fdi0].type = FDTYPE_EPIPE;
	Pfdtab[fdi1].type = FDTYPE_EPIPE;
	Pfdtab[fdi0].oflag = O_RDWR;
	Pfdtab[fdi1].oflag = O_RDWR;
	hpin1 = hpout1 = hpin2 = hpout2 = 0;
	pmode = PIPE_NOWAIT;	/* needed for ConnectNamedPipe to work */
	if (type == SOCK_DGRAM)
		pmode |= PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE;
	/* create unique name */
	cnt = pipecnt++;
	UWIN_PIPE(inname, 'R', P_CP->ntpid, cnt);
	hpin1 = CreateNamedPipe(inname, PIPE_ACCESS_DUPLEX, pmode, 1, PIPE_BUF+24, PIPE_BUF+24, 0, sattr(1));
	if (!hpin1)
		goto cleanup_pipe;
	/* Ignore error return due to PIPE_NOWAIT mode */
	(void) ConnectNamedPipe(hpin1, NULL);
	UWIN_PIPE(outname, 'W', P_CP->ntpid, cnt);
	hpout1 = CreateNamedPipe(outname, PIPE_ACCESS_DUPLEX, pmode, 1, PIPE_BUF+24, PIPE_BUF+24, 0, sattr(1));
	if (!hpout1)
		goto cleanup_pipe;
	/* Ignore error return due to PIPE_NOWAIT mode */
	(void) ConnectNamedPipe(hpout1, NULL);
	hpin2 = createfile(outname, (GENERIC_READ|GENERIC_WRITE|FILE_WRITE_ATTRIBUTES),
			(FILE_SHARE_READ|FILE_SHARE_WRITE), sattr(1),
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hpin2)
		goto cleanup_pipe;
	hpout2 = createfile(inname, (GENERIC_READ|GENERIC_WRITE|FILE_WRITE_ATTRIBUTES),
			(FILE_SHARE_READ|FILE_SHARE_WRITE), sattr(1),
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hpout2)
		goto cleanup_pipe;
	/* Set Pipes to WAIT Mode */
	pmode = PIPE_WAIT;
	if (type == SOCK_DGRAM)
		pmode |= PIPE_READMODE_MESSAGE;
	if (SetNamedPipeHandleState(hpin1, &pmode, NULL, NULL) == 0)
	{
		logerr(LOG_DEV+4, "socketpair set hpin1 failed");
		goto cleanup_pipe;
	}
	if (SetNamedPipeHandleState(hpout1, &pmode, NULL, NULL) == 0)
	{
		logerr(LOG_DEV+4, "socketpair set hpout1 failed");
		goto cleanup_pipe;
	}
	if (SetNamedPipeHandleState(hpin2, &pmode, NULL, NULL) == 0)
	{
		logerr(LOG_DEV+4, "socketpair set hpin2 failed");
		goto cleanup_pipe;
	}
	if (SetNamedPipeHandleState(hpout2, &pmode, NULL, NULL) == 0)
	{
		logerr(LOG_DEV+4, "socketpair set hpout2 failed");
		goto cleanup_pipe;
	}
	Phandle(sv[0]) = hpin1;
	Xhandle(sv[0]) = hpout1;
	Phandle(sv[1]) = hpin2;
	Xhandle(sv[1]) = hpout2;
	Pfdtab[fdi0].sigio = fdi1+2;
	Pfdtab[fdi1].sigio = fdi0+2;
	sethandle(&Pfdtab[fdi0],sv[0]);
	sethandle(&Pfdtab[fdi1],sv[1]);
	ret=0;
	goto done;
cleanup_pipe:
	{
		errno = unix_err(GetLastError());
		DisconnectNamedPipe(hpin1);
		CloseHandle(hpin1);
		DisconnectNamedPipe(hpout1);
		CloseHandle(hpout1);
		DisconnectNamedPipe(hpin2);
		CloseHandle(hpin2);
		DisconnectNamedPipe(hpout2);
		CloseHandle(hpout2);
		setfdslot(P_CP,sv[0],-1);
		setfdslot(P_CP,sv[1],-1);
		fdpclose(&Pfdtab[fdi0]);
		fdpclose(&Pfdtab[fdi1]);
	}
done:
	sigcheck(0);
	return(ret);
}

#else

int socketpair(int d, int type, int protocol, int *sv)
{
	int fdi0, fdi1, ret=-1;
	HANDLE hpin1,hpout1;
	HANDLE hpin2,hpout2;

/* This is just for the error checking */
	if ( d != AF_UNIX && d!=AF_INET )
	{
		errno = EAFNOSUPPORT;
		return -1;
	}
	if(protocol)
	{
		if ( (protocol != IPPROTO_UDP ) && ( protocol != IPPROTO_TCP))
		{
			if(protocol==IPPROTO_ICMP || protocol==IPPROTO_RAW || protocol==IPPROTO_IP)
				errno = EOPNOTSUPP;
			else
				errno = EPROTONOSUPPORT;
			return -1;
		}
		if ( (type == SOCK_DGRAM) && ( protocol != IPPROTO_UDP ))
		{
			errno = ESOCKTNOSUPPORT;
			return -1;
		}
		if ( (type == SOCK_STREAM) && ( protocol != IPPROTO_TCP ))
		{
			errno = ESOCKTNOSUPPORT;
			return -1;
		}
	}
	if ( (type != SOCK_STREAM ) && ( type != SOCK_DGRAM ))
	{
		if(type==SOCK_RAW)
			errno = ESOCKTNOSUPPORT;
		else
			errno = EINVAL;
		return -1;
	}
	if (d==AF_INET)
	{
		errno = EOPNOTSUPP;
		return -1;
	}
	if(IsBadWritePtr(sv,2*sizeof(int)))
	{
		errno = EFAULT;
		return -1;
	}

/* Error checking code ends here */

	sigdefer(1);
	if((sv[0]=get_fd(0)) == -1 || (fdi0=fdfindslot()) < 0)
	{
		errno = EMFILE;
		goto done;
	}
	setfdslot(P_CP,sv[0],fdi0);

	if((sv[1]=get_fd(sv[0])) == -1 || (fdi1=fdfindslot()) < 0)
	{
		setfdslot(P_CP,sv[0],-1);
		fdpclose(&Pfdtab[fdi0]);
		errno = EMFILE;
		goto done;
	}
	setfdslot(P_CP,sv[1],fdi1);
	Pfdtab[fdi0].type = FDTYPE_EPIPE;
	Pfdtab[fdi1].type = FDTYPE_EPIPE;
	Pfdtab[fdi0].oflag = O_RDWR;
	Pfdtab[fdi1].oflag = O_RDWR;

	if(CreatePipe(&hpin1, &hpout1, sattr(1), PIPE_BUF+24) &&
	   CreatePipe(&hpin2, &hpout2, sattr(1), PIPE_BUF+24))
	{
		Phandle(sv[0]) = hpin1;
		Xhandle(sv[0]) = hpout2;
		Phandle(sv[1]) = hpin2;
		Xhandle(sv[1]) = hpout1;
		Pfdtab[fdi0].sigio = fdi1+2;
		Pfdtab[fdi1].sigio = fdi0+2;
	}
	else
	{
		setfdslot(P_CP,sv[0],-1);
		setfdslot(P_CP,sv[1],-1);
		fdpclose(&Pfdtab[fdi0]);
		fdpclose(&Pfdtab[fdi1]);
		errno = unix_err(GetLastError());
		goto done;
	}
	sethandle(&Pfdtab[fdi0],sv[0]);
	sethandle(&Pfdtab[fdi1],sv[1]);
	ret=0;

done:
	sigcheck(0);
	return ret;
}

#endif /* NEW_SOCKETPAIR */

struct getby
{
	union
	{
		void *(PASCAL *a1)(void*);
		void *(PASCAL *a2)(void*,void*);
		void *(PASCAL *a3)(void*,void*, int);
	} fun;
	void	*arg[3];
	int	narg;
	int	type;
	int	herr;
	int	err;
	char	*out;
	int	outsize;
	int	size;
};

struct common
{
	char *name;
	char **aliases;
};

#if _X64_

struct servent_native
{
	char*	s_name;
	char**	s_aliases;
	char*	s_proto;
	short	s_port;
};

#else

#define servent_native	servent

#endif

static int copy_to_local(void *ptr, int type)
{
	struct getby *gp = (struct getby*)ptr;
	struct common *pp = (struct common*)gp->out;
	char **av, **bv, **cv, *cp;
	int left,len,nalias=1;
	void *val = gp->out;

	cp = &gp->out[gp->size];
	len = (int)strlen(((struct common*)val)->name)+1;
	left = gp->outsize-gp->size;
	if(left<len)
		goto noroom;
	memcpy(cp,((struct common*)val)->name,len);
	pp->name = cp;
	cp += len;
	left -= len;

	for(len=0,av=((struct common*)val)->aliases; *av; av++)
	{
		len += (int)strlen(*av)+1+sizeof(char*);
		nalias++;
	}
	if(left < len+(signed)sizeof(char*))
		goto noroom;
	if(nalias>0)
	{
		cv = bv = (char**)(&gp->out[gp->outsize]-nalias*sizeof(char*));
		left -= nalias*sizeof(char*);
		for(av=((struct common*)val)->aliases; *av; av++)
		{
			len = (int)strlen(*av)+1;
			memcpy(cp,*av,len);
			*bv++ = cp;
			cp += len;
			left -= len;
		}
		*bv = 0;
		pp->aliases = cv;
	}
	switch(type)
	{
	    case SERVENT:
		len = (int)strlen(((struct servent_native*)val)->s_proto)+1;
		if(left < len)
			goto noroom;
		memcpy(cp, ((struct servent_native*)val)->s_proto,len);
#ifndef servent_native
		((struct servent*)val)->s_port = ((struct servent_native*)val)->s_port;
#endif
		((struct servent*)val)->s_proto = cp;
		cp += len;
		left -= len;
		break;

	    case HOSTENT:
	    {
		struct hostent *hp = (struct hostent*)val;
		int naddr =1;
		for(len=0,av=((struct hostent*)val)->h_addr_list; *av; av++)
		{
			len += hp->h_length+sizeof(char*);
				naddr++;
		}
		if(left < len+(signed)sizeof(char*))
			goto noroom;

		if(len)
		{
			cv = bv = (char**)(&gp->out[gp->outsize]-(nalias+naddr)*sizeof(char*));
			left -= naddr*sizeof(char*);
			for(av=((struct hostent*)val)->h_addr_list; *av; av++)
			{
				len = hp->h_length;
				memcpy(cp,*av,len);
				*bv++ = cp;
				cp += len;
				left -= len;
			}
			*bv = 0;
			hp->h_addr_list = cv;
		}
	    	break;
	    }
	    case PROTOENT:
	    default:
    		break;
	}
	gp->size = (int)(cp-gp->out);
	return(1);
noroom:
	logmsg(0, "getby no room left to copy data");
	gp->size = (int)(cp-gp->out);
	return(1);
}

static DWORD WINAPI getby(void* ptr)
{
	void *val=0;
	struct getby *gp = (struct getby*)ptr;
	struct common *pp = (struct common*)gp->out;
	switch(gp->narg)
	{
	    case 1:
		val = (*gp->fun.a1)(gp->arg[0]);
		val = (*gp->fun.a1)(gp->arg[0]);	/* DO NOT REMOVE.  Required to circumvent Microsoft bug. */
		break;
	    case 2:
		val = (*gp->fun.a2)(gp->arg[0],gp->arg[1]);
		break;
	    case 3:
		val = (*gp->fun.a3)(gp->arg[0],gp->arg[1],(int)gp->arg[2]);
		break;
	}
	if(val)
	{
		memcpy((void*)gp->out,val,gp->size);
		copy_to_local(ptr, gp->type);
		return(1);
	}
	gp->err = h_error;
	return(0);
}




/*
 * Runs function in a separate thread and waits for interrupt or completion
 */
static int getXbyY(struct getby *gp)
{
	HANDLE  objects[2];
	DWORD n,rval=0,id;
	int sigsys;
	objects[0]=P_CP->sigevent;
	objects[1] = CreateThread(NULL,0,getby,(void*)gp,0,&id);
	if(!objects[1])
	{
			gp->err = GetLastError();
			logerr(0, "CreateThread");
			return(0);
	}
	sigsys = sigdefer(1);
	while((n=WaitForMultipleObjects(2,objects,FALSE,INFINITE))==WAIT_OBJECT_0)
	{
		if(sigcheck(sigsys))
		{
			sigdefer(1);
			continue;
		}
		Sleep(10);
	}
	sigcheck(sigsys);

	if(n==WAIT_OBJECT_0+1)
	{
		if(!GetExitCodeThread(objects[1],&rval))
		{
			logerr(0, "GetExitCodeThread tid=%04x", objects[1]);
			rval = 0;
		}
	}
	closehandle(objects[1],HT_THREAD);
	return(rval);
}



/* Maps the WSA "h_errno" codes to those specified in netdb.h */
static void
set_herrno(int wsaerr)
{
	switch(wsaerr)
	{
	case WSAHOST_NOT_FOUND:
		*dllinfo._ast_herrno = HOST_NOT_FOUND;
		break;

	case WSATRY_AGAIN:
		*dllinfo._ast_herrno = TRY_AGAIN;
		break;

	case WSANO_RECOVERY:
		*dllinfo._ast_herrno = NO_RECOVERY;
		break;

	case WSANO_DATA:
		*dllinfo._ast_herrno = NO_DATA;
		break;

	default:
		*dllinfo._ast_herrno = NETDB_INTERNAL; /* see errno */
		break;
	}
}

char *hstrerror(int herror)
{
	switch(herror)
	{
	    case WSAHOST_NOT_FOUND:
		return("Unknown host");
	    case WSATRY_AGAIN:
		return("Host name lookup failure");
	    case WSANO_DATA:
		return("No address associated with name");
	    case WSANO_RECOVERY:
		return("Unknown server error");
	}
	return("Unknown error");
}


struct hostent *gethostbyname(const char *name)
{
	static void *(PASCAL *sgethostbyname)(void*);
	struct getby data;
	char name1[512];

	memset(name1, 0, sizeof(name1));

	// Get current host if host is not specified

	if(name)
	{
		if(strlen(name) > (sizeof(name1) -1))
		{
			errno = EINVAL;
			return(NULL);
		}
		memcpy(name1,name,strlen(name));
	}
	else
		gethostname(name1, sizeof(name1));

	if(!name1 || IsBadStringPtr(name1, 1024))
	{
		errno=EINVAL;
		*dllinfo._ast_herrno = NETDB_INTERNAL; /* see errno */
		return(NULL);
	}
	if(!sinit || !sgethostbyname)
		sgethostbyname = (void*(PASCAL*)(void*))getaddr("gethostbyname");
	if(sgethostbyname)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETHBYN,sgethostbyname,name1,strlen(name1)+1,0,hostbuff,sizeof(hostbuff)))
				return((struct hostent*)hostbuff);
			return(NULL);
		}
		data.fun.a1 = sgethostbyname;
		data.arg[0] = (void*)name1;
		data.narg = 1;
		data.out = hostbuff;
		data.outsize = sizeof(hostbuff);
		data.size = sizeof(struct hostent);
		data.type =  HOSTENT;
		if(getXbyY(&data))
			return((struct hostent*)hostbuff);
		errno=unix_err(data.err);
		set_herrno(data.err);
	}
	else
	{
		*dllinfo._ast_herrno = NETDB_INTERNAL; /* see errno */
		errno = ENOSYS;
	}
	return(NULL);
}




struct hostent *gethostbyaddr(const char *addr, int len, int type)
{
	static void *(PASCAL *sgethostbyaddr)(void*,void*,int);
	struct getby data;
	if(type == -1)
		return(NULL);
	if(IsBadReadPtr(addr,len))
	{
		errno=EFAULT;
		*dllinfo._ast_herrno = NETDB_INTERNAL; /* see errno */
		return(NULL);
	}
	if(!sinit || !sgethostbyaddr)
		sgethostbyaddr = (void*(PASCAL*)(void*,void*,int))getaddr("gethostbyaddr");
	if(sgethostbyaddr)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETHBYA,sgethostbyaddr,addr,(size_t)len,(void*)type,hostbuff,sizeof(struct hostent)))
				return((struct hostent*)hostbuff);
			return(NULL);
		}
		data.fun.a3 = sgethostbyaddr;
		data.arg[0] = (void*)addr;
		data.arg[1] = (void*)len;
		data.arg[2] = (void*)type;
		data.narg = 3;
		data.out = hostbuff;
		data.outsize = sizeof(hostbuff);
		data.size = sizeof(struct hostent);
		data.type =  HOSTENT;
		if(getXbyY(&data))
			return((struct hostent*)hostbuff);
		errno=unix_err(data.err);
		set_herrno(data.err);
	}
	else
	{
		*dllinfo._ast_herrno = NETDB_INTERNAL; /* see errno */
		errno = ENOSYS;
	}
	return(NULL);
}

#define SYSNAMELENGTH 80 //workaround for GetComputerName() bug in Win95
int getdomainname(char* name,size_t namelen)
{
	char buffer[PATH_MAX];
	HKEY hkey=0;
	DWORD type,nlen=sizeof(buffer);
	int r;
	struct qparam* pp;

	if(Share->Platform==VER_PLATFORM_WIN32_NT)
		pp = &ParamsNT;
	else
		pp = &Params95;
	if(r=RegOpenKeyEx(HKEY_LOCAL_MACHINE, pp->subkey, 0, KEY_READ, &hkey))
	{
		SetLastError(r);
		logerr(0, "RegOpenKeyEx");
			goto err;
	}
	if(r=RegQueryValueEx(hkey, pp->domain_val, NULL, &type, (void*)(buffer), &nlen))
	{
		SetLastError(r);
		logerr(0, "RegQueryValueEx");
		goto err;
	}
	if(nlen > namelen || nlen <= 1)
	{
		errno = EINVAL;
		return(-1);
	}
	RegCloseKey(hkey);
	buffer[PATH_MAX-1] = 0;	/* safety */
	strncpy(name, buffer, namelen);
	name[namelen-1] = 0;
	return(0);
err:
	if(hkey)
		RegCloseKey(hkey);
	errno = unix_err(GetLastError());
	return(-1);
}

int gethostname(char* name,size_t namelen)
{
	static int (PASCAL *sgethostname)(char*,int);
	char str[PATH_MAX+1];
	int len=sizeof(str),rlen;
	if (namelen==0 || IsBadReadPtr(name, namelen))
	{
		errno = EFAULT;
		return(-1);
	}
	if(!sinit || !sgethostname)
		sgethostname = (int(PASCAL*)(char*,int))getaddr("gethostname");
	if(sgethostname)
	{
		if(state.clone.hp)
		{
			rlen = len;
			len = (int)cl_sockops(SOP_HNAME,sgethostname,NULL,str,&rlen);
		}
		else
			len = (*sgethostname)(str,len);
		if(len >= 0)
			goto skip;
	}
	if(!GetComputerName(str, &len))
	{
		errno = unix_err(GetLastError());
		return(-1);
	}
skip:
	if(strchr(str,'.')==0)
	{
		int err = errno;
		len = (int)strlen(str);
		if(getdomainname(&str[len+1],sizeof(str)-1-len)==0)
			str[len] = '.';
		errno = err;
	}
	strncpy(name, str, namelen);
	name[namelen-1] = 0;
	if(name[namelen-2]=='.')
		name[namelen-2] = 0;
	return(0);
}

extern struct servent  *getservbyname_95(const char*, const char*);
struct servent  *getservbyname(const char *name, const char *proto)
{
	static void *(PASCAL *sgetservbyname)(void*,void*);
	struct getby data;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && GetVersionEx(&osinfo) && osinfo.dwMinorVersion<10)
		return(getservbyname_95(name,proto));
	if(!sinit || !sgetservbyname)
		sgetservbyname = (void*(PASCAL*)(void*,void*))getaddr("getservbyname");
	if(sgetservbyname)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETSBYN,sgetservbyname,name,strlen(name+1),(void*)proto,servbuff,sizeof(struct servent)))
				return((struct servent*)servbuff);
			return(NULL);
		}
		data.fun.a2 = sgetservbyname;
		data.arg[0] = (void*)name;
		data.arg[1] = (void*)proto;
		data.narg = 2;
		data.out = servbuff;
		data.outsize = sizeof(servbuff);
		data.size = sizeof(struct servent);
		data.type =  SERVENT;
		if(getXbyY(&data))
			return((struct servent*)servbuff);
		errno=unix_err(h_error);
	}
	else
		errno = ENOSYS;
	return(NULL);
}


extern struct servent  *getservbyport_95(int, const char*);
struct servent  *getservbyport(int port, const char *proto)
{
	static void *(PASCAL *sgetservbyport)(void*,void*);
	struct getby data;
	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	port = htons((unsigned short)port);
	if(Share->Platform!=VER_PLATFORM_WIN32_NT && GetVersionEx(&osinfo) && osinfo.dwMinorVersion<10)
		return(getservbyport_95(port,proto));
	if(!sinit || !sgetservbyport)
		sgetservbyport = (void*(PASCAL*)(void*,void*))getaddr("getservbyport");
	if(sgetservbyport)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETSBYP,sgetservbyport,proto,strlen(proto+1),(void*)port,servbuff,sizeof(struct servent)))
				return((struct servent*)servbuff);
			return(NULL);
		}
		data.fun.a2 = sgetservbyport;
		data.arg[0] = (void*)port;
		data.arg[1] = (void*)proto;
		data.narg = 2;
		data.out = servbuff;
		data.outsize = sizeof(servbuff);
		data.size = sizeof(struct servent);
		data.type =  SERVENT;
		if(getXbyY(&data))
			return((struct servent*)servbuff);
		errno=unix_err(h_error);
	}
	else
		errno = ENOSYS;
	return(NULL);
}



struct protoent *getprotobyname(const char *name)
{
	static void *(PASCAL *sgetprotobyname)(void*);
	struct getby data;
	if(!name || IsBadStringPtr(name, 1024))
	{
		errno=EFAULT;
		return(NULL);
	}
	if(!sinit || !sgetprotobyname)
		sgetprotobyname = (void*(PASCAL*)(void*))getaddr("getprotobyname");
	if(sgetprotobyname)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETPBYN,sgetprotobyname,name,strlen(name),NULL,protobuff,sizeof(struct protoent)))
				return((struct protoent*)protobuff);
			return(NULL);
		}
		data.fun.a1 = sgetprotobyname;
		data.arg[0] = (void*)name;
		data.narg = 1;
		data.out = protobuff;
		data.outsize = sizeof(protobuff);
		data.size = sizeof(struct protoent);
		data.type =  PROTOENT;
		if(getXbyY(&data))
			return((struct protoent*)protobuff);
		errno=unix_err(h_error);
	}
	else
		errno = ENOSYS;
	return(NULL);
}




struct protoent *getprotobynumber(int nproto)
{
	static void *(PASCAL *sgetprotobynumber)(void*);
	struct getby data;
	if(!sinit || !sgetprotobynumber)
		sgetprotobynumber = (void*(PASCAL*)(void*))getaddr("getprotobynumber");
	if(sgetprotobynumber)
	{
		if(state.clone.hp)
		{
			if(cl_getby(SOP_GETPBY_,sgetprotobynumber,NULL,0,(void*)nproto,protobuff,sizeof(struct protoent)))
				return((struct protoent*)protobuff);
			return(NULL);
		}
		data.fun.a1 = sgetprotobynumber;
		data.arg[0] = (void*)nproto;
		data.narg = 1;
		data.out = protobuff;
		data.outsize = sizeof(protobuff);
		data.size = sizeof(struct protoent);
		data.type =  PROTOENT;
		if(getXbyY(&data))
			return((struct protoent*)protobuff);
		errno=unix_err(h_error);
	}
	else
		errno = ENOSYS;
	return(NULL);
}

#undef ntohs
unsigned short ntohs(unsigned short ns)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned short ms;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&ns,&ms,sizeof(short));
	return(ms);
}

/*
 * local definition to avoid linking with winsock2 dll, used in net_disp
 */
unsigned long sock_ntohs(unsigned short ns)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned short ms;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&ns,&ms,sizeof(short));
	return ms;
}

#undef htons
unsigned short htons(unsigned short hs)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned short ms;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&hs,&ms,sizeof(short));
	return(ms);
}


#undef ntohl
unsigned long ntohl(unsigned long nl)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned long ml;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&nl,&ml,sizeof(long));
	return(ml);
}

/*
 * local definition to avoid linking with winsock2 dll, used in net_disp
 */
unsigned long sock_ntohl(unsigned long nl)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned long ml;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&nl,&ml,sizeof(long));
	return(ml);
}


#undef htonl
unsigned long htonl(unsigned long hl)
{
	static void* (*swapmem)(int, void*, void*,int);
	unsigned long ml;
	if(!swapmem)
		swapmem= (void*(*)(int,void*,void*,int))getsymbol(MODULE_ast,"swapmem");
	(*swapmem)(int_swap,&hl,&ml,sizeof(long));
	return(ml);
}



static int
siocgifnetmask( HANDLE hp, long cmd, char *argp)
{
	errno = ENOSYS;
	return(-1);
}

static int
siocgifbrdaddr( HANDLE hp, long cmd, char *argp)
{
	errno = ENOSYS;
	return(-1);
}

static int
siocgifdstaddr( HANDLE hp, long cmd, char *argp)
{
	errno = ENOSYS;
	return(-1);
}


/*
 * TODO -- this is still incomplete and may need to be recoded
 */
static int
siocgifflags( HANDLE hp, long cmd, char *argp)
{
	int	level = SOL_SOCKET;
	int	flags = 0;
	char	*buf;
	int	bufsize;
	int	optlen;
	int	n=0;
	int	i;
	int olderrno = errno;

	level = SOL_SOCKET;
	if( (buf = malloc(512)) == NULL)
	{
		errno = ENOMEM;
		return(-1);
	}

	bufsize=512;

	for(i = 0; ifFlagMap[i].ifunix != -1; ++i)
	{
		do
		{
			errno = 0;
			if( buf ) free(buf);
			bufsize += MEMBLOCKSIZE;
			if( (buf = malloc(bufsize)) == NULL)
			{
				errno = ENOMEM;
				return(-1);
			}
			optlen=bufsize;
			// what if this is the first time through...
			// and variable 'n' has not been set?  we
			// changed initialization to be zero... but
			// perhaps it should be -1?
			if(ifFlagMap[i].sowin32 == -1)
				continue;

			if(!sinit || !sgetsockopt)
				sgetsockopt =  (int (PASCAL*)(HANDLE,int,int,char*,int*))getaddr("getsockopt");
			if(sgetsockopt)
				n = (*sgetsockopt)(hp,level,ifFlagMap[i].sowin32,buf,&optlen);
			else
				n = -1;
			if( n != 0 )
				errno = h_errno;
		} while( n!=0 && errno == EFAULT);

		if( n != 0  )	/* getsockopt() will set errno. */
		{
			free(buf);
			return(-1);
		}

		errno = olderrno;	/* Restore errno.  No error occured except EFAULT which is trapped here. */

		if(  *buf )		/* WARNING: May not work for non-Boolean socket option values, but have only boolean now. */
			flags |= ifFlagMap[i].ifunix;
	}
	if(buf) free(buf);

	memcpy(argp, &flags, sizeof(flags));
	return(0);
}



static int
siocgifaddr( HANDLE hp, long cmd, char *argp)
{
	errno = ENOSYS;
	return(-1);
}


static int
siocgifconf(HANDLE hp, long cmd, char *argp)
{
	int	n;
	int i;
	long len;
	int	ifclen;
	char* ifcpos;
	char* buf	= NULL;
	int	sbs	= MEMBLOCKSIZE;
	int	retn = 0;
	struct	ifreq	ifr;
	struct	ifconf	ifc;
	WSAQUERYSETA	qs, *pqs;
	CSADDR_INFO	*pcsa;
	GUID		guidsc = SVCID_HOSTNAME;
	AFPROTOCOLS	afProtocols[2];
	long		flags;
	HANDLE		h;
	static int (PASCAL *wsalsb)(WSAQUERYSETA*, DWORD, HANDLE*);
	static int (PASCAL *wsalsn)(HANDLE,DWORD,LPDWORD,WSAQUERYSETA*);
	static int (PASCAL *wsalse)(HANDLE);

	if(!sinit || !wsalsb || !wsalsn || !wsalse)
	{
		wsalsb = (int (PASCAL*)(WSAQUERYSETA*,DWORD,HANDLE*))	\
				getaddr("WSALookupServiceBeginA");
		wsalsn = (int (PASCAL*)(HANDLE,DWORD,LPDWORD,WSAQUERYSETA*)) \
				getaddr("WSALookupServiceNextA");
		wsalse = (int (PASCAL*)(HANDLE))			\
				getaddr("WSALookupServiceEnd");
	}

	if( !wsalsb || !wsalsn || !wsalse)
	{
		errno = unix_err(h_error);
		logerr(0, "siocgifconf() failed to loaded WSA proc addresses");
		return(-1);
	}

	memcpy(&ifc,argp,sizeof(struct ifconf));
	ifclen = ifc.ifc_len;
	ifcpos = ifc.ifc_ifcu.ifcu_buf;
	memset(&qs,0,sizeof(WSAQUERYSETA));
	qs.dwSize = sizeof(WSAQUERYSETA);
	qs.lpszServiceInstanceName = NULL;
	qs.lpServiceClassId = &guidsc;
	qs.dwNameSpace = NS_ALL;
	qs.dwNumberOfProtocols = 2;
	qs.lpafpProtocols = afProtocols;
	afProtocols[0].iAddressFamily = AF_INET;
	afProtocols[0].iProtocol = IPPROTO_TCP;
	afProtocols[1].iAddressFamily = AF_INET;
	afProtocols[1].iProtocol = IPPROTO_UDP;
	flags = LUP_RETURN_ALL;

	if( (*wsalsb)( &qs, flags, &h) == SOCKET_ERROR)
	{
		errno = unix_err(h_error);
		logerr(0, "siocgifconf() wsalsb failed with error SOCKET_ERROR");
		return(-1);
	}

	if( (buf=malloc(sbs)) == NULL )
	{
		errno = ENOMEM;
		return(-1);
	}

	for(;;)
	{
		flags = LUP_FLUSHPREVIOUS;
		len   = sbs;
		memset(buf,0,len);
		if( ifclen < sizeof(struct ifreq))
			break;
			/* call WSALookupServiceNext.  Results stored in buf
				h == handle opened by WSALookupServiceBegin
				flags == LUP_PREVIOUS
				len   == sizeof buf
			*/
		n = (*wsalsn)(h,flags,&len,(WSAQUERYSETA *)buf);

		if(n != 0)
		{
			n=(*sWSAGetLastError)();

			if(n == WSAENOMORE || n == WSA_E_NO_MORE)
			{
				break;
			}
			else if(n == WSAEFAULT)
			{
				free(buf);
				sbs += MEMBLOCKSIZE;
				buf = malloc(sbs);
				if( buf == NULL )
				{
					errno = ENOMEM;
					retn = -1;
					goto out;
				}
			}
			else
			{
				errno=unix_err(h_error);
				logerr(0, "siocgifconf() wsalsn failed SOCKET_ERROR");
				retn = -1;
				goto out;
			}
		}
		else
		{
			pqs = (WSAQUERYSETA *)buf;
			// cast the lpcsaBuffer to a CSADDR_INFO pointer
			pcsa = pqs->lpcsaBuffer;
			for(i=0; i < (int)pqs->dwNumberOfCsAddrs; ++i)
			{
				memset(&ifr,0,sizeof(struct ifreq));
				strcpy(ifr.ifr_ifrn.ifrn_name,pqs->lpszServiceInstanceName);
				ifr.ifr_ifru.ifru_addr.sa_family = pcsa->LocalAddr.lpSockaddr->sa_family;
				ifr.ifr_ifru.ifru_data = (caddr_t)&(pcsa->LocalAddr);
				memcpy(ifcpos,&ifr,sizeof(struct ifreq));
				ifcpos += sizeof(struct ifreq);
				ifclen -= sizeof(struct ifreq);
			}
		}
	}
	ifc.ifc_len = (int)(ifcpos - ifc.ifc_ifcu.ifcu_buf);
	ifc.ifc_ifcu.ifcu_req = (struct ifreq *) ifc.ifc_buf;
	memcpy(argp, &ifc, sizeof(struct ifconf));
out:
	wsalse(h);
	if(buf)
		free(buf);
	return(0);
}

int ioctlsocket(HANDLE hp, long cmd, unsigned long *argp)
{
	static int (PASCAL *sioctlsocket)(HANDLE,long,unsigned long*);
	int r= -1;
	if(!hp)
		return(-1);

	switch(cmd)
	{
	case SIOCGIFCONF:
		return(siocgifconf(hp, cmd, (char*)argp));

	case SIOCGIFADDR:
		return(siocgifaddr(hp,cmd,(char*)argp));

	case SIOCGIFFLAGS:
		return(siocgifflags(hp,cmd,(char*)argp));

	case SIOCGIFDSTADDR:
		return(siocgifdstaddr(hp,cmd,(char*)argp));

	case SIOCGIFBRDADDR:
		return(siocgifbrdaddr(hp,cmd,(char*)argp));

	case SIOCGIFNETMASK:
		return(siocgifnetmask(hp,cmd,(char*)argp));

	default:
		if(!sinit || !sioctlsocket)
			sioctlsocket = (int (PASCAL*)(HANDLE,long,unsigned long*))getaddr("ioctlsocket");

		if(sioctlsocket)
			r = (*sioctlsocket)(hp,cmd,argp);

		if (r != 0 )
		{
			if(h_error==WSAENOTSOCK )
				errno = EINVAL;
			else
				errno = unix_err(h_error);
		}
	}
	return(r);
}

static ssize_t sockread(int fd, register Pfd_t *fdp, char *buf, size_t size)
{
	if(!Phandle(fd))
	{
		errno = EBADF;
		return(-1);
	}
	return common_recv(fd, buf, size, 0, NULL, NULL);
}


static ssize_t sockwrite(int fd, register Pfd_t *fdp, char *buf, size_t size)
{
	if(!Phandle(fd))
	{
		errno = EBADF;
		return(-1);
	}
	if (IsBadReadPtr(buf, size))
	{
		logmsg(LOG_DEV+4, "sockwrite() IsBadReadPtr() true  buf=%p size=%d", buf, size);
		errno=EFAULT;
		return(-1);
	}
	return(common_send(fd, buf, size, 0, NULL, 0));
}


static int sockfstat(int fd, Pfd_t *fdp, struct stat *st)
{
	pipefstat(fd,fdp,st);
	st->st_mode = S_IFSOCK;
	return(0);
}


void sethostent(int stayopen)
{
}

void endhostent(void)
{
}

void setprotoent(int stayopen)
{
}

void endprotoent(void)
{
}

struct hostent *gethostent( void )
{
	return(NULL);
}

int gethostid(void)
{
	char buffer[128];
	struct in_addr  host_id ;
	struct hostent *host ;

	if (gethostname(buffer, sizeof(buffer)) < 0 )
		return(0);
	if(!(host = (void *)gethostbyname(buffer)))
		return(0);
	memcpy(&host_id, (void *)(host->h_addr_list)[0], sizeof(host_id));
	return(host_id.s_addr);
}

void sockinit()
{
	getaddr("socket");
}

static ssize_t noread(int fd, register Pfd_t *fdp, char *buff, size_t size)
{
	if(fdp->devno)
		errno = unix_err(fdp->devno);
	else
		errno = ENOTCONN;
	return(-1);
}

static ssize_t nowrite(int fd, register Pfd_t *fdp, char *buff, size_t size)
{
	errno = ENOTCONN;
	return(-1);
}

static int unixclose(int fd, Pfd_t *fdp, int noclose)
{
	if(!noclose && Xhandle(fd))
	{
		if(fdp->refcount<=0)
		{
			/* last close, duplicate handle from init process */
			Path_t info;
			HANDLE hp,init_proc,me=GetCurrentProcess();
			info.oflags = GENERIC_READ;
			if(pathreal(fdname(file_slot(fdp)),P_FILE|P_EXIST|P_NOHANDLE,&info))
			{
				hp = (HANDLE)info.name[1];
				if(init_proc=open_proc(info.name[0],0))
				{
					if(DuplicateHandle(init_proc,hp,me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS|DUPLICATE_CLOSE_SOURCE))
						closehandle(hp,HT_PIPE);
				}
				closehandle(init_proc,HT_PROC);
			}
		}
		return(fileclose(fd,fdp,noclose));
	}
	return(0);
}

static HANDLE inetselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	long events;
	int flags, errorCodes[FD_MAX_EVENTS+1];
	if(type<0)
		return(hp);
	if(type==0)
		flags = (FD_READ|FD_ACCEPT|FD_CLOSE);
	else if(type==1)
	{
		flags =  (FD_WRITE|FD_CLOSE);
		if(fdp->type==FDTYPE_CONNECT_INET)
			flags |= FD_CONNECT;
	}
	else
		flags = (FD_OOB|FD_CLOSE);
	if(!sinit)
		 getaddr("closesocket");
	if(hp)
	{
		if((events = getnetevents(fd, errorCodes,0,flags,14)) < 0)
			return(0);
		if(events&flags)
		{
			if((flags&FD_CONNECT) && fdp->type==FDTYPE_CONNECT_INET)
			{
				if(errorCodes[0] && errorCodes[FD_CONNECT_BIT])
					fdp->devno = errorCodes[FD_CONNECT_BIT];
				else
					fdp->type=FDTYPE_SOCKET;
			}
			return(0);
		}
		return(hp);
	}
	if(events = getnetevents(fd, errorCodes,0,flags,type+11))
	{
		if((events&FD_CONNECT) && fdp->type==FDTYPE_CONNECT_INET)
		{
			if(errorCodes[0] && errorCodes[FD_CONNECT_BIT])
				fdp->devno = errorCodes[FD_CONNECT_BIT];
			else
			{
				fdp->type=FDTYPE_SOCKET;
				ResetEvent(Xhandle(fd));
			}
		}
		return(0);
	}
	return(Xhandle(fd));
}

static HANDLE sockselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	if(hp && type<0)
		return(hp);
	return(inetselect(fd,fdp,type,hp));
}

static HANDLE unixselect(int fd, Pfd_t* fdp,int type, HANDLE hp)
{
	if(hp)
		return(0);
	return(Xhandle(fd));
}

void socket_init(void)
{
	filesw(FDTYPE_SOCKET, sockread, sockwrite, pipelseek, sockfstat, sockclose, sockselect);
	filesw(FDTYPE_DGRAM, sockread, sockwrite, pipelseek, sockfstat, sockclose,sockselect);
	filesw(FDTYPE_CONNECT_UNIX, noread, nowrite, pipelseek, sockfstat, unixclose,unixselect);
	filesw(FDTYPE_CONNECT_INET, noread, nowrite, pipelseek, sockfstat, sockclose,inetselect);
}

#define TYPE_SHIFT	12
#define FD_MASK		((1<<TYPE_SHIFT)-1)

static int common_select(int nfd, fd_set *in, fd_set *out, fd_set *err, struct timeval *tp,int bsd)
{
	register unsigned int i,nwait=1;
	register int j,fd;
	int r=0, winwait=0, setsize;
	fd_set *ready[3] ,*inset[3], *winset;
	DWORD milli=INFINITE;
	short *fdmap;
	HANDLE hp,*objects;

#if 0
	if(!sinit)
		 getaddr("closesocket");
#endif
	setsize = sizeof(long)*((nfd+NFDBITS-1)/NFDBITS);
	objects = (HANDLE*)alloca((3*nfd+1)*(sizeof(HANDLE)+sizeof(short))+4*setsize);
	fdmap = (short*)(&objects[3*nfd+1]);
	ready[0] = (fd_set*)(&fdmap[3*nfd+1]);
	ready[1] = (fd_set*)((char*)ready[0]+setsize);
	ready[2] = (fd_set*)((char*)ready[1]+setsize);
	winset  = (fd_set*)((char*)ready[2]+setsize);
	inset[0] = in;
	inset[1] = out;
	inset[2] = err;
	if(!P_CP->sigevent)
	{
		if (!state.sigevent)
			initsig(1);
		P_CP->sigevent = state.sigevent;
	}
	objects[0] = P_CP->sigevent;
	ZeroMemory((void*)ready[0],4*setsize);
	if(tp)
	{
		if(tp->tv_sec>=100000)
		{
			tp->tv_sec = 100000;
			tp->tv_usec = 0;
		}
		if(tp->tv_usec >= 1000000 || tp->tv_usec < 0 || tp->tv_sec < 0)
		{
			errno = EINVAL;
			return(-1);
		}
		milli = (DWORD)(1000*tp->tv_sec + tp->tv_usec/1000);
	}
	sigdefer(1);
	for(fd=0; fd<nfd; fd++)
	{
		j = 0;
		for(i=0;i< 3; i++)
		{
			if(!inset[i] || !FD_ISSET(fd,inset[i]))
				continue;
			if (!isfdvalid(P_CP,fd))
			{
				errno = EBADF;
				r = -1;
				goto finish;
			}
			if((hp=Fs_select(fd, i, 0))==INVALID_HANDLE_VALUE)
			{
				r = -1;
				goto finish;
			}
			else if(hp == 0)
			{
				milli = 0;
				FD_SET(fd, ready[i]);
				/* only count descriptor once for bsd */
				if(j++==0 || !bsd)
					r++;
			}
			else if(hp==(HANDLE)1)
			{
				winwait = QS_ALLINPUT;
				FD_SET(fd,winset);
			}
			else if(i==0 || hp!=objects[nwait-1])
			{
				objects[nwait] = hp;
				fdmap[nwait++] = fd|((1<<i)<<TYPE_SHIFT);
			}
			else if(nwait>1 && hp==objects[nwait-1])
				fdmap[nwait-1] |= ((1<<i)<<TYPE_SHIFT);
		}
	}
	P_CP->state = PROCSTATE_SELECT;
	while(nwait)
	{
#ifdef LOGIT
		logmsg(LOG_DEV+4, "select nwait=%d r=%d handle=%x fdmap=%x milli=%d", nwait, r, objects[1], fdmap[1], milli);
#endif
		i = mwait(nwait, objects, FALSE, milli, winwait);
		if(i > WAIT_OBJECT_0 && i < WAIT_OBJECT_0+nwait)
		{
			int k= 0;
			i -= WAIT_OBJECT_0;
			fd = fdmap[i]&FD_MASK;
			for(j=0; j < 3; j++)
			{
				if(bsd && FD_ISSET(fd, ready[j]))
					k = 1;
				if(!((1<<j)&(fdmap[i]>>TYPE_SHIFT)))
					continue;
				if(!(hp=Fs_select(fd, j, objects[i])))
				{
					FD_SET(fd, ready[j]);
					if(k++==0 || !bsd)
						r++;
				}
				else if(hp==INVALID_HANDLE_VALUE)
				{
					r = -1;
					goto finish;
				}
				if(r>0)
					milli = 0;
			}
			/* nothing was ready, so just try again */
			if(r==0 && milli)
				continue;
			j = (fdmap[i]>>TYPE_SHIFT);
			Fs_select(fd,-j,objects[i]);
			if(--nwait<=1)
				break;
			objects[i] = objects[nwait];
			fdmap[i] = fdmap[nwait];
		}
		else if (i==WAIT_OBJECT_0 ) /* Signal */
		{
			if(sigcheck(0) && !bsd)
			{
				sigdefer(1);
				continue;
			}
			errno = EINTR;
			r=-1;
			break;
		}
		else if (i==WAIT_OBJECT_0+nwait) /* /dev/windows */
		{
			for(fd=0; fd<nfd; fd++)
			{
				if(FD_ISSET(fd,winset))
				{
					FD_SET(fd, ready[0]);
					r++;
					milli = 0;
				}
			}
			winwait = 0;
		}
		else if (i==WAIT_TIMEOUT)
		{
			if (milli==0)
				break;
			milli = 0;
		}
		else if(i == 0xFFFFFFFF)
		{
			static int count;
			HANDLE me=GetCurrentProcess();
			if(!count)
				logerr(0, "MsgWaitForMultipleObjects=%d failed", nwait);
			for(j=0; j < (int)nwait; j++)
			{
				if(DuplicateHandle(me,objects[j],me,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
					CloseHandle(hp);
				else if (!count)
				{
					logerr(0, "select() dup handle object[%d]=%x failed", j, objects[j]);
					/* dump file information */
					fd = fdmap[j]&FD_MASK;
					if (isfdvalid(P_CP, fd))
					{
						Pfd_t *fdp;

						fdp = getfdp(P_CP, fd);
					   	logmsg(0, "file slot=%d type=%(fdtype)s refcnt=%d name='%s'", file_slot(fdp), fdp->type, fdp->refcount, fdp->index, (fdp->index>0?((char*)block_ptr(fdp->index)):""));
					}
					else
						logmsg(0, "fd=%d invalid", fd);
				}
			}
			if(count++ > 30)
			{
				errno = unix_err(h_error);
				exit(129);
			}
			Sleep(200);
		}
	}
	if(r>=0)
	{
		if(in)
			memcpy((void*)in,ready[0],setsize);
		if(out)
			memcpy((void*)out,ready[1],setsize);
		if(err)
			memcpy((void*)err,ready[2],setsize);
	}
finish:
	P_CP->state = PROCSTATE_RUNNING;
	for(i=1; i < nwait; i++)
	{
		j = (fdmap[i]>>TYPE_SHIFT);
		Fs_select(fdmap[i]&FD_MASK,-j,objects[i]);
	}
	sigcheck(0);
#ifdef LOGIT
	logmsg(LOG_DEV+3, "select() done r=%d", r);
#endif
	return(r);
}

#undef select
int select(int nfd, fd_set *in, fd_set *out, fd_set *err, struct timeval *tp)
{
	return(common_select(nfd,in,out,err,tp,0));
}

/*
 * select is not restartable even with SA_RESTART
 * on timeout, in, out, and err are not touched
 * the return value is number of ready fds, not total ready events
 */
int _bsd_select(int nfd, fd_set *in, fd_set *out, fd_set *err, struct timeval *tp)
{
	return(common_select(nfd,in,out,err,tp,1));
}

int poll(struct pollfd *fds, unsigned long nfds, int timeout)
{
	int ret, max=-1;
	unsigned long i;
	fd_set readfds, writefds, exceptfds;
	struct timeval tvptr[1];
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	if(IsBadReadPtr(fds, nfds*sizeof(struct pollfd)))
	{
		errno = EFAULT;
		return(-1);
	}
	for (i=0;i<nfds;i++)
	{
		if (!isfdvalid(P_CP,fds[i].fd))
			continue;
		if (fds[i].events & POLLIN)
			FD_SET(fds[i].fd, &readfds);
		if (fds[i].events & POLLOUT)
			FD_SET(fds[i].fd, &writefds);
		if (fds[i].events & POLLERR)
			FD_SET(fds[i].fd, &exceptfds);

		if (fds[i].fd > max)
			max=fds[i].fd;
	}

	if (timeout != -1)
	{
		tvptr->tv_sec = timeout/1000;
		tvptr->tv_usec = (timeout%1000)*1000;
	}
	ret = select( max+1, &readfds, &writefds, &exceptfds, (timeout==-1)?NULL:tvptr);

	/* check for error */
	if (ret <= 0)
		return ret;

	for (i=0,ret=0;i<nfds;i++)
	{
		fds[i].revents = 0;
		if (!isfdvalid(P_CP,fds[i].fd))
		{
			fds[i].revents |= POLLNVAL;
			continue;
		}
		if ((FD_ISSET(fds[i].fd, &readfds)))
			fds[i].revents |= POLLIN;
		if ((FD_ISSET(fds[i].fd, &writefds)))
			fds[i].revents |= POLLOUT;
		if ((FD_ISSET(fds[i].fd, &exceptfds)))
			fds[i].revents |= POLLERR;
		if (getfdp(P_CP,fds[i].fd)->socknetevents&FD_CLOSE)
		{
			fds[i].revents |= POLLHUP;
			fds[i].revents &= ~POLLOUT;
		}
		if (fds[i].revents)
			ret++;
	}
	return ret;
}
