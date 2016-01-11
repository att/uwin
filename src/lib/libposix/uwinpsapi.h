/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
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
#ifndef _UWIN_PSAPI_H
#define _UWIN_PSAPI_H	1

/* PSAPI interface function typedefs */

#if _WIN32_WINNT < 0x0501
#undef	_WIN32_WINNT
#define _WIN32_WINNT	0x0501
#endif

#include <psapi.h>

#if _MSC_VER < 1500

#if _MSC_VER
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#endif

typedef union PSAPI_WORKING_SET_BLOCK_u
{
	ULONG_PTR	Flags;
	struct
	{
        ULONG_PTR Protection : 5;
        ULONG_PTR ShareCount : 3;
        ULONG_PTR Shared : 1;
        ULONG_PTR Reserved : 3;
#if defined(_WIN64)
        ULONG_PTR VirtualPage : 52;
#else
        ULONG_PTR VirtualPage : 20;
#endif
	};
} PSAPI_WORKING_SET_BLOCK_t;

typedef struct PSAPI_WORKING_SET_INFORMATION_s
{
	ULONG_PTR			NumberOfEntries;
	PSAPI_WORKING_SET_BLOCK_t	WorkingSetInfo[1];
} PSAPI_WORKING_SET_INFORMATION_t;

#if _MSC_VER
#pragma warning(default:4201)
#pragma warning(default:4214)
#endif

#define PSAPI_WORKING_SET_INFORMATION	PSAPI_WORKING_SET_INFORMATION_t
#define PSAPI_WORKING_SET_BLOCK		PSAPI_WORKING_SET_BLOCK_t

#endif

typedef BOOL  (__stdcall* Psapi_EnumProcessModules_f)(HANDLE, HMODULE*, DWORD, LPDWORD);
typedef DWORD (__stdcall* Psapi_GetModuleBaseName_f)(HANDLE, HMODULE, LPSTR, DWORD);
typedef DWORD (__stdcall* Psapi_GetModuleFileNameEx_f)(HANDLE, HMODULE, LPSTR, DWORD);
typedef BOOL  (__stdcall* Psapi_GetModuleInformation_f)(HANDLE, HMODULE, LPMODULEINFO, DWORD);
typedef DWORD (__stdcall* Psapi_GetProcessImageFileName_f)(HANDLE, LPSTR, DWORD);
typedef BOOL  (__stdcall* Psapi_GetProcessMemoryInfo_f)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
typedef BOOL  (__stdcall* Psapi_QueryWorkingSet_f)(HANDLE, PVOID, DWORD);

typedef struct Psapi_s
{
	Psapi_EnumProcessModules_f	EnumProcessModules;
	Psapi_GetModuleBaseName_f	GetModuleBaseName;
	Psapi_GetModuleFileNameEx_f	GetModuleFileNameEx;
	Psapi_GetModuleInformation_f	GetModuleInformation;
	Psapi_GetProcessImageFileName_f	GetProcessImageFileName;
	Psapi_GetProcessMemoryInfo_f	GetProcessMemoryInfo;
	Psapi_QueryWorkingSet_f		QueryWorkingSet;
} Psapi_t;

/* Defines for PSAPI_WORK_SET_BLOCK protection field, not in psapi.h */
#define PG_NOACCESS	0
#define PG_RDONLY	1
#define PG_EXEC		2
#define PG_EXEC_RDONLY	3
#define PG_RDWR		4
#define PG_COW		5
#define PG_EXEC_RDWR	6
#define PG_EXEC_COW	7

extern Psapi_t*	init_psapi(void);

#endif
