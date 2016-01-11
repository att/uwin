# Microsoft Developer Studio Project File - Name="testcpl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=testcpl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "testcpl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "testcpl.mak" CFG="testcpl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testcpl - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "testcpl - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""$/UWIN GUI Tool", EUHAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "testcpl - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 4
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_USRDLL" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 user32.lib advapi32.lib netapi32.lib /nologo /subsystem:windows /dll /machine:I386 /out:"Release\uwin.cpl"

!ELSEIF  "$(CFG)" == "testcpl - Win32 Debug"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 4
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_USRDLL" /FR /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib advapi32.lib netapi32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"c:\winnt\system32\uwin.cpl" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "testcpl - Win32 Release"
# Name "testcpl - Win32 Debug"
# Begin Group "Source Files"

SOURCE=.\ConfigConsoleDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfigMsgDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfigResource.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfigSemDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfigShmDlg.cpp
# End Source File
# Begin Source File

SOURCE=.\globals.cpp
# End Source File
# Begin Source File

SOURCE=.\JkInetdConf.cpp
# End Source File
# Begin Source File

SOURCE=.\RegistryOps.cpp
# End Source File
# Begin Source File

SOURCE=.\regupdate.cpp
# End Source File
# Begin Source File

SOURCE=.\SortClass.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\SysInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\testcpl.cpp
# End Source File
# Begin Source File

SOURCE=.\testcpl.def
# End Source File
# Begin Source File

SOURCE=.\testcpl.rc

!IF  "$(CFG)" == "testcpl - Win32 Release"

!ELSEIF  "$(CFG)" == "testcpl - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\UcsInstall.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinClient.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinGeneral.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinInetd.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinSystem.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinTelnet.cpp
# End Source File
# Begin Source File

SOURCE=.\UwinUms.cpp
# End Source File
# End Group
# Begin Group "Header Files"

SOURCE=.\ConfigMsgDlg.h
# End Source File
# Begin Source File

SOURCE=.\ConfigResource.h
# End Source File
# Begin Source File

SOURCE=.\ConfigSemDlg.h
# End Source File
# Begin Source File

SOURCE=.\ConfigShmDlg.h
# End Source File
# Begin Source File

SOURCE=.\ConfigSys.h
# End Source File
# Begin Source File

SOURCE=.\globals.h
# End Source File
# Begin Source File

SOURCE=.\JkInetdConf.h
# End Source File
# Begin Source File

SOURCE=.\RegistryOps.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\SortClass.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\SysInfo.h
# End Source File
# Begin Source File

SOURCE=.\testcpl.h
# End Source File
# Begin Source File

SOURCE=.\UcsInstall.h
# End Source File
# Begin Source File

SOURCE=.\UwinClient.h
# End Source File
# Begin Source File

SOURCE=.\UwinGeneral.h
# End Source File
# Begin Source File

SOURCE=.\UwinInetd.h
# End Source File
# Begin Source File

SOURCE=.\UwinSystem.h
# End Source File
# Begin Source File

SOURCE=.\UwinTelnet.h
# End Source File
# Begin Source File

SOURCE=.\UwinUms.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\testcpl.rc2
# End Source File
# Begin Source File

SOURCE=.\toolbar1.bmp
# End Source File
# Begin Source File

SOURCE=..\doc\uwin.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
