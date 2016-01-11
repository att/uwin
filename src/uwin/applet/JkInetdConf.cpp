// JkInetdConf.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "JkInetdConf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CJkInetdConf dialog


CJkInetdConf::CJkInetdConf(CWnd* pParent /*=NULL*/)
	: CDialog(CJkInetdConf::IDD, pParent)
{
	//{{AFX_DATA_INIT(CJkInetdConf)
	m_PortNumber = 0;
	//}}AFX_DATA_INIT
	m_Enable = 0;
	m_ServerName = "";
}


void CJkInetdConf::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CJkInetdConf)
	DDX_Text(pDX, IDC_EDIT_JK_PORT, m_PortNumber);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CJkInetdConf, CDialog)
	//{{AFX_MSG_MAP(CJkInetdConf)
	ON_BN_CLICKED(IDC_BUTTON_JK_DEFAULT, OnButtonJkDefault)
	ON_BN_CLICKED(IDC_CHECK_JK_ENABLE, OnCheckJkEnable)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CJkInetdConf message handlers

BOOL CJkInetdConf::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	SetDialogValues();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CJkInetdConf::OnButtonJkDefault() 
{
	// TODO: Add your control notification handler code here
	CButton *enableBtn;
	
	if(!strcmp(m_ServerName, "ftp"))
	{
		m_PortNumber = 21;
	}
	else if(!strcmp(m_ServerName, "rlogin"))
	{
		m_PortNumber = 513;
	}
	else if(!strcmp(m_ServerName, "rsh"))
	{
		m_PortNumber = 514;
	}
	else if(!strcmp(m_ServerName, "telnet"))
	{
		m_PortNumber = 23;
	}

	SetDlgItemInt(IDC_EDIT_JK_PORT, m_PortNumber, FALSE);
	
	enableBtn = (CButton *)GetDlgItem(IDC_CHECK_JK_ENABLE);
	if(enableBtn != NULL)
	{
		enableBtn->SetCheck(1);
		m_Enable = 1;
	}
}

void CJkInetdConf::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CJkInetdConf::GetPortNumber(DWORD *port)
{
	*port = m_PortNumber;
}

void CJkInetdConf::SetPortNumber(DWORD port)
{
	m_PortNumber = port;
}

void CJkInetdConf::SetServerName(char *servname)
{
	m_ServerName = servname;
}

void CJkInetdConf::GetEnable(DWORD *enable)
{
	*enable = m_Enable;
}

void CJkInetdConf::SetEnable(DWORD enable)
{
	m_Enable = enable;
}

void CJkInetdConf::SetDialogValues()
{
	CButton *enableBtn;
	
	SetDlgItemText(IDC_EDIT_JK_SERVNAME, m_ServerName);
	SetDlgItemInt(IDC_EDIT_JK_PORT, m_PortNumber, FALSE);
	enableBtn = (CButton *)GetDlgItem(IDC_CHECK_JK_ENABLE);
	if(enableBtn != NULL)
		enableBtn->SetCheck(m_Enable);
}

void CJkInetdConf::OnCheckJkEnable() 
{
	// TODO: Add your control notification handler code here
	CButton *enableBtn;
	
	enableBtn = (CButton *)GetDlgItem(IDC_CHECK_JK_ENABLE);
	if(enableBtn != NULL)
		m_Enable = enableBtn->GetCheck();
}
