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
#include	"uwindef.h"
#include	"uwin_stack.h"
#include	<uwin.h>
#include	"vmalloc/vmalloc.h"

#define LOGCHUNK_OLD	(16L*1024*1024)
#define LOGCHUNK_NEW	(256L*1024)

#define LOG_MAX		200000

typedef struct Flag_s
{
	const char*		name;
	unsigned long		value;
} Flag_t;

static const char	category[] = " didpdidrdidpdidsdidpdidrdidpdid";

static const Flag_t	exetype_flags[] =
{
	"UWIN",			EXE_UWIN,
	"FORK",			EXE_FORK,
	"GUI",			EXE_GUI,
	"SCRIPT",		EXE_SCRIPT,
	"32",			EXE_32,
	"64",			EXE_64,

	0,0
};

static const Flag_t	page_flags[] =
{
#ifdef PAGE_NOACCESS
	"NOACCESS",		PAGE_NOACCESS,
#endif
#ifdef PAGE_READONLY
	"READONLY",		PAGE_READONLY,
#endif
#ifdef PAGE_READWRITE
	"READWRITE",		PAGE_READWRITE,
#endif
#ifdef PAGE_WRITECOPY
	"WRITECOPY",		PAGE_WRITECOPY,
#endif
#ifdef PAGE_EXECUTE
	"EXECUTE",		PAGE_EXECUTE,
#endif
#ifdef PAGE_EXECUTE_READ
	"EXECUTE_READ",		PAGE_EXECUTE_READ,
#endif
#ifdef PAGE_EXECUTE_READWRITE
	"EXECUTE_READWRITE",	PAGE_EXECUTE_READWRITE,
#endif
#ifdef PAGE_EXECUTE_WRITECOPY
	"EXECUTE_WRITECOPY",	PAGE_EXECUTE_WRITECOPY,
#endif
#ifdef PAGE_NOCACHE
	"NOCACHE",		PAGE_NOCACHE,
#endif
#ifdef PAGE_WRITECOMBINE
	"WRITECOMBINE",		PAGE_WRITECOMBINE,
#endif
#ifdef PAGE_GUARD
	"GUARD",		PAGE_GUARD,
#endif
	0,0
};

static const Flag_t	mem_flags[] =
{
#ifdef MEM_COMMIT
	"COMMIT",		MEM_COMMIT,
#endif
#ifdef MEM_RESERVE
	"RESERVE",		MEM_RESERVE,
#endif
#ifdef MEM_DECOMMIT
	"DECOMMIT",		MEM_DECOMMIT,
#endif
#ifdef MEM_RELEASE
	"RELEASE",		MEM_RELEASE,
#endif
#ifdef MEM_FREE
	"FREE",			MEM_FREE,
#endif
#ifdef MEM_RESET
	"RESET",		MEM_RESET,
#endif
#ifdef MEM_TOP_DOWN
	"TOP_DOWN",		MEM_TOP_DOWN,
#endif
#ifdef MEM_WRITE_WATCH
	"WRITE_WATCH",		MEM_WRITE_WATCH,
#endif
#ifdef MEM_PHYSICAL
	"PHYSICAL",		MEM_PHYSICAL,
#endif
#ifdef MEM_ROTATE
	"ROTATE",		MEM_ROTATE,
#endif
#ifdef MEM_LARGE_PAGES
	"LARGE_PAGES",		MEM_LARGE_PAGES,
#endif
#ifdef MEM_4MB_PAGES
	"4MB_PAGES",		MEM_4MB_PAGES,
#endif
	0,0
};

static const Flag_t	path_flags[] =
{
	"BIN",		P_BIN,
	"CASE",		P_CASE,
	"CHDIR",	P_CHDIR,
	"CREAT",	P_CREAT,
	"DELETE",	P_DELETE,
	"DIR",		P_DIR,
	"EXEC",		P_EXEC,
	"EXIST",	P_EXIST,
	"FILE",		P_FILE,
	"FORCE",	P_FORCE,
	"HPFIND",	P_HPFIND,
	"LINK",		P_LINK,
	"LSTAT",	P_LSTAT,
	"MYBUF",	P_MYBUF,
	"NOEXIST",	P_NOEXIST,
	"NOFOLLOW",	P_NOFOLLOW,
	"NOHANDLE",	P_NOHANDLE,
	"NOMAP",	P_NOMAP,
	"SLASH",	P_SLASH,
	"SPECIAL",	P_SPECIAL,
	"STAT",		P_STAT,
	"SYMLINK",	P_SYMLINK,
	"TRUNC",	P_TRUNC,
	"USEBUF",	P_USEBUF,
	"USECASE",	P_USECASE,
	"VIEW",		P_VIEW,
	"WOW",		P_WOW,
	0,0
};

static int logwrite(void *buf, DWORD sz)
{
	HANDLE			hp;
	DWORD			written;
	FILETIME		mtime;
	int			n;

	static int		count;
	static int		cpid = -1;
	static int		opid = -1;

	static const char	fallback[] = "log file write failed -- remaining messages will be to the standard error\r\n";

	if (P_CP && P_CP->pid != cpid)
	{
		cpid = P_CP->pid;
		count = 0;
	}
	if (count > LOG_MAX && (!P_CP || P_CP->pid!=1))
		return 0;
	n = 0;
	do
	{
		if (state.log)
		{
			if (WriteFile(state.log, buf, sz, &written, NULL))
			{
				if (++count >= LOG_MAX && (!P_CP || P_CP->pid!=1))
				{
					count = 0;
					logmsg(0, "too many log messages for process -- %d MAX", LOG_MAX);
					count = LOG_MAX + 1;
					return 0;
				}
				time_getnow(&mtime);
				SetFileTime(state.log, NULL, NULL, &mtime);
				return 1;
			}
			if (n++)
				break;
			CloseHandle(state.log);
		}
	} while (state.log = createfile(state.logpath, FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, sattr(0), OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
	hp = GetStdHandle(STD_ERROR_HANDLE);
	if (P_CP && P_CP->pid != opid || opid == -1)
	{
		opid = P_CP ? P_CP->pid : 0;
		WriteFile(hp, fallback, sizeof(fallback)-1, &written, NULL);
	}
	WriteFile(hp, buf, sz, &written, NULL);
	return 0;
}

/*
 * temporary circular format buffer
 */

char* fmtbuf(size_t n)
{
	char*		cur;

	static char	buf[4 * 1024];
	static char*	nxt = buf;

	if (n > (size_t)(&buf[elementsof(buf)] - nxt))
		nxt = buf;
	cur = nxt;
	nxt += n;
	return cur;
}

/*
 * format a handle type
 */

static char* fmthandle(int type)
{
	switch (type)
	{
	case HT_NONE:
		return "closed";
	case HT_FILE:
		return "file";
	case HT_SOCKET:
		return "socket";
	case HT_PIPE:
		return "pipe";
	case HT_NPIPE:
		return "named-pipe";
	case HT_THREAD:
		return "thread";
	case HT_PROC:
		return "process";
	case HT_EVENT:
		return "event";
	case HT_MUTEX:
		return "mutex";
	case HT_ICONSOLE:
		return "console-input";
	case HT_OCONSOLE:
		return "console-output";
	case HT_SEM:
		return "semaphore";
	case HT_TOKEN:
		return "token";
	case HT_MAPPING:
		return "file-mapping";
	case HT_COMPORT:
		return "comport";
	case HT_MAILSLOT:
		return "mail-slot";
	case HT_CHANGE:
		return "change-notification";
	case HT_NOACCESS:
		return "no-access";
	case HT_CHAR:
		return "character-device";
	case HT_DIR:
		return "directory";
	}
	return "unknown";
}

/*
 * format Flag_t flags
 */

static char* prtflags(register char* p, register char* e, const Flag_t* flags, unsigned long f, int sep)
{
	int	first;

	first = sep == '|';
	if (f)
		for (; flags->name; flags++)
			if (flags->value & f)
			{
				if (first)
					first = 0;
				else if (p < e)
					*p++ = sep;
				p += sfsprintf(p, e-p, "%s", flags->name);
			}
	return p;
}

/*
 * add vmalloc regions to the windows regions
 */

typedef struct Vmsegment_s
{
	void*		region;
	void*		segment;
	unsigned long	size;
} Vmsegment_t;

static Vmsegment_t*	vmsegment_nxt;
static Vmsegment_t*	vmsegment_end;

static int vmalloc_segment(Vmalloc_t* region, void* segment, size_t size, Vmdisc_t* dp)
{
	if (vmsegment_nxt >= vmsegment_end)
		return -1;
	vmsegment_nxt->region = region;
	vmsegment_nxt->segment = segment;
	vmsegment_nxt->size = (unsigned long)size;
	vmsegment_nxt++;
	return 0;
}

/*
 * check if hp is on the heap
 */

static int isheap(HANDLE hp, HANDLE* heaps, int nheaps)
{
	register int	i;

	for (i = 0; i < nheaps; i++)
		if (hp == heaps[i])
			return 1;
	return 0;
}

/*
 * format process memory regions
 */

#define PRT_NAME_WIDTH	12

static char* prtmemory(register char* p, register char* e, HANDLE ph)
{
	MEMORY_BASIC_INFORMATION	mi;
	char*				cur;
	char*				s;
	char*				w;
	Vmsegment_t*			seg;
	SIZE_T				size;
	int				base;
	int				n;
	unsigned long			f;
	DWORD				nheaps;
	HANDLE				heaps[128];
	Vmsegment_t			segments[128];

	p += sfsprintf(p, e-p, "memory regions\r\n");
	nheaps = GetProcessHeaps(elementsof(heaps), heaps);
	for (cur = 0;; cur += size)
	{
		if (VirtualQueryEx(ph, (void*)cur, &mi, sizeof(mi)) != sizeof(mi) || (size = mi.RegionSize) <= 0)
			break;
		if (mi.State != MEM_FREE)
		{
			if (base = cur && mi.AllocationBase == (void*)mi.BaseAddress)
				p += sfsprintf(p, e-p, "    %08x", mi.AllocationBase);
			else
				p += sfsprintf(p, e-p, "            ");
			p += sfsprintf(p, e-p, " %08x %7u ", mi.BaseAddress, size);
			f =  mi.State|mi.Type;
			w = p + (((e-p) > PRT_NAME_WIDTH) ? PRT_NAME_WIDTH : (e-p));
			if (base)
			{
				if (isheap((HANDLE)cur, heaps, nheaps))
					p += sfsprintf(p, e-p, "heap");
				else if (p < e)
				{
					if ((n = GetModuleFileName((HMODULE)cur, p, (int)(e-p))) > 0 && n < (e-p))
					{
						if (s = strrchr(p, '\\'))
							while (*p = *++s)
								p++;
						else
							p += n;
					}
#ifdef PAGE_GUARD
					else if (mi.Protect & PAGE_GUARD)
						p += sfsprintf(p, e-p, "stack");
#endif
#ifdef MEM_MAPPED
					else if (f & MEM_MAPPED)
						p += sfsprintf(p, e-p, "mapped");
#endif
#ifdef MEM_PRIVATE
					else if (f & MEM_PRIVATE)
						p += sfsprintf(p, e-p, "private");
#endif
					else
						p += sfsprintf(p, e-p, "(noname)");
				}
			}
#ifdef PAGE_GUARD
			else if (mi.Protect & PAGE_GUARD)
				p += sfsprintf(p, e-p, "stack");
#endif
			while (p < w)
				*p++ = ' ';
			p = prtflags(p, e, page_flags, mi.Protect, ' ');
			p = prtflags(p, e, mem_flags, f, ' ');
			if (p < (e-1))
			{
				*p++ = '\r';
				*p++ = '\n';
			}
		}
	}

	/*
	 * vmalloc region and segement details
	 */

	vmsegment_nxt = segments;
	vmsegment_end = segments + elementsof(segments);
	vmwalk(0, vmalloc_segment);
	for (seg = segments; seg < vmsegment_nxt; seg++)
	{
		if (seg == segments || seg->region != (seg-1)->region)
		{
			s = " vmalloc";
			p += sfsprintf(p, e-p, "    %08x", seg->region);
		}
		else
		{
			s = "";
			p += sfsprintf(p, e-p, "            ");
		}
		p += sfsprintf(p, e-p, " %08x %7u%s\r\n", seg->segment, seg->size, s);
	}

	/*
	 * logging thinks this is one line
	 */

	if (*(p - 1) == '\n')
		p -= 2;
	return p;
}

/*
 * format the program counter
 */

static char* fmtpc(DWORD pc)
{
	size_t				siz = 256;
	char*				buf;
	MEMORY_BASIC_INFORMATION	minfo;
	char*				bp;
	char*				cp;
	int				n;

	buf = fmtbuf(siz);
	if (VirtualQuery((void*)pc, &minfo, sizeof(minfo)) != sizeof(minfo))
	{
		logerr(0, "VirtualQuery");
		n = (int)sfsprintf(buf, siz, "(unknown)");
	}
	else
	{
		GetModuleFileName(minfo.AllocationBase, buf, (int)siz);
		buf[siz-1] = 0;
		if (cp = strrchr(buf, '\\'))
		{
			for (bp = buf; *bp = *++cp; bp++);
			n = (int)(bp - buf);
		}
		else
			n = (int)strlen(buf);
		if (n > 4 && buf[n-4] == '.' && (buf[n-3] == 'e' || buf[n-3] == 'E') && (buf[n-2] == 'x' || buf[n-2] == 'X') && (buf[n-1] == 'e' || buf[n-1] == 'E'))
			buf[n-=4] = 0;
		if (buf[0])
			pc = (int)((char*)pc - (char*)minfo.AllocationBase);
		else
			n = (int)sfsprintf(buf, siz, "(unknown)");
	}
	sfsprintf(buf+n, siz-n, "+%x", pc);
	buf[siz-1] = 0;
	return buf;
}

/*
 * format a token privileges list
 */

static char* fmtprivileges(TOKEN_PRIVILEGES* pp)
{
	DWORD	n = 256;
	DWORD	i;
	char*	buf;
	char*	s;
	char*	e;

	buf = fmtbuf(n);
	s = buf;
	e = buf + n - 1;
	for (i = 0; i < pp->PrivilegeCount; i++)
	{
		if ((e  - s) < 20)
			break;
		if (s > buf)
			*s++ = ',';
		n = (DWORD)(e - s);
		if (LookupPrivilegeName(0, &pp->Privileges[i].Luid, s, &n))
		{
			s += n;
			if ((e - s) < 9)
				break;
			*s++ = '=';
		}
		s += sfsprintf(s, e - s, "%x", pp->Privileges[i].Attributes);
	}
	if (s == buf)
		return "(nil)";
	*s = 0;
	return buf;
}

/*
 * format a security id
 */

static char* fmtsid(SID* sid)
{
	size_t	siz = 256;
	char*	buf;

	buf = fmtbuf(siz);
	printsid(buf, siz, (SID*)sid, 0);
	buf[siz-1] = 0;
	return buf;
}

static const char*	fmt_fdtype[] =
{
	"INIT",
	"FILE",
	"TTY",
	"PIPE",
	"SOCKET",
	"DIR",
	"CONSOLE",
	"NULL",
	"DEVFD",
	"REG",
	"PROC",
	"NPIPE",
	"PTY",
	"WINDOW",
	"CLIPBOARD",
	"MOUSE",
	"MODEM",
	"UNIX",
	"INET",
	"TAPE",
	"FLOPPY",
	"TFILE",
	"FIFO",
	"XFILE",
	"DGRAM",
	"LP",
	"ZERO",
	"EPIPE",
	"EFIFO",
	"NETDIR",
	"AUDIO",
	"RANDOM",
	"MEM",
	"FULL",
	"NONE",
};

static const char*	fmt_proc_state[] =
{
	"RRUNNING",
	"SCHILDWAIT",
	"SSIGWAIT",
	"SREAD",
	"SWRITE",
	"ISPAWN",
	"TSTOPPED",
	"SSELECT",
	"SMSGWAIT",
	"SSEMWAIT",
	"?UNKNOWN",
	"NEXITED",
	"NTERMINATE",
	"NABORT",
	"ZZOMBIE"
};

/*
 * format process state
 */

char* fmtstate(int r, int c)
{
	char*	s;

	if (r >= 0 && r < elementsof(fmt_proc_state))
	{
		if (c)
		{
			s = fmtbuf(2);
			s[0] = *fmt_proc_state[r];
			s[1] = 0;
		}
		else
			s = (char*)fmt_proc_state[r] + 1;
	}
	else if (c)
	{
		s = fmtbuf(2);
		s[0] = 'N';
		s[1] = 0;
	}
	else
		sfsprintf(s = fmtbuf(32), 32, "STATE+%d", r);
	return s;
}

/*
 *  buffered printf
 */

#define VASIGNED(a,n)	(((n)>sizeof(long))?va_arg(a,__int64):((n)?va_arg(a,long):va_arg(a,int)))
#define VAUNSIGNED(a,n)	(((n)>sizeof(unsigned long))?va_arg(a,unsigned __int64):((n)?va_arg(a,unsigned long):va_arg(a,unsigned int)))

/*
 * stripped down printf -- only { %c %[l][l][dopux] %s }
 */

static ssize_t bvprintf(char** buf, char* end, register const char* format, va_list ap)
{
	register int		c;
	register char*		p;
	register char*		e;
	__int64			n;
	unsigned __int64	u;
	ssize_t			z;
	int			w;
	int			x;
	int			l;
	int			f;
	int			g;
	int			r;
	FILETIME*		ft;
	SYSTEMTIME		st;
	char*			s;
	char*			b;
	char*			t;
	void*			v;
	wchar_t*		y;
	char			num[32];
	char			typ[32];

	static char	digits[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLNMOPQRSTUVWXYZ@_";

	p = *buf;
	e = end;
	for (;;)
	{
		switch (c = *format++)
		{
		case 0:
			goto done;
		case '%':
			if (*format == '-')
			{
				format++;
				l = 1;
			}
			else
				l = 0;
			if (*format == '0')
			{
				format++;
				f = l ? ' ' : '0';
			}
			else
				f = ' ';
			if ((c = *format) == '*')
			{
				format++;
				w = va_arg(ap, int);
			}
			else
			{
				w = 0;
				while (c >= '0' && c <= '9')
				{
					w = w * 10 + c - '0';
					c = *++format;
				}
			}
			x = r = 0;
			if (c == '.')
			{
				if ((c = *++format) == '*')
				{
					format++;
					x = va_arg(ap, int);
				}
				else
					while (c >= '0' && c <= '9')
					{
						x = x * 10 + c - '0';
						c = *++format;
					}
				if (c == '.')
				{
					if ((c = *++format) == '*')
					{
						format++;
						r = va_arg(ap, int);
					}
					else
						while (c >= '0' && c <= '9')
						{
							r = r * 10 + c - '0';
							c = *++format;
						}
				}
			}
			if (c == '(')
			{
				t = typ;
				while (c = *++format)
				{
					if (c == ')')
					{
						format++;
						break;
					}
					if (t < &typ[sizeof(typ)-1])
						*t++ = c;
				}
				*t = 0;
				t = typ;
			}
			else
				t = 0;
			n = 0;
			g = 0;
			b = num;
			for (;;)
			{
				switch (c = *format++)
				{
				case 'I':
					if (*format == '*')
					{
						format++;
						n = va_arg(ap, int);
					}
					continue;
				case 'l':
					if (n <= sizeof(long))
						n += sizeof(long);
					continue;
				case 'L':
					n = sizeof(__int64);
					continue;
				case 'z':
					n = sizeof(size_t);
					continue;
				}
				break;
			}
			switch (c)
			{
			case 0:
				break;
			case 'c':
				*b++ = va_arg(ap, int);
				break;
			case 'd':
				n = VASIGNED(ap, n);
				if (n < 0)
				{
					g = '-';
					n = -n;
				}
				u = n;
				goto dec;
			case 'o':
				u = VAUNSIGNED(ap, n);
				do *b++ = (char)(u & 07) + '0'; while (u >>= 3);
				break;
			case 's':
				if (!t)
				{
					s = va_arg(ap, char*);
					if (!s)
						s = "(null)";
#if 0
					else if (IsBadStringPtr(s, 2))
						s = "(invalid)";
#endif
					if (n)
					{
						y = (wchar_t*)s;
						r = (int)wcslen(y);
						if (l && x && r > x)
							r = x;
						s = fmtbuf(4 * r + 1);
						if (!WideCharToMultiByte(CP_ACP, 0, y, r, s, 4 * r + 1, NULL, NULL))
							s = "(BadUnicode)";
					}
				}
				else if (streq(t, "exetype"))
				{
					p = prtflags(p, e, exetype_flags, va_arg(ap, unsigned long), '|');
					continue;
				}
				else if (streq(t, "fdtype"))
				{
					r = va_arg(ap, long);
					if (r >= 0 && r < elementsof(fmt_fdtype))
						s = (char*)fmt_fdtype[r];
					else
						sfsprintf(s = fmtbuf(32), 32, "FDTYPE+%d", r);
				}
				else if (streq(t, "filetime"))
				{
					ft = va_arg(ap, FILETIME*);
					FileTimeToSystemTime(ft, &st);
					sfsprintf(s = fmtbuf(20), 20, "%.4d-%02d-%02d+%02d:%02d:%02d"
						, st.wYear
						, st.wMonth
						, st.wDay
						, st.wHour
						, st.wMinute
						, st.wSecond
						);
				}
				else if (streq(t, "handle"))
					s = fmthandle(va_arg(ap, long));
				else if (streq(t, "memory"))
				{
					p = prtmemory(p, e, va_arg(ap, HANDLE));
					continue;
				}
				else if (streq(t, "pathflags"))
				{
					p = prtflags(p, e, path_flags, va_arg(ap, unsigned long), '|');
					continue;
				}
				else if (streq(t, "pc"))
					s = fmtpc(va_arg(ap, DWORD));
				else if (streq(t, "privileges"))
					s = fmtprivileges(va_arg(ap, TOKEN_PRIVILEGES*));
				else if (streq(t, "sid"))
					s = fmtsid(va_arg(ap, SID*));
				else if (streq(t, "stack-exception"))
				{
					EXCEPTION_POINTERS*	ep = va_arg(ap, EXCEPTION_POINTERS*);

					if (Share->stack_trace)
						uwin_stack_exception(ep, ",", s = fmtbuf(4*1024), 4*1024);
					else
						s = fmtpc((DWORD)prog_pc(ep->ContextRecord));
				}
				else if (streq(t, "stack-process"))
					uwin_stack_process(0, va_arg(ap, HANDLE), ",", s = fmtbuf(4*1024), 4*1024);
				else if (streq(t, "stack-thread"))
					uwin_stack_thread(0, va_arg(ap, HANDLE), ",", s = fmtbuf(4*1024), 4*1024);
				else if (streq(t, "state"))
				{
					r = va_arg(ap, long);
					s = fmtstate(r, w == 1);
				}
				else
					s = t;
				if (w || l && x)
				{
					if (!l || !x)
						x = (int)strlen(s);
					if (!w)
						w = x;
					n = w - x;
					if (l)
					{
						while (w-- > 0)
						{
							if (p >= e)
								goto done;
							if (!(*p = *s++))
								break;
							p++;
						}
						while (n-- > 0)
						{
							if (p >= e)
								goto done;
							*p++ = f;
						}
						continue;
					}
					while (n-- > 0)
					{
						if (p >= e)
							goto done;
						*p++ = f;
					}
				}
				for (;;)
				{
					if (p >= e)
						goto done;
					if (!(*p = *s++))
						break;
					p++;
				}
				continue;
			case 'u':
				u = VAUNSIGNED(ap, n);
			dec:
				if (r <= 0 || r >= sizeof(digits))
					r = 10;
				do *b++ = digits[u % r]; while (u /= r);
				break;
			case 'p':
				v = va_arg(ap, void*);
				if (u = (unsigned __int64)((char*)v - (char*)0))
				{
					if (sizeof(v) == 4)
						u &= 0xffffffff;
					g = 'x';
					w = 10;
					f = '0';
					l = 0;
				}
				goto hex;
			case 'x':
				u = VAUNSIGNED(ap, n);
			hex:
				do *b++ = ((c = (char)(u & 0xf)) >= 0xa) ? c - 0xa + 'a' : c + '0'; while (u >>= 4);
				break;
			case 'z':
				u = VAUNSIGNED(ap, n);
				do *b++ = ((c = (char)(u & 0x1f)) >= 0xa) ? c - 0xa + 'a' : c + '0'; while (u >>= 5);
				break;
			default:
				if (p >= e)
					goto done;
				*p++ = c;
				continue;
			}
			if (w)
			{
				if (g == 'x')
					w -= 2;
				else
					if (g) w -= 1;
				n = w - (b - num);
				if (!l)
				{
					if (g && f != ' ')
					{
						if (g == 'x')
						{
							if (p >= e)
								goto done;
							*p++ = '0';
							if (p >= e)
								goto done;
							*p++ = 'x';
						}
						else if (p >= e)
							goto done;
						else
							*p++ = g;
						g = 0;
					}
					while (n-- > 0)
					{
						if (p >= e)
							goto done;
						*p++ = f;
					}
				}
			}
			if (g == 'x')
			{
				if (p >= e)
					goto done;
				*p++ = '0';
				if (p >= e)
					goto done;
				*p++ = 'x';
			}
			else if (g)
			{
				if (p >= e)
					goto done;
				*p++ = g;
			}
			while (b > num)
			{
				if (p >= e)
					goto done;
				*p++ = *--b;
			}
			if (w && l)
				while (n-- > 0)
				{
					if (p >= e)
						goto done;
					*p++ = f;
				}
			continue;
		default:
			if (p >= e)
				goto done;
			*p++ = c;
			continue;
		}
		break;
	}
 done:
	if (p < e)
		*p = 0;
	z = (ssize_t)(p - *buf);
	*buf = p;
	return z;
}

ssize_t bprintf(char** buf, char* end, const char* format, ...)
{
	va_list	ap;
	ssize_t	n;

	va_start(ap, format);
	n = bvprintf(buf, end, format, ap);
	va_end(ap);
	return n;
}

ssize_t sfsprintf(char* buffer, size_t size, const char* format, ...)
{
	va_list	ap;
	char**	buf;
	char*	end;
	ssize_t	n;

	if (!size)
		return 0;
	va_start(ap, format);
	buf = &buffer;
	end = buffer + size;
	n = bvprintf(buf, end, format, ap);
	va_end(ap);
	return n;
}

void _assert(const char* expr, const char* file, const char* line)
{
	char	buf[1024];
	DWORD	len;
	DWORD	written;

	len = (DWORD)sfsprintf(buf, sizeof(buf), "assertion failed: %s, file %s, line %d\n", expr, file, line);
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, len, &written, NULL);
	abort();
}

/*
 * limit the log file size to LOGCHUNK_OLD+2*LOGCHUNK_NEW
 * maintain 3 sections
 *	1 (LOGCHUNK_OLD) the original set of log messages
 *	2 (LOGCHUNK_NEW) the most recent set of messages
 *	3 (LOGCHUNK_NEW) new messages
 * when 3 overflows a line of ...'s is written after 1 and
 * 3 is moved to 2 and the log is truncated to 1+2
 */

static void logback(int level)
{
	char*		cur;
	char*		end;
	char*		s;
	char		buf[LOGCHUNK_NEW];
	DWORD		sz;

	if (!Share->backlog++)
	{
		if (SetFilePointer(state.log, -LOGCHUNK_NEW, 0, FILE_END) != INVALID_SET_FILE_POINTER &&
		    ReadFile(state.log, buf, LOGCHUNK_NEW, &sz, NULL) &&
		    sz == LOGCHUNK_NEW &&
		    SetFilePointer(state.log, LOGCHUNK_OLD, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
		{
			s = buf;
			cur = s + 32;
			end = buf + LOGCHUNK_NEW - 2;
			*s++ = '\r';
			*s++ = '\n';
			while (*s != '\n' || s < cur)
				if (s >= end)
				{
					s = cur;
					*s++ = '\r';
					*s = '\n';
					break;
				}
				else
					*s++ = '.';
			WriteFile(state.log, buf, LOGCHUNK_NEW, &sz, NULL);
			SetEndOfFile(state.log);
		}
	}
	Share->backlog--;
	CloseHandle(state.log);
	state.log = createfile(state.logpath, FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, sattr(0), OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

/*
 * append printf-style message line to the log
 * format should not contain newlines
 */

ssize_t logsrc(int level, const char* file, int line, const char* format, ...)
{
	va_list		ap;
	char		buf[16*1024];
	char*		cur;
	char*		end;
	char*		s;
	SYSTEMTIME	sys;
	DWORD		err;
	DWORD		sz;
	DWORD		hz;
	int		n;

	err = GetLastError();
	if (!logshow(level) || !state.log)
		return 0;
	cur = buf;
	end = buf + sizeof(buf) - 2;

	/*
	 * fixed message prefix
	 */

	if(state.clone.hp)
	{
		uint64_t	t;
		FILETIME	*ft = (FILETIME*)&t;
		GetSystemTimeAsFileTime(ft);
		t -= (*dllinfo._ast_timezone)*10000000LL;
		FileTimeToSystemTime(ft,&sys);
	}
	else
		GetLocalTime(&sys);
	n = GetCurrentProcessId();
	bprintf(&cur, end, "%.4d-%02d-%02d+%02d:%02d:%02d %6d %8d %8d %5x %3d %c%c (%s%s) "
			, sys.wYear
			, sys.wMonth
			, sys.wDay
			, sys.wHour
			, sys.wMinute
			, sys.wSecond
			, n
			, P_CP ? P_CP->pid : (Share ? nt2unixpid(n) : 1)
			, P_CP ? P_CP->ppid : 0
			, GetCurrentThreadId()
			, P_CP ? proc_slot(P_CP) : 0
			, (level & LOG_INIT) ? 'I' : !(level & LOG_TYPE) ? ' ' : (category[(level & LOG_TYPE) >> LOG_LEVEL_BITS] - ((level & LOG_SYSTEM) ? 32 : 0))
			, '0' + (level & LOG_LEVEL)
			, P_CP && *P_CP->prog_name ? P_CP->prog_name : state.local
			, state.wow ? "*32" : ""
			);
	if (file)
	{
		if (s = strrchr(file, '/'))
			file = (const char*)s;
		bprintf(&cur, end, "%s:%d%s", file, line, format && *format == ':' ? "" : ": ");
	}

	/*
	 * the caller part
	 */

	va_start(ap, format);
	bvprintf(&cur, end, format, ap);
	va_end(ap);

	/*
	 * finally append to the log
	 */

	if (cur > buf)
	{
		if ((level & LOG_SYSTEM) && err && (int)(end - cur) > 32)
		{
			cur += sfsprintf(cur, end - cur, " [%lu:", err);
			if (!(n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, err, 0, cur, (int)(end - cur), 0)))
				n = (int)sfsprintf(cur, end - cur, "Unknown error code %d.\n", err);
			if (n >= (int)(end - cur))
				cur[n-1] = 0;
			if (s = strchr(cur, '\n'))
				n = (int)(s - cur);
			cur += n;
			if (*(cur-1) == '\r')
				cur--;
			if (*(cur-1) == '.')
				cur--;
			if (cur < end)
				*cur++ = ']';
		}
		if (cur > end)
			cur = end;
		*cur++ = '\r';
		*cur++ = '\n';
		if (Share && Share->backlog > 0)
		{
			Sleep(400);
			if (Share->backlog > 0)
				Share->backlog = 0;
		}
		logwrite(buf, (int)(cur - buf));
		if (Share && !Share->backlog && (sz = GetFileSize(state.log, &hz)) != INVALID_FILE_SIZE && sz > (LOGCHUNK_OLD + 2 * LOGCHUNK_NEW))
			logback(level);
	}
	SetLastError(err);
	return (ssize_t)(cur - buf);
}
