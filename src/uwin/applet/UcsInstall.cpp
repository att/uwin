
// UcsInstall.cpp : implementation file
//

#include "stdafx.h"
#include "UcsInstall.h"
#include "UwinClient.h"
#if 0
#include <afxtempl.h>
#endif
#include "globals.h"
#include <lm.h>
#include <lmaccess.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
struct _TREEITEM {}; 


static char servname[256], servdname[256];
char domain[500];


static SERVICE_STATUS status;
static SERVICE_STATUS_HANDLE statushandle;
int check_ucs(char *usrname,int);
void get_domain(char *machine,char *domain);

/////////////////////////////////////////////////////////////////////////////
// CUcsInstall dialog


HMODULE hpNetAPI32;

NET_API_STATUS (WINAPI *ptrNetUserEnum)(LPCWSTR, DWORD, DWORD, LPBYTE*, DWORD, LPDWORD, LPDWORD, LPDWORD);

NET_API_STATUS (WINAPI *ptrNetApiBufferFree)(LPVOID);

NET_API_STATUS (WINAPI *ptrNetWkstaGetInfo)(LPWSTR, DWORD, LPBYTE*);

struct threadparam 
	{
		HTREEITEM hItem;
		CTreeCtrl   *pTreeCtrl;
	} threadstruct;

BOOL InstallCanceled = FALSE;
CUcsInstall::CUcsInstall(CWnd* pParent /*=NULL*/)
	: CDialog(CUcsInstall::IDD, pParent)
{
	g_bHasDomainExpanded = false;
	//{{AFX_DATA_INIT(CUcsInstall)
	m_szPassword = _T("");
	//}}AFX_DATA_INIT
}


void CUcsInstall::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUcsInstall)
	DDX_Control(pDX, IDC_TREE1, m_TreeCtrl);
	DDX_Text(pDX, IDC_EDIT_PASSWORD, m_szPassword);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUcsInstall, CDialog)
	//{{AFX_MSG_MAP(CUcsInstall)
	ON_BN_CLICKED(IDC_BUTTON_UCS_INSTALL, OnButtonUcsInstall)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_TREE1, OnItemexpandingTree1)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUcsInstall message handlers

void CUcsInstall::OnButtonUcsInstall() 
{
	// Add code to install UCS here.
	CString  szTreeItem;
	HTREEITEM tmpTreeItem,tmpParentItem;
	TV_ITEM parentTVItem;
	parentTVItem.mask =  TVIF_HANDLE |TVIF_TEXT ;
	parentTVItem.pszText = (char *)malloc(100);
	parentTVItem.cchTextMax = 99;
		
	UpdateData();
	tmpTreeItem = m_TreeCtrl.GetSelectedItem( );
	szTreeItem = m_TreeCtrl.GetItemText(tmpTreeItem);
	tmpParentItem = m_TreeCtrl.GetParentItem(tmpTreeItem);
	parentTVItem.hItem = tmpParentItem;
	m_TreeCtrl.GetItem(&parentTVItem);

	if(!*szTreeItem)
	::MessageBox(NULL,"Invalid Selection",servdname,MB_OK);
	else
	{
		int successflag = 1;
		if(stricmp(parentTVItem.pszText,domain) ==0)
		{
			char domclient[1024];
				
			strcpy(domclient,domain);
			strcat(domclient,"/");
			strcat(domclient,(char *)LPCSTR(szTreeItem));
			if(!installucs(domclient,(char *)LPCSTR(m_szPassword)))
				successflag = 0;
		}
		else
		if(!installucs((char *)LPCSTR(szTreeItem),(char *)LPCSTR(m_szPassword)))
			successflag = 0;
		if(successflag)
			m_TreeCtrl.DeleteItem(tmpTreeItem);
	}
    m_szPassword.Empty();
	UpdateData(FALSE);
	
}

int check_ucs(char *usrname,int flag)
{
	SC_HANDLE scm, service;
	char servname[500];

	if((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		::MessageBox(NULL,"Error: OpenSCManager failed",servname,MB_OK);
		return(0);
	}
	strcpy(servname,"uwin_cs");
	if(flag)
	{
		strcat(servname,domain);
		strcat(servname,"#");
	}
	strcat(servname,usrname);
	if((service = OpenService(scm,servname,SERVICE_STOP|SERVICE_QUERY_STATUS))==NULL)
	{
		CloseServiceHandle(scm);
		return(0);
	}
	CloseServiceHandle(scm);
	CloseServiceHandle(service);
	return 1;

}

BOOL CUcsInstall::OnInitDialog() 
{
	CDialog::OnInitDialog();
	//HTREEITEM localtree;
	DWORD siz;
	bool b_TreeHasDomain = true;
	char compname[80];

	siz=sizeof(compname);

	if(!(hpNetAPI32 = LoadLibrary("netapi32.dll")))
	{
		char buff[80];
		sprintf(buff,"Loading of NetApi32.dll failed, Error=%d", GetLastError() );
		AfxMessageBox( buff);

	}
	else
	{
		ptrNetUserEnum = (NET_API_STATUS (WINAPI *)(LPCWSTR, DWORD, DWORD, LPBYTE*, DWORD, LPDWORD, LPDWORD, LPDWORD))GetProcAddress(hpNetAPI32, "NetUserEnum");
		if(!ptrNetUserEnum)
		{
			char buff[80];
			sprintf(buff,"GetProcAddress failed(NetUserEnum), Error=%ld",GetLastError());
			AfxMessageBox( buff);
		}

		ptrNetApiBufferFree = (NET_API_STATUS (WINAPI *)(LPVOID))GetProcAddress(hpNetAPI32, "NetApiBufferFree");
		if(!ptrNetApiBufferFree)
		{
			char buff[80];
			sprintf(buff,"GetProcAddress failed(NetApiBufferFree), Error=%ld",GetLastError());
			AfxMessageBox( buff);
		}

		ptrNetWkstaGetInfo = (NET_API_STATUS (WINAPI *)(LPWSTR, DWORD, LPBYTE*))GetProcAddress(hpNetAPI32, "NetWkstaGetInfo");
		if(!ptrNetWkstaGetInfo)
		{
			char buff[80];
			sprintf(buff,"GetProcAddress failed(NetWkstaGetInfo), Error=%ld",GetLastError());
			AfxMessageBox( buff);
		}

	}

	GetComputerName(compname,&siz);
	get_domain(compname,domain);


	if(domain && strcmp(domain,"WORKGROUP"))
		b_TreeHasDomain = true;
	else
		b_TreeHasDomain = false;


	InstallCanceled = FALSE;
	HTREEITEM localname = m_TreeCtrl.InsertItem(compname);	
	HTREEITEM localNode = m_TreeCtrl.InsertItem("Local",localname);	
	
	char usrname[256];
	USER_INFO_0 *user_info;
	int i;
	DWORD entriesread,totalentries, Rshandle,size=sizeof(usrname);
	NET_API_STATUS ret;
					
		Rshandle=0;

loop:

		if(ptrNetUserEnum)
			ret = (*ptrNetUserEnum)(NULL,0, 0, (unsigned char **)&user_info,10024,&entriesread, &totalentries, &Rshandle);
		else
		{
			AfxMessageBox( "ptrNetUserEnum is NULL");
			ret = !NERR_Success;  
		}
		

		if(ret == NERR_Success)
		{
			for(i=0;i<(int)totalentries;i++)
			{
				size = sizeof(usrname);
				wcstombs(usrname,user_info[i].usri0_name,size);
				if(!check_ucs(usrname,0))
				{
					m_TreeCtrl.InsertItem(usrname,localNode );	
				}
			}
			if(ptrNetApiBufferFree)
				(*ptrNetApiBufferFree)(user_info);
		}
		else 
		{
			char buff[80];
			sprintf(buff,"NetUserEnum failed, Error=%ld",ret);
			AfxMessageBox( buff);
		} 

		if(ret == ERROR_MORE_DATA)
		{
			for(i=0;i<(int)entriesread;i++)

			{
				size = sizeof(usrname);
				wcstombs(usrname,user_info[i].usri0_name,size);
				if(!check_ucs(usrname,0))
				{
					m_TreeCtrl.InsertItem(usrname,localNode );	
				}
			}
			if(ptrNetApiBufferFree)
				(*ptrNetApiBufferFree)(user_info);
			goto loop;
		}

	
		if(b_TreeHasDomain)
		{
			HTREEITEM domaintree = m_TreeCtrl.InsertItem(domain,localname);	
			m_TreeCtrl.InsertItem("Loading Items..",domaintree);	

		}
		m_TreeCtrl.Expand(localname,TVE_EXPAND);
		UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void get_domain(char *machine,char *domain)
{
	wchar_t mbuffer[256];
	char domainname[256];
	WKSTA_INFO_100 *wks_info;
	int ret;

	
	if(machine)
	{
	 	mbstowcs(mbuffer,machine,sizeof(mbuffer));
		if(ptrNetWkstaGetInfo)
		{
			ret = (*ptrNetWkstaGetInfo)(mbuffer, 100, (unsigned char **)&wks_info);
			if(ret == NERR_Success)
			{
				wcstombs(domainname,(wchar_t *)LPCSTR(wks_info->wki100_langroup),256);
				strcpy(domain,domainname);
			}
			else
				AfxMessageBox("NetWkstaGetInfo failed ");
		}
		else
			AfxMessageBox("NetWkstaGetInfo call not initialized ");
	}
}

int  CUcsInstall::set_privileges()
{
	 // Grant Privileges Log on as user privilege.
		HANDLE hToken;
		TOKEN_PRIVILEGES *tkp;
		char buff[8045];
		int i;
		
		tkp = (TOKEN_PRIVILEGES*)buff;
		if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hToken))
		{
			return 0;
		}
		// Assigning privileges..
		LookupPrivilegeValue(NULL, SE_INCREASE_QUOTA_NAME, &tkp->Privileges[0].Luid);
		LookupPrivilegeValue(NULL, SE_CREATE_TOKEN_NAME, &tkp->Privileges[1].Luid);

		LookupPrivilegeValue(NULL, SE_TCB_NAME, &tkp->Privileges[2].Luid);
		LookupPrivilegeValue(NULL, SE_ASSIGNPRIMARYTOKEN_NAME, &tkp->Privileges[3].Luid);

		tkp->PrivilegeCount = 4;
		for(i=0;i<=3;i++)
			tkp->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
				
					
		if(!AdjustTokenPrivileges(hToken,FALSE,tkp,0,(PTOKEN_PRIVILEGES)NULL,0))
		{
			::MessageBox(NULL,"Unable to set Privileges","Error",MB_OK);
			return 0;
		}
		return 1;
}

FARPROC CUcsInstall:: getapi_addr(const char *lib, const char *sym)
{
	char sys_dir[512];
	FARPROC addr;
	HMODULE hp;
	int len;
	len = GetSystemDirectory(sys_dir, 512);
	sys_dir[len++] = '\\';
	strcpy(&sys_dir[len], lib);
	if(!(hp = LoadLibraryEx(sys_dir,0,0)))
	{
		::MessageBox(NULL,"LoadLibrary Failed","Error",MB_OK);
		return(0);
	}
	addr = GetProcAddress(hp,sym);
	FreeLibrary(hp);
	return(addr);
}

int CUcsInstall::installucs(char *account, char *pass)
{
	SC_HANDLE service, scm;
	TCHAR path[512], system[80], name[80], username[80];
	DWORD ret;
	char userpass[256], domuser[256], dom[256];
	int fail = 0, i;
	int  pid = -1;

	ret = getregpath(path);
	
	if (ret == 0)
	{
		::MessageBox(NULL,"UCS executable not found","Error",MB_OK);
		return(0);
	}
	strcat(path,"usr\\etc\\ucs.exe");
	if (!account)
	{
		::MessageBox(NULL,"Account Not Selected","Error",MB_OK);
		return 0;
		
	}
	else
		strcpy(name, account);

	strcpy(username, name);
	mangle(name, system);

	if(!pass)
		 return 0;         
	else 
		strcpy(userpass,pass);
	{
		
		int	sid[1024];
		unsigned long sidlen=1024, domlen=256; 
		typedef BOOL (WINAPI *logonfn) (LPSTR,LPSTR,LPSTR,DWORD,DWORD,PHANDLE);
		logonfn logonfunc;
		HANDLE atok;
		SID_NAME_USE puse;

		if(!LookupAccountName(	system, 
								name, 
								sid, 
								&sidlen, 
								dom, 
								&domlen, 
								&puse))
		{
			::MessageBox(NULL,"Invalid UserName",name,MB_OK);
			return(0);
		}
		else
		{
			strcpy(domuser, dom);
			strcat(domuser, "\\");
			strcat(domuser, name);
		}

		logonfunc=(logonfn)getapi_addr("advapi32.dll", "LogonUserA");
		if (!logonfunc)
			return(0);
		
		if (!((*logonfunc)(name, dom, userpass, 2, 0, &atok)))
		{ 
			::MessageBox(NULL,"Incorrect Password",name,MB_OK);					
				return(0);
		}
		CloseHandle(atok);
		//The user has entered correct UserName & Password
		//So, we proceed to install the Service in his/her name
	}
	sprintf(servname, "UWIN_CS%s", username);
	sprintf(servdname, "UWIN Client(%s)", username);
	for(i=0;i<(signed)strlen(servname);i++)
		if(servname[i] == '/')
			servname[i]= '#';
	
	scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (scm)
	{
		if((service=OpenService(scm,TEXT(servname),SERVICE_QUERY_STATUS)))
		{
			if(GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
			{
				//We Return if the Service is already installed
				CloseServiceHandle(scm);
				CloseServiceHandle(service);
				::MessageBox(NULL,"Service Already Exists",servdname,MB_OK);
				return(0);
			}
		}
		if (!set_privileges())
		{
			CloseServiceHandle(scm);
			CloseServiceHandle(service);
			::MessageBox(NULL,"Unable to SetPrivileges","Error",MB_OK);
			return 0;
		}
		
		service = CreateService(scm, 
								TEXT(servname), 
								TEXT(servdname), 
								SERVICE_ALL_ACCESS, 
								SERVICE_WIN32_OWN_PROCESS, 
								SERVICE_DEMAND_START, 
								SERVICE_ERROR_NORMAL, 
								path, 
								NULL, 
								NULL, 
								NULL, 
								TEXT(domuser), 
								userpass);

		if (service)
		{
			CloseServiceHandle(service);
			::MessageBox(NULL,"Service Installed",servdname,MB_OK);
			fail =1;
		} else
		{
			DWORD err;
			err = GetLastError();
			fail = 0;
		}

		CloseServiceHandle(scm);
	}
	else
	{
		fail = 0;
	}
	
	return(fail);
}

void CUcsInstall::mangle(char *name, char *system)
{
	NET_API_STATUS (WINAPI *ptrNetServerEnum)(LPCWSTR, DWORD, LPBYTE*, DWORD, LPDWORD, LPDWORD, DWORD, LPCWSTR, LPDWORD);
	int result;
	char username[80], *token; 
	NET_API_STATUS ret;
	DWORD totalread, totalentry;
	CUwinClient clientobj;
	
	token = strchr(name,'/');
	result = token - name;
	if(NULL != token) //Name is in "Domain/Username" format
	{
		char temp[80];
		WKSTA_INFO_100 *wks_info;
		SERVER_INFO_100 *serv_info;

		strcpy(system, name);
		system[result]='\0';
		if(ptrNetWkstaGetInfo)
			(*ptrNetWkstaGetInfo)(NULL, 100, (unsigned char **)&wks_info);
		wcstombs(temp,(wchar_t *) wks_info->wki100_langroup, 80);
		if(_stricmp(system, temp)!=0)//Domain name improper
		{
			*system='\0';
			printf("Wrong Domain\n");
		}
		else
		{
			if((ptrNetServerEnum = (NET_API_STATUS (WINAPI *)(LPCWSTR, DWORD, LPBYTE*, DWORD, LPDWORD, LPDWORD, DWORD, LPCWSTR, LPDWORD))GetProcAddress(hpNetAPI32, "NetServerEnum")))
			{
				ret = (*ptrNetServerEnum)( NULL, 100, (unsigned char **)&serv_info, 1024, &totalread, &totalentry, SV_TYPE_DOMAIN_CTRL, (wchar_t *)wks_info->wki100_langroup, 0);
			}
			else
				ret = !NERR_Success;

			if(ret != NERR_Success)
				*system = '\0';
			else
				wcstombs(system, (wchar_t *)serv_info->sv100_name, 80);
		}
		strcpy(username, name+result+1);
		strcpy(name, username);
	//clientobj.ucs_services("UWIN_CS");
	}
	else
		*system='\0';
}


void CUcsInstall::OnItemexpandingTree1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	
	UINT FillTree( LPVOID pParam );
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	CString szString = m_TreeCtrl.GetItemText((HTREEITEM)(pNMTreeView->itemNew).hItem);
	if(szString == domain)
	{
		if ((pNMTreeView->action != TVE_COLLAPSE)&&(!g_bHasDomainExpanded))
		{
			threadstruct.hItem = pNMTreeView->itemNew.hItem;
			threadstruct.pTreeCtrl = &m_TreeCtrl;
			AfxBeginThread(FillTree,(LPVOID)&threadstruct);
		
		}
		g_bHasDomainExpanded= true;
	}
	*pResult = 0;
}

UINT FillTree( LPVOID pParam )
{
	NET_API_STATUS (WINAPI *ptrNetGetDCName)(LPCWSTR, LPCWSTR, LPBYTE*);
	CTreeCtrl *pTreeCtrl;
	struct threadparam *pThreadData;
	//MSG msg;
	pThreadData = (struct threadparam *) pParam;
 	pTreeCtrl = pThreadData->pTreeCtrl;
	
	if (InstallCanceled == TRUE)
	{
			AfxEndThread(1);		
			return 0;
	}
	HTREEITEM LoadItem = pTreeCtrl->GetChildItem(pThreadData->hItem );
	
	//::PeekMessage(&msg,m_hWnd,NULL,NULL,PM_NOREMOVE);
	//::DispatchMessage(&msg);

		wchar_t mbuffer[1024];
	char usrname[1024],buffer[1024],pdc[1024];
	USER_INFO_0 *user_info;
	int i;
	DWORD entriesread,totalentries, Rshandle,size=sizeof(usrname),ret;
	LPBYTE CompName;
	
		Rshandle =0;
			
		mbstowcs(mbuffer,domain,80);
		if((ptrNetGetDCName = (NET_API_STATUS (WINAPI *)(LPCWSTR, LPCWSTR, LPBYTE*))GetProcAddress(hpNetAPI32, "NetGetDCName")))
		{
			(*ptrNetGetDCName)(NULL,mbuffer, &CompName); 
		}
		else
		{
			char buff[80];
			sprintf(buff,"GetProcAddress failed(NetGetDCName), Error=%ld",GetLastError());
			AfxMessageBox( buff);

		}
		wcstombs(pdc,(wchar_t *)CompName,80);
		strcpy(buffer,pdc);
		i = mbstowcs(mbuffer,buffer,80);
					
		Rshandle=0;
	
again:
			
		if(ptrNetUserEnum)
			ret = (*ptrNetUserEnum)((WCHAR *)mbuffer,0, FILTER_NORMAL_ACCOUNT, (unsigned char **)&user_info,8048,&entriesread, &totalentries,&Rshandle);
		else
			ret = !NERR_Success;
		if(ret == NERR_Success)
		{
			for(i=0;i<(int)totalentries;i++)
			{
				if (InstallCanceled == TRUE)
				{
					AfxEndThread(1);		
					return 0;
				}
	
				size = sizeof(usrname);
				wcstombs(usrname,user_info[i].usri0_name,size);
				if(!check_ucs(usrname,1))
				{
					pTreeCtrl->InsertItem(usrname,pThreadData->hItem );
				}
			}
			if (InstallCanceled == TRUE)
			{
				AfxEndThread(1);		
				return 0;
			}
		}

		if(ret == ERROR_MORE_DATA)
		{
			for(i=0;i<(int)entriesread;i++)
			{
				if (InstallCanceled == TRUE)
				{
					AfxEndThread(1);		
					return 0;
				}
	
				size = sizeof(usrname);
				wcstombs(usrname,user_info[i].usri0_name,size);
				if(!check_ucs(usrname,1))
				{
					pTreeCtrl->InsertItem(usrname,pThreadData->hItem );
				}
			}
			if (InstallCanceled == TRUE)
			{
				AfxEndThread(1);		
				return 0;
			}
			
			if(ptrNetApiBufferFree)
				(*ptrNetApiBufferFree)((LPVOID)user_info);
			goto again;
		}
		
	pTreeCtrl->DeleteItem(LoadItem);
	if(ptrNetApiBufferFree)
	{
		(*ptrNetApiBufferFree)(user_info);
		(*ptrNetApiBufferFree)(CompName);
	}
	AfxEndThread(1);		
	return 1;
}

void CUcsInstall::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	InstallCanceled = TRUE;
	
	CDialog::OnClose();
}

