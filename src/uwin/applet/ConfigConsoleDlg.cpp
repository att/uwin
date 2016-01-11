// ConfigConsoleDlg.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "ConfigConsoleDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigConsoleDlg dialog


CConfigConsoleDlg::CConfigConsoleDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigConsoleDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigConsoleDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConfigConsoleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigConsoleDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigConsoleDlg, CDialog)
	//{{AFX_MSG_MAP(CConfigConsoleDlg)
	ON_BN_CLICKED(IDC_BUTTON_PREVIEW, OnButtonPreview)
	ON_BN_CLICKED(IDC_CHECK_BB, OnCheckBb)
	ON_BN_CLICKED(IDC_CHECK_BG, OnCheckBg)
	ON_BN_CLICKED(IDC_CHECK_BI, OnCheckBi)
	ON_BN_CLICKED(IDC_CHECK_BR, OnCheckBr)
	ON_BN_CLICKED(IDC_CHECK_FB, OnCheckFb)
	ON_BN_CLICKED(IDC_CHECK_FG, OnCheckFg)
	ON_BN_CLICKED(IDC_CHECK_FI, OnCheckFi)
	ON_BN_CLICKED(IDC_CHECK_FR, OnCheckFr)
	ON_BN_CLICKED(IDC_RADIO_BLINK, OnRadioBlink)
	ON_BN_CLICKED(IDC_RADIO_BOLD, OnRadioBold)
	ON_BN_CLICKED(IDC_RADIO_UNDERLINE, OnRadioUnderline)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigConsoleDlg message handlers

BOOL CConfigConsoleDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	CButton *pRadio = (CButton *)GetDlgItem(IDC_RADIO_BOLD);
	if(pRadio)
		pRadio->SetCheck(TRUE);
	m_txtattr = RADIO_BOLD;
	
	m_color_bold = FB|FI;
	m_color_blink = FR|FI;
	m_color_underline = FG|FI;
	check_box();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CConfigConsoleDlg::check_box()
{
	CButton *pButton;


	pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
	if(pButton)
		pButton->SetCheck(FALSE);
	pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
	if(pButton)
		pButton->SetCheck(FALSE);

	switch(m_txtattr)
	{
		case RADIO_BOLD:
			// Foreground
			if(m_color_bold & FB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & FR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & FG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & FI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			
			// Background
			if(m_color_bold & BB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & BR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & BG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_bold & BI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			break;
		case RADIO_BLINK:
			// Foreground
			if(m_color_blink & FB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & FR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & FG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & FI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			
			// Background
			if(m_color_blink & BB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & BR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & BG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_blink & BI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			break;
		case RADIO_UNDERLINE:
			// Foreground
			if(m_color_underline & FB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & FR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & FG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & FI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			
			// Background
			if(m_color_underline & BB)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & BR)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & BG)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			if(m_color_underline & BI)
			{
				pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
				if(pButton)
					pButton->SetCheck(TRUE);
			}
			break;
	}
}

/*
 * Read the console values from registry and store them
 * in the class variables. If any of the registry key is absent,
 * the key is created with default value.
 */
int CConfigConsoleDlg::getregval(BOOL flag)
{
	DWORD conVal[3];
	int ret;

	ret = regOps.GetConsoleVal(conVal, flag);
	m_color_blink = conVal[0];
	m_color_bold = conVal[1];
	m_color_underline = conVal[2];

	return ret;
}

/*
 * Set the console values in the registry.
 */
void CConfigConsoleDlg::setregval()
{
	DWORD conVal[3];

	conVal[0] = m_color_blink;
	conVal[1] = m_color_bold;
	conVal[2] = m_color_underline;
	regOps.SetConsoleVal(conVal);
}

// Sets the dialog values accordingly
void CConfigConsoleDlg::setdialogvalues()
{
	check_box(); // Set the new settings
}

// Gets the dialog values and stores in member variables
BOOL CConfigConsoleDlg::getdialogvalues()
{
	CButton *pButton;

	switch(m_txtattr)
	{
		case RADIO_BOLD:
			// Foreground
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
			if(pButton->GetCheck())
				m_color_bold |= FB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
			if(pButton->GetCheck())
				m_color_bold |= FR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
			if(pButton->GetCheck())
				m_color_bold |= FG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
			if(pButton->GetCheck())
				m_color_bold |= FI;
			
			// Background
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
			if(pButton->GetCheck())
				m_color_bold |= BB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
			if(pButton->GetCheck())
				m_color_bold |= BR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
			if(pButton->GetCheck())
				m_color_bold |= BG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
			if(pButton->GetCheck())
				m_color_bold |= BI;
			break;
		case RADIO_BLINK:
			// Foreground
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
			if(pButton->GetCheck())
				m_color_blink |= FB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
			if(pButton->GetCheck())
				m_color_blink |= FR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
			if(pButton->GetCheck())
				m_color_blink |= FG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
			if(pButton->GetCheck())
				m_color_blink |= FI;
			
			// Background
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
			if(pButton->GetCheck())
				m_color_blink |= BB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
			if(pButton->GetCheck())
				m_color_blink |= BR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
			if(pButton->GetCheck())
				m_color_blink |= BG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
			if(pButton->GetCheck())
				m_color_blink |= BI;
			break;
		case RADIO_UNDERLINE:
			// Foreground
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
			if(pButton->GetCheck())
				m_color_underline |= FB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
			if(pButton->GetCheck())
				m_color_underline |= FR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
			if(pButton->GetCheck())
				m_color_underline |= FG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
			if(pButton->GetCheck())
				m_color_underline |= FI;
			
			// Background
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
			if(pButton->GetCheck())
				m_color_underline |= BB;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
			if(pButton->GetCheck())
				m_color_underline |= BR;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
			if(pButton->GetCheck())
				m_color_underline |= BG;
			pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
			if(pButton->GetCheck())
				m_color_underline |= BI;
			break;
	}

	return 1;
}

void CConfigConsoleDlg::OnButtonPreview() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigConsoleDlg::OnCheckBb() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_BB);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);

	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= BB;
			else
				m_color_bold &= ~BB;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= BB;
			else
				m_color_blink &= ~BB;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= BB;
			else
				m_color_underline &= ~BB;
			break;
	}
}

void CConfigConsoleDlg::OnCheckBg() 
{
	// TODO: Add your control notification handler code here

	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_BG);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= BG;
			else
				m_color_bold &= ~BG;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= BG;
			else
				m_color_blink &= ~BG;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= BG;
			else
				m_color_underline &= ~BG;
			break;
	}
}

void CConfigConsoleDlg::OnCheckBi() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_BI);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= BI;
			else
				m_color_bold &= ~BI;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= BI;
			else
				m_color_blink &= ~BI;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= BI;
			else
				m_color_underline &= ~BI;
			break;
	}
}

void CConfigConsoleDlg::OnCheckBr() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_BR);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= BR;
			else
				m_color_bold &= ~BR;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= BR;
			else
				m_color_blink &= ~BR;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= BR;
			else
				m_color_underline &= ~BR;
			break;
	}
}

void CConfigConsoleDlg::OnCheckFb() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_FB);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= FB;
			else
				m_color_bold &= ~FB;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= FB;
			else
				m_color_blink &= ~FB;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= FB;
			else
				m_color_underline &= ~FB;
			break;
	}
}

void CConfigConsoleDlg::OnCheckFg() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_FG);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= FG;
			else
				m_color_bold &= ~FG;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= FG;
			else
				m_color_blink &= ~FG;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= FG;
			else
				m_color_underline &= ~FG;
			break;
	}
}

void CConfigConsoleDlg::OnCheckFi() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_FI);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= FI;
			else
				m_color_bold &= ~FI;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= FI;
			else
				m_color_blink &= ~FI;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= FI;
			else
				m_color_underline &= ~FI;
			break;
	}
}

void CConfigConsoleDlg::OnCheckFr() 
{
	// TODO: Add your control notification handler code here
	
	int ret;
	CButton *pButton = (CButton *)GetDlgItem(IDC_CHECK_FR);
	CWnd *pParent = GetParent();
	CPropertyPage *pPropParent = (CPropertyPage *)pParent;

	if(pPropParent)
		pPropParent->SetModified(TRUE);


	ret = pButton->GetCheck();
	switch(m_txtattr)
	{
		case RADIO_BOLD:
			if(ret)
				m_color_bold |= FR;
			else
				m_color_bold &= ~FR;
			break;
		case RADIO_BLINK:
			if(ret)
				m_color_blink |= FR;
			else
				m_color_blink &= ~FR;
			break;
		case RADIO_UNDERLINE:
			if(ret)
				m_color_underline |= FR;
			else
				m_color_underline &= ~FR;
			break;
	}
}

void CConfigConsoleDlg::OnRadioBlink() 
{
	// TODO: Add your control notification handler code here
	
	m_txtattr = RADIO_BLINK;
	check_box(); // Set the new settings
}

void CConfigConsoleDlg::OnRadioBold() 
{
	// TODO: Add your control notification handler code here
	
	m_txtattr = RADIO_BOLD;
	check_box(); // Set the new settings
}

void CConfigConsoleDlg::OnRadioUnderline() 
{
	// TODO: Add your control notification handler code here
	
	m_txtattr = RADIO_UNDERLINE;
	check_box(); // Set the new settings
}
