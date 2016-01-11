#if !defined(AFX_UWININETD_H__9B7C1D93_CD61_11D2_A0FC_0080C856DCB7__INCLUDED_)
#define AFX_UWININETD_H__9B7C1D93_CD61_11D2_A0FC_0080C856DCB7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UwinInetd.h : header file
//

#include "SortClass.h"
#include "RegistryOps.h"
#include "JkInetdConf.h"

#define REFRESH 0
#define CREATE 1

/////////////////////////////////////////////////////////////////////////////
// CUwinInetd dialog

class CUwinInetd : public CPropertyPage
{
	DECLARE_DYNCREATE(CUwinInetd)

// Construction
public:
	CUwinInetd();
	~CUwinInetd();

	void AddColumnsToList();
	void AddListControlItems(int);
	void ConfigureItem();
	void GetListValues();
	void SetInetdValues(char *, char *, char *);
	void InvokeScript();
	
// Dialog Data
	//{{AFX_DATA(CUwinInetd)
	enum { IDD = IDD_PROPPAGE_INETD };
	CListCtrl	m_ListDisplayControl;
	//}}AFX_DATA

	CRegistryOps regOps;
	CJkInetdConf confDlg;

	BOOL bSortAscending;
	int nSortedCol;
	int m_arrColType[3];
	DWORD inetdValues[8];

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CUwinInetd)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	BOOL change_flag;

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CUwinInetd)
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkListServices(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnColumnclickListServices(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButtonAdd();
	afx_msg void OnButtonConfigure();
	afx_msg void OnButtonDelete();
	afx_msg void OnButtonDisable();
	afx_msg void OnButtonEnable();
	afx_msg void OnButtonRefresh();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UWININETD_H__9B7C1D93_CD61_11D2_A0FC_0080C856DCB7__INCLUDED_)
