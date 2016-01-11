#include <ast.h>
#include <strings.h>
#include <sfio.h>
#include <ctype.h>
#include <pwd.h>
#include <uwin.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>

#define MAXTRY	3

FILE *fp;

static char astconfig[] = "_AST_FEATURES=UNIVERSE - att PATH_LEADING_SLASHES - 1 PATH_RESOLVE - metaphysical";
int main( int argc, char *argv[], char *envp[])
{
	register char *cp, *sp;
	char *name,*passwd, **myenv=NULL, myname[PATH_MAX];
	char *nthomebuf=NULL, *nthomedrive=NULL, *tmp_ptr;
	struct sockaddr_in serv_addr, cli_addr;
	int i, newfd, sockfd, clilen, ret, homeindex= -1,shell= -1,logname= -1;
	int total=0, tsize=0, istty=0, issocket=0, isconsole=0, nohome=1,bon=0;
	struct passwd* pw;
	HANDLE tok = 0;
	char unixhomebuf[PATH_MAX]	= "HOME=/usr" ;
	char shellbuff[PATH_MAX]	= "SHELL=/usr/bin/sh";
	char lognamebuff[PATH_MAX]	= "LOGNAME=";
	char uwindebugbuff[]		= "__UWIN_DEBUG__=b5";

	strcpy(myname, argv[0]);
	tmp_ptr = strrchr( myname, '.' );
	if(tmp_ptr != NULL )
		*tmp_ptr = '\0';
	if(!(tmp_ptr = strrchr(myname,'/')))
		tmp_ptr = strrchr(myname,'\\');
	if( tmp_ptr)
		tmp_ptr++; 
	else
		tmp_ptr = myname;
	if(name=argv[1])
	{
		int j;
		char buff[256],c;
		for(i=0;  i < MAXTRY; i++)
		{
			passwd = getpass("Password: ");
			tok = uwin_mktoken(name,passwd,0);
			if(tok)
				break;
			write(2,"login incorrect\n",16);
			write(2,"login: ",7);
			j = 0;
			name = buff;
			while(read(0,&c,1)>0 && j<sizeof(buff))
			{
				if(c=='\n')
					goto ok;
				else if(c!='\r')
					buff[j++] = c;
			}
			if(j>=sizeof(buff))
				sfprintf(sfstderr,"name too long\n");
				sfprintf(sfstderr,"end-of-file unexpected\n");
			exit(2);
		ok:
			buff[j] = 0;

		}
		if(!tok)
			exit(1);
		pw = getpwnam(name);
	}
	else 
	{
		uid_t uid = geteuid();
		if((int)uid>=0)
			pw = getpwuid(uid);
		if(!pw)
			strncpy(&lognamebuff[8],getlogin(),sizeof(lognamebuff)-8);
	}
	if(pw && pw->pw_shell && *pw->pw_shell)
		strncpy(&shellbuff[6],pw->pw_shell,sizeof(shellbuff)-6);
	if(pw && pw->pw_name && *pw->pw_name)
		strncpy(&lognamebuff[8],pw->pw_name,sizeof(lognamebuff)-8);
	if(pw && pw->pw_dir && *pw->pw_dir)
	{
		strncpy(&unixhomebuf[5],pw->pw_dir,sizeof(unixhomebuf)-5);
		if(strcasecmp(pw->pw_dir,"/tmp"))
			nohome = 0;
	}
	for(i=0; tmp_ptr[i]; i++ )
		tmp_ptr[i] = tolower(tmp_ptr[i]);

	if(*tmp_ptr=='b')
	{
		bon = 1;
		tmp_ptr++;
	}
	if (!strcmp( tmp_ptr, "login" ))
		isconsole = 1;
	if (!strcmp( tmp_ptr, "tlogin" ))
		istty = 1;
	if (!strcmp( tmp_ptr, "slogin" ))
		issocket = 1;
	if (!isconsole && !istty && !issocket)
		isconsole = 1;
	/* 
	 * Calculate total number of env. variables and copy the 
	 * HOMEPATH variable to nthomebuf
	 */
	for(i=0; cp=envp[i]; i++)
	{
		total++;
		tsize += (int)strlen(cp) + 1;
		if ((cp[0] == 'H') &&
		    (cp[1] == 'O') &&
		    (cp[2] == 'M') &&
		    (cp[3] == 'E'))
		{
			if(strncmp(&cp[4],"PATH=",5)==0)
				nthomebuf = cp;
			else if(strncmp(&cp[4],"DRIVE=",6)==0)
				nthomedrive = cp;
			else if(cp[4]=='=')
				homeindex = i;
		}
		else if(memcmp(cp,"SHELL=",6)==0)
			shell = i;
		else if(memcmp(cp,"LOGNAME=",8)==0)
			logname = i;
	}

	/* 
	 * If nthomebuf is not NULL, copy it to unixhomebuf
	 */
	if(nthomebuf && nthomedrive && nohome)
	{
		strcpy(unixhomebuf, "HOME=");
		unixhomebuf[5]='/';
		unixhomebuf[6]=nthomedrive[10];
		strncpy(&unixhomebuf[7], nthomebuf + 9,sizeof(unixhomebuf)-7);
	}
	/*
	 * In unixhomebuf, change all \ to /
	 */
	for (i=0; unixhomebuf[i]; i++ )
	{
		if(unixhomebuf[i] == '\\')
			unixhomebuf[i] = '/';
	}
	/*
	 * Copy the environment variable to a separate buffer and add
	 */
	myenv = (char**)malloc(sizeof(char*)*(total+5)+tsize+1);
	cp = (char*)myenv + sizeof(char*)*(total+5);
	for(i=0; sp=envp[i]; i++ )
	{
		myenv[i] = cp;
		while(*cp++ = *sp++);
	}
	myenv[i++] = astconfig;

	/*
	 * Add unixhomebuf to environment
	 */
	if(homeindex<0)
		homeindex = i++;
	myenv[homeindex] = unixhomebuf;
	if(shell<0)
		shell = i++;
	myenv[shell] = shellbuff;
	if(logname<0)
		logname = i++;
	myenv[logname] = lognamebuff;
	if(bon)
		myenv[i++] = uwindebugbuff;
	myenv[i] = 0;

	if (istty)
	{
		close(0);
		close(1);
		close(2);
		newfd = open( "/dev/tty00",O_CREAT|O_RDWR);
		if (newfd < 0 )
			return( 1 );
		dup2(newfd,0);
		dup2(newfd,1);
		dup2(newfd,2);
		/*close(newfd);*/
	}
	if(issocket)
	{
		sockfd = socket( AF_INET, SOCK_STREAM, 0 );
		if (sockfd < 0)
			return(1);
		memset( ( char * )&serv_addr, '\0', sizeof( serv_addr ) );
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
		serv_addr.sin_port = htons( 8000 );

		ret = bind( sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		if (ret < 0)
			return(1);
		listen(sockfd, 5 );
		clilen = sizeof(cli_addr);
		newfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if ( newfd < 0 )
			return(1);
		dup2(newfd,0);
		dup2(newfd,1);
		dup2(newfd,2);
		close(newfd);
	}
	if(cp = strrchr(&shellbuff[6],'/'))
		cp++;
	else
		cp = &shellbuff[6];
	myname[0] = '-';
	strncpy(&myname[1],cp,sizeof(myname)-1);
	if(tok)
	{
		struct spawndata sdata;
		char *av[2];
		av[0] = myname;
		av[1] = 0;
		memset(&sdata, 0, sizeof(sdata));
		sdata.flags = UWIN_SPAWN_EXEC;
		sdata.tok = tok;
		uwin_spawn(&shellbuff[6],av,myenv,&sdata);
	}
	else
		execle(&shellbuff[6],myname,(char*)0,myenv);
	return( 1 );
}
