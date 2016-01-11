// testcpl.cpp : Defines the initialization routines for the DLL.

static const char uwin_config_version_id[] = "\n@(#)uwin config 2011-02-22\0\n";

#include "stdafx.h"
#include "testcpl.h"
#include <cpl.h>

#include "UwinClient.h"
#include "UwinUms.h"

#include "UwinTelnet.h"
#include "UwinSystem.h"
#include "UwinInetd.h"
#include "UwinGeneral.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/*
 * these are expected by nafxcw.lib(appcore.obj)
 */

char**		__argv;
int		__argc;

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

/////////////////////////////////////////////////////////////////////////////
// CTestcplApp

BEGIN_MESSAGE_MAP(CTestcplApp, CWinApp)
	//{{AFX_MSG_MAP(CTestcplApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTestcplApp construction

CTestcplApp::CTestcplApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTestcplApp object

CTestcplApp theApp;
void ShowPropSheet(void)
{
	OSVERSIONINFO	osver;
	BOOL win9x=1;

	CPropertySheet propsheet("UWIN Configuration Manager");

	CUwinClient uwinclient;
	CUwinTelnet uwintelnet;
	
	CUwinUms		uwinums;
	CUwinSystem		uwinsys;
	CUwinInetd		uwininetd;
	CUwinGeneral	uwingeneral;

	osver.dwOSVersionInfoSize = sizeof(osver);
	if(GetVersionEx(&osver))
	{
		if(osver.dwPlatformId == VER_PLATFORM_WIN32_NT)
			win9x = 0;
		else
			win9x = 1;
	}
	propsheet.AddPage(&uwingeneral);
	propsheet.AddPage(&uwinsys);
	//propsheet.AddPage(&uwintelnet);
	if(!win9x)
	{
		propsheet.AddPage(&uwinums);
		propsheet.AddPage(&uwininetd);
		propsheet.AddPage(&uwinclient);
	}
	propsheet.DoModal();
	
}

HINSTANCE  hinst = NULL;

LONG APIENTRY CPlApplet(HWND hwndCPL,UINT uMsg,LPARAM lParam1,LPARAM lParam2) 
{ 
    LPCPLINFO lpCPlInfo; 
	switch (uMsg) { 
        case CPL_INIT:      // first message, sent once 
				return TRUE;
			break;
        case CPL_GETCOUNT:  // second message, sent once 
			return 1;//NUM_APPLETS; 
            break; 
 
        case CPL_INQUIRE: // third message, sent once per application 

			lpCPlInfo = (LPCPLINFO) lParam2; 
            lpCPlInfo->lData = 0; 
            lpCPlInfo->idIcon = IDI_ICON2;
            lpCPlInfo->idName = IDS_STRING1;
            lpCPlInfo->idInfo = IDS_STRING2;
            break; 

        case CPL_DBLCLK:    // application icon double-clicked 
			ShowPropSheet();
		    break; 
 
        case CPL_STOP:      // sent once per application before CPL_EXIT 
			 break; 
 
        case CPL_EXIT:    // sent once before FreeLibrary is called 
			  break; 
 
        default: 
            break; 
    } 
    return 0; 
} 
