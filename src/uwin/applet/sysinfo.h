#ifndef _SYSINFO_H
#define _SYSINFO_H

// Include Files

#include <windows.h>
#include "stdafx.h"
#include "stdafx.h"
#include "RegistryOps.h"

class CSystemInfo
{
	public:
		CSystemInfo();
		~CSystemInfo();

		void GetSysInfo(CString&);

		void GetOSInfo();
		void GetProcessorInfo();
		void GetFileSystemInfo();

	protected:
		CString m_OSInfo, m_ProcessorInfo, m_FileSysInfo;
		int m_Platform; // Set by GetOsInfo() and used by GetProcessorInfo()

		CRegistryOps regOps;
};

#endif // _SYSINFO_H
