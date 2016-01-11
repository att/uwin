#include	"termhdr.h"
#include	<stdio.h>

#ifndef	_CHCTRL
#define	_CHCTRL(c)	('c' & 037)
#endif	/* _CHCTRL */

#if __STD_C
static char* _branchto(reg char* cp, char to)
#else
static char* _branchto(cp, to)
reg char	*cp;
char		to;
#endif
{
	reg int		level = 0;
	reg char	c;

	while((c = *cp++) != 0)
	{
		if (c == '%')
		{	if ((c = *cp++) == to || c == ';')
	    		{	if (level == 0)
		    			return (cp);
	    		}
	    		if (c == '?')
				level++;
	    		if (c == ';')
				level--;
		}
	}
	return NIL(char*);
}

/*
 * Routine to perform parameter substitution.
 * instring is a string containing printf type escapes.
 * The whole thing uses a stack, much like an HP 35.
 * The following escapes are defined for substituting row/column:
 *
 *	%[:[-+ #0]][0-9][.][0-9][dsoxX]
 *		print pop() as in printf(3), as defined in the local
 *		sprintf(3), except that a leading + or - must be preceded
 *		with a colon (:) to distinguish from the plus/minus operators.
 *
 *	%c	print pop() like %c in printf(3)
 *	%l	pop() a string address and push its length.
 *	%P[a-z] set dynamic variable a-z
 *	%g[a-z] get dynamic variable a-z
 *	%P[A-Z] set static variable A-Z
 *	%g[A-Z] get static variable A-Z
 *
 *	%p[1-0]	push ith parm
 *	%'c'	char constant c
 *	%{nn}	integer constant nn
 *
 *	%+ %- %* %/ %m		arithmetic (%m is mod): push(pop() op pop())
 *	%& %| %^		bit operations:		push(pop() op pop())
 *	%= %> %<		logical operations:	push(pop() op pop())
 *	%A %O			logical AND, OR		push(pop() op pop())
 *	%! %~			unary operations	push(op pop())
 *	%%			output %
 *	%? expr %t thenpart %e elsepart %;
 *				if-then-else, %e elsepart is optional.
 *				else-if's are possible ala Algol 68:
 *				%? c1 %t %e c2 %t %e c3 %t %e c4 %t %e %;
 *	% followed by anything else
 *				is not defined, it may output the character,
 *				and it may not. This is done so that further
 *				enhancements to the format capabilities may
 *				be made without worrying about being upwardly
 *				compatible from buggy code.
 *
 * all other characters are ``self-inserting''.  %% gets % output.
 *
 * The stack structure used here is based on an idea by Joseph Yao.
 */

#define	_PUSH(i)	(stack[++top] = (Void_t*)(i))
#define	_POP()		(stack[top--])

#if __STD_C
char* tparm(char* instring, ...)
#else
char* tparm(va_alist)
va_dcl
#endif
{
	static char	result[512];
	static char	added[100];
	char		*vars[26];
	char		*regs[128];
	reg int		c, iop;
	reg char	*op;
	int		sign;
	int		onrow = 0;
	char		*xp;
	char		formatbuffer[100];
	char		*format;
	int		looping;
	char		*p[10];
	Void_t		*stack[10];
	reg char	*cp, *outp, *outstr;
	int		p_n, top;
	va_list		args;
/* the real argument list that tparm likes */
#define _SETP(i) 	while(p_n <= i) p[p_n++] = va_arg(args,char*);

#if __STD_C
	va_start(args,instring);
#else
	char		*instring;
	va_start(args);
	instring = va_arg(args,char*);
#endif

	if (!instring )
	{	outstr = NIL(char*);
		goto done;
	}

	p[1] = va_arg(args,char*);
	p[2] = va_arg(args,char*);
	p_n = 3;

	if(istermcap())
	{	if(instring == T_cup || instring == T_mrcup)
			outstr = tgoto(instring,p[2],p[1]);
		else	outstr = tgoto(instring,p[1],p[2]);
		goto done;
	}

	cp = instring;
	outp = outstr = result;
	top = 0;
	added[0] = 0;
	stack[0] = NIL(char*);
	while ((c = *cp++) != 0)
    	{
		if (c != '%')
		{	*outp++ = c;
	    		continue;
		}
		op = stack[top];
		iop = (int)op;
		switch (c = *cp++)
		{
	    	/* PRINTING CASES */
	    	case ':':
	    	case ' ':
	    	case '#':
	    	case '0':
	    	case '1':
	    	case '2':
	    	case '3':
	    	case '4':
	    	case '5':
	    	case '6':
	    	case '7':
	    	case '8':
	    	case '9':
	    	case '.':
	    	case 'd':
	    	case 's':
	    	case 'o':
	    	case 'x':
	    	case 'X':
			format = formatbuffer;
			*format++ = '%';

			/* leading ':' to allow +/- in format */
			if (c == ':')
		    		c = *cp++;

			/* take care of flags, width and precision */
			looping = 1;
			while (c && looping)
		    	switch (c)
		    	{
			case '-':
			case '+':
			case ' ':
			case '#':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
				*format++ = c;
			    	c = *cp++;
			    	break;
			default:
				looping = 0;
				break;
		    	}

			/* add in the conversion type */
			switch (c)
			{
		    	case 'd':
		    	case 's':
		    	case 'o':
		    	case 'x':
		    	case 'X':
				*format++ = c;
				break;
		    	default:
			    	outstr = NIL(char*);
				goto done;
			}
			*format = '\0';

			/*
		 	 * Pass off the dirty work to sprintf.
			 * It's debatable whether we should just pull in
			 * the appropriate code here. I decided not to for now.
		 	*/
			if (c == 's')
		    		(void) sprintf(outp, formatbuffer, op);
			else	(void) sprintf(outp, formatbuffer, iop);
			/*
			 * Advance outp past what sprintf just did. 
			 * sprintf returns an indication of its length on some
			 * systems, others the first char, and there's
			 * no easy way to tell which. The Sys V on
			 * BSD emulations are particularly confusing.
			 */
			while (*outp)
		    		outp++;
			(void) _POP();
			continue;

	    	case 'c':
			/*
			 * This code is worth scratching your head at for a
			 * while.  The idea is that various weird things can
			 * happen to nulls, EOT's, tabs, and newlines by the
			 * tty driver, arpanet, and so on, so we don't send
			 * them if we can help it.  So we instead alter the
			 * place being addessed and then move the cursor
			 * locally using UP or RIGHT.
			 *
			 * This is a kludge, clearly.  It loses if the
			 * parameterized string isn't addressing the cursor
			 * (but hopefully that is all that %c terminals do
			 * with parms).  Also, since tab and newline happen
			 * to be next to each other in ASCII, if tab were
			 * included a loop would be needed.  Finally, note
			 * that lots of other processing is done here, so
			 * this hack won't always work (e.g. the Ann Arbor
			 * 4080, which uses %B and then %c.)
			 */
			switch (iop)
			{
		    	/*
		    	 * Null.  Problem is that our 
		   	 * output is, by convention, null terminated.
		   	 */
		    	case 0:
				iop = 0200;   /* Parity should be ignored */
				break;
			/*
			 * Control D.  Problem is that certain very ancient
			 * hardware hangs up on this, so the current (!)
			 * UNIX tty driver doesn't xmit control D's.
			 */
		    	case _CHCTRL(d):
			/*
			 * Newline.  Problem is that UNIX will expand
			 * this to CRLF.
			 */
		    	case '\n':
				xp = onrow ? T_cud1 : T_cuf1;
				if (onrow && xp && iop < T_lines-1 && T_cuu1)
				{
			    		iop += 2;
			    		xp = T_cuu1;
				}
				if (xp && instring == T_cup)
				{
			    		(void) strcat(added, xp);
			    		iop--;
				}
				break;
			/*
			 * Tab used to be in this group too,
			 * because UNIX might expand it to blanks.
			 * We now require that this tab mode be turned
			 * off by any program using this routine,
			 * or using termcap in general, since some
			 * terminals use tab for other stuff, like
			 * nondestructive space.  (Filters like ul
			 * or vcrt will lose, since they can't stty.)
			 * Tab was taken out to get the Ann Arbor
			 * 4080 to work.
			 */
			}

			*outp++ = iop;
			(void) _POP();
			break;

	    	case 'l':
			_PUSH(strlen(_POP()));
			break;

	    	case '%':
			*outp++ = c;
			break;

	    	/*
	    	 * %i: shorthand for increment first two parms.
	    	 * Useful for terminals that start numbering from
	    	 * one instead of zero (like ANSI terminals).
	    	 */
	    	case 'i':
			p[1]++;
			p[2]++;
			break;

	    	/* %pi: push the ith parameter */
	    	case 'p':
			c = *cp++ - '0';
			if(c < 1 || c > 9)
			{	outstr = NIL(char*);
				goto done;
			}
			_SETP(c);
			_PUSH(p[c]);
			onrow = (c == 1) ? 1 : 0;
			break;

		/* %Pi: pop from stack into variable i (a-z) */
	    	case 'P':
			if (*cp >= 'a' && *cp <= 'z')
		    		vars[*cp++ - 'a'] = _POP();
			else if (*cp >= 'A' && *cp <= 'Z')
				regs[*cp++ - 'a'] = _POP();
			break;

	    	/* %gi: push variable i (a-z) */
	    	case 'g':
			if (*cp >= 'a' && *cp <= 'z')
		    		_PUSH(vars[*cp++ - 'a']);
			else if (*cp >= 'A' && *cp <= 'Z')
				_PUSH(regs[*cp++ - 'a']);
			break;

	    	/* %'c' : character constant */
	    	case '\'':
			_PUSH((int)(*cp++));
			if (*cp++ != '\'')
			{	outstr = NIL(char*);
				goto done;
		    	}
			break;

	    	/* %{nn} : integer constant.  */
	    	case '{':
			iop = 0;
			sign = 1;
			if (*cp == '-')
			{
		    		sign = -1;
		    		cp++;
			}
			else if (*cp == '+')
				cp++;
			while ((c = *cp++) >= '0' && c <= '9')
				iop = 10 * iop + c - '0';
			if (c != '}')
		    	{	outstr = NIL(char*);
				goto done;
			}
			_PUSH(sign * iop);
			break;

	    	/* binary operators */
	    	case '+':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop + c);
			break;
	    	case '-':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop - c);
			break;
	    	case '*':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop * c);
			break;
	    	case '/':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop / c);
			break;
	    	case 'm':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop % c);
			break; /* %m: mod */
	    	case '&':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop & c);
			break;
	    	case '|':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop | c);
			break;
	    	case '^':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop ^ c);
			break;
	    	case '=':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop == c);
			break;
	    	case '>':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop > c);
			break;
	    	case '<':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop < c);
			break;
	    	case 'A':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop && c);
			break; /* AND */
	    	case 'O':
			c = (int)_POP();
			iop = (int)_POP();
			_PUSH(iop || c);
			break; /* OR */

	    	/* Unary operators. */
	    	case '!':
			iop = (int)_POP();
			_PUSH(!iop);
			break;
	    	case '~':
			iop = (int)_POP();
			_PUSH(~iop);
			break;
	    	/* Sorry, no unary minus, because minus is binary. */

	    	/*
	    	 * If-then-else.  Implemented by a low level hack of
	    	 * skipping forward until the match is found, counting
	    	 * nested if-then-elses.
	    	 */
	    	case '?':	/* IF - just a marker */
			break;

	    	case 't':	/* THEN - branch if false */
			if (!_POP())
		    	cp = _branchto(cp, 'e');
			break;

	    	case 'e':	/* ELSE - branch to ENDIF */
			cp = _branchto(cp, ';');
			break;

	    	case ';':	/* ENDIF - just a marker */
			break;

	    	default:
			outstr = NIL(char*);
			goto done;
		}
    	}

done:
	va_end(args);
	if(outstr && outp)
    		(void) strcpy(outp, added);
    	return (outstr);
}
