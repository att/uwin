#if !defined(AFX_CONFIGCONSOLEDLG_H__D9C4D334_BF42_11D2_A0F3_0080C856DCB7__INCLUDED_)
#define AFX_CONFIGCONSOLEDLG_H__D9C4D334_BF42_11D2_A0F3_0080C856DCB7__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ConfigConsoleDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CConfigConsoleDlg dialog

#include "RegistryOps.h"

#define RADIO_BOLD	0
#define RADIO_BLINK	1
#define RADIO_UNDERLINE 2

// Taken from the Developer Studio <wincon.h> file
#define FB	0x0001 // text color contains blue.
#define FG	0x0002 // text color contains green.
#define FR	0x0004 // text color contains red.
#define FI	0x0008 // text color is intensified.
#define BB	0x0010 // background color contains blue.
#define BG	0x0020 // background color contains green.
#define BR	0x0040 // background color contains red.
#define BI	0x0080 // background color is intensified.

class CConfigConsoleDlg : public CDialog
{
// Construction
public:
	CConfigConsoleDlg(CWnd* pParent = NULL);   // standard constructor
	int getregval(BOOL); // Reads values from registry 
	void setregval(); /* Set values in registry */
	BOOL getdialogvalues(); // Get the dialog values and store in 
							// member variables
	void setdialogvalues(); // Set the dialog values to that in member variables
	void check_box();	// Checks the appropriate checkboxes depending on
						// colors selected for that perticular text attribute

// Dialog Data
	//{{AFX_DATA(CConfigConsoleDlg)
	enum { IDD = IDD_DIALOG_CONSOLE };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	DWORD	m_txtattr;			// Indicates the text attribute being displayed
								// (bold, blink, or underline)
	DWORD	m_color_bold;		// Indicates the combination for bold text
	DWORD	m_color_blink;		// Indicates the combination for blink text
	DWORD	m_color_underline;	// Indicates the combination for underline text

	CRegistryOps regOps;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CConfigConsoleDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CConfigConsoleDlg)
	afx_msg void OnButtonPreview();
	afx_msg void OnCheckBb();
	afx_msg void OnCheckBg();
	afx_msg void OnCheckBi();
	afx_msg void OnCheckBr();
	afx_msg void OnCheckFb();
	afx_msg void OnCheckFg();
	afx_msg void OnCheckFi();
	afx_msg void OnCheckFr();
	afx_msg void OnRadioBlink();
	afx_msg void OnRadioBold();
	afx_msg void OnRadioUnderline();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGCONSOLEDLG_H__D9C4D334_BF42_11D2_A0F3_0080C856DCB7__INCLUDED_)
