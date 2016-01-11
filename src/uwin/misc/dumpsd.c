#include	<ast.h>
#include	<error.h>
#include	<windows.h>
#include	<uwin.h>

static const char usage[] =
"[-?\n@(#)$Id: dumpsd (AT&T Labs Research) 2011-04-12 $\n]"
USAGE_LICENSE
"[+NAME? dumpsd - dump the contents of a security descriptor]"
"[+DESCRIPTION?\bdumpsd\b writes the contents of security descriptors "
	"for the specified \aobject\as. to standard output.]"
"[+?If no options and no \aobject\as are specified then the security "
	"descriptor for this process is displayed.]"
""
"[T?Also display the process token security descriptor along with the "
	"process security descriptor.]"
"[e?Each \aobject\a is a named event.]"
"[f?Each \aobject\a is a UNIX file name.]"
"[s?Each \aobject\a names a semaphore.]"
"[t?Each \aobject\a token.]"
"[p]:[pid?Dumps the security descriptor of the process with UNIX "
	"process id \apid\a.]"
"[P]:[ntpid?Dumps the security descriptor of the process with native "
	"process id \antpid\a.]"
"\n"
"\n[object] ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Success.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bother\b(1)]"
;

#define FILE_HANDLE	0
#define PROC_HANDLE	1
#define TOK_HANDLE	2
#define THREAD_HANDLE	3
#define EVENT_HANDLE	4
#define SEM_HANDLE	5
#define MUTEX_HANDLE	6
#define KEY_HANDLE	7

static void printcontrol(Sfio_t *iop, SECURITY_DESCRIPTOR_CONTROL mask)
{
	register unsigned int i, first=0;
	char *cp,buff[20];
	for(i=1; i<(1L<<31); i<<=1)
	{
		cp = 0;
		switch(i&mask)
		{
		    case SE_OWNER_DEFAULTED:
			cp = "OWNER_DEFAULTED";
			break;
		    case SE_GROUP_DEFAULTED:
			cp = "GROUP_DEFAULTED";
			break;
		    case SE_DACL_PRESENT:
			cp = "DACL_PRESENT";
			break;
		    case SE_DACL_DEFAULTED:
			cp = "DACL_DEFAULTED";
			break;
		    case SE_SACL_PRESENT:
			cp = "SACL_PRESENT";
			break;
		    case SE_SACL_DEFAULTED:
			cp = "SACL_DEFAULTED";
			break;
		    case SE_DACL_AUTO_INHERIT_REQ:
			cp = "DACL_AUTO_INHERIT_REQ";
			break;
		    case SE_SACL_AUTO_INHERIT_REQ:
			cp = "SACL_AUTO_INHERIT_REQ";
			break;
		    case SE_DACL_AUTO_INHERITED:
			cp = "DACL_AUTO_INHERITED";
			break;
		    case SE_SACL_AUTO_INHERITED:
			cp = "SACL_AUTO_INHERITED";
			break;
		    case SE_DACL_PROTECTED:
			cp = "DACL_PROTECTED";
			break;
		    case SE_SACL_PROTECTED:
			cp = "SACL_PROTECTED";
			break;
		    case SE_RM_CONTROL_VALID:
			cp = "RM_CONTROL_VALID";
			break;
		    case SE_SELF_RELATIVE:
			cp = "SELF_RELATIVE";
			break;
		    case 0:
			continue;
		    default:
			sfsprintf(cp=buff, sizeof(buff),"%x\0",i);
		}
		if(cp)
		{
			if(first++)
				sfputc(iop,'|');
			sfputr(iop,cp,-1);
		}
	}
	sfputc(iop,'\n');
}

static void printperm(Sfio_t *iop,int mask,char *(*fun)(int))
{
	register  unsigned long i;
	register int first=0;
	register char *cp;
	if(!fun)
	{
		sfprintf(iop,"%x",mask);
		return;
	}
	for(i=1; i<(1L<<31); i<<=1)
	{
		cp = 0;
		switch(i&mask)
		{
		    case DELETE:
			cp = "delete";
			break;
		    case READ_CONTROL:
			cp = "read";
			break;
		    case WRITE_DAC:
			cp = "chmod";
			break;
		    case WRITE_OWNER:
			cp = "chown";
			break;
		    case SYNCHRONIZE:
			cp = "sync";
			break;
		    default:
			if(i&mask)
				cp = (*fun)(i&mask);
		}
		if(cp)
		{
			if(first++)
				sfputc(iop,'|');
			sfputr(iop,cp,-1);
		}
	}
}

static char *keyperm(int mask)
{
	switch(mask)
	{
	    case KEY_QUERY_VALUE:
		return("query_value");
	    case KEY_SET_VALUE:
		return("set_value");
	    case KEY_CREATE_SUB_KEY:
		return("create_subkey");
	    case KEY_ENUMERATE_SUB_KEYS:
		return("enumerate_subkeys");
	    case KEY_NOTIFY:
		return("notify");
	    case KEY_CREATE_LINK:
		return("create_link");
	}
	return(0);
}

static char *eventperm(int mask)
{
	switch(mask)
	{
	    case EVENT_MODIFY_STATE:
		return("modify_state");
	    case MUTANT_QUERY_STATE:
		return("query_state");
	}
	return(0);
}

static char *fileperm(int mask)
{
	switch(mask)
	{
	    case FILE_READ_DATA:
		return("read_data");
	    case FILE_WRITE_DATA:
		return("write_data");
	    case FILE_APPEND_DATA:
		return("append");
	    case FILE_READ_EA:
		return("read_perm");
	    case FILE_WRITE_EA:
		return("write_perm");
	    case FILE_EXECUTE:
		return("execute");
	    case FILE_DELETE_CHILD:
		return("delete_child");
	    case FILE_READ_ATTRIBUTES:
		return("read_attr");
	    case FILE_WRITE_ATTRIBUTES:
		return("write_attr");
	}
	return(0);
}

static char *threadperm(int mask)
{
	switch(mask)
	{
	    case THREAD_TERMINATE:
		return("terminate");
	    case THREAD_SUSPEND_RESUME:
		return("suspend");
	    case THREAD_GET_CONTEXT:
		return("get_context");
	    case THREAD_SET_CONTEXT:
		return("set_context");
	    case THREAD_SET_INFORMATION:
		return("setinfo");
	    case THREAD_QUERY_INFORMATION:
		return("query");
	    case THREAD_SET_THREAD_TOKEN:
		return("set_token");
	    case THREAD_IMPERSONATE:
		return("impersonate");
	    case THREAD_DIRECT_IMPERSONATION:
		return("direct_impersonate");
	}
	return(0);
}

static char *procperm(int mask)
{
	switch(mask)
	{
	    case PROCESS_TERMINATE:
		return("terminate");
	    case PROCESS_CREATE_THREAD:
		return("create_thread");
	    case PROCESS_VM_OPERATION:
		return("vmop");
	    case PROCESS_VM_READ:
		return("vmread");
	    case PROCESS_VM_WRITE:
		return("vmwrite");
	    case PROCESS_DUP_HANDLE:
		return("duplicate");
	    case PROCESS_CREATE_PROCESS:
		return("create_proc");
	    case PROCESS_SET_QUOTA:
		return("set_quota");
	    case PROCESS_QUERY_INFORMATION:
		return("query");
	}
	return(0);
}

static char *tokperm(int mask)
{
	switch(mask)
	{
	    case TOKEN_ASSIGN_PRIMARY:
		return("assign");
	    case TOKEN_DUPLICATE:
		return("duplicate");
	    case TOKEN_IMPERSONATE:
		return("impersonate");
	    case TOKEN_QUERY:
		return("query");
	    case TOKEN_QUERY_SOURCE:
		return("qsource");
	    case TOKEN_ADJUST_PRIVILEGES:
		return("adjprivs");
	    case TOKEN_ADJUST_GROUPS:
		return("adjgroups");
	    case TOKEN_ADJUST_DEFAULT:
		return("adjdefault");
	}
	return(0);
}

static char *aceflag(int mask)
{
	switch(mask)
	{
	    case OBJECT_INHERIT_ACE:
		return("oinherit");
	    case CONTAINER_INHERIT_ACE:
		return("cinherit");
	    case NO_PROPAGATE_INHERIT_ACE:
		return("nopropogate");
	    case INHERIT_ONLY_ACE:
		return("inheritonly");
	}
	return(0);
}

static char *acetype(int type)
{
	switch(type)
	{
	    case ACCESS_ALLOWED_ACE_TYPE:
		return("allowed");
	    case ACCESS_DENIED_ACE_TYPE:
		return("denied");
	    case SYSTEM_AUDIT_ACE_TYPE:
		return("audit");
	    case SYSTEM_ALARM_ACE_TYPE:
		return("alarm");
	    case ACCESS_ALLOWED_COMPOUND_ACE_TYPE:
		return("compound");
	    default:
		return("unknown");
	}
}

static void printsidname(Sfio_t *iop,register SID *sid)
{
	char domain[256];
	char name[256];
	SID_NAME_USE  psnu;
	int nsize = sizeof(name), dsize=sizeof(domain);
	if(LookupAccountSid(NULL, sid, name, &nsize, domain, &dsize,&psnu))
	{
		if(dsize>0)
			sfprintf(iop,"(%s/%s)",domain,name);
		else
			sfprintf(iop,"(%s)",name);
	}
	else
		sfprintf(sfstderr,"cannot get name\n");
}

/*
 * prints the sid into <buff> and returns the number of bytes
 * <bufsiz> is the size of the buffer in bytes
 */
int printsid(Sfio_t *iop, register SID *sid, int delim)
{
	register int i,l,m,n;
	unsigned char *cp = (unsigned char*)GetSidIdentifierAuthority(sid);
	if(cp[0] || cp[1])
		l = sfprintf(iop,"S-%u-%#2x%#2x%#2x%#2x%#2x%#2x",SID_REVISION,cp[0],cp[1],cp[2],cp[3],cp[4],cp[5]);
	else
	{
		n = (cp[2]<<24) | (cp[3]<<16) | (cp[4]<<8) | cp[5];
		l = sfprintf(iop,"S-%u-%u",SID_REVISION,n);
	}
	if(l<=0)
		return(l);
	n= *GetSidSubAuthorityCount(sid);
	for(i=0; i <n; i++)
	{
		m = sfprintf(iop,"-%u",*GetSidSubAuthority(sid,i));
		if(m<=0)
			return(l);
		l += m;
	}
	if(delim>=0)
		sfputc(iop,delim);
	return(l);
}

static void printacl(Sfio_t *iop, ACL *acl, int type)
{
	int i,j,first;
	char *(*fun)(int);
	char *cp;
	ACCESS_ALLOWED_ACE *ace;
	switch(type)
	{
	    case FILE_HANDLE:
		fun = fileperm;
		break;
	    case PROC_HANDLE:
		fun = procperm;
		break;
	    case TOK_HANDLE:
		fun = tokperm;
		break;
	    case THREAD_HANDLE:
		fun = threadperm;
		break;
	    case EVENT_HANDLE:
	    case SEM_HANDLE:
	    case MUTEX_HANDLE:
		fun = eventperm;
		break;
	    default:
		fun = 0;
	}
	sfprintf(iop,"Number of ACE: %d\n",acl->AceCount);
	for(i=0; i < acl->AceCount; i++)
	{
		GetAce(acl,i,&ace);
		sfputc(iop,'\t');
		printsidname(iop,(SID*)&ace->SidStart);
		printsid(iop,(SID*)&ace->SidStart,':');
		for(first=0,j=1; j < VALID_INHERIT_FLAGS; j<<=1)
		{
			if(cp=aceflag(ace->Header.AceFlags&j))
			{
				if(first++)
					sfputc(iop,'|');
				sfprintf(iop,"%s",cp);
			}
		}
		sfprintf(iop," %s ",acetype(ace->Header.AceType));
		printperm(iop,ace->Mask,fun);
		sfputc(iop,'\n');
	}
}

static void printsd(Sfio_t *iop, SECURITY_DESCRIPTOR* sd, int type)
{
	SECURITY_DESCRIPTOR_CONTROL  control;
	SID *owner,*group;
	BOOL r,x;
	ACL *acl;
	int revision;
	unsigned char rmcontrol;
	if(GetSecurityDescriptorOwner(sd,&owner,&r) && owner)
	{
		sfprintf(iop,"Owner: ");
		printsidname(iop,owner);
		printsid(iop,owner,'\n');
	}
	else
		sfprintf(sfstderr,"security user failed error=%d\n",GetLastError());
	if(GetSecurityDescriptorGroup(sd,&group,&r) && group)
	{
		sfprintf(iop,"Group: ");
		printsidname(iop,group);
		printsid(iop,group,'\n');
	}
	else
		sfprintf(sfstderr,"security group failed error=%d\n",GetLastError());
	if(GetSecurityDescriptorDacl(sd,&x,&acl,&r))
	{
		if(x && acl)
			printacl(iop,acl,type);
		else
			sfprintf(sfstderr,"Dacl not present\n");
	}
	else
		sfprintf(sfstderr,"security descriptor dacl error=%d\n",GetLastError());
	if(GetSecurityDescriptorSacl(sd,&x,&acl,&r))
	{
		if(x && acl)
			printacl(iop,acl,type);
		else
			sfprintf(sfstderr,"Sacl not present\n");
	}
	else
		sfprintf(sfstderr,"security descriptor sacl error=%d\n",GetLastError());
	if(GetSecurityDescriptorControl(sd,&control,&revision))
	{
		sfprintf(iop,"revision=%d control=%x ",revision,control);
		printcontrol(iop,control);
	}
	else
		sfprintf(sfstderr,"security descriptor control error=%d\n",GetLastError());
	if(GetSecurityDescriptorRMControl(sd,&rmcontrol))
		sfprintf(iop,"rmcontrol=%x\n",rmcontrol);
}

void printhandlesd(Sfio_t *iop, HANDLE hp, int type)
{
	int buffer[1024];
	int size;
	SECURITY_INFORMATION si;
        si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	if(GetKernelObjectSecurity(hp,si|SACL_SECURITY_INFORMATION,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
		printsd(iop,(SECURITY_DESCRIPTOR*)buffer,type);
	else if(GetKernelObjectSecurity(hp,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
		printsd(iop,(SECURITY_DESCRIPTOR*)buffer,type);
	else
		sfprintf(sfstderr,"get security descriptor error=%d\n",GetLastError());

}

static HANDLE sem_open(const char* name)
{
	return(OpenSemaphore(SEMAPHORE_ALL_ACCESS,FALSE,name));
}

static HANDLE event_open(const char* name)
{
	return(OpenEvent(EVENT_ALL_ACCESS,FALSE,name));
}

static HANDLE key_open(const char* name)
{
	int fd = open(name,0);
	if(fd<0)
	{
		return(0);
	}
	return(uwin_handle(fd,1));
}
static HANDLE file_open(const char* name)
{
	HANDLE hp;
	hp=CreateFile(name,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL);
	if(hp==INVALID_HANDLE_VALUE)
	{
		int fd;
		char *last;
		fd = strtol(name,&last,10);
		if(*last==0)
			return(uwin_handle(fd,0));
		return(0);
	}
	return(hp);
}

static HANDLE proc_open(const char* name)
{
	char *last;
	int pid =  strtol(name,&last,0);
	if(pid<=1)
		return(GetCurrentProcess());
	return(OpenProcess(GENERIC_READ|PROCESS_DUP_HANDLE,FALSE,pid));
}

int main(int argc, char *argv[])
{
	int pid=0,size,n,type=PROC_HANDLE,keydir=0;
	DWORD ntpid=0;
	char *cp, path[1024], pwd[256];
	int buffer[1024],prevstate[256];
	HANDLE atok=0,hp,tok=0,handle=0,me=GetCurrentProcess();
	SECURITY_INFORMATION si;
	HANDLE (*openfn)(const char*);
	LUID luid;
        si = OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION;
	openfn = 0;
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'e':
		type = EVENT_HANDLE;
		openfn = event_open;
		break;
	    case 'f':
		openfn = file_open;
		type = FILE_HANDLE;
		break;
	    case 'P':
		openfn = proc_open;
		ntpid = opt_info.num;
		break;
	    case 'p':
		openfn = proc_open;
		pid = opt_info.num;
		break;
	    case 's':
		openfn = sem_open;
		type = SEM_HANDLE;
		break;
	    case 't':
		type = THREAD_HANDLE;
		break;
	    case 'T':
		tok=(HANDLE)1;
		break;
	    case ':':
		error(2, opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(!(cp=argv[0]) && !pid && type!=PROC_HANDLE)
		error(ERROR_exit(2),"process id or argument is required");
	if(LookupPrivilegeValue(NULL, SE_SECURITY_NAME, &luid) && 
		OpenProcessToken(me,TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES,&atok))
	{
		char buf[sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)];
		TOKEN_PRIVILEGES *tokpri=(TOKEN_PRIVILEGES *)buf;
		tokpri->PrivilegeCount = 1;
		tokpri->Privileges[0].Luid = luid;
		tokpri->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if(AdjustTokenPrivileges(atok,FALSE,tokpri,sizeof(prevstate),(void*)prevstate,&size))
		{
		}
	}
	if(pid || ntpid || (!cp&&type==PROC_HANDLE))
	{
		if(ntpid)
			pid = (ntpid<<5);
		else if(pid==0)
		{
			ntpid = GetCurrentProcessId();
			pid = getpid();
		}
		else
			ntpid = uwin_ntpid(pid);
		if(!(hp=OpenProcess(GENERIC_READ|PROCESS_DUP_HANDLE,FALSE,ntpid)))
			error(ERROR_exit(1),"cannot open process %x err=%d",ntpid,GetLastError());
		sfprintf(sfstdout,"UNIX PROCESS ID: %d NT PROCESSID: %x\n",pid,ntpid);
		if(type==PROC_HANDLE)
		{
			printhandlesd(sfstdout,hp,type);
			if(!tok)
				exit(0);
			if(!OpenProcessToken(hp,GENERIC_READ,&tok))
				error(ERROR_exit(1),"cannot open token %d err=%d",pid,GetLastError());
			if(GetTokenInformation(tok,TokenDefaultDacl,(void*)buffer,sizeof(buffer),&size))
			{
				sfprintf(sfstdout,"TOKEN default acl:\n");
				printacl(sfstdout,((TOKEN_DEFAULT_DACL*)buffer)->DefaultDacl,type);
			}
			else
				sfprintf(sfstderr,"token defacl info error=%d\n",GetLastError());
			sfprintf(sfstdout,"TOKEN:\n");
			
		}
		else
		{
			handle = (HANDLE)atoi(cp);
			if(!DuplicateHandle(hp,handle,me,&tok,0,FALSE,DUPLICATE_SAME_ACCESS))
				error(ERROR_exit(1),"duplicate handle %d failed err=%d",handle,GetLastError());
		}
		printhandlesd(sfstdout,tok,type);
		exit(0);
	}
	if(openfn)
	{
		if(hp = (*openfn)(cp))
			printhandlesd(sfstdout,hp,type);
		else
			error(ERROR_exit(1)," open %s failed err=%d",cp,GetLastError());
		goto done;
		exit(0);
	}
	getcwd(pwd,sizeof(pwd));
	if(memcmp(pwd,"/reg/",5==0))
		keydir=1;
	while( cp = *argv++)
	{
			type = FILE_HANDLE;
		if((*cp=='/' && memcmp(cp,"/reg/",5)==0) || (*cp!='/' && keydir))
		{
			type = KEY_HANDLE;
			sfprintf(sfstdout,"KEY: %s\n",cp);
			if(hp = key_open(cp))
				printhandlesd(sfstdout,hp,KEY_HANDLE);
			else
			{
				sfprintf(sfstderr,"Unable to open key=%s\n",cp);
				n = 1;
			}
		}
		else
		{
			uwin_path(cp,path,sizeof(path));
			sfprintf(sfstdout,"FILE: %s\n",path);
			if(GetFileSecurity(path,si,(SECURITY_DESCRIPTOR*)buffer,sizeof(buffer),&size))
				printsd(sfstdout,(SECURITY_DESCRIPTOR*)buffer,FILE_HANDLE);
			else
			{
				sfprintf(sfstderr,"getfile security failed error=%d\n",GetLastError());
				n = 1;
			}
		}
	}
done:
	if(atok)
		AdjustTokenPrivileges(hp,FALSE,(void*)prevstate,0,NULL,0); 
	return(n);
}
