#include	<stdio.h>

int yyerror(const char *string)
{
	return(fprintf(stderr, "%s\n",string));
}
