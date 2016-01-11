// UwinInetd.cpp : implementation file
//

#include "stdafx.h"
#include "testcpl.h"
#include "UwinInetd.h"
#include "SortClass.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CUwinInetd property page

IMPLEMENT_DYNCREATE(CUwinInetd, CPropertyPage)

CUwinInetd::CUwinInetd() : CPropertyPage(CUwinInetd::IDD)
{
	//{{AFX_DATA_INIT(CUwinInetd)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	change_flag = 0;
}

CUwinInetd::~CUwinInetd()
{
}

void CUwinInetd::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinInetd)
	DDX_Control(pDX, IDC_LIST_SERVICES, m_ListDisplayControl);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinInetd, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinInetd)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_SERVICES, OnDblclkListServices)
	ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST_SERVICES, OnColumnclickListServices)
	ON_BN_CLICKED(IDC_BUTTON_ADD, OnButtonAdd)
	ON_BN_CLICKED(IDC_BUTTON_CONFIGURE, OnButtonConfigure)
	ON_BN_CLICKED(IDC_BUTTON_DELETE, OnButtonDelete)
	ON_BN_CLICKED(IDC_BUTTON_DISABLE, OnButtonDisable)
	ON_BN_CLICKED(IDC_BUTTON_ENABLE, OnButtonEnable)
	ON_BN_CLICKED(IDC_BUTTON_REFRESH, OnButtonRefresh)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUwinInetd message handlers

BOOL CUwinInetd::OnApply() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	// Set inetdValues array
	GetListValues();

	// Set registry values
	regOps.SetInetdVal(inetdValues);
	
	if(change_flag)
	{
		// Write to inetd.conf.new and services.new files; call script to do it
		InvokeScript();
	}

	SetModified(FALSE);
	change_flag = 0;
	
	return CPropertyPage::OnApply();
}

BOOL CUwinInetd::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// TODO: Add extra initialization here

	// Add columns to list control
	AddColumnsToList();

	// Get information from inetd.conf file and add items to list control
	AddListControlItems(CREATE);
	
	// Add the list of item from inetd.conf file
	
	nSortedCol = 0;
	bSortAscending = FALSE;
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// This function adds appropriate columns to the list control 
void CUwinInetd::AddColumnsToList()
{
	LVCOLUMN lvColumn;
	char colName[256], msg[1024];

	*msg = 0;

	// Add 'Service Name' column
	lvColumn.mask = LVCF_FMT|LVCF_ORDER|LVCF_TEXT|LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 100;
	strcpy(colName, "Service Name");
	lvColumn.pszText = colName;
	lvColumn.iOrder = 0;
	if(m_ListDisplayControl.InsertColumn(0, &lvColumn) == -1)
		strcat(msg, "Could not insert \"Service Name\" Column to list control\n");
	m_arrColType[0] = STR_TYPE;


	// Add 'Port' column
	lvColumn.mask = LVCF_FMT|LVCF_ORDER|LVCF_TEXT|LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 45;
	strcpy(colName, "Port");
	lvColumn.pszText = colName;
	lvColumn.iOrder = 1;
	if(m_ListDisplayControl.InsertColumn(1, &lvColumn) == -1)
		strcat(msg, "Could not insert \"Port\" Column to list control\n");
	m_arrColType[1] = INT_TYPE;
	
	// Add 'Status' column
	lvColumn.mask = LVCF_FMT|LVCF_ORDER|LVCF_TEXT|LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 60;
	strcpy(colName, "Status");
	lvColumn.pszText = colName;
	lvColumn.iOrder = 2;
	if(m_ListDisplayControl.InsertColumn(2, &lvColumn) == -1)
		strcat(msg, "Could not insert \"Status\" Column to list control\n");
	m_arrColType[2] = STR_TYPE;

	if(*msg)
		::MessageBox(NULL, msg, "UWIN Inetd", MB_OK|MB_ICONEXCLAMATION);
}

void CUwinInetd::AddListControlItems(int flag)
{
	LVITEM lvItem;
	char itemVal[256], msg[1024];
	DWORD inetdVal[8];

	// Get inetd values from registry
	if(!regOps.GetInetdVal(inetdVal, REGISTRY))
	{
		change_flag = 1;
		SetModified(TRUE);
	}

	*msg = 0;

	lvItem.mask = LVIF_TEXT|LVIF_STATE;
	lvItem.state = 0;
	lvItem.stateMask = LVIS_SELECTED;


	do
	{
		// Set 'Service' item for ftp
		lvItem.iItem = 0; // Row number
		lvItem.mask |= LVIF_PARAM;
		lvItem.lParam = 0;
		lvItem.iSubItem = 0; // Column number
		sprintf(itemVal, "ftp");
		lvItem.pszText = itemVal;
		if(flag == CREATE)
		{
			if(m_ListDisplayControl.InsertItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}
		else
		{
			if(m_ListDisplayControl.SetItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}

		// Set 'Port' items for ftp
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 1; // Column number
		sprintf(itemVal, "%d", inetdVal[1]);
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}

		// Set 'Status' items for ftp
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 2; // Column number
		switch(inetdVal[0])
		{
			case 0:
				sprintf(itemVal, "Disabled");
				break;
				
			case 1:
				sprintf(itemVal, "Enabled");
				break;
		}
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}

		// Set 'Service' item for rlogin
		lvItem.iItem = 1; // Row number
		lvItem.mask |= LVIF_PARAM;
		lvItem.lParam = 0;
		lvItem.iSubItem = 0; // Column number
		sprintf(itemVal, "rlogin");
		lvItem.pszText = itemVal;
		if(flag == CREATE)
		{
			if(m_ListDisplayControl.InsertItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}
		else
		{
			if(m_ListDisplayControl.SetItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}

		// Set 'Port' items for rlogin
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 1; // Column number
		sprintf(itemVal, "%d", inetdVal[3]);
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item");
			break;
		}

		// Set 'Status' items for rlogin
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 2; // Column number
		switch(inetdVal[2])
		{
			case 0:
				sprintf(itemVal, "Disabled");
				break;
				
			case 1:
				sprintf(itemVal, "Enabled");
				break;
		}
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item");
			break;
		}


		// Set 'Service' item for rsh
		lvItem.iItem = 2; // Row number
		lvItem.mask |= LVIF_PARAM;
		lvItem.lParam = 0;
		lvItem.iSubItem = 0; // Column number
		sprintf(itemVal, "rsh");
		lvItem.pszText = itemVal;
		if(flag == CREATE)
		{
			if(m_ListDisplayControl.InsertItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}
		else
		{
			if(m_ListDisplayControl.SetItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}

		// Set 'Port' items for rsh
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 1; // Column number
		sprintf(itemVal, "%d", inetdVal[5]);
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}

		// Set 'Status' items for rsh
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 2; // Column number
		switch(inetdVal[4])
		{
			case 0:
				sprintf(itemVal, "Disabled");
				break;
				
			case 1:
				sprintf(itemVal, "Enabled");
				break;
		}
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}

		// Set 'Service' item for telnet
		lvItem.iItem = 3; // Row number
		lvItem.mask |= LVIF_PARAM;
		lvItem.lParam = 0;
		lvItem.iSubItem = 0; // Column number
		sprintf(itemVal, "telnet");
		lvItem.pszText = itemVal;
		if(flag == CREATE)
		{
			if(m_ListDisplayControl.InsertItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}
		else
		{
			if(m_ListDisplayControl.SetItem(&lvItem) == -1)
			{
				sprintf(msg, "Could not insert item #%d");
				break;
			}
		}

		// Set 'Port' items for telnet
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 1; // Column number
		sprintf(itemVal, "%d", inetdVal[7]);
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}

		// Set 'Status' items for telnet
		lvItem.mask &= ~LVIF_PARAM;
		lvItem.iSubItem = 2; // Column number
		switch(inetdVal[6])
		{
			case 0:
				sprintf(itemVal, "Disabled");
				break;
				
			case 1:
				sprintf(itemVal, "Enabled");
				break;
		}
		lvItem.pszText = itemVal;
		if(m_ListDisplayControl.SetItem(&lvItem) == -1)
		{
			sprintf(msg, "Could not insert item #%d");
			break;
		}
	}
	while(0);

	if(*msg)
		::MessageBox(NULL, msg, "UWIN Inetd", MB_OK|MB_ICONEXCLAMATION);
}

void CUwinInetd::GetListValues()
{
	int itemPos = -1;
	LVITEM lvItem;
	char servName[256], port[64], status[64];

	do
	{
		if((itemPos = m_ListDisplayControl.GetNextItem(itemPos, LVNI_ALL)) == -1)
			return;

		// Retrieve details of the item selected
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = itemPos;
		lvItem.iSubItem = 0;
		lvItem.pszText = servName;
		lvItem.cchTextMax = sizeof(servName);
		m_ListDisplayControl.GetItem(&lvItem);

		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = itemPos;
		lvItem.iSubItem = 1;
		lvItem.pszText = port;
		lvItem.cchTextMax = sizeof(port);
		m_ListDisplayControl.GetItem(&lvItem);

		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = itemPos;
		lvItem.iSubItem = 2;
		lvItem.pszText = status;
		lvItem.cchTextMax = sizeof(status);
		m_ListDisplayControl.GetItem(&lvItem);
		
		SetInetdValues(servName, port, status);
	}
	while(1);
}

void CUwinInetd::SetInetdValues(char *servName, char *port, char *status)
{
	if(!strcmp(servName, "ftp"))
	{
		inetdValues[0] = ((strcmp(status, "Enabled") == 0) ? 1 : 0);
		inetdValues[1] = atoi(port);
	}
	else if(!strcmp(servName, "rlogin"))
	{
		inetdValues[2] = ((strcmp(status, "Enabled") == 0) ? 1 : 0);
		inetdValues[3] = atoi(port);
	}
	else if(!strcmp(servName, "rsh"))
	{
		inetdValues[4] = ((strcmp(status, "Enabled") == 0) ? 1 : 0);
		inetdValues[5] = atoi(port);
	}
	else if(!strcmp(servName, "telnet"))
	{
		inetdValues[6] = ((strcmp(status, "Enabled") == 0) ? 1 : 0);
		inetdValues[7] = atoi(port);
	}
}

void CUwinInetd::OnDblclkListServices(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here

	ConfigureItem();

	*pResult = 0;
}

void CUwinInetd::ConfigureItem()
{
	int itemPos = 0;
	LVITEM lvItem;
	char lvItemVal[256];

	if((itemPos = m_ListDisplayControl.GetNextItem(-1, LVNI_SELECTED)) == -1)
		return;

	// Retrieve details of the item selected
	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = itemPos;
	lvItem.iSubItem = 0;
	lvItem.pszText = lvItemVal;
	lvItem.cchTextMax = sizeof(lvItemVal);
	m_ListDisplayControl.GetItem(&lvItem);

	confDlg.SetServerName(lvItemVal);

	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = itemPos;
	lvItem.iSubItem = 1;
	lvItem.pszText = lvItemVal;
	lvItem.cchTextMax = sizeof(lvItemVal);
	m_ListDisplayControl.GetItem(&lvItem);

	confDlg.SetPortNumber(atoi(lvItemVal));

	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = itemPos;
	lvItem.iSubItem = 2;
	lvItem.pszText = lvItemVal;
	lvItem.cchTextMax = sizeof(lvItemVal);
	m_ListDisplayControl.GetItem(&lvItem);
	confDlg.SetEnable((strcmp(lvItemVal, "Enabled") == 0) ? 1 : 0);

	if(confDlg.DoModal() == IDOK)
	{
		DWORD port, enable;

		// Refresh List control with modified values
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = itemPos;

		lvItem.iSubItem = 1;
		confDlg.GetPortNumber(&port);
		sprintf(lvItemVal, "%d", port);
		lvItem.pszText = lvItemVal;
		lvItem.cchTextMax = sizeof(lvItemVal);
		m_ListDisplayControl.SetItem(&lvItem);

		lvItem.iSubItem = 2;
		confDlg.GetEnable(&enable);
		sprintf(lvItemVal, "%s", ((enable==1)?"Enabled":"Disabled"));
		lvItem.pszText = lvItemVal;
		lvItem.cchTextMax = sizeof(lvItemVal);
		m_ListDisplayControl.SetItem(&lvItem);

		change_flag = 1;
		SetModified(TRUE);
	}
}

void CUwinInetd::InvokeScript()
{
	BOOL ret;
	PROCESS_INFORMATION procinfo;
	STARTUPINFO strinfo;
	char* root;
	char shellpath[256];
	
	//Get path for shell and script
	if(!(root = regOps.GetRoot()))
		return;

	sprintf(shellpath, "%s\\usr\\bin\\ksh.exe", root);

	ZeroMemory(&strinfo, sizeof(strinfo));
	strinfo.cb = sizeof(strinfo);
	strinfo.dwFlags = STARTF_USESHOWWINDOW;
	strinfo.wShowWindow = SW_HIDE;

	ret = CreateProcess(shellpath, "-c /etc/inetdconfig.sh", NULL, NULL, FALSE, CREATE_NEW_CONSOLE|NORMAL_PRIORITY_CLASS, NULL, NULL, &strinfo, &procinfo);

	// Wait for script to execute
	WaitForSingleObject(procinfo.hProcess, INFINITE);
}

void CUwinInetd::OnColumnclickListServices(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	HD_NOTIFY *phdn = (HD_NOTIFY *)pNMHDR;
	
	if(phdn->iButton >= 0)
	{
		// User clicked on header using left mouse button
		if( phdn->iButton == nSortedCol )
			bSortAscending = !bSortAscending;
		else
			bSortAscending = TRUE;

		nSortedCol = phdn->iButton;		
		CMySortClass csc(&m_ListDisplayControl, nSortedCol);		

		csc.Sort(bSortAscending, m_arrColType[nSortedCol]);
	}
	
	*pResult = 0;
}

void CUwinInetd::OnButtonAdd() 
{
	// TODO: Add your control notification handler code here
}

void CUwinInetd::OnButtonConfigure() 
{
	// TODO: Add your control notification handler code here
	
	ConfigureItem();
}

void CUwinInetd::OnButtonDelete() 
{
	// TODO: Add your control notification handler code here
}

void CUwinInetd::OnButtonDisable() 
{
	// TODO: Add your control notification handler code here
}

void CUwinInetd::OnButtonEnable() 
{
	// TODO: Add your control notification handler code here
}

void CUwinInetd::OnButtonRefresh() 
{
	// TODO: Add your control notification handler code here
	
	AddListControlItems(REFRESH);
}
