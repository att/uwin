#if !defined(AFX_CONFIGMISCDLG_H__767B1EB1_5899_11D4_BDD5_006067444D03__INCLUDED_)
#define AFX_CONFIGMISCDLG_H__767B1EB1_5899_11D4_BDD5_006067444D03__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ConfigMiscDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigMiscDlg dialog

#include "RegistryOps.h"

class CConfigMiscDlg : public CDialog
{
// Construction
public:
	CConfigMiscDlg(CWnd* pParent = NULL);   // standard constructor
	int getregval(BOOL); // Reads values from registry
	void setregval(); // Set values in registry
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues(); // Set the dialog values to that in member variables

// Dialog Data
	//{{AFX_DATA(CConfigMiscDlg)
	enum { IDD = IDD_DIALOG_MISC };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
	DWORD m_CaseSensitive;
	DWORD m_NoShamWoW;

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigMiscDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigMiscDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheckCasesensitive();
	afx_msg void OnCheckNoShamWoW();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGMISCDLG_H__767B1EB1_5899_11D4_BDD5_006067444D03__INCLUDED_)
