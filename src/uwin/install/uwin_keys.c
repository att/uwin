/*
 * get/set uwin registry keys
 */

#include <uwin_keys.h>

#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY	(0x0100)
#endif
#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY	(0x0200)
#endif

#define getapi_addr(m,f)	GetProcAddress(GetModuleHandle(m),f)

#if UWIN_KEYS_DIAGNOSTICS

#include <stdio.h>

static const char*	keypath;

static void keyerror(const char* op, const char* key, int code)
{
	int	n;
	char	buf[128];

	if (!(n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, 0, code, 0, buf, sizeof(buf), 0)))
		sprintf(buf, "Unknown error %d", code);
	else if (n > 3)
		buf[n - 3] = 0;
	fprintf(stderr, "uwin_keys: %s%s%s: %s failed [%s]\n", keypath == key ? "" : keypath, keypath == key ? "" : "/", key, op, buf);
	fflush(stderr);
}

static int keyopen(HKEY pfx, const char* key, DWORD internal, DWORD flags, HKEY* kh)
{
	int	c;

	keypath = key;
	if ((c = RegOpenKeyEx(pfx, key, internal, flags|KEY_WOW64_64KEY, kh)) &&
	    (c = RegOpenKeyEx(pfx, key, internal, flags|KEY_WOW64_32KEY, kh)))
		keyerror("open", key, c);
	return c;
}

#undef	RegOpenKeyEx
#define RegOpenKeyEx	keyopen

static int keycreate(HKEY pfx, const char* key, int ignore_1, void* ignore_2, DWORD flags, DWORD access, SECURITY_ATTRIBUTES* sa, PHKEY kh, LPDWORD disposition)
{
	int	c;

	if (c = RegCreateKeyEx(pfx, key, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, sa, kh, disposition))
		keyerror("create", key, c);
	return c;
}

#undef	RegCreateKeyEx
#define RegCreateKeyEx keycreate

static int keyget(HKEY kh, const char* key, void* internal, LPDWORD type, char* buf, LPDWORD size)
{
	int	c;

	if (c = RegQueryValueEx(kh, key, internal, type, buf, size))
		keyerror("get", key, c);
	return c;
}

#undef	RegQueryValueEx
#define RegQueryValueEx	keyget

static int keyset(HKEY kh, const char* key, DWORD internal, DWORD type, const char* buf, DWORD size)
{
	int	c;

	if (c = RegSetValueEx(kh, key, internal, type, buf, size))
		keyerror("set", key, c);
	return c;
}

#undef	RegSetValueEx
#define RegSetValueEx	keyset

static int keyunset(HKEY kh, const char* key)
{
	int	c;

	if (c = RegDeleteValue(kh, key))
		keyerror("unset", key, c);
	return c;
}

#undef	RegDeleteValue
#define RegDeleteValue	keyunset

typedef LONG (WINAPI* RDKEA_t)(HKEY, LPCTSTR, REGSAM, DWORD);

static int keydelete(HKEY kh, const char* key)
{
	int		c;

	static int	initialized;
	static RDKEA_t	RDKEA;

	if (!initialized)
		initialized = (RDKEA = (RDKEA_t)getapi_addr("kernel32.dll", "RegDeleteKeyExA")) ? 1 : -1;
	c = RegDeleteKey(kh, key);
	if (initialized > 0)
	{
		(*RDKEA)(kh, key, KEY_WOW64_32KEY, 0);
		c = (*RDKEA)(kh, key, KEY_WOW64_64KEY, 0);
	}
	if (c)
		keyerror("delete", key, c);
	return c;
}

#undef	RegDeleteKey
#define RegDeleteKey	keydelete

#else

#define keyerror(a,b,c)

#endif

extern int	uwin_getkeys(const char*, char*, size_t, char*, size_t);

#define SID_BUF_MAX	64

#ifndef SECURITY_NT_SID_AUTHORITY
#define SECURITY_NT_SID_AUTHORITY	{0,0,0,0,0,5}
#endif

static SID	admins_sid_hdr =
{
	SID_REVISION,
	2,
	SECURITY_NT_SID_AUTHORITY,
	SECURITY_BUILTIN_DOMAIN_RID
};

static SID	worldsid =
{
	SID_REVISION,
	1,
	SECURITY_WORLD_SID_AUTHORITY,
	SECURITY_WORLD_RID
};

static SECURITY_ATTRIBUTES* security_attributes(void)
{
	static int			initialized;
	static SECURITY_ATTRIBUTES	asa;
	static int			aclbuf[512];
	static ACL*			acl = (ACL*)&aclbuf[0];
	static SECURITY_DESCRIPTOR	asd;
	static int			sid[SID_BUF_MAX];

	if (initialized < 0)
		return 0;
	if (initialized > 0)
		return &asa;
	initialized = -1;
	CopySid(sizeof(sid), (SID*)sid, &admins_sid_hdr);
	*GetSidSubAuthority((SID*)sid, 1) = DOMAIN_ALIAS_RID_ADMINS;
	InitializeSecurityDescriptor(&asd, SECURITY_DESCRIPTOR_REVISION);
	if (!SetSecurityDescriptorGroup(&asd, (SID*)sid, 0))
		return 0;
	if (!InitializeAcl(acl, sizeof(ACL) + 2 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) + GetLengthSid((SID*)sid) + GetLengthSid(&worldsid), ACL_REVISION))
		return 0;
	if (!AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL|READ_CONTROL|WRITE_DAC, (SID*)sid))
		return 0;
	if (!AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_READ, &worldsid))
		return 0;
	if (!SetSecurityDescriptorDacl(&asd, 1, acl, 0))
		return 0;
	asa.nLength = sizeof(asa);
	asa.bInheritHandle = 0;
	asa.lpSecurityDescriptor = &asd;
	initialized = 1;
	return &asa;
}

/*
 * uwin_release(0,0,buf,sizeof(buf))
 *	get current release in buf
 * uwin_release(rel,root,buf,sizeof(buf))
 *	get sub-release for rel+root in buf
 *	if root is for rel then rel copied to buf
 */

int
uwin_release(const char* release, const char* root, char* rel, size_t relsize)
{
	HKEY	kh;
	DWORD	type;
	DWORD	m;
	char	dir[MAX_PATH];
	char	buf[MAX_PATH];

	if (!release)
	{
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, UWIN_KEY, 0, KEY_READ|KEY_EXECUTE|KEY_WOW64_64KEY, &kh) &&
		    RegOpenKeyEx(HKEY_LOCAL_MACHINE, UWIN_KEY, 0, KEY_READ|KEY_EXECUTE|KEY_WOW64_32KEY, &kh))
			return -1;
		m = sizeof(buf) - 1;
		if (RegQueryValueEx(kh, UWIN_KEY_REL, 0, &type, buf, &m))
			m = 0;
		if (type != REG_SZ)
			m = 0;
		RegCloseKey(kh);
		if (!m)
			return -1;
		buf[m] = 0;
		release = (const char*)buf;
	}
	if (root && !uwin_getkeys(release, dir, sizeof(dir), 0, 0) && strcmp(dir, root))
	{
		if ((strlen(release) + 3) > relsize)
			return -1;
		for (m = 'a';;m++)
		{
			sprintf(rel, "%s.%c", release, m);
			if (uwin_getkeys(rel, dir, sizeof(dir), 0, 0))
				break;
			if (!strcmp(dir, root))
				return 0;
		}
		return 0;
	}
	if (release == (const char*)buf)
	{
		if ((strlen(release) + 1) > relsize)
			return -1;
		strcpy(rel, release);
		return 0;
	}
	return -1;
}

/*
 * return newest release in alt besides release
 * if release is the only release then -1 returned
 */

int
uwin_alternate(const char* release, char* alt, size_t altsize)
{
	HKEY	kh;
	DWORD	n;
	int	i;
	char	buf[MAX_PATH];

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, UWIN_KEY, 0, KEY_ENUMERATE_SUB_KEYS|KEY_WOW64_64KEY, &kh))
		return -1;
	*alt = 0;
	for (i = 0; !RegEnumKey(kh, i, buf, sizeof(buf)); i++)
		if (strcmp(buf, release) && (!*alt || strcmp(buf, alt) > 0))
		{
			n = strlen(buf) + 1;
			if (n > altsize)
			{
				*alt = 0;
				break;
			}
			strcpy(alt, buf);
		}
	RegCloseKey(kh);
	return *alt ? 0 : -1;
}

int
uwin_getkeys(const char* release, char* root, size_t rootsize, char* home, size_t homesize)
{
	int	r = 0;
	HKEY	kh;
	DWORD	size;
	char	key[MAX_PATH];
	char	rel[MAX_PATH];

	if (!release)
	{
		if (uwin_release(0, 0, rel, sizeof(rel)))
			return -1;
		release = (const char*)rel;
	}
	if (*release)
	{
		sprintf(key, "%s\\%s\\%s", UWIN_KEY, release, UWIN_KEY_INST);
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE|KEY_WOW64_64KEY, &kh) &&
		    RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE|KEY_WOW64_32KEY, &kh))
		{
			sprintf(key, "%s%s\\%s", UWIN_KEY, release, UWIN_KEY_INST);
			if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE|KEY_WOW64_32KEY, &kh))
				kh = 0;
		}
	}
	else
		kh = 0;
	if (root)
	{
		if (!kh)
			size = 0;
		else
		{
			size = rootsize - 1;
			if (RegQueryValueEx(kh, UWIN_KEY_ROOT, 0, 0, root, &size))
				size = 0;
		}
		size = rootsize - 1;
		if (root[size])
			root[size] = 0;
		if (!root[0])
		{
			if (sizeof(UWIN_DEF_ROOT) > rootsize)
				r = -1;
			else
				strcpy(root, UWIN_DEF_ROOT);
		}
	}
	if (home)
	{
		if (!kh)
			size = 0;
		else
		{
			size = homesize - 1;
			if (RegQueryValueEx(kh, UWIN_KEY_HOME, 0, 0, home, &size))
				size = 0;
		}
		if (home[size])
			home[size] = 0;
		if (!home[0])
		{
			if (sizeof(UWIN_DEF_HOME) > homesize)
				r = -1;
			else
				strcpy(home, UWIN_DEF_HOME);
		}
	}
	if (kh)
	{
		RegCloseKey(kh);
		return r;
	}
	return -1;
}

int
uwin_setkeys(const char* package, const char* release, const char* version, const char* root, const char* home) 
{
	HKEY		kh;
	int		r;
	DWORD		disposition;
	DWORD		type = REG_SZ;
	char		pkg[MAX_PATH];
	char		key[MAX_PATH];
	char*		key_rel;
	char*		key_inst;
	char*		key_root;
	char*		key_home;

	r = (int)strtol(release, &key_rel, 10) * 1000;
	if (*key_rel == '.')
		r += (int)strtol(key_rel + 1, 0, 10);
	if (r >= 4006)
	{
		key_rel = UWIN_KEY_REL;
		key_inst = UWIN_KEY_INST;
		key_root = UWIN_KEY_ROOT;
		key_home = UWIN_KEY_HOME;
	}
	else
	{
		key_rel = "CurrentVersion";
		key_inst = "UserInfo";
		key_root = "InstallRoot";
		key_home = "UserActiveHomePath";
	}
	r = 0;
	sprintf(key, "%s", UWIN_KEY);
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, security_attributes(), &kh, &disposition))
		r = -1;
	else
	{
		if (RegSetValueEx(kh, key_rel, 0, type, release, strlen(release)))
			r = -1;
		RegCloseKey(kh);
	}
	if (version)
	{
		sprintf(key, "%s\\%s\\%s", UWIN_KEY, release, UWIN_KEY_PKGS);
		if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, security_attributes(), &kh, &disposition))
			r = -1;
		else
		{
#if !_X64_
			typedef			BOOL (WINAPI* Shamwowed_f)(HANDLE, BOOL*);

			BOOL			b;

			static int		initialized = 0;
			static Shamwowed_f	shamwowed;

			if (!initialized)
			{
				initialized = 1;
				shamwowed = (Shamwowed_f)getapi_addr("kernel32.dll", "IsWow64Process");
			}
			if (shamwowed && shamwowed(GetCurrentProcess(), &b) && b)
			{
				sprintf(pkg, "%s*32", package);
				package = (const char*)pkg;
			}
#endif
			if (RegSetValueEx(kh, package, 0, type, version, strlen(version)))
				r = -1;
			RegCloseKey(kh);
		}
	}
	if (root || home)
	{
		sprintf(key, "%s\\%s\\%s", UWIN_KEY, release, key_inst);
		if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, 0, &kh, &disposition))
			r = -1;
		else
		{
			if (root && RegSetValueEx(kh, key_root, 0, type, root, strlen(root)))
				r = -1;
			if (home && RegSetValueEx(kh, key_home, 0, type, home, strlen(home)))
				r = -1;
			RegCloseKey(kh);
		}
	}
	return r;
}
