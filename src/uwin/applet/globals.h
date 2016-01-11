#ifndef _GLOBALS_H
#define _GLOBALS_H

//#define REGISTRY 0 // Indicates that values must be taken from registry
//#define DEFAULT 1  // Indicates that only default values must be taken

#define UWIN_MASTER 1
#define UWIN_TELNETD 2

#define STATE_SERVICE 1
#define TYPE_SERVICE  2
#define CONTROL_SERVICE 3

#include "stdafx.h"
#include <winsvc.h>

int getregpath(char *path);
int get_config_ums(int);
int get_config_telnetd(int flag);
int set_config_ums(int flag, int opt);
int set_config_telnetd(int flag, int op);
int installservice(int  serviceno, char *instPath);
int deleteservice(char *servname);
void ErrorBox(const char*, const char*, int);
void convert_state_to_string(char *str, DWORD code, int opt);
int startservice(char *servname);
int stopservice(char *servname);
int query_servicestatus(char *servname,SERVICE_STATUS *stat);
int getservice_displayname(char *servname,char *dispname);
int uwin_service_status(char *servname,SERVICE_STATUS *stat);
int query_service_config(char *servname,QUERY_SERVICE_CONFIG *lpqscBuf);
int config_status_to_string(char *str, DWORD code, int opt);
int telnet_regkeys();
int ums_regkeys();
int set_service_config(char *servname, QUERY_SERVICE_CONFIG *config);

#endif /* _GLOBALS_H */
