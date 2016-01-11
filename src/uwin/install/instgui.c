#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include "resource.h"

#include "uwin_keys.c"

#ifndef DWL_MSGRESULT
#define DWL_MSGRESULT	0
#endif

typedef struct state_s {
    HINSTANCE hInstance;
    char homepath[MAX_PATH];
    char rootpath[MAX_PATH];
    char package[64];
    char release[64];
    char version[64];
    int created;
    int cancelled;
} state_t;

char mymsg[MAX_PATH];

state_t state;

BOOL InitApplication (HANDLE);
BOOL InitInstance (HANDLE, int);
LONG APIENTRY MainWndProc (HWND, UINT, UINT, LONG);
int CreateWizard (HWND, HINSTANCE);
void FillPage (PROPSHEETPAGE *, int, LPCTSTR, DLGPROC);
BOOL APIENTRY Welcome (HWND, UINT, UINT, LONG);
BOOL APIENTRY License (HWND, UINT, UINT, LONG);
BOOL APIENTRY UserInfo (HWND, UINT, UINT, LONG);
BOOL APIENTRY Notes (HWND, UINT, UINT, LONG);
BOOL APIENTRY Root_Dest (HWND, UINT, UINT, LONG);
BOOL APIENTRY Home_Dest (HWND, UINT, UINT, LONG);
BOOL APIENTRY Install (HWND, UINT, UINT, LONG);
BOOL APIENTRY LoadFmFile (HWND, int, char *);
BOOL BrowseFolder (HWND, char *, const char *);
BOOL MakeADir (HWND, char *, int, int);

/*
 * is_admin() support function
 */
static TOKEN_PRIVILEGES *backup_restore(void)
{
	static LUID bluid, rluid;
	static char buf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
	TOKEN_PRIVILEGES *tokpri=(TOKEN_PRIVILEGES *)buf;
	if(!LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &bluid) ||
		!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &rluid))
		return(0);
	tokpri->PrivilegeCount = 2;
	tokpri->Privileges[0].Luid = bluid;
	tokpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tokpri->Privileges[1].Luid = rluid;
	tokpri->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
	return(tokpri);
}

/*
 * is_admin() support function
 */
static HANDLE setprivs(void *prevstate, int size, TOKEN_PRIVILEGES*(*fn)(void))
{
	HANDLE atok=NULL, me=GetCurrentProcess();
	TOKEN_PRIVILEGES *tokpri;
	int dummy;

	if(!OpenProcessToken(me,TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,&atok))
		return(NULL);
	if(tokpri = (*fn)())
		AdjustTokenPrivileges(atok, FALSE, tokpri, size, prevstate, &dummy);
	if (GetLastError() == ERROR_SUCCESS)
		return(atok);
	CloseHandle(atok);
	return(NULL);
}

/*
 * return non-zero if the caller has Administrators privileges
 */
static int is_admin(void)
{
	int prevstate[256];
	HANDLE atok;
	TOKEN_PRIVILEGES *oldpri=(TOKEN_PRIVILEGES *)prevstate;
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(info);
	if(GetVersionEx(&info) && info.dwPlatformId!=VER_PLATFORM_WIN32_NT)
		return(1);
	if(atok=setprivs(prevstate, sizeof(prevstate),backup_restore))
	{
		AdjustTokenPrivileges(atok, FALSE, oldpri, 0, NULL, 0);
		CloseHandle(atok);
		return(oldpri->PrivilegeCount==2);
	}
	return(0);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    int i;
    char* s;
    char* t;

    if (s = lpCmdLine) {
	do {
		if (*s != '-' || *(s + 1) != '-')
			break;
		if (*(s + 2) == ' ')
			i = 2;
		else if (!strncmp(s, "--debug", 7)) {
			i = 7;
		}
		for (s += i; *s && *s != ' '; s++);
		for (; *s == ' '; s++);
	} while (i != 2);
	for (t = state.package; *s && *s != ' '; *t++ = *s++);
	*t = 0;
	for (; *s == ' '; s++);
	for (t = state.release; *s && *s != ' '; *t++ = *s++);
	*t = 0;
	for (; *s == ' '; s++);
	for (t = state.version; *s && *s != ' '; *t++ = *s++);
	*t = 0;
    }
    if (!state.version[0]) {
        MessageBox (NULL, "UWIN Installation Failed -- PACKAGE RELEASE VERSION operands expected", "UWIN Setup", MB_OK);
	return 2;
    }
    state.hInstance = hInstance;
    state.created = 0;
    state.cancelled = 0;
    /* if the initialization fails, return. */
    if (!InitApplication (hInstance))
        return 2;

    /* Perform initializations that apply to a specific instance */
    if (!InitInstance (hInstance, nCmdShow))
        return 2;

    /* Acquire and dispatch messages until a WM_QUIT message is received. */
    while (GetMessage (&msg, NULL, 0, 0)) {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
    if (state.cancelled) {
	if (state.cancelled > 0) {
	    sprintf(mymsg, "UWIN %s %s %s %d bit installation cancelled", state.release, state.package, state.version, 8 * sizeof(char*));
            MessageBox (NULL, mymsg, "UWIN Setup", MB_OK);
	}
	return 1;
    }
    else if (uwin_setkeys(state.package, state.release, state.version, state.rootpath, state.homepath)) {
	sprintf(mymsg, "UWIN %s %s %s %d bit installation failed", state.release, state.package, state.version, 8 * sizeof(char*));
        MessageBox (NULL, mymsg, "UWIN Setup", MB_OK);
	return 1;
    }
    return 0;
}

BOOL InitApplication (HANDLE hInstance) {
    WNDCLASS wc;

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (EXE_ICON));
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject (WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TEXT ("SampleWClass");
    return RegisterClass (&wc);
}

BOOL InitInstance (HANDLE inst, int show) {
    HWND hWnd;

    if(!(hWnd = CreateWindow (
        TEXT ("SampleWClass"), TEXT ("UWIN Setup"), WS_OVERLAPPEDWINDOW,
        180, 180, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, inst, NULL
    )))
        return 0;
    ShowWindow (hWnd, SW_HIDE);
    UpdateWindow (hWnd);
    return TRUE;
}

LONG APIENTRY MainWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam) {
    char msg[MAX_PATH];
    switch (message) {
    case WM_CREATE:
	if (!uwin_getkeys(state.release, state.rootpath, sizeof(state.rootpath), state.homepath, sizeof(state.homepath))) {
            wsprintf (msg, "Install UWIN %s %s %s %d bit with\r\n    ROOT='%s'\r\n    HOME='%s'\r\nClick NO to change the installation defaults.", state.release, state.package, state.version, 8 * sizeof(char*), state.rootpath, state.homepath);
            switch (MessageBox (NULL, msg, "UWIN Setup", MB_YESNOCANCEL)) {
	    default:
		state.cancelled = -1;
		/*FALLTHROUGH*/
	    case IDYES:
        	PostQuitMessage (0);
		return 0;
	    case IDNO:
		break;
	    }
	}
        // Start up the install
        PostMessage (hWnd, WM_COMMAND, ID_INSTALL, 0);
        break;
    case WM_WINDOWPOSCHANGING:
        if (state.created)
            ((LPWINDOWPOS) lParam)->flags = (
                SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE |
                SWP_NOREDRAW | SWP_NOREPOSITION
            );
        else
            state.created = 1;
        break;
    case WM_COMMAND:
        switch (LOWORD (wParam)) {
        case ID_INSTALL:
            /*
             * Here is where the real work takes place
             * Collect the user information
             * Do the installation
             * Update the registry
             */
            CreateWizard (hWnd, state.hInstance);
            if (state.cancelled) {
                PostMessage (hWnd, WM_DESTROY, 0, 0);
                return 0;
            }
            PostMessage (hWnd, WM_DESTROY, 0, 0);
            break;
        default:
            return DefWindowProc (hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage (0);
        break;
    default:
        return DefWindowProc (hWnd, message, wParam, lParam);
    }
    return 0;
}

int CreateWizard (HWND hwndOwner, HINSTANCE hInst) {
    PROPSHEETPAGE psp[6];
    PROPSHEETHEADER psh;

    FillPage (&psp[0], IDD_WELCOME, TEXT ("Welcome"), Welcome);
    FillPage (&psp[1], IDD_LICENSE, TEXT ("License Agreement"), License);
    FillPage (&psp[2], IDD_NOTES, TEXT ("Notes"), Notes);
    FillPage (&psp[3], IDD_ROOT, TEXT ("Root Location"), Root_Dest);
    FillPage (&psp[4], IDD_HOME, TEXT ("Home Location"), Home_Dest);
    FillPage (&psp[5], IDD_INSTALL, TEXT ("Finish Installation"), Install);

    psh.dwSize = sizeof (PROPSHEETHEADER);
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_WIZARD | PSH_NOAPPLYNOW;
    psh.hwndParent = hwndOwner;
    psh.hInstance = state.hInstance;
    psh.pszIcon = NULL;
    psh.pszCaption = (LPSTR) TEXT ("Product Install");
    psh.nPages = sizeof (psp) / sizeof (PROPSHEETPAGE);
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE) &psp;
    psh.pfnCallback = NULL;

    return PropertySheet (&psh);
}

void FillPage (PROPSHEETPAGE *psp, int id, LPCTSTR title, DLGPROC proc) {
    psp->dwSize = sizeof (PROPSHEETPAGE);
    psp->dwFlags = 0;
    psp->hInstance = state.hInstance;
    psp->pszTemplate = MAKEINTRESOURCE (id);
    psp->pszIcon = NULL;
    psp->pszTitle = title;
    psp->pfnDlgProc = proc;
    psp->lParam = 0;
    psp->pfnCallback = NULL;
    psp->pcRefParent = NULL;
}

BOOL APIENTRY Welcome (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    switch (message) {
    case WM_INITDIALOG:
	if (!is_admin())
	{
	        MessageBox (hDlg, "Install UWIN from an account with Administrators privilege", "UWIN Setup Critical Error", 0);
            	PostQuitMessage (0);
            	return FALSE;
	}
        SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_RESET:
            state.cancelled = 1;
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (GetParent (hDlg), PSWIZB_NEXT);
            break;
        case PSN_WIZNEXT:
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY License (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
        if (!LoadFmFile (hDlg, LICENSE_TEXT, "license.txt")) {
            PostQuitMessage (0);
            return FALSE;
        }
        return FALSE;
        break;
    case WM_COMMAND:
        // LOWORD added for portability
        switch (LOWORD (wParam)) {
        case IDC_LICENSE_ACCEPT:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK | PSWIZB_NEXT);
            return 0;
        case IDC_LICENSE_DECLINE:
            state.cancelled = 1;
            PostQuitMessage (0);
            return 0;
        }
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK);
            break;
        case PSN_WIZBACK:
            break;
        case PSN_WIZNEXT:
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY UserInfo (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_RESET:
            state.cancelled = 1;
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            break;
        case PSN_SETACTIVE:
	    break;
        case PSN_WIZBACK:
            break;
        case PSN_WIZNEXT:
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}


BOOL APIENTRY Notes (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
        if (!LoadFmFile (hDlg, NOTES_TEXT, "relnotes.txt")) {
            PostQuitMessage (0);
            return FALSE;
        }
        return FALSE;
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK | PSWIZB_NEXT);
            break;
        case PSN_WIZBACK:
            break;
        case PSN_WIZNEXT:
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY Root_Dest (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
	uwin_getkeys(state.release, state.rootpath, sizeof(state.rootpath), 0, 0);
        SendMessage (GetDlgItem (
            hDlg, IDE_PATH_ROOT
        ), WM_SETTEXT, 0, (LPARAM) state.rootpath);
        break;
    case WM_COMMAND:
        switch (LOWORD (wParam)) {
        case IDC_BROWSE_ROOT:
            if (BrowseFolder (hDlg, state.rootpath, "Select Root Folder"))
                SendMessage (GetDlgItem (
                    hDlg, IDE_PATH_ROOT
                ), WM_SETTEXT, 0, (LPARAM) state.rootpath);
            break;
        case IDE_PATH_ROOT:
            GetDlgItemText (hDlg, IDE_PATH_ROOT, state.rootpath, MAX_PATH);
            break;
        }
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_RESET:
            state.cancelled = 1;
            lstrcpy (state.rootpath, TEXT(""));
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK | PSWIZB_NEXT);
            SendMessage (GetDlgItem (
                hDlg, IDE_PATH_ROOT
            ), WM_SETTEXT, 0, (LPARAM) state.rootpath);
            break;
        case PSN_WIZBACK:
            SendMessage (GetDlgItem (
                hDlg, IDE_PATH_ROOT
            ), WM_SETTEXT, 0, (LPARAM) state.rootpath);
            break;
        case PSN_WIZNEXT:
            SendDlgItemMessage (
                hDlg, IDE_PATH_ROOT, WM_GETTEXT, (WPARAM) MAX_PATH,
                (LPARAM) state.rootpath
            );
            if (!MakeADir (hDlg, state.rootpath, TRUE, FALSE)) {
                SetWindowLong (hDlg, DWL_MSGRESULT, -1);
                return -1;
            }
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY Home_Dest (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
	uwin_getkeys(state.release, 0, 0, state.homepath, sizeof(state.homepath));
        SendMessage (GetDlgItem(
            hDlg, IDE_PATH_HOME
        ), WM_SETTEXT, 0, (LPARAM) state.homepath);
        break;
    case WM_COMMAND:
        switch (LOWORD (wParam)) {
        case IDC_BROWSE_HOME:
            if (BrowseFolder (hDlg, state.homepath, "Select Home Folder"))
                SendMessage (GetDlgItem (
                    hDlg, IDE_PATH_HOME
                ), WM_SETTEXT, 0, (LPARAM) state.homepath);
            break;
        case IDE_PATH_HOME:
            GetDlgItemText (hDlg, IDE_PATH_HOME, state.homepath, MAX_PATH);
            break;
        }
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_RESET:
            state.cancelled = 1;
            lstrcpy (state.homepath, TEXT (""));
            SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK | PSWIZB_NEXT);
            SendMessage (GetDlgItem (
                hDlg, IDE_PATH_HOME
            ), WM_SETTEXT, 0, (LPARAM) state.homepath);
            break;
        case PSN_WIZBACK:
            SendMessage (GetDlgItem (
                hDlg, IDE_PATH_HOME
            ), WM_SETTEXT, 0, (LPARAM) state.homepath);
            break;
        case PSN_WIZNEXT:
            SendDlgItemMessage (
                hDlg, IDE_PATH_HOME, WM_GETTEXT, (WPARAM) MAX_PATH,
                (LPARAM) state.homepath
            );
            if (!MakeADir (hDlg, state.homepath, TRUE, TRUE)) {
                SetWindowLong (hDlg, DWL_MSGRESULT, -1);
                return -1;
            }
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY Install (HWND hDlg, UINT message, UINT wParam, LONG lParam) {
    HWND phdlg;

    phdlg = GetParent (hDlg);
    switch (message) {
    case WM_INITDIALOG:
        break;
    case WM_NOTIFY:
        switch (((NMHDR FAR *) lParam)->code) {
        case PSN_KILLACTIVE:
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            return TRUE;
            break;
        case PSN_RESET:
            state.cancelled = 1;
            SetWindowLong (hDlg, DWL_MSGRESULT, FALSE);
            break;
        case PSN_SETACTIVE:
            PropSheet_SetWizButtons (phdlg, PSWIZB_BACK | PSWIZB_FINISH);
            break;
        case PSN_WIZBACK:
            break;
        case PSN_WIZFINISH:
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL APIENTRY LoadFmFile (HWND hDlg, int id, char *name) {
    char file[MAX_PATH], msg[MAX_PATH];
    HANDLE hFile;
    LPSTR bufp;
    DWORD bufn, bufl;

    // Look for file in directory we launched from
    GetModuleFileName (NULL, file, _MAX_PATH);
    *(strrchr (file, '\\') + 1) = '\0';
    strcat (file, name);
    if ((hFile = CreateFile (
        &file[0], GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    )) == INVALID_HANDLE_VALUE) {
        wsprintf (msg, "Error opening file: %s", file);
        MessageBox (hDlg, msg, "UWIN Setup Critical Error", 0);
        return FALSE;
    }
    bufl = GetFileSize (hFile, NULL) ;
    if (!(bufp = malloc ((bufl + 1)))) {
        wsprintf (msg, "Error allocating memory");
        CloseHandle (hFile);
        MessageBox (hDlg, msg, "UWIN Setup Critical Error", 0);
        free (bufp);
        return FALSE;
    }
    if (!ReadFile (hFile, bufp, bufl, &bufn, NULL)) {
        wsprintf (msg, "Error reading license file: %s", file);
        free (bufp);
        CloseHandle (hFile);
        MessageBox (hDlg, "UWIN Setup Critical Error", msg, 0);
        return FALSE;
    }
    CloseHandle (hFile);
    bufp[bufl] = '\0';
    SetDlgItemText (hDlg, id, bufp);
    free (bufp);
    return TRUE;
}

BOOL BrowseFolder (HWND hWnd, char *path, const char *msg) {
    BROWSEINFO bi;
    LPITEMIDLIST iil = NULL;

    bi.hwndOwner = hWnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = path;
    bi.lpszTitle = msg;
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    bi.lpfn = NULL;
    bi.lParam = 0;
    bi.iImage = 0;
    if (!(iil = SHBrowseForFolder (&bi)))
        return FALSE;
    SHGetPathFromIDList (iil, path);
    return TRUE;
}

BOOL MakeADir (HWND hDlg, char *path, int createflag, int overwriteflag) {
    DWORD attr;

    if ((attr = GetFileAttributes (path)) == 0xffffffff) {
        if (!createflag) {
            MessageBox (
                hDlg, "Folder does not exist", TEXT ("UwinSetup"),
                MB_OK | MB_APPLMODAL
            );
            return FALSE;
        }
        if (CreateDirectory (path, security_attributes()))
            return TRUE;
        MessageBox (
            hDlg, "Folder could not be created",
            TEXT ("UwinSetup"), MB_OK | MB_APPLMODAL
        );
        return FALSE;
    } else if(!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBox (
            hDlg, "Path is not a folder",
            TEXT ("UwinSetup"), MB_OK | MB_APPLMODAL
        );
        return FALSE;
    } else {
        if (overwriteflag)
            return TRUE;
        switch (MessageBox (
            hDlg, "Folder already exists, do you want UWIN to overwrite it?",
            TEXT ("UwinSetup"), MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL
        )) {
        case IDYES:
            return TRUE;
            break;
        case IDNO:
            return FALSE;
            break;
        }
    }
    return FALSE;
}
