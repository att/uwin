#include	"scrhdr.h"

/*
**	Find a best match of a string in a set of strings.
**	Initial blanks are ignored.
**	subj: string to be matched
**	objs, n_objs: the set of object strings to be matched and its size
**	match:	if !NULL, return the set of strings satisfying the
**		'best matched' test.
**	n_match: if !NULL, return the # of best matches. If match != NULL,
**		n_match must be !NULL.
**	Return the first index of a best matched string.
**
**	Written by Kiem-Phong Vo
*/

#define APPROX_MATCH	0
#define PREFIX_MATCH	1
#define EXACT_MATCH	2

#define SENSITIVE	0
#define INSENSITIVE	1

#ifndef isupper
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef tolower
#define tolower(c)	(isupper(c) ? 'a' + ((c)-'A') : (c))
#endif

#define equal(a,b)	(Case == SENSITIVE ? ((a)==(b)) : (tolower(a)==tolower(b)))
#define max(x,y)	((x) > (y) ? (x) : (y))
#define skipblank(s)	while(*s == ' ' || *s == '\t') s += 1;

static int	Type = APPROX_MATCH;
static int	Case = SENSITIVE;

/*
	Set the type of match desired
*/ 
#if __STD_C
void initmatch(int type, int c_ase)
#else
void initmatch(type,c_ase)
int	type, c_ase;
#endif
{
	if(type >= 0 && type <= 2)
		Type = type;
	if(c_ase >= 0 && c_ase <= 1)
		Case = c_ase;
}



/*
	Compute the length of a longest common subsequence
	This implements Wagner-Fischer dynamic programming algorithm.
	Since we only want the length of the lcs, we can make do with
	linear space.
*/
#if __STD_C
static int lcs(uchar* one, int n_one, uchar* two, int n_two)
#else
static int lcs(one,n_one,two,n_two)
reg uchar	*one;
reg int		n_one;
reg uchar	*two;
reg int		n_two;
#endif
{
	register int	i, k;
	static int	*last, *here, n_size;

	if(n_one <= 0 || n_two <= 0)
		return 0;

	if(n_two > n_size)
	{
		if(n_size > 0)
		{
			free((char*)last);
			free((char*)here);
			n_size = 0;
		}
		if(!(last = (int*) malloc(n_two*sizeof(int))) ||
		   !(here = (int*) malloc(n_two*sizeof(int))))
			return 0;
		n_size = n_two;
	}

	/* initialize first row */
	here[0] = equal(one[0],two[0]);
	for(k = 1; k < n_two; ++k)
	{
		reg int match = equal(one[0],two[k]);
		here[k] = max(here[k-1],match);
	}

	/* do the rest of the rows */
	for(i = 1; i < n_one; ++i)
	{
		int	*t;

		/* swap here and last */
		t = here; here = last; last = t;

		/* first element of here */
		here[0] = equal(one[i],two[0]);

		/* the rest */
		for(k = 1; k < n_two; ++k)
		{
			reg int left, above, diag;

			left = here[k-1];
			above = last[k];
			diag = last[k-1]+equal(one[i],two[k]);
			here[k] = max(max(left,above),diag);
		}
	}

	return here[n_two-1];
}

#if __STD_C
int _wstrmatch(char* subj, char** objs, int n_objs, int **match, int* n_match)
#else
int _wstrmatch(subj,objs,n_objs,match,n_match)
char	*subj, **objs;
int	n_objs, **match, *n_match;
#endif
{
	reg uchar	*sub, *obj;	/* subject/object string */
	reg int		i,		/* index of current object string */
			n_sub, n_obj,	/* lengths of subject/object strings */
			best,		/* current best match */
			m_best,		/* the best match value */
			p_best,		/* the best prefix match */
			n_best,		/* number of best matches */
			exact;		/* if best match is exact */
	static int	*list, n_list;

	/* match set count */
	if(n_match)
		*n_match = 0;

	/* length of subject string */
	skipblank(subj);
	if((n_sub = strlen((char*)subj)) <= 0)
		return -1;

	if(match)
	{	/* make the list of matches if user asks for it */
		if(n_objs > n_list)
		{
			if(n_list > 0)
				free((char*)list);
			if(!(list = (int*)malloc(n_objs*sizeof(int))))
				return -1;
			n_list = n_objs;
		}
		*match = list;
	}

	/* the best match, and its score values */
	best   = -1;
	m_best = p_best = 0;
	n_best = 0;
	exact  = 0;

	for(i = 0; i < n_objs; ++i)
	{
		reg int	m_val, p_val;

		sub = (uchar*)subj;
		obj = (uchar*)objs[i];
		skipblank(obj);
		n_obj = strlen((char*)obj);

		/* don't match with shorter strings */
		if(n_sub > n_obj)
			continue;

		if(Type == APPROX_MATCH)
			m_val = lcs(sub,n_sub,obj,n_obj);

		/* match a prefix */
		for(; *sub && *obj; ++sub, ++obj)
			if(!equal(*sub,*obj))
				break;
		p_val = sub - (uchar*)subj;
		if(Type != APPROX_MATCH)
			m_val = p_val;

		switch(Type)
		{
		case EXACT_MATCH :
			/* must exhaust this string */
			if(*obj)
				continue;
			/* fall thru */

		case PREFIX_MATCH :
			/* must exhaust this string */
			if(*sub)
				continue;
			break;

		case APPROX_MATCH :
			/* must match all characters typed */
			if(m_val < n_sub)
				continue;
			break;
		}

		if(m_val > m_best ||
		   (m_val == m_best &&
		    (p_val > p_best || (p_val == n_sub && n_sub == n_obj))))
		{	/* found a better match */
			m_best = m_val;
			p_best = p_val;
			exact = m_val == n_obj ? 1 : 0;
			best = i;
			if(match)	
				list[0] = best;
			n_best = 1;
		}
		/* as good as current best */
		else if(m_val == m_best && p_val == p_best &&
			n_best > 0 && (!exact || m_val == n_obj))
		{
			if(match)
				list[n_best] = i;
			n_best += 1;
		}
	}

	if(n_match)
		*n_match = n_best;
	return best;
}
