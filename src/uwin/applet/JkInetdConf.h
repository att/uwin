#if !defined(AFX_JKINETDCONF_H__E99AB3B3_D12A_11D2_9259_00606736623C__INCLUDED_)
#define AFX_JKINETDCONF_H__E99AB3B3_D12A_11D2_9259_00606736623C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// JkInetdConf.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CJkInetdConf dialog

class CJkInetdConf : public CDialog
{
// Construction
public:
	CJkInetdConf(CWnd* pParent = NULL);   // standard constructor

	void SetServerName(char *);

	void GetPortNumber(DWORD *);
	void SetPortNumber(DWORD);

	void GetEnable(DWORD *);
	void SetEnable(DWORD);

	void SetDialogValues();

// Dialog Data
	//{{AFX_DATA(CJkInetdConf)
	enum { IDD = IDD_DIALOG_INETDCONF_JUNK };
	DWORD	m_PortNumber;
	//}}AFX_DATA

	CString m_ServerName;
	int m_Enable;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CJkInetdConf)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CJkInetdConf)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonJkDefault();
	virtual void OnOK();
	afx_msg void OnCheckJkEnable();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_JKINETDCONF_H__E99AB3B3_D12A_11D2_9259_00606736623C__INCLUDED_)
