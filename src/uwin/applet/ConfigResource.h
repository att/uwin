#if !defined(AFX_CONFIGRESOURCE_H__C0C5E873_BC3D_11D2_A0F0_0080C856DCB7__INCLUDED_)
#define AFX_CONFIGRESOURCE_H__C0C5E873_BC3D_11D2_A0F0_0080C856DCB7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigResource.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigResource dialog

#include "RegistryOps.h"

class CConfigResource : public CDialog
{
// Construction
public:
	CConfigResource(CWnd* pParent = NULL);   // standard constructor

	int getregval(BOOL);
	void setregval();
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues();

// Dialog Data
	//{{AFX_DATA(CConfigResource)
	enum { IDD = IDD_DIALOG_RESOURCE };
	DWORD	m_Fifo;
	DWORD	m_File;
	DWORD	m_Proc;
	DWORD	m_Sid;
	//}}AFX_DATA

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigResource)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigResource)
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditFifo();
	afx_msg void OnChangeEditFile();
	afx_msg void OnChangeEditProc();
	afx_msg void OnChangeEditSid();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGRESOURCE_H__C0C5E873_BC3D_11D2_A0F0_0080C856DCB7__INCLUDED_)
