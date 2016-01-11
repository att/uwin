/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
#pragma prototyped

#include	<stdlib.h>
#include	<windows.h>
#include	<stdio.h>
#include	<uwin_keys.h>

#ifndef OFFSET
#    define OFFSET		0
#endif

static void message(const char *string)
{
	HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);
	int n;
	WriteFile(out,string,strlen(string),&n,NULL);
	WriteFile(out,"\n",1,&n,NULL);
}

static void error_exit(const char *string)
{
	message(string);
	exit(1);
}


static int runksh(HANDLE hp,char *argv[])
{
	PROCESS_INFORMATION	pinfo;
	STARTUPINFO		sinfo;
	char			command[MAX_PATH], path[MAX_PATH];
	char			*args,**av,*cp,tmpdir[MAX_PATH];
	void			*nenv=0,*env = GetEnvironmentStrings();
	int			n,size,cflags=NORMAL_PRIORITY_CLASS;
	HKEY			key,root=HKEY_LOCAL_MACHINE;
#ifdef NOWIN
	cflags |= CREATE_NO_WINDOW;
#endif
#ifdef DETACH
	cflags |= DETACHED_PROCESS;
#endif
	strcpy(path,UWIN_KEY);
	cp = path + strlen(path)+1;
	size = sizeof(path)-sizeof(UWIN_KEY);
	if(RegOpenKeyEx(root,path,0,KEY_READ|KEY_EXECUTE,&key) || RegQueryValueEx(key,UWIN_KEY_REL, NULL, &n, cp,&size) || n!=REG_SZ)
		error_exit("cannot find UWIN registry keys");
	RegCloseKey(key);
	cp[-1] = '\\';
	cp += size;
	*cp++ = '\\';
	strcpy(cp, UWIN_KEY_INST);
	if(!RegOpenKeyEx(root,path,0,KEY_READ|KEY_EXECUTE,&key))
	{
		size = sizeof(tmpdir);
		if(!RegQueryValueEx(key,UWIN_KEY_ROOT, NULL, &n, tmpdir,&size) && n==REG_SZ)
		{
			size--;
			if(tmpdir[size-1]=='\\' && tmpdir[size]==0)
				tmpdir[--size] = 0;
		}
		else
			error_exit("cannot find UWIN install root");
		RegCloseKey(key);
		strcpy(&tmpdir[size],"\\usr\\bin");
	}
	else
		error_exit("cannot find UWIN version registry key");
	strcpy(command,"ksh");
	if(argv[1])
	{
		for(size=0,cp= (char*)env; *cp; size++)
		{
			while(*cp++)
				size++;
		}
		for(n=size+7,av=argv; cp= *av++;)
			n += strlen(cp)+3;
		nenv = malloc(n);
		memcpy(nenv,env,size);
		cp = (char*)nenv+size;
		memcpy(cp,"ARGS=",5);
		for(cp += 5, av=argv; *++av;)
		{
			n = sprintf(cp, "'%s' ",*av);
			cp += n;
		}
		cp[-1] = 0;
		*cp = 0;
	}
	ZeroMemory (&sinfo,sizeof(sinfo));
	sinfo.dwFlags = STARTF_USESTDHANDLES;
	sinfo.hStdInput = hp;
	sinfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	sinfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);;
	sprintf(path,"%s\\%s.exe\0",tmpdir,command);
	if (CreateProcess(path,command,NULL,NULL,TRUE,cflags,nenv,NULL,&sinfo,&pinfo))
	{
		CloseHandle(pinfo.hThread);
#ifdef NOWAIT
		exit(0);
#else
		WaitForSingleObject(pinfo.hProcess, INFINITE);
		if(!GetExitCodeProcess(pinfo.hProcess,&n))
			message("Unable to get stage1 exit status");
		CloseHandle(pinfo.hProcess);
		if(n)
		{
			message("ksh returned non-zero exit status");
			return(1);
		}
#endif
	}
	else 
	{
		sprintf(path, "Process creation failed for ksh\n Stage one Err = %d\n", GetLastError());
		message(path);
		return(1);
	}
}

int main(int argc, char *argv[])
{
	SECURITY_ATTRIBUTES sattr;
	HANDLE hp;
	int flags=0;
	char name[256];;
	if(!GetModuleFileName(0,name, sizeof(name)))
		error_exit("GetModuleFileName fails");
        ZeroMemory ((void*)&sattr, sizeof (sattr));
        sattr.nLength = sizeof(sattr);
        sattr.lpSecurityDescriptor=0;
        sattr.bInheritHandle = TRUE;
	hp = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, &sattr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,NULL); 
	if(hp==INVALID_HANDLE_VALUE)
		error_exit("CreateFile");
	if(SetFilePointer(hp, (DWORD)OFFSET, NULL, SEEK_SET)< 0)
		error_exit("SetFilePointer");
	runksh(hp,argv);
	return(0);
}
