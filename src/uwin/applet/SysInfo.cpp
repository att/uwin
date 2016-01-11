#include "stdafx.h"
#include "sysinfo.h"

CSystemInfo::CSystemInfo()
{
	m_OSInfo = "-";
	m_ProcessorInfo = "-";
	m_FileSysInfo = "-";
	m_Platform = 0;
}

CSystemInfo::~CSystemInfo()
{
}

/* 
 * Returns the following system information in a single string
 * 		OS information
 * 		Processor Information
 * 		File system information
 */
void CSystemInfo::GetSysInfo(CString& cSysInfo)
{
	GetOSInfo();
	GetProcessorInfo();
	GetFileSystemInfo();

	cSysInfo = m_OSInfo;
	cSysInfo += " / ";
	cSysInfo += m_FileSysInfo;
	cSysInfo += " / ";
	cSysInfo += m_ProcessorInfo;

	return;
}

/*
 * Retrieves information about the operating system and sets
 * the member variable "m_Platform" to  
 * 		0 => Error
 * 		1 => Win95/98
 * 		2 => WinNT
 */
void CSystemInfo::GetOSInfo()
{
	OSVERSIONINFO	osver;
	char			temp[256];

	osver.dwOSVersionInfoSize = sizeof(osver);
	if(!GetVersionEx(&osver))
	{
		m_Platform = 0;
		m_OSInfo = "-";
		return;
	}

	switch(osver.dwPlatformId)
	{
		case VER_PLATFORM_WIN32_WINDOWS:
			m_Platform = 1;
			m_OSInfo = "Windows ";
			if(osver.dwMinorVersion == 0)
				m_OSInfo += "95";
			else if(osver.dwMinorVersion == 10)
				m_OSInfo += "98";
			sprintf(temp, "(Build:%d %s)", osver.dwBuildNumber & 0x0FF, osver.szCSDVersion);
			m_OSInfo += temp;
			break;

		case VER_PLATFORM_WIN32_NT:
			m_Platform = 2;
			/*
			m_OSInfo = "WindowsNT ";
			sprintf(m_OSInfo, "WindowsNT %d.%d", osver.dwMajorVersion, osver.dwMinorVersion);
			if(osver.dwMinorVersion == 51)
				m_OSInfo += "3.51";
			else if(osver.dwMinorVersion == 0)
				m_OSInfo += "4.0";
			*/
			sprintf(temp, "WindowsNT %d.%d (Build:%d %s)", osver.dwMajorVersion, osver.dwMinorVersion, osver.dwBuildNumber, osver.szCSDVersion);
			m_OSInfo = temp;
			break;
	}

	return;
}

/*
 * Retrieves processor information
 */
void CSystemInfo::GetProcessorInfo()
{
	SYSTEM_INFO		sysinfo;
	MEMORYSTATUS	memstat;
	char			temp[256];

	GetSystemInfo(&sysinfo);

	if(m_Platform == 1) /* Win95/98 */
	{
		sprintf(temp, "Intel i%d", sysinfo.dwProcessorType);
		m_ProcessorInfo = temp;
	}
	else if(m_Platform == 2) /* WinNT */
	{
		switch(sysinfo.wProcessorArchitecture)
		{
			case PROCESSOR_ARCHITECTURE_INTEL:
				sprintf(temp, "Intel i%d86", sysinfo.wProcessorLevel);
				m_ProcessorInfo = temp;
				break;

			case PROCESSOR_ARCHITECTURE_ALPHA:
				sprintf(temp, "Alpha %d", sysinfo.wProcessorLevel);
				m_ProcessorInfo = temp;
				break;

			case PROCESSOR_ARCHITECTURE_UNKNOWN:
				m_ProcessorInfo = "Unknown";
				break;
		}
	}

	sprintf(temp, "(%d processor)", sysinfo.dwNumberOfProcessors);
	m_ProcessorInfo += temp;

	GlobalMemoryStatus(&memstat);
	sprintf(temp, "(%d KB)", memstat.dwTotalPhys/1024);
	m_ProcessorInfo += temp;

	return;
}

/* 
 * Retrieves file system information
 */
void CSystemInfo::GetFileSystemInfo()
{
	DWORD	maxcomplen, fsflags;
	char*	root;
	char	filesystemname[256], drive[4];

	// Get UWIN installed root
	if(!(root = regOps.GetRoot()))
		return;

	// Extract drive from install root
	drive[0] = root[0];
	drive[1] = root[1];
	drive[2] = '\\';
	drive[3] = 0;

	if(!GetVolumeInformation(drive, NULL, 0, NULL, &maxcomplen, &fsflags, filesystemname, sizeof(filesystemname)))
		return;

	m_FileSysInfo = filesystemname;

	if(fsflags & FS_CASE_IS_PRESERVED)
		m_FileSysInfo += " (case preserving)";
	else if(fsflags & FS_CASE_SENSITIVE)
		m_FileSysInfo += " (case sensitive)";

	return;
}
