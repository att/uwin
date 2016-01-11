#if !defined(AFX_UWINGENERAL_H__6EF20E74_CF9B_11D2_9258_00606736623C__INCLUDED_)
#define AFX_UWINGENERAL_H__6EF20E74_CF9B_11D2_9258_00606736623C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinGeneral.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CUwinGeneral dialog

#include <stdio.h>
#include <string.h>
#include "sysinfo.h"
#include "RegistryOps.h"

#define MAX 	256
#define MAXREAD 1024

class CUwinGeneral : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinGeneral)

// Construction
public:
	CUwinGeneral();
	~CUwinGeneral();

	void SetOsVersion();
	void SetUwinRelease();
	void SetRegisteredUser();
	void SetLicenceKey();
	void SetErrorLoggingLevel();
	void SetInstalledUwinReleases();

	void ProcessInstalledReleaseRequest();

// Dialog Data
	//{{AFX_DATA(CUwinGeneral)
	enum { IDD = IDD_PROPPAGE_GENERAL };
	CComboBox	m_ComboRelease;
	CComboBox	m_ComboLogLevel;
	//}}AFX_DATA

	CSystemInfo sysInfo;
	CRegistryOps regOps;
	char m_Release[64];

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinGeneral)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUwinGeneral)
	afx_msg void OnSelendokComboLoglevel();
	afx_msg void OnSelendokComboRelease();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditLicencekey();
	afx_msg void OnChangeEditReguser();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWINGENERAL_H__6EF20E74_CF9B_11D2_9258_00606736623C__INCLUDED_)
