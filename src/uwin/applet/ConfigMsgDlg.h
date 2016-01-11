#if !defined(AFX_CONFIGMSGDLG_H__72FC2433_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
#define AFX_CONFIGMSGDLG_H__72FC2433_BB48_11D2_AF29_0000E2034AEA__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigMsgDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigMsgDlg dialog

#include "RegistryOps.h"

class CConfigMsgDlg : public CDialog
{
// Construction
public:
	CConfigMsgDlg(CWnd* pParent = NULL);   // standard constructor
	int getregval(BOOL); // Reads values from registry 
	void setregval(); /* Set values in registry */
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues(); // Set the dialog values to that in member variables

// Dialog Data
	//{{AFX_DATA(CConfigMsgDlg)
	enum { IDD = IDD_DIALOG_MSG };
	DWORD	m_MsgMaxIds;
	DWORD	m_MsgMaxQSize;
	DWORD	m_MsgMaxSize;
	//}}AFX_DATA

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigMsgDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigMsgDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEDITMsgMaxIds();
	afx_msg void OnChangeEDITMsgMaxQSize();
	afx_msg void OnChangeEDITMsgMaxSize();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGMSGDLG_H__72FC2433_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
