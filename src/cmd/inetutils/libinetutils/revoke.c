/* stub revoke */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>

#ifndef HAVE_ERRNO_DECL
extern int errno;
#endif

int revoke (path)
     char *path;
{
#ifdef ENOSYS
  errno = ENOSYS;
#else
  errno = EINVAL; /* ? */
#endif
  return -1;
}
