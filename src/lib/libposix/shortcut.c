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
 * standalone shortcut rd/mk
 * this bypasses the crazy (and leaky?) shell32 api
 */

#include "uwindef.h"

#define	SC_IDLIST		0x00000001
#define	SC_PATH			0x00000002
#define	SC_DESCRIPTION		0x00000004
#define	SC_RELATIVE_PATH	0x00000008
#define	SC_WORKING_DIR		0x00000010
#define	SC_ARGUMENTS		0x00000020
#define	SC_ICON_PATH		0x00000040
#define	SC_UNICODE		0x00000080
#define	SC_ELEVATE		0x00002000

typedef struct SC_guid_s
{
	unsigned char	id[16];
} SC_guid_t;

typedef struct SC_puid_s
{
	unsigned char	id[20];
} SC_puid_t;

typedef struct SC_item_s
{
	unsigned short	size;
	unsigned char	data[1];
} SC_item_t;

typedef struct SC_time_s
{
	uint32_t	lo;
	uint32_t	hi;
} SC_time_t;

typedef struct SC_header_s
{
	unsigned char	length;
	unsigned char	zero[3];
	SC_guid_t	id;
	unsigned int	flags;
	uint32_t	dwFileAttributes;
	SC_time_t	ftCreationTime;
	SC_time_t	ftLastAccessTime;
	SC_time_t	ftLastWriteTime;
	uint32_t	nFileSizeLow;
	unsigned int	icon_number;
	unsigned int	window;
	unsigned int	hotkey;
	unsigned int	pad[2];
} SC_header_t;

typedef struct SC_file_s
{
	unsigned short	length;
	unsigned short	pad;
	uint32_t	hsize;
	uint32_t	type;
	uint32_t	offset[4];
} SC_file_t;

typedef struct SC_s
{
	const char*		path_start;
	const char*		path_finish;
	const unsigned char*	description;
	const unsigned char*	working_dir;
	const unsigned char*	relative_path;
	const SC_item_t*	idlist;
	unsigned int		uwin;
	unsigned short		description_size;
	unsigned short		working_dir_size;
	unsigned short		relative_path_size;
	unsigned char		flags;
	unsigned char		path_type;
} SC_t;

static const SC_guid_t	sc_guid =
{
	0x01,0x14,0x02,0x00,
	0x00,0x00,0x00,0x00,
	0xc0,0x00,0x00,0x00,
	0x00,0x00,0x00,0x46
};

static const SC_puid_t	sc_puid =
{
	0x14,0x00,0x1f,0x50,
	0xe0,0x4f,0xd0,0x20,
	0xea,0x3a,0x69,0x10,
	0xa2,0xd8,0x08,0x00,
	0x2b,0x30,0x30,0x9d
};

static const char	sc_suffix[] = ".lnk";

#define LEN(p)		(*(unsigned short*)(p))

static int
MINLEN(const unsigned char* p, int m)
{
	int	n = LEN(p);

	return n ? n : m;
}

/*
 * parse shortcut info from data using text for string data
 */

static ssize_t
getshortcut(const void* data, size_t datasize, UWIN_shortcut_t* shortcut, char* text, size_t textsize)
{
	register int		n;
	register char*		t = text;
	register char*		e = text + textsize;
	register unsigned char*	p = (char*)data;
	register unsigned char*	q = p + datasize;
	const SC_header_t*	h = (const SC_header_t*)data;
	const SC_file_t*	f;
	char*			b;
	char*			o;
	char*			s;
	char*			v;
	unsigned char*		d;
	unsigned char*		r;
	unsigned short*		y;
	unsigned short*		z;
	int			flags;
	int			w;
	int			i;
	int			j;
	char			tmp[PATH_MAX];
	char			var[NAME_MAX];
	char*			x = &tmp[sizeof(tmp)];

	/* check header size */

	if (h->length < sizeof(SC_header_t) || memcmp(&h->id, &sc_guid, sizeof(sc_guid)) || (q - p) < h->length)
		goto bad;

	/* commit to shortcut */

	p += h->length;
	memset(shortcut, 0, sizeof(*shortcut));
	flags = h->flags;
	w = (flags & SC_UNICODE) ? 2 : 1;

	/* peel off sections in layout order */

	if (flags & SC_IDLIST)
	{
		n = MINLEN(p, 2);
		if ((q - p) < n)
			goto bad;
		d = p + 2;
		p += n;
		b = tmp;
		while (n = LEN(d))
		{
			r = d + 2;
			d += n;
			switch (*r)
			{
			case '/':
				while (b < x && (*b = *++r))
					b++;
				if (b > tmp && *(b - 1) == '\\')
					b--;
				break;
			case '1':
			case '2':
				if (b > tmp && b < x)
					*b++ = '\\';
				y = z = (unsigned short*)(d - 4);
				while (*--y);
				if (r[10] & 0x01)
				{
					z = y;
					while (*--y);
				}
				n = (int)(z - ++y);
				if ((n = WideCharToMultiByte(CP_ACP, 0, y, n, b, (int)(x - b), NULL, NULL)) <= 0)
					n = -1;
				b += n;
				break;
			}
		}
		if (b > tmp)
		{
			if (b >= x)
				goto bad;
			*b = 0;
			i = uwin_pathmap(tmp, t, (int)(e - t), UWIN_W2U|UWIN_PHY);
			if ((e - t) < (i + 1))
				goto more;
			shortcut->sc_target = t;
			t += i;
			*t++ = 0;
			i = (int)(b - tmp) + 1;
			if ((e - t) < i)
				goto more;
			shortcut->sc_native = t;
			memcpy(t, tmp, i);
			t += i;
		}
		if (!shortcut->sc_target)
			flags &= ~SC_IDLIST;

		/* ignore next section */

		n = MINLEN(p, 2);
		if ((q - p) < n)
			goto bad;
		p += n;
	}
	if (flags & SC_PATH)
	{
		n = *((uint32_t*)p);
		if ((q - p) < n)
			goto bad;
		if (!(flags & SC_IDLIST))
		{
			if ((e - t) <= n)
				goto more;
			f = (const SC_file_t*)p;
			if (f->type >= 1 && f->type <= 2)
			{
				d = p + (f->type == 2 ? (f->offset[2] + f->offset[6]) : f->offset[3]);
				r = p + f->offset[3];
				i = (int)strlen(d);
				j = (int)strlen(r);
				b = tmp;
				if ((x - b) < (i + j + 2))
					goto bad;
				if (i)
				{
					memcpy(b, d, i);
					b += i;
					if (f->type == 2)
						*b++ = '\\';
				}
				if (j)
				{
					memcpy(b, r, j);
					b += j;
				}
				if (b > tmp)
				{
					*b = 0;
					i = uwin_pathmap(tmp, t, (int)(e - t), UWIN_W2U|UWIN_PHY);
					if ((e - t) < (i + 1))
						goto more;
					shortcut->sc_target = t;
					t += i;
					*t++ = 0;
				}
			}
		}
		if (!shortcut->sc_target)
			flags &= ~SC_PATH;
		p += n;
	}
	if (flags & SC_DESCRIPTION)
	{
		i = LEN(p);
		n = i * w + 2;
		if ((q - p) < n)
			goto bad;
		b = t;
		if (flags & SC_UNICODE)
		{
			j = WideCharToMultiByte(CP_ACP, 0, (const unsigned short*)(p + 2), i, t, (int)(e - t), NULL, NULL);
			if (j == 0)
				goto more;
			t += j;
		}
		else if ((e - t) < i)
			goto more;
		else
		{
			memcpy(t, p + 2, i);
			t += i;
		}
		if ((e - t) < 1)
			goto more;
		*t++ = 0;
		shortcut->sc_description = b;
		p += n;
	}
	if (flags & SC_RELATIVE_PATH)
	{
		i = LEN(p);
		n = i * w + 2;
		if ((q - p) < n)
			goto bad;
		b = tmp;
		if (flags & SC_UNICODE)
		{
			j = WideCharToMultiByte(CP_ACP, 0, (const unsigned short*)(p + 2), i, b, (int)(x - b), NULL, NULL);
			if (j == 0)
				goto more;
			b += j;
		}
		else if ((x - b) < i)
			goto bad;
		else
		{
			memcpy(b, p + 2, i);
			b += i;
		}
		if ((x - b) < 1)
			goto bad;
		*b = 0;
		i = uwin_pathmap(tmp, t, (int)(e - t), UWIN_W2U|UWIN_PHY);
		if ((e - t) < (i + 1))
			goto more;
		if (i > 0)
		{
			t[i] = 0;
			if (t[0] != '.' || t[1] == 0 || t[1] != '.' || t[2] != 0 && t[2] != '/')
			{
				shortcut->sc_target = t;
				t += i + 1;
			}
		}
		p += n;
	}
	if (!shortcut->sc_target)
	{
		if (!shortcut->sc_description)
			goto bad;
		n = 1;
	}
	else if (!shortcut->sc_description)
		n = 0;
	else
	{
		i = (int)strlen(shortcut->sc_target);
		j = (int)strlen(shortcut->sc_description);
		n = i > j && memicmp(shortcut->sc_target + i - j, shortcut->sc_description, j) == 0;
	}
	if (n)
	{
		if (*(s = shortcut->sc_description) == '@')
		{
			b = t;
			while ((n = *++s) && (n != ',' || *(s + 1) != '-'))
			{
				if (n == '%' && *++s != n)
				{
					v = var;
					o = s;
					while ((i = *s++) && i != '%')
					{
						if (v >= &var[sizeof(var)-1])
							goto bad;
						*v++ = i;
					}
					if (i)
					{
						s--;
						*v = 0;
						if ((i = GetEnvironmentVariable(var, tmp, sizeof(tmp))) > 0)
						{
							if ((e - t) < i)
								goto more;
							memcpy(t, tmp, i);
							t += i;
						}
						continue;
					}
					s = o - 1;
				}
				if ((e - t) <= 1)
					goto more;
				*t++ = n;
			}
			*t++ = 0;
			shortcut->sc_target = b;
		}
		else if ((e - t) < ++i)
			goto more;
		else
		{
			memcpy(t, shortcut->sc_description, i);
			shortcut->sc_target = t;
			t += i;
		}
	}
	if (flags & SC_WORKING_DIR)
	{
		i = LEN(p);
		n = i * w + 2;
		if ((q - p) < n)
			goto bad;
		b = tmp;
		if (flags & SC_UNICODE)
		{
			j = WideCharToMultiByte(CP_ACP, 0, (const unsigned short*)(p + 2), i, b, (int)(x - b), NULL, NULL);
			if (j == 0)
				goto more;
			b += j;
		}
		else if ((x - b) < i)
			goto bad;
		else
		{
			memcpy(b, p + 2, i);
			b += i;
		}
		if ((x - b) < 1)
			goto bad;
		*b = 0;
		i = uwin_pathmap(tmp, t, (int)(e - t), UWIN_W2U|UWIN_PHY);
		if ((e - t) < (i + 1))
			goto more;
		shortcut->sc_dir = t;
		t += i;
		*t++ = 0;
		p += n;
	}
	if (flags & SC_ARGUMENTS)
	{
		i = LEN(p);
		n = i * w + 2;
		if ((q - p) < n)
			goto bad;
		b = t;
		if (flags & SC_UNICODE)
		{
			j = WideCharToMultiByte(CP_ACP, 0, (const unsigned short*)(p + 2), i, t, (int)(e - t), NULL, NULL);
			if (j == 0)
				goto more;
			t += j;
		}
		else if ((e - t) < (i + 1))
			goto more;
		else
		{
			memcpy(t, p + 2, i);
			t += i;
			*t++ = 0;
		}
		if ((e - t) < 1)
			goto more;
		*t++ = 0;
		shortcut->sc_arguments = b;
		p += n;
	}
	if (flags & SC_ICON_PATH)
	{
		i = LEN(p);
		n = i * w + 2;
		if ((q - p) < n)
			goto bad;
		b = tmp;
		if (flags & SC_UNICODE)
		{
			j = WideCharToMultiByte(CP_ACP, 0, (const unsigned short*)(p + 2), i, b, (int)(x - b), NULL, NULL);
			if (j == 0)
				goto more;
			b += j;
		}
		else if ((x - b) < i)
			goto bad;
		else
		{
			memcpy(b, p + 2, i);
			b += i;
		}
		if ((x - b) < 1)
			goto bad;
		*b = 0;
		i = uwin_pathmap(tmp, t, (int)(e - t), UWIN_W2U|UWIN_PHY);
		if ((e - t) < (i + 1))
			goto more;
		shortcut->sc_icon = t;
		t += i;
		*t++ = 0;
		p += n;
	}
	else
		shortcut->sc_index = h->icon_number;
	switch (h->window)
	{
	case 2:
	case 7:
		shortcut->sc_window = "minimized";
		break;
	case 3:
		shortcut->sc_window = "maximized";
		break;
	default:
		shortcut->sc_window = "normal";
		break;
	}
	shortcut->sc_elevate = !!(flags & SC_ELEVATE);
	return (ssize_t)(t - text);
 more:
 	return (ssize_t)textsize + PATH_MAX;
 bad:
	SetLastError(ERROR_INVALID_DATA);
	return -1;
}

/*
 * read fh shortcut information using buf for string data
 */

ssize_t
read_shortcut(HANDLE fh, UWIN_shortcut_t* shortcut, char* buf, size_t size)
{
	DWORD	n;
	char	dat[8*PATH_MAX];

	n = GetFileSize(fh, 0);
	if (n > sizeof(dat))
	{
		if (shortcut)
			memset(shortcut, 0, sizeof(*shortcut));
		return n;
	}
	if (!ReadFile(fh, dat, n, &n, 0))
		goto bad;
	if (!shortcut)
	{
		if (buf && (DWORD)size >= n)
			memcpy(buf, dat, n);
		return n;
	}
	return getshortcut(dat, n, shortcut, buf, size);
 bad:
	errno = unix_err(GetLastError());
	return -1;
}

/*
 * read path shortcut information using buf for string data
 */

ssize_t
uwin_rdshortcut(const char* path, UWIN_shortcut_t* shortcut, char* buf, size_t size)
{
	HANDLE	fh;
	ssize_t	n;

	if (!path || shortcut && IsBadStringPtr((char*)shortcut, sizeof(*shortcut)) || IsBadStringPtr(path, PATH_MAX))
	{
		errno = EFAULT;
		return -1;
	}
	uwin_pathmap(path, buf, (int)size, UWIN_U2W|UWIN_PHY);
	if (!(fh = createfile(buf, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)))
	{
		errno = unix_err(GetLastError());
		return -1;
	}
	n = read_shortcut(fh, shortcut, buf, size);
	CloseHandle(fh);
	return n;
}

/*
 * set shortcut data in buf
 * return < 0 : error
 * return > size : buf too small
 */

static ssize_t
setshortcut(const UWIN_shortcut_t* shortcut, char* buf, size_t size)
{
	char*		p = buf;
	char*		e = p + size;
	SC_header_t*	h = (SC_header_t*)p;
	const char*	s;
	const char*	b;
	char*		a;
	char*		d;
	char*		f;
	char*		w;
	char*		z;
	ssize_t		m;
	int		n;
	int		i;
	int		j;
	char		tmp[PATH_MAX];
	char		dos[PATH_MAX];

	if (!shortcut || !shortcut->sc_target)
	{
		SetLastError(ERROR_INVALID_DATA);
		return -1;
	}
	m = sizeof(SC_header_t) + 25 + (int)strlen(shortcut->sc_target) * 4 + 7 * 4 + 2;
	if ((ssize_t)size < m)
		return m;
	memset(p, 0, e - p);

	/* header */

	h->length = sizeof(SC_header_t);
	h->id = sc_guid;
	h->flags = SC_IDLIST|SC_UNICODE;
	if (shortcut->sc_elevate)
		h->flags |= SC_ELEVATE;
	h->window = (shortcut->sc_window && *shortcut->sc_window == 'm') ? (*(shortcut->sc_window + 1) == 'i' ? 2 : 3) : 1;
	p = (char*)(h + 1);

	/* SC_IDLIST */

	a = p;
	p += 2;
	memcpy(p, &sc_puid, sizeof(sc_puid));
	p += sizeof(sc_puid);
	j = uwin_pathmap(shortcut->sc_target, tmp, sizeof(tmp), UWIN_U2W) + 1;
	s = tmp;
	uwin_pathmap(shortcut->sc_target, dos, sizeof(dos), UWIN_U2S);
	d = dos;
	LEN(p) = 25;
	p += 2;
	*p++ = '/';
	if (d[0] != 0 && d[1] == ':' && d[2] == '\\')
		d += 3;
	if (s[0] != 0 && s[1] == ':' && s[2] == '\\')
	{
		*p++ = s[0];
		s += 3;
	}
	else
		*p++ = 'C';
	*p++ = ':';
	*p++ = '\\';
	p += 19;
	while (*(b = s))
	{
		z = p;
		while (*s && *s != '/' && *s != '\\')
			s++;
		if (*s)
		{
			p[2] = '1';
			p[12] = 0x10;
		}
		else
			p[2] = '2';
		p += 14;
		for (f = d; *d && *d != '/' && *d != '\\'; d++);
		n = (int)(s - b);
		if (n == (d - f))
			memcpy(p, b, n);
		else
		{
			n = (int)(d - f);
			memcpy(p, f, n);
		}
		i = n + 1 + !(n & 1);
		p += i;
		w = p;
		*(p += 2) = 0x08;
		*(p += 2) = 0x04;
		*(p += 2) = 0xef;
		*++p = 0xbe;
		*(p += 8) = 0x2a;
		p += 26;
		n = (int)(s - b);
		if ((n = MultiByteToWideChar(CP_ACP, 0, b, n, (unsigned short*)p, 8 * n)) < 0)
			return -1;
		p += 2 * (n + 1);
		LEN(p) = (unsigned short)(i + 8);
		LEN(w) = (unsigned short)(p - w);
		p += 2;
		LEN(z) = (unsigned short)(p - z);
		if (*d)
			d++;
		if (*s)
			s++;
	}
	p += 2;
	LEN(a) = (unsigned short)(p - a);
	LEN(p) = 2;
	p += 2;

	/* SC_DESCRIPTION */

	j = uwin_pathmap(shortcut->sc_target, tmp, sizeof(tmp), UWIN_W2U) + 1;
	if (j > 5 && !_stricmp(tmp + j - 5, ".exe"))
	{
		tmp[j - 5] = 0;
		j -= 4;
	}
	if ((a = shortcut->sc_description) || (a = tmp))
	{
		h->flags |= SC_DESCRIPTION;
		if (a == tmp)
			i = j;
		else
			i = (int)strlen(a) + 1;
		if ((n = MultiByteToWideChar(CP_ACP, 0, a, i, (unsigned short*)(p + 2), (int)(e - p))) <= 0)
			return (ssize_t)((e - p) + i * 2);
		LEN(p) = n;
		if ((p += (n + 1) * 2) > e)
			return (ssize_t)(p - buf);
	}

	/* SC_RELATIVE_PATH */

	h->flags |= SC_RELATIVE_PATH;
	if ((n = MultiByteToWideChar(CP_ACP, 0, tmp, j, (unsigned short*)(p + 2), (int)(e - p))) <= 0)
		return (ssize_t)((e - p) + j * 2);
	LEN(p) = n;
	if ((p += (n + 1) * 2) > e)
		return (ssize_t)(p - buf);

	/* SC_WORKING_DIR */

	if (shortcut->sc_dir)
	{
		h->flags |= SC_WORKING_DIR;
		i = uwin_pathmap(shortcut->sc_dir, tmp, sizeof(tmp), UWIN_U2W) + 1;
		if ((n = MultiByteToWideChar(CP_ACP, 0, tmp, i, (unsigned short*)(p + 2), (int)(e - p))) <= 0)
			return (ssize_t)((e - p) + i * 2);
		LEN(p) = n;
		if ((p += (n + 1) * 2) > e)
			return (ssize_t)(p - buf);
	}

	/* SC_ARGUMENTS */

	if (shortcut->sc_arguments)
	{
		h->flags |= SC_ARGUMENTS;
		i = (int)strlen(shortcut->sc_arguments) + 1;
		if ((n = MultiByteToWideChar(CP_ACP, 0, shortcut->sc_arguments, i, (unsigned short*)(p + 2), (int)(e - p))) <= 0)
			return (ssize_t)((e - p) + i * 2);
		LEN(p) = n;
		if ((p += (n + 1) * 2) > e)
			return (ssize_t)(p - buf);
	}

	/* SC_ICON_PATH */

	if (shortcut->sc_icon)
	{
		h->flags |= SC_ICON_PATH;
		i = uwin_pathmap(shortcut->sc_icon, tmp, PATH_MAX, UWIN_U2W) + 1;
		if ((n = MultiByteToWideChar(CP_ACP, 0, tmp, i, (unsigned short*)(p + 2), (int)(e - p))) <= 0)
			return (ssize_t)((e - p) + i * 2);
		LEN(p) = n;
		if ((p += (n + 1) * 2) > e)
			return (ssize_t)(p - buf);
	}
	else
		h->icon_number = shortcut->sc_index;

	/* trailer */

	p += 2;

	/* done */

	return (ssize_t)(p - buf);
}

/*
 * create shortcut on path
 */

int
uwin_mkshortcut(const char* path, const UWIN_shortcut_t* shortcut)
{
	HANDLE		fh;
	DWORD		a;
	ssize_t		n;
	ssize_t		m;
	char		buf[4*PATH_MAX];
	char		tmp[PATH_MAX];

	if (!path || !shortcut || IsBadStringPtr(shortcut->sc_target, PATH_MAX) || IsBadStringPtr(path, PATH_MAX))
	{
		errno = EFAULT;
		return -1;
	}
	if ((n = setshortcut(shortcut, buf, sizeof(buf))) < 0)
		goto bad;
	if (n > sizeof(buf))
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		goto bad;
	}
	if ((m = uwin_pathmap(path, tmp, sizeof(tmp) - sizeof(sc_suffix), UWIN_U2W)) < 0)
		goto bad;
	memcpy(tmp + m, sc_suffix, sizeof(sc_suffix));
	if (!(fh = createfile(tmp, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0)))
		goto bad;
	if (!WriteFile(fh, buf, (DWORD)n, &a, 0))
		a = -1;
	CloseHandle(fh);
	if (a >= 0)
	{
		lchmod(path, S_IRUSR|S_IRGRP|S_IROTH|((S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)&~P_CP->umask));

		/* FILE_ATTRIBUTE_READONLY is the pre-10-10-10 symlink marker */

		if ((a = GetFileAttributes(tmp)) != (DWORD)-1)
			SetFileAttributes(tmp, a|FILE_ATTRIBUTE_READONLY);
		return 0;
	}
 bad:
	errno = unix_err(GetLastError());
	return -1;
}
