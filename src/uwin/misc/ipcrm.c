#pragma prototyped
/*
 * ipcrm.c
 * Written by David Korn
 * Tue Jun 23 14:25:03 EDT 1998
 */

static char id[] = "\n@(#)ipcrm (AT&T Labs) 06/23/98\0\n";

#include	<cmd.h>
#include	<sys/shm.h>
#include	<sys/sem.h>
#include	<sys/msg.h>

main(int argc, char *argv[])
{
	register int n;
	key_t id;

#if _AST_VERSION >= 20060701L
	cmdinit(argc, argv,0,NULL,0);
#else
	cmdinit(argv,0,NULL,0);
#endif
	while (n = optget(argv, "[q#[msgid]m#[shmid]s#[semid]Q#[msgkey]M#[shmkey]S#[semkey]] ")) switch (n)
	{
	    case 'Q':
		id = (key_t)opt_info.num;
		if((opt_info.num = msgget(id,0)) < 0)
			error(ERROR_exit(1),"%x: unknown message queue",id);
	    case 'q':
		if(msgctl(opt_info.num, IPC_RMID,0)<0)
			error(ERROR_system(1),"%x: failed",opt_info.num);
		break;
	    case 'M':
		id = (key_t)opt_info.num;
		if((opt_info.num = shmget(id,0,0)) < 0)
			error(ERROR_exit(1),"%x: unknown shared memory segment",id);
	    case 'm':
		if(shmctl(opt_info.num, IPC_RMID, 0)<0)
			error(ERROR_system(1),"%x: failed",opt_info.num);
		break;
	    case 'S':
		id = (key_t)opt_info.num;
		if((opt_info.num = semget(id,0,0)) < 0)
			error(ERROR_exit(1),"%x: unknown semaphore",opt_info.num);
	    case 's':
		if(semctl(opt_info.num, 0, IPC_RMID, 0) < 0)
			error(ERROR_system(1),"%x: failed",id);
		break;
	    case ':':
		error(2, opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(*argv || error_info.errors)
		error(ERROR_usage(2),optusage((char*)0));
	return(error_info.errors?1:0);
}

