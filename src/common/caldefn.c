/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Store variable definitions.
 *
 *  7/1/85  Greg Ward
 *
 *  11/11/85  Added conditional compiles (OUTCHAN) for control output.
 *
 *  4/2/86  Added conditional compiles for function definitions (FUNCTION).
 *
 *  1/15/88  Added clock for caching of variable values.
 *
 *  11/16/88  Added VARDEF structure for hard linking.
 *
 *  5/31/90  Added conditional compile (REDEFW) for redefinition warning.
 *
 *  4/23/91  Added ':' assignment for constant expressions
 */

#include  <stdio.h>

#include  <ctype.h>

#include  "calcomp.h"

#ifndef  NHASH
#define  NHASH		521		/* hash size (a prime!) */
#endif

#define  newnode()	(EPNODE *)ecalloc(1, sizeof(EPNODE))

extern char  *ecalloc(), *savestr();

static double  dvalue();

long  eclock = -1;			/* value storage timer */

static VARDEF  *hashtbl[NHASH];		/* definition list */
static int  htndx;			/* index for */		
static VARDEF  *htpos;			/* ...dfirst() and */
#ifdef  OUTCHAN
static EPNODE  *ochpos;			/* ...dnext */
static EPNODE  *outchan;
#endif

#ifdef  FUNCTION
EPNODE  *curfunc;
#define  dname(ep)	((ep)->v.kid->type == SYM ? \
			(ep)->v.kid->v.name : \
			(ep)->v.kid->v.kid->v.name)
#else
#define  dname(ep)	((ep)->v.kid->v.name)
#endif


fcompile(fname)			/* get definitions from a file */
char  *fname;
{
    FILE  *fp;

    if (fname == NULL)
	fp = stdin;
    else if ((fp = fopen(fname, "r")) == NULL) {
	eputs(fname);
	eputs(": cannot open\n");
	quit(1);
    }
    initfile(fp, fname, 0);
    while (nextc != EOF)
	loaddefn();
    if (fname != NULL)
	fclose(fp);
}


scompile(str, fn, ln)		/* get definitions from a string */
char  *str;
char  *fn;
int  ln;
{
    initstr(str, fn, ln);
    while (nextc != EOF)
	loaddefn();
}


double
varvalue(vname)			/* return a variable's value */
char  *vname;
{
    return(dvalue(vname, dlookup(vname)));
}


double
evariable(ep)			/* evaluate a variable */
EPNODE  *ep;
{
    register VARDEF  *dp = ep->v.ln;

    return(dvalue(dp->name, dp->def));
}


varset(vname, assign, val)	/* set a variable's value */
char  *vname;
int  assign;
double  val;
{
    register EPNODE  *ep1, *ep2;
					/* check for quick set */
    if ((ep1 = dlookup(vname)) != NULL && ep1->v.kid->type == SYM) {
	ep2 = ep1->v.kid->sibling;
	if (ep2->type == NUM) {
	    ep2->v.num = val;
	    ep1->type = assign;
	    return;
	}
    }
					/* hand build definition */
    ep1 = newnode();
    ep1->type = assign;
    ep2 = newnode();
    ep2->type = SYM;
    ep2->v.name = savestr(vname);
    addekid(ep1, ep2);
    ep2 = newnode();
    ep2->type = NUM;
    ep2->v.num = val;
    addekid(ep1, ep2);
    dremove(vname);
    dpush(ep1);
}


dclear(name)			/* delete variable definitions of name */
char  *name;
{
    register EPNODE  *ep;

    while ((ep = dpop(name)) != NULL) {
	if (ep->type == ':') {
	    dpush(ep);		/* don't clear constants */
	    return;
	}
	epfree(ep);
    }
}


dremove(name)			/* delete all definitions of name */
char  *name;
{
    register EPNODE  *ep;

    while ((ep = dpop(name)) != NULL)
	epfree(ep);
}


vardefined(name)	/* return non-zero if variable defined */
char  *name;
{
    register EPNODE  *dp;

    return((dp = dlookup(name)) != NULL && dp->v.kid->type == SYM);
}


#ifdef  OUTCHAN
chanout(cs)			/* set output channels */
int  (*cs)();
{
    register EPNODE  *ep;

    for (ep = outchan; ep != NULL; ep = ep->sibling)
	(*cs)(ep->v.kid->v.chan, evalue(ep->v.kid->sibling));

}
#endif


dcleanup(lvl)			/* clear definitions (0->vars,1->consts,2->output) */
int  lvl;
{
    register int  i;
    register VARDEF  *vp;
    register EPNODE  *ep;

    for (i = 0; i < NHASH; i++)
	for (vp = hashtbl[i]; vp != NULL; vp = vp->next)
	    if (lvl >= 1)
		dremove(vp->name);
	    else
		dclear(vp->name);
#ifdef  OUTCHAN
    if (lvl >= 2) {
	for (ep = outchan; ep != NULL; ep = ep->sibling)
	    epfree(ep);
	outchan = NULL;
    }
#endif
}


EPNODE *
dlookup(name)			/* look up a definition */
char  *name;
{
    register VARDEF  *vp;
    
    if ((vp = varlookup(name)) == NULL)
    	return(NULL);
    return(vp->def);
}


VARDEF *
varlookup(name)			/* look up a variable */
char  *name;
{
    register VARDEF  *vp;
    
    for (vp = hashtbl[hash(name)]; vp != NULL; vp = vp->next)
    	if (!strcmp(vp->name, name))
    	    return(vp);
    return(NULL);
}


VARDEF *
varinsert(name)			/* get a link to a variable */
char  *name;
{
    register VARDEF  *vp;
    int  hv;
    
    hv = hash(name);
    for (vp = hashtbl[hv]; vp != NULL; vp = vp->next)
    	if (!strcmp(vp->name, name)) {
    	    vp->nlinks++;
    	    return(vp);
    	}
    vp = (VARDEF *)emalloc(sizeof(VARDEF));
    vp->name = savestr(name);
    vp->nlinks = 1;
    vp->def = NULL;
    vp->lib = NULL;
    vp->next = hashtbl[hv];
    hashtbl[hv] = vp;
    return(vp);
}


varfree(ln)				/* release link to variable */
register VARDEF  *ln;
{
    register VARDEF  *vp;
    int  hv;

    if (--ln->nlinks > 0)
    	return;				/* still active */

    hv = hash(ln->name);
    vp = hashtbl[hv];
    if (vp == ln)
    	hashtbl[hv] = vp->next;
    else {
    	while (vp->next != ln)		/* must be in list */
    		vp = vp->next;
    	vp->next = ln->next;
    }
    freestr(ln->name);
    efree((char *)ln);
}


EPNODE *
dfirst()			/* return pointer to first definition */
{
    htndx = 0;
    htpos = NULL;
#ifdef  OUTCHAN
    ochpos = outchan;
#endif
    return(dnext());
}


EPNODE *
dnext()				/* return pointer to next definition */
{
    register EPNODE  *ep;

    while (htndx < NHASH) {
    	if (htpos == NULL)
    		htpos = hashtbl[htndx++];
    	while (htpos != NULL) {
    	    ep = htpos->def;
    	    htpos = htpos->next;
    	    if (ep != NULL)
    	    	return(ep);
    	}
    }
#ifdef  OUTCHAN
    if ((ep = ochpos) != NULL)
    	ochpos = ep->sibling;
    return(ep);
#else
    return(NULL);
#endif
}


EPNODE *
dpop(name)			/* pop a definition */
char  *name;
{
    register VARDEF  *vp;
    register EPNODE  *dp;
    
    if ((vp = varlookup(name)) == NULL || vp->def == NULL)
    	return(NULL);
    dp = vp->def;
    vp->def = dp->sibling;
    varfree(vp);
    return(dp);
}


dpush(ep)			/* push on a definition */
register EPNODE  *ep;
{
    register VARDEF  *vp;

    vp = varinsert(dname(ep));
    ep->sibling = vp->def;
    vp->def = ep;
}


#ifdef  OUTCHAN
addchan(sp)			/* add an output channel assignment */
EPNODE  *sp;
{
    int  ch = sp->v.kid->v.chan;
    register EPNODE  *ep, *epl;

    for (epl = NULL, ep = outchan; ep != NULL; epl = ep, ep = ep->sibling)
	if (ep->v.kid->v.chan >= ch) {
	    if (epl != NULL)
		epl->sibling = sp;
	    else
		outchan = sp;
	    if (ep->v.kid->v.chan > ch)
		sp->sibling = ep;
	    else {
		sp->sibling = ep->sibling;
		epfree(ep);
	    }
	    return;
	}
    if (epl != NULL)
	epl->sibling = sp;
    else
	outchan = sp;
    sp->sibling = NULL;

}
#endif


loaddefn()			/* load next definition */
{
    register EPNODE  *ep;
    EPNODE  *lastdef;

    if (nextc == ';') {		/* empty statement */
	scan();
	return;
    }
#ifdef  OUTCHAN
    if (nextc == '$') {		/* channel assignment */
	ep = getchan();
	addchan(ep);
    } else
#endif
    {				/* ordinary definition */
	ep = getdefn();
#ifdef  REDEFW
	if ((lastdef = dlookup(dname(ep))) != NULL) {
	    wputs(dname(ep));
	    if (lastdef->type == ':')
		wputs(": redefined constant expression\n");
	    else
		wputs(": redefined\n");
	}
#ifdef  FUNCTION
	else if (ep->v.kid->type == FUNC &&
			liblookup(ep->v.kid->v.kid->v.name) != NULL) {
	    wputs(ep->v.kid->v.kid->v.name);
	    wputs(": redefined library function\n");
	}
#endif
#endif
	if (ep->type == ':')
	    dremove(dname(ep));
	else
	    dclear(dname(ep));
	dpush(ep);
    }
    if (nextc != EOF) {
	if (nextc != ';')
	    syntax("';' expected");
	scan();
    }
}


EPNODE *
getdefn()			/* A -> SYM = E1 */
				/*	SYM : E1 */
				/*      FUNC(SYM,..) = E1 */
				/*	FUNC(SYM,..) : E1 */
{
    register EPNODE  *ep1, *ep2;

    if (!isalpha(nextc))
	syntax("illegal variable name");

    ep1 = newnode();
    ep1->type = SYM;
    ep1->v.name = savestr(getname());

#ifdef  FUNCTION
    if (nextc == '(') {
	ep2 = newnode();
	ep2->type = FUNC;
	addekid(ep2, ep1);
	ep1 = ep2;
	do {
	    scan();
	    if (!isalpha(nextc))
		syntax("illegal variable name");
	    ep2 = newnode();
	    ep2->type = SYM;
	    ep2->v.name = savestr(getname());
	    addekid(ep1, ep2);
	} while (nextc == ',');
	if (nextc != ')')
	    syntax("')' expected");
	scan();
	curfunc = ep1;
    } else
	curfunc = NULL;
#endif

    if (nextc != '=' && nextc != ':')
	syntax("'=' or ':' expected");

    ep2 = newnode();
    ep2->type = nextc;
    scan();
    addekid(ep2, ep1);
    addekid(ep2, getE1());

    if (
#ifdef  FUNCTION
    	    ep1->type == SYM &&
#endif
	    ep1->sibling->type != NUM) {
	ep1 = newnode();
	ep1->type = TICK;
	ep1->v.tick = -1;
	addekid(ep2, ep1);
	ep1 = newnode();
	ep1->type = NUM;
	addekid(ep2, ep1);
    }

    return(ep2);
}


#ifdef  OUTCHAN
EPNODE *
getchan()			/* A -> $N = E1 */
{
    register EPNODE  *ep1, *ep2;

    if (nextc != '$')
	syntax("missing '$'");
    scan();

    ep1 = newnode();
    ep1->type = CHAN;
    ep1->v.chan = getinum();

    if (nextc != '=')
	syntax("'=' expected");
    scan();

    ep2 = newnode();
    ep2->type = '=';
    addekid(ep2, ep1);
    addekid(ep2, getE1());

    return(ep2);
}
#endif



/*
 *  The following routines are for internal use only:
 */


static double
dvalue(name, d)			/* evaluate a variable */
char  *name;
EPNODE  *d;
{
    register EPNODE  *ep1, *ep2;
    
    if (d == NULL || d->v.kid->type != SYM) {
	eputs(name);
	eputs(": undefined variable\n");
	quit(1);
    }
    ep1 = d->v.kid->sibling;			/* get expression */
    if (ep1->type == NUM)
	return(ep1->v.num);			/* return if number */
    ep2 = ep1->sibling;				/* check time */
    if (ep2->v.tick < 0 || ep2->v.tick < eclock) {
	ep2->v.tick = d->type == ':' ? 1L<<30 : eclock;
	ep2 = ep2->sibling;
	ep2->v.num = evalue(ep1);		/* needs new value */
    } else
	ep2 = ep2->sibling;			/* else reuse old value */

    return(ep2->v.num);
}


static int
hash(s)				/* hash a string */
register char  *s;
{
    register int  rval = 0;

    while (*s)
	rval += *s++;
    
    return(rval % NHASH);
}
