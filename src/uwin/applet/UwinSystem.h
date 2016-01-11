#if !defined(AFX_UWINSYSTEM_H__72FC2437_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
#define AFX_UWINSYSTEM_H__72FC2437_BB48_11D2_AF29_0000E2034AEA__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinSystem.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CUwinSystem dialog

#include "ConfigConsoleDlg.h"
#include "ConfigMsgDlg.h"
#include "ConfigSemDlg.h"
#include "ConfigShmDlg.h"
#include "ConfigResource.h"
#include "ConfigMiscDlg.h"

#define ALL_USERS 0
#define CURRENT_USER 1

class CUwinSystem : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinSystem)

// Construction
public:
	CUwinSystem();
	~CUwinSystem();
	
	CConfigConsoleDlg	dlgCon;
	CConfigMsgDlg		dlgMsg;
	CConfigSemDlg		dlgSem;
	CConfigShmDlg		dlgShm;
	CConfigResource		dlgRes;
	CConfigMiscDlg		dlgMisc;

	void hide_prev();
	void refresh(BOOL);

// Dialog Data
	//{{AFX_DATA(CUwinSystem)
	enum { IDD = IDD_PROPPAGE_SYSTEM };
	CComboBox	m_ComboSelect;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinSystem)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUwinSystem)
	afx_msg void OnSelendokComboCategory();
	afx_msg void OnButtonDefault();
	afx_msg void OnRadioAlluser();
	afx_msg void OnRadioCurrentuser();
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	BOOL profile; /* Indicates profile. 0=>all; 1=>current user */
	int old_category; /* Indicates the previous category selected */
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWINSYSTEM_H__72FC2437_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
