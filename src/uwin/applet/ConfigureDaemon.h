#if !defined(AFX_CONFIGUREDAEMON_H__6EF20E73_CF9B_11D2_9258_00606736623C__INCLUDED_)
#define AFX_CONFIGUREDAEMON_H__6EF20E73_CF9B_11D2_9258_00606736623C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigureDaemon.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigureDaemon dialog

class CConfigureDaemon : public CDialog
{
// Construction
public:
	CConfigureDaemon(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CConfigureDaemon)
	enum { IDD = IDD_DIALOG_CONFIGUREDAEMON };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigureDaemon)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigureDaemon)
	afx_msg void OnButtonApply();
	afx_msg void OnButtonBrowse();
	afx_msg void OnRadioDgram();
	afx_msg void OnRadioNowait();
	afx_msg void OnRadioStream();
	afx_msg void OnRadioTcp();
	afx_msg void OnRadioUdp();
	afx_msg void OnRadioWait();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGUREDAEMON_H__6EF20E73_CF9B_11D2_9258_00606736623C__INCLUDED_)
