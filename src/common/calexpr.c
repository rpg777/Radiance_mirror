/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Compute data values using expression parser
 *
 *  7/1/85  Greg Ward
 *
 *  11/11/85  Made channel input conditional with (INCHAN) compiles.
 *
 *  4/2/86  Added conditional compiles for function definitions (FUNCTION).
 *
 *  1/29/87  Made variables conditional (VARIABLE)
 *
 *  5/19/88  Added constant subexpression elimination (RCONST)
 */

#include  <stdio.h>

#include  <ctype.h>

#include  <errno.h>

#include  "calcomp.h"

#define  MAXLINE	256		/* maximum line length */
#define  MAXWORD	64		/* maximum word length */

#define  newnode()	(EPNODE *)ecalloc(1, sizeof(EPNODE))

#define  isid(c)	(isalnum(c) || (c) == '_' || (c) == '.')

#define  isdecimal(c)	(isdigit(c) || (c) == '.')

extern double  atof(), pow();
extern char  *fgets(), *savestr();
extern char  *emalloc(), *ecalloc();
extern EPNODE  *curfunc;
extern double  efunc(), evariable();
static double  euminus(), echannel(), eargument(), enumber();
static double  eadd(), esubtr(), emult(), edivi(), epow();
static double  ebotch();
extern int  errno;

int  nextc;				/* lookahead character */

double  (*eoper[])() = {		/* expression operations */
	ebotch,
#ifdef  VARIABLE
	evariable,
#else
	ebotch,
#endif
	enumber,
	euminus,
#ifdef  INCHAN
	echannel,
#else
	ebotch,
#endif
#ifdef  FUNCTION
	efunc,
	eargument,
#else
	ebotch,
	ebotch,
#endif
	ebotch,
	ebotch,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	emult,
	eadd,
	0,
	esubtr,
	0,
	edivi,
	0,0,0,0,0,0,0,0,0,0,
	ebotch,
	0,0,
	ebotch,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	epow,
};

static FILE  *infp;			/* input file pointer */
static char  *linbuf;			/* line buffer */
static char  *infile;			/* input file name */
static int  lineno;			/* input line number */
static int  linepos;			/* position in buffer */


EPNODE *
eparse(expr)			/* parse an expression string */
char  *expr;
{
    EPNODE  *ep;

    initstr(expr, NULL, 0);
#if  defined(VARIABLE) && defined(FUNCTION)
    curfunc = NULL;
#endif
    ep = getE1();
    if (nextc != EOF)
	syntax("unexpected character");
    return(ep);
}


double
eval(expr)			/* evaluate an expression string */
char  *expr;
{
    register EPNODE  *ep;
    double  rval;

    ep = eparse(expr);
    rval = evalue(ep);
    epfree(ep);
    return(rval);
}


epfree(epar)			/* free a parse tree */
register EPNODE  *epar;
{
    register EPNODE  *ep;

    switch (epar->type) {

#if  defined(VARIABLE) || defined(FUNCTION)
	case VAR:
	    varfree(epar->v.ln);
	    break;
#endif
	    
	case SYM:
	    freestr(epar->v.name);
	    break;

	case NUM:
	case CHAN:
	case ARG:
	case TICK:
	    break;

	default:
	    for (ep = epar->v.kid; ep != NULL; ep = ep->sibling)
		epfree(ep);
	    break;

    }

    efree((char *)epar);
}

				/* the following used to be a switch */
#ifdef  FUNCTION
static double
eargument(ep)
EPNODE  *ep;
{
    return(argument(ep->v.chan));
}
#endif

static double
enumber(ep)
EPNODE  *ep;
{
    return(ep->v.num);
}

static double
euminus(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;

    return(-evalue(ep1));
}

#ifdef  INCHAN
static double
echannel(ep)
EPNODE  *ep;
{
    return(chanvalue(ep->v.chan));
}
#endif

static double
eadd(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;

    return(evalue(ep1) + evalue(ep1->sibling));
}

static double
esubtr(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;

    return(evalue(ep1) - evalue(ep1->sibling));
}

static double
emult(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;

    return(evalue(ep1) * evalue(ep1->sibling));
}

static double
edivi(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;
    double  d;

    d = evalue(ep1->sibling);
    if (d == 0.0) {
	wputs("Division by zero\n");
	errno = ERANGE;
	return(0.0);
    }
    return(evalue(ep1) / d);
}

static double
epow(ep)
EPNODE  *ep;
{
    register EPNODE  *ep1 = ep->v.kid;
    double  d;
    int  lasterrno;

    lasterrno = errno;
    errno = 0;
    d = pow(evalue(ep1), evalue(ep1->sibling));
#ifdef  IEEE
    if (!finite(d))
	errno = EDOM;
#endif
    if (errno) {
	wputs("Illegal power\n");
	return(0.0);
    }
    errno = lasterrno;
    return(d);
}

static double
ebotch(ep)
EPNODE  *ep;
{
    eputs("Bad expression!\n");
    quit(1);
}


EPNODE *
ekid(ep, n)			/* return pointer to a node's nth kid */
register EPNODE  *ep;
register int  n;
{

    for (ep = ep->v.kid; ep != NULL; ep = ep->sibling)
	if (--n < 0)
	    break;

    return(ep);
}


int
nekids(ep)			/* return # of kids for node ep */
register EPNODE  *ep;
{
    register int  n = 0;

    for (ep = ep->v.kid; ep != NULL; ep = ep->sibling)
	n++;

    return(n);
}


initfile(fp, fn, ln)		/* prepare input file */
FILE  *fp;
char  *fn;
int  ln;
{
    static char  inpbuf[MAXLINE];

    infp = fp;
    linbuf = inpbuf;
    infile = fn;
    lineno = ln;
    linepos = 0;
    inpbuf[0] = '\0';
    scan();
}


initstr(s, fn, ln)		/* prepare input string */
char  *s;
char  *fn;
int  ln;
{
    infp = NULL;
    infile = fn;
    lineno = ln;
    linbuf = s;
    linepos = 0;
    scan();
}


scan()				/* scan next character */
{
    do {
	if (linbuf[linepos] == '\0')
	    if (infp == NULL || fgets(linbuf, MAXLINE, infp) == NULL)
		nextc = EOF;
	    else {
		nextc = linbuf[0];
		lineno++;
		linepos = 1;
	    }
	else
	    nextc = linbuf[linepos++];
	if (nextc == '{') {
	    scan();
	    while (nextc != '}')
		if (nextc == EOF)
		    syntax("'}' expected");
		else
		    scan();
	    scan();
	}
    } while (isspace(nextc));
}


char *
ltoa(l)				/* convert long to ascii */
long  l;
{
    static char  buf[16];
    register char  *cp;
    int  neg = 0;

    if (l == 0)
	return("0");
    if (l < 0) {
	l = -l;
	neg++;
    }
    cp = buf + sizeof(buf);
    *--cp = '\0';
    while (l) {
	*--cp = l % 10 + '0';
	l /= 10;
    }
    if (neg)
	*--cp = '-';
    return(cp);
}


syntax(err)			/* report syntax error and quit */
char  *err;
{
    register int  i;

    if (infile != NULL || lineno != 0) {
	if (infile != NULL) eputs(infile);
	if (lineno != 0) {
	    eputs(infile != NULL ? ", line " : "line ");
	    eputs(ltoa((long)lineno));
	}
	eputs(": syntax error:\n");
    }
    eputs(linbuf);
    if (linbuf[strlen(linbuf)-1] != '\n')
	eputs("\n");
    for (i = 0; i < linepos-1; i++)
	eputs(linbuf[i] == '\t' ? "\t" : " ");
    eputs("^ ");
    eputs(err);
    eputs("\n");
    quit(1);
}


addekid(ep, ekid)			/* add a child to ep */
register EPNODE  *ep;
EPNODE  *ekid;
{
    if (ep->v.kid == NULL)
	ep->v.kid = ekid;
    else {
	for (ep = ep->v.kid; ep->sibling != NULL; ep = ep->sibling)
	    ;
	ep->sibling = ekid;
    }
    ekid->sibling = NULL;
}


char *
getname()			/* scan an identifier */
{
    static char  str[MAXWORD+1];
    register int  i;

    for (i = 0; i < MAXWORD && isid(nextc); i++, scan())
	str[i] = nextc;
    str[i] = '\0';

    return(str);
}


int
getinum()			/* scan a positive integer */
{
    register int  n;

    n = 0;
    while (isdigit(nextc)) {
	n = n * 10 + nextc - '0';
	scan();
    }
    return(n);
}


double
getnum()			/* scan a positive float */
{
    register int  i;
    char  str[MAXWORD+1];

    i = 0;
    while (isdigit(nextc) && i < MAXWORD) {
	str[i++] = nextc;
	scan();
    }
    if (nextc == '.' && i < MAXWORD) {
    	str[i++] = nextc;
    	scan();
	while (isdigit(nextc) && i < MAXWORD) {
	    str[i++] = nextc;
	    scan();
	}
    }
    if ((nextc == 'e' || nextc == 'E') && i < MAXWORD) {
    	str[i++] = nextc;
    	scan();
	if ((nextc == '-' || nextc == '+') && i < MAXWORD) {
	    str[i++] = nextc;
	    scan();
	}
	while (isdigit(nextc) && i < MAXWORD) {
	    str[i++] = nextc;
	    scan();
	}
    }
    str[i] = '\0';

    return(atof(str));
}


EPNODE *
getE1()				/* E1 -> E1 ADDOP E2 */
				/*       E2 */
{
    register EPNODE  *ep1, *ep2;

    ep1 = getE2();
    while (nextc == '+' || nextc == '-') {
	ep2 = newnode();
	ep2->type = nextc;
	scan();
	addekid(ep2, ep1);
	addekid(ep2, getE2());
#ifdef  RCONST
	if (ep1->type == NUM && ep1->sibling->type == NUM)
		ep2 = rconst(ep2);
#endif
	ep1 = ep2;
    }
    return(ep1);
}


EPNODE *
getE2()				/* E2 -> E2 MULOP E3 */
				/*       E3 */
{
    register EPNODE  *ep1, *ep2;

    ep1 = getE3();
    while (nextc == '*' || nextc == '/') {
	ep2 = newnode();
	ep2->type = nextc;
	scan();
	addekid(ep2, ep1);
	addekid(ep2, getE3());
#ifdef  RCONST
	if (ep1->type == NUM && ep1->sibling->type == NUM)
		ep2 = rconst(ep2);
#endif
	ep1 = ep2;
    }
    return(ep1);
}


EPNODE *
getE3()				/* E3 -> E4 ^ E3 */
				/*       E4 */
{
    register EPNODE  *ep1, *ep2;

    ep1 = getE4();
    if (nextc == '^') {
	ep2 = newnode();
	ep2->type = nextc;
	scan();
	addekid(ep2, ep1);
	addekid(ep2, getE3());
#ifdef  RCONST
	if (ep1->type == NUM && ep1->sibling->type == NUM)
		ep2 = rconst(ep2);
#endif
	return(ep2);
    }
    return(ep1);
}


EPNODE *
getE4()				/* E4 -> ADDOP E5 */
				/*       E5 */
{
    register EPNODE  *ep1, *ep2;

    if (nextc == '-') {
	scan();
	ep2 = getE5();
	if (ep2->type == NUM) {
		ep2->v.num = -ep2->v.num;
		return(ep2);
	}
	ep1 = newnode();
	ep1->type = UMINUS;
	addekid(ep1, ep2);
	return(ep1);
    }
    if (nextc == '+')
	scan();
    return(getE5());
}


EPNODE *
getE5()				/* E5 -> (E1) */
				/*       VAR */
				/*       NUM */
				/*       $N */
				/*       FUNC(E1,..) */
				/*       ARG */
{
    int  i;
    register EPNODE  *ep1, *ep2;

    if (nextc == '(') {
	scan();
	ep1 = getE1();
	if (nextc != ')')
	    syntax("')' expected");
	scan();
	return(ep1);
    }

#ifdef  INCHAN
    if (nextc == '$') {
	scan();
	ep1 = newnode();
	ep1->type = CHAN;
	ep1->v.chan = getinum();
	return(ep1);
    }
#endif

#if  defined(VARIABLE) || defined(FUNCTION)
    if (isalpha(nextc)) {
	ep1 = newnode();
	ep1->type = VAR;
	ep1->v.ln = varinsert(getname());

#if  defined(VARIABLE) && defined(FUNCTION)
	if (curfunc != NULL)
	    for (i = 1, ep2 = curfunc->v.kid->sibling;
	    			ep2 != NULL; i++, ep2 = ep2->sibling)
		if (!strcmp(ep2->v.name, ep1->v.ln->name)) {
		    epfree(ep1);
		    ep1 = newnode();
		    ep1->type = ARG;
		    ep1->v.chan = i;
		    break;
		}
#endif
#ifdef  FUNCTION
	if (nextc == '(') {
	    ep2 = newnode();
	    ep2->type = FUNC;
	    addekid(ep2, ep1);
	    ep1 = ep2;
	    do {
		scan();
		addekid(ep1, getE1());
	    } while (nextc == ',');
	    if (nextc != ')')
		syntax("')' expected");
	    scan();
	}
#ifndef  VARIABLE
	else
	    syntax("'(' expected");
#endif
#endif
#ifdef  RCONST
	if (isconstvar(ep1))
	    ep1 = rconst(ep1);
#endif
	return(ep1);
    }
#endif

    if (isdecimal(nextc)) {
	ep1 = newnode();
	ep1->type = NUM;
	ep1->v.num = getnum();
	return(ep1);
    }
    syntax("unexpected character");
}


#ifdef  RCONST
EPNODE *
rconst(epar)			/* reduce a constant expression */
register EPNODE  *epar;
{
    register EPNODE  *ep;

    ep = newnode();
    ep->type = NUM;
    errno = 0;
    ep->v.num = evalue(epar);
    if (errno)
    	syntax("bad constant expression");
    epfree(epar);
 
    return(ep);
}


isconstvar(ep)			/* is ep linked to a constant expression? */
register EPNODE  *ep;
{
#ifdef  VARIABLE
    register EPNODE  *ep1;
#ifdef  FUNCTION

    if (ep->type == FUNC) {
	if (!isconstfun(ep->v.kid))
		return(0);
	for (ep1 = ep->v.kid->sibling; ep1 != NULL; ep1 = ep1->sibling)
	    if (ep1->type != NUM && !isconstfun(ep1))
		return(0);
	return(1);
    }
#endif
    if (ep->type != VAR)
	return(0);
    ep1 = ep->v.ln->def;
    if (ep1 == NULL || ep1->type != ':')
	return(0);
#ifdef  FUNCTION
    if (ep1->v.kid->type != SYM)
	return(0);
#endif
    return(1);
#else
    return(ep->type == FUNC);
#endif
}


#if  defined(FUNCTION) && defined(VARIABLE)
isconstfun(ep)			/* is ep linked to a constant function? */
register EPNODE  *ep;
{
    register EPNODE  *dp;
    register LIBR  *lp;

    if (ep->type != VAR)
	return(0);
    dp = ep->v.ln->def;
    if (dp != NULL && dp->type != ':')
	return(0);
    if ((dp == NULL || dp->v.kid->type != FUNC)
	    && ((lp = liblookup(ep->v.ln->name)) == NULL
		    || lp->atyp != ':'))
	return(0);
    return(1);
}
#endif
#endif
