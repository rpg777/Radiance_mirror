/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  readobj.c - routines for reading in object descriptions.
 *
 *     7/28/85
 */

#include  "standard.h"

#include  "object.h"

#include  "otypes.h"

#include  <ctype.h>

OBJREC  *objblock[MAXOBJBLK];		/* our objects */
OBJECT  nobjects = 0;			/* # of objects */


readobj(input)			/* read in an object file or stream */
char  *input;
{
	FILE  *popen();
	char  *fgets(), *fgetline();
	FILE  *infp;
	char  buf[512];
	register int  c;

	if (input == NULL) {
		infp = stdin;
		input = "standard input";
	} else if (input[0] == '!') {
		if ((infp = popen(input+1, "r")) == NULL) {
			sprintf(errmsg, "cannot execute \"%s\"", input);
			error(SYSTEM, errmsg);
		}
	} else if ((infp = fopen(input, "r")) == NULL) {
		sprintf(errmsg, "cannot open scene file \"%s\"", input);
		error(SYSTEM, errmsg);
	}
	while ((c = getc(infp)) != EOF) {
		if (isspace(c))
			continue;
		if (c == '#') {				/* comment */
			fgets(buf, sizeof(buf), infp);
		} else if (c == '!') {			/* command */
			ungetc(c, infp);
			fgetline(buf, sizeof(buf), infp);
			readobj(buf);
		} else {				/* object */
			ungetc(c, infp);
			getobject(input, infp);
		}
	}
	if (input[0] == '!')
		pclose(infp);
	else
		fclose(infp);
}


getobject(name, fp)			/* read the next object */
char  *name;
FILE  *fp;
{
	OBJECT  obj;
	char  sbuf[MAXSTR];
	int  rval;
	register OBJREC  *objp;

	if ((obj = newobject()) == OVOID)
		error(SYSTEM, "out of object space");
	objp = objptr(obj);
					/* get modifier */
	fscanf(fp, "%s", sbuf);
	if (!strcmp(sbuf, VOIDID))
		objp->omod = OVOID;
	else if ((objp->omod = modifier(sbuf)) == OVOID) {
		sprintf(errmsg, "(%s): undefined modifier \"%s\"", name, sbuf);
		error(USER, errmsg);
	}
					/* get type */
	fscanf(fp, "%s", sbuf);
	if (!strcmp(sbuf, ALIASID))
		objp->otype = -1;
	else if ((objp->otype = otype(sbuf)) < 0) {
		sprintf(errmsg, "(%s): unknown type \"%s\"", name, sbuf);
		error(USER, errmsg);
	}
					/* get identifier */
	fscanf(fp, "%s", sbuf);
	objp->oname = savqstr(sbuf);
					/* get arguments */
	if (objp->otype == -1) {
		register OBJECT  alias;
		fscanf(fp, "%s", sbuf);
		if ((alias = modifier(sbuf)) == OVOID) {
			sprintf(errmsg,
			"(%s): bad reference \"%s\" for %s \"%s\"",
					name, sbuf, ALIASID, objp->oname);
			error(USER, errmsg);
		}
		objp->otype = objptr(alias)->otype;
		copystruct(&objp->oargs, &objptr(alias)->oargs);
	} else if ((rval = readfargs(&objp->oargs, fp)) == 0) {
		sprintf(errmsg, "(%s): bad arguments", name);
		objerror(objp, USER, errmsg);
	} else if (rval < 0) {
		sprintf(errmsg, "(%s): error reading scene", name);
		error(SYSTEM, errmsg);
	}
					/* initialize */
	objp->os = NULL;
	objp->lastrno = -1;

	insertobject(obj);		/* add to global structure */
}


int
newobject()				/* get a new object */
{
	register int  i;

	if ((nobjects & 077) == 0) {		/* new block */
		errno = 0;
		i = nobjects >> 6;
		if (i >= MAXOBJBLK)
			return(OVOID);
		objblock[i] = (OBJREC *)bmalloc(0100*sizeof(OBJREC));
		if (objblock[i] == NULL)
			return(OVOID);
	}
	return(nobjects++);
}
