/***********************************************************************
*                                                                      *
*              This software is part of the uwin package               *
*          Copyright (c) 1996-2011 AT&T Intellectual Property          *
*                         All Rights Reserved                          *
*                     This software is licensed by                     *
*                      AT&T Intellectual Property                      *
*           under the terms and conditions of the license in           *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*      (with an md5 checksum of b35adb5213ca9657e911e9befb180842)      *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                  David Korn <dgk@research.att.com>                   *
*                                                                      *
***********************************************************************/
#include "uwindef.h"
#include <sys/mtio.h>

#define BLOCKSIZE 512
#define RETRY_COUNT 5

/*The following flags are specific to each (logical) tape device
 *And these are being stored at the end of the fdname block
 */
struct Tapeinfo
{
	int blocking;
	int file_mark;		/* used for writing filemark on close */
	int eof;		/* end of file */
	int eom_read;		/* end of media */
	int eom_write;		/* end of media */
	long end_offset;
	int no_rewind;
	int bsd;
	int notused;		/* reserve space for block type marker */
};

#define	tinfo(fdp)	((struct Tapeinfo*)(fdname(file_slot((fdp)))+(BLOCK_SIZE-sizeof(struct Tapeinfo))))


#define blocking_factor(fdp)  		(tinfo(fdp)->blocking)
#define write_file_mark_on_close(fdp)	(tinfo(fdp)->file_mark)
#define eof_detected(fdp)  		(tinfo(fdp)->eof)
#define eom_in_read(fdp)  		(tinfo(fdp)->eom_read)
#define eom_in_write(fdp)  		(tinfo(fdp)->eom_write)
#define no_rewind_on_close(fdp)  	(tinfo(fdp)->no_rewind)
#define bsd_behaviour(fdp)  		(tinfo(fdp)->bsd)
#define lseek_end_offset(fdp)  		(tinfo(fdp)->end_offset)

static int position_tape(int, int , long );
static int prepare_tape(HANDLE hTape, int immediate, int cmd);
static int set_block_size(int, int user_given_bf);
static int get_block_size(HANDLE hTape, struct mtop* mp);
static int get_tape_status(HANDLE , struct mtget* );


static HANDLE tape_open(Devtab_t *dp, Pfd_t *fdp, Path_t *ip, int oflags, HANDLE *extra)
{
	HANDLE hTape;
	int minor=ip->name[1], major=ip->name[0],retry;
	int tape_no;  			//as in the 1 of Tape1
	char devname[20] = "\\\\.\\Tape0";
	struct mtop mt;


	tape_no = (minor & 0x07);
	bsd_behaviour(fdp) = minor & 0x40;
	no_rewind_on_close(fdp) = minor & 0x80;
	write_file_mark_on_close(fdp) = 0;

	if(tape_no>0 && tape_no <= 7)
		devname[8] = '0'+tape_no;

	//tape devices sometimes fail the first time you try to
	//open it and this is not really an error
	for(retry=0;retry<RETRY_COUNT;retry++)
	{
		if(hTape = createfile(devname, GENERIC_READ|GENERIC_WRITE, 0,0,OPEN_EXISTING,0,NULL))
			break;

		if (retry == RETRY_COUNT-1)  //ok: createfile really failed
		{
			if(GetLastError() == ERROR_NO_MEDIA_IN_DRIVE)
			{
				errno = EIO;
				return 0;
			}
			errno = EINVAL;
			logerr(0, "CreateFile");
			return 0;
		}
	}

	//prepare the tape: load it before doing any further operations
	if(!no_rewind_on_close(fdp) && prepare_tape(hTape, 0, 3)!=0)
		return 0;

	if((blocking_factor(fdp) = (get_block_size(hTape, &mt)/BLOCKSIZE)) == 0)
		return 0;

	return hTape;
}


static ssize_t taperead(int fd, Pfd_t* fdp, char *buff, size_t asize)
{
	long rsize;
	DWORD lasterror;
	int size;

	if(!Phandle(fd))
	{
		errno = EBADF;
		return(-1);
	}
	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (int)asize;
	if(size % (BLOCKSIZE*blocking_factor(fdp)))
	{
		errno = EINVAL;
		return -1;
	}

	/* Set the ACCESS_TIME_UPDATE bit */
	fdp->oflag |= ACCESS_TIME_UPDATE;


	if (eom_in_read(fdp) == -1)
		return -1;  //more than two read at EOM is an error

	if (eom_in_read(fdp) == 1)
	{
		eom_in_read(fdp) = -1;
		return 0; //second read of EOM; return 0;
	}

	if (eof_detected(fdp) == 1)
		return -1;

	if(ReadFile(Phandle(fd), buff, size, &rsize, NULL))
	{
		eof_detected(fdp) = 0;
		eom_in_read(fdp) = 0;
		return(rsize);
	}
	else if (((lasterror=GetLastError()) == ERROR_FILEMARK_DETECTED) || (lasterror == ERROR_NO_DATA_DETECTED))
	{
		eof_detected(fdp) = 1;
		return 0;
	}
	else if (lasterror == ERROR_END_OF_MEDIA)
	{
		eom_in_read(fdp) = 1; //first time EOM encountered
		return 0;
	}
	errno = unix_err(GetLastError());
	logerr(0, "ReadFile(tape)");
	return(-1);
}


static ssize_t tapewrite(int fd, register Pfd_t* fdp, char *buff, size_t asize)
{
	long rsize;
	HANDLE hp;
	char *pbuff;
	DWORD lasterror;
	long offset = (long)(lseek_end_offset(fdp));
	int size;

	if(!(hp = Xhandle(fd)))
		hp = Phandle(fd);
	if(!hp)
	{
		errno = EBADF;
		return(-1);
	}
	if(asize > SSIZE_MAX)
		asize = SSIZE_MAX;
	size = (int)asize;
	if(size % (BLOCKSIZE*blocking_factor(fdp)))
	{
		errno = EINVAL;
		return -1;
	}

	/* Set the MODIFICATION_TIME_UPDATE bit */
	fdp->oflag |= MODIFICATION_TIME_UPDATE;
	if(offset > 0)
	{
		pbuff = (char *)malloc(offset);
		memset(pbuff, 0,(sizeof(char)*offset));
		if(eom_in_write(fdp) == 1)
		{
			eom_in_write(fdp) = -1;
			return 0;
		}
		else if(eom_in_write(fdp) == -1)
		{

			errno = ENOSPC;
			return -1;
		}

		if(WriteFile(hp, pbuff, offset,&rsize,NULL)==0)
		{
			lasterror = GetLastError();
			if(lasterror == ERROR_END_OF_MEDIA ||
					lasterror == ERROR_NOT_ENOUGH_MEMORY)
			{
				eom_in_write(fdp) = 1;
				return 0;
			}
			free(pbuff);
			return -1;
		}
		else
			eom_in_write(fdp) = 0;

		lseek_end_offset(fdp) = (off64_t)0;
		free(pbuff);
	}

	if((eom_in_write(fdp)) == 1)
	{
		eom_in_write(fdp) = -1;
		return 0;
	}
	else if(eom_in_write(fdp) == -1)
	{
		errno = ENOSPC;
		return -1;
	}

	if(WriteFile(hp, buff, size, &rsize, NULL))
	{
		eom_in_write(fdp) = 0;
		write_file_mark_on_close(fdp) = 1;
		return(rsize);
	}
	else
	{
		lasterror = GetLastError();
		if(lasterror == ERROR_END_OF_MEDIA ||
				lasterror == ERROR_NOT_ENOUGH_MEMORY)
		{
			eom_in_write(fdp) = 1;
			return 0;
		}
	}

	errno = unix_err(GetLastError());
	logerr(0, "WriteFile (tape)");
	return(-1);
}

static int tapeclose(int fd, Pfd_t* fdp,int noclose)
{
	if(noclose<=0)
	{
		if(!Phandle(fd))
			return 0;

		if(write_file_mark_on_close(fdp))
		{
			//first, write a filemark!
			write_file_mark_on_close(fdp) = 0;
			if(WriteTapemark(Phandle(fd), TAPE_FILEMARKS, 1, 0) != NO_ERROR)
				logerr(0, "WriteTapemark");
		}

		if(!bsd_behaviour(fdp))
		{
			//if BSD behaviour no operation is performed
			if(no_rewind_on_close(fdp))
				position_tape(fd, 1, 0);
			else
				position_tape(fd, 2, 0); //rewind
		}

		eom_in_read(fdp)=0;
		eom_in_write(fdp)=0;
		eof_detected(fdp)=0;
		lseek_end_offset(fdp)=(off64_t)0;
		CloseHandle(Phandle(fd));
	}
	return 0;
}

static off64_t tapelseek(int fd, Pfd_t* fdp, off64_t offset, int whence)
{
	DWORD low,high,current_partition;
	off64_t *p_off=((off64_t*)fdname(file_slot(fdp))+503);


	if(!Phandle(fd))
	{
		errno = EBADF;
		return(-1);
	}
	if (write_file_mark_on_close(fdp))
	{
		write_file_mark_on_close(fdp) = 0;
		if(WriteTapemark(Phandle(fd), TAPE_FILEMARKS, 1, 0) != NO_ERROR)
				logerr(0, "WriteTapemark");
	}
	if(offset % (BLOCKSIZE*blocking_factor(fdp)))
	{
		errno = EINVAL;
		return -1;
	}

	switch (whence)
	{
		case SEEK_SET:  //from beginning of file
			if(position_tape(fd, 4, (long)(offset/(blocking_factor(fdp)*BLOCKSIZE))) == 0)
			{
				if ( GetTapePosition(Phandle(fd), TAPE_ABSOLUTE_POSITION, &current_partition, &low, &high) != NO_ERROR)
					logerr(0, "GetTapePosition");
				else
				{
					return (off64_t) (blocking_factor(fdp)*BLOCKSIZE*low);
				}
			}
			break;
		case SEEK_CUR:
			if(position_tape(fd, 0, (long)(offset/(blocking_factor(fdp)*BLOCKSIZE))) == 0)
			{
				if ( GetTapePosition(Phandle(fd), TAPE_ABSOLUTE_POSITION, &current_partition, &low, &high) != NO_ERROR)
					logerr(0, "GetTapePosition");
				else
				{
					return blocking_factor(fdp)*BLOCKSIZE*low;
				}
			}
			break;
		case SEEK_END:
			if(position_tape(fd, 3, 0) != 0)
			{
				errno = EINVAL;
				return -1;
			}
			*p_off = offset;
			return 0;
			break;
		default:
			errno = EINVAL;
			break;
	}
	return -1;
}


static int position_tape(int fd, int code, long count)
{
	long posmode;
	int partition=0; //use the current partition

	if (write_file_mark_on_close(getfdp(P_CP,fd)))
	{
		write_file_mark_on_close(getfdp(P_CP,fd)) = 0;
		if(WriteTapemark(Phandle(fd), TAPE_FILEMARKS, 1, 0) != NO_ERROR)
				logerr(0, "WriteTapemark");
	}
	switch (code)
	{
      case 0:
		posmode = TAPE_SPACE_RELATIVE_BLOCKS;
        break;
      case 1:
		posmode = TAPE_SPACE_FILEMARKS;
        break;
	  case 2:
		posmode = TAPE_REWIND;
		break;
      case 3:
	 	posmode = TAPE_SPACE_END_OF_DATA;
		break;
      case 4:
	 	posmode = TAPE_ABSOLUTE_BLOCK;
		break;
	  case 5:
		posmode = TAPE_SPACE_SETMARKS;
		break;
	  case 6:
		posmode = TAPE_LOGICAL_BLOCK;
		partition = count; //count here specifies the partition no.
		count = 0; //make count = 1 to go to the first logical block
		break;
	  default:
		errno = EINVAL;
	  	return -1;
	}
	if(SetTapePosition (Phandle(fd), posmode, partition, count, 0, FALSE) != NO_ERROR)
	{
		errno = EINVAL;
		logerr(0, "SetTapePosition");
		return -1;
	}
	else
			eof_detected(getfdp(P_CP,fd)) = 0;
	return 0;
}



// 1 - retension
// 2 - unload
// 3 - load
static int prepare_tape(HANDLE hTape, int immediate, int cmd)
{
	DWORD prepmode;
	int retry;


	if (cmd == 1)
		prepmode = TAPE_TENSION;
	else if (cmd == 2)
		prepmode = TAPE_UNLOAD;
	else if (cmd == 3)
		prepmode = TAPE_LOAD;
	else
		return 0;


	for(retry=0;retry<RETRY_COUNT;retry++)
	{
		if (PrepareTape (hTape, prepmode, FALSE) == NO_ERROR)
			break;
		if(retry == RETRY_COUNT-1)
		{
			errno = EINVAL;
			logerr(0, "PrepareTape");
			return -1;
		}
	}
  return 0;
}

static int set_block_size(int fd, int user_given_bf)
{
	TAPE_GET_DRIVE_PARAMETERS tapedrive;
	TAPE_SET_MEDIA_PARAMETERS setmediapar;
	DWORD varlen, retry;
	HANDLE hTape=Phandle(fd);

	for(retry=10;retry;retry--)
	{
		varlen = sizeof (tapedrive);
		if (GetTapeParameters (hTape, GET_TAPE_DRIVE_INFORMATION, &varlen, &tapedrive) == NO_ERROR)
			break;

	}
	if(!retry)
	{
		logerr(0, "GetTapeParameters");
		return -1;
	}

    /* set blocksize if supported */
    if (tapedrive.FeaturesHigh & TAPE_DRIVE_SET_BLOCK_SIZE)
    {
		if((unsigned)(BLOCKSIZE * user_given_bf) <= tapedrive.MaximumBlockSize)
		{
			setmediapar.BlockSize = BLOCKSIZE * user_given_bf;
			blocking_factor(getfdp(P_CP,fd)) = user_given_bf;

			if(SetTapeParameters (hTape, SET_TAPE_MEDIA_INFORMATION, &setmediapar) != NO_ERROR)
			{
				errno = EINVAL;
				logerr(0, "SetTapeParameter");
				return -1;
			}
			return 0;
		}
	}
	return -1;
}

static int get_block_size(HANDLE hTape, struct mtop* mp)
{
	TAPE_GET_MEDIA_PARAMETERS getmediapar;
	DWORD varlen, retry;

	varlen = sizeof (getmediapar);
	for(retry=0;retry<RETRY_COUNT;retry++)
	{
		if (GetTapeParameters (hTape, GET_TAPE_MEDIA_INFORMATION, &varlen, &getmediapar) == NO_ERROR)
			break;
		if(retry == RETRY_COUNT-1)
		{
			errno = EINVAL;
			logerr(0, "GetTapeParameters");
			return -1;
		}
	}
	return(mp->mt_count = getmediapar.BlockSize);
}

// IOCTL control functions for tape devices
int tape_ioctl(int fd, void* mt, int opcode)
{
	HANDLE hp=Phandle(fd);

	if(hp==0)
	{
		errno = EBADF;
		return -1;
	}

	if(opcode == 1)  //MTIOCTOP
	{
		struct mtop* mp = (struct mtop *)mt;

		if (mp == NULL)
			return -1;

		switch ( mp->mt_op )
		{
			case	MTFSF:
				// forward space over file mark
				// position tape just after filemark
				// next read happens on first block of the file
				return(position_tape(fd, 1, mp->mt_count));
				break;	//just for switch consistency
			case	MTBSF:
				// backward space over file mark (1/2" only )
				// position tape before filemark
				// next read will read an EOF
				return(position_tape(fd, 1, -mp->mt_count));
				break;
			case	MTFSR:
				// forward space to inter-record gap
				return(position_tape(fd, 0, mp->mt_count));
				break;
			case	MTBSR:
				// backward space to inter-record gap
				return(position_tape(fd, 0, -mp->mt_count));
				break;
			case	MTWEOF:
				// write an end-of-file record
				if(WriteTapemark(hp,TAPE_FILEMARKS,1,0) != NO_ERROR)
				{
					 logerr(0, "WriteTapemark(filemark)");
					 errno = EINVAL;
					 return -1;
				}
				else
					write_file_mark_on_close(getfdp(P_CP,fd)) = 0;
				return 0;
				break;
			case	MTREW:
				// rewind
				return(position_tape(fd, 2, 0));
				break;
			case	MTOFFL:
				// rewind and put the drive offline
				if(position_tape(fd, 2, 0)==0)
					return(prepare_tape(hp, 0, 2));
				else
					return -1;
				break;
			case	MTNOP:
				// no operation, sets status only
				return 0;
				break;
			case	MTRETEN:
				// retension the tape (cartridge tape only)
				return(prepare_tape(Phandle(fd), 0, 1));
				break;
			case	MTEOM:
				// position to end of media
				return(position_tape(fd, 3, 0));
				break;
			case	MTERASE:
				// erase the entire tape
				if(position_tape(fd, 2, 0)==0)
				{
					if(EraseTape (hp, TAPE_ERASE_LONG, 0) != NO_ERROR)
					{
						errno = EINVAL;
						logerr(0, "EraseTape");
						return -1;
					}
					else
						return 0;
				}
				else
					return -1;
				break;
			case    MTSETBLK:
				// set record size
				return(set_block_size(fd,mp->mt_count));
				break;
			case MTSEEK:
				//seek to block
				/* this doesn't look right */
				return((int)tapelseek(fd, 0, mp->mt_count, SEEK_CUR));
				break;
			case MTFSS:
				//space forward over setmarks
				return(position_tape(fd, 5, mp->mt_count));
				break;
			case MTBSS:
				//space backward over setmarks
				return(position_tape(fd, 5, -mp->mt_count));
				break;
			case MTWSM:
				//write setmarks
				if(WriteTapemark(Phandle(fd), TAPE_SETMARKS, mp->mt_count,0) != NO_ERROR)
				{
					 logerr(0, "WriteTapemark(setmark)");
					 errno = EINVAL;
					 return -1;
				}
				return 0;
				break;
			case	MTGRSZ:
				 // get record size
				return(get_block_size(Phandle(fd),mp));
				break;
			case MTSRSZ:
				// set record size
				return(set_block_size(fd,mp->mt_count));
				break;
			case	MTLOAD:
				//load the tape
				return(prepare_tape(Phandle(fd), 0, 3));
				break;
			case	MTUNLOAD:
				//unload the tape
				return(prepare_tape(Phandle(fd), 0, 2));
				break;
			case MTSETPART:
				//Change the active tape partition
				return(position_tape(fd, 6, mp->mt_count));
				break;
			case MTMKPART:
				if(CreateTapePartition(hp, TAPE_SELECT_PARTITIONS, mp->mt_count,0)!= NO_ERROR)
					logerr(0, "CreateTapePartition");
				else
					return 0;
				break;
			case MTSETDENSITY:
			case MTRAS1:
			case MTRAS2:
			case MTRAS3:
			case MTBSFM:
			case MTFSFM:
			default:
				errno = ENOSYS;
				return -1;
				break;
		}
	}
	else if (opcode == 2 ) //MTIOCGET
	{
		struct mtget* mg = (struct mtget*) mt;
		errno = ENOSYS;
		return -1;
	}
	else if (opcode == 3 )  //MTIOCPOS
	{
		struct mtpos* mtp = (struct mtpos *) mt;
		DWORD current_partition, low, high;

		if(mtp == NULL)
			return -1;

		if (GetTapePosition(hp, TAPE_ABSOLUTE_POSITION, &current_partition, &low, &high) == NO_ERROR)
			return (mtp->mt_blkno = (long)low);
		else
		{
			logerr(0, "GetTapePosition");
			return -1;
		}
	}
	errno = EOPNOTSUPP ;   // when opcode is not 1,2 or 3.
	return(-1);  // Added to prevent compiler warning.
}


// Not currently supported...
static int get_tape_status(HANDLE hTape, struct mtget* mg)
{
#if 0
	mg->mt_type = ;
	mg->mt_dsreg = ;	/* ``drive status'' register */
	mg->mt_erreg = ;	/* ``error'' register */
	/* optional error info. */
	mg->mt_resid = ;	/* residual count */
	(daddr_t)	mg->mt_fileno = ;	/* file number of current position */
	(daddr_t)	mg->mt_blkno = ;	/* block number of current position */
	(u_short)	mg->mt_flags = ;
	(short)	mg->mt_bf = ;		/* optimum blocking_factor factor */
#endif
	return 0;
}

void init_tape(Devtab_t *dp)
{
	filesw(FDTYPE_TAPE, taperead, tapewrite, tapelseek, filefstat, tapeclose,0);
	dp->openfn = tape_open;
}
