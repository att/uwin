#if !defined(AFX_UWINCLIENT_H__91900224_2150_11D2_B7C0_00606704E193__INCLUDED_)
#define AFX_UWINCLIENT_H__91900224_2150_11D2_B7C0_00606704E193__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinClient.h : header file
//
#include "resource.h"
/////////////////////////////////////////////////////////////////////////////
// CUwinClient dialog

class CUwinClient : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinClient)

// Construction
public:
	CUwinClient();
	~CUwinClient();

// Dialog Data
	//{{AFX_DATA(CUwinClient)
	enum { IDD = IDD_PROPPAGE_UWINCLIENT };
	CListCtrl	m_ClientList;
	CString	m_szAccepted;
	CString	m_szDependencies;
	CString	m_szServiceName;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinClient)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	public:
	int ucs_services(char *servname);

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUwinClient)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonInstall();
	afx_msg void OnRadioDelete();
	afx_msg void OnClickListClientlist(NMLISTVIEW* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedListClientlist(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	BOOL bIsRadioClicked;
    //int ucs_services(char *servname);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWINCLIENT_H__91900224_2150_11D2_B7C0_00606704E193__INCLUDED_)
