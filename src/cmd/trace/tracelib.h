/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
/*
 * NOTE: to be included by tracehdr.h
 */

#include "tracesym.h"

#define interval	timeval

#ifdef _DLL

#if _X86_ || _X64_

#define _lib_cycles	1

#include <intrin.h>

__inline unsigned int_8
cycles(void)
{
	return __rdtsc();
}

#endif

#if _ALPHA_

#define _lib_cycles	1

__inline unsigned int_8
cycles(void)
{
	unsigned int_8		cy;
        unsigned int		lo;
        unsigned int		hi;

	static unsigned int	ohi;
	static unsigned int	olo;

	lo = __RCC();
	if (lo > olo)
		hi = ohi;
	else
		hi = ++ohi;
	olo = lo;
	cy = hi;
	return (cy<<32) | lo;
}

#endif

#endif

#if !_lib_cycles

__inline unsigned int_8
cycles(void)
{
	static unsigned int_8	cy;

	return cy++;
}

#endif

/*
 * flush the buffer to the output handle
 * nice line breaks are attempted
 * its a hog, but tracing isn't for speed
 */

static char*
trace_flush(void)
{
	register char*	b;
	register char*	s;
	register char*	e;
	register char*	l;
	register char*	x;
	register char*	z;
	long		r;

	if (trace.bp > trace.buf)
	{
		if (trace.cc >= 0)
		{
			b = s = l = z = trace.buf;
			e = trace.bp;
			x = b + TRACE_LINE - trace.cc - 1;
			if (x > e)
				x = e;
			for (;;)
			{
				if (s >= x)
				{
					if (s >= e)
					{
						WriteFile(trace.hp, b, (DWORD)(s - b), &r, NULL);
						break;
					}
					if (z == b || trace.buffer.string)
					{
						WriteFile(trace.hp, b, (DWORD)(s - b), &r, NULL);
						WriteFile(trace.hp, "\\\n        ", 10, &r, NULL);
						z = s;
					}
					else
					{
						WriteFile(trace.hp, b, (DWORD)(z - b - 1), &r, NULL);
						WriteFile(trace.hp, "\n        ", 9, &r, NULL);
						s = z;
					}
					trace.cc = 8;
					b = l = s;
					x = s + TRACE_LINE - 9;
					if (x > e)
						x = e;
				}
				switch (*s++)
				{
				case '\n':
					trace.cc = 0;
					l = z = s;
					x = s + TRACE_LINE - 1;
					if (x > e)
						x = e;
					break;
				case ' ':
					z = s;
					break;
				}
			}
			trace.cc += (int)(s - l);
		}
		else
			WriteFile(trace.hp, trace.buf, (DWORD)(trace.bp - trace.buf), &r, NULL);
		trace.bp = trace.buf;
	}
	return trace.bp;
}

/*
 * stripped down vprintf -- only { %c %[l[l]][dopux] %s }
 */

static int
trace_vprintf(register const char* format, va_list ap)
{
	register int	c;
	register char*	p;
	register char*	e;
	int		w;
	int		l;
	int		f;
	int		g;
	int		r;
	long		n;
	unsigned long	u;
#ifdef int_8
	int_8		q;
	unsigned int_8	v;
#endif
	char*		s;
	char*		b;
	char*		x;
	char		num[32];

	static char	digits[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLNMOPQRSTUVWXYZ@_";

	p = trace.bp;
	e = trace.ep;
	for (;;)
	{
		switch (c = *format++)
		{
		case 0:
			break;
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
			r = 0;
			if (c == '.')
			{
				if ((c = *++format) == '*')
				{
					format++;
					va_arg(ap, int);
				}
				else
					while (c >= '0' && c <= '9')
						c = *++format;
				if (c == '.')
				{
					if ((c = *++format) == '*')
					{
						format++;
						r = va_arg(ap, int);
					}
					else while (c >= '0' && c <= '9')
					{
						r = r * 10 + c - '0';
						c = *++format;
					}
				}
			}
			if ((c = *format++) != 'l')
				n = 0;
			else if ((c = *format++) != 'l')
				n = 1;
			else
			{
				n = 2;
				c = *format++;
			}
			g = 0;
			b = num;
			switch (c)
			{
			case 0:
				break;
			case 'c':
				*b++ = va_arg(ap, int);
				break;
			case 'd':
				switch (n)
				{
				case 0:
					n = va_arg(ap, int);
					break;
				default:
#ifdef int_8
					q = va_arg(ap, int_8);
					if (q < 0)
					{
						g = '-';
						q = -q;
					}
					v = q;
					goto dec_8;
				case 1:
#endif
					n = va_arg(ap, long);
					break;
				}
				if (n < 0)
				{
					g = '-';
					n = -n;
				}
				u = n;
				goto dec;
			case 'o':
				switch (n)
				{
				case 0:
					u = va_arg(ap, unsigned int);
					break;
				default:
#ifdef int_8
					v = va_arg(ap, unsigned int_8);
					goto oct_8;
				case 1:
#endif
					u = va_arg(ap, unsigned long);
					break;
				}
				do *b++ = (char)((u & 07) + '0'); while (u >>= 3);
				break;
			case 's':
				s = va_arg(ap, char*);
				if (!s)
					s = "(null)";
				if (w)
				{
					n = (int)(w - strlen(s));
					if (l)
					{
						while (w-- > 0)
						{
							if (p >= e)
								p = trace_flush();
							if (!(*p = *s++))
								break;
							p++;
						}
						while (n-- > 0)
						{
							if (p >= e)
								p = trace_flush();
							*p++ = f;
						}
						continue;
					}
					while (n-- > 0)
					{
						if (p >= e)
							p = trace_flush();
						*p++ = f;
					}
				}
				for (;;)
				{
					if (p >= e)
						p = trace_flush();
					if (!(*p = *s++))
						break;
					p++;
				}
				continue;
			case 'u':
				switch (n)
				{
				case 0:
					u = va_arg(ap, unsigned int);
					break;
				default:
#ifdef int_8
					v = va_arg(ap, unsigned int_8);
					goto dec_8;
				case 1:
#endif
					u = va_arg(ap, unsigned long);
					break;
				}
			dec:
				if (r <= 0 || r >= sizeof(digits))
					r = 10;
				do *b++ = digits[u % r]; while (u /= r);
				break;
			case 'p':
				if (x = va_arg(ap, char*))
				{
					g = 'x';
					w = 2 * (sizeof(char*) + 1);
					f = '0';
					l = 0;
				}
#ifdef int_8
				if (sizeof(char*) == sizeof(int_8))
				{
					v = (unsigned int_8)x;
					goto hex_8;
				}
#endif
				u = (unsigned long)x;
				goto hex;
			case 'x':
				switch (n)
				{
				case 0:
					u = va_arg(ap, unsigned int);
					break;
				default:
#ifdef int_8
					v = va_arg(ap, unsigned int_8);
					goto hex_8;
				case 1:
#endif
					u = va_arg(ap, unsigned long);
					break;
				}
			hex:
				do *b++ = (char)(((n = (u & 0xf)) >= 0xa) ? n - 0xa + 'a' : n + '0'); while (u >>= 4);
				break;
			default:
				if (p >= e)
					p = trace_flush();
				*p++ = c;
				continue;
#ifdef int_8
			dec_8:
				if (r <= 0 || r >= sizeof(digits))
					r = 10;
				do *b++ = digits[v % r]; while (v /= r);
				break;
			hex_8:
				do *b++ = (char)(((n = (long)(v & 0xf)) >= 0xa) ? n - 0xa + 'a' : n + '0'); while (v >>= 4);
				break;
			oct_8:
				do *b++ = (char)((v & 07) + '0'); while (v >>= 3);
				break;
#endif
			}
			if (w)
			{
				if (g == 'x')
					w -= 2;
				else if (g)
					w -= 1;
				n = (int)(w - (b - num));
				if (!l)
				{
					if (g && f != ' ')
					{
						if (p >= e)
							p = trace_flush();
						if (g == 'x')
						{
							*p++ = '0';
							if (p >= e)
								p = trace_flush();
						}
						*p++ = g;
						g = 0;
					}
					while (n-- > 0)
					{
						if (p >= e)
							p = trace_flush();
						*p++ = f;
					}
				}
			}
			if (g)
			{
				if (p >= e)
					p = trace_flush();
				if (g == 'x')
				{
					*p++ = '0';
					if (p >= e)
						p = trace_flush();
				}
				*p++ = g;
			}
			while (b > num)
			{
				if (p >= e)
					p = trace_flush();
				*p++ = *--b;
			}
			if (w && l)
				while (n-- > 0)
				{
					if (p >= e)
						p = trace_flush();
					*p++ = f;
				}
			continue;
		default:
			if (p >= e)
				p = trace_flush();
			*p++ = c;
			continue;
		}
		break;
	}
	if (p >= e)
		p = trace_flush();
	*p = 0;
	c = (int)(p - trace.bp);
	trace.bp = p;
	return c;
}

/*
 * stripped down printf
 */

static int
trace_printf(const char* format, ...)
{
	va_list	ap;
	int	n;

	va_start(ap, format);
	n = trace_vprintf(format, ap);
	va_end(ap);
	return n;
}

/*
 * stripped down error()
 */

static int
trace_error(int level, const char* format, ...)
{
	va_list	ap;
	int	n;

	n = 0;
	if (level >= trace.debug)
	{
		if (level)
		{
			n += trace_printf("trace: ");
			if (level == 1)
				n += trace_printf("warning: ");
			else if (level < -1)
				n += trace_printf("debug%d: ", level);
			else if (level < 0)
				n += trace_printf("debug: ");
		}
		va_start(ap, format);
		n += trace_vprintf(format, ap);
		va_end(ap);
		n += trace_printf("\n");
		trace_flush();
	}
	return n;
}

typedef struct			/* ops for each char in mode string	*/
{
	int	mask1;		/* first mask				*/
	int	shift1;		/* first shift count			*/
	int	mask2;		/* second mask				*/
	int	shift2;		/* second shift count			*/
	char*	name;		/* mode char using mask/shift as index	*/
} Trace_mode_t;

static const Trace_mode_t	trace_mode_tab[] =
{
	0170000, 12, 0000000, 0, "-pc?d?b?-Cl?s???",
	0000400,  8, 0000000, 0, "-r",
	0000200,  7, 0000000, 0, "-w",
	0004000, 10, 0000100, 6, "-xSs",
	0000040,  5, 0000000, 0, "-r",
	0000020,  4, 0000000, 0, "-w",
	0002000,  9, 0000010, 3, "-xls",
	0000004,  2, 0000000, 0, "-r",
	0000002,  1, 0000000, 0, "-w",
	0001000,  8, 0000001, 0, "-xTt",
};

static void
trace_mode(register long mode, int offset)
{
	register const Trace_mode_t*	p;

	if (trace.oflags & O_CREAT)
	{
		for (p = trace_mode_tab + offset; p < &trace_mode_tab[elementsof(trace_mode_tab)] && trace.bp < trace.ep; p++)
			*trace.bp++ = p->name[((mode & p->mask1) >> p->shift1) | ((mode & p->mask2) >> p->shift2)];
	}
	trace.oflags = O_CREAT;
}

/*
 * trace a time_t
 */

static void
trace_time(time_t t)
{
	register struct tm*	tm;

	if (t == 0)
		trace_printf("0");
	else if (tm = localtime(&t))
		trace_printf("%4d-%02d-%02d+%02d:%02d:%02d", 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	else
		trace_printf("#%lu", t);
}

/*
 * trace nsecs
 */

static void
trace_nsec(unsigned long nsec)
{
	trace_printf(".%09lu", nsec);
}

/*
 * trace timeval pointer
 */

static void
trace_timespec(struct timespec* tv)
{
	trace_time(tv->tv_sec);
	trace_nsec(tv->tv_nsec);
}

/*
 * trace usecs
 */

static void
trace_usec(unsigned long usec)
{
	trace_printf(".%06lu", usec);
}

/*
 * trace timeval pointer
 */

static void
trace_timeval(struct timeval* tv)
{
	trace_time(tv->tv_sec);
	trace_usec(tv->tv_usec);
}

/*
 * trace timeval interval pointer
 */

static void
trace_interval(struct timeval* tv)
{
	register unsigned long	s;
	register unsigned long	t;
	register int		n;

	static unsigned long	amt[] = { 1, 60, 60, 24, 7, 4, 12, 20 };
	static char		chr[] = "smhdwMYS";

	t = (unsigned long)tv->tv_sec;
	if (t == ~0L)
	{
		trace_printf("infinite");
		return;
	}
	if (t == 0)
	{
		trace_printf("0");
		if (tv->tv_usec == 0)
			return;
	}
	else
	{
		s = t;
		if (s < 60)
			trace_printf("%lu", s);
		else
		{
			for (n = 1; n < elementsof(amt) - 1; n++)
			{
				if ((t = s / amt[n]) < amt[n + 1])
					break;
				s = t;
			}
			if (chr[n - 1] != 's')
			{
				trace_printf("%d%c%02d%c", (s / amt[n]) % 100, chr[n], s % amt[n], chr[n - 1]);
				return;
			}
			trace_printf("%d%c%02d", (s / amt[n]) % 100, chr[n], s % amt[n]);
		}
	}
	if (tv->tv_usec)
		trace_usec(tv->tv_usec);
	trace_printf("s");
}

/*
 * trace flags in table
 */

static void
trace_flags(const Sym_t* tab, register unsigned long flags)
{
	register const Sym_t*	sym;
	register char*		sep;

	if (!flags)
	{
		trace_printf("0");
		return;
	}
	sep = "";
	for (sym = tab + 1; sym->name; sym++)
		if (flags & sym->value)
		{
			flags &= ~sym->value;
			trace_printf("%s%s", sep, sym->name);
			sep = "|";
		}
	if (flags)
		trace_printf("%s%s(0x%08lx)", sep, tab->name, flags);
}

/*
 * trace mask values in table
 */

static void
trace_mask(const Sym_t* tab, register unsigned long flags)
{
	register const Sym_t*	sym;
	register char*		sep;
	register unsigned long	m;

	if (!flags)
	{
		trace_printf("0");
		return;
	}
	sep = "";
	for (sym = tab + 1; sym->name; sym++)
		if (flags & (m = ((unsigned long)1) << (sym->value - 1)))
		{
			flags &= ~m;
			trace_printf("%s%s", sep, sym->name);
			sep = "|";
		}
	if (flags)
		trace_printf("%s%s(0x%08lx)", sep, tab->name, flags);
}

/*
 * trace value in table
 */

static void
trace_value(const Sym_t* tab, register unsigned long value)
{
	register const Sym_t*	sym;

	for (sym = tab + 1; sym->name; sym++)
		if (value == sym->value)
		{
			trace_printf("%s", sym->name);
			return;
		}
	if (tab->name)
		trace_printf("%s(0x%08lx)", tab->name, value);
	else
		trace_printf("0x%08lx", value);
}

/*
 * trace first part of buffer
 */

static void
trace_buffer(register unsigned char* s, size_t x, size_t n)
{
	register int		c;
	register unsigned char*	e;
	size_t			m;

	if (!s)
		trace_printf("nil");
	else if (n < 0)
		trace_printf("%p", s);
	else
	{
		if (trace.flags & UWIN_TRACE_VERBOSE)
		{
			m = n;
			trace.buffer.string = 1;
			trace_flush();
		}
		else
			m = 32;
		trace_printf("\"");
		if (m > n)
			m = n;
		e = s + m;
		while (s < e)
			if ((c = *s++) < 040)
			{
				switch (c)
				{
				case '\007':
					c = 'a';
					break;
				case '\b':
					c = 'b';
					break;
				case '\f':
					c = 'f';
					break;
				case '\n':
					c = 'n';
					break;
				case '\r':
					c = 'r';
					break;
				case '\t':
					c = 't';
					break;
				case '\013':
					c = 'v';
					break;
				case '\033':
					c = 'E';
					break;
				default:
					trace_printf("\\%03o", c);
					continue;
				}
				trace_printf("\\%c", c);
			}
			else if (c < 0177)
			{
				if (c == '"')
					trace_printf("\\\"");
				else
					trace_printf("%c", c);
			}
			else
				trace_printf("\\%03o", c);
		trace_printf("\"%s", m == n ? "" : "...");
		if (trace.buffer.string)
		{
			trace_flush();
			trace.buffer.string = 0;
		}
	}
	trace_printf(" %lu", x);
}

/*
 * trace struct iovec
 */

static void
trace_iov(register struct iovec* iov, int n)
{
	register struct iovec*	eov;

	if (!iov || n <= 0)
		trace_printf("nil");
	else
	{
		trace_printf("[ ");
			for (eov = iov + n; iov < eov; iov++)
				trace_buffer((unsigned char*)iov->iov_base, iov->iov_len, iov->iov_len);
		trace_printf(" ]");
	}
}

/*
 * trace struct sockaddr
 */

static void
trace_sockaddr(struct sockaddr_in* addr)
{
	register unsigned char*	u;
	register unsigned char*	v;

	trace_printf("[ ");
	trace_value(sym_sock_domain, addr->sin_family);
	switch (addr->sin_family)
	{
	case AF_INET:
		u = (unsigned char*)&addr->sin_addr;
		v = (unsigned char*)&addr->sin_port;
		trace_printf(" %u.%u.%u.%u %u ]", u[0], u[1], u[2], u[3], (v[0]<<8) + v[1]);
		break;
	case AF_UNIX:
		trace_printf(" \"%s\" ]", ((struct sockaddr_un*)addr)->sun_path);
		break;
	default:
		trace_printf(" %p ]", addr);
		break;
	}
}

/*
 * trace struct msghdr
 */

static void
trace_msg(register struct msghdr* msg)
{
	register int*	a;
	register int*	e;

	trace_printf("[ ");
	if (!msg->msg_name || msg->msg_namelen <= 0)
		trace_printf("nil");
	else
		trace_sockaddr((struct sockaddr_in*)msg->msg_name);
	trace_printf(" ");
	trace_iov(msg->msg_iov, msg->msg_iovlen);
	trace_printf(" ");
	if (!msg->msg_accrights || msg->msg_accrightslen <= 0)
		trace_printf("nil");
	else
	{
		trace_printf("[");
		a = (int*)msg->msg_accrights;
		for (e = (int*)((char*)a + msg->msg_accrightslen); a < e; a++)
			trace_printf(" %d", *a);
		trace_printf(" ]");
	}
	trace_printf(" ]");
}

#define DT_NAME(t)	dt_name[(t)&0xf]

static const char*	dt_name[] =
{
	"(0)",
	"DIR",
	"REG",
	"LNK",
	"(4)",
	"(5)",
	"(6)",
	"(7)",
	"(8)",
	"(9)",
	"(a)",
	"(b)",
	"(c)",
	"(d)",
	"(e)",
	"SPC",
};

/*
 * trace an individual arg
 */

static void
trace_arg(int type, register Systype_t* ap, long prev)
{
	register int	i;
	register long	n;
	char**		p;
	char*		s;

	if (type & ARG_POINTER)
	{
		if (type & ARG_BUFFER)
		{
			trace.buffer.pointer = ap->pointer;
			trace.buffer.type = type;
			return;
		}
		if (!ap->pointer)
		{
			trace_printf("%snil", trace.sep);
			return;
		}
	}
	trace_printf("%s", trace.sep);
	switch (ARGTYPE(type))
	{
	case ARG_ACCESS:
		trace_mode(ap->number, 7);
		break;
	case ARG_ARGV|ARG_POINTER:
		if (trace.flags & UWIN_TRACE_VERBOSE)
		{
			trace_printf("[");
			p = (char**)ap->pointer; 
			while (s = *p++)
				trace_printf(" %s", s);
			trace_printf(" ]");
		}
		else
			trace_printf("%p", ap->pointer);
		break;
	case ARG_BUFFER|ARG_INT:
	case ARG_BUFFER|ARG_SIZE:
		switch (ARGTYPE(trace.buffer.type))
		{
		case ARG_IOVEC|ARG_BUFFER|ARG_POINTER:
			trace_iov((struct iovec*)trace.buffer.pointer, ap->number);
			break;
		default:
			trace_buffer((unsigned char*)trace.buffer.pointer, ap->number, (trace.buffer.type & ARG_OUTPUT) ? trace.ret : ap->number);
			break;
		}
		break;
	case ARG_CHAR|ARG_POINTER:
		trace_printf("\"%s\"", ap->pointer);
		break;
	case ARG_CONFSTR:
		trace_value(sym_confstr, ap->number);
		break;
	case ARG_DIRENT|ARG_POINTER:
		trace_printf("[ %11lu %s \"%s\" ]", ((struct dirent*)ap->pointer)->d_fileno, DT_NAME(((struct dirent*)ap->pointer)->d_type), ((struct dirent*)ap->pointer)->d_name);
		break;
	case ARG_FCNTL:
		trace_value(sym_fcntl, ap->number);
		break;
	case ARG_FCNTL_ARG|ARG_POINTER:
		switch (prev)
		{
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			trace_printf(" [ ");
			trace_value(sym_lock, ((struct flock*)ap->pointer)->l_type);
			trace_printf(" ");
			trace_value(sym_seek, ((struct flock*)ap->pointer)->l_whence);
			trace_printf(" %ld %ld %ld ]", ((struct flock*)ap->pointer)->l_start, ((struct flock*)ap->pointer)->l_len, ((struct flock*)ap->pointer)->l_pid);
			break;
		case F_GETLK64:
		case F_SETLK64:
		case F_SETLKW64:
			trace_printf(" [ ");
			trace_value(sym_lock, ((struct flock64*)ap->pointer)->l_type);
			trace_printf(" ");
			trace_value(sym_seek, ((struct flock64*)ap->pointer)->l_whence);
			trace_printf(" %lld %lld %ld ]", ((struct flock64*)ap->pointer)->l_start, ((struct flock64*)ap->pointer)->l_len, ((struct flock64*)ap->pointer)->l_pid);
			break;
		default:
			trace_printf("%p", ap->pointer);
			break;
		}
		break;
	case ARG_FD_SET|ARG_POINTER:
		{
			n = ((fd_set*)ap->pointer)->fds_bits[0];
			trace_printf("[");
			for (i = 0; i < 32; i++)
				if (n & ((unsigned long)1 << i))
					trace_printf(" %d", i);
			trace_printf(" ]");
		}
		break;
	case ARG_INT|ARG_POINTER:
		trace_printf("%ld", *(int*)ap->pointer);
		break;
	case ARG_INT|ARG_POINTER|ARG_ARRAY:
		trace_printf("[ %ld %ld ]", ((int*)ap->pointer)[0], ((int*)ap->pointer)[1]);
		break;
	case ARG_INTERVAL|ARG_POINTER:
		trace_interval((struct timeval*)ap->pointer);
		break;
	case ARG_IOCTL:
		trace_value(sym_ioctl, ap->number);
		break;
	case ARG_ITIMERVAL|ARG_POINTER:
		trace_printf("[ ");
		trace_interval(&((struct itimerval*)ap->pointer)->it_interval);
		trace_printf(" ");
		trace_interval(&((struct itimerval*)ap->pointer)->it_value);
		trace_printf(" ]");
		break;
	case ARG_LOCALE:
		trace_value(sym_locale, ap->number);
		break;
	case ARG_MAP_FLAGS:
		trace_flags(sym_map_flags, ap->number);
		break;
	case ARG_MAP_PROTECT:
		trace_flags(sym_map_protect, ap->number);
		break;
	case ARG_MODE:
		trace_mode(ap->number, 0);
		break;
	case ARG_MOUNT:
		trace_flags(sym_mount, ap->number);
		break;
	case ARG_MSGHDR|ARG_POINTER:
		trace_msg((struct msghdr*)ap->pointer);
		break;
	case ARG_OPEN:
		if (trace.oflags = ap->number)
			trace_flags(sym_open, ap->number);
		else
			trace_printf("O_RDONLY");
		break;
	case ARG_PATHCONF:
		trace_value(sym_pathconf, ap->number);
		break;
	case ARG_RLIMIT_TYPE:
		trace_value(sym_rlimit, ap->number);
		break;
	case ARG_RLIMIT|ARG_POINTER:
		{
			struct rlimit*	rp = (struct rlimit*)ap->pointer;

			trace_printf("[ ");
			trace_printf(" cur=%08x", rp->rlim_cur);
			trace_printf(" max=%08x", rp->rlim_max);
			trace_printf(" ]");
		}
		break;
	case ARG_RLIMIT64|ARG_POINTER:
		{
			struct rlimit64*rp = (struct rlimit64*)ap->pointer;

			trace_printf("[ ");
			trace_printf(" cur=%08llx", rp->rlim_cur);
			trace_printf(" max=%08llx", rp->rlim_max);
			trace_printf(" ]");
		}
		break;
	case ARG_RUSAGE|ARG_POINTER:
		{
			struct rusage*	rp = (struct rusage*)ap->pointer;

			trace_printf("[ usr=");
			trace_timeval(&rp->ru_utime);
			trace_printf(" sys=");
			trace_timeval(&rp->ru_stime);
			trace_printf(" maxrss=%ld", rp->ru_maxrss);
			trace_printf(" ixrss=%ld", rp->ru_ixrss);
			trace_printf(" idrss=%ld", rp->ru_idrss);
			trace_printf(" isrss=%ld", rp->ru_isrss);
			trace_printf(" minflt=%ld", rp->ru_minflt);
			trace_printf(" majflt=%ld", rp->ru_majflt);
			trace_printf(" nswap=%ld", rp->ru_nswap);
			trace_printf(" inblock=%ld", rp->ru_inblock);
			trace_printf(" oublock=%ld", rp->ru_oublock);
			trace_printf(" msgsnd=%ld", rp->ru_msgsnd);
			trace_printf(" msgrcv=%ld", rp->ru_msgrcv);
			trace_printf(" nsignals=%ld", rp->ru_nsignals);
			trace_printf(" nvcsw=%ld", rp->ru_nvcsw);
			trace_printf(" nivcsw=%ld", rp->ru_nivcsw);
			trace_printf(" ]");
		}
		break;
	case ARG_SEEK:
		trace_value(sym_seek, ap->number);
		break;
	case ARG_SIG_HANDLER:
		trace_value(sym_sig_handler, ap->number);
		break;
	case ARG_SIG_PROC:
		trace_value(sym_sig_proc, ap->number);
		break;
	case ARG_SIGMASK:
		trace_mask(sym_signal, ap->number);
		break;
	case ARG_SIGNAL:
		trace_value(sym_signal, ap->number);
		break;
	case ARG_SIGSET|ARG_POINTER:
		trace_mask(sym_signal, *(unsigned long*)ap->pointer);
		break;
	case ARG_SOCK_DOMAIN:
		trace_value(sym_sock_domain, ap->number);
		break;
	case ARG_SOCK_MSG:
		trace_flags(sym_sock_msg, ap->number);
		break;
	case ARG_SOCK_PROTOCOL:
		trace_value(sym_sock_protocol, ap->number);
		break;
	case ARG_SOCK_TYPE:
		trace_value(sym_sock_type, ap->number);
		break;
	case ARG_SOCKADDR|ARG_POINTER:
		trace_sockaddr((struct sockaddr_in*)ap->pointer);
		break;
	case ARG_SPAWNDATA|ARG_POINTER:
		{
			struct spawndata*	sp = (struct spawndata*)ap->pointer;

			trace_printf("[ ");
			trace_printf(" tok=%08x", sp->tok);
			trace_printf(" flags=");
			trace_flags(sym_spawndata_flags, ap->number);
			trace_printf(" start=%p", sp->start);
			trace_printf(" grp=%d", sp->grp);
			trace_printf(" trace=%d", sp->trace);
			trace_printf(" ]");
		}
		break;
	case ARG_STAT|ARG_POINTER:
		{
			struct stat*	sp = (struct stat*)ap->pointer;

			trace_printf("[ ");
			trace_mode(sp->st_mode, 0);
			if (S_ISBLK(sp->st_mode) || S_ISCHR(sp->st_mode))
				trace_printf(" rdev=%03u,%03u", major(sp->st_rdev), minor(sp->st_rdev));
			trace_printf(" dev=%03u,%03u ino=%u nlink=%u uid=%u gid=%u size=%u", major(sp->st_dev), minor(sp->st_dev), sp->st_ino, sp->st_nlink, sp->st_uid, sp->st_gid, sp->st_size);
			trace_printf(" atime=");
			trace_time(sp->st_atime);
			trace_printf(" mtime=");
			trace_time(sp->st_mtime);
			trace_printf(" ctime=");
			trace_time(sp->st_ctime);
			trace_printf(" ]");
		}
		break;
#ifdef ARG_STAT64
	case ARG_STAT64|ARG_POINTER:
		{
			struct stat64*	sp = (struct stat64*)ap->pointer;

			trace_printf("[ ");
			trace_mode(sp->st_mode, 0);
			if (S_ISBLK(sp->st_mode) || S_ISCHR(sp->st_mode))
				trace_printf(" rdev=%03u,%03u", major(sp->st_rdev), minor(sp->st_rdev));
			trace_printf(" dev=%03u,%03u ino=%u nlink=%u uid=%u gid=%u size=%llu", major(sp->st_dev), minor(sp->st_dev), sp->st_ino, sp->st_nlink, sp->st_uid, sp->st_gid, sp->st_size);
			trace_printf(" atime=");
			trace_time(sp->st_atime);
			trace_printf(" mtime=");
			trace_time(sp->st_mtime);
			trace_printf(" ctime=");
			trace_time(sp->st_ctime);
			trace_printf(" ]");
		}
		break;
#endif
	case ARG_STATVFS|ARG_POINTER:
		{
			struct statvfs*	sp = (struct statvfs*)ap->pointer;

			trace_printf("[ basetype=%s fstr=%s bsize=%lu frsize=%lu blocks=%lu bfree=%lu bavail=%lu files=%lu ffree=%lu favail=%lu fsid=%lu flag=0x%x namemax=%lu ]", sp->f_basetype, sp->f_fstr, sp->f_bsize, sp->f_frsize, sp->f_blocks, sp->f_bfree, sp->f_bavail, sp->f_files, sp->f_ffree, sp->f_favail, sp->f_fsid, sp->f_flag, sp->f_namemax);
		}
		break;
	case ARG_SYSCONF:
		trace_value(sym_sysconf, ap->number);
		break;
	case ARG_TIME:
		trace_time((time_t)ap->number);
		break;
	case ARG_TIME|ARG_POINTER:
		trace_time(*(time_t*)ap->pointer);
		break;
	case ARG_TIMESPEC|ARG_POINTER:
		trace_timespec((struct timespec*)ap->pointer);
		break;
	case ARG_TIMESPEC|ARG_POINTER|ARG_ARRAY:
		trace_printf("[ ");
		trace_timespec((struct timespec*)ap->pointer);
		trace_printf(" ");
		trace_timespec((struct timespec*)ap->pointer + 1);
		trace_printf(" ]");
		break;
	case ARG_TIMEVAL|ARG_POINTER:
		trace_timeval((struct timeval*)ap->pointer);
		break;
	case ARG_TIMEVAL|ARG_POINTER|ARG_ARRAY:
		trace_printf("[ ");
		trace_timeval((struct timeval*)ap->pointer);
		trace_printf(" ");
		trace_timeval((struct timeval*)ap->pointer + 1);
		trace_printf(" ]");
		break;
	case ARG_TIMEZONE|ARG_POINTER:
		trace_printf("[ %dmin %d ]", ((struct timezone*)ap->pointer)->tz_minuteswest, ((struct timezone*)ap->pointer)->tz_dsttime);
		break;
	case ARG_UTIMBUF|ARG_POINTER:
		trace_time(((struct utimbuf*)ap->pointer)->actime);
		trace_printf(" ");
		trace_time(((struct utimbuf*)ap->pointer)->modtime);
		break;
	default:
		if (type & ARG_POINTER)
			trace_printf("%p", ap->pointer);
		else
			trace_printf("%ld", ap->number);
		break;
	}
}

/*
 * dummy for ARG_AGAIN
 */

static int
trace_again(void)
{
	return 0;
}

static Syscall_t	sysagain = { 0, 0, (Syscall_f)trace_again, (Syscall_f)trace_again, { ARG_AGAIN|ARG_INT, ARG_OUTPUT|ARG_POINTER } };

/*
 * trace what we have of the syscall before the actual call
 */

static void
trace_call(const Syscall_t* sys, ...)
{
	register const int*	at;
	va_list			ap;
	register int		type;
	register long		prev;
	Systype_t		arg;
	unsigned int_8		watch;

	if (trace.flags & UWIN_TRACE_CALL)
	{
		va_start(ap, sys);
		if (trace.flags & UWIN_TRACE_INHERIT)
			trace_printf("%03u: ", getpid());
		if (trace.flags & UWIN_TRACE_TIME)
			trace_printf("%012llu ", cycles() - trace.start);
		trace_printf("%s (", sys->name + sys->prefix);
		trace.sep = " ";
		prev = 0;
		at = sys->type + 1;
		while ((type = *at++) && !(type & ARG_OUTPUT))
		{
			if (type & ARG_POINTER)
				arg.pointer = va_arg(ap, void*);
			else
				arg.number = va_arg(ap, long);
			trace_arg(type, &arg, prev);
			prev = arg.number;
		}
		if (!type)
		{
			trace_printf(" ) = ");
			trace.sep = "";
			if (sys->type[0] & ARG_AGAIN)
				trace_printf("...\n");
		}
		va_end(ap);
		trace_flush();
	}
	if (trace.flags & (UWIN_TRACE_COUNT|UWIN_TRACE_TIME))
	{
		trace.count[sys - sys_call].calls++;
		watch = cycles();
		trace.cycles += watch - trace.watch;
		trace.watch = watch;
	}
}

/*
 * trace what's left of the syscall after the actual call
 */

static void
trace_return(const Syscall_t* sys, long r, ...)
{
	register const int*	at;
	va_list			ap;
	register int		type;
	register long		prev;
	unsigned long		u;
	Systype_t		arg;
	Syscount_t*		cp;
	unsigned int_8		watch;

	if (trace.flags & (UWIN_TRACE_COUNT|UWIN_TRACE_TIME))
	{
		watch = cycles();
		cp = &trace.count[sys - sys_call];

		/*
		 * can this ever be false? man, that's fast.
		 */

		if (watch > trace.watch)
		{
			cp->cycles += watch - trace.watch;
			trace.watch = watch;
		}
		if (r == -1)
			cp->errors++;
		else if (r > 0 && (sys->type[0] & ARG_IO))
			cp->io += r;
	}
	if (trace.flags & UWIN_TRACE_CALL)
	{
		trace.ret = r;
		if (sys->type[0] & ARG_AGAIN)
		{
			sysagain.name = sys->name;
			sysagain.prefix = sys->prefix;
			TRACE_CALL((&sysagain, NULL));
			trace.sep = " ";
		}
		prev = 0;
		if (*trace.sep)
		{
			va_start(ap, r);
			at = sys->type + 1;
			while ((type = *at++) && !(type & (ARG_AGAIN|ARG_OUTPUT)))
			{
				if (type & ARG_POINTER)
					arg.pointer = va_arg(ap, void*);
				else
					arg.number = va_arg(ap, long);
				prev = arg.number;
			}
			if (type)
				do
				{
					if (type & ARG_POINTER)
						arg.pointer = va_arg(ap, void*);
					else
						arg.number = va_arg(ap, long);
					trace_arg((r == -1 && !(type & ARG_AGAIN)) ? 0 : type, &arg, prev);
					prev = arg.number;
				} while (type = *at++);
			va_end(ap);
			trace.sep = " ) = ";
		}
		arg.number = r;
		trace_arg(sys->type[0], &arg, prev);
		if (r == -1)
		{
			if (errno > 0 && errno < sys_nerr)
				trace_printf(" [%s", sys_errlist[errno]);
			else
				trace_printf(" [Error %d", errno);
			if (u = GetLastError())
			{
				trace_printf(" (");
				trace_value(sym_winerror, u);
				trace_printf(")");
			}
			trace_printf("]\n");
		}
		else
			trace_printf("\n");
		trace_flush();
	}
}
