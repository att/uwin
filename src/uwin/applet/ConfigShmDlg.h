#if !defined(AFX_CONFIGSHMDLG_H__72FC2435_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
#define AFX_CONFIGSHMDLG_H__72FC2435_BB48_11D2_AF29_0000E2034AEA__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigShmDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigShmDlg dialog

#include "RegistryOps.h"

class CConfigShmDlg : public CDialog
{
// Construction
public:
	CConfigShmDlg(CWnd* pParent = NULL);   // standard constructor
	int getregval(BOOL); // Reads values from registry 
	void setregval(); // Set values in registry 
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues(); // Set the dialog values to that in member variables

// Dialog Data
	//{{AFX_DATA(CConfigShmDlg)
	enum { IDD = IDD_DIALOG_SHM };
	DWORD	m_ShmMaxIds;
	DWORD	m_ShmMaxSegPerProc;
	DWORD	m_ShmMaxSize;
	DWORD	m_ShmMinSize;
	//}}AFX_DATA

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigShmDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigShmDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEDITShmMaxIds();
	afx_msg void OnChangeEDITShmMaxSegPerProc();
	afx_msg void OnChangeEDITShmMaxSize();
	afx_msg void OnChangeEDITShmMinSize();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGSHMDLG_H__72FC2435_BB48_11D2_AF29_0000E2034AEA__INCLUDED_)
