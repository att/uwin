// UwinUms.cpp : implementation file
//

#include "stdafx.h"
#include "UwinUms.h"
#include "globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define setcheckapply(pBtn,id) pBtn = (CButton *) GetDlgItem(id);\
		ASSERT(pBtn !=NULL);\
		if( pBtn->GetCheck() )\
		{	SetModified(TRUE);	return;	}

/////////////////////////////////////////////////////////////////////////////
// CUwinUms property page

IMPLEMENT_DYNCREATE(CUwinUms, CPropertyPage)

CUwinUms::CUwinUms() : CPropertyPage(CUwinUms::IDD)
{
	bIsUmsStartClicked = FALSE;
	bIsUmsStopClicked = FALSE;
	bIsUmsInstallClicked = FALSE;
	bIsUmsDeleteClicked = FALSE;
	b_IsCheckModified = false;

	//{{AFX_DATA_INIT(CUwinUms)
	m_szServiceName = _T("");
	m_szState = _T("");
	m_szDependencies = _T("");
	m_szAccepted = _T("");
	//}}AFX_DATA_INIT
}

CUwinUms::~CUwinUms()
{
}

void CUwinUms::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinUms)
	DDX_Control(pDX, IDC_COMBO_UMS_STARTUP, m_ComboBox);
	DDX_Text(pDX, IDC_EDIT_UMS_SERVICE_NAME, m_szServiceName);
	DDX_Text(pDX, IDC_EDIT_UMS_STATE, m_szState);
	DDX_Text(pDX, IDC_EDIT_UMS_DEPENDENCIES, m_szDependencies);
	DDX_Text(pDX, IDC_EDIT_UMS_ACCEPTED, m_szAccepted);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinUms, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinUms)
	ON_BN_CLICKED(IDC_RADIO_UMS_DELETE, OnRadioUmsDelete)
	ON_BN_CLICKED(IDC_RADIO_UMS_INSTALL, OnRadioUmsInstall)
	ON_BN_CLICKED(IDC_RADIO_UMS_START, OnRadioUmsStart)
	ON_BN_CLICKED(IDC_RADIO_UMS_STOP, OnRadioUmsStop)
	ON_BN_CLICKED(IDC_CHECK_ENUMERATE, OnCheckEnumerate)
	ON_BN_CLICKED(IDC_CHECK_REUSE, OnCheckReuse)
	ON_BN_CLICKED(IDC_CHECK_SETUID, OnCheckSetuid)
	ON_BN_CLICKED(IDC_CHECK_PASSFLAG, OnCheckPassFlag)
	ON_CBN_SELENDOK(IDC_COMBO_UMS_STARTUP, OnSelendokComboUmsStartup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUwinUms message handlers



BOOL CUwinUms::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

	char dispname[1024],temp[1024],name[500];
	SERVICE_STATUS *stat;
	QUERY_SERVICE_CONFIG *config;
	CButton *pbtn;
	strcpy(name,"uwin_ms");
	stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS)+1);
	config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);

	if(!stat || !config)
		return 0;
	
	if(getservice_displayname(name,dispname))
		m_szServiceName.Format("%s",dispname);
	else
	{
		CWnd *tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		 tmp = GetDlgItem(IDC_RADIO_UMS_DELETE);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		 tmp = GetDlgItem(IDC_RADIO_UMS_START);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		return 0;
	}
		
	query_servicestatus("uwin_ms",stat);
	convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
	m_szState.Format("%s",temp);

	if(!stricmp("Running",temp))
	{
		/* disable install start buttons */
		CWnd *tmp= GetDlgItem(IDC_RADIO_UMS_START);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		tmp = GetDlgItem(IDC_RADIO_UMS_INSTALL);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);

	}
	else if(!stricmp("Stopped",temp))
	{
		/*disable install and stop */
		CWnd *tmp = GetDlgItem(IDC_RADIO_UMS_INSTALL);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
		tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
		SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
	}
	convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
	m_szAccepted.Format("%s",temp);
	query_service_config("uwin_ms",config);

	char *ptr = config->lpDependencies;
	char buffer[2048];
	int len=0;
	if(ptr)
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
	buffer[len] = 0;
	
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
	ums_regkeys(); //Creates Registry Entries if they does not exist

	if (get_config_ums(1))
	{
		pbtn = (CButton *)GetDlgItem(IDC_CHECK_SETUID);
		ASSERT(pbtn!=NULL);
		pbtn->SetCheck(1); 
	}
	if (get_config_ums(2))
	{
		pbtn = (CButton *)GetDlgItem(IDC_CHECK_ENUMERATE);
		ASSERT(pbtn!=NULL);
		pbtn->SetCheck(1); 
	}
	if (get_config_ums(3))
	{
		pbtn = (CButton *)GetDlgItem(IDC_CHECK_REUSE);
		ASSERT(pbtn!=NULL);
		pbtn->SetCheck(1); 
	}
	if (get_config_ums(4))
	{
		pbtn = (CButton *)GetDlgItem(IDC_CHECK_PASSFLAG);
		ASSERT(pbtn!=NULL);
		pbtn->SetCheck(1); 
	}
	
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CUwinUms::OnApply() 
{
	int iSelected,iRadioSelected; 
	CString szComboString;
	CButton *pbtn;
	iSelected = m_ComboBox.GetCurSel( );
	SERVICE_STATUS *stat;
	QUERY_SERVICE_CONFIG *config;
	char temp[500];
	stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS)+1);
	config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);
			
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
		config->lpDisplayName = NULL;
		if(!stricmp("Auto Start",szComboString))
		{
			config->dwStartType = SERVICE_AUTO_START;
		}
		else
		 if(!stricmp("Demand Start",szComboString))
			config->dwStartType=  SERVICE_DEMAND_START;
		 else
			if(!stricmp("Disabled",szComboString))
			 config->dwStartType= SERVICE_DISABLED;
		else
			config->dwStartType = SERVICE_AUTO_START;
			
		set_service_config("uwin_ms",config);

	}
	/////////////////
	pbtn = (CButton *)GetDlgItem(IDC_CHECK_SETUID);
	ASSERT(pbtn!=NULL);
	
	if( pbtn->GetCheck())
		set_config_ums(1,1); //setuid enabled
	else
		set_config_ums(0,1);
	
	pbtn = (CButton *)GetDlgItem(IDC_CHECK_ENUMERATE);
	ASSERT(pbtn!=NULL);
	
	if( pbtn->GetCheck()) // enumerate checked
		set_config_ums(1,2);
	else
		set_config_ums(0,2);
	
	pbtn = (CButton *)GetDlgItem(IDC_CHECK_REUSE);
	ASSERT(pbtn!=NULL);
	
	if( pbtn->GetCheck()) // reuse password and group checked
		set_config_ums(1,3);
	else
		set_config_ums(0,3);

	pbtn = (CButton *)GetDlgItem(IDC_CHECK_PASSFLAG);
	ASSERT(pbtn!=NULL);
	
	if( pbtn->GetCheck()) // generate local password file
		set_config_ums(1,4);
	else
		set_config_ums(0,4);

	/////////////////

	//
	// Selected Radio button indicates any of the controls install, start, stop, delete.
	//
	iRadioSelected = GetRadioSelected();
	CWnd *tmp;
	
	switch(iRadioSelected)
	{
		case UMSINSTALL :
			if(!installservice(UWIN_MASTER, NULL))
				break;

			pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_INSTALL);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 

			tmp = GetDlgItem(IDC_RADIO_UMS_INSTALL);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			
			tmp = GetDlgItem(IDC_RADIO_UMS_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			tmp = GetDlgItem(IDC_RADIO_UMS_DELETE);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);

			convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
			m_szAccepted.Format("%s",temp);
		    OnInitDialog();
			break;
	case UMSDELETE:
			if(!deleteservice("uwin_ms"))
				break;
			Sleep(500);
			
			m_szState.Format("%s","");
			m_szAccepted.Format("%s","");
			m_szDependencies.Format("%s","");
			m_ComboBox.ResetContent();
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_DELETE);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 
			tmp = GetDlgItem(IDC_RADIO_UMS_INSTALL);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_UMS_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_UMS_DELETE);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			break;
	
	case UMSSTART:
			
			if(!startservice("uwin_ms"))
				break;
			Sleep(500);
			query_servicestatus("uwin_ms",stat);
			convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
			m_szState.Format("%s",temp);
			convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
			m_szAccepted.Format("%s",temp);

			pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_START);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 
			
			tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_UMS_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			UpdateData(FALSE);
			break;

	case UMSSTOP:
			if (!stopservice("uwin_ms"))
				break;
			Sleep(500); // Give some time for Service control Manager to stop the service
			query_servicestatus("uwin_ms",stat);
			convert_state_to_string(temp,stat->dwCurrentState,STATE_SERVICE);
			m_szState.Format("%s",temp);
			
			pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_STOP);
			ASSERT(pbtn!=NULL);
			pbtn->SetCheck(0); 
		
			tmp = GetDlgItem(IDC_RADIO_UMS_START);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)&~WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
	
			tmp = GetDlgItem(IDC_RADIO_UMS_STOP);
			SetWindowLong(tmp->m_hWnd,GWL_STYLE,GetWindowLong(tmp->m_hWnd,GWL_STYLE)|WS_DISABLED);
			tmp->InvalidateRect(NULL);
			::SendMessage(tmp->m_hWnd, WM_PAINT, NULL, NULL);
			UpdateData(FALSE);
			break;
	}

	free(stat);
	return CPropertyPage::OnApply();
}

int CUwinUms::GetRadioSelected()
{
	CButton *pbtn;
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_INSTALL);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return UMSINSTALL;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_DELETE);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return UMSDELETE;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_START);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return UMSSTART;
	}
	pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_STOP);
	if(pbtn !=NULL)
	{
		if( pbtn->GetCheck())
			return UMSSTOP;
	}
	return OPTIONERROR;
}

void CUwinUms::OnRadioUmsDelete() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_DELETE);
	bIsUmsDeleteClicked = !bIsUmsDeleteClicked;
	bIsUmsStartClicked = FALSE;
	bIsUmsStopClicked = FALSE;
	bIsUmsInstallClicked = FALSE;
	if(pbtn !=NULL)
	{
		if(bIsUmsDeleteClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}

void CUwinUms::OnRadioUmsInstall() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_INSTALL);
	bIsUmsInstallClicked = !bIsUmsInstallClicked;
	bIsUmsStartClicked = FALSE;
	bIsUmsStopClicked = FALSE;
	bIsUmsDeleteClicked = FALSE;
	if(pbtn !=NULL)
	{
		if(bIsUmsInstallClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}

void CUwinUms::OnRadioUmsStart() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_START);
	bIsUmsStartClicked = !bIsUmsStartClicked;
	bIsUmsStopClicked = FALSE;
	bIsUmsInstallClicked = FALSE;
	bIsUmsDeleteClicked = FALSE;
	if(pbtn !=NULL)
	{
		if(bIsUmsStartClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
	
}

void CUwinUms::OnRadioUmsStop() 
{
	
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_UMS_STOP);
	bIsUmsStopClicked = !bIsUmsStopClicked;
	bIsUmsStartClicked = FALSE;
	bIsUmsInstallClicked = FALSE;
	bIsUmsDeleteClicked = FALSE;
	if(pbtn !=NULL)
	{
		if(bIsUmsStopClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetApply();
}

void CUwinUms::OnCheckEnumerate() 
{
		
	b_IsCheckModified = true;
	SetApply();		
}

void CUwinUms::OnCheckReuse() 
{
	b_IsCheckModified = true;
	SetApply();
}

void CUwinUms::OnCheckSetuid() 
{
	b_IsCheckModified = true;
	SetApply();
}

void CUwinUms::SetApply() 
{
	CButton *pBtn;
	setcheckapply(pBtn,IDC_RADIO_UMS_INSTALL)
	setcheckapply(pBtn,IDC_RADIO_UMS_DELETE)
	setcheckapply(pBtn,IDC_RADIO_UMS_START)
	setcheckapply(pBtn,IDC_RADIO_UMS_STOP)
	if(b_IsCheckModified == true)
	{
		SetModified(TRUE);
		return;
	}
	SetModified(FALSE);
}

void CUwinUms::OnSelendokComboUmsStartup() 
{
	SetModified(TRUE);	
}

void CUwinUms::OnOK() 
{
	CPropertyPage::OnOK();
}

void CUwinUms::OnCheckPassFlag()
{
	b_IsCheckModified = true;
	SetApply();
}
