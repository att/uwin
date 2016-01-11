// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__D0653188_3780_11D2_B7C1_00606704E193__INCLUDED_)
#define AFX_STDAFX_H__D0653188_3780_11D2_B7C1_00606704E193__INCLUDED_

// Refresh flags
#define CURRENT 0  // Indicates that the refreshing be done with current values
#define REGISTRY 1 // Indicates that values must be taken from registry
#define DEFAULT 2  // Indicates that only default values must be taken

#define STR_TYPE 1
#define INT_TYPE 2

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__D0653188_3780_11D2_B7C1_00606704E193__INCLUDED_)
