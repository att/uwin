// UwinSystem.cpp : implementation file
//

#include "globals.h"
#include "stdafx.h"
#include "testcpl.h"
#include "UwinSystem.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CUwinSystem property page


IMPLEMENT_DYNCREATE(CUwinSystem, CPropertyPage)

CUwinSystem::CUwinSystem() : CPropertyPage(CUwinSystem::IDD)
{
	//{{AFX_DATA_INIT(CUwinSystem)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

}

CUwinSystem::~CUwinSystem()
{
}

void CUwinSystem::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CUwinSystem)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	DDX_Control(pDX, IDC_COMBO_CATEGORY, m_ComboSelect);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CUwinSystem, CPropertyPage)
	//{{AFX_MSG_MAP(CUwinSystem)
	ON_CBN_SELENDOK(IDC_COMBO_CATEGORY, OnSelendokComboCategory)
	ON_BN_CLICKED(IDC_BUTTON_DEFAULT, OnButtonDefault)
	ON_BN_CLICKED(IDC_RADIO_ALLUSR, OnRadioAlluser)
	ON_BN_CLICKED(IDC_RADIO_CURUSR, OnRadioCurrentuser)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CUwinSystem message handlers

BOOL CUwinSystem::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	// TODO: Add extra initialization here
	
	// Create dialog instances for child dialogs
	dlgCon.Create(MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),this);
	dlgMsg.Create(MAKEINTRESOURCE(IDD_DIALOG_MSG),this);
	dlgRes.Create(MAKEINTRESOURCE(IDD_DIALOG_RESOURCE),this);
	dlgSem.Create(MAKEINTRESOURCE(IDD_DIALOG_SEM),this);
	dlgShm.Create(MAKEINTRESOURCE(IDD_DIALOG_SHM),this);
	dlgMisc.Create(MAKEINTRESOURCE(IDD_DIALOG_MISC),this);

	old_category = -1; // No previous category


	// Set 'All users' radio button
	profile = ALL_USERS;
	CButton *pAllUsr = (CButton *)GetDlgItem(IDC_RADIO_ALLUSR);
	if(pAllUsr)
		pAllUsr->SetCheck(TRUE);
	m_ComboSelect.SetCurSel(0);

	// Show the first dialog
	OnSelendokComboCategory();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CUwinSystem::OnApply() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	switch(m_ComboSelect.GetCurSel())
	{
		case 0: // Console
			dlgCon.getdialogvalues();
			dlgCon.setregval();
			break;
		case 1: // Message queues
			dlgMsg.getdialogvalues();
			dlgMsg.setregval();
			break;
		case 2: // Resources
			dlgRes.getdialogvalues();
			dlgRes.setregval();
			break;
		case 3: // Semaphores
			dlgSem.getdialogvalues();
			dlgSem.setregval();
			break;
		case 4: // Shared memory
			dlgShm.getdialogvalues();
			dlgShm.setregval();
			break;
		case 5: // Miscellaneous
			dlgMisc.getdialogvalues();
			dlgMisc.setregval();
			break;
	}

	SetModified(FALSE); // Turn off Apply button

	return CPropertyPage::OnApply();
}

void CUwinSystem::OnSelendokComboCategory() 
{
	// TODO: Add your control notification handler code here
	int iSelected,width,height;
	RECT Rect,childRect;

	GetWindowRect(&Rect);
	ScreenToClient(&Rect);
		
	iSelected = m_ComboSelect.GetCurSel();
	switch(iSelected)
	{
		case 0:	// Console
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;

			// Set window position
			dlgCon.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgCon.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
		case 1:	// Message queues
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;
			
			// Set window position
			dlgMsg.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgMsg.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
		case 2:	// Resources
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;

			// Set window position
			dlgRes.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgRes.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
		case 3:	// Semaphores
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;

			// Set window position
			dlgSem.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgSem.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
		case 4:	// Shared memory
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;

			// Set window position
			dlgShm.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgShm.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
		case 5:	// Miscellaneous
			// Hide the previous dialog
			hide_prev();
			old_category = iSelected;

			// Set window position
			dlgMisc.GetWindowRect(&childRect);
			width = childRect.right - childRect.left;
			height = childRect.bottom - childRect.top;
			dlgMisc.SetWindowPos(NULL,Rect.left+10,Rect.top+40,width,height,NULL);

			// Display window
			refresh(REGISTRY);
			break;
	}
}

void CUwinSystem::hide_prev()
{
	switch(old_category)
	{
		case 0: // Console
			dlgCon.ShowWindow(SW_HIDE);
			break;
		case 1: // Message queues
			dlgMsg.ShowWindow(SW_HIDE);
			break;
		case 2: // Resources
			dlgRes.ShowWindow(SW_HIDE);
			break;
		case 3: // Semaphores
			dlgSem.ShowWindow(SW_HIDE);
			break;
		case 4: // Shared memory
			dlgShm.ShowWindow(SW_HIDE);
			break;
		case 5: // Miscellaneous
			dlgMisc.ShowWindow(SW_HIDE);
			break;
	}
}

void CUwinSystem::OnButtonDefault() 
{
	// TODO: Add your control notification handler code here
	
	// Refresh current dialog with default values
	refresh(DEFAULT);
	SetModified(TRUE);
}

void CUwinSystem::OnRadioAlluser() 
{
	// TODO: Add your control notification handler code here

	if(profile != ALL_USERS)
	{
		profile = ALL_USERS;	

		// Refresh current dialog with appropriate values from registry
		refresh(REGISTRY);
	}
}

void CUwinSystem::OnRadioCurrentuser() 
{
	// TODO: Add your control notification handler code here
	
	if(profile != CURRENT_USER)
	{
		profile = CURRENT_USER;	

		// Refresh current dialog with appropriate values from registry
		refresh(REGISTRY);
	}
}

/* 
 * Refreshes the current dialog with appropriate values.
 * flag indicates whether default or current registry values needs
 * to obtained or whether to refresh the dialog with current values itself
 */ 
void CUwinSystem::refresh(BOOL flag)
{
	int ret;

	switch(m_ComboSelect.GetCurSel())
	{
		case 0: // Console
			if(flag != CURRENT)
			{
				ret = dlgCon.getregval(flag);
				dlgCon.setdialogvalues();
				if(!ret)
					SetModified(TRUE);
				else
					SetModified(FALSE);
			}
			dlgCon.ShowWindow(SW_SHOW);
			break;
		case 1: // Message queues
			if(flag != CURRENT)
			{
				ret = dlgMsg.getregval(flag);
				dlgMsg.setdialogvalues();
				if(!ret)
					SetModified(TRUE);
				else
					SetModified(FALSE);
			}
			dlgMsg.ShowWindow(SW_SHOW);
			break;
		case 2: // Resources
			if(flag != CURRENT)
			{
				ret = dlgRes.getregval(flag);
				dlgRes.setdialogvalues();
				if(!ret)
					SetModified(TRUE);
				else
					SetModified(FALSE);
			}
			dlgRes.ShowWindow(SW_SHOW);
			break;
		case 3: // Semaphores
			if(flag != CURRENT)
			{
				ret = dlgSem.getregval(flag);
				dlgSem.setdialogvalues();
				if(!ret)
				{
					SetModified(TRUE);
				}
				else
				{
					SetModified(FALSE);
				}
			}
			dlgSem.ShowWindow(SW_SHOW);
			break;
		case 4: // Shared memory
			if(flag != CURRENT)
			{
				ret = dlgShm.getregval(flag);
				dlgShm.setdialogvalues();
				if(!ret)
				{
					SetModified(TRUE);
				}
				else
				{
					SetModified(FALSE);
				}
			}
			dlgShm.ShowWindow(SW_SHOW);
			break;
		case 5: // Miscellaneous
			if(flag != CURRENT)
			{
				ret = dlgMisc.getregval(flag);
				dlgMisc.setdialogvalues();
				if(!ret)
				{
					SetModified(TRUE);
				}
				else
				{
					SetModified(FALSE);
				}
			}
			dlgMisc.ShowWindow(SW_SHOW);
			break;
	}
}

void CUwinSystem::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	refresh(CURRENT);
	
	// Do not call CPropertyPage::OnPaint() for painting messages
}
