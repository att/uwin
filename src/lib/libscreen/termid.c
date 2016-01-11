#include	<stdio.h>
#include	"termhdr.h"


/*
**	Determine a terminal type by sending probes to the terminal.
**	Terminals are searched for from a terminal id data file.
**
**	Each line in the file must be of the form:
**		SendSequence:ReceivedSequence:TermName
**
**	Sequences can be defined as in termcap.
**	In the ReceivedSequence, '.' matches any character so if a '.'
**	is needed as is, it must be encoded as '\256'.
**
**	The routine will run faster if all terminals with
**	the same send sequences are grouped together.
**
**	Written by Kiem-Phong Vo
*/

#ifndef TERMID
#define TERMID	"/usr/lib/termid"
#endif

#if _hdr_termio
#define GET_TTY(fd,tty) ioctl(fd,TCGETA,&(tty))
#define SET_TTY(fd,tty)	ioctl(fd,TCSETAW,&(tty))
#else
#define GET_TTY(fd,tty)	ioctl(fd,TIOCGETP,&(tty))
#define SET_TTY(fd,tty)	ioctl(fd,TIOCSETN,&(tty))
#endif

#if __STD_C
char *termid(reg int outfd, reg int infd)
#else
char *termid(outfd,infd)
reg int	outfd, infd;
#endif
{
	reg int		n, i;
	reg char	*cp, *name;
	char		*code;
	char		buf[1024], iobuf[BUFSIZ],
			send[256], rcv[256], tsend[256], trcv[256];
	FILE		*filep;
	SGTTYB		otty, ctty;
	static char	term[32];

	/* open the proper termid database */
	if(!(name = getenv("TERMID")) || !name[0])
		name = TERMID;
	if(!(filep = fopen(name,"r")) )
		return NIL(char*);

	/* prevent stdio from malloc-ing which is bad for vi */
	setbuf(filep,iobuf);

	/* get terminal states */
	if(GET_TTY(outfd,otty) < 0 || GET_TTY(outfd,ctty) < 0)
		return NIL(char*);

	/* put terminal in raw mode */
#if _hdr_termio
	ctty.c_oflag &= ~(ONLCR|OCRNL);
	ctty.c_iflag &= ~(ICRNL|ISTRIP);
	ctty.c_lflag &= ~(ECHO|ICANON);
	ctty.c_cc[VMIN] = 1;
	ctty.c_cc[VTIME] = 0;
#else
	ctty.sg_flags |= CBREAK;
	ctty.sg_flags &= ~(CRMOD|ECHO);
#endif
	if(SET_TTY(outfd,ctty) < 0)
		return NIL(char*);

	/* read each line until terminal name is set */
	tsend[0] = trcv[0] = '\0';
	name = NIL(char*);
	for(;;)
	{
		if(!fgets(buf,sizeof(buf),filep) )
			break;
		if(buf[0] == '#')
			continue;

		/* get the code to send */
		code = send;
		tcdecode(buf,&code);
		if(!send[0])
			continue;

		/* get the code to be received */
		code = rcv;
		for(n = 0; buf[n] != '\0'; ++n)
			if(buf[n] == ':')
				break; 
		if(buf[n] == '\0')
			continue;
		tcdecode(buf+n+1,&code);

		/* done */
		if(rcv[0] == '\0')
			break;

		/* get the terminal name */
		for(n += 1; buf[n] != '\0'; ++n)
			if(buf[n] == ':')
				break; 
		if(buf[n] == '\0')
			continue;
		cp = buf+n+1;
		for(n = 0; *cp != '\n' && *cp != '\0'; ++cp, ++n)
			term[n] = *cp;
		term[n] = '\0';

		/* if code has not been sent yet */
		if(!tsend[0] || strcmp(send,tsend) != 0)
		{	/* flush the queue */
#if _hdr_termio
			ioctl(outfd,TCFLSH,0);
#else
			ioctl(outfd,TIOCSETP,&(ctty));
#endif

			/* send the code+erase bytes in case it's not eaten */
			for(n = 0; send[n] != '\0'; ++n)
				buf[n] = (char)(send[n]&A_CHARTEXT);
			buf[n] = '\r';
			for(i = 1; i <= n; ++i)
				buf[n+i] = ' ';
			buf[n+i] = '\r';
			if(write(outfd,buf,n+n+2) != n+n+2)
				break;

			/* read the return code */
			cp = buf;
			while((cp-buf) < (sizeof(buf)-16) &&
			      (n = _rdtimeout(infd,500,(uchar*)cp,16)) > 0)
				cp += n;
			*cp = '\0';

			/* save the codes */
			strcpy(tsend,send);
			strcpy(trcv,buf);
		}

		/* compare the receive codes */
		if(strcmp(tsend,send) == 0)
		{
			for(n = 0; rcv[n]; ++n)
				if(rcv[n] != '.' &&
				   (rcv[n]&A_CHARTEXT) != (trcv[n]&A_CHARTEXT))
					break;

			if(n > 0 && rcv[n] == '\0')
			{
				name = term;
				break;
			}
		}
	}

	/* restore the terminal to normal modes */
	SET_TTY(outfd,otty);
	fclose(filep);
	return name;
}
