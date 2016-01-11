// ConfigMsgDlg.cpp : implementation file
//

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "globals.h"
#include "stdafx.h"
#include "testcpl.h"
#include "ConfigMsgDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CConfigMsgDlg dialog


CConfigMsgDlg::CConfigMsgDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigMsgDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigMsgDlg)
	m_MsgMaxIds = 0;
	m_MsgMaxQSize = 0;
	m_MsgMaxSize = 0;
	//}}AFX_DATA_INIT
	
}


void CConfigMsgDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigMsgDlg)
	DDX_Text(pDX, IDC_EDIT_MsgMaxIds, m_MsgMaxIds);
	DDX_Text(pDX, IDC_EDIT_MsgMaxQSize, m_MsgMaxQSize);
	DDX_Text(pDX, IDC_EDIT_MsgMaxSize, m_MsgMaxSize);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigMsgDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigMsgDlg)
	ON_EN_CHANGE(IDC_EDIT_MsgMaxIds, OnChangeEDITMsgMaxIds)
	ON_EN_CHANGE(IDC_EDIT_MsgMaxQSize, OnChangeEDITMsgMaxQSize)
	ON_EN_CHANGE(IDC_EDIT_MsgMaxSize, OnChangeEDITMsgMaxSize)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigMsgDlg message handlers

BOOL CConfigMsgDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/*
 * Read the message queue values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigMsgDlg::getregval(BOOL flag)
{
	DWORD msgVal[3];
	int ret;

	ret = regOps.GetMsgVal(msgVal, flag);
	m_MsgMaxIds = msgVal[0];
	m_MsgMaxQSize = msgVal[1];
	m_MsgMaxSize = msgVal[2];

	return ret;
}

/*
 * Set the message queue values in the registry.
 */
void CConfigMsgDlg::setregval()
{
	DWORD msgVal[3];

	msgVal[0] = m_MsgMaxIds;
	msgVal[1] = m_MsgMaxQSize;
	msgVal[2] = m_MsgMaxSize;
	regOps.SetMsgVal(msgVal);
}

// Gets the dialog values and stores in member variables
BOOL CConfigMsgDlg::getdialogvalues()
{
	DWORD dwMsgMaxIds, dwMsgMaxQSize, dwMsgMaxSize;
	BOOL error;
	int i;
	char buff[1024];

	*buff = 0;
	
	dwMsgMaxIds = GetDlgItemInt(IDC_EDIT_MsgMaxIds, &error, FALSE);
	if(error == 0)
		i = sprintf(buff, "Invalid \"Maximum number of identifiers\"\n");

	dwMsgMaxQSize = GetDlgItemInt(IDC_EDIT_MsgMaxQSize, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum queue size\"\n");

	dwMsgMaxSize = GetDlgItemInt(IDC_EDIT_MsgMaxSize, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum size of a message in queue\"\n");

	if(*buff)
	{
		i += sprintf(buff+i, "\nPlease correct them and apply again for changes to take effect");
		::MessageBox(NULL, buff, "Error", MB_ICONSTOP|MB_OK);
		return 0;
	}
	else
	{
		m_MsgMaxIds = dwMsgMaxIds;
		m_MsgMaxQSize = dwMsgMaxQSize;
		m_MsgMaxSize = dwMsgMaxSize;
	}

	return 1;
}

// Sets the dialog values to that in member variables
void CConfigMsgDlg::setdialogvalues()
{
	SetDlgItemInt(IDC_EDIT_MsgMaxIds, m_MsgMaxIds, FALSE);
	SetDlgItemInt(IDC_EDIT_MsgMaxQSize, m_MsgMaxQSize, FALSE);
	SetDlgItemInt(IDC_EDIT_MsgMaxSize, m_MsgMaxSize, FALSE);
}

void CConfigMsgDlg::OnChangeEDITMsgMaxIds() 
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
}

void CConfigMsgDlg::OnChangeEDITMsgMaxQSize() 
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
}

void CConfigMsgDlg::OnChangeEDITMsgMaxSize() 
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
}
