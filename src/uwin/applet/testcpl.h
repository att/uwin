// testcpl.h : main header file for the TESTCPL DLL
//

#if !defined(AFX_TESTCPL_H__D0653186_3780_11D2_B7C1_00606704E193__INCLUDED_)
#define AFX_TESTCPL_H__D0653186_3780_11D2_B7C1_00606704E193__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CTestcplApp
// See testcpl.cpp for the implementation of this class
//

class CTestcplApp : public CWinApp
{
public:
	CTestcplApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTestcplApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CTestcplApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TESTCPL_H__D0653186_3780_11D2_B7C1_00606704E193__INCLUDED_)
