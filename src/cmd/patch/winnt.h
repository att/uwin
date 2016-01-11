
/* $XConsortium: winnt.h,v 1.1 94/09/09 20:27:49 kaleb Exp $ */


#define stat _stat
#define dev_t _dev_t
#define ino_t _ino_t
#define chdir _chdir
#define chmod(p1,p2) _chmod(p1,_S_IWRITE)
#define open _open
#define close _close
#define creat(p1,p2) _creat(p1,_S_IWRITE)
#define fileno _fileno
#define fstat _fstat
#define isatty _isatty
#define lseek _lseek
#define mktemp _mktemp
#define pclose _pclose
#define popen _popen
#define read _read
#define write _write
#define unlink _unlink
#define sys_errlist _sys_errlist
#define sys_nerr (*_sys_nerr_dll)

