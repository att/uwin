#if !defined(AFX_UCSINSTALL_H__63BFE382_230E_11D2_B7C0_00606704E193__INCLUDED_)
#define AFX_UCSINSTALL_H__63BFE382_230E_11D2_B7C0_00606704E193__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// UcsInstall.h : header file
//
#include "resource.h"
/////////////////////////////////////////////////////////////////////////////
// CUcsInstall dialog

class CUcsInstall : public CDialog
{
// Construction
public:
	CUcsInstall(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CUcsInstall)
	enum { IDD = IDD_DIALOG_UCSINSTALL };
	CTreeCtrl	m_TreeCtrl;
	CString	m_szPassword;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUcsInstall)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUcsInstall)
	afx_msg void OnButtonUcsInstall();
	virtual BOOL OnInitDialog();
	afx_msg void OnItemexpandingTree1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
//public:
		//int check_ucs(char *usrname);
private:
	int set_privileges();
	void mangle(char *name,char *system);
	int installucs(char *account, char *pass);
	FARPROC getapi_addr(const char *, const char *);
	//void get_domain(char *machine,char *domain);
	bool g_bHasDomainExpanded;
	//int check_ucs(char *usrname);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_UCSINSTALL_H__63BFE382_230E_11D2_B7C0_00606704E193__INCLUDED_)
