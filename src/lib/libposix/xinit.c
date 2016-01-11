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
#if _X86_
#define btowc		___btowc
#define mblen		___mblen
#define mbtowc		___mbtowc
#define mbrtowc		___mbrtowc
#define mbstowcs	___mbstowcs
#define wctomb		___wctomb
#define wcrtomb		___wcrtomb
#define wcstombs	___wcstombs
#endif

#define strtod		___strtod
#define atof		___atof

#include "uwin_init.h"
#include "uwindef.h"
#include "uwin_serve.h"
#include "ipchdr.h"
#include "mnthdr.h"
#include "procdir.h"
#include "fatlink.h"
#include <glob.h>
#include <locale.h>
#include <mmsystem.h>
#include "ident.h"
#ifdef GTL_TERM
#include "raw.h"
#endif

#ifndef LOG_INSTALL
#define LOG_INSTALL	2	/* state.install default debug level */
#endif

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION	0x1000
#endif

/*
 * the following is for initialization in case the library
 * is used without wmain.c
 */

short *pctype;				/* used by setlocale() */
static char _Sfstdin[SFIO_T_SPACE];
static char _Sfstdout[SFIO_T_SPACE];
static char _Sfstderr[SFIO_T_SPACE];
static int  tmp_herrno;			/* until  dllinfo is initialized */
static int  tmp_daylight;
static int first_process;
static int use_vmalloc;
static int console = -1;
static int curdir;
static int exit_done = 0;
#define TERM_REINIT		(-2)

static char standard_time[TZNAME_MAX] = "EST";
static char daylight_time[TZNAME_MAX] = "EDT";

static char             *tzname[2] =
{
	standard_time,
	daylight_time
};

#undef timezone
long	timezone;

void *xxaddr[10];

static SECURITY_ATTRIBUTES Sattr[3];

HANDLE		Ih[4];
struct _astdll	dllinfo;
DWORD		Ntpid;
Pproc_t		*P_CP;
Pproc_t		*Pproctab;
short		*Pprochead;
short		*Pprocnthead;
Pfd_t 		*Pfdtab;
Pfifo_t 	*Pfifotab;
unsigned short 	*Pshmtab;
Psid_t		*Psidtab;
unsigned short	*Blocktype;
unsigned long	*Generation;
Uwin_share_t	*Share;
int 		*errno_addr;
char		***environ_addr;
char		**saved_env;
char		*saved_env0;
int		proc_exec_exit;
DWORD		main_thread;
int		crit_init;
void		*malloc_first_addr;
void		*(*clib_malloc)(size_t);

State_t		state;

extern void init_modem(Devtab_t*);
extern void init_devtty(Devtab_t*);
extern void init_devpty(Devtab_t*);
extern void init_tty(Devtab_t*);
extern void init_pty(Devtab_t*);
extern void init_tape(Devtab_t*);
extern void init_lp(Devtab_t*);
extern void init_clipboard(Devtab_t*);
extern void init_console(Devtab_t*);
extern void init_devfd(Devtab_t*);
extern void init_audio(Devtab_t*);
extern void init_dev_mem(Devtab_t*);

#define NCRITICAL	10

static CRITICAL_SECTION csection[NCRITICAL];
/*
 * the entry index is the device minor number
 * add new devices at the end to preserve minor numbers
 */
Devtab_t Blockdev[] =
{
	{ "disk", 6, FDTYPE_FILE, 0, NULL, "\\\\.\\PhysicalDrive%d" },
	{ "tape", 128, FDTYPE_TAPE, 1, init_tape, "" },
	{ "cdrom", 4, FDTYPE_FILE, 0, NULL, "\\\\.\\CdRom%d" },
};

/*
 * the entry index is the device minor number
 * add new devices at the end to preserve minor numbers
 */
Devtab_t Chardev[] =
{
	{ "null", 1, FDTYPE_NULL, 0, NULL, "nul" },
	{ "tty", 256, FDTYPE_TTY, 1, init_tty, "" },
	{ "console", 256, FDTYPE_CONSOLE, 1, init_console, "" },
	{ "pty", 256, FDTYPE_PTY, 1, init_pty,"" },
	{ "window", 1, FDTYPE_WINDOW, 0, NULL, "" },
	{ "clipboard", 1, FDTYPE_CLIPBOARD,0, init_clipboard, "" },
	{ "floppy", 26, FDTYPE_FLOPPY,0, NULL, "\\\\.\\%c:" },
	{ "ttyclone", 1, FDTYPE_TTY, 0, init_devtty, "" },
	{ "ptyclone", 1, FDTYPE_PTY, 0, init_devpty, "" },
	{ "modem", 8, FDTYPE_MODEM,1, 	init_modem, "com%d" },
	{ "fd", OPEN_MAX, FDTYPE_DEVFD, 0, init_devfd, "" },
	{ "printer", 8, FDTYPE_LP, 0, init_lp, "lpt%d:" },
	{ "zero", 1, FDTYPE_ZERO, 0, NULL, "" },
	{ "audio", 8, FDTYPE_AUDIO, 0, init_audio, "" },
	{ "random", 2, FDTYPE_RANDOM, 0, NULL, "nul" },
	{ "memory", 2, FDTYPE_MEM, 0, init_dev_mem, "" },
	{ "full", 1, FDTYPE_FULL, 0, NULL, "" },
};

#define FILE_type(x)	((x==FILE_TYPE_DISK?FDTYPE_FILE:(x==FILE_TYPE_PIPE?FDTYPE_PIPE:FDTYPE_CONSOLE)))

static char *fixpid(register char *ename)
{
	static char pidvar[20];
	pid_t pid;
	pid = atoi(&ename[4]);
	if(pid == P_CP->pid)
		return(ename);
	sfsprintf(pidvar,sizeof(pidvar),"PID=%d",P_CP->pid);
	return(pidvar);
}

static char* fixpath(register char* ename)
{
	char*		cp;
	char*		dp;
	char*		ep;
	Mount_t*	mp;
	int		c;
	int		i;
	int		m;
	int		n;
	int		bin;
	int		sys;

	/* really should be dynamic */
	static int	first;
	static char	upath[8*PATH_MAX];

	if (!upath[first])
	{
		memcpy(&upath[first = i = 9], "PATH=", 5);
		sys = bin = !!(P_CP->posixapp&POSIX_PARENT);
		i += 5;
		cp = &ename[5];
		for (;;)
		{
			if (ep = strchr(cp, ';'))
				*ep = 0;
			if (*cp)
			{
				if (ep)
					n = (int)(ep - cp);
				else
					n = (int)strlen(cp);
				if (*cp=='"' && cp[n-1]=='"')
				{
					n -= 2;
					cp++;
				}
				if (mp = mount_look(cp, n, 1, 0))
					cp = mp->path;
				dp = &upath[i];
				c = cp[m = n];
				cp[n] = 0;
				n = uwin_pathmap(cp, dp, sizeof(upath) - i, UWIN_W2U);
				cp[m] = c;
				i += (int)strlen(dp);
				if (!sys && !_stricmp(dp, "/sys"))
					sys = 1;
				else if (!bin && (!_stricmp(dp,"/bin") || !_stricmp(dp,"/usr/bin")))
					bin = 1;
			}
			if (!ep)
				break;
			upath[i++] = ':';
			*ep = ';';
			cp = ep + 1;
		}
		/* append system directory if not there already */
		if (!sys)
			strcpy(&upath[i], ":/sys");
		/* prepend /usr/bin directory if not there already */
		if (state.install)
			memcpy(&upath[first = 6], "PATH=/.:", 8);
		else if (!bin)
			memcpy(&upath[first = 0], "PATH=/usr/bin:", 14);
	}
	return &upath[first];
}

static char astconfig[] = "_AST_FEATURES=PLATFORM - 32/32 UNIVERSE = att PATH_LEADING_SLASHES - 1 PATH_RESOLVE - metaphysical";
#define ASTCONFIG_EQ			13
#define ASTCONFIG_PLATFORM_VERIFY	17
#define ASTCONFIG_PLATFORM_UWIN		25
#define ASTCONFIG_PLATFORM_WINDOWS	28

/*
 * fix up PATH and DOSPATHVARS in new and old environments
 * old environment used when main is invoked with three arguments
 */
static void fixenv(char ***envp, char **oenv)
{
	register char *ename;
	char **env = *envp, *dospath=0, *buff=0, *buffmax, *ast_features=0;
	register int i,n;
	int fix_pid = 0, pre_load = 0, path_var= -1;
	while(*env && **env=='=')
	{
		env++;
		oenv++;
	}
	*envp = env;

	for (i = 0; ename = env[i]; i++)
	{
		if(!ast_features && ename[0]=='_' && ename[1]=='A' && strncmp(ename,"_AST_FEATURES=",14)==0)
			ast_features = ename;
		if(!dospath && (ename[0]=='D') && (ename[1]=='O') && (ename[2]=='S') && memcmp(ename,"DOSPATHVARS=",12)==0)
		{
			dospath = &ename[12];
			i = sysconf(_SC_ARG_MAX);
			buff = arg_getbuff();
			buffmax = buff + i;
			i = 0;
			continue;
		}
		if(dospath && env_inlist(ename,dospath))
		{
			*((char**)buff) = ename;
			buff += sizeof(char*);
			n = env_convert(ename, buff, (int)(buffmax-buff), UWIN_W2U);
			oenv[i] = env[i] = buff;
			buff += roundof(n,sizeof(char*));
		}
		if(ename[0]=='=' && ename[1] =='P' && ename[2]=='A' &&
			ename[3]=='T' && ename[4]=='H' && ename[5]=='=')
			path_var = i;
		else if (((ename[0] == 'P')||(ename[0]=='p')) &&
		 	((ename[1] == 'A')||(ename[1]=='a')) &&
			((ename[2] == 'T')||(ename[2]=='t')) &&
			((ename[3] == 'H')||(ename[3]=='h')) && (ename[4] == '='))
		{
			if(path_var>=0)
				env[i] = env[path_var]+1;
			else
				env[i] = fixpath(ename);
			oenv[i] = env[i];
		}
		else if(!fix_pid && ename[0]=='P' && ename[1]=='I' && ename[2]=='D' && ename[3]=='=')
		{
			oenv[i] = env[i] = fixpid(ename);
			fix_pid = 1;
		}
		else if(!pre_load && ename[0]=='L' && ename[1]=='D' && ename[2]=='_' && memcmp(&ename[3],"PRELOAD=",8)==0)
		{
			if(!LoadLibraryEx("preload10.dll",0,0))
				logerr(0, "LoadLibraryEx");
			pre_load = 1;
		}
	}
	if(path_var>=0)
	{
		/* delete _PATH= */
		env[path_var] = env[--i];
		env[i] = 0;
	}
	if(buff)
		arg_setbuff(buff);
	if(!ast_features)
	{
		env[i] = astconfig;
		logmsg(LOG_PROC+5, "fixenv initialize %s", astconfig);
	}
	else
		logmsg(LOG_PROC+5, "fixenv inherit %s", ast_features);
}

static void init_pipetimes(void)
{
	FILETIME *fp = (FILETIME*)block_ptr(Share->pipe_times);
	FILETIME *fplast = (FILETIME*)((char*)fp+BLOCK_SIZE);
	while((fp+2) <= fplast)
	{
		*((long*)fp) = -1;
		fp+=2;
	}
}

/*
 * initialize device memory
 */
static void init_devmem(void)
{
	Devtab_t *dp;
	int i;
	short *sp = (unsigned short*)block_ptr(Share->blockdev_index);
	Share->blockdev_size = elementsof(Blockdev);
	for(i=0; i < elementsof(Blockdev) ; i++)
	{
		dp = &Blockdev[i];
		if(dp->vector)
			sp[i] = block_alloc(BLK_VECTOR);
	}
	sp = (unsigned short*)block_ptr(Share->chardev_index);
	Share->chardev_size = elementsof(Chardev);
	for(i=0; i < elementsof(Chardev) ; i++)
	{
		dp = &Chardev[i];
		if(dp->vector)
			sp[i] = block_alloc(BLK_VECTOR);
	}
}

struct Tblsize
{
	unsigned short sid_max;
	unsigned short file_max;
	unsigned short proc_max;
	unsigned short fifo_max;
	unsigned short child_max;
	unsigned short affinity;
	unsigned short case_sensitive;
	unsigned short no_shamwow;
	unsigned short update_immediate;
};

void begin_critical(int n)
{
	if(!crit_init)
	{
		register int i;
		crit_init = 1;
		ZeroMemory((void*)csection,sizeof(csection));
		for(i=0; i < NCRITICAL; i++)
			InitializeCriticalSection(&csection[i]);
	}
	EnterCriticalSection(&csection[n>>1]);
}
void end_critical(int n)
{
	LeaveCriticalSection(&csection[n>>1]);
}

SECURITY_ATTRIBUTES *sattr(int inherit)
{
	return(&Sattr[inherit]);
}

/*
 *	get configuration limits from registry
 */
static void ipc_getlimits(Uwin_ipclim_t *ip, struct Tblsize *tp, OSVERSIONINFO* os)
{
	DWORD		type;
	DWORD		data;
	char		ipc_str[256];

	DWORD		datalen = sizeof(DWORD);
	HKEY		ipckey = 0;
	HKEY		reskey = 0;

	/*
	 * Fill the ip structure with default values
	 */

	ip->msgmaxids = MSGMNI;
	ip->msgmaxqsize = MSGMNB;
	ip->msgmaxsize = MSGMAX;

	ip->semmaxids = SEMMNI;
	ip->semmaxsemperid = SEMMSL;
	ip->semmaxinsys = ip->semmaxids*ip->semmaxsemperid;
	ip->semmaxopspercall = SEMOPM;
	ip->semmaxval = SEMVMX;

	ip->shmmaxids = SHMMNI;
	ip->shmmaxsegperproc = SHMSEG;
	ip->shmmaxsize = SHMMAX;
	ip->shmminsize = SHMMIN;

	tp->sid_max = MAXSID;
	tp->proc_max = PROCTABSIZE;
	tp->file_max = FDTABSIZE;
	tp->fifo_max = FIFOTABSIZE;
	tp->child_max = CHILD_MAX;
	tp->affinity = 0;
	tp->case_sensitive = 0;
	tp->update_immediate = 0;
	tp->no_shamwow = os->dwPlatformId < VER_PLATFORM_WIN32_NT || os->dwPlatformId == VER_PLATFORM_WIN32_NT && ((os->dwMajorVersion << 8) | os->dwMinorVersion) <= 0x0600;
	if (!state.standalone)
	{
		sfsprintf(ipc_str, sizeof(ipc_str), "%s\\%s\\%s", UWIN_KEY, state.rel, UWIN_KEY_RES);
		/* open resource key */
		if(RegOpenKeyEx(state.key, ipc_str, 0, KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS|KEY_WOW64_64KEY, &reskey))
			goto skip;
		/* fill in with resources parameter settings from registry */
		if(!RegQueryValueEx(reskey, "ChildMax", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->child_max = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "FifoMax", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->fifo_max = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "FileMax", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->file_max = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "Affinity", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->affinity = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "CaseSensitive", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->case_sensitive = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "NoShamWoW", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD && data)
			tp->no_shamwow = (unsigned short)data;
		if(!RegQueryValueEx(reskey, "UpdateImmediate", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->update_immediate = (unsigned short)data;
		if (!RegQueryValueEx(reskey, "ProcMax", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
		{
			register int i;
			tp->proc_max = (unsigned short)data;
			/* make a power of 2 > 256 <= 32K */
			if(tp->proc_max > (1<<15))
				tp->proc_max = (1<<15);
			else
				for(i=8; i < 15; i++)
					if((1<<i) > tp->proc_max)
					{
						tp->proc_max = (1<<i);
						break;
					}
		}
		if(!RegQueryValueEx(reskey, "SidMax", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			tp->sid_max = (unsigned short)data;
 skip:
		/* Open IPC key */
		sfsprintf(ipc_str, sizeof(ipc_str), "%s\\%s\\IPC", UWIN_KEY, state.rel);
		if(RegOpenKeyEx(state.key, ipc_str, 0, KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS|KEY_WOW64_64KEY, &ipckey))
			goto err;
		/* Query the Registry for Message queue values */
		if(!RegQueryValueEx(ipckey, "MsgMaxIds", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->msgmaxids = data;
		if(!RegQueryValueEx(ipckey, "MsgMaxQSize", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->msgmaxqsize = data;
		if(!RegQueryValueEx(ipckey, "MsgMaxSize", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->msgmaxsize = data;

		/* Query the Registry for Semaphore values */
		if(!RegQueryValueEx(ipckey, "SemMaxIds", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->semmaxids = data;
		if(!RegQueryValueEx(ipckey, "SemMaxInSys", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->semmaxinsys = data;
		if(!RegQueryValueEx(ipckey, "SemMaxOpsPerCall", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->semmaxopspercall = data;
		if(!RegQueryValueEx(ipckey, "SemMaxSemPerId", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->semmaxsemperid = data;
		if(!RegQueryValueEx(ipckey, "SemMaxVal", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->semmaxval = data;

		/* Query the Registry for Shared memory values */
		if(!RegQueryValueEx(ipckey, "ShmMaxIds", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->shmmaxids = data;
		if(!RegQueryValueEx(ipckey, "ShmMaxSegPerProc", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->shmmaxsegperproc = data;
		if(!RegQueryValueEx(ipckey, "ShmMaxSize", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->shmmaxsize = data;
		if(!RegQueryValueEx(ipckey, "ShmMinSize", NULL, &type,(LPBYTE)&data, &datalen) && type == REG_DWORD)
			ip->shmminsize = data;
		logmsg(LOG_INIT+1, "uwin %s platformid=%d majorversion=%d minorversion=%d noshamwow=%d", state.rel, os->dwPlatformId, os->dwMajorVersion, os->dwMinorVersion, tp->no_shamwow);
	}
 err:
	if(reskey)
		RegCloseKey(reskey);
	if(ipckey)
		RegCloseKey(ipckey);
}

static int pid_shift(void)
{
	register int i,j;
	register HANDLE hp;
	for(j=0; j<2; j++)
	{
		for(i=j+1; i <= 500;i+=4)
		{
			if(hp = OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,(DWORD)1))
				goto found;
			if(GetLastError()!= ERROR_INVALID_PARAMETER)
				goto found;
		}
	}
found:
	if(hp)
		closehandle(hp,HT_PROC);
	return(j);
}

#define DESIRED_ADDRESS	((void*)0x5000000)

/*
 * set up the global state
 * return -1:error 0:success
 */

static int init_state(int* debug)
{
	HANDLE				hp;
	HKEY				key;
	char*				s;
	char*				t;
	char*				v;
	char*				x;
	int				i;
	int				type;
	int				n;
	BOOL				b;
	MEMORY_BASIC_INFORMATION	minfo;
	Uwin_ipclim_t			ipclim;
	struct Tblsize			tblsiz;
	OSVERSIONINFO			os;
	char				old[PATH_MAX];
	char				buf[2*PATH_MAX];
	char				mem[PATH_MAX];
	char				mode[64];

	unsigned int			fifosize = 2048;
	int				rooted = 1;

	ZeroMemory((void*)&state, sizeof(state));

	/* common security attributes */

	ZeroMemory((void*)Sattr, sizeof (Sattr));
	Sattr[0].nLength = Sattr[1].nLength = sizeof(Sattr[0]);
	Sattr[0].nLength = Sattr[1].nLength = Sattr[2].nLength = sizeof(Sattr[0]);
	Sattr[0].lpSecurityDescriptor = Sattr[1].lpSecurityDescriptor = nulldacl();
	Sattr[2].lpSecurityDescriptor = 0;
	Sattr[0].bInheritHandle = FALSE;
	Sattr[1].bInheritHandle = Sattr[2].bInheritHandle = TRUE;

	/* /sys native path */

	GetSystemDirectory(state.sys, sizeof(state.sys));

	/* release */

	state.key = HKEY_LOCAL_MACHINE;
	if (!state.rel[0])
		strcpy(state.rel, RELEASE_STRING);

	/* shamwow */

	if (state.shamwowed = (Shamwowed_f)getsymbol(MODULE_kernel, "IsWow64Process"))
	{
		if (!state.shamwowed(GetCurrentProcess(), &b))
			b = 0;
		if (b || sizeof(char*) == 8)
		{
			state.wow = b;
			state.wowx86 = GetFileAttributes("C:\\Program Files (x86)") != INVALID_FILE_ATTRIBUTES;
		}
		else
			state.shamwowed = 0;
	}

	/* kernel resource name prefix */

	strcpy(mode, "standard");
	strcpy(state.resource, UWIN_SHARE_GLOBAL UWIN_SHARE_DOMAIN);
	state.local = state.resource + sizeof(UWIN_SHARE_GLOBAL) - 1;

	/* optional posix.ini */

	*debug = -1;
	if (!VirtualQuery((void*)&state, &minfo, sizeof(minfo)) == sizeof(minfo) || !GetModuleFileName(minfo.AllocationBase, state.dll, sizeof(state.dll)))
		strcpy(state.dll, "/sys/" POSIX_DLL ".dll");
	else if ((x = strrchr(state.dll, '.')) && _stricmp(x, ".dll") == 0)
	{
		strcpy(x, ".ini");
		hp = CreateFileA(state.dll, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		strcpy(x, ".dll");
		if (hp && hp != INVALID_HANDLE_VALUE)
		{
			i = ReadFile(hp, buf, (DWORD)sizeof(buf)-1, &n, NULL);
			CloseHandle(hp);
			if (i && n > 2)
			{
				/*
				 * space, ',' or '\n' separated [no]name[=value]
				 *
				 *	clone
				 *	debug=<log-message-debug-level>
				 *	home=<home-directory-dos-path>
				 *	install
				 *	root=<root-directory-dos-path>
				 *	standalone=<shared-memory-name>
				 *	wow
				 */

				buf[n] = 0;
				t = buf;
				while (*t)
				{
					while (*t == ',' || *t == ' ' || *t == '\t' || *t == '\r' || *t == '\n')
						t++;
					if (*t == '#')
					{
						while (*++t && *t != '\n');
						continue;
					}
					if (!*t)
						break;
					i = 0;
					v = 0;
					for (s = t; *t && (i || *t != ',' && *t != ' ' && *t != '\t' && *t != '\r' && *t != '\n'); t++)
						if (!v && *t == '=')
						{
							*t = 0;
							v = t + 1;
							if (*v == '\'' || *v == '"')
							{
								i = *v++;
								t++;
							}
						}
						else if (v && *t == i)
						{
							*t = 0;
							i = 0;
						}
					if (*t)
					{
						*t++ = 0;
						while (*t == ' ' || *t == '\t' || *t == '\r' || *t == '\n')
							t++;
					}
					if (*s == 'n' && *(s + 1) == 'o')
					{
						s += 2;
						n = 0;
					}
					else
						n = 1;
					if (streq(s, "clone"))
						state.clone.on = n;
					else if (streq(s, "debug"))
					{
						if (n && v)
							n = atoi(v);
						*debug = n;
					}
					else if (streq(s, "home"))
					{
						if (n && v && *v && strlen(v) < sizeof(state.home))
							strcpy(state.home, v);
					}
					else if (streq(s, "install"))
					{
						if ((state.install = n) || n && streq(s, "standalone"))
						{
							if (!v || !*v)
								v = s;
							if (!state.install && streq(v, "install"))
								state.install = 1;
							if (strlen(v) >= sizeof(mode))
								v[sizeof(mode)-1] = 0;
							strcpy(mode, v);
							state.standalone = 1;
							sfsprintf(state.resource, sizeof(state.resource), "%s.%s", UWIN_SHARE_DOMAIN, v);
							state.local = state.resource;
						}
					}
					else if (streq(s, "install") && (state.install = n) || streq(s, "standalone"))
					{
						if (n)
						{
							if (!v || !*v)
								v = s;
							if (!state.install && streq(v, "install"))
								state.install = 1;
							if (strlen(v) >= sizeof(mode))
								v[sizeof(mode)-1] = 0;
							strcpy(mode, v);
							state.standalone = 1;
							sfsprintf(state.resource, sizeof(state.resource), "%s.%s", UWIN_SHARE_DOMAIN, v);
							state.local = state.resource;
						}
					}
					else if (streq(s, "root"))
					{
						if (n && v && *v && strlen(v) < sizeof(state.root))
							strcpy(state.root, v);
					}
					else if (streq(s, "wow"))
						state.wow = state.wowx86 = n;
#ifdef TEST_SUD
					else if (streq(s, "sud"))
					{
						if (n)
							state.test |= TEST_SUD;
						else
							state.test &= ~TEST_SUD;
					}
#endif
				}
				if (state.install)
					state.install = *debug >= 0 ? *debug : LOG_INSTALL;
				if (state.standalone && !state.root[0])
				{
					/* by convention the standalone root is the directory of POSIX_DLL */

					rooted = 0;
					while (x > state.dll)
						if (*--x == '\\' || *x == '/')
						{
							while (x > state.dll && *(x - 1) == *x)
								x--;
							if (x > state.dll && (x - state.dll + 1) < sizeof(state.root))
								memcpy(state.root, state.dll, x - state.dll);
							break;
						}
				}
			}
		}
	}

	/* check if shared mem map exists */

	sfsprintf(mem, sizeof(mem), "%s%s", state.resource, UWIN_SHARE);
	if (!(state.map = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, mem)))
		state.init = LOG_INIT;

	/* root path */

	if (!state.standalone)
	{
		sfsprintf(buf, sizeof(buf), "%s\\%s\\%s", UWIN_KEY, state.rel, UWIN_KEY_INST);
		if (!RegOpenKeyEx(state.key, buf, 0, KEY_QUERY_VALUE|KEY_WOW64_64KEY, &key))
		{
			n = sizeof(state.root);
			if (RegQueryValueEx(key, UWIN_KEY_ROOT, 0, &type, state.root, &n) || type != REG_SZ)
				state.root[0] = 0;
			n = sizeof(state.home);
			if (RegQueryValueEx(key, UWIN_KEY_HOME, 0, &type, state.home, &n) || type != REG_SZ)
				state.home[0] = 0;
			RegCloseKey(key);
		}
	}
	if (!state.root[0])
	{
		s = GetCommandLine();
		if (t = strrchr(s, '\\'))
		{
			if (*s == '"')
				s++;
			while (--t > s && *t != '\\');
			if (t - 4 > s)
			{
				t -= 4;
				if (t[0] == '\\' && (t[1] == 'u' || t[1] == 'U') && (t[2] == 's' || t[2] == 'S') && (t[3] == 'r' || t[3] == 'R'))
				{
					memcpy(state.root, s, t - s);
					state.root[t - s] = 0;
				}
			}
		}
		if (!state.root[0])
			strcpy(state.root, UWIN_DEF_ROOT);
	}
	state.rootlen = (int)strlen(state.root);
	if (!state.home[0])
		strcpy(state.home, UWIN_DEF_HOME);
	state.homelen = (int)strlen(state.root);
#if _X64_
	sfsprintf(buf, sizeof(buf), "%s\\u32", state.root);
	state.wow32 = GetFileAttributes(buf) != INVALID_FILE_ATTRIBUTES;
#endif

	/* log file handle */

	if (rooted)
		n = (DWORD)sfsprintf(state.logpath, sizeof(state.logpath), "%s\\var\\log\\uwin", state.root);
	else
		n = (DWORD)sfsprintf(state.logpath, sizeof(state.logpath), "%s\\uwin.log", state.root) - 4;
	if (state.init)
	{
		strcpy(old, state.logpath);
		strcpy(old + n, ".old");
		MoveFileEx(state.logpath, old, MOVEFILE_REPLACE_EXISTING);
		os.dwOSVersionInfoSize = sizeof(os);
		GetVersionEx(&os);
	}
	state.log = createfile(state.logpath, FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, sattr(0), OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	/* initial log messages */

	logmsg(LOG_INIT+1, "%s rel=%s wow=%d root=%s dll=%s mem=%s %s %s", mode, state.rel, state.wow, state.root, state.dll, mem, state.init ? "create" : "open", IDENT_TEXT(version_id));

	/* finalize shared mem map */

	for (;;)
	{
		if (state.init)
		{
			logmsg(LOG_INIT+1, "ipc_getlimits");
			ipc_getlimits(&ipclim, &tblsiz, &os);
			if(tblsiz.fifo_max*sizeof(Pfifo_t) > fifosize)
				fifosize = tblsiz.fifo_max*sizeof(Pfifo_t);
			n = BLOCK_SIZE +
				ipclim.msgmaxids*sizeof(unsigned long) +
				ipclim.semmaxids*sizeof(unsigned long) +
				ipclim.shmmaxids*sizeof(unsigned long) +
				fifosize +
				tblsiz.sid_max*sizeof(Psid_t) +
				tblsiz.file_max*sizeof(Pfd_t) +
				tblsiz.file_max*sizeof(long) +
				2*tblsiz.proc_max*sizeof(short) +
				(tblsiz.file_max+20)*BLOCK_SIZE;
			logmsg(LOG_INIT+1, "CreateFileMapping %s size=%u", mem, n);
			if (!(state.map = CreateFileMapping(INVALID_HANDLE_VALUE, sattr(0), PAGE_READWRITE, 0, n, mem)) && state.local != state.resource)
			{
				memmove(state.resource, state.local, strlen(state.local) + 1);
				state.local = state.resource;
				sfsprintf(mem, sizeof(mem), "%s%s", state.resource, UWIN_SHARE);
				logerr(LOG_INIT+1, "CreateFileMapping %s size=%u", mem, n);
				state.map = CreateFileMapping(INVALID_HANDLE_VALUE, sattr(0), PAGE_READWRITE, 0, n, mem);
			}
			if (!state.map)
			{
				logerr(LOG_INIT+1, "CreateFileMapping");
				return -1;
			}
		}
		if (!(state.share = MapViewOfFileEx(state.map, FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, 0, DESIRED_ADDRESS)) &&
		    !(state.share = MapViewOfFileEx(state.map, FILE_MAP_READ|FILE_MAP_WRITE, 0, 0, 0, NULL)))
		{
			CloseHandle(state.map);
			state.map = 0;
			logerr(LOG_INIT+1, "MapViewOfFileEx");
			return -1;
		}
		if (state.init)
			ZeroMemory(state.share, n);

		/* Share must correspond to state.root */

		Share = (Uwin_share_t*)state.share;
		if (state.init || !state.install)
			break;
		s = state.share + Share->root_offset;
		if (!_stricmp(state.root, s))
			break;
		uwin_pathmap(state.root, old, sizeof(old), UWIN_U2S);
		if (!_stricmp(old, s))
			break;
		if (!(hp = OpenProcess(PROCESS_DUP_HANDLE|PROCESS_TERMINATE, FALSE, i = Share->init_ntpid)))
			break;
		Share = 0;

		/* kill invalid install init */

		logmsg(LOG_INIT+1, "killing init %d root %s from incomplete install -- current root %s (%s)", i, s, state.root, old);
		strcpy(buf, s);
		TerminateProcess(hp, 256+9);
		CloseHandle(hp);
		CloseHandle(state.map);
		UnmapViewOfFile(state.share);
		state.map = 0;
		state.share = 0;

		/*
		 * if the other shared install root dir base name matches SEA*.tmp
		 * and it contains at most { init.exe posix.dll uwin.log } then it
		 * is the remnants of an incomplete uwin-base install and is removed
		 */

		t = s = buf + strlen(buf);
		if ((t - buf) > 8 && *--t == 'p' && *--t == 'm' && *--t == 't')
		{
			while (t > buf && *--t != '\\');
			if (*t++ == '\\' && *t++ == 'S' && *t++ == 'E' && *t == 'A')
			{
				strcpy(s, "\\" POSIX_DLL ".ini");
				if (GetFileAttributes(buf) == INVALID_FILE_ATTRIBUTES)
				{
					strcpy(s, "\\init.exe");
					DeleteFile(buf);
					strcpy(s, "\\" POSIX_DLL ".dll");
					DeleteFile(buf);
					strcpy(s, "\\uwin.log");
					DeleteFile(buf);
					*s = 0;
					if (RemoveDirectory(buf))
						logmsg(LOG_INIT+1, "incomplete uwin-base installation temp directory %s removed", buf);
				}
			}
		}
		state.init = LOG_INIT;
	}

	/* initialize shared mem and internal tables */

	if (state.init)
	{
		Share->magic = UWIN_SHARE_MAGIC;
		Share->version = UWIN_SHARE_VERSION;
		Share->block_size = BLOCK_SIZE;
		Share->old_block_size = BLOCK_SIZE;	/* OBSOLETE in 2012 */
		Share->share_size = BLOCK_SIZE;
		Share->stack_trace = 1;
		Share->mem_size = n;
		Share->Platform = (unsigned short)os.dwPlatformId;
		Share->Version = (unsigned short)((os.dwMajorVersion << 8) | os.dwMinorVersion);
	}
	else if (Share->share_size > 128)
	{
		ipclim = Share->ipc_limits;
		tblsiz.proc_max = Share->nprocs;
		tblsiz.file_max = Share->nfiles;
		tblsiz.fifo_max = Share->nfifos;
		tblsiz.sid_max = Share->nsids;
		if (Share->nfifos*sizeof(Pfifo_t) > fifosize)
			fifosize = Share->nfifos*sizeof(Pfifo_t);
	}
	Pshmtab = (unsigned short*)(state.share+Share->share_size);
	Pfifotab = (Pfifo_t*)&Pshmtab[ipclim.msgmaxids+ipclim.shmmaxids+ipclim.semmaxids];
	Psidtab = (Psid_t*)((char*)Pfifotab+fifosize);
	Pfdtab = (Pfd_t*)&Psidtab[tblsiz.sid_max];
	Pprochead = (short*)&Pfdtab[tblsiz.file_max];
	Pprocnthead = (short*)&Pprochead[tblsiz.proc_max];
	Blocktype = (unsigned short*)(state.share+roundof((char*)&Pprocnthead[tblsiz.proc_max]-state.share,BLOCK_SIZE));
	Pproctab = (Pproc_t*)(Blocktype+(tblsiz.file_max*sizeof(unsigned short)));
	Generation = (unsigned long*)((char*)Pproctab + BLOCK_SIZE*tblsiz.file_max);
	Share->block_table = (unsigned short)((char*)Pproctab - (char*)Share) / BLOCK_SIZE;
	if (state.init)
	{
		logmsg(LOG_INIT+1, "initialize shared mem");

		/* windows 95/98 might not set this to zero */

		ZeroMemory((void*)Pproctab,tblsiz.proc_max*sizeof(short));

#if !_X64_
		/* see if kernel32 supports compare_exchange function */
#if 0 /* cas vs semaphore usage fixed */
		Share->compare_exchange = (LONG (PASCAL*)(void*,LONG,LONG))getsymbol(MODULE_kernel,"InterlockedCompareExchange");
#else
		Share->compare_exchange = 0;
#endif
#endif
		for (i = 0; i < tblsiz.file_max; i++)
		{
			Pfdtab[i].refcount = -1;
			Pfdtab[i].type = FDTYPE_NONE;
		}

		/* initialize free pool, leave last one free */

		for (i = tblsiz.file_max-1; i > 0; i--)
		{
			register Blockhead_t* blk;

			blk = block_ptr(i);
			blk->next = Share->top;
			Blocktype[i] = BLK_FREE;
			Share->top = i;
		}
		Share->block = tblsiz.file_max-1;
		ZeroMemory(&Pfifotab[0], tblsiz.fifo_max * sizeof(Pfifotab[0]));
		for (i = 0; i < tblsiz.fifo_max; i++)
			Pfifotab[i].count = -1;
#ifdef _X86_
		if (Share->Platform != VER_PLATFORM_WIN32_NT)
		{
			DWORD				tibp;
			_asm{
				push	FS:[18h]
				pop	tibp
			}
			Share->pidxor = GetCurrentThreadId()^(tibp-0x10);
			VirtualQuery((void*)(~Share->pidxor), &minfo, sizeof(minfo));
			Share->pidbase = (DWORD)minfo.AllocationBase;
			Share->movex = 0;
		}
		else
#endif
			Share->movex = 1;
		Share->nblocks = tblsiz.file_max;
		Share->nprocs = tblsiz.proc_max;
		Share->nfiles = tblsiz.file_max;
		Share->nfifos = tblsiz.fifo_max;
		Share->nsids = tblsiz.sid_max;
		Share->child_max = tblsiz.child_max;
		if (Share->Platform == VER_PLATFORM_WIN32_NT)
			Share->affinity = tblsiz.affinity;
		Share->case_sensitive = (unsigned char)tblsiz.case_sensitive;
		Share->no_shamwow = (unsigned char)tblsiz.no_shamwow;
		Share->update_immediate = tblsiz.update_immediate;
		Share->sockstate = 0;	/* winsock uninitialized */
		Share->blockdev_index = block_alloc(BLK_VECTOR);
		Share->chardev_index = block_alloc(BLK_VECTOR);
		Share->mount_table = block_alloc(BLK_VECTOR);
		Share->pipe_times = block_alloc(BLK_VECTOR);
		Share->pid_counters = block_alloc(BLK_VECTOR);
		Share->lastslot = block_alloc(BLK_VECTOR);
		Share->pid_shift = pid_shift();
		Share->init_pipe = 0;
		init_devmem();
		logmsg(LOG_INIT+1, "init_pipetimes");
		init_pipetimes();
		logmsg(LOG_INIT+1, "init_mount");
		init_mount();
		Share->ipc_limits = ipclim;
		Share->argmax = sysconf(_SC_ARG_MAX);
		logmsg(LOG_INIT+1, "block_table=%d", Share->block_table);
	}
	else
		logmsg(LOG_INIT+1, "shared mem open");
	state.process_query = Share->Platform == VER_PLATFORM_WIN32_NT && Share->Version >= 0x0600 ? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION;
	state.urootlen = uwin_pathmap(state.root, state.uroot, sizeof(state.uroot), UWIN_W2U);
	state.uhomelen = uwin_pathmap(state.home, state.uhome, sizeof(state.uhome), UWIN_W2U);

	/* shamwow redux */

	if (state.wow && !Share->no_shamwow)
		state.shamwow = 1;
	if (state.wow || sizeof(char*) == 8)
	{
		astconfig[ASTCONFIG_PLATFORM_WINDOWS+0] = '6';
		astconfig[ASTCONFIG_PLATFORM_WINDOWS+1] = '4';
	}
	if (sizeof(char*) == 8)
	{
		astconfig[ASTCONFIG_PLATFORM_UWIN+0] = '6';
		astconfig[ASTCONFIG_PLATFORM_UWIN+1] = '4';
	}
	astconfig[ASTCONFIG_EQ] = 0;
	if (!GetEnvironmentVariable(astconfig, buf, sizeof(buf)) || memcmp(buf, astconfig+ASTCONFIG_EQ+1, ASTCONFIG_PLATFORM_VERIFY))
		SetEnvironmentVariable(astconfig, astconfig+ASTCONFIG_EQ+1);
	astconfig[ASTCONFIG_EQ] = '=';

	/* last chance for Share initialization */

	if (state.init)
	{
		logmsg(LOG_INIT+1, "init_boot_times");
		time_getboot(&Share->boot);
		time_getnow(&Share->start);
	}
	return 0;
}

static int terminit(Pfd_t *fdp)
{
	HANDLE in=NULL, out=NULL, me=GetCurrentProcess();
	int type = fdp->type;
	int count=3,blkno=dev_getdevno(type);
	Pdev_t	*pdev;
	if(Pfdtab[fdslot(P_CP,0)].type==type)
	{
		in = Phandle(0);
		logmsg(LOG_DEV+3, "terminit type=%(fdtype)s fd=%d slot=%d handle=%p", type, 0, fdslot(P_CP,0), in);
#ifdef GTL_TERM
		getfdp(P_CP,0)->devno=blkno;
#endif
	}
	else
		count--;
	if(Pfdtab[fdslot(P_CP,1)].type==type)
	{
		out = Phandle(1);
		logmsg(LOG_DEV+3, "terminit type=%(fdtype)s fd=%d slot=%d handle=%p", type, 1, fdslot(P_CP,1), out);
#ifdef GTL_TERM
		getfdp(P_CP,1)->devno=blkno;
#endif
	}
	else
		count--;
	if(Pfdtab[fdslot(P_CP,2)].type==type)
	{
		if(!out)
			out = Phandle(2);
		logmsg(LOG_DEV+3, "terminit type=%(fdtype)s fd=%d slot=%d handle=%p", type, 2, fdslot(P_CP,2), out);
#ifdef GTL_TERM
		getfdp(P_CP,2)->devno=blkno;
#endif
	}
	else
		count--;
	pdev = dev_ptr(blkno);
#ifdef GTL_TERM
	pdev->state= PDEV_INITIALIZE;
#else
	if(in && !DuplicateHandle(me, in, me, &in, 0, FALSE, DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle %p", in);
	if(out && !DuplicateHandle(me, out, me, &out, 0, FALSE, DUPLICATE_SAME_ACCESS))
		logerr(0, "DuplicateHandle %p", out);
#endif
	if(device_open(blkno,in,fdp,out)<0)
		return(-1);
	while(--count>0)
		InterlockedIncrement(&pdev->count);
	return(0);
}

static HMODULE trace_lib;

int trace(HANDLE hp, int flags)
{
	int		(*trace_init)(HANDLE, int);

	if (!trace_lib && !(trace_lib = LoadLibraryEx("trace10.dll", 0, 0)))
		return 0;
	if (!(trace_init = (int(*)(HANDLE, int))GetProcAddress(trace_lib, "trace_init")))
		return 0;
	state.trace_done = (void(*)(int))GetProcAddress(trace_lib, "trace_done");

	return (*trace_init)(hp, flags);
}

/*
 * per process device initialization
 */
static void init_devices(void)
{
	Devtab_t *dp;
	int i;
	for(i=0; i < elementsof(Blockdev) ; i++)
	{
		dp = &Blockdev[i];
		if(dp->initfn)
			(*dp->initfn)(dp);
	}
	for(i=0; i < elementsof(Chardev) ; i++)
	{
		dp = &Chardev[i];
		if(dp->initfn)
			(*dp->initfn)(dp);
	}
}

static LPTOP_LEVEL_EXCEPTION_FILTER filtfun;

static long WINAPI exfilter(struct _EXCEPTION_POINTERS *ep)
{
	unsigned long l = ep->ExceptionRecord->ExceptionCode;
	l = uwin_exception_filter(l,ep);
	if(l==EXCEPTION_CONTINUE_SEARCH)
		return((filtfun)(ep));
	return(l);
}

void _ast_atexit(void)
{
	void	(*ast_exit)(void);
	Pdev_t*	pdev;

	logmsg(LOG_PROC+1, "PROC_end sid=%d pgid=%d grent=%d state=%(state)s inexec=0x%x inuse=%d posix=0x%x name=%s", P_CP->sid, P_CP->pgid, P_CP->group, P_CP->state, P_CP->inexec, P_CP->inuse, P_CP->posixapp, P_CP->prog_name);
	if(!exit_done)
		stop_wait();
	exit_done = 1;
	if(ast_exit = (void(*)(void))getsymbol(MODULE_ast,"_ast_exit"))
		(*ast_exit)();
	if(state.trace_done)
		(*state.trace_done)(P_CP->exitcode);
	fdcloseall(P_CP);
	P_CP->posixapp &= ~UWIN_PROC;
	if(P_CP->pid!=1 && P_CP->child)
		proc_orphan(P_CP->pid);
	if(P_CP->pid==P_CP->pgid && P_CP->group && !P_CP->phandle)
	{
		if (!dup_to_init(NULL,Share->init_slot))
			logerr(0, "dup_to_init");
		else if (!P_CP->phandle)
			logmsg(0, "dup_to_init failed to gen phandle");
	}
	if(P_CP->sid==P_CP->pid && P_CP->console && (pdev=dev_ptr(P_CP->console)))
		kill(-pdev->tgrp,SIGHUP);
	if(P_CP->state!=PROCSTATE_TERMINATE && P_CP->state!=PROCSTATE_ABORT && P_CP->state!=PROCSTATE_EXITED)
		P_CP->state = PROCSTATE_ZOMBIE;
	if(P_CP->curdir>=0 && curdir<0 && Pfdtab[P_CP->curdir].refcount>=0)
		fdpclose(&Pfdtab[P_CP->curdir]);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, VOID *reserved)
{
	register i,maxfd = 0;
	DWORD ntpid = GetCurrentProcessId();
	HANDLE token = 0, me = GetCurrentProcess();
	LPTOP_LEVEL_EXCEPTION_FILTER fun;
	STARTUPINFO sinfo;
	Pdev_t *pdev;
	unsigned char*	ftype;
#define UWIN_STATIC_FDTAB	1
#if UWIN_STATIC_FDTAB
	static Pprocfd_t fdtabp[OPEN_MAX+sizeof(P_CP->slotblocks)*BLOCK_SIZE/sizeof(short)];
#else
	Pprocfd_t *fdtabp;
#endif
	int newthread=0,new_proc=0;
	int debug;

	switch(reason)
	{
	case DLL_PROCESS_ATTACH:
		filtfun = SetUnhandledExceptionFilter(exfilter);
		errno_addr = _errno();
		environ_addr = &_environ;
		curdir = -1;
		dllinfo._ast_environ = environ_addr;
		dllinfo._ast_errno = errno_addr;
		dllinfo._ast_stdin = (void*)&(_Sfstdin);
		dllinfo._ast_stdout = (void*)&(_Sfstdout);
		dllinfo._ast_stderr = (void*)&(_Sfstderr);
		dllinfo._ast_tzname = (void*)&(tzname);
		dllinfo._ast_daylight = (void*)&(tmp_daylight);
		dllinfo._ast_herrno = (void*)&tmp_herrno;
		dllinfo._ast_libversion = 8;
		Ntpid = GetCurrentProcessId();
		break;
	case DLL_PROCESS_DETACH:
		if ((fun = SetUnhandledExceptionFilter(filtfun)) && fun != exfilter)
			logmsg(0, "DLL_PROCESS_DETACH: exception filter %p should be %p (exfilter)", fun, exfilter);
		P_CP->sigevent = 0;
		P_CP->sevent = 0;
		if(proc_exec_exit || (P_CP->inexec&PROC_HAS_PARENT))
		{
			logmsg(LOG_PROC+1, "PROC_end sid=%d pgid=%d state=%(state)s inexec=0x%x ref=%d name=%s", P_CP->sid, P_CP->pgid, P_CP->state, P_CP->inexec, P_CP->inuse, P_CP->prog_name);
			P_CP->posixapp |= POSIX_DETACH;
			return 1;
		}
		if(proc_slot(P_CP)==Share->init_slot)
			logmsg(0, "init process terminating exitval=0x%x", P_CP->exitcode);
		if (!exit_done)
		{
			exit_done = 1;
			_ast_atexit();
		}
		logmsg(LOG_PROC+4, "detach");
		Share = 0;
		P_CP->posixapp |= POSIX_DETACH;
		P_CP = 0;
		UnmapViewOfFile(state.share);
		CloseHandle(state.map);
	default:
		return 1;
	}
	main_thread = GetCurrentThreadId();
	if (init_state(&debug))
		return 0;
	fs_init();
	if(Share->affinity)
	{
		BOOL (PASCAL*affinity)(HANDLE,WORD) = (BOOL(PASCAL*)(HANDLE,WORD))getsymbol(MODULE_kernel,"SetProcessAffinityMask");
		if(affinity && !(*affinity)(me, 1))
			logerr(LOG_INIT+1, "SetProcessAffinityMask");
	}
	logmsg(LOG_INIT+1, "init_devices");
	init_devices();
	logmsg(LOG_INIT+1, "GetStartupInfo");
	GetStartupInfo(&sinfo);
#if !UWIN_STATIC_FDTAB
	fdtabp = (Pprocfd_t*)malloc(OPEN_MAX*sizeof(Pprocfd_t));
#endif
	logmsg(LOG_INIT+1, "clear fdtab");
	ZeroMemory((void*)fdtabp,OPEN_MAX*sizeof(Pprocfd_t));
	if(Share->Platform==VER_PLATFORM_WIN32_NT)
	{
		LUID			luid;
		char			tokbuf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
		TOKEN_PRIVILEGES*	tokpri=(TOKEN_PRIVILEGES *)tokbuf;

		if(!OpenProcessToken(me,TOKEN_ADJUST_DEFAULT|TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE|TOKEN_ADJUST_PRIVILEGES,&token) &&
		   !OpenProcessToken(me,TOKEN_QUERY|TOKEN_IMPERSONATE|TOKEN_DUPLICATE|TOKEN_ADJUST_PRIVILEGES,&token))
			logerr(LOG_INIT+1, "OpenProcessToken");

		/* grant admin uwin processes debug priv to get process tokens */
		if(LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
		{
			tokpri->PrivilegeCount = 1;
			tokpri->Privileges[0].Luid = luid;
			tokpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(token, FALSE, tokpri, 0, NULL, NULL);
		}

		/*Setting up the Process Shutdown Parameter...*/
		if(!SetProcessShutdownParameters(0x300,SHUTDOWN_NORETRY))
			logerr(LOG_INIT+1, "SetProcessShutdownParameters");
	}

	/*
	 * parent-bits=>child-bits maxfd/cbReserved2
	 * 
	 * 32=>32  3/19  3/19
	 * 32=>64  3/19  3/31
	 * 64=>32  3/31  3/19
	 * 64=>64  3/31  3/31
	 */

	if (!state.init && sinfo.lpReserved2 && sinfo.cbReserved2 >= sizeof(DWORD))
	{
		P_CP = proc_ntgetlocked(ntpid, 1);
		maxfd = *((DWORD*)sinfo.lpReserved2);
		logmsg(LOG_INIT+2, "%d/%d maxfd=%d P_CP=%p cbReserved2=%d  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x", 8 * sizeof(char*), state.shamwow ? 64 : 8 * sizeof(char*), maxfd, P_CP, sinfo.cbReserved2, ((unsigned char*)sinfo.lpReserved2)[0], ((unsigned char*)sinfo.lpReserved2)[1], ((unsigned char*)sinfo.lpReserved2)[2], ((unsigned char*)sinfo.lpReserved2)[3], ((unsigned char*)sinfo.lpReserved2)[4], ((unsigned char*)sinfo.lpReserved2)[5], ((unsigned char*)sinfo.lpReserved2)[6], ((unsigned char*)sinfo.lpReserved2)[7], ((unsigned char*)sinfo.lpReserved2)[8], ((unsigned char*)sinfo.lpReserved2)[9], ((unsigned char*)sinfo.lpReserved2)[10], ((unsigned char*)sinfo.lpReserved2)[11]);
		if (P_CP)
		{
			HANDLE		hp;
			HANDLE		xp;
			Pfd_t*		fdp;
			char*		handle;
			char*		xhandle;

#if _X64_
			P_CP->type |= PROCTYPE_64;
#else
			P_CP->type |= PROCTYPE_32;
#endif
			ftype = (unsigned char*)sinfo.lpReserved2 + sizeof(DWORD);
			handle = (char*)ftype + maxfd * sizeof(unsigned char);
			xhandle = handle + (maxfd/2)*sizeof(HANDLE);
			proc_release(P_CP);
			P_CP->sigevent = 0;
			if(P_CP->max_limits[RLIMIT_NOFILE]==0)
				P_CP->cur_limits[RLIMIT_NOFILE] = OPEN_MAX;
			if(P_CP->phandle)
			{
				DWORD	pid2=0;

				if(i=WaitForSingleObject(P_CP->ph2,5000))
				{
					pid2 = GetProcessId(P_CP->ph2);
					logmsg(1,"exec_timeout i=%d ph2=%p ntpid=%x origntpid=%x",i,P_CP->ph2,pid2,P_CP->wait_threadid);
					if(i!=WAIT_TIMEOUT)
						pid2 = 0;
					else if(!TerminateProcess(P_CP->ph2,0))
						logerr(0,"terminateproc");
					if(pid2!=P_CP->wait_threadid)
						pid2 = 0;

				}
				if(pid2 && (i=WaitForSingleObject(P_CP->ph2,5000)))
					logmsg(0,"exec_timeout2 i=%d ph2=%p ntpid2=%x",i,P_CP->ph2,pid2);
				closehandle(P_CP->ph2,HT_PROC);
				P_CP->ph2 = 0;
				/* parent must be complete and waited for */
				i=WaitForSingleObject(P_CP->phandle,3000);
				if(i==WAIT_TIMEOUT)
					logmsg(1, "phandle timed out phandle=%p ph=%p inexec=0x%x",P_CP->phandle,P_CP->ph,P_CP->inexec);
				else if(i!=WAIT_OBJECT_0)
					logerr(0, "WaitForSingleObject");
				closehandle(P_CP->phandle,HT_EVENT);
				P_CP->phandle = 0;
			}
			P_CP->wait_threadid = 0;
			P_CP->inexec &= ~PROC_DAEMON;
			sigdefer(1);
			P_CP->fdtab = fdtabp;
			for (i = 0; i < maxfd/2; i++)
			{
				/* NOTE: this only works for little-endian -- no matter, m$ will never suffer big-endian */
				memcpy(&hp, handle, sizeof(HANDLE));
				handle += sizeof(HANDLE);
				memcpy(&xp, xhandle, sizeof(HANDLE));
				xhandle += sizeof(HANDLE);
				if(fdslot(P_CP,i) < 0)
					continue;
				fdp = getfdp(P_CP,i);
				/* keys are not inherited in windows 9x */
				if(Share->Platform!=VER_PLATFORM_WIN32_NT && fdp->type==FDTYPE_REG)
				{
					if (Phandle(i) = regopen(fdname(file_slot(fdp)),fdp->wow,fdp->oflag,0,&hp,0))
						Xhandle(i) = hp;
					hp = Phandle(i);
				}
				logmsg(LOG_INIT+2, "reserved console=%d type=%(fdtype)s fd=%d slot=%d handle=%p", console, fdp->type, i, file_slot(fdp), hp);
				logmsg(LOG_INIT+2, "fd%3d %x%02x %08x:%08x %(fdtype)s", i, iscloexec(i), ftype[i], (DWORD)hp, (DWORD)xp, fdp->type);
				if(fdp->type==FDTYPE_PTY && (i==1||i==2))
				{
					Phandle(i) = xp;
					Xhandle(i) = hp;
				}
				else
				{
					Phandle(i) = hp;
					Xhandle(i) = xp;
				}
				pdev = (fdp->devno? dev_ptr(fdp->devno): 0);
				if(hp && (fdp->type==FDTYPE_EPIPE||fdp->type==FDTYPE_EFIFO) && (fdp->oflag&O_WRONLY) && !iscloexec(i))
				{
					int v = PIPE_NOWAIT;
					if(!SetNamedPipeHandleState(hp,&v,NULL,NULL))
						logerr(LOG_PROC+2, "SetNamedPipeHandleState");
				}
			}
			/* prevents detatched processes from creating pty */
			if(maxfd==0)
				maxfd=1;
			if(P_CP->console == 0)
			{
				/* will get here by call to wterm */
				if(sinfo.dwFlags&~STARTF_USESTDHANDLES)
					maxfd = 0;
			}
		}
		else
		{
			HANDLE	handles[64];
			Pfd_t*	fdp;

			if(!(P_CP = proc_create(ntpid)))
				exit(99);
#if _X64_
			P_CP->type |= PROCTYPE_64;
#else
			P_CP->type |= PROCTYPE_32;
#endif
			P_CP->fdtab = fdtabp;
			sigdefer(1);
			new_proc = 1;
			P_CP->pgid = P_CP->pid;
			ftype = (unsigned char*)sinfo.lpReserved2 + sizeof(DWORD);
			if((i=maxfd*sizeof(HANDLE)) > sizeof(handles))
				i = sizeof(handles);
			memcpy((void*)handles,(void*)&ftype[maxfd],i);
			for(i=0; i < maxfd;i++)
			{
				if((ftype[i]&F_OPEN) && handles[i] && handles[i]!=INVALID_HANDLE_VALUE)
				{
					DWORD	mode;

					Phandle(i) = handles[i];
					Xhandle(i) = 0;
					if(console>0 && GetConsoleMode(Phandle(i),&mode))
					{
						incr_refcount(&Pfdtab[console]);
						setfdslot(P_CP,i,console);
						continue;
					}
					setfdslot(P_CP,i,fdfindslot());
					fdp = getfdp(P_CP,i);
					fdp->oflag = O_RDWR;
					if(ftype[i]&F_APPEND)
						fdp->oflag |= O_APPEND;
					if(ftype[i]&F_PIPE)
					{
						fdp->type = FDTYPE_PIPE;
						fdp->sigio = file_slot(fdp)+2;
					}
					else if(ftype[i]&F_DEV)
					{
						if(GetConsoleMode(Phandle(i),&mode))
						{
							fdp->type = FDTYPE_CONSOLE;
							console = file_slot(fdp);
						}
						else
							fdp->type = FDTYPE_TTY;
					}
					else if(ftype[i]&F_TEXT)
					{
						fdp->type = FDTYPE_TFILE;
						fdp->oflag |= O_TEXT;
					}
					else
						fdp->type = FDTYPE_FILE;
					logmsg(LOG_INIT+2, "fd%3d %x%02x %08x:%08x %(fdtype)s", i, iscloexec(i), ftype[i], (DWORD)Phandle(i), (DWORD)Xhandle(i), fdp->type);
				}
				else
					logmsg(LOG_INIT+2, "fd%3d -%02x %08x", i, ftype[i], (DWORD)handles[i]);
			}
		}
		P_CP->maxfds = maxfd;
		P_CP->state = PROCSTATE_RUNNING;
		P_CP->rtoken = token;
	}
	if(maxfd==0)
	{
		HANDLE	fp;
		int	stdtype;
		int	type;
		int	w;

		if(!P_CP)
		{
			state.clone.helper = 1;
			if(!(P_CP = proc_create(ntpid)))
				exit(98);
			strcpy(P_CP->prog_name, state.local);
			P_CP->fdtab = fdtabp;
			new_proc = 1;
			P_CP->pgid = P_CP->pid;
		}
		P_CP->tls = (short)TlsAlloc();
		P_CP->state = PROCSTATE_RUNNING;
		P_CP->rtoken = token;
		if (debug >= 0)
			P_CP->debug = debug;
		fp = GetStdHandle(STD_INPUT_HANDLE);
		if (fp && fp != INVALID_HANDLE_VALUE)
		{
			stdtype = GetFileType(fp)&~FILE_TYPE_REMOTE;
			type = FILE_type(stdtype);
			logmsg(LOG_DEV+2, "GetStdHandle type=%(fdtype)s fd=%d handle=%p", type, 0, fp);
			if(type==FDTYPE_CONSOLE && !GetConsoleMode(fp,&w) && GetLastError()==ERROR_INVALID_HANDLE)
				type = FDTYPE_FILE;
			i = fdfindslot();
			if(type==FDTYPE_CONSOLE)
				console = i;
			else if(type==FDTYPE_PIPE)
			{
				Pfd_t *fdp = getfdp(P_CP,0);
				fdp->sigio = file_slot(fdp)+2;
			}
			setfdslot(P_CP,0,i);
			Phandle(0) = fp;
			Xhandle(0) = 0;
			resetcloexec(0);
			Pfdtab[i].oflag = O_RDONLY;
			Pfdtab[i].type = type;
			P_CP->maxfds = 1;
			logmsg(LOG_DEV+2, "GetStdHandle console=%d type=%(fdtype)s fd=%d slot=%d handle=%p", console, type, 0, i, fp);
			logmsg(LOG_PROC+3, "fd%3d %x%02x %08x:%08x %(fdtype)s", 0, iscloexec(0), stdtype, (DWORD)Phandle(0), (DWORD)Xhandle(0), type);
		}
		fp = GetStdHandle(STD_OUTPUT_HANDLE);
		if (fp && (fp != INVALID_HANDLE_VALUE))
		{
			stdtype = GetFileType(fp)&~FILE_TYPE_REMOTE;
			type = FILE_type(stdtype);
			logmsg(LOG_DEV+3, "GetStdHandle type=%(fdtype)s fd=%d handle=%p", type, 1, fp);
			if(type==FDTYPE_CONSOLE && !GetConsoleMode(fp,&w) && GetLastError()==ERROR_INVALID_HANDLE)
				type = FDTYPE_FILE;
			if(type!=FDTYPE_CONSOLE || console<0)
			{
				i = fdfindslot();
				if(type==FDTYPE_CONSOLE)
					console = i;
			}
			else
			{
				i = console;
				incr_refcount(&Pfdtab[i]);
			}
			setfdslot(P_CP,1,i);
			Phandle(1) = fp;
			Xhandle(1) = 0;
			resetcloexec(1);
			Pfdtab[i].oflag = O_WRONLY;
			Pfdtab[i].type = type;
			P_CP->maxfds = 2;
			logmsg(LOG_DEV+3, "GetStdHandle console=%d type=%(fdtype)s fd=%d slot=%d handle=%p", console, type, 1, i, fp);
			logmsg(LOG_PROC+4, "fd%3d %x%02x %08x:%08x %(fdtype)s", 1, iscloexec(1), stdtype, (DWORD)Phandle(1), (DWORD)Xhandle(1), type);
		}
		fp = GetStdHandle(STD_ERROR_HANDLE);
		if (fp && (fp != INVALID_HANDLE_VALUE))
		{
			stdtype = GetFileType(fp)&~FILE_TYPE_REMOTE;
			type = FILE_type(stdtype);
			logmsg(LOG_DEV+3, "GetStdHandle type=%(fdtype)s fd=%d handle=%p", type, 2, fp);
			if(type==FDTYPE_CONSOLE && !GetConsoleMode(fp,&w) && GetLastError()==ERROR_INVALID_HANDLE)
				type = FDTYPE_FILE;
			if(type!=FDTYPE_CONSOLE || console<0)
			{
				i = fdfindslot();
				if(type==FDTYPE_CONSOLE)
					console = i;
			}
			else
			{
				i = console;
				incr_refcount(&Pfdtab[i]);
			}
			setfdslot(P_CP,2,i);
			Phandle(2) = fp;
			Xhandle(2) = 0;
			resetcloexec(2);
			Pfdtab[i].oflag = O_RDWR;
			Pfdtab[i].type = type;
			P_CP->maxfds = 3;
			logmsg(LOG_DEV+3, "GetStdHandle console=%d type=%(fdtype)s fd=%d slot=%d handle=%p", console, type, 2, i, fp);
			logmsg(LOG_PROC+4, "fd%3d %x%02x %08x:%08x %(fdtype)s", 2, iscloexec(2), stdtype, (DWORD)Phandle(2), (DWORD)Xhandle(2), type);
		}
	}
	else
	{
		if (debug >= 0)
			P_CP->debug = debug;
		P_CP->ntpid = ntpid;
		P_CP->state = PROCSTATE_RUNNING;
		if(P_CP->pgid==0)
		{
			logmsg(0, "pgid was zero");
			P_CP->pgid = P_CP->pid;
		}
		if(!new_proc)
			console = TERM_REINIT;
	}
	logmsg(LOG_INIT+1, "init_sid");
	init_sid(1);
	if(new_proc)
	{
		P_CP->posixapp &= ~POSIX_PARENT;
		proc_addlist(P_CP);
	}
	/* set the uid & gid required for proc file system */
	P_CP->sigevent = P_CP->sevent = 0;
	if(P_CP->rtoken)
	{
		P_CP->uid = sid_to_unixid(sid(SID_UID));
		if(P_CP->gid != (gid_t)-1)
			assign_group(P_CP->gid,REAL);
		if(P_CP->egid != (gid_t)-1)
			assign_group(P_CP->egid,EFFECTIVE);
	}
	else
	{
		struct passwd *pw;
		char name[40];
		int i;
		i = sizeof(name);
		if(GetUserName(name, &i) && (pw=getpwnam(name)))
		{
			P_CP->uid = pw->pw_uid;
			P_CP->gid = pw->pw_gid;
		}
		else
		{
			P_CP->uid = 0;
			P_CP->gid = 0;
			P_CP->egid = 0;
		}
	}
	if(!P_CP->ph && Share->init_slot)
	{
		/* open handle to process so that it can be passed to init */
		if(!(P_CP->ph = OpenProcess(PROCESS_DUP_HANDLE|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,ntpid)))
			logerr(0, "OpenProcess");
	}
	if (Share->Platform==VER_PLATFORM_WIN32_NT && P_CP->rtoken)
	{
		/* disable backup/restore privileges by default */
		TOKEN_PRIVILEGES *tp = backup_restore();
		HANDLE tok = P_CP->etoken?P_CP->etoken:P_CP->rtoken;
		tp->Privileges[0].Attributes = tp->Privileges[1].Attributes = 0;
		AdjustTokenPrivileges(tok, FALSE, tp, 0, NULL, NULL);
	}
	if (Share->Platform==VER_PLATFORM_WIN32_NT && P_CP->etoken)
	{
		if(!DuplicateHandle(me,P_CP->etoken,me,&P_CP->stoken,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		if(impersonate_user(P_CP->etoken)<0)
			logerr(0, "impersonate failed token=%p", P_CP->etoken);
	}
	if(P_CP->phandle)
		closehandle(P_CP->phandle,HT_EVENT);
	P_CP->phandle = 0;
	if(state.init)
	{
		struct stat		statb;
		char			name[UWIN_RESOURCE_MAX];

		first_process = 1;
		logmsg(LOG_INIT+1, "ipc_init");
		ipc_init();
		if (stat("/dev/null", &statb)) /* no /dev/null,  so create it */
			mknod("/dev/null", S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, 0);
		UWIN_EVENT_INIT_RESTART(name);
		if (!CreateEvent(sattr(0), TRUE, FALSE, name))
			logerr(LOG_INIT, "CreateEvent %s failed", name);
		else
			logmsg(LOG_INIT+1, "create event %s", name);
		init_init();
		if (P_CP->ph)
		{
			closehandle(P_CP->ph, HT_PROC);
			P_CP->ph = 0;
		}
		P_CP->parent = Share->init_slot;
		logmsg(LOG_INIT+1, "init_mount_extra");
		init_mount_extra();
		SetLastError(0);
	}
	else if(new_proc && Share->init_slot && P_CP->ph)
	{
		HANDLE ph = P_CP->ph, me=GetCurrentProcess();
		if(!DuplicateHandle(me,GetCurrentThread(),me,&P_CP->thread,0,FALSE,DUPLICATE_SAME_ACCESS))
			logerr(0, "DuplicateHandle");
		proc_reparent(P_CP,Share->init_slot);
		closehandle(ph,HT_PROC);
	}
	if(state.init)
	{
		struct utsname	ut;
		uname(&ut);
		SetLastError(0);
		logmsg(0, "%s %s %s %s %d/%d dll=%s mem=%s%s %s", ut.sysname, ut.release, ut.version, ut.machine, sizeof(char*) == 8 ? 64 : 32, sizeof(char*) == 8 || state.wow ? 64 : 32, state.dll, state.resource, UWIN_SHARE, IDENT_TEXT(version_id));
	}
	logmsg(LOG_PROC+1, "PROC_begin maxfd=%d sid=%d pgid=%d state=%(state)s name=%s", P_CP->maxfds, P_CP->sid, P_CP->pgid, P_CP->state, P_CP->prog_name);
	if(P_CP->console<=0)
		SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
	P_CP->threadid = GetCurrentThreadId();
	if((curdir=P_CP->curdir)>=0)
	{
		setcurdir(-1, 0, 0, 0);
		SetCurrentDirectory(state.pwd);
	}
	if(P_CP->trace)
		trace(P_CP->trace,P_CP->traceflags);
	if(console>=0)
	{
		if(!(P_CP->posixapp&POSIX_PARENT) && !(sinfo.dwFlags&CREATE_SEPARATE_WOW_VDM))
			P_CP->console_child = 1;
		if(!P_CP->console)
			P_CP->console = console;
	}
	logmsg(LOG_INIT+2, "exception filter %p => %p (%p)", filtfun, exfilter, SetUnhandledExceptionFilter(exfilter));
	return 1; /* we're happy */
}

/*
 * use static address for main thread, otherwise, thread specific
 */
int *_ast_errno(void)
{
	static int *(*errfn)(void);
	if(main_thread != GetCurrentThreadId())
	{
		if(!errfn)
			errfn = (int*(*)(void))getsymbol(MODULE_msvcrt,"_errno");
		if(errfn)
			return((*errfn)());
	}
	return(errno_addr);
}

char **_ast_tzname(void)
{
	return(dllinfo._ast_tzname);
}

char ***_ast_environ(void)
{
	return(environ_addr);
}

extern struct _astdll *_ast_getdll(void)
{
	if(errno_addr && dllinfo._ast_errno != errno_addr)
		dllinfo._ast_errno = errno_addr;
	return(&dllinfo);
}

typedef int (WINAPI *Winmain)(HINSTANCE,HINSTANCE,LPSTR,int);
#ifndef GLOB_ICASE
#   define GLOB_ICASE GLOB_IGNCASE
#endif

/*
 * does ksh style pathname expansion for non-posix startups
 * requires newer pmain.c in order to work
 */
static int file_expand(void)
{
	int (*globfn)(), flags=GLOB_DOOFFS|GLOB_ICASE|GLOB_NOCHECK;
	glob_t gdata;
	char *cp,**av = dllinfo._ast__argv;
	if(!(globfn = (int(*)())getsymbol(MODULE_ast,"glob")))
		return(-1);
	ZeroMemory((void*)&gdata,sizeof(gdata));
	gdata.gl_offs = 1;
	if((cp = strrchr(av[0],'.')) && _stricmp(cp,".exe")==0)
		*cp = 0;
	while(cp= *++av)
	{
		if((*globfn)(cp, flags, NULL, &gdata) < 0)
			logerr(0, "glob");
		flags |= GLOB_APPEND;
	}
	if(gdata.gl_pathc>0)
	{
		gdata.gl_pathv[0] = dllinfo._ast__argv[0];
		dllinfo._ast__argv = gdata.gl_pathv;
	}
	return(1); // Added to prevent compiler warning.
}

#ifdef _ALPHA_

static int copy_args(void)
{
	char *dp, *cp,**av=dllinfo._ast__argv;
	int i=0,size=0;
	while (cp=av[i++])
		size += strlen(cp);
	size += i*(sizeof(char*)+1);
	if(!(av = malloc(size)))
		return(0);
	dp = (char*)&av[i];
	for (i=0;cp=dllinfo._ast__argv[i];i++)
	{
		av[i] = dp;
		size = strlen(cp)+1;
		memcpy(dp,cp,size);
		dp += size;
	}
	av[i] = 0;
	dllinfo._ast__argv = av;
	return(1);
}

#endif

static DWORD WINAPI start_clone(LPVOID param)
{
	char			pname[64];
	HANDLE			hp,th;
	DWORD			type=PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT;
	UWIN_PIPE_CLONE(pname, 1);
	for (;;)
	{
		if ((hp = CreateNamedPipe(pname,PIPE_ACCESS_DUPLEX,type,25,8192,8192,NMPWAIT_WAIT_FOREVER,sattr(0))) == INVALID_HANDLE_VALUE)
		{
			logerr(0,"start_clone_pipe failed to create pname=%s",pname);
			Sleep(1000);
			continue;
		}
		if(ConnectNamedPipe(hp,NULL))
		{
			logmsg(0,"clone pipe connected hp=%p",hp);
			if(th=CreateThread(sattr(0),0,fh_main,(void*)hp,0,&state.clone.helper))
				CloseHandle(th);
			else
				logmsg(0,"Create clone thread");
		}
	}
	return(FALSE);
}

static DWORD WINAPI dup_init_handle(LPVOID param)
{
	char			pname[64];
	HANDLE			hp, xp, init_pipe, me=GetCurrentProcess();
	unsigned short		slot;
	Pproc_t			*proc;
	DWORD			size, ntpid=(DWORD)param;
	DWORD			type=PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT;
	UWIN_PIPE_INIT(pname, ntpid);
	if ((init_pipe = CreateNamedPipe(pname,PIPE_ACCESS_DUPLEX,type,1,256,256,NMPWAIT_WAIT_FOREVER,sattr(0))) == INVALID_HANDLE_VALUE)
	{
		logerr(0,"dup_init_pipe failed to create pname=%s",pname);
		return(FALSE);
	}
	Ih[3] = init_pipe;
	for (;;)
	{
		if (ConnectNamedPipe(init_pipe,NULL))
		{
			if (ReadFile(init_pipe,&slot,sizeof(slot),&size,NULL) && size==sizeof(slot))
			{
				proc = proc_ptr(slot);
				if (xp=OpenProcess(PROCESS_DUP_HANDLE,FALSE,proc->ntpid))
				{
					if (DuplicateHandle(me,me,xp,&hp,0,FALSE,DUPLICATE_SAME_ACCESS))
						proc->phandle = hp;
					else
						logerr(0,"dupinithandle slot=%d ntpid=%x",slot,proc->ntpid);
					CloseHandle(xp);
				}
				else
					logerr(0,"openproc ntpid=%x slot=%d",proc->ntpid,slot);
			}
			else
				logerr(0,"read_init_pipe");
			if(!DisconnectNamedPipe(init_pipe))
				logerr(0,"disconnect_init_pipe");
		}
		else
			logerr(0,"connect_init_pipe");
	}
	logmsg(0,"dup_init_hande thread trminated");
	return TRUE;
}

static int run_init_proc(void)
{
	HANDLE		th,event;
	DWORD		status;
	char		name[UWIN_RESOURCE_MAX];
	int		i;
	int		fd;
	char*		s;

	proc_mutex(0, 0);
	initsig(1);
	if (state.install)
		s = ".install";
	else if (!state.standalone || !(s = strrchr(state.resource, '.')))
		s = "";
	sfsprintf(P_CP->prog_name, sizeof(P_CP->prog_name), "init%s", s);
	closehandle(GetStdHandle(STD_INPUT_HANDLE), HT_ICONSOLE);
	closehandle(GetStdHandle(STD_OUTPUT_HANDLE), HT_OCONSOLE);
	closehandle(GetStdHandle(STD_ERROR_HANDLE), HT_OCONSOLE);
	CreateThread(sattr(0), 0, dup_init_handle, (void*)P_CP->ntpid, 0, &status);
	if(th=CreateThread(sattr(0), 0, start_clone, (void*)P_CP->ntpid, 0, &status))
		CloseHandle(th);
	if ((fd = open("/dev/null", 0)) >= 0)
	{
		i = fdslot(P_CP,fd);
		Share->nullblock = Pfdtab[i].index;
		close(fd);
	}
	if (Share->Platform == VER_PLATFORM_WIN32_NT)
	{
		HANDLE hpin,hpout;
		if (CreatePipe(&hpin, &hpout, sattr(0), 1024))
		{
			UWIN_EVENT_INIT(name);
			if (event = CreateEvent(sattr(0), TRUE, FALSE, name))
			{
				P_CP->ph = event;
				P_CP->ph2 = hpin;
				Share->init_pipe = hpout;
				Ih[0] = event;
				Ih[1] = hpin;
				Ih[2] = hpout;
			}
			else
			{
				logerr(0, "CreateEvent %s failed", name);
				closehandle(hpin,HT_PIPE);
				closehandle(hpout,HT_PIPE);
			}
		}
		else
			logerr(0, "CreatePipe");
	}
	start_wait_thread(proc_slot(P_CP),&P_CP->wait_threadid);
	Share->init_thread_id = (int)P_CP->wait_threadid;
	logmsg(LOG_PROC+2, "set init_thread_id=%04x", Share->init_thread_id);
	sigdefer(1);
	initsig(1);
	for (i = 1; i < NSIG; i++)
		sigsetignore(P_CP, i, 1);
	FreeConsole();
	sigsetignore(P_CP, SIGABRT, 0);
	sigsetignore(P_CP, SIGIOT, 0);
	sigsetignore(P_CP, SIGEMT, 0);
	sigsetignore(P_CP, SIGFPE, 0);
	sigsetignore(P_CP, SIGBUS, 0);
	sigsetignore(P_CP, SIGSEGV, 0);
	if (P_CP->curdir >= 0)
	{
		setcurdir(-1, 0, 0, 0);
		SetCurrentDirectory(state.pwd);
	}
	UWIN_EVENT_INIT_START(name);
	if (!(event = OpenEvent(EVENT_MODIFY_STATE|SYNCHRONIZE, FALSE, name)))
		logerr(0, "OpenEvent %s failed", name);
	else
	{
		UWIN_MUTEX_INIT_RUNNING(name, P_CP->ntpid);
		if (!CreateMutex(sattr(0), TRUE, name))
			logerr(0, "CreateMutex %s failed", name);
		else if (!SetEvent(event))
			logerr(0, "SetEvent %s failed", name);
		else
		{
			closehandle(event, HT_EVENT);
			logmsg(0, "%s %s running", P_CP->prog_name, name);
			start_sockgen();
			for (;;)
			{
				WaitForSingleObject(P_CP->sevent, INFINITE);
				ResetEvent(P_CP->sevent);
			}
		}
	}
	logmsg(0, "%s exit", P_CP->prog_name);
	return 1;
}

void clean_deleted(char *name)
{
	WIN32_FIND_DATA info;
	HANDLE hp;
	int n;
	char *cp;

	n = (int)strlen(name);
	strcpy(&name[n],"\\*.*");
	hp = FindFirstFile(name,&info);
	if(!hp || hp==INVALID_HANDLE_VALUE)
		return;
	name[n] = 0;
	if(!SetCurrentDirectory(name))
		return;
	do
	{
		cp = info.cFileName;
		if(cp[1]==0 && *cp=='.')
			continue;
		else if(cp[2]==0 && *cp=='.' && cp[1]==0)
			continue;
		DeleteFile(cp);
	} while(FindNextFile(hp,&info));
	FindClose(hp);
}

/*
 * clean out .deleted directories
 */
static void deleted_init(void)
{
	char name[20],dir[256];
	int i,mode = SetErrorMode(SEM_FAILCRITICALERRORS);
	unsigned long dtime,drives = GetLogicalDrives();
	FILETIME now;
	strcpy(name,"a:\\.deleted");
	GetCurrentDirectory(sizeof(dir),dir);
	for(i=0; i < 26; i++)
	{
		if(i<2 &&  (dtime=drive_time(&now))==Share->drivetime[i])
			continue;
		*name = 'A'+i;
		if(GetFileAttributes(name) != INVALID_FILE_ATTRIBUTES)
			clean_deleted(name);
		else if(i<2)
			Share->drivetime[i] = dtime;
	}
	clean_deleted(uwin_deleted(0, name, sizeof(name), 0));
	SetCurrentDirectory(dir);
	SetErrorMode(mode);
}

extern int _ast_runprog(int(*begin)(int, char*[], char**), int argc, char *argv[], char ***env, int *erraddr)
{
	static int beenhere;
	TIMECAPS tc;
	Pdev_t *pdev=0;
	Pfd_t *fdp;
	char **oenv,*last;
	void (*initfn)(void);
	int rsize=1;
	/* repeat the exfilter() setting in case an interloper clobbered our DLL_PROCESS_ATTACH setting */
	SetUnhandledExceptionFilter(exfilter);
	if(beenhere)
		return(0);
	beenhere = 1;
	if(timeGetDevCaps(&tc,sizeof(tc))==0)
		rsize = tc.wPeriodMin;
	timeBeginPeriod(rsize);
	tzset();
	dllinfo._ast_errno = (void*)erraddr;
	dllinfo._ast_environ = (void*)env;
	errno = 0;
	oenv = _environ;
	if(!dllinfo._ast_exitfn)
	{
		/* _DLL initialization */
		dllinfo._ast__exitfn = (int*)getsymbol(MODULE_msvcrt,"_exit");
		dllinfo._ast_exitfn = (int*)exit;
		if(!dllinfo._ast__argv)
			dllinfo._ast__argv = __argv;
		dllinfo._ast_msvc = (void*)atexit;
		*env = _environ;
	}
	if(*dllinfo._ast_herrno)
		pctype = *((short**)(dllinfo._ast_herrno));
	*dllinfo._ast_herrno = 0;
	saved_env = *env;
	errno_addr = erraddr;
	timezone = *dllinfo._ast_timezone;
	if(initfn = (void(*)(void))getsymbol(MODULE_ast,"_ast_init"))
		(*initfn)();
	if(environ_addr=env)
	{
		if(!P_CP->forksp)
		{
			int i;
			for(i=0; i < sizeof(xxaddr)/sizeof(*xxaddr); i++)
				xxaddr[i] = VirtualAlloc(NULL,8192,MEM_RESERVE,PAGE_READWRITE);
		}
		else
			P_CP->exitcode = 58;
		use_vmalloc = 1;
	}
	if(first_process)
	{
		deleted_init();
		fat_init();
	}
	if (isfdvalid(P_CP, 0) && Phandle(0))
		SetStdHandle(STD_INPUT_HANDLE, Phandle(0));
	if (isfdvalid(P_CP, 1))
	{
		if(getfdp(P_CP,1)->type==FDTYPE_PTY && Xhandle(1))
			SetStdHandle(STD_OUTPUT_HANDLE, Xhandle(1));
		else if(Phandle(1))
			SetStdHandle(STD_OUTPUT_HANDLE, Phandle(1));
	}
	if (isfdvalid(P_CP, 2))
	{
		if(getfdp(P_CP,2)->type==FDTYPE_PTY && Xhandle(2))
			SetStdHandle(STD_ERROR_HANDLE, Xhandle(2));
		else if(Phandle(2))
			SetStdHandle(STD_ERROR_HANDLE, Phandle(2));
	}
	if (!P_CP->prog_name[0] || P_CP->ppid == 1)
	{
		register char *arg, *cp = P_CP->prog_name, *dp = 0;
		register int c,quote=0;
		char *cpend = &P_CP->prog_name[sizeof(P_CP->prog_name)-1];
		arg = GetCommandLine();
		if(state.clone.helper && !memcmp(arg,CLONE,sizeof(CLONE)-1) && arg[sizeof(CLONE)-1]==' ')
			state.clone.helper = strtol(&arg[sizeof(CLONE)],0,10);
		else
			state.clone.helper = 0;
		while(c = *arg++)
		{
			if(c=='/' || c=='\\')
			{
				cp = P_CP->prog_name;
				dp = 0;
			}
			else if(!quote && (c==' ' || c == '\t'))
				break;
			else if(c=='"')
				quote = !quote;
			else if(cp < cpend && (*cp++ = c)=='.')
				dp = cp;
		}
		*cp = 0;
		if(dp == (cp-3) && (dp[0] == 'e' || dp[0] == 'E') && (dp[1] == 'x' || dp[1] == 'X') && (dp[2] == 'e' || dp[2] == 'E'))
			*(dp-1) = 0;
		strcpy(P_CP->cmd_line, P_CP->prog_name);
		if(c && (c = (int)(cp - P_CP->prog_name)) < sizeof(P_CP->cmd_line)-1)
			memcpy(&P_CP->cmd_line[c], arg-1, sizeof(P_CP->cmd_line)-c-1);
		if ((c = (int)(strlen(P_CP->cmd_line) + 2)) > (sizeof(P_CP->args) - 3))
			c = sizeof(P_CP->args) - 3;
		memcpy(P_CP->args, P_CP->cmd_line, c);
		P_CP->args[c++] = 0;
		P_CP->args[c++] = 0;
		P_CP->argsize = c;
	}
	else if(!strcmp(P_CP->prog_name,CLONE) && (last=strrchr(P_CP->cmd_line,' ')))
		state.clone.helper = strtol(last+1,0,10);
	P_CP->posixapp |= UWIN_PROC;
	if (P_CP->pid == 1)
		return run_init_proc();
	if(P_CP->priority)
		setpriority(PRIO_PROCESS,0,P_CP->priority);
	intercept_init();
	fdp = getfdp(P_CP,0);
	if(P_CP->forksp)
	{
		Pproc_t *proc =  P_CP->parent?proc_ptr(P_CP->parent):0;
		P_CP->exitcode = 59;
		if(fdp && fdp->devno)
			pdev = dev_ptr(fdp->devno);
		if(pdev && !pdev->state)
			ttysetmode(pdev);
		if(proc)
			InterlockedIncrement(&proc->inuse);
		else
			proc = proc_getlocked(P_CP->ppid, 0);
		if(proc && procexited(proc))
		{
			logmsg(0,"fork_exit slot=%d pid=%x-%d state=%d posix=0x%x code=%d name=%s",proc_slot(proc),proc->ntpid,proc->pid,proc->state,proc->posixapp,proc->exitcode,proc->prog_name);
			P_CP->state = proc->state;
			ExitProcess(P_CP->exitcode);
		}
		P_CP->exitcode = 59;
		if(proc)
			child_fork(proc,P_CP->forksp,(HANDLE*)&rsize);
		else
			logmsg(0, "fork error ppid=%d parent=%d", P_CP->ppid, P_CP->parent);
		/* shouldn't return */
		rsize = ( unsigned long )-1;
	}
	else
	{
		/* make a copy of the environment, leave room for 1 more */
		char *ptr,*cp,*first,**ep;
		initsig(1);
		if(ptr = cp = first = GetEnvironmentStrings())
		{
			char ***envp = dllinfo._ast_environ;
			for(rsize=2; *cp; rsize++)
				while(*cp++);
			cp++;
			dllinfo._ast_environ = 0;
			ep = malloc(rsize*sizeof(char*)+cp+1-first);
			dllinfo._ast_environ = envp;
			*env = ep;
			memcpy((void*)&ep[rsize],first,cp+1-first);
			first = (char*)&ep[rsize];
			for(cp=first;*cp;)
			{
				*ep++ = cp;
				while(*cp++);
			}
			*ep++ = 0;
			*ep = 0;
			FreeEnvironmentStrings(ptr);
		}
		fixenv(env, oenv);
		saved_env0 = **env;
		if(console>=0 && terminit(&Pfdtab[console]) <0)
		{
			logmsg(0, "terminit failed");
			return(0);
		}
		else if(console == TERM_REINIT)
			device_reinit();
		if(fdp && fdp->devno)
			pdev = dev_ptr(fdp->devno);
		if(pdev && !pdev->state)
			ttysetmode(pdev);
		/* give console write thread a chance to start */
#ifndef GTL_TERM
		if(P_CP->console && (pdev=dev_ptr(P_CP->console)) && P_CP->ntpid==pdev->NtPid)
		{
			rsize = WaitForSingleObject(pdev->WriteSyncEvent,500);
			if(rsize != 0)
				logmsg(0, "WaitForSingleObject=%d", rsize);
		}
#endif
		sigcheck(0);
		if(dllinfo._ast__argv)
		{
			register char *cp;
			if(cp = dllinfo._ast__argv[0])
			{
				register int c;
				if(cp[1]==':')
				{
					cp[1] = cp[0];
					cp[0] = '/';
					cp += 2;
				}
				while(c = *cp++)
				{
					if(c=='\\')
						cp[-1] = '/';
				}
			}
			if(!(P_CP->posixapp&POSIX_PARENT))
				file_expand();
#ifdef _ALPHA_
			else
				copy_args();
			proc_setcmdline(P_CP,dllinfo._ast__argv);
#endif
		}
		if (state.clone.helper)
		{
			char	name[60];
			HANDLE	hp;
			DWORD	ntpid=state.clone.helper;
			UWIN_PIPE_CLONE(name, ntpid);
			if (!(hp = CreateFile(name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)) || hp==INVALID_HANDLE_VALUE)

			{
				logerr(0,"%s: createpipe failed name=%s",CLONE,name);
				return 0;
			}
			if(hp=CreateThread(sattr(0),0,fh_main,(void*)hp,0,&state.clone.helper))
				CloseHandle(hp);
			else
				logerr(0,"%s thread failed to start", CLONE);
		}
		if(!begin)
			return(0);
		if(argc==0)
		{
			/* invoked from WinMain */
			rsize = (*((Winmain)begin))((HINSTANCE)argv[0],(HINSTANCE)argv[1],(LPSTR)argv[2],(int)argv[3]);
		}
		else
		{
			unmapfilename(argv[0], 0, 0);
			rsize = (*begin)(argc,argv,*env);
		}
	}
	if (P_CP->vfork)
		_vfork_done(0,rsize<<8);
	if (P_CP->curdir < 0)
		logmsg(0, "ast_runprog_curdir proc=%d pid=%x-%d posix=0x%x parent=%d name=%s cmd=%s", proc_slot(P_CP), P_CP->ntpid, P_CP->pid, P_CP->posixapp, P_CP->parent, P_CP->prog_name, P_CP->cmd_line);
	return(rsize);
}

#ifdef getenv

char *getenv(const char *string)
#undef	getenv
{
	return getenv(string);
}

#ifdef __EXPORT__
__EXPORT__
#endif

#endif

/*
 * look up value of given environment variable
 */
char *getenv(const char *string)
{
	register char c0,c1;
	register const char *cp, *sp;
	register char **av;
	if(!dllinfo._ast_environ)
	{
		/* call before initialization */
		static char buffer[512];
		if(GetEnvironmentVariable(string,buffer,sizeof(buffer)))
			return(buffer);
		return(0);
	}
	av = *(char***)dllinfo._ast_environ;
	if(!av || !string || (c0= *string)==0)
		return(0);
	if((c1=*++string)==0)
		c1= '=';
	while(cp = *av++)
	{
		if(cp[0]!=c0 || cp[1]!=c1)
			continue;
		sp = string;
		cp++;
		while(*sp && *sp++ == *cp++);
		if(*(sp-1) != *(cp-1))
			continue;
		if(*sp==0 && *cp=='=')
			return((char*)(cp+1));
	}
	return(0);
}

#define baseaddr(x)	((void*)((int)x&~0xffff))

#undef malloc
extern void *malloc(size_t size)
{
	register void *addr;
	begin_critical(CR_MALLOC);
	if(!use_vmalloc)
	{
		if(!clib_malloc)
			clib_malloc = (void*(*)(size_t))getsymbol(MODULE_msvcrt,"malloc");
		if(clib_malloc)
			addr = (*clib_malloc)(size);
		if(!malloc_first_addr)
			malloc_first_addr = addr;
	}
	else
		addr = _ast_malloc(size);
#ifdef MTRACE
	logmsg(0, "malloc size=%d addr=%p", size, addr);
#endif
	end_critical(CR_MALLOC);
	return(addr);
}

#undef realloc
extern void *realloc(void *addr, size_t size)
{
	static void *(*clib_realloc)(void*,size_t);
	begin_critical(CR_MALLOC);
	if(!use_vmalloc)
	{
		if(!clib_realloc)
			clib_realloc = (void*(*)(void*,size_t))getsymbol(MODULE_msvcrt,"realloc");
		if(clib_realloc)
			addr = (*clib_realloc)(addr,size);
	}
	else
		addr = _ast_realloc(addr,size);
#ifdef MTRACE
	logmsg(0, "realloc size=%d addr=%p", size, addr);
#endif
	end_critical(CR_MALLOC);
	return(addr);
}

#undef calloc
extern void *calloc(size_t n, size_t size)
{
	static void *(*clib_calloc)(size_t,size_t);
	register void *addr;
	begin_critical(CR_MALLOC);
	if(!use_vmalloc)
	{
		if(!clib_calloc)
			clib_calloc = (void*(*)(size_t,size_t))getsymbol(MODULE_msvcrt,"calloc");
		if(clib_calloc)
			addr = (*clib_calloc)(n,size);
	}
	else
	{
		addr = _ast_calloc(n,size);
	}
#ifdef MTRACE
	logmsg(0, "calloc size=%d addr=%p", size, addr);
#endif
	end_critical(CR_MALLOC);
	return(addr);
}


static int staklen;
static char *stakbase;

unsigned long _record[2048];
__inline int is_astaddr(void *addr)
{
	register unsigned x = ((unsigned int)addr)>>16;
	return(_record[x>>5]&(1L << (x&0x1f)) );
}

#undef free
extern void free(void* data)
{
	static void (*clib_free)(void*);
	void *baddr = baseaddr(data);
	MEMORY_BASIC_INFORMATION minfo;
	if(!data)
		return;
#ifdef MTRACE
	logmsg(0, "free addr=%p", data);
#endif
	if(staklen==0)
	{
		staklen = 4;
		if(VirtualQuery((void*)&baddr,&minfo,sizeof(minfo))==sizeof(minfo))
		{
			char *addr;
			stakbase = addr = minfo.AllocationBase;
			staklen = (int)minfo.RegionSize;
			while(VirtualQuery((void*)addr,&minfo,sizeof(minfo))==sizeof(minfo))
			{
				if(minfo.AllocationBase!=stakbase)
					break;
				staklen += (int)minfo.RegionSize;
				addr += minfo.RegionSize;
			}
		}
	}
	/* ignore frees from stack addresses */
	if((char*)data >= stakbase &&  (char*)data < &stakbase[staklen])
		return;
	begin_critical(CR_MALLOC);
	if(is_astaddr(data))
	{
		_ast_free(data);
	}
	else if(P_CP->state < PROCSTATE_EXITED)
		logmsg(LOG_PROC+3, "free with ptr=%p state=%(state)s exitcode=%d", data, P_CP->state, P_CP->exitcode);
	end_critical(CR_MALLOC);
}

/*
 * these are for use in memalign to block signals
 */
int	_sigblock(void)
{
	int sigsys = sigdefer(2);
	if(sigsys==2)
		logmsg(0, "memalign with sigsys==2");
	return(sigsys);
}

void	_sigunblock(int sigsys)
{
	sigcheck(sigsys);
}

#if _X86_

#undef mblen
int    mblen(const char *str, size_t size)
{
	static int (*mblenf)(const char*, size_t);
	if(!mblenf && !(mblenf = (int(*)(const char*,size_t))getsymbol(MODULE_msvcrt,"mblen")))
		return -1;
	return((*mblenf)(str,size));
}

#undef mbtowc
int    mbtowc(wchar_t *wc, const char *str, size_t size)
{
	wchar_t w;
	static int (*mbtowcf)(wchar_t*, const char*, size_t);
	if(!mbtowcf && !(mbtowcf = (int(*)(wchar_t*,const char*,size_t))getsymbol(MODULE_msvcrt,"mbtowc")))
		return -1;
	if(!wc)
		wc = &w;
	return((*mbtowcf)(wc,str,size));
}

#undef btowc
wint_t	btowc(int c)
{
	wchar_t	w;
	char	b[1];

	b[0] = (char)c;
	if (mbtowc(&w, b, sizeof(b)) < 0)
		return -1;
	return w;
}

#undef mbstowcs
size_t mbstowcs(wchar_t *wc, const char *str, size_t size)
{
	static size_t (*mbstowcsf)(wchar_t*,const char*,size_t);
	if(!mbstowcsf && !(mbstowcsf = (size_t(*)(wchar_t*,const char*,size_t))getsymbol(MODULE_msvcrt,"mbstowcs")))
		return 0;
	return((*mbstowcsf)(wc,str,size));
}

#undef wctomb
int    wctomb(char *str, wchar_t wc)
{
	static int (*wctombf)(char*, wchar_t);
	char buff[5];
	if(!wctombf && !(wctombf = (int(*)(char *str,const wchar_t))getsymbol(MODULE_msvcrt,"wctomb")))
		return -1;
	if(!str)
		str = buff;
	return((*wctombf)(str,wc));
}

#undef wcstombs
size_t wcstombs(char *str, const wchar_t *wc, size_t size)
{
	static size_t (*wcstombsf)(char*,const wchar_t*, size_t);
	if(!wcstombsf && !(wcstombsf = (size_t(*)(char *str,const wchar_t*,size_t))getsymbol(MODULE_msvcrt,"wcstombs")))
		return 0;
	return((*wcstombsf)(str,wc,size));
}

#undef	mbstate_t
#define mbstate_t	int

#undef mbrtowc

static size_t uwin_mbrtowc(wchar_t* t, const char* s, size_t n, mbstate_t* q)
{
	memset(q, 0, sizeof(mbstate_t));
	return mbtowc(t, s, n);
}

typedef size_t (*mbrtowc_f)(wchar_t*, const char*, size_t, mbstate_t*);

size_t mbrtowc(wchar_t* t, const char* s, size_t n, mbstate_t* q)
{
	static mbrtowc_f	mbrtowcf;

	if (!mbrtowcf && !(mbrtowcf = (mbrtowc_f)getsymbol(MODULE_msvcrt, "mbrtowc")))
		mbrtowcf = uwin_mbrtowc;
	return (*mbrtowcf)(t, s, n, q);
}

#undef wcrtomb

static size_t uwin_wcrtomb(char* t, wchar_t w, mbstate_t* q)
{
	memset(q, 0, sizeof(mbstate_t));
	return wctomb(t, w);
}

typedef size_t (*wcrtomb_f)(char*, wchar_t, mbstate_t*);

size_t wcrtomb(char* t, wchar_t w, mbstate_t* q)
{
	static wcrtomb_f	wcrtombf;

	if (!wcrtombf && !(wcrtombf = (wcrtomb_f)getsymbol(MODULE_msvcrt, "wcrtomb")))
		wcrtombf = uwin_wcrtomb;
	return (*wcrtombf)(t, w, q);
}

#endif

char *mbstrchr(const char *string, wchar_t d)
{
	wchar_t c;
	register int m;
	register const char *cp=string;
	while((m=mbtowc(&c,cp,MB_CUR_MAX))>0 && c)
	{
		if(c==d)
			return((char*)cp);
		cp+=m;
	}
	return(0);
}

char *mbstrrchr(const char *string, wchar_t d)
{
	wchar_t c;
	register const char *cp=string;
	register int m;
	char *ep = 0;
	while((m=mbtowc(&c,cp,MB_CUR_MAX))>0 && c)
	{
		if(c==d)
			ep = (char*)cp;
		cp+=m;
	}
	return((char*)ep);
}

#if defined(_M_ALPHA)
#   pragma intrinsic(_alloca)
#endif

#undef strtod
double strtod(const char *str, char **ptr)
{
	static double (*fun)(const char*, char**);
	static int ast=0;
	const char *cp=str;
	register int c;
	if(!fun && (fun = (double(*)(const char*,char**))getsymbol(MODULE_ast,"strtod")))
		ast = 1;
	else if(!fun && !(fun = (double(*)(const char*,char**))getsymbol(MODULE_msvcrt,"strtod")))
		return 0;
	if(ast)
		return((*fun)(str,ptr));
	while(c= *cp++)
		if((c<'0' || c>'9') && c!='.' && c!=',' && c!='+' && c!='-')
			break;
	if(fold(c)=='d')
	{
		double d;
		char *last,*sp = (char*)_alloca(c=(int)(cp-str));
		memcpy(sp,str,c);
		sp[c-1] = 0;
		d = (*fun)(sp,&last);
		if(ptr)
			*ptr = (char*)str + (last-sp);
		return(d);
	}
	return((*fun)(str,ptr));
}

#undef atof
double atof(const char *str)
{
	return(strtod(str,NULL));
}

int uwin_sys(int op, void* data, size_t size)
{
	logmsg(0, "uwin_sys op=%d data=%p size=%zd", op, data, size);
	if (data && size && IsBadReadPtr(data, size))
	{
		errno = EFAULT;
		return -1;
	}
	errno = ENOSYS;
	return -1;
}
