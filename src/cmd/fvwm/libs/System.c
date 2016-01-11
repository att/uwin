/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
** System.c: code for dealing with various OS system call variants
*/

#include "config.h"
#include "fvwmlib.h"


#if HAVE_UNAME
#include <sys/utsname.h>
#endif
#include <sys/stat.h>

/* needed for QNX to define FD_SETSIZE */
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif


/*
** just in case...
*/
#ifndef FD_SETSIZE
#define FD_SETSIZE 2048
#endif


int GetFdWidth(void)
{
#if HAVE_SYSCONF
    return min(sysconf(_SC_OPEN_MAX),FD_SETSIZE);
#else
    return min(getdtablesize(),FD_SETSIZE);
#endif
}


/* return a string indicating the OS type (i.e. "Linux", "SINIX-D", ... ) */
int getostype(char *buf, int max)
{
#if HAVE_UNAME
    struct utsname sysname;

    if ( uname( &sysname ) >= 0 ) {
        buf[0] = '\0';
	strncat( buf, sysname.sysname, max );
	return 0;
    }
#endif
    strcpy (buf,"");
    return -1;
}


/**
 * Set a colon-separated path, with environment variable expansions,
 * and expand '+' to be the value of the previous path.
 **/
void setPath( char** p_path, const char* newpath, int free_old_path )
{
    char* oldpath = *p_path;
    int oldlen = strlen( oldpath );
    char* stripped_path = stripcpy( newpath );
    int found_plus = strchr( newpath, '+' ) != NULL;

    /** Leave room for the old path, if we find a '+' in newpath **/
    *p_path = envDupExpand( stripped_path, found_plus ? oldlen - 1 : 0 );
    free( stripped_path );

    if ( found_plus ) {
	char* p = strchr( *p_path, '+' );
	memmove(p + oldlen, p + 1, strlen(p+1) + 1);
	memmove(p, oldpath, oldlen);
    }

    if ( free_old_path )
	free( oldpath );
}


/****************************************************************************
 *
 * Find the specified file somewhere along the given path.
 *
 * There is a possible race condition here:  We check the file and later
 * do something with it.  By then, the file might not be accessible.
 * Oh well.
 *
 ****************************************************************************/
char* searchPath( const char* pathlist, const char* filename,
		  const char* suffix, int type )
{
    char* path;
    const char* dir_end;
    const char* rel_end;
    int l;
    int rel;

    if ( filename == NULL || *filename == 0 )
	return NULL;

    l = (pathlist) ? strlen(pathlist) : 0;
    l += (suffix) ? strlen(suffix) : 0;

    /* +1 for extra / and +1 for null termination */
    path = safemalloc( strlen(filename) + l + 2 );
    *path = '\0';

    if (*filename == '/' || pathlist == NULL || *pathlist == '\0')
    {
	/* No search if filename begins with a slash */
	/* No search if pathlist is empty */
	strcpy( path, filename );
	return path;
    }
    if (rel = (type < 0))
	type = -type;

    /* Search each element of the pathlist for the file */
    while ((pathlist)&&(*pathlist))
    {
	rel_end = 0;
	dir_end = pathlist;
	for (;;)
	{
	    switch (*dir_end++)
	    {
	    case 0:
		dir_end = 0;
		break;
	    case ':':
		dir_end--;
		break;
	    case '/':
		rel_end = dir_end - 1;
		break;
	    }
	    break;
	}
	if (!rel || !rel_end)
	    rel_end = dir_end;
	if (rel_end != NULL)
	{
	    strncpy(path, pathlist, rel_end - pathlist);
	    path[rel_end - pathlist] = 0;
	}
	else
	    strcpy(path, pathlist);

	strcat(path, "/");
	strcat(path, filename);
	if (access(path, type) == 0)
	    return path;

	if ( suffix && *suffix != 0 ) {
	    strcat( path, suffix );
	    if (access(path, type) == 0)
		return path;
	}

	/* Point to next element of the path */
	if(dir_end == NULL)
	    break;
	else
	    pathlist = dir_end + 1;
    }
    /* Hmm, couldn't find the file.  Return NULL */
    free(path);
    return NULL;
}

/*
 * void setFileStamp(FileStamp *stamp, const char *name);
 * Bool isFileStampChanged(const FileStamp *stamp, const char *name);
 *
 * An interface for verifying cached files.
 * The first function associates a file stamp with file (named by name).
 * The second function returns True or False in case the file was changed
 * from the time the stamp was associated.
 *
 * FileStamp can be a structure; try to save memory by evaluating a checksum.
 */

FileStamp getFileStamp(const char *name)
{
  static struct stat buf;

  if (!name || stat(name, &buf))
    return 0;
  return
    ((FileStamp)buf.st_mtime << 13) + (FileStamp)buf.st_size;
}

void setFileStamp(FileStamp *stamp, const char *name)
{
  *stamp = getFileStamp(name);
}

Bool isFileStampChanged(const FileStamp *stamp, const char *name)
{
  return
    *stamp != getFileStamp(name);
}

/*
 * this code along with the -type change to searchPath() above
 * makes a position independent (and $PATH relative) fvwm possible
 */

#ifdef FVWM_CONFSUBDIR
static char* confdir;
#endif
#ifdef FVWM_DATASUBDIR
static char* datadir;
#endif
#ifdef FVWM_MODULESUBDIR
static char* moduledir;
#endif

#if dfined(FVWM_CONFSUBDIR) || defined(FVWM_DATASUBDIR) || defined(FVWM_MODULESUBDIR)
static void initdirs(void)
{
  char* path = getenv("PATH");
#ifdef FVWM_CONFSUBDIR
  if (!confdir && !(confdir = searchPath(path, FVWM_CONFSUBDIR, NULL, -X_OK)))
    confdir = "/usr/" FVWM_CONFSUBDIR;
#endif
#ifdef FVWM_DATASUBDIR
  if (!datadir && !(datadir = searchPath(path, FVWM_DATASUBDIR, NULL, -X_OK)))
    datadir = "/usr/" FVWM_DATASUBDIR;
#endif
#ifdef FVWM_MODULESUBDIR
  if (!moduledir && !(moduledir = searchPath(path, FVWM_MODULESUBDIR, NULL, -X_OK)))
    moduledir = "/usr/" FVWM_MODULESUBDIR;
#endif
}
#endif

#ifdef FVWM_CONFSUBDIR
char* fvwm_confdir(void)
{
  if (!confdir)
    initdirs();
  return confdir;
}
#endif

#ifdef FVWM_DATASUBDIR
char* fvwm_datadir(void)
{
  if (!datadir)
    initdirs();
  return datadir;
}
#endif

#ifdef FVWM_MODULESUBDIR
char* fvwm_moduledir(void)
{
  if (!moduledir)
    initdirs();
  return moduledir;
}
#endif
