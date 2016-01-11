/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#ifndef _NTCLONE_
#define _NTCLONE_	1
#ifndef _NTDEF_
typedef LONG NTSTATUS, *PNTSTATUS;
#endif

typedef DWORD CLIENT_ID;
// symbols
typedef struct _SECTION_IMAGE_INFORMATION
{
    PVOID TransferAddress;
    ULONG ZeroBits;
    SIZE_T MaximumStackSize;
    SIZE_T CommittedStackSize;
    ULONG SubSystemType;
    union
    {
        struct
        {
            USHORT SubSystemMinorVersion;
            USHORT SubSystemMajorVersion;
        };
        ULONG SubSystemVersion;
    };
    ULONG GpValue;
    USHORT ImageCharacteristics;
    USHORT DllCharacteristics;
    USHORT Machine;
    BOOLEAN ImageContainsCode;
    union
    {
        UCHAR ImageFlags;
        struct
        {
            UCHAR ComPlusNativeReady : 1;
            UCHAR ComPlusILOnly : 1;
            UCHAR ImageDynamicallyRelocated : 1;
            UCHAR ImageMappedFlat : 1;
            UCHAR Reserved : 4;
        };
    };
    ULONG LoaderFlags;
    ULONG ImageFileSize;
    ULONG CheckSum;
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

typedef struct _RTL_USER_PROCESS_INFORMATION
{
    ULONG Length;
    HANDLE Process;
    HANDLE Thread;
    CLIENT_ID ClientId;
    SECTION_IMAGE_INFORMATION ImageInformation;
} RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION; 

// begin_rev
#define RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED 0x00000001
#define RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES 0x00000002
#define RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE 0x00000004 // don't update synchronization objects
// end_rev

// private
NTSYSAPI
NTSTATUS
NTAPI
RtlCloneUserProcess(
    __in ULONG ProcessFlags,
    __in_opt PSECURITY_DESCRIPTOR ProcessSecurityDescriptor,
    __in_opt PSECURITY_DESCRIPTOR ThreadSecurityDescriptor,
    __in_opt HANDLE DebugPort,
    __out PRTL_USER_PROCESS_INFORMATION ProcessInformation
    ); 

#define RTL_CLONE_PARENT	0
#define RTL_CLONE_CHILD		297

typedef NTSYSAPI NTSTATUS (NTAPI *RtlCreateUserThread_f)(HANDLE,SECURITY_DESCRIPTOR*,BOOL,ULONG, ULONG*, ULONG*, void*, void*, HANDLE*, void**); 
typedef NTSYSAPI NTSTATUS (NTAPI *RtlCloneUserProcess_f)(ULONG,PSECURITY_DESCRIPTOR,PSECURITY_DESCRIPTOR,HANDLE,PRTL_USER_PROCESS_INFORMATION);

static RtlCreateUserThread_f clonet;
static RtlCloneUserProcess_f clonep;

#endif /* _NTCLONE_ */
