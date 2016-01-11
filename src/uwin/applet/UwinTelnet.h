#if !defined(AFX_UWINTELNET_H__91900221_2150_11D2_B7C0_00606704E193__INCLUDED_)
#define AFX_UWINTELNET_H__91900221_2150_11D2_B7C0_00606704E193__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinTelnet.h : header file
//
#include "resource.h"
/////////////////////////////////////////////////////////////////////////////
// CUwinTelnet dialog
#define TELNETINSTALL	 0
#define TELNETDELETE	 1
#define TELNETSTART		 2
#define TELNETSTOP		 3
#define OPTIONERROR			-99
class CUwinTelnet : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinTelnet)

// Construction
public:
	CUwinTelnet();
	~CUwinTelnet();
	int CUwinTelnet::GetRadioSelected();
// Dialog Data
	//{{AFX_DATA(CUwinTelnet)
	enum { IDD = IDD_PROPPAGE_TELNET };
	CComboBox	m_ComboBox;
	CString	m_szAccepted;
	CString	m_szDependencies;
	CString	m_szServiceName;
	CString	m_szState;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinTelnet)
	public:
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUwinTelnet)
	virtual BOOL OnInitDialog();
	afx_msg void OnRadioTelnetDelete();
	afx_msg void OnRadioTelnetInstall();
	afx_msg void OnRadioTelnetStart();
	afx_msg void OnRadioTelnetStop();
	afx_msg void OnChangeEditTelnetPort();
	afx_msg void OnChangeEditMaxConns();
	afx_msg void OnSelendokComboTelnetStartup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void CUwinTelnet::SetApply() ;
	BOOL bIsTelnetStartClicked;
	BOOL bIsTelnetStopClicked;
	BOOL bIsTelnetInstallClicked;
	BOOL bIsTelnetDeleteClicked;

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWINTELNET_H__91900221_2150_11D2_B7C0_00606704E193__INCLUDED_)
