/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  readoct.c - routines to read octree information.
 *
 *     7/30/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "otypes.h"

static double  ogetflt();
static long  ogetint();
static char  *ogetstr();
static int  getobj(), octerror();
static OCTREE  getfullnode(), gettree();

static char  *infn;			/* input file name */
static FILE  *infp;			/* input file stream */
static int  objsize;			/* size of stored OBJECT's */
static OBJECT  objorig;			/* zeroeth object */
static short  otypmap[NUMOTYPE+8];	/* object type map */


int
readoct(fname, load, scene, ofn)	/* read in octree from file */
char  *fname;
int  load;
CUBE  *scene;
char  *ofn[];
{
	extern int  fputs();
	char  sbuf[512];
	int  nf;
	OBJECT	fnobjects;
	register int  i;
	long  m;
	
	if (fname == NULL) {
		infn = "standard input";
		infp = stdin;
	} else {
		infn = fname;
		if ((infp = fopen(fname, "r")) == NULL) {
			sprintf(errmsg, "cannot open octree file \"%s\"",
					fname);
			error(SYSTEM, errmsg);
		}
	}
#ifdef MSDOS
	setmode(fileno(infp), O_BINARY);
#endif
					/* get header */
	if (checkheader(infp, OCTFMT, load&IO_INFO ? stdout : NULL) < 0)
		octerror(USER, "not an octree");
					/* check format */
	if ((objsize = ogetint(2)-OCTMAGIC) <= 0 ||
			objsize > MAXOBJSIZ || objsize > sizeof(long))
		octerror(USER, "incompatible octree format");
					/* get boundaries */
	if (load & IO_BOUNDS) {
		for (i = 0; i < 3; i++)
			scene->cuorg[i] = atof(ogetstr(sbuf));
		scene->cusize = atof(ogetstr(sbuf));
	} else {
		for (i = 0; i < 4; i++)
			ogetstr(sbuf);
	}
	objorig = nobjects;		/* set object offset */
	nf = 0;				/* get object files */
	while (*ogetstr(sbuf)) {
		if (load & IO_SCENE)
			readobj(sbuf);
		if (load & IO_FILES)
			ofn[nf] = savqstr(sbuf);
		nf++;
	}
	if (load & IO_FILES)
		ofn[nf] = NULL;
					/* get number of objects */
	fnobjects = m = ogetint(objsize);
	if (fnobjects != m)
		octerror(USER, "too many objects");

	if (load & IO_TREE)			/* get the octree */
		scene->cutree = gettree();
	else if (load & IO_SCENE && nf == 0)
		skiptree();
		
	if (load & IO_SCENE)		/* get the scene */
	    if (nf == 0) {
		for (i = 0; *ogetstr(sbuf); i++)
			if ((otypmap[i] = otype(sbuf)) < 0) {
				sprintf(errmsg, "unknown type \"%s\"", sbuf);
				octerror(WARNING, errmsg);
			}
		while (getobj() != OVOID)
			;
	    } else {			/* consistency checks */
				/* check object count */
		if (nobjects != objorig+fnobjects)
			octerror(USER, "bad object count; octree stale?");
				/* check for non-surfaces */
		if (nonsurfinset(objorig, fnobjects))
			octerror(USER, "modifier in tree; octree stale?");
	    }
	fclose(infp);
	return(nf);
}


static char *
ogetstr(s)			/* get null-terminated string */
char  *s;
{
	extern char  *getstr();

	if (getstr(s, infp) == NULL)
		octerror(USER, "truncated octree");
	return(s);
}


static OCTREE
getfullnode()			/* get a set, return fullnode */
{
	OBJECT	set[MAXSET+1];
	register int  i;
	register long  m;

	if ((set[0] = ogetint(objsize)) > MAXSET)
		octerror(USER, "bad set in getfullnode");
	for (i = 1; i <= set[0]; i++) {
		m = ogetint(objsize) + objorig;
		if ((set[i] = m) != m)
			octerror(USER, "too many objects");
	}
	return(fullnode(set));
}	


static long
ogetint(siz)			/* get a siz-byte integer */
int  siz;
{
	extern long  getint();
	register long  r;

	r = getint(siz, infp);
	if (feof(infp))
		octerror(USER, "truncated octree");
	return(r);
}


static double
ogetflt()			/* get a floating point number */
{
	extern double  getflt();
	double	r;

	r = getflt(infp);
	if (feof(infp))
		octerror(USER, "truncated octree");
	return(r);
}
	

static OCTREE
gettree()			/* get a pre-ordered octree */
{
	register OCTREE	 ot;
	register int  i;
	
	switch (getc(infp)) {
	case OT_EMPTY:
		return(EMPTY);
	case OT_FULL:
		return(getfullnode());
	case OT_TREE:
		if ((ot = octalloc()) == EMPTY)
			octerror(SYSTEM, "out of tree space in gettree");
		for (i = 0; i < 8; i++)
			octkid(ot, i) = gettree();
		return(ot);
	case EOF:
		octerror(USER, "truncated octree");
	default:
		octerror(USER, "damaged octree");
	}
}


static
skiptree()				/* skip octree on input */
{
	register int  i;
	
	switch (getc(infp)) {
	case OT_EMPTY:
		return;
	case OT_FULL:
		for (i = ogetint(objsize)*objsize; i-- > 0; )
			if (getc(infp) == EOF)
				octerror(USER, "truncated octree");
		return;
	case OT_TREE:
		for (i = 0; i < 8; i++)
			skiptree();
		return;
	case EOF:
		octerror(USER, "truncated octree");
	default:
		octerror(USER, "damaged octree");
	}
}


static
getobj()				/* get next object */
{
	char  sbuf[MAXSTR];
	int  obj;
	register int  i;
	register long  m;
	register OBJREC	 *objp;

	i = ogetint(1);
	if (i == -1)
		return(OVOID);		/* terminator */
	if ((obj = newobject()) == OVOID)
		error(SYSTEM, "out of object space");
	objp = objptr(obj);
	if ((objp->otype = otypmap[i]) < 0)
		octerror(USER, "reference to unknown type");
	if ((m = ogetint(objsize)) != OVOID) {
		m += objorig;
		if ((OBJECT)m != m)
			octerror(USER, "too many objects");
	}
	objp->omod = m;
	objp->oname = savqstr(ogetstr(sbuf));
	if (objp->oargs.nsargs = ogetint(2)) {
		objp->oargs.sarg = (char **)bmalloc
				(objp->oargs.nsargs*sizeof(char *));
		if (objp->oargs.sarg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.nsargs; i++)
			objp->oargs.sarg[i] = savestr(ogetstr(sbuf));
	} else
		objp->oargs.sarg = NULL;
#ifdef	IARGS
	if (objp->oargs.niargs = ogetint(2)) {
		objp->oargs.iarg = (long *)bmalloc
				(objp->oargs.niargs*sizeof(long));
		if (objp->oargs.iarg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.niargs; i++)
			objp->oargs.iarg[i] = ogetint(4);
	} else
		objp->oargs.iarg = NULL;
#endif
	if (objp->oargs.nfargs = ogetint(2)) {
		objp->oargs.farg = (FLOAT *)bmalloc
				(objp->oargs.nfargs*sizeof(FLOAT));
		if (objp->oargs.farg == NULL)
			goto memerr;
		for (i = 0; i < objp->oargs.nfargs; i++)
			objp->oargs.farg[i] = ogetflt();
	} else
		objp->oargs.farg = NULL;
						/* initialize */
	objp->os = NULL;
	objp->lastrno = -1;
						/* insert */
	insertobject(obj);
	return(obj);
memerr:
	error(SYSTEM, "out of memory in getobj");
}


static
octerror(etyp, msg)			/* octree error */
int  etyp;
char  *msg;
{
	char  msgbuf[128];

	sprintf(msgbuf, "(%s): %s", infn, msg);
	error(etyp, msgbuf);
}
