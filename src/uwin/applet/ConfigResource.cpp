// ConfigResource.cpp : implementation file
//

#include "globals.h"
#include "stdafx.h"
#include "testcpl.h"
#include "ConfigResource.h"
#include "RegistryOps.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigResource dialog


CConfigResource::CConfigResource(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigResource::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigResource)
	m_Fifo = 0;
	m_File = 0;
	m_Proc = 0;
	m_Sid = 0;
	//}}AFX_DATA_INIT
}


void CConfigResource::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigResource)
	DDX_Text(pDX, IDC_EDIT_FIFO, m_Fifo);
	DDX_Text(pDX, IDC_EDIT_FILE, m_File);
	DDX_Text(pDX, IDC_EDIT_PROC, m_Proc);
	DDX_Text(pDX, IDC_EDIT_SID, m_Sid);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigResource, CDialog)
	//{{AFX_MSG_MAP(CConfigResource)
	ON_EN_CHANGE(IDC_EDIT_FIFO, OnChangeEditFifo)
	ON_EN_CHANGE(IDC_EDIT_FILE, OnChangeEditFile)
	ON_EN_CHANGE(IDC_EDIT_PROC, OnChangeEditProc)
	ON_EN_CHANGE(IDC_EDIT_SID, OnChangeEditSid)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigResource message handlers

BOOL CConfigResource::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/*
 * Read the resource values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigResource::getregval(BOOL flag)
{
	DWORD resVal[4];
	int ret;

	ret = regOps.GetResourceVal(resVal, flag);
	m_Fifo = resVal[0];
	m_File = resVal[1];
	m_Proc = resVal[2];
	m_Sid = resVal[3];

	return ret;
}

/*
 * Set the Resources values in the registry.
 */
void CConfigResource::setregval()
{
	DWORD resVal[4];

	resVal[0] = m_Fifo;
	resVal[1] = m_File;
	resVal[2] = m_Proc;
	resVal[3] = m_Sid;
	regOps.SetResourceVal(resVal);
}

// Gets the dialog values and stores in member variables
BOOL CConfigResource::getdialogvalues()
{
	DWORD dwFifo, dwFile, dwProc, dwSid;
	BOOL error;
	int i;
	char buff[1024];

	*buff = 0;
	dwFifo = GetDlgItemInt(IDC_EDIT_FIFO, &error, FALSE);
	if(error == 0)
		i = sprintf(buff, "Invalid \"Maximum FIFOs\"\n");

	dwFile = GetDlgItemInt(IDC_EDIT_FILE, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum files\"\n");

	dwProc = GetDlgItemInt(IDC_EDIT_PROC, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum processes\"\n");

	dwSid = GetDlgItemInt(IDC_EDIT_SID, &error, FALSE);
	if(error == 0)
		i += sprintf(buff+i, "Invalid \"Maximum SIDs\"\n");

	if(*buff)
	{
		i += sprintf(buff+i, "\nPlease correct them and apply again for changes to take effect");
		::MessageBox(NULL, buff, "Error", MB_ICONSTOP|MB_OK);
		return 0;
	}
	else
	{
		m_Fifo = dwFifo;
		m_File = dwFile;
		m_Proc = dwProc;
		m_Sid = dwSid;
	}

	return 1;
}

// Sets the dialog values to that in member variables
void CConfigResource::setdialogvalues()
{
	SetDlgItemInt(IDC_EDIT_FIFO, m_Fifo, FALSE);
	SetDlgItemInt(IDC_EDIT_FILE, m_File, FALSE);
	SetDlgItemInt(IDC_EDIT_PROC, m_Proc, FALSE);
	SetDlgItemInt(IDC_EDIT_SID, m_Sid, FALSE);
}

void CConfigResource::OnChangeEditFifo() 
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
//AfxMessageBox("OnChangeEditFifo, enable", MB_OK, 0);
}

void CConfigResource::OnChangeEditFile() 
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
//AfxMessageBox("OnChangeEditFile, enable", MB_OK, 0);
}

void CConfigResource::OnChangeEditProc() 
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
//AfxMessageBox("OnChangeEditProc, enable", MB_OK, 0);
}

void CConfigResource::OnChangeEditSid() 
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
//AfxMessageBox("OnChangeEditSid, enable", MB_OK, 0);
}
