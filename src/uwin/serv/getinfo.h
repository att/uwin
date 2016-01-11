#ifndef _GETINFO_H
#define _GETINFO_H	1

#include <lm.h>
#include <sfio.h>

#define LOG_LEVEL	0x00f
#define LOG_ALL		0x010
#define LOG_EVENT	0x020
#define LOG_STDERR	0x040
#define LOG_SYSTEM	0x080
#define LOG_USAGE	0x100

#define logerr(d,a ...)	logsrc((d)|LOG_SYSTEM,0,__LINE__,a)
#define logmsg(d,a ...)	logsrc(d,0,__LINE__,a)

extern const char*	log_command;
extern const char*	log_service;

extern int		log_level;

extern void		logopen(const char*, int);
extern void		logclose(void);
extern int		logsrc(int, const char*, int, const char*, ...);

#define MAXNAME 80

typedef NET_API_STATUS (NET_API_FUNCTION *wk_getinfo_def)(char*, DWORD,unsigned char
**);
typedef NET_API_STATUS (NET_API_FUNCTION *query_info_def)(const wchar_t*, DWORD,DWORD, DWORD, DWORD, DWORD*,  void**);
typedef NET_API_STATUS (NET_API_FUNCTION *serv_enum_def)(wchar_t*, DWORD,unsigned char**,DWORD,DWORD*,DWORD*,DWORD,wchar_t*,DWORD*);

typedef NET_API_STATUS (NET_API_FUNCTION *api_buffer_free_def)(LPVOID);   
typedef NET_API_STATUS (NET_API_FUNCTION *get_dcname_def)(const wchar_t*, const wchar_t*, LPBYTE*);

extern FARPROC		getapi_addr(const char*, const char*,HMODULE*); 
extern int		getinfo(int);
extern int		mangle(char *name, char *system, int);
extern int		parse_username(char*);
extern SECURITY_DESCRIPTOR* nulldacl(void);

#define IsPdc() 		getinfo(1)
#define IsBdc() 		getinfo(2)
#define IsDomain() 		getinfo(3)
#define IsDc() 			getinfo(4)

#endif
