// UwinGeneral.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "UwinGeneral.h"
#include "globals.h"
#include <uwin_keys.h>
#include <signal.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#if     UWIN_MAGIC==0
#   define string(a)
#else
#   define      stringize(a)    #a
#   define string(a)    "." stringize(a)
#endif

#ifdef STANDALONE
#   define SHMEM_NAME      "uwin.standalone" string(UWIN_MAGIC)
#endif


#ifndef SHMEM_NAME
#   define SHMEM_NAME      "uwin.v1" string(UWIN_MAGIC)
#endif


/////////////////////////////////////////////////////////////////////////////
// CUwinGeneral property page

IMPLEMENT_DYNCREATE(CUwinGeneral, CPropertyPage)

CUwinGeneral::CUwinGeneral() : CPropertyPage(CUwinGeneral::IDD)
{
	//{{AFX_DATA_INIT(CUwinGeneral)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	*m_Release = 0;
}

CUwinGeneral::~CUwinGeneral()
{
}

void CUwinGeneral::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinGeneral)
	DDX_Control(pDX, IDC_COMBO_RELEASE, m_ComboRelease);
	DDX_Control(pDX, IDC_COMBO_LOGLEVEL, m_ComboLogLevel);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinGeneral, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinGeneral)
	ON_CBN_SELENDOK(IDC_COMBO_LOGLEVEL, OnSelendokComboLoglevel)
	ON_CBN_SELENDOK(IDC_COMBO_RELEASE, OnSelendokComboRelease)
	ON_EN_CHANGE(IDC_EDIT_LICENCEKEY, OnChangeEditLicencekey)
	ON_EN_CHANGE(IDC_EDIT_REGUSER, OnChangeEditReguser)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUwinGeneral message handlers

BOOL CUwinGeneral::OnApply() 
{
	// TODO: Add your specialized code here and/or call the base class

	ProcessInstalledReleaseRequest();
	
	SetModified(FALSE);

	return CPropertyPage::OnApply();
}

void CUwinGeneral::ProcessInstalledReleaseRequest()
{
	char*	release;
	char*	root;
	char	cmd[1024];

	if(!(release = regOps.GetRelease()))
		return;
	if(*m_Release && strcmp(m_Release, release))
	{
		// Get install directory for current release
		if(!(root = regOps.GetRoot()))
		{
			::MessageBox(NULL, "Cannot determine release root directory", "UWIN Change Release", MB_OK);
			return;
		}
		sprintf(cmd, "Stop UWIN release %s and restart UWIN release %s?", release, m_Release);
		if(::MessageBox(NULL, cmd, "UWIN Change Release", MB_YESNO|MB_ICONWARNING) == IDYES)
		{
			// release.exe does all the work
			sprintf(cmd, "\"%s/var/uninstall/release.exe\" %s", root, m_Release);
			if(system(cmd) || !regOps.SetRelease(m_Release))
			{
				::MessageBox(NULL, "Release change failed", "UWIN Change Release", MB_OK);
				return;
			}
		}
	}
}

BOOL CUwinGeneral::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// TODO: Add extra initialization here

	// Set System information
	SetOsVersion();

	// Set uwin information
	SetUwinRelease();

	// Set registered user information
	SetRegisteredUser();

	// Set licence key information
	SetLicenceKey();

	// Set error logging level
	SetErrorLoggingLevel();

	// Set installed UWIN releases
	SetInstalledUwinReleases();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CUwinGeneral::SetOsVersion()
{
	CString system;

	sysInfo.GetSysInfo(system);
	SetDlgItemText(IDC_EDIT_OSVERSION, system);
}

void CUwinGeneral::SetUwinRelease()
{
	/* 
	 * The below definition is taken from 
	 * %UWIN_INSTALL_ROOT%\usr\include\ast\limits.h
	 */
#define _POSIX_NAME_MAX 14

	/* 
	 * The below structure is taken from 
	 * %UWIN_INSTALL_ROOT%\usr\include\sys\utsname.h
	 */
	struct utsname 
	{
		char sysname[_POSIX_NAME_MAX];
		char nodename[_POSIX_NAME_MAX];
		char release[_POSIX_NAME_MAX];
		char version[_POSIX_NAME_MAX];
		char machine[_POSIX_NAME_MAX];
	}info;

	char*		release;
	char		uname_string[256]; 
	char		sysdll_path[MAX_PATH], dll_str[MAX_PATH];
	HINSTANCE	hPosix, hAst;
	int		res= 0, (*suname)(struct utsname *),(*skill)(int,int);
	HANDLE		running = OpenFileMapping(FILE_MAP_ALL_ACCESS,FALSE,SHMEM_NAME);

	if(GetSystemDirectory(sysdll_path,sizeof(sysdll_path)) == FALSE)
		goto err;
	// Load proper ast dll
	if(release = regOps.GetRelease())
	{
		if(!strcmp(release, "1.6"))
			sprintf(dll_str, "%s\\ast52.dll", sysdll_path);
		else
			sprintf(dll_str, "%s\\ast54.dll", sysdll_path);
		hAst = LoadLibrary(dll_str);
	}

	// Load posix.dll
	sprintf(dll_str, "%s\\posix.dll", sysdll_path);
	if((hPosix = LoadLibrary(dll_str)) == NULL)
		goto err;

	// Get uname() function pointer
	if((suname = (int (*)(struct utsname *))GetProcAddress(hPosix,"uname")) == NULL)
		goto err;

	// Call uname()
	if(((*suname)(&info)) < 0)
		goto err;
	
	sprintf(uname_string,"%s %s %s %s", info.sysname, info.release, info.version, info.machine);

	SetDlgItemText(IDC_EDIT_UWINVERSION, uname_string);
	res = 1;
err:
	/* if uwin wasn't running then kill the init process */
	if(running)
		CloseHandle(running);
	else if(hPosix && (skill = (int(*)(int,int))GetProcAddress(hPosix,"kill")))
		(*skill)(1,SIGTERM);
	// Unload ast54.dll
	if(hAst)
		FreeLibrary(hAst);
	// Unload posix.dll
	if(hPosix)
		FreeLibrary(hPosix);
	if(!res)
		SetDlgItemText(IDC_EDIT_UWINVERSION, "None");
	return;
}

void CUwinGeneral::SetRegisteredUser()
{
	SetDlgItemText(IDC_EDIT_REGUSER, "www.opensource.org/licenses/" LICENSE_ID);
}

void CUwinGeneral::SetLicenceKey()
{
	SetDlgItemText(IDC_EDIT_LICENCEKEY, "uwin-users@research.att.com");
}

void CUwinGeneral::SetErrorLoggingLevel()
{
	m_ComboLogLevel.SetCurSel(0);
}

void CUwinGeneral::SetInstalledUwinReleases()
{
	DWORD	i;
	LONG	ret;
	HKEY	key;
	char*	rel;
	char	release[64];

	if((ret=RegOpenKeyEx( HKEY_LOCAL_MACHINE,UWIN_KEY,0,KEY_ENUMERATE_SUB_KEYS|KEY_WOW64_64KEY,&key)) != ERROR_SUCCESS)
		return;
	i = 0;
	while (RegEnumKey(key, i++, release, sizeof(release)) == ERROR_SUCCESS)
		m_ComboRelease.AddString(release);
	RegCloseKey(key);

	// Set the selection text
	if (rel = regOps.GetRelease())
	{
		i = m_ComboRelease.FindStringExact(-1, rel);
		if(i != CB_ERR)
			m_ComboRelease.SetCurSel(i);
		strcpy(m_Release, rel);
	}
	return;
}

void CUwinGeneral::OnSelendokComboLoglevel() 
{
	// TODO: Add your control notification handler code here

}

void CUwinGeneral::OnSelendokComboRelease() 
{
	// TODO: Add your control notification handler code here
	char	release[64];

	if (m_ComboRelease.GetLBText(m_ComboRelease.GetCurSel(), release) != CB_ERR)
	{
		if(strcmp(m_Release, release))
		{
			strcpy(m_Release, release);
			SetModified(TRUE);
		}
	}
}

void CUwinGeneral::OnChangeEditLicencekey() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
}

void CUwinGeneral::OnChangeEditReguser() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
}
