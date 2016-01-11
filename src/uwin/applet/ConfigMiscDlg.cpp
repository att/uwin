// ConfigMiscDlg.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "ConfigMiscDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigMiscDlg dialog


CConfigMiscDlg::CConfigMiscDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigMiscDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigMiscDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConfigMiscDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigMiscDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigMiscDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigMiscDlg)
	ON_BN_CLICKED(IDC_CHECK_CASESENSITIVE, OnCheckCasesensitive)
	ON_BN_CLICKED(IDC_CHECK_NOSHAMWOW, OnCheckNoShamWoW)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigMiscDlg message handlers

BOOL CConfigMiscDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/*
 * Read the miscellaneous values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigMiscDlg::getregval(BOOL flag)
{
	DWORD miscVal[2];
	int ret;

	ret = regOps.GetMiscVal(miscVal, flag);
	m_CaseSensitive = miscVal[0];
	m_NoShamWoW = miscVal[1];

	return ret;
}

/*
 * Set the miscellaneous values in the registry.
 */
void CConfigMiscDlg::setregval()
{
	DWORD miscVal[2];

	miscVal[0] = m_CaseSensitive;
	miscVal[1] = m_NoShamWoW;
	regOps.SetMiscVal(miscVal);
}

// Gets the dialog values and stores in member variables
BOOL CConfigMiscDlg::getdialogvalues()
{
	CButton *pCaseSensitive = (CButton *)GetDlgItem(IDC_CHECK_CASESENSITIVE);
	if(pCaseSensitive != NULL)
		m_CaseSensitive = pCaseSensitive->GetCheck();
	CButton *pNoShamWoW = (CButton *)GetDlgItem(IDC_CHECK_NOSHAMWOW);
	if(pCaseSensitive != NULL)
		m_NoShamWoW = pNoShamWoW->GetCheck();

	return(1);
}

// Sets the dialog values to that in member variables
void CConfigMiscDlg::setdialogvalues()
{
	CButton *pCaseSensitive = (CButton *)GetDlgItem(IDC_CHECK_CASESENSITIVE);
	if(pCaseSensitive != NULL)
		pCaseSensitive->SetCheck(m_CaseSensitive);
	CButton *pNoShamWoW = (CButton *)GetDlgItem(IDC_CHECK_NOSHAMWOW);
	if(pNoShamWoW != NULL)
		pNoShamWoW->SetCheck(m_NoShamWoW);
}

void CConfigMiscDlg::OnCheckCasesensitive() 
{
	// TODO: Add your control notification handler code here
	CButton *pCaseSensitive = (CButton *)GetDlgItem(IDC_CHECK_CASESENSITIVE);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pCaseSensitive != NULL)
		m_CaseSensitive = pCaseSensitive->GetCheck();

	if(pPropParent)
		pPropParent->SetModified(TRUE);
}

void CConfigMiscDlg::OnCheckNoShamWoW() 
{
	// TODO: Add your control notification handler code here
	CButton *pNoShamWoW = (CButton *)GetDlgItem(IDC_CHECK_NOSHAMWOW);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pNoShamWoW != NULL)
		m_NoShamWoW = pNoShamWoW->GetCheck();

	if(pPropParent)
		pPropParent->SetModified(TRUE);
}
