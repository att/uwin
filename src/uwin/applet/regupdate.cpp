#include "globals.h"
#include "stdafx.h"
#include "RegistryOps.h"

#define elementsof(x)	(sizeof(x)/sizeof(x[0]))

typedef struct Keytab_s
{
	const char*	name;
	int		data;
} Keytab_t;

static const Keytab_t	ums_keys[] =
{
	0,			0,
	"setuidFlag",		1,
	"domainEnumFlag",	0,
	"pswdFlag",		1,
	"genLocalPasswd",	1,
};

static const Keytab_t	telnet_keys[] =
{
	0,			0,
	"telnetPort",		1,
	"maxConnections",	1,
};

static int get_config(const Keytab_t tab[], int opt)
{
	CRegistryOps	regOps;
	HKEY		key;
	int		err;
	DWORD		dwData;
	DWORD		dwType;
	DWORD		dwRead;
	char		buf[64];

	if (!(key = regOps.GetKey("UMS", 1)))
		return 0;
	dwRead = sizeof(dwData);
	if (err = RegQueryValueEx(key, tab[opt].name, NULL, &dwType, (BYTE*)&dwData, &dwRead))
	{
		sprintf(buf, "UMS/.../%s", tab[opt].name);
		ErrorBox("Cannot query registry key", buf, err);
		dwData = tab[opt].data;
	}
	RegCloseKey(key);
	return dwData;
}

int get_config_ums(int opt)
{
	return get_config(ums_keys, opt);
}

int get_config_telnetd(int opt)
{
	return get_config(telnet_keys, opt);
}

static int set_config(const Keytab_t tab[], int opt, int data)
{
	CRegistryOps	regOps;
	HKEY		key;
	int		err;
	DWORD		dwData;
	char		buf[64];

	if (!(key = regOps.SetKey("UMS")))
	{
		ErrorBox("Cannot set registry key", "UMS", 0);
		return 0;
	}
	dwData = !!data;
	if (err = RegSetValueEx(key, tab[opt].name, 0, REG_DWORD, (BYTE*)&dwData, sizeof(dwData)))
	{
		sprintf(buf, "UMS/.../%s", tab[opt].name);
		ErrorBox("Cannot set registry key", buf, err);
	}
	RegCloseKey(key);
	return err;
}

int set_config_ums(int data, int opt)
{
	return set_config(ums_keys, opt, data);
}

int set_config_telnetd(int data, int opt)
{
	return set_config(telnet_keys, opt, data);
}

static int default_regkeys(const Keytab_t tab[], int n)
{
	CRegistryOps	regOps;
	HKEY		key;
	int		err;
	int		i;
	DWORD		dwData;
	char		buf[64];

	if (!(key = regOps.GetKey("UMS", 0)))
	{
		if (!(key = regOps.SetKey("UMS")))
			return 0;
		for (i = 1; i < n; i++)
		{
			dwData = tab[i].data;
			if (err = RegSetValueEx(key, tab[i].name, 0, REG_DWORD, (BYTE*)&dwData, sizeof(dwData)))
			{
				sprintf(buf, "UMS/.../%s", tab[i].name);
				ErrorBox("Cannot set registry key", buf, err);
			}
		}
	}
	RegCloseKey(key);
	return 1;
}

int ums_regkeys()
{
	return default_regkeys(ums_keys, elementsof(ums_keys));
}

int telnet_regkeys()
{
	return default_regkeys(telnet_keys, elementsof(telnet_keys));
}
