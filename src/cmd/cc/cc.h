#ifdef	__INTERIX
#   include <interix/interix.h>
#   include <interix/security.h>

    typedef wchar_t* LPTSTR;
#   define	__TEXT(q)	L##q
#   define	TEXT(q)	__TEXT(q)

#   define	REGPFX	TEXT("\\Registry\\Machine\\")

#   define	REG_SZ	WIN_REG_SZ
#   define	RegQueryValueEx(a,b,c,d,e,f)	(0)
#   define	RegCloseKey(a)

#   define _stricmp		strcasecmp
#   define _strnicmp		strncasecmp
#   define DeleteFile(x)	unlink(posix_pathname(x))
#   define spawnv(a,b)		spawnve(a,b,environ)
#   define spawnl(a,b,c,d,e,f)	spawnlp(a,b,c,d,e,f)

extern pid_t spawnve(const char*,char* const [],char* const []);
extern pid_t spawnlp(const char*,const char*,...);

#   define	MSDIR(drv, dir) "/dev/fs/" drv "/" dir

#   define OMF_LIBHDR	0xF0
#   define OMF_LIBDHD	0xF1
#   define OMF_MODEND	0x8A

extern int sprintf(char *,const char*,...);

#else

#   include <windows.h>
#   include <omf.h>

#   define REGPFX

#   ifdef __CYGWIN__
#	define  MSDIR(drv, dir)	"/cygdrive/" drv "/" dir
#   else
#	define  MSDIR(drv, dir)	"/" drv "/" dir
#   endif

#endif

#ifdef	_UWIN
#include <uwin.h>
#endif

#ifndef IMAGE_FILE_MACHINE_I386
#define	IMAGE_FILE_MACHINE_I386		0x014c
#define	IMAGE_FILE_MACHINE_ALPHA	0x0184

typedef struct
{
	unsigned short	Machine;
	unsigned short	NumberOfSections;
	unsigned long	TimeDateStamp;
	unsigned long	PointerToSymbolTable;
	unsigned long	NumberOfSymbols;
	unsigned short	SizeOfOptionalHeader;
	unsigned short	Characteristics;
} IMAGE_FILE_HEADER;
typedef struct
{
	unsigned char	Name[8];
	union {
		unsigned long	PhysicalAddress;
		unsigned long	VirtualSize;
	} Misc;
	unsigned long	VirtualAddress;
	unsigned long	SiteOfRawData;
	unsigned long	PointerToRawData;
	unsigned long	PointerToRelocations;
	unsigned long	PointerToLinenumbers;
	unsigned short	NumberOfRelocations;
	unsigned short	NumberOfLinenumbers;
	unsigned long	Characteristics;
} IMAGE_SECTION_HEADER;
#endif

#ifndef	IMAGE_ARCHIVE_START_SIZE
#define	IMAGE_ARCHIVE_START_SIZE	8
#define	IMAGE_ARCHIVE_START	"!<arch>\n"

typedef	struct
{
	unsigned char	Name[16];
	unsigned char	Date[12];
	unsigned char	UserID[6];
	unsigned char	GroupID[6];
	unsigned char	Mode[8];
	unsigned char	Size[10];
	unsigned char	EndHeader[2];
} IMAGE_ARCHIVE_MEMBER_HEADER;
#endif

/* just to make sure */

#ifndef ETXTBSY
#define	ETXTBSY	EBUSY
#endif

