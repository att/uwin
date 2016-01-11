#include "stdafx.h"
#include "RegistryOps.h"
#include "globals.h"

CRegistryOps::CRegistryOps()
{
	LogLevel = 1;
	*m_Root = 0;
	*m_Release = 0;
}

CRegistryOps::~CRegistryOps()
{
}

char* CRegistryOps::GetRelease()
{
	HKEY	key;
	DWORD	type;
	DWORD	len;
	int	err;

	if (!*m_Release)
	{
		if (err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, UWIN_KEY, 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &key))
		{
			ErrorBox("Cannot query registry key", UWIN_KEY, err);
			return 0;
		}
		len = sizeof(m_Release);
		err = RegQueryValueEx(key, UWIN_KEY_REL, NULL, &type, (LPBYTE)m_Release, &len);
		RegCloseKey(key);
		if (err || type != REG_SZ)
		{
			ErrorBox("Cannot query registry key", UWIN_KEY "/.../" UWIN_KEY_REL, err);
			return 0;
		}
	}
	return m_Release;
}

int CRegistryOps::SetRelease(char* release)
{
	HKEY	key;
	DWORD	disp;
	int	err;

	if (err = RegCreateKeyEx(HKEY_LOCAL_MACHINE, UWIN_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, NULL, &key, &disp))
	{
		ErrorBox("Cannot create registry key", UWIN_KEY, err);
		return 0;
	}
	err = RegSetValueEx(key, UWIN_KEY_REL, 0, REG_SZ, (BYTE*)release, strlen(release) + 1);
	RegCloseKey(key);
	if (err)
	{
		ErrorBox("Cannot create registry key", UWIN_KEY "/.../" UWIN_KEY_REL, err);
		return 0;
	}
	strcpy(m_Release, release);
	return 1;
}

HKEY CRegistryOps::GetKey(const char* name, int must)
{
	HKEY	key;
	int	err;
	char*	release;
	char	path[256];

	if (!(release = GetRelease()))
		return 0;
	sprintf(path, "%s\\%s\\%s", UWIN_KEY, release, name);
	if (!(err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &key)))
		return key;
	if (must)
		ErrorBox("Cannot query registry key", path, err);
	return 0;
}

HKEY CRegistryOps::SetKey(const char* name)
{
	HKEY	key;
	int	err;
	DWORD	disp;
	char*	release;
	char	path[256];

	if (!(release = GetRelease()))
		return 0;
	sprintf(path, "%s\\%s\\%s", UWIN_KEY, release, name);
	if (!(err = RegCreateKeyEx(HKEY_LOCAL_MACHINE, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS|KEY_WOW64_64KEY, NULL, &key, &disp)))
		return key;
	ErrorBox("Cannot set registry key", path, err);
	return 0;
}

char* CRegistryOps::GetRoot()
{
	HKEY	key;
	DWORD	type;
	DWORD	len;
	int	err;

	if (!*m_Root)
	{
		if (!(key = GetKey(UWIN_KEY_INST, 1)))
			return 0;
		len = sizeof(m_Root);
		err = RegQueryValueEx(key, UWIN_KEY_ROOT, NULL, &type, (LPBYTE)m_Root, &len);
		RegCloseKey(key);
		if (err || type != REG_SZ)
		{
			ErrorBox("Cannot query registry key", UWIN_KEY_INST "/.../" UWIN_KEY_ROOT, err);
			return 0;
		}
		// Remove trailing '\' slash character
		if (m_Root[len-1] == '\\')
			m_Root[len-1] = 0;
	}
	return m_Root;
}

int CRegistryOps::GetMsgVal(DWORD *msgq, BOOL flag)
{
	int	def_flag = 1;
	DWORD	type,data,datalen=sizeof(data);
	HKEY	key;

	*msgq = 32;	// MsgMaxIds
	*(msgq + 1) = 16384; // MsgMaxQSize
	*(msgq + 2) = 4056; // MsgMaxSize

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_IPC, 0)))
		return 0;

	/* Query the Registry for Message queue values */
	if(RegQueryValueEx(key, "MsgMaxIds", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*msgq = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "MsgMaxQSize", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(msgq + 1) = data;
	else
		def_flag = 0;


	if(RegQueryValueEx(key, "MsgMaxSize", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(msgq + 2) = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetMsgVal(DWORD *msgq)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_IPC)))
		return 0;

	*msg = 0;
	/* Set the Registry for Message queue values */
	value =  *msgq;
	if(RegSetValueEx(key, "MsgMaxIds", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"MsgMaxIds\"\n");

	value =  *(msgq + 1);
	if(RegSetValueEx(key, "MsgMaxQSize", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"MsgMaxQSize\"\n");

	value =  *(msgq + 2);
	if(RegSetValueEx(key, "MsgMaxSize", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"MsgMaxSize\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetSemVal(DWORD *sem, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;

	*sem = 32; // SemMaxIds
	*(sem+1) = 512; // SemMaxInSys
	*(sem+2) = 32; // SemMaxOpsPerCall
	*(sem+3) = 16; // SemMaxSemPerId
	*(sem+4) = 32767; // SemMaxVal

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_IPC, 0)))
		return 0;

	/* Query the Registry for semaphore values */
	if(RegQueryValueEx(key, "SemMaxIds", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*sem = data;
	else
		def_flag = 0;


	if(RegQueryValueEx(key, "SemMaxInSys", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(sem + 1) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "SemMaxOpsPerCall", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(sem + 2) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "SemMaxSemPerId", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(sem + 3) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "SemMaxVal", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(sem + 4) = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetSemVal(DWORD *sem)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_IPC)))
		return 0;

	*msg = 0;
	/* Set the Registry for semaphore values */
	value =  *sem;
	if(RegSetValueEx(key, "SemMaxIds", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SemMaxIds\"\n");

	value =  *(sem+1);
	if(RegSetValueEx(key, "SemMaxInSys", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SemMaxInSys\"\n");

	value =  *(sem+2);
	if(RegSetValueEx(key, "SemMaxOpsPerCall", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SemMaxOpsPerCall\"\n");

	value =  *(sem+3);
	if(RegSetValueEx(key, "SemMaxSemPerId", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SemMaxSemPerId\"\n");

	value =  *(sem+4);
	if(RegSetValueEx(key, "SemMaxVal", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SemMaxVal\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetShmVal(DWORD *shm, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;

	*shm = 32; // ShmMaxIds
	*(shm+1) = 6; //ShmMaxSegPerProc
	*(shm+2) = (1L<<30); // ShmMaxSize
	*(shm+3) = 1; // ShmMinSize

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_IPC, 0)))
		return 0;

	/* Query the Registry for shared memory values */
	if(RegQueryValueEx(key, "ShmMaxIds", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*shm = data;
	else
		def_flag = 0;


	if(RegQueryValueEx(key, "ShmMaxSegPerProc", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(shm + 1) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "ShmMaxSize", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(shm + 2) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "ShmMinSize", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(shm + 3) = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetShmVal(DWORD *shm)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_IPC)))
		return 0;

	*msg = 0;
	/* Set the shared memory values in the registry */
	value =  *shm;
	if(RegSetValueEx(key, "ShmMaxIds", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"ShmMaxIds\"\n");

	value =  *(shm + 1);
	if(RegSetValueEx(key, "ShmMaxSegPerProc", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"ShmMaxSegPerProc\"\n");

	value =  *(shm + 2);
	if(RegSetValueEx(key, "ShmMaxSize", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"ShmMaxSize\"\n");

	value =  *(shm + 3);
	if(RegSetValueEx(key, "ShmMinSize", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"ShmMinSize\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);
	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetConsoleVal(DWORD *con, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;

	*con = 12; // BLINK
	*(con+1) = 9; // BOLD
	*(con+2) = 10; // UNDERLINE

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_CON, 0)))
		return 0;

	/* Query the Registry for console values */
	if(RegQueryValueEx(key, "BlinkText", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*con = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "BoldText", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(con + 1) = data;
	else
		def_flag = 0;


	if(RegQueryValueEx(key, "UnderlineText", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(con + 2) = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetConsoleVal(DWORD *con)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_CON)))
		return 0;

	*msg = 0;
	/* Set the Console values in the registry */
	value =  *con;
	if(RegSetValueEx(key, "BlinkText", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"BlinkText\"\n");

	value =  *(con + 1);
	if(RegSetValueEx(key, "BoldText", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"BoldText\"\n");

	value =  *(con + 2);
	if(RegSetValueEx(key, "UnderlineText", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"UnderlineText\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetResourceVal(DWORD *res, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;

	*res = 64; // Fifo max
	*(res+1) = 1024; // File max
	*(res+2) = 256; // Proc max
	*(res+3) = 128; // SID max

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_RES, 0)))
		return 0;

	/* Query the Registry for Resource values */
	if(RegQueryValueEx(key, "FifoMax", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*res = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "FileMax", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(res + 1) = data;
	else
		def_flag = 0;


	if(RegQueryValueEx(key, "ProcMax", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(res + 2) = data;
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "SidMax", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		*(res + 3) = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetResourceVal(DWORD *res)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_RES)))
		return 0;

	*msg = 0;
	/* Set the Resource values in the registry */
	value =  *res;
	if(RegSetValueEx(key, "FifoMax", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"FifoMax\"\n");

	value =  *(res + 1);
	if(RegSetValueEx(key, "FileMax", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"FileMax\"\n");

	value =  *(res + 2);
	if(RegSetValueEx(key, "ProcMax", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"ProcMax\"\n");

	value =  *(res + 3);
	if(RegSetValueEx(key, "SidMax", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"SidMax\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetMiscVal(DWORD *misc, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;

	misc[0] = 0; // Case sensitivity is disabled by default
	misc[1] = 0; // ShamWoW is enabled by default

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_RES, 0)))
		return 0;

	/* Query the Registry for Miscellaneous values */
	if(RegQueryValueEx(key, "CaseSensitive", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		misc[0] = data;
	else
		def_flag = 0;
	if(RegQueryValueEx(key, "NoShamWoW", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		misc[1] = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetMiscVal(DWORD *misc)
{
	DWORD value;
	HKEY key;
	char msg[1024];

	if (!(key = SetKey(UWIN_KEY_RES)))
		return 0;

	*msg = 0;
	/* Set the Miscellaneous values in the registry */
	value =  misc[0];
	if(RegSetValueEx(key, "CaseSensitive", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"CaseSensitive\"\n");
	value =  misc[1];
	if(RegSetValueEx(key, "NoShamWoW", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"NoShamWoW\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::GetTelnetVal(DWORD *telnet, BOOL flag)
{
	return 1;
}

int CRegistryOps::SetTelnetVal(DWORD *telnet)
{
	return 1;
}

int CRegistryOps::GetUmsVal(DWORD *ums, BOOL flag)
{
	return 1;
}

int CRegistryOps::SetUmsVal(DWORD *ums)
{
	return 1;
}

int CRegistryOps::GetInetdVal(DWORD *inetd, BOOL flag)
{
	int def_flag = 1;
	DWORD type,data,datalen=sizeof(DWORD);
	HKEY key;
	char value[64];
	DWORD value_len;

	inetd[0] = 1;						  // FTP_DISABLE
	inetd[1] = 21;	 			  // FTP_PORT
	inetd[2] = 1;	 			  // RLOGIN_DISABLE
	inetd[3] = 513;				  // RLOGIN_PORT
	inetd[4] = 1; 				  // RSH_DISABLE
	inetd[5] = 514;			      // RSH_PORT
	inetd[6] = 1; 			 	  // TELNET_DISABLE
	inetd[7] = 23;	 			  // TELNET_PORT

	if(flag == DEFAULT)
	    return 1;

	if (!(key = GetKey(UWIN_KEY_INET, 0)))
		return 0;

	/* Query the Registry for inetd values */
	value_len = sizeof(value);
	if(RegQueryValueEx(key, "FTP_DISABLE", NULL, &type,(LPBYTE)value, &value_len)==0 && type == REG_SZ)
	{
		if(*value == 0)
			inetd[0] = 1; 	// Service  Enabled
		else
			inetd[0] = 0;	// Service Disabled
	}
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "FTP_PORT", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		inetd[1] = data;
	else
		def_flag = 0;

	value_len = sizeof(value);
	if(RegQueryValueEx(key, "RLOGIN_DISABLE", NULL, &type,(LPBYTE)value, &value_len)==0 && type == REG_SZ)
	{
		if(*value == 0)
			inetd[2] = 1;
		else
			inetd[2] = 0;
	}
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "RLOGIN_PORT", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		inetd[3] = data;
	else
		def_flag = 0;

	value_len = sizeof(value);
	if(RegQueryValueEx(key, "RSH_DISABLE", NULL, &type,(LPBYTE)value, &value_len)==0 && type == REG_SZ)
	{
		if(*value == 0)
			inetd[4] = 1;
		else
			inetd[4] = 0;
	}
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "RSH_PORT", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		inetd[5] = data;
	else
		def_flag = 0;

	value_len = sizeof(value);
	if(RegQueryValueEx(key, "TELNET_DISABLE", NULL, &type,(LPBYTE)value, &value_len)==0 && type == REG_SZ)
	{
		if(*value == 0)
			inetd[6] = 1;
		else
			inetd[6] = 0;
	}
	else
		def_flag = 0;

	if(RegQueryValueEx(key, "TELNET_PORT", NULL, &type,(LPBYTE)&data, &datalen)==0 && type == REG_DWORD)
		inetd[7] = data;
	else
		def_flag = 0;

	RegCloseKey(key);
	return def_flag;
}

int CRegistryOps::SetInetdVal(DWORD *inetd)
{
	DWORD value;
	HKEY key;
	char msg[1024],data[64] = "1";

	if (!(key = SetKey(UWIN_KEY_INET)))
		return 0;

	*msg = 0;

	/* Set the Registry for InetdConf values */
	if(inetd[0] == 1)
		*data = 0;
	else
		*data = '1';
	if(RegSetValueEx(key, "FTP_DISABLE", 0, REG_SZ, (BYTE *)data, strlen(data)+1))
		strcat(msg, "Could not set \"FTP_DISABLE\"\n");

	value = inetd[1];
	if(RegSetValueEx(key, "FTP_PORT", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"FTP_PORT\"\n");

	if(inetd[2] == 1)
		*data = 0;
	else
		*data = '1';
	if(RegSetValueEx(key, "RLOGIN_DISABLE", 0, REG_SZ, (BYTE *)data, strlen(data)+1))
		strcat(msg, "Could not set \"RLOGIN_DISABLE\"\n");

	value = inetd[3];
	if(RegSetValueEx(key, "RLOGIN_PORT", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"RLOGIN_PORT\"\n");


	if(inetd[4] == 1)
		*data = 0;
	else
		*data = '1';
	if(RegSetValueEx(key, "RSH_DISABLE", 0, REG_SZ, (BYTE *)data, strlen(data)+1))
		strcat(msg, "Could not set \"RSH_DISABLE\"\n");

	value = inetd[5];
	if(RegSetValueEx(key, "RSH_PORT", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"RSH_PORT\"\n");

	if(inetd[6] == 1)
		*data = 0;
	else
		*data = '1';
	if(RegSetValueEx(key, "TELNET_DISABLE", 0, REG_SZ, (BYTE *)data, strlen(data)+1))
		strcat(msg, "Could not set \"TELNET_DISABLE\"\n");

	value = inetd[7];
	if(RegSetValueEx(key, "TELNET_PORT", 0, REG_DWORD, (BYTE *)&value, sizeof(value)))
		strcat(msg, "Could not set \"TELNET_PORT\"\n");

	if(*msg)
		ErrorBox(msg, NULL, -1);

	RegCloseKey(key);
	return 1;
}

int CRegistryOps::SetRunOnceEntry(char *entry)
{
	char subkey[] = "SOFTWARE\\Microsoft\\Windows\\RunningVersion\\RunOnce";
	HKEY runOnce;
	int err;

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &runOnce))
		return 0;

	if(err = RegSetValueEx(runOnce, "UWIN Change Version", 0, REG_SZ, (BYTE *)entry, strlen(entry)))
	{
		RegCloseKey(runOnce);
		ErrorBox("Could not set \"UWIN Change Version\"", subkey, err);
		return 0;
	}
	RegCloseKey(runOnce);

	return 1;
}
