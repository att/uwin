
/* This file is part of telnetd source code */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>

extern int ptym_open(char *);
extern int ptys_open(char *);

int pid;

void sighan(int i)
{
	int stat;
	wait(&stat);
	exit(0);
}

int sighuphandler(int i)
{
	kill(pid,SIGHUP);
	kill(pid,SIGHUP); //you need two HUP signals
	return(i);
}

int main( int argc, char * argv[ ], char * envp[] )
{
  	struct termios term;
	struct sigaction new;
	int newfd=20,fds,fdm,nfds;
	char slave[20];
	char temp_store[4096];
	int i,count;

	fd_set	readfds;
	chdir("/");
	fdm = ptym_open(slave);
	if(fdm < 0)
	{
		strcat(temp_store, "ERROR : Could not open pseudoterminal\n");
		write(20, temp_store, strlen(temp_store));
		return(1);
	}
	dup2(fdm, 13);
	close(fdm);
	fdm=13;
	fcntl(fdm,F_SETFD,FD_CLOEXEC);
	fds = ptys_open(slave);
	if(tcgetattr(fds, &term) >= 0) //Setting in ECHO mode
	{
		term.c_lflag |= ECHO;
  		tcsetattr(fds,TCSANOW, &term);
	}
		
	dup2(fds, 14);
	close(fds);
	fds=14;

	dup2(fds,0);
	dup2(fds,1);
	dup2(fds,2);
	close(fds);
	pid=spawnl("/usr/bin/logins.exe", "-logins.exe", ( char * )0);
	if (pid < 0)
		exit(1);
	nfds = ((newfd > fdm)?newfd:fdm) + 1;
	sigemptyset(&new.sa_mask);
	signal(SIGCLD, sighan);
	signal(SIGHUP, sighuphandler);
	fcntl(20,F_SETFD, 1);

	while(1)
	{
		int r;
		FD_ZERO(&readfds);
		FD_SET(newfd,&readfds);
		FD_SET(fdm,&readfds);

		if ((r=select(nfds,&readfds,NULL,NULL,NULL))<0)
			break;
		else
		{
			char ch[4096];
			int ret;
			if(!r)
				continue;
			if(FD_ISSET(newfd,&readfds))
			{
				ret = read(newfd,ch,4096);
				if (ret<=0)
					break;

				count = 0;
				for (i=0;i <= ret;i++)
				{
					if (ch[i] != '\r')
					{
						temp_store[count] = ch[i];
						count++;   /* To keep track of no of chars*/
						continue;
					}
					else
					{
						 temp_store[count] = ch[i];/*copy the carriage return */
						 count++;
						 if ((i < ret)&& (ch[i+1] == '\n'))
							 i++; /*skip the new line character */
					}
				} /* End of for loop */
				write(fdm,temp_store,count-1);
			}
			if(FD_ISSET(fdm,&readfds))
			{
				ret = read(fdm,ch,4096);
				if(ret <= 0)
					break;
				write(newfd,ch,ret);
			}
		}
	}
	close(newfd);
	close(fdm);
	kill(-tcgetpgrp(0),SIGHUP);
	sleep(1);
	kill(-pid,SIGHUP);
	return(1);
}
