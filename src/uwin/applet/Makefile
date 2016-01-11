/*
 * uwin control panel applet
 */

:PACKAGE: - uwin mfc crt

CC = ncc

CCFLAGS = $(CC.DLL)

CPL ==		"uwin.cpl"
LICENSE_ID ==	"$(LICENSEINFO:P=W=query=${id-${type}})"
NDEBUG ==	1
WIN32 ==	1
_WINDOWS ==	1
_WINDLL ==	1
_USRDLL ==	1
stricmp ==	_stricmp

:INSTALL:

uwin - :LIBRARY: testcpl.def StdAfx.cpp globals.cpp regupdate.cpp \
	testcpl.cpp testcpl.rc UcsInstall.cpp UwinClient.cpp UwinTelnet.cpp \
	UwinUms.cpp ConfigConsoleDlg.cpp ConfigMsgDlg.cpp ConfigResource.cpp \
	ConfigSemDlg.cpp ConfigShmDlg.cpp RegistryOps.cpp UwinSystem.cpp \
	ConfigureDaemon.cpp JkInetdConf.cpp SortClass.cpp SysInfo.cpp \
	UwinGeneral.cpp UwinInetd.cpp ConfigMiscDlg.h ConfigMiscDlg.cpp \
	FrameHandler.cpp \
	-luser32 -ladvapi32 -lnetapi32 -lnafxcw-$$(CC.HOSTTYPE)

uwin.cpl :LINK: uwin.so/uwin.dll

$(SYSDIR) :INSTALLDIR: uwin.cpl uwincpl

:INSTALLDIR: standalone

standalone :: standalone.c

uwincpl : standalone (MKADMIN) (MKADMINFLAGS)
	$(MKADMIN) $(MKADMINFLAGS) -o $(<) $(*)

:: RELEASE ReadMe.txt testcpl.dsp testcpl.dsw
