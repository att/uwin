// UwinClient.cpp : implementation file
//

#include "stdafx.h"
#include "UwinClient.h"
#include "UcsInstall.h"
#include "globals.h"
#include <lm.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CUwinClient property page

IMPLEMENT_DYNCREATE(CUwinClient, CPropertyPage)
int g_Index;
CUwinClient::CUwinClient() : CPropertyPage(CUwinClient::IDD)
{
	bIsRadioClicked =FALSE;

	//{{AFX_DATA_INIT(CUwinClient)
	m_szAccepted = _T("");
	m_szDependencies = _T("");
	m_szServiceName = _T("");
	//}}AFX_DATA_INIT
}

CUwinClient::~CUwinClient()
{
}

void CUwinClient::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinClient)
	DDX_Control(pDX, IDC_LIST_CLIENTLIST, m_ClientList);
	DDX_Text(pDX, IDC_EDIT_CLIENT_ACCEPTED_CONTROLS, m_szAccepted);
	DDX_Text(pDX, IDC_EDIT_CLIENT_DEPENDENCIES, m_szDependencies);
	DDX_Text(pDX, IDC_EDIT_CLIENT_SERVICE, m_szServiceName);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinClient, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinClient)
	ON_BN_CLICKED(IDC_BUTTON_CLIENT_INSTALL_WIZARD, OnButtonInstall)
	ON_BN_CLICKED(IDC_RADIO_CLIENT_DELETE, OnRadioDelete)
	ON_NOTIFY(NM_CLICK, IDC_LIST_CLIENTLIST, OnClickListClientlist)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_CLIENTLIST, OnItemchangedListClientlist)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUwinClient message handlers

BOOL CUwinClient::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	CString szSelectedItem;
	int iSelectedItem;
	RECT rect;
	m_ClientList.GetClientRect(&rect);
	g_Index = m_ClientList.InsertColumn(1,"hello",LVCFMT_LEFT,(rect.right)*3/4,1);
	LV_ITEM ListItem;
	ListItem.mask = LVIF_TEXT ;
	int iIndex = 0;
		m_szServiceName.Format("%s","ServiceName");
	m_szAccepted.Format("%s","Accepted");
	m_szDependencies.Format("%s","Dependencies");
	
	// Fill up the list box with the m_ClientList.InsertItem
	
	ucs_services("ucs");
	int ItemCount = m_ClientList.GetItemCount();
	m_ClientList.RedrawItems(0,ItemCount);
	m_ClientList.UpdateWindow();
	iSelectedItem = m_ClientList.GetNextItem( -1,LVNI_SELECTED );	

	if(iSelectedItem == 1)
	{
		QUERY_SERVICE_CONFIG *config;
		config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);
	
		szSelectedItem = m_ClientList.GetItemText(iSelectedItem,0);
		m_szServiceName.Format("%s","Service Name");
		query_service_config((char *)LPCSTR(szSelectedItem),config);
		m_szAccepted.Format("%s","Accepted");
		m_szDependencies.Format("%s","Dependencies");
		if(config)
			free(config);
	}
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CUwinClient::OnButtonInstall() 
{
	CUcsInstall  uwinucs;
	uwinucs.DoModal();
	m_ClientList.DeleteAllItems();
	ucs_services("ucs");
}

void CUwinClient::OnRadioDelete() 
{
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_CLIENT_DELETE);
	bIsRadioClicked = !bIsRadioClicked;
	if(pbtn !=NULL)
	{	
		if(bIsRadioClicked)
			pbtn->SetCheck(1);
		else
			pbtn->SetCheck(0);
	}
	SetModified(bIsRadioClicked);
}

BOOL CUwinClient::OnApply() 
{	
	CString szSelectedItem;
	int iSelectedItem,i;
	iSelectedItem = m_ClientList.GetNextItem( -1,LVNI_SELECTED );
	char servname[256];

	while( (iSelectedItem > -1)&&(bIsRadioClicked) )
	{
		szSelectedItem = m_ClientList.GetItemText(iSelectedItem,0);
		
		strcpy(servname,"UWIN_CS");
		strcat(servname,(char *)LPCTSTR(szSelectedItem));
		for(i=0;i<(int)strlen(servname);i++)
		if(servname[i] == '/')
			servname[i]= '#';

		if (deleteservice(servname))
			m_ClientList.DeleteItem(iSelectedItem);
		else
		{
			// Remove the selection of the item to prevent infinite loop!
			UINT mask;

			mask = m_ClientList.GetItemState(iSelectedItem, LVIS_SELECTED);
			mask &= ~LVIS_SELECTED;
			m_ClientList.SetItemState(iSelectedItem, mask, LVIS_SELECTED);
		}
		iSelectedItem = m_ClientList.GetNextItem( -1,LVNI_SELECTED );
	}
	CButton *pbtn = (CButton *)GetDlgItem(IDC_RADIO_CLIENT_DELETE);
	if(pbtn !=NULL)
		pbtn->SetCheck(0);
	
	return CPropertyPage::OnApply();
}


void CUwinClient::OnClickListClientlist(NMLISTVIEW* pNMHDR, LRESULT* pResult) 
{
	CString szSelectedItem;
	*pResult = 0;
}

int CUwinClient::ucs_services(char *servname)
{
	BOOL ret;
	SC_HANDLE scm;
	int fail=0;
	ENUM_SERVICE_STATUS *statusbuff;
	DWORD bytesrequired, totalentries, Rshandle=0;
	LV_ITEM ListItem;
	ListItem.mask = LVIF_TEXT ;
	int iIndex = 0,iCount;
	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (scm)
	{
		ret = EnumServicesStatus(scm, SERVICE_WIN32, SERVICE_STATE_ALL, 0, 0, &bytesrequired, &totalentries, &Rshandle);
		
		statusbuff = (ENUM_SERVICE_STATUS *)malloc(bytesrequired+1);
		
		if(!statusbuff)
		{
			CloseServiceHandle(scm);
			return 0;	
		}
		
		Rshandle =0;
		totalentries=0;

		ret = EnumServicesStatus(scm, SERVICE_WIN32, SERVICE_STATE_ALL,statusbuff, bytesrequired, &bytesrequired, &totalentries, &Rshandle);
		if(!ret)
		{
			ErrorBox("Cannot get service status", servname, GetLastError());
			CloseServiceHandle(scm);
			return 0;
		}
		for(iCount = 0;iCount <(int)totalentries;iCount++)
			{
			if(strspn("UWIN_CS",statusbuff[iCount].lpServiceName)==7)
			{
				char *ptr,buffer[500];
								
				ListItem.iItem = iIndex;
				ListItem.iSubItem = g_Index;
				ListItem.pszText=statusbuff[iCount].lpServiceName;
				ptr = strchr(ListItem.pszText,(int)'S');
				ptr++;
				strcpy(buffer,ptr);
				ptr=NULL;
				ptr = strchr(buffer,(int)'#');
					if(ptr)
						*ptr = (int)'/';
					strcpy(ListItem.pszText,buffer);

				iIndex = m_ClientList.InsertItem(&ListItem);
				iIndex++;
			}
		}
		CloseServiceHandle(scm);
		return 1;
	}
	return 0;
}

void CUwinClient::OnItemchangedListClientlist(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	CString szSelectedItem ;

	if((pNMListView->iItem >-1)&& (pNMListView->uNewState& LVIS_SELECTED)) 
	{
		szSelectedItem = m_ClientList.GetItemText(pNMListView->iItem,0);
		QUERY_SERVICE_CONFIG *config;
		SERVICE_STATUS *stat;
		char temp[500],servname[500];
		config = (QUERY_SERVICE_CONFIG *)malloc(sizeof(QUERY_SERVICE_CONFIG)+4096);
		stat = (SERVICE_STATUS *) malloc(sizeof(SERVICE_STATUS)+1);
		strcpy(servname,"UWIN_CS");
		strcat(servname,LPCSTR(szSelectedItem));
		char *pt = strchr(servname,(int)'/');
			if(pt)
				*pt= (int)'#';

		getservice_displayname(servname,temp);
		m_szServiceName.Format("%s",servname);
		query_servicestatus(servname,stat);
		query_service_config(servname,config);
		convert_state_to_string(temp,stat->dwControlsAccepted,CONTROL_SERVICE);
		m_szAccepted.Format("%s","Start, Stop");
		
		
		char *ptr = config->lpDependencies;
		char buffer[2048];
		int len=0;
		strcpy(buffer,"");
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

		if(config)
			free(config);
		if(stat)
			free(stat);

		UpdateData(FALSE);
	}
	*pResult = 0;
}
