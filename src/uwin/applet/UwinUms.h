#if !defined(AFX_UWINUMS_H__91900223_2150_11D2_B7C0_00606704E193__INCLUDED_)
#define AFX_UWINUMS_H__91900223_2150_11D2_B7C0_00606704E193__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinUms.h : header file
//
#include "resource.h"
/////////////////////////////////////////////////////////////////////////////
// CUwinUms dialog
#define UMSINSTALL	 0
#define UMSDELETE	 1
#define UMSSTART	 2
#define UMSSTOP		 3
#define UMSUID		 4
#define UMSENUMERATE	5
#define UMSREUSE		6
#define UMSCOMBO		7
#define OPTIONERROR		-99

class CUwinUms : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinUms)

// Construction
public:
	CUwinUms();
	~CUwinUms();
	int CUwinUms::GetRadioSelected();
// Dialog Data
	//{{AFX_DATA(CUwinUms)
	enum { IDD = IDD_PROPPAGE_UMS };
	CComboBox	m_ComboBox;
	CString	m_szServiceName;
	CString	m_szState;
	CString	m_szDependencies;
	CString	m_szAccepted;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinUms)
	public:
	virtual BOOL OnApply();
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void OnCheckPassFlag();
	// Generated message map functions
	//{{AFX_MSG(CUwinUms)
	virtual BOOL OnInitDialog();
	
	afx_msg void OnRadioUmsDelete();
	afx_msg void OnRadioUmsInstall();
	afx_msg void OnRadioUmsStart();
	afx_msg void OnRadioUmsStop();
	afx_msg void OnCheckEnumerate();
	afx_msg void OnCheckReuse();
	afx_msg void OnCheckSetuid();
	afx_msg void OnSelendokComboUmsStartup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	void CUwinUms::SetApply() ;
	BOOL bIsUmsStartClicked;
	BOOL bIsUmsStopClicked;
	BOOL bIsUmsInstallClicked;
	BOOL bIsUmsDeleteClicked;
	bool b_IsCheckModified;
	

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWINUMS_H__91900223_2150_11D2_B7C0_00606704E193__INCLUDED_)
