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
/**
 * $Id: devmanif.c 488 2005-09-15 12:27:00Z juruotsa $
 *
 * derived from http://code.google.com/p/kuvasin/source/browse/devmanif/devmanif.c
 */

#include "fsnt.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <tchar.h>

#define VOLUME_NAME_SIZE	1024

#define EJECT_CONTINUE		01
#define EJECT_SUCCESS		02

#define RETURN(a)		do { iRetVal = (a); goto FAST_EXIT; } while (0)

typedef BOOL (WINAPI* GetVolumeNameForVolumeMountPoint_f)(LPCTSTR,LPTSTR,DWORD);
typedef BOOL (WINAPI* GetVolumePathName_f)(LPCTSTR,LPTSTR,DWORD);

typedef CONFIGRET	(WINAPI* Get_DevNode_Status_f)(PULONG,PULONG,DEVINST,ULONG);
typedef CONFIGRET	(WINAPI* Get_Parent_f)(PDEVINST,DEVINST,ULONG);
typedef CONFIGRET	(WINAPI* Request_Device_Eject_f)(DEVINST,PPNP_VETO_TYPE,LPSTR,ULONG,ULONG);

typedef BOOL		(WINAPI* SetupDiDestroyDeviceInfoList_f)(HDEVINFO);
typedef BOOL		(WINAPI* SetupDiEnumDeviceInterfaces_f)(HDEVINFO,PSP_DEVINFO_DATA,CONST GUID*,DWORD,PSP_DEVICE_INTERFACE_DATA);
typedef HDEVINFO	(WINAPI* SetupDiGetClassDevs_f)(GUID*,PCSTR,HWND,DWORD);
typedef BOOL		(WINAPI* SetupDiGetDeviceInterfaceDetail_f)(HDEVINFO,PSP_DEVICE_INTERFACE_DATA,PSP_DEVICE_INTERFACE_DETAIL_DATA_A,DWORD,PDWORD,PSP_DEVINFO_DATA);
typedef BOOL		(WINAPI* SetupDiGetDeviceRegistryProperty_f)(HDEVINFO,PSP_DEVINFO_DATA,DWORD,PDWORD,PBYTE,DWORD,PDWORD);

typedef struct Ejectstate_s
{
	int					initialized;
	Get_DevNode_Status_f			Get_DevNode_Status;
	Get_Parent_f				Get_Parent;
	GetVolumeNameForVolumeMountPoint_f	GetVolumeNameForVolumeMountPoint;
	GetVolumePathName_f			GetVolumePathName;
	Request_Device_Eject_f			Request_Device_Eject;
	SetupDiDestroyDeviceInfoList_f		SetupDiDestroyDeviceInfoList;
	SetupDiEnumDeviceInterfaces_f		SetupDiEnumDeviceInterfaces;
	SetupDiGetClassDevs_f			SetupDiGetClassDevs;
	SetupDiGetDeviceInterfaceDetail_f	SetupDiGetDeviceInterfaceDetail;
	SetupDiGetDeviceRegistryProperty_f	SetupDiGetDeviceRegistryProperty;
} Ejectstate_t;

static Ejectstate_t	ejectstate;

static void ejectinit(void)
{
	ejectstate.initialized = -1;
	if (!(ejectstate.GetVolumeNameForVolumeMountPoint = (GetVolumeNameForVolumeMountPoint_f)getsymbol(MODULE_kernel, "GetVolumeNameForVolumeMountPointA")))
		return;
	if (!(ejectstate.GetVolumePathName = (GetVolumePathName_f)getsymbol(MODULE_kernel, "GetVolumePathNameA")))
		return;
	if (!(ejectstate.Get_DevNode_Status = (Get_DevNode_Status_f)getsymbol(MODULE_setup, "CM_Get_DevNode_Status")))
		return;
	if (!(ejectstate.Get_Parent = (Get_Parent_f)getsymbol(MODULE_setup, "CM_Get_Parent")))
		return;
	if (!(ejectstate.Request_Device_Eject = (Request_Device_Eject_f)getsymbol(MODULE_setup, "CM_Request_Device_EjectA")))
		return;
	if (!(ejectstate.SetupDiDestroyDeviceInfoList = (SetupDiDestroyDeviceInfoList_f)getsymbol(MODULE_setup, "SetupDiDestroyDeviceInfoList")))
		return;
	if (!(ejectstate.SetupDiEnumDeviceInterfaces = (SetupDiEnumDeviceInterfaces_f)getsymbol(MODULE_setup, "SetupDiEnumDeviceInterfaces")))
		return;
	if (!(ejectstate.SetupDiGetClassDevs = (SetupDiGetClassDevs_f)getsymbol(MODULE_setup, "SetupDiGetClassDevsA")))
		return;
	if (!(ejectstate.SetupDiGetDeviceInterfaceDetail = (SetupDiGetDeviceInterfaceDetail_f)getsymbol(MODULE_setup, "SetupDiGetDeviceInterfaceDetailA")))
		return;
	if (!(ejectstate.SetupDiGetDeviceRegistryProperty = (SetupDiGetDeviceRegistryProperty_f)getsymbol(MODULE_setup, "SetupDiGetDeviceRegistryPropertyA")))
		return;
	ejectstate.initialized = 1;
}

static int ejectinterface(HDEVINFO aSet, LPGUID aGuid, DWORD i, const char* aVolume)
{
	SP_DEVICE_INTERFACE_DATA		DeviceInterfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
	SP_DEVINFO_DATA				DeviceInfoData = { sizeof(SP_DEVINFO_DATA) };
	PSP_DEVICE_INTERFACE_DETAIL_DATA	DeviceInterfaceDetailData = 0;
	DWORD					RequiredSize;
	int					iRetVal = EJECT_CONTINUE;
	static char				strbuf[PATH_MAX];
	static char				devicepath[PATH_MAX];

	/**
	 * Enumerate index'th interface.
	 */
	if (ejectstate.SetupDiEnumDeviceInterfaces(aSet, 0, aGuid, i, &DeviceInterfaceData))
	{
		/**
		 * Get detailed info for this particular device:
		 * 1. Call with zeroes to get buffer size.
		 */
		if (!ejectstate.SetupDiGetDeviceInterfaceDetail(aSet, &DeviceInterfaceData, 0, 0, &RequiredSize, 0) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			RETURN(0);
		/**
		 * Alloc mem.
		 */
		if (!(DeviceInterfaceDetailData = malloc(RequiredSize)))
			RETURN(0);
		DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		/**
		 * Get detailed info for this particular device:
		 * 2. Call with valid values.
		 */
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		if (!ejectstate.SetupDiGetDeviceInterfaceDetail(aSet, &DeviceInterfaceData, DeviceInterfaceDetailData, RequiredSize, 0, &DeviceInfoData))
			RETURN(0);
		/**
		 * Try to convert received device path into a volume mount point.
		 */
		lstrcpy(devicepath, DeviceInterfaceDetailData->DevicePath);
		lstrcat(devicepath, "\\");
		if (ejectstate.GetVolumeNameForVolumeMountPoint(devicepath, strbuf, sizeof(strbuf)))
		{
			/**
			 * Compare received volume name into the recently generated one.
			 */
			if (!lstrcmp(strbuf,aVolume))
			{
				PNP_VETO_TYPE	err;
				DEVINST		out = DeviceInfoData.DevInst;
				DWORD		data;
				DWORD		capabilities;
				DWORD		bufsize;

				if (ejectstate.SetupDiGetDeviceRegistryProperty(aSet, &DeviceInfoData, SPDRP_CAPABILITIES, &data, (PBYTE)&capabilities, sizeof(int), &bufsize))
				{
					/**
					 * Try to eject only a removable drive.
					 */
					if (!(capabilities & CM_DEVCAP_REMOVABLE))
					{
						/**
						 * Loop ejecting & getting the parent.
						 */
						for (;;)
						{
							/**
							 * Check DevNode's status for DN_REMOVABLE.
							 */
							DWORD	dnStatus;
							DWORD	dnProblem;

							if (ejectstate.Get_DevNode_Status(&dnStatus, &dnProblem, out, 0) == CR_SUCCESS && (dnStatus & DN_REMOVABLE))
							{
								/**
								 * Try to ejectstate.
								 *
								 * JRu, 15-Sep-05
								 * Seems to work on XP now. All I had to do was
								 * to add "err == PNP..." to the if-clause.
								 *
								 * JRu, 16-Sep-05
								 * Refined further: I check the DN_REMOVABLE flag,
								 * and eject based on that.
								 */
								if (ejectstate.Request_Device_Eject(out, &err, devicepath, sizeof(devicepath), 0) == CR_SUCCESS && err == PNP_VetoTypeUnknown)
									RETURN(EJECT_SUCCESS);
								logerr(2, "eject Request_Device_Eject %s failed\n", devicepath);
							}
							/**
							 * Get parent. If this fails, there are no more parents left.
							 */
							if (ejectstate.Get_Parent(&out, out, 0) != CR_SUCCESS)
								RETURN(0);
						}
					}
				}
			}
		}
		else
			RETURN(EJECT_CONTINUE);
	}
	else
		RETURN(0);
 FAST_EXIT:
	free(DeviceInterfaceDetailData);
	return(iRetVal);
}

static int ejectvolume(char* volume)
{
	HDEVINFO	hp;
	DWORD		i;
	int		f;

	static const GUID GUID_DEVINTERFACE_VOLUME = { 0x53f5630dL, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };

	if ((hp = ejectstate.SetupDiGetClassDevs((LPGUID)&GUID_DEVINTERFACE_VOLUME, 0, 0, DIGCF_DEVICEINTERFACE|DIGCF_PRESENT)) == INVALID_HANDLE_VALUE)
	{
		logerr(2, "eject %s SetupDiGetClassDevs failed", volume);
		return -1;
	}
	f = EJECT_CONTINUE;
	for (i = 0; f & EJECT_CONTINUE; i++)
		f = ejectinterface(hp, (LPGUID)&GUID_DEVINTERFACE_VOLUME, i, volume);
	i = GetLastError();
	ejectstate.SetupDiDestroyDeviceInfoList(hp);
	SetLastError(i);
	return (f & EJECT_SUCCESS) ? 0 : -1;
}

int eject(const char* path)
{
	char	drive[2 * PATH_MAX];
	char	volume[sizeof(drive)];

	if (!ejectstate.initialized)
		ejectinit();
	if (ejectstate.initialized < 0)
	{
		errno = ENOSYS;
		return -1;
	}
	if (!ejectstate.GetVolumePathName(path, drive, sizeof(drive)))
	{
		logerr(2, "eject %s GetVolumePathName failed", path);
		goto bad;
	}
	if (!ejectstate.GetVolumeNameForVolumeMountPoint(drive, volume, sizeof(volume)))
	{
		logerr(2, "eject %s GetVolumeNameForVolumeMountPoint %s failed", path, drive);
		goto bad;
	}
	if (ejectvolume(volume))
		goto bad;
	logmsg(0, "eject %s volume %s", path, volume);
	return 0;
 bad:
	errno = unix_err(GetLastError());
	return -1;
}
