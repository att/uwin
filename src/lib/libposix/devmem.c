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
/* Author:
 * Karsten Fleischer
 * Omnium Software Engineering
 * Rennbaumstr. 32
 * D-51379 Leverkusen
 * Germany
 *
 * <K.Fleischer@omnium.de>
 */

#include <windows.h>
#include "uwindef.h"
#include "native.h"

static NTSTATUS (__stdcall *NtUnmapViewOfSection)(HANDLE, PVOID) = 0;
static NTSTATUS (__stdcall *NtOpenSection)(HANDLE, ACCESS_MASK,
					   POBJECT_ATTRIBUTES) = 0;
static NTSTATUS (__stdcall *NtMapViewOfSection)(HANDLE, HANDLE, PVOID, ULONG,
						ULONG, PLARGE_INTEGER, PULONG,
						SECTION_INHERIT, ULONG, ULONG) = 0;
static NTSTATUS (__stdcall *RtlInitUnicodeString)(PUNICODE_STRING, PCWSTR) = 0;
static ULONG (__stdcall *RtlNtStatusToDosError)(NTSTATUS) = 0;
static NTSTATUS (__stdcall *NtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS,
						      PVOID, ULONG, PULONG) = 0;

static int devmem_api()
{
	if(!NtUnmapViewOfSection && !(NtUnmapViewOfSection =
		(NTSTATUS (__stdcall*)(HANDLE, PVOID))
		getsymbol(MODULE_nt, "NtUnmapViewOfSection")))
		return 0;
	if(!NtOpenSection && !(NtOpenSection =
		(NTSTATUS (__stdcall*)(HANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES))
		getsymbol(MODULE_nt, "NtOpenSection")))
		return 0;
	if(!NtMapViewOfSection && !(NtMapViewOfSection =
		(NTSTATUS (__stdcall*)(HANDLE, HANDLE, PVOID, ULONG, ULONG,
		PLARGE_INTEGER, PULONG, SECTION_INHERIT, ULONG, ULONG))
		getsymbol(MODULE_nt, "NtMapViewOfSection")))
		return 0;
	if(!RtlInitUnicodeString && !(RtlInitUnicodeString =
		(NTSTATUS (__stdcall *)(PUNICODE_STRING, PCWSTR))
		getsymbol(MODULE_nt, "RtlInitUnicodeString")))
		return 0;
	if(!RtlNtStatusToDosError && !(RtlNtStatusToDosError =
		(ULONG (__stdcall *)(NTSTATUS))
		getsymbol(MODULE_nt, "RtlNtStatusToDosError")))
		return 0;
	if(!NtQuerySystemInformation && !(NtQuerySystemInformation =
		(NTSTATUS (__stdcall *)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG,
		PULONG))
		getsymbol(MODULE_nt, "NtQuerySystemInformation")))
		return 0;
	return 1;
}

static int devmem_map(HANDLE h, off64_t *addr, PDWORD len, PDWORD vaddr, int oflags)
{
	NTSTATUS	r;
	PHYSICAL_ADDRESS	base;
	unsigned long	am;

	switch(oflags & O_ACCMODE)
	{
	case O_RDONLY:
		am = PAGE_READONLY;
		break;
	case O_WRONLY:
	case O_RDWR:
		am = PAGE_READWRITE;
		break;
	}

	*vaddr = 0;
	base.QuadPart = (ULONGLONG) *addr;
	r = NtMapViewOfSection(h, (HANDLE) -1, (PVOID) vaddr, 0, *len, &base, len, ViewShare, 0, am);
	if(!NT_SUCCESS(r))
	{
		logerr(0, "NtMapViewOfSection failed");
		errno = unix_err(RtlNtStatusToDosError(r));
		return 0;
	}

	*addr = base.QuadPart;
	return 1;
}

static void devmem_unmap(DWORD addr)
{
	NTSTATUS	r;

	r = NtUnmapViewOfSection((HANDLE) -1, (PVOID)addr);

	if(!NT_SUCCESS(r))
	{
		logerr(0, "NtUnmapViewOfSection failed");
		errno = unix_err(RtlNtStatusToDosError(r));
	}
}

static off64_t devmem_lseek(int fd, Pfd_t* fpd, off64_t offset, int whence)
{
	Pdevmem_t	*pm = (Pdevmem_t*) dev_ptr(fpd->devno);

	switch(whence)
	{
	case SEEK_SET:
		fpd->extra64 = offset;
		break;
	case SEEK_CUR:
		fpd->extra64 += offset;
		break;
	case SEEK_END:
		fpd->extra64 = pm->max_addr + offset;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if(fpd->extra64 < (unsigned long long)pm->min_addr || fpd->extra64 >= (unsigned long long)pm->max_addr)
	{
		errno = EINVAL;
		return -1;
	}

	return fpd->extra64;
}

static ssize_t devmem_read(int fd, Pfd_t* fpd, char *buff, size_t asize)
{
	off64_t		offset = fpd->extra64;
	DWORD		size;
	DWORD		sz;
	DWORD		vaddr;
	Pdevmem_t*	pdm = (Pdevmem_t *) dev_ptr(fpd->devno);

	if(asize == 0 || offset >= pdm->max_addr)
		return -1;
	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	sz = size = (DWORD)asize;

	if(offset + size >= pdm->max_addr)
		size = (int)(pdm->max_addr - offset);

	if(devmem_map(Phandle(fd), &offset, &sz, &vaddr, fpd->oflag))
	{
		memcpy(buff, (char*)vaddr + (fpd->extra64 - offset), size);
		devmem_unmap(vaddr);
	}
	else
		return -1;

	return size;
}

static ssize_t devmem_write(int fd, Pfd_t* fpd, char *buff, size_t asize)
{
	off64_t		offset = fpd->extra64;
	DWORD		size;
	DWORD		sz;
	DWORD		vaddr;
	Pdevmem_t*	pdm = (Pdevmem_t *) dev_ptr(fpd->devno);

	if(asize == 0 || offset >= pdm->max_addr)
		return 0;
	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	sz = size = (DWORD)asize;

	if(offset + size >= pdm->max_addr)
		size = (int)(pdm->max_addr - offset);

	if(devmem_map(Phandle(fd), &offset, &sz, &vaddr, fpd->oflag))
	{
		memcpy((char*)vaddr + (fpd->extra64 - offset), buff, size);
		devmem_unmap(vaddr);
	}
	else
		return 0;

	return size;
}

static int devmem_close(int fd, Pfd_t* fpd, int noclose)
{
	Pdev_t	*pdev = dev_ptr(fpd->devno);
	int	r;

	if(fpd->refcount> pdev->count)
		badrefcount(fd, fpd ,pdev->count);
	r = fileclose(fd, fpd, noclose);
	if(pdev->count<=0)
		;
	else
		InterlockedDecrement(&pdev->count);
	return r;
}

static HANDLE devmem_select(int fd, Pfd_t* fdp, int type, HANDLE hp)
{
        return NULL;
}

#define	DEVMEM_MINOR		0
#define	DEVPORT_MINOR		1
#define	DEVKMEM_MINOR		2
#define	DEVALLKMEM_MINOR	3

static HANDLE devmem_open(Devtab_t *pdevtab, Pfd_t *fpd, Path_t *ip, int oflags,
			  HANDLE *extra)
{
	HANDLE	hp;
	int	blkno, minor = ip->name[1];
	unsigned short *blocks = devtab_ptr(Share->chardev_index, DEVMEM_MAJOR);
	NTSTATUS	r;
	UNICODE_STRING	ucstr;
	OBJECT_ATTRIBUTES	attr;
	SYSTEM_BASIC_INFORMATION	sbi;
	Pdevmem_t	*pdm;
	ACCESS_MASK	am;

	if(Share->Platform != VER_PLATFORM_WIN32_NT)
	{
		logerr(0, "/dev/mem not supported on non NT systems");
		goto enodev;
	}
	if(!devmem_api())
	{
		logerr(0, "failed to get ntdll.dll API functions");
		goto enodev;
	}
	if(minor > DEVALLKMEM_MINOR)
	{
		logerr(0, "illegal minor device number");
		goto enodev;
	}
	if(minor > DEVPORT_MINOR)
	{
		logerr(0, "minor device not implemented yet");
		goto enodev;
	}
	if(blkno = blocks[minor])
	{	/* device already in use, increase device usage counter */
		pdm = (Pdevmem_t*)dev_ptr(blkno);
		InterlockedIncrement(&pdm->count);
	}
	else
	{	/* allocate new device block, fill in info */
		if((blkno = block_alloc(BLK_PDEV)) == 0)
			return(0);

		pdm = (Pdevmem_t*)dev_ptr(blkno);
		ZeroMemory((void *)pdm, BLOCK_SIZE-1);

		pdm->major = DEVMEM_MAJOR;
		pdm->minor = minor;

		blocks[minor] = blkno;

		/* get physical memory size */

		r = NtQuerySystemInformation(SystemBasicInformation, &sbi, sizeof(sbi), 0);

		if(!NT_SUCCESS(r))
		{
			logerr(0, "NtQuerySystemInformation failed");
			goto nterr;
		}

		switch(minor)
		{
		case DEVMEM_MINOR:
			pdm->min_addr = 0; // sbi.LowestPhysicalPage * sbi.PhysicalPageSize;
			pdm->max_addr = sbi.HighestPhysicalPage * sbi.PhysicalPageSize;
			break;
		case DEVPORT_MINOR:
			pdm->min_addr = 0;
			pdm->max_addr = 0x10000;
			break;
		default:
			pdm->min_addr = pdm->max_addr = 0;
			break;
		}
	}

	fpd->devno = blkno;
	fpd->extra64 = 0;

	RtlInitUnicodeString(&ucstr, L"\\Device\\PhysicalMemory");
	InitializeObjectAttributes(&attr, &ucstr, OBJ_CASE_INSENSITIVE | OBJ_INHERIT, 0, 0);

	switch(oflags & O_ACCMODE)
	{
	case O_RDONLY:
		am = SECTION_MAP_READ;
		break;
	case O_WRONLY:
	case O_RDWR:
		am = SECTION_MAP_READ | SECTION_MAP_WRITE;
		break;
	}

	r = NtOpenSection(&hp, am, &attr);

	if(!NT_SUCCESS(r))
	{
		logerr(0, "NtOpenSection failed");
		goto nterr;
	}

	return hp;

nterr:
	errno = unix_err(RtlNtStatusToDosError(r));
	return 0;

enodev:
	errno = ENODEV;
	return 0;
}

void init_dev_mem(Devtab_t* dp)
{
	filesw(FDTYPE_MEM, devmem_read, devmem_write, devmem_lseek,
	       filefstat, devmem_close, devmem_select);
	dp->openfn = devmem_open;
}
