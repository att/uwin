// ConfigureDaemon.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "ConfigureDaemon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CConfigureDaemon dialog


CConfigureDaemon::CConfigureDaemon(CWnd* pParent /*=NULL*/)
	: CDialog(CConfigureDaemon::IDD, pParent)
{
	//{{AFX_DATA_INIT(CConfigureDaemon)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CConfigureDaemon::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CConfigureDaemon)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CConfigureDaemon, CDialog)
	//{{AFX_MSG_MAP(CConfigureDaemon)
	ON_BN_CLICKED(IDC_BUTTON_APPLY, OnButtonApply)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, OnButtonBrowse)
	ON_BN_CLICKED(IDC_RADIO_DGRAM, OnRadioDgram)
	ON_BN_CLICKED(IDC_RADIO_NOWAIT, OnRadioNowait)
	ON_BN_CLICKED(IDC_RADIO_STREAM, OnRadioStream)
	ON_BN_CLICKED(IDC_RADIO_TCP, OnRadioTcp)
	ON_BN_CLICKED(IDC_RADIO_UDP, OnRadioUdp)
	ON_BN_CLICKED(IDC_RADIO_WAIT, OnRadioWait)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CConfigureDaemon message handlers

void CConfigureDaemon::OnButtonApply() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnButtonBrowse() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioDgram() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioNowait() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioStream() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioTcp() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioUdp() 
{
	// TODO: Add your control notification handler code here
	
}

void CConfigureDaemon::OnRadioWait() 
{
	// TODO: Add your control notification handler code here
	
}
