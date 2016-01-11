// Header file for RegistryOps.cpp

#ifndef _REGISTRYOPS_H
#define _REGISTRYOPS_H

#include <windows.h>
#include <uwin_keys.h>
#include "stdafx.h"

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY	(0x0200)
#endif
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY	(0x0100)
#endif

class CRegistryOps
{
    public :
		CRegistryOps();
		~CRegistryOps();

		char* GetRelease();
		int SetRelease(char*);

		HKEY GetKey(const char*, int);
		HKEY SetKey(const char*);

		char* GetRoot();

		int GetMsgVal(DWORD *, BOOL);
		int SetMsgVal(DWORD *);

		int GetSemVal(DWORD *, BOOL);
		int SetSemVal(DWORD *);

		int GetShmVal(DWORD *, BOOL);
		int SetShmVal(DWORD *);

		int GetResourceVal(DWORD *, BOOL);
		int SetResourceVal(DWORD *);

		int GetMiscVal(DWORD *, BOOL);
		int SetMiscVal(DWORD *);
		
		int GetConsoleVal(DWORD *, BOOL);
		int SetConsoleVal(DWORD *);

		int GetTelnetVal(DWORD *, BOOL);
		int SetTelnetVal(DWORD *);

		int GetUmsVal(DWORD *, BOOL);
		int SetUmsVal(DWORD *);

		int GetInetdVal(DWORD *, BOOL);
		int SetInetdVal(DWORD *);

		int SetRunOnceEntry(char *);

		int LogLevel;

	protected :
		char m_Root[MAX_PATH];
		char m_Release[64];
};

#endif // _REGISTRYOPS_H
