// ConfigSemDlg.cpp : implementation file
//

#include "globals.h"
#include "stdafx.h"
#include "testcpl.h"
#include "ConfigSemDlg.h"
#include "RegistryOps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigSemDlg dialog


CConfigSemDlg::CConfigSemDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigSemDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigSemDlg)
	m_SemMaxIds = 0;
	m_SemMaxInSys = 0;
	m_SemMaxOpsPerCall = 0;
	m_SemMaxSemPerId = 0;
	m_SemMaxVal = 0;
	//}}AFX_DATA_INIT
}


void CConfigSemDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigSemDlg)
	DDX_Text(pDX, IDC_EDIT_SemMaxIds, m_SemMaxIds);
	DDX_Text(pDX, IDC_EDIT_SemMaxInSys, m_SemMaxInSys);
	DDX_Text(pDX, IDC_EDIT_SemMaxOpsPerCall, m_SemMaxOpsPerCall);
	DDX_Text(pDX, IDC_EDIT_SemMaxSemPerId, m_SemMaxSemPerId);
	DDX_Text(pDX, IDC_EDIT_SemMaxVal, m_SemMaxVal);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigSemDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigSemDlg)
	ON_EN_CHANGE(IDC_EDIT_SemMaxIds, OnChangeEDITSemMaxIds)
	ON_EN_CHANGE(IDC_EDIT_SemMaxInSys, OnChangeEDITSemMaxInSys)
	ON_EN_CHANGE(IDC_EDIT_SemMaxOpsPerCall, OnChangeEDITSemMaxOpsPerCall)
	ON_EN_CHANGE(IDC_EDIT_SemMaxSemPerId, OnChangeEDITSemMaxSemPerId)
	ON_EN_CHANGE(IDC_EDIT_SemMaxVal, OnChangeEDITSemMaxVal)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigSemDlg message handlers

BOOL CConfigSemDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/*
 * Read the semaphore values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigSemDlg::getregval(BOOL flag)
{
	DWORD semVal[5];
	int ret;

	ret = regOps.GetSemVal(semVal, flag);
	m_SemMaxIds = semVal[0];
	m_SemMaxInSys = semVal[1];
	m_SemMaxOpsPerCall = semVal[2];
	m_SemMaxSemPerId = semVal[3];
	m_SemMaxVal = semVal[4];

	return ret;
}

/*
 * Set the semaphore values in the registry.
 */
void CConfigSemDlg::setregval()
{
	DWORD semVal[5];

	semVal[0] = m_SemMaxIds;
	semVal[1] = m_SemMaxInSys;
	semVal[2] = m_SemMaxOpsPerCall;
	semVal[3] = m_SemMaxSemPerId;
	semVal[4] = m_SemMaxVal;
	regOps.SetSemVal(semVal);
}

// Gets the dialog values and stores in member variables
BOOL CConfigSemDlg::getdialogvalues()
{
	DWORD dwSemMaxIds, dwSemMaxInSys, dwSemMaxSemPerId;
	DWORD dwSemMaxVal, dwSemMaxOpsPerCall;
	BOOL error;
	int i;
	char buff[1024];

	*buff = 0;
	dwSemMaxIds = GetDlgItemInt(IDC_EDIT_SemMaxIds, &error, FALSE);
	if(error == 0)
		i = sprintf(buff, "Invalid \"Maximum number of identifiers\"\n");

	dwSemMaxInSys = GetDlgItemInt(IDC_EDIT_SemMaxInSys, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum in system\"\n");

	dwSemMaxOpsPerCall = GetDlgItemInt(IDC_EDIT_SemMaxOpsPerCall,&error,FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum operations per call\"\n");

	dwSemMaxSemPerId = GetDlgItemInt(IDC_EDIT_SemMaxSemPerId, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum number per identifier\"\n");

	dwSemMaxVal = GetDlgItemInt(IDC_EDIT_SemMaxVal, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum value\"\n");

	if(*buff)
	{
		i += sprintf(buff+i, "\nPlease correct them and apply again for changes to take effect");
		::MessageBox(NULL, buff, "Error", MB_ICONSTOP|MB_OK);
		return 0;
	}
	else
	{
		m_SemMaxIds = dwSemMaxIds;
		m_SemMaxInSys = dwSemMaxInSys;
		m_SemMaxOpsPerCall = dwSemMaxOpsPerCall;
		m_SemMaxSemPerId = dwSemMaxSemPerId;
		m_SemMaxVal = dwSemMaxVal;
	}

	return 1;
}

// Sets the dialog values to that in member variables
void CConfigSemDlg::setdialogvalues()
{
	SetDlgItemInt(IDC_EDIT_SemMaxIds, m_SemMaxIds, FALSE);
	SetDlgItemInt(IDC_EDIT_SemMaxSemPerId, m_SemMaxSemPerId, FALSE);
	SetDlgItemInt(IDC_EDIT_SemMaxInSys, m_SemMaxInSys, FALSE);
	SetDlgItemInt(IDC_EDIT_SemMaxOpsPerCall, m_SemMaxOpsPerCall, FALSE);
	SetDlgItemInt(IDC_EDIT_SemMaxVal, m_SemMaxVal, FALSE);
}

void CConfigSemDlg::OnChangeEDITSemMaxIds() 
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
//AfxMessageBox("OnChangeEDITSemMaxIds, enable", MB_OK, 0);
}

void CConfigSemDlg::OnChangeEDITSemMaxInSys() 
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
//AfxMessageBox("OnChangeEDITSemMaxInSys, enable", MB_OK, 0);
}

void CConfigSemDlg::OnChangeEDITSemMaxOpsPerCall() 
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
//AfxMessageBox("OnChangeEDITSemMaxOpsPerCall, enable", MB_OK, 0);
}

void CConfigSemDlg::OnChangeEDITSemMaxSemPerId() 
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
//AfxMessageBox("OnChangeEDITSemMaxSemPerId, enable", MB_OK, 0);
}

void CConfigSemDlg::OnChangeEDITSemMaxVal() 
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
//AfxMessageBox("OnChangeEDITSemMaxVal, enable", MB_OK, 0);
}
