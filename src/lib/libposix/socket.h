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
#ifndef _libposix_socket_h
#define _libposix_socket_h

#include	"uwindef.h"
#include	"reg.h"
#include	<sys/types.h>
#include 	<sys/time.h>
#include 	<sys/uio.h>
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<netdb.h>
#include	<netinet/in.h>
#include	<sys/select.h>
#include	<net/if.h>
#include	<sys/sockio.h>

/*
 * For use in the implementation of gethostname.
 */

struct qparam
{
	char* subkey;
	char* domain_val;
	char* hostname_val;
};

#ifndef WSABASEERR
/*
 * All Windows Sockets error constants are biased by WSABASEERR from
 * the "normal"
 */
#define WSABASEERR              10000
/*
 * Windows Sockets definitions of regular Microsoft C error constants
 */
#define WSAEINTR                (WSABASEERR+4)
#define WSAEBADF                (WSABASEERR+9)
#define WSAEACCES               (WSABASEERR+13)
#define WSAEFAULT               (WSABASEERR+14)
#define WSAEINVAL               (WSABASEERR+22)
#define WSAEMFILE               (WSABASEERR+24)

/*
 * Windows Sockets definitions of regular Berkeley error constants
 */
#define WSAEWOULDBLOCK          (WSABASEERR+35)
#define WSAEINPROGRESS          (WSABASEERR+36)
#define WSAEALREADY             (WSABASEERR+37)
#define WSAENOTSOCK             (WSABASEERR+38)
#define WSAEDESTADDRREQ         (WSABASEERR+39)
#define WSAEMSGSIZE             (WSABASEERR+40)
#define WSAEPROTOTYPE           (WSABASEERR+41)
#define WSAENOPROTOOPT          (WSABASEERR+42)
#define WSAEPROTONOSUPPORT      (WSABASEERR+43)
#define WSAESOCKTNOSUPPORT      (WSABASEERR+44)
#define WSAEOPNOTSUPP           (WSABASEERR+45)
#define WSAEPFNOSUPPORT         (WSABASEERR+46)
#define WSAEAFNOSUPPORT         (WSABASEERR+47)
#define WSAEADDRINUSE           (WSABASEERR+48)
#define WSAEADDRNOTAVAIL        (WSABASEERR+49)
#define WSAENETDOWN             (WSABASEERR+50)
#define WSAENETUNREACH          (WSABASEERR+51)
#define WSAENETRESET            (WSABASEERR+52)
#define WSAECONNABORTED         (WSABASEERR+53)
#define WSAECONNRESET           (WSABASEERR+54)
#define WSAENOBUFS              (WSABASEERR+55)
#define WSAEISCONN              (WSABASEERR+56)
#define WSAENOTCONN             (WSABASEERR+57)
#define WSAESHUTDOWN            (WSABASEERR+58)
#define WSAETOOMANYREFS         (WSABASEERR+59)
#define WSAETIMEDOUT            (WSABASEERR+60)
#define WSAECONNREFUSED         (WSABASEERR+61)
#define WSAELOOP                (WSABASEERR+62)
#define WSAENAMETOOLONG         (WSABASEERR+63)
#define WSAEHOSTDOWN            (WSABASEERR+64)
#define WSAEHOSTUNREACH         (WSABASEERR+65)
#define WSAENOTEMPTY            (WSABASEERR+66)
#define WSAEPROCLIM             (WSABASEERR+67)
#define WSAEUSERS               (WSABASEERR+68)
#define WSAEDQUOT               (WSABASEERR+69)
#define WSAESTALE               (WSABASEERR+70)
#define WSAEREMOTE              (WSABASEERR+71)

/*
 * Extended Windows Sockets error constant definitions
 */
#define WSASYSNOTREADY          (WSABASEERR+91)
#define WSAVERNOTSUPPORTED      (WSABASEERR+92)
#define WSANOTINITIALISED       (WSABASEERR+93)
#define WSAEDISCON              (WSABASEERR+101)
#define WSAENOMORE              (WSABASEERR+102)
#define WSAECANCELLED           (WSABASEERR+103)
#define WSAEINVALIDPROCTABLE    (WSABASEERR+104)
#define WSAEINVALIDPROVIDER     (WSABASEERR+105)
#define WSAEPROVIDERFAILEDINIT  (WSABASEERR+106)
#define WSASYSCALLFAILURE       (WSABASEERR+107)
#define WSASERVICE_NOT_FOUND    (WSABASEERR+108)
#define WSATYPE_NOT_FOUND       (WSABASEERR+109)
#define WSA_E_NO_MORE           (WSABASEERR+110)
#define WSA_E_CANCELLED         (WSABASEERR+111)
#define WSAEREFUSED             (WSABASEERR+112)

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (when using the resolver). Note that these errors are
 * retrieved via WSAGetLastError() and must therefore follow
 * the rules for avoiding clashes with error numbers from
 * specific implementations or language run-time systems.
 * For this reason the codes are based at WSABASEERR+1001.
 * Note also that [WSA]NO_ADDRESS is defined only for
 * compatibility purposes.
 */

/* Authoritative Answer: Host not found */
#define WSAHOST_NOT_FOUND       (WSABASEERR+1001)

/* Non-Authoritative: Host not found, or SERVERFAIL */
#define WSATRY_AGAIN            (WSABASEERR+1002)

/* Non-recoverable errors, FORMERR, REFUSED, NOTIMP */
#define WSANO_RECOVERY          (WSABASEERR+1003)

/* Valid name, no data record of requested type */
#define WSANO_DATA              (WSABASEERR+1004)

/* no address, look for MX record */
#define WSANO_ADDRESS           WSANO_DATA

#endif

#define SVCID_HOSTNAME { 0x0002a800, 0, 0, { 0xC0,0,0,0,0,0,0,0x46 } }

#define NS_ALL			(0)
#define LUP_RETURN_ALL          0x0FF0
#define LUP_FLUSHPREVIOUS    0x2000

#define MEMBLOCKSIZE	2048

/*
 * Winsock 2 Overlapped structures
 */

#define WSAOVERLAPPED           OVERLAPPED
typedef struct _OVERLAPPED *    LPWSAOVERLAPPED;

typedef
void
(CALLBACK * LPWSAOVERLAPPED_COMPLETION_ROUTINE)(
    DWORD dwError,
    DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped,
    DWORD dwFlags
    );

typedef struct _BLOB {
	unsigned long	cbSize;
	BYTE		*pBlobData;
} BLOB;

typedef enum _WSAEcomparator
{
	COMP_EQUAL = 0,
	COMP_NOTLESS
} WSAECOMPARATOR;

typedef struct sockaddr	SOCKADDR;
/* Microsoft Windows Extended data types */
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in *PSOCKADDR_IN;
typedef struct sockaddr_in FAR *LPSOCKADDR_IN;

typedef struct _SOCKET_ADDRESS {
	SOCKADDR	*lpSockaddr;
	int		iSockaddrLength;
} SOCKET_ADDRESS;

typedef struct _CSADDR_INFO {
	SOCKET_ADDRESS	LocalAddr;
	SOCKET_ADDRESS	RemoteAddr;
	int		iSocketType;
	int		iProtocol;
} CSADDR_INFO;

typedef struct _WSAVersion
{
	DWORD           dwVersion;
	WSAECOMPARATOR  ecHow;
}WSAVERSION;

typedef struct _AFPROTOCOLS {
	int	iAddressFamily;
	int	iProtocol;
} AFPROTOCOLS;

typedef struct _WSAQuerySetA
{
	DWORD           dwSize;
	LPSTR           lpszServiceInstanceName;
	GUID            *lpServiceClassId;
	WSAVERSION      *lpVersion;
	LPSTR           lpszComment;
	DWORD           dwNameSpace;
	GUID            *lpNSProviderId;
	LPSTR           lpszContext;
	DWORD           dwNumberOfProtocols;
	AFPROTOCOLS     *lpafpProtocols;
	LPSTR           lpszQueryString;
	DWORD           dwNumberOfCsAddrs;
	CSADDR_INFO     *lpcsaBuffer;
	DWORD           dwOutputFlags;
	BLOB            *lpBlob;
} WSAQUERYSETA;

#define FD_READ_BIT      0
#define FD_WRITE_BIT     1
#define FD_OOB_BIT       2
#define FD_ACCEPT_BIT    3
#define FD_CONNECT_BIT   4
#define FD_CLOSE_BIT     5
#define FD_QOS_BIT       6
#define FD_GROUP_QOS_BIT 7
#define FD_ROUTING_INTERFACE_CHANGE_BIT 8
#define FD_ADDRESS_LIST_CHANGE_BIT 9


#define FD_READ		(1 << 0)
#define FD_WRITE	(1 << 1)
#define FD_OOB		(1 << 2)
#define FD_ACCEPT	(1 << 3)
#define FD_CONNECT	(1 << 4)
#define FD_CLOSE	(1 << 5)
#define FD_QOS		(1 << 6)
#define FD_GROUP_QOS	(1 << 7)
#define FD_ROUTING_INTERFACE_CHANGE (1 << 8)
#define FD_ADDRESS_LIST_CHANGE (1 << 9)

#define FD_MAX_EVENTS    10
#define FD_ALL_EVENTS    ((1 << FD_MAX_EVENTS) - 1)

/*
 * The new type to be used in all
 * instances which refer to winsock sockets.
 */
typedef u_int           SOCKET;

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#define FD_zero(p,n)      memset((char *)(p),(char)0,sizeof(long)*(((n)+(NFDBITS-1))/NFDBITS))

#define _SOL_SOCKET		0xffff
#define _INVALID_SOCKET		INVALID_HANDLE_VALUE
#define _SO_OPENTYPE		0x7008
#define _SO_SYNCHRONOUS_ALERT	0x10
#define _SO_SYNCHRONOUS_NONALERT	0x20
#define SO_BSP_STATE		0x1009	/* from ws2def.h */
#define SO_PROTOCOL_INFO	0x2005	/* from Winsock2.h */

#define WSADESCRIPTION_LEN      256
#define WSASYS_STATUS_LEN       128

/* WinSock 2 extension -- manifest constants for shutdown() */
#define SD_RECEIVE      0x00
#define SD_SEND         0x01
#define SD_BOTH         0x02

/* Handle duplication modes */
#define DUP_INHERIT		1
#define DUP_CLOSEORIG	2

typedef struct
{
	unsigned long	ntpid;
	int		nfds;
	int		data_len;
} Msg_header_t;

typedef struct
{
	Msg_header_t	header;
	struct
	{
		int	fd;
		HANDLE	phandle;
		HANDLE	xhandle;
	}	fd[32];
} Msg_rights_t;


typedef enum tag_WINSOCKVER
{
	WS_NONE = -1,
	WS_UNINITIALIZED = 0,
	WS_11 = 1,
	WS_20 = 2,
	WS_22 = 3,
} WINSOCKVER;

/*
 * WinSock 2 extension -- data type for WSAEnumNetworkEvents()
 */

typedef struct _WSANETWORKEVENTS
{
       long lNetworkEvents;
       int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS, FAR * LPWSANETWORKEVENTS;


/*
 * WinSock 2 extension -- WSABUF and QOS struct
 */

typedef struct _WSABUF {
    u_long      len;     /* the length of the buffer */
    char FAR *  buf;     /* the pointer to the buffer */
} WSABUF, FAR * LPWSABUF;

typedef enum
{
    BestEffortService,
    ControlledLoadService,
    PredictiveService,
    GuaranteedDelayService,
    GuaranteedService
} GUARANTEE;

typedef long int32;

typedef struct _flowspec
{
    int32        TokenRate;              /* In Bytes/sec */
    int32        TokenBucketSize;        /* In Bytes */
    int32        PeakBandwidth;          /* In Bytes/sec */
    int32        Latency;                /* In microseconds */
    int32        DelayVariation;         /* In microseconds */
    GUARANTEE    LevelOfGuarantee;       /* Guaranteed, Predictive */
                                         /*   or Best Effort       */
    int32        CostOfCall;             /* Reserved for future use, */
                                         /*   must be set to 0 now   */
    int32        NetworkAvailability;    /* read-only:         */
                                         /*   1 if accessible, */
                                         /*   0 if not         */
} FLOWSPEC, FAR * LPFLOWSPEC;

typedef struct _QualityOfService
{
    FLOWSPEC      SendingFlowspec;       /* the flow spec for data sending */
    FLOWSPEC      ReceivingFlowspec;     /* the flow spec for data receiving */
    WSABUF        ProviderSpecific;      /* additional provider specific stuff */
} QOS, FAR * LPQOS;

/*
 * WinSock 2 extension -- WSAPROTOCOL_INFO structure and associated
 * manifest constants
 */

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID
{
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
#endif /* GUID_DEFINED */

#ifndef __LPGUID_DEFINED__
#define __LPGUID_DEFINED__
typedef GUID *LPGUID;
#endif

#define MAX_PROTOCOL_CHAIN 7

#define BASE_PROTOCOL      1
#define LAYERED_PROTOCOL   0

typedef struct _WSAPROTOCOLCHAIN {
    int ChainLen;                                 /* the length of the chain,     */
                                                  /* length = 0 means layered protocol, */
                                                  /* length = 1 means base protocol, */
                                                  /* length > 1 means protocol chain */
    DWORD ChainEntries[MAX_PROTOCOL_CHAIN];       /* a list of dwCatalogEntryIds */
} WSAPROTOCOLCHAIN, FAR * LPWSAPROTOCOLCHAIN;

#define WSAPROTOCOL_LEN  255

typedef struct _WSAPROTOCOL_INFOA {
    DWORD dwServiceFlags1;
    DWORD dwServiceFlags2;
    DWORD dwServiceFlags3;
    DWORD dwServiceFlags4;
    DWORD dwProviderFlags;
    GUID ProviderId;
    DWORD dwCatalogEntryId;
    WSAPROTOCOLCHAIN ProtocolChain;
    int iVersion;
    int iAddressFamily;
    int iMaxSockAddr;
    int iMinSockAddr;
    int iSocketType;
    int iProtocol;
    int iProtocolMaxOffset;
    int iNetworkByteOrder;
    int iSecurityScheme;
    DWORD dwMessageSize;
    DWORD dwProviderReserved;
    CHAR   szProtocol[WSAPROTOCOL_LEN+1];
} WSAPROTOCOL_INFOA, FAR * LPWSAPROTOCOL_INFOA;

typedef WSAPROTOCOL_INFOA WSAPROTOCOL_INFO;
typedef LPWSAPROTOCOL_INFOA LPWSAPROTOCOL_INFO;

/* Flag bit definitions for dwProviderFlags */
#define PFL_MULTIPLE_PROTO_ENTRIES          0x00000001
#define PFL_RECOMMENDED_PROTO_ENTRY         0x00000002
#define PFL_HIDDEN                          0x00000004
#define PFL_MATCHES_PROTOCOL_ZERO           0x00000008

/* Flag bit definitions for dwServiceFlags1 */
#define XP1_CONNECTIONLESS                  0x00000001
#define XP1_GUARANTEED_DELIVERY             0x00000002
#define XP1_GUARANTEED_ORDER                0x00000004
#define XP1_MESSAGE_ORIENTED                0x00000008
#define XP1_PSEUDO_STREAM                   0x00000010
#define XP1_GRACEFUL_CLOSE                  0x00000020
#define XP1_EXPEDITED_DATA                  0x00000040
#define XP1_CONNECT_DATA                    0x00000080
#define XP1_DISCONNECT_DATA                 0x00000100
#define XP1_SUPPORT_BROADCAST               0x00000200
#define XP1_SUPPORT_MULTIPOINT              0x00000400
#define XP1_MULTIPOINT_CONTROL_PLANE        0x00000800
#define XP1_MULTIPOINT_DATA_PLANE           0x00001000
#define XP1_QOS_SUPPORTED                   0x00002000
#define XP1_INTERRUPT                       0x00004000
#define XP1_UNI_SEND                        0x00008000
#define XP1_UNI_RECV                        0x00010000
#define XP1_IFS_HANDLES                     0x00020000
#define XP1_PARTIAL_MESSAGE                 0x00040000

#define BIGENDIAN                           0x0000
#define LITTLEENDIAN                        0x0001

#define SECURITY_PROTOCOL_NONE              0x0000

/*
 * WinSock 2 extension -- manifest constants for WSAJoinLeaf()
 */
#define JL_SENDER_ONLY    0x01
#define JL_RECEIVER_ONLY  0x02
#define JL_BOTH           0x04

/*
 * WinSock 2 extension -- manifest constants for WSASocket()
 */
#define WSA_FLAG_OVERLAPPED           0x01
#define WSA_FLAG_MULTIPOINT_C_ROOT    0x02
#define WSA_FLAG_MULTIPOINT_C_LEAF    0x04
#define WSA_FLAG_MULTIPOINT_D_ROOT    0x08
#define WSA_FLAG_MULTIPOINT_D_LEAF    0x10

/*
 * U/WIN specific constant.
 *
 * The following constant used in oflag to indicate that the socket
 * has WSA_FLAG_MULTIPOINT_C_LEAF and WSA_FLAG_MULTIPOINT_D_LEAF attributes
 * set.  Its value is the same as O_DIRECT.
 */
#define MULTICAST_SET    0x10000

/*
 * WinSock 2 extension -- manifest constants for WSAIoctl()
 */
#define IOC_UNIX                      0x00000000
#define IOC_WS2                       0x08000000
#define IOC_PROTOCOL                  0x10000000
#define IOC_VENDOR                    0x18000000

#define _WSAIO(x,y)                   (IOC_VOID|(x)|(y))
#define _WSAIOR(x,y)                  (IOC_OUT|(x)|(y))
#define _WSAIOW(x,y)                  (IOC_IN|(x)|(y))
#define _WSAIORW(x,y)                 (IOC_INOUT|(x)|(y))

#define SIO_ASSOCIATE_HANDLE          _WSAIOW(IOC_WS2,1)
#define SIO_ENABLE_CIRCULAR_QUEUEING  _WSAIO(IOC_WS2,2)
#define SIO_FIND_ROUTE                _WSAIOR(IOC_WS2,3)
#define SIO_FLUSH                     _WSAIO(IOC_WS2,4)
#define SIO_GET_BROADCAST_ADDRESS     _WSAIOR(IOC_WS2,5)
#define SIO_GET_EXTENSION_FUNCTION_POINTER  _WSAIORW(IOC_WS2,6)
#define SIO_GET_QOS                   _WSAIORW(IOC_WS2,7)
#define SIO_GET_GROUP_QOS             _WSAIORW(IOC_WS2,8)
#define SIO_MULTIPOINT_LOOPBACK       _WSAIOW(IOC_WS2,9)
#define SIO_MULTICAST_SCOPE           _WSAIOW(IOC_WS2,10)
#define SIO_SET_QOS                   _WSAIOW(IOC_WS2,11)
#define SIO_SET_GROUP_QOS             _WSAIOW(IOC_WS2,12)
#define SIO_TRANSLATE_HANDLE          _WSAIORW(IOC_WS2,13)

/*
 * WinSock 2 extension -- manifest constants for SIO_TRANSLATE_HANDLE ioctl
 */
#define TH_NETDEV        0x00000001
#define TH_TAPI          0x00000002

/*
 * Microsoft Windows Extended data types required for the functions to
 * convert   back  and  forth  between  binary  and  string  forms  of
 * addresses.
 */
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr *PSOCKADDR;
typedef struct sockaddr FAR *LPSOCKADDR;

/*
 * Manifest constants and type definitions related to name resolution and
 * registration (RNR) API
 */

typedef struct WSAData
{
        WORD                    wVersion;
        WORD                    wHighVersion;
        char                    szDescription[WSADESCRIPTION_LEN+1];
        char                    szSystemStatus[WSASYS_STATUS_LEN+1];
        unsigned short          iMaxSockets;
        unsigned short          iMaxUdpDg;
        char *              	lpVendorInfo;
} WSADATA;

/*
 * WinSock 2 extension -- data type and manifest constants for socket groups
 */
typedef unsigned int             GROUP;

#define SG_UNCONSTRAINED_GROUP   0x01
#define SG_CONSTRAINED_GROUP     0x02

/*
 *	Winsock 1.1 control thread data and structures.
 *
 */

#define FDS_READ		1
#define FDS_WRITE		2
#define FDS_EXCEPTION	4

#define ACK_NONE	-1
#define ACK_OK		1
#define ACK_BAD		2

#define MAX_WS_11_SELECT_FAILURES	5

/*
 * Winsock select uses arrays of SOCKETs.  These macros manipulate such
 * arrays.
 *
 * CAVEAT IMPLEMENTOR: THESE MACROS AND TYPES COME FROM
 * WINSOCK.H AND MUST BE EXACTLY AS SHOWN HERE.	THEIR NAMES HAVE BEEN
 * CHANGED TO AVOID CONFLICTS WITH SIMILAR STRUCTURES AND MACROS DEFINED
 * ELSEWHERE FOR U/WIN.
 *
 * There is one function, __WSAFDIsSet(), which will be loaded from wsock32.dll
 * during winsock initialization time, if winsock 1.1 is used.  This function is
 * not documented in the online documentation from Microsoft, but it is declared in
 * both winsock.h and winsock2.h.
 */

typedef struct fd_set_WS_11 {
        u_int   fd_count;               /* how many are SET? */
        SOCKET  fd_array[FD_SETSIZE];   /* an array of SOCKETs */
} fd_set_WS_11;

#define FD_CLR_WS_11(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_set_WS_11 FAR *)(set))->fd_count ; __i++) { \
        if (((fd_set_WS_11 FAR *)(set))->fd_array[__i] == fd) { \
            while (__i < ((fd_set_WS_11 FAR *)(set))->fd_count-1) { \
                ((fd_set_WS_11 FAR *)(set))->fd_array[__i] = \
                    ((fd_set_WS_11 FAR *)(set))->fd_array[__i+1]; \
                __i++; \
            } \
            ((fd_set_WS_11 FAR *)(set))->fd_count--; \
            break; \
        } \
    } \
} while(0)

#define FD_SET_WS_11(fd, set) do { \
    if (((fd_set_WS_11 FAR *)(set))->fd_count < FD_SETSIZE) \
        ((fd_set_WS_11 FAR *)(set))->fd_array[((fd_set_WS_11 FAR *)(set))->fd_count++]=(fd);\
} while(0)

#define FD_ZERO_WS_11(set) (((fd_set_WS_11 FAR *)(set))->fd_count=0)

#define FD_ISSET_WS_11(fd, set) (*s__WSAFDIsSet)((SOCKET)(fd), (fd_set_WS_11 FAR *)(set))

/*
 * END WINSOCK 1.1 MACRO DEFINITIONS
 */

/*
 * Winsock 1.1 constrol structures
 */

typedef struct
{
	int   fd;		/* INET socket file descriptor */
	int   sets;		/* Bit map indicating which fd_set(s) to associate with this socket. */
	long  netevents;	/* Bit map for set of network events interested in */
	int   result_sets;	/* fd set(s) that were set by select(). */

} Control_fd_info;

typedef struct
{
	int fdcount;
	Control_fd_info controltab[FD_SETSIZE];
} Control_data;

typedef struct tag_unix_win32_iffmap
{
	int ifunix;
	int sowin32;
} unix_win32_iffmap;

#endif
