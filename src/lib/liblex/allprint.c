# include <stdio.h>
# include <ctype.h>

extern FILE *yyout;

void allprint(char c)
{
	switch(c)
	{
		case '\n':
			fprintf(yyout,"\\n");
			break;
		case '\t':
			fprintf(yyout,"\\t");
			break;
		case '\b':
			fprintf(yyout,"\\b");
			break;
		case ' ':
			fprintf(yyout,"\\\bb");
			break;
		default:
			if(!isprint(c))
				fprintf(yyout,"\\%-3o",c);
			else 
				putc(c,yyout);
			break;
		}
	return;
}

void sprint(char *s)
{
	while(*s)
		allprint(*s++);
	return;
}

