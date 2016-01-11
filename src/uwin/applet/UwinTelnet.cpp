// UwinTelnet.cpp : implementation file
//

#include "stdafx.h"
#include "UwinTelnet.h"
#include "globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

# define setcheckapply(pBtn,id) pBtn = (CButton *) GetDlgItem(id);\
		ASSERT(pBtn !=NULL);\
		if( pBtn->GetCheck() )\
		{	SetModified(TRUE);	return;	}

/////////////////////////////////////////////////////////////////////////////
// CUwinTelnet property page

IMPLEMENT_DYNCREATE(CUwinTelnet, CPropertyPage)

CUwinTelnet::CUwinTelnet() : CPropertyPage(CUwinTelnet::IDD)
{
	bIsTelnetStartClicked = FALSE;
	bIsTelnetStopClicked = FALSE;
	bIsTelnetInstallClicked = FALSE;
	bIsTelnetDeleteClicked = FALSE;
	


	//{{AFX_DATA_INIT(CUwinTelnet)
	m_szAccepted = _T("");
	m_szDependencies = _T("");
	m_szServiceName = _T("");
	m_szState = _T("");
	//}}AFX_DATA_INIT
}

CUwinTelnet::~CUwinTelnet()
{
}

void CUwinTelnet::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinTelnet)
	DDX_Control(pDX, IDC_COMBO_TELNET_STARTUP, m_ComboBox);
	DDX_Text(pDX, IDC_EDIT_TELNET_ACCEPTED, m_szAccepted);
	DDX_Text(pDX, IDC_EDIT_TELNET_DEPENDECIES, m_szDependencies);
	DDX_Text(pDX, IDC_EDIT_TELNET_SERVICE_NAME, m_szServiceName);
	DDX_Text(pDX, IDC_EDIT_TELNET_STATE, m_szState);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinTelnet, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinTelnet)
	ON_BN_CLICKED(IDC_RADIO_TELNET_DELETE, OnRadioTelnetDelete)
	ON_BN_CLICKED(IDC_RADIO_TELNET_INSTALL, OnRadioTelnetInstall)
	ON_BN_CLICKED(IDC_RADIO_TELNET_START, OnRadioTelnetStart)
	ON_BN_CLICKED(IDC_RADIO_TELNET_STOP, OnRadioTelnetStop)
	ON_EN_CHANGE(IDC_EDIT_TELNET_PORT, OnChangeEditTelnetPort)
	ON_EN_CHANGE(IDC_EDIT_TELNET_MAXCONNS, OnChangeEditMaxConns)
	ON_CBN_SELENDOK(IDC_COMBO_TELNET_STARTUP, OnSelendokComboTelnetStartup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CUwinTelnet message handlers

BOOL CUwinTelnet::OnInitDialog() 
{
	
	char dispname[500];
	char temp[500],buffer[2048];
	int len=0;
	SERVICE_STATUS *stat;
	QUERY_SERVICE_CONFIG *config;
	stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS));
	config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);
	
	CWnd *pWnd = GetDlgItem(IDC_COMBO_TELNET_STARTUP);
	SetWindowLong(pWnd->m_hWnd,GWL_STYLE,
		(WS_CHILD|WS_VISIBLE|WS_VSCROLL |WS_HSCROLL|WS_GROUP |WS_TABSTOP|CBS_DROPDOWNLIST)|GetWindowLong(pWnd->m_hWnd,GWL_STYLE));
		
	CPropertyPage::OnInitDialog();
	
	if(!stat || !config)
		return 0;
	if(getservice_displayname("uwin_telnetd",dispname))
			m_szServiceName.Format("%s",dispname);
	else
	{
		CWnd *tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		 tmp = GetDlgItem(IDC_RADIO_TELNET_DELETE);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		 tmp = GetDlgItem(IDC_RADIO_TELNET_START);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		return 0;
	}

	telnet_regkeys(); //Creates Registry Entries If they does not exist
	Sleep(100);
	int port= get_config_telnetd(1);
		SetDlgItemInt(IDC_EDIT_TELNET_PORT,port,FALSE);
	int maxconn= get_config_telnetd(2);
		SetDlgItemInt(IDC_EDIT_TELNET_MAXCONNS,maxconn,FALSE);
	query_servicestatus("uwin_telnetd",stat);
	convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
	m_szState.Format("%s",temp);
	
	if(!stricmp("Running",temp))
	{
		/* disable install start buttons */
		CWnd *tmp= GetDlgItem(IDC_RADIO_TELNET_START);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		tmp = GetDlgItem(IDC_RADIO_TELNET_INSTALL);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);

	}
	else if(!stricmp("Stopped",temp))
	{
		/*disable install and stop */
		CWnd *tmp = GetDlgItem(IDC_RADIO_TELNET_INSTALL);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
	}

	
	convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
	m_szAccepted.Format("%s",temp);
	query_service_config("uwin_telnetd",config);

	char *ptr = config->lpDependencies;
	while(*ptr != NULL)
	{
		if(len == 0)
			strcpy(buffer,ptr);
		else
		{
			strcat(buffer,",");
			strcat(buffer,ptr);
		}
		len = strlen(ptr);
		ptr++; ptr=ptr+len;
	}
	
	m_szDependencies.Format("%s",buffer);
	config_status_to_string(temp,config->dwStartType,1);
	m_ComboBox.AddString("Auto Start");
	m_ComboBox.AddString("Demand Start");
	m_ComboBox.AddString("Disabled");

	if(!stricmp("Auto Start",temp))
		m_ComboBox.SetCurSel(0);
	else
		if(!stricmp("Demand Start",temp))
			m_ComboBox.SetCurSel(1);
	 else
		if(!stricmp("Disabled",temp))
			m_ComboBox.SetCurSel(2);
	else
		m_ComboBox.SetCurSel(0);

	free(stat); 
	free(config);

	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CUwinTelnet::OnApply() 
{
	int iSelected,iRadioSelected,iTelnetPort,iMaxConns;
	BOOL bEditError;
	CString szComboString;
	CButton *pbtn;
	SERVICE_STATUS *stat;
	QUERY_SERVICE_CONFIG *config;
	char temp[500];
	stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS)+1);
	config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);		
	
	CButton *instbutton = (CButton *)GetDlgItem(IDC_RADIO_TELNET_INSTALL);

	iTelnetPort = GetDlgItemInt(IDC_EDIT_TELNET_PORT,&bEditError,FALSE);
	if((bEditError ==0) && (!instbutton->GetCheck()))
	{
		::MessageBox(NULL,"Invalid Port Number ","Error",MB_OK);		
	}
	else
		 set_config_telnetd(1, iTelnetPort);
	
	iMaxConns = GetDlgItemInt(IDC_EDIT_TELNET_MAXCONNS,&bEditError,FALSE);
	if((bEditError ==0) && (!instbutton->GetCheck()))
	{
		::MessageBox(NULL,"Invalid Number For MaxConnections","Error",MB_OK);
	}
	else
	{
		set_config_telnetd(2, iMaxConns);
	}
	
	iSelected = m_ComboBox.GetCurSel( );
	if(CB_ERR != iSelected)
	{
		m_ComboBox.GetLBText( iSelected,szComboString); 
		config->dwServiceType = SERVICE_NO_CHANGE;
		config->dwErrorControl= SERVICE_NO_CHANGE;
		config->lpBinaryPathName=NULL;
		config->lpLoadOrderGroup = NULL;
		config->dwTagId = NULL;
		config->lpDependencies = NULL;
		config->lpServiceStartName =NULL;
		//config->lpPassword = NULL;
		config->lpDisplayName = NULL;
		if(!stricmp("Auto Start",szComboString))
		{
			config->dwStartType = SERVICE_AUTO_START;
		}
		else
		 if(!stricmp("Demand Start",szComboString))
			config->dwStartType=  SERVICE_DEMAND_START;
		 else if(!stricmp("Disabled",szComboString))
			 config->dwStartType= SERVICE_DISABLED;
		 else
			config->dwStartType = SERVICE_AUTO_START;
			
		set_service_config("uwin_telnetd",config);
	}
		
	iRadioSelected = GetRadioSelected();
	CWnd *tmp;

	switch(iRadioSelected)
	{
		case TELNETINSTALL :
			if (!installservice(UWIN_TELNETD, NULL))
				break;
			
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_INSTALL);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 

			tmp = GetDlgItem(IDC_RADIO_TELNET_INSTALL);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			
			tmp = GetDlgItem(IDC_RADIO_TELNET_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			tmp = GetDlgItem(IDC_RADIO_TELNET_DELETE);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			OnInitDialog();
			break;
	case TELNETDELETE:

		deleteservice("uwin_telnetd");
		Sleep(500);
			
			m_szState.Format("%s","");
			m_szAccepted.Format("%s","");
			m_szDependencies.Format("%s","");
			m_ComboBox.ResetContent();
		
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_DELETE);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 

			tmp = GetDlgItem(IDC_RADIO_TELNET_INSTALL);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_TELNET_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_TELNET_DELETE);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			break;

	case TELNETSTART:
			
			if (!startservice("uwin_telnetd"))
				break;
			Sleep(500);
			query_servicestatus("uwin_telnetd",stat);
			convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
			m_szState.Format("%s",temp);
			convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
			m_szAccepted.Format("%s",temp);

			
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_START);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 
			
			tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_TELNET_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			UpdateData(FALSE);
			break;

	case TELNETSTOP:

			if(!stopservice("uwin_telnetd"))
				break;
			Sleep(500);
			query_servicestatus("uwin_telnetd",stat);
			convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
			m_szState.Format("%s",temp);
			
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_STOP);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 
		

			tmp = GetDlgItem(IDC_RADIO_TELNET_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_TELNET_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	

			UpdateData(FALSE);
			break;
	}
	free(stat);

	return CPropertyPage::OnApply();
}
int CUwinTelnet::GetRadioSelected()
{
	CButton *pbtn;
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_INSTALL);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return TELNETINSTALL;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_DELETE);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return TELNETDELETE;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_START);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return TELNETSTART;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_STOP);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return TELNETSTOP;
	}


	return OPTIONERROR;
}

BOOL CUwinTelnet::OnSetActive() 
{
	return CPropertyPage::OnSetActive();
}

void CUwinTelnet::OnRadioTelnetDelete() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_DELETE);
	bIsTelnetDeleteClicked = !bIsTelnetDeleteClicked;
	bIsTelnetStartClicked = FALSE;
	bIsTelnetStopClicked = FALSE;
	bIsTelnetInstallClicked = FALSE;
	if(pbtn !=NULL)
	{
		if(bIsTelnetDeleteClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
	
}

void CUwinTelnet::OnRadioTelnetInstall() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_INSTALL);
	bIsTelnetInstallClicked = !bIsTelnetInstallClicked;
	bIsTelnetStartClicked = FALSE;
	bIsTelnetStopClicked = FALSE;
	bIsTelnetDeleteClicked = FALSE;

	if(pbtn !=NULL)
	{
		if(bIsTelnetInstallClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}

void CUwinTelnet::OnRadioTelnetStart() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_START);
	bIsTelnetStartClicked = !bIsTelnetStartClicked;
	bIsTelnetStopClicked = FALSE;
	bIsTelnetInstallClicked = FALSE;
	bIsTelnetDeleteClicked = FALSE;

	if(pbtn !=NULL)
	{
		if(bIsTelnetStartClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}

void CUwinTelnet::OnRadioTelnetStop() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_TELNET_STOP);
	bIsTelnetStopClicked = !bIsTelnetStopClicked;
	bIsTelnetStartClicked = FALSE;
	bIsTelnetInstallClicked = FALSE;
	bIsTelnetDeleteClicked = FALSE;

	if(pbtn !=NULL)
	{
		if(bIsTelnetStopClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}
void CUwinTelnet::SetApply() 
{
	CButton *pBtn;
	setcheckapply(pBtn,IDC_RADIO_TELNET_INSTALL)
	setcheckapply(pBtn,IDC_RADIO_TELNET_DELETE)
	setcheckapply(pBtn,IDC_RADIO_TELNET_START)
	setcheckapply(pBtn,IDC_RADIO_TELNET_STOP)
	SetModified(FALSE);

}

void CUwinTelnet::OnChangeEditTelnetPort() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	SetModified(TRUE);
	
}
void CUwinTelnet::OnChangeEditMaxConns() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CPropertyPage::OnInitDialog()
	// function to send the EM_SETEVENTMASK message to the control
	// with the ENM_CHANGE flag ORed into the lParam mask.
	
	SetModified(TRUE);
	
}

void CUwinTelnet::OnSelendokComboTelnetStartup() 
{
	// TODO: Add your control notification handler code here
	SetModified(TRUE);
	
}
