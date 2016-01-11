#if !defined(AFX_CONFIGSEMDLG_H__72FC2434_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
#define AFX_CONFIGSEMDLG_H__72FC2434_BB48_11D2_AF29_0000E2034AEA__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigSemDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigSemDlg dialog

#include "RegistryOps.h"

class CConfigSemDlg : public CDialog
{
// Construction
public:
	CConfigSemDlg(CWnd* pParent = NULL);   // standard constructor
	int getregval(BOOL); /* Reads values from registry */
	void setregval(); // Set values in registry
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues(); // Set the dialog values to that in member variables

// Dialog Data
	//{{AFX_DATA(CConfigSemDlg)
	enum { IDD = IDD_DIALOG_SEM };
	DWORD	m_SemMaxIds;
	DWORD	m_SemMaxInSys;
	DWORD	m_SemMaxOpsPerCall;
	DWORD	m_SemMaxSemPerId;
	DWORD	m_SemMaxVal;
	//}}AFX_DATA

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigSemDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigSemDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEDITSemMaxIds();
	afx_msg void OnChangeEDITSemMaxInSys();
	afx_msg void OnChangeEDITSemMaxOpsPerCall();
	afx_msg void OnChangeEDITSemMaxSemPerId();
	afx_msg void OnChangeEDITSemMaxVal();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGSEMDLG_H__72FC2434_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
