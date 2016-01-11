extern char yytext[];
extern int yyleng;
extern int yyprevious;

void yyless(int x)
{
	register char *lastch, *ptr;
	lastch = yytext+yyleng;
	if (x>=0 && x <= yyleng)
		ptr = x + yytext;
	else
	/*
	 * The cast on the next line papers over an unconscionable nonportable
	 * glitch to allow the caller to hand the function a pointer instead of
	 * an integer and hope that it gets figured out properly.  But it's
	 * that way on all systems .   
	 */
		ptr = (char *) x;
	while (lastch > ptr)
		yyunput(*--lastch);
	*lastch = 0;
	if (ptr >yytext)
		yyprevious = *--lastch;
	yyleng = ptr-yytext;
}
