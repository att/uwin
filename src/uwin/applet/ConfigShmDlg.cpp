// ConfigShmDlg.cpp : implementation file
//

#include "globals.h"
#include "stdafx.h"
#include "testcpl.h"
#include "ConfigShmDlg.h"
#include "RegistryOps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigShmDlg dialog


CConfigShmDlg::CConfigShmDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigShmDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigShmDlg)
	m_ShmMaxIds = 0;
	m_ShmMaxSegPerProc = 0;
	m_ShmMaxSize = 0;
	m_ShmMinSize = 0;
	//}}AFX_DATA_INIT
}


void CConfigShmDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigShmDlg)
	DDX_Text(pDX, IDC_EDIT_ShmMaxIds, m_ShmMaxIds);
	DDX_Text(pDX, IDC_EDIT_ShmMaxSegPerProc, m_ShmMaxSegPerProc);
	DDX_Text(pDX, IDC_EDIT_ShmMaxSize, m_ShmMaxSize);
	DDX_Text(pDX, IDC_EDIT_ShmMinSize, m_ShmMinSize);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigShmDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigShmDlg)
	ON_EN_CHANGE(IDC_EDIT_ShmMaxIds, OnChangeEDITShmMaxIds)
	ON_EN_CHANGE(IDC_EDIT_ShmMaxSegPerProc, OnChangeEDITShmMaxSegPerProc)
	ON_EN_CHANGE(IDC_EDIT_ShmMaxSize, OnChangeEDITShmMaxSize)
	ON_EN_CHANGE(IDC_EDIT_ShmMinSize, OnChangeEDITShmMinSize)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigShmDlg message handlers

BOOL CConfigShmDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/*
 * Read the shared memory values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigShmDlg::getregval(BOOL flag)
{
	DWORD shmVal[4];
	int ret;

	ret = regOps.GetShmVal(shmVal, flag);
	m_ShmMaxIds = shmVal[0];
	m_ShmMaxSegPerProc = shmVal[1];
	m_ShmMaxSize = shmVal[2];
	m_ShmMinSize = shmVal[3];

	return ret;
}

/*
 * Set the shared memory values in the registry.
 */
void CConfigShmDlg::setregval()
{
	DWORD shmVal[4];

	shmVal[0] = m_ShmMaxIds;
	shmVal[1] = m_ShmMaxSegPerProc;
	shmVal[2] = m_ShmMaxSize;
	shmVal[3] = m_ShmMinSize;
	regOps.SetShmVal(shmVal);
}

// Gets the dialog values and stores in member variables
BOOL CConfigShmDlg::getdialogvalues()
{
	DWORD dwShmMaxIds, dwShmMaxSegPerProc, dwShmMaxSize, dwShmMinSize;
	BOOL error;
	int i;
	char buff[1024];

	*buff = 0;
	dwShmMaxIds = GetDlgItemInt(IDC_EDIT_ShmMaxIds, &error, FALSE);
	if(error == 0)
		i = sprintf(buff, "Invalid \"Maximum number of identifiers\"\n");

	dwShmMaxSegPerProc = GetDlgItemInt(IDC_EDIT_ShmMaxSegPerProc,&error,FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum segments per process\"\n");

	dwShmMaxSize = GetDlgItemInt(IDC_EDIT_ShmMaxSize, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum size\"\n");

	dwShmMinSize = GetDlgItemInt(IDC_EDIT_ShmMinSize, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Minimum size\"\n");

	if(*buff)
	{
		i += sprintf(buff+i, "\nPlease correct them and apply again for changes to take effect");
		::MessageBox(NULL, buff, "Error", MB_ICONSTOP|MB_OK);
		return 0;
	}
	else
	{
		m_ShmMaxIds = dwShmMaxIds;
		m_ShmMaxSegPerProc = dwShmMaxSegPerProc;
		m_ShmMaxSize = dwShmMaxSize;
		m_ShmMinSize = dwShmMinSize;
	}

	return 1;
}

// Sets the dialog values to that in member variables
void CConfigShmDlg::setdialogvalues()
{
	SetDlgItemInt(IDC_EDIT_ShmMaxIds, m_ShmMaxIds, FALSE);
	SetDlgItemInt(IDC_EDIT_ShmMaxSegPerProc, m_ShmMaxSegPerProc, FALSE);
	SetDlgItemInt(IDC_EDIT_ShmMaxSize, m_ShmMaxSize, FALSE);
	SetDlgItemInt(IDC_EDIT_ShmMinSize, m_ShmMinSize, FALSE);
}

void CConfigShmDlg::OnChangeEDITShmMaxIds() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);
//AfxMessageBox("OnChangeEDITShmMaxIds, enable", MB_OK, 0);
}

void CConfigShmDlg::OnChangeEDITShmMaxSegPerProc() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);
//AfxMessageBox("OnChangeEDITShmMaxSegPerProc, enable", MB_OK, 0);
}

void CConfigShmDlg::OnChangeEDITShmMaxSize() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);
//AfxMessageBox("OnChangeEDITShmMaxSize, enable", MB_OK, 0);
}

void CConfigShmDlg::OnChangeEDITShmMinSize() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	// TODO: Add your control notification handler code here
	
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);
//AfxMessageBox("OnChangeEDITShmMinSize, enable", MB_OK, 0);
}
