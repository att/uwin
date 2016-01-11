#include	"termhdr.h"


/*
**	Set input file descriptor and type-ahead descriptor.
**	If fd < 0, only the type-ahead descriptor is set.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int typeahead(int fd)
#else
int typeahead(fd)
int	fd;
#endif
{
	if(fd < 0)
		_ahead = fd;
	else	tinputfd(fd);
	return OK;
}
