/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  p_func.c - routine for procedural patterns.
 *
 *     4/8/86
 */

#include  "ray.h"

#include  "func.h"

/*
 *	A procedural pattern can either be a brightness or a
 *  color function.  A brightness function is given as:
 *
 *	modifier brightfunc name
 *	2+ bvarname filename xf
 *	0
 *	n A1 A2 ..
 *
 *  A color function is given as:
 *
 *	modifier colorfunc name
 *	4+ rvarname gvarname bvarname filename xf
 *	0
 *	n A1 A2 ..
 *
 *  Filename is the name of the file where the variable definitions
 *  can be found.  The list of real arguments can be accessed by
 *  definitions in the file.  The xf is a transformation
 *  to get from the original coordinates to the current coordinates.
 */


p_bfunc(m, r)			/* compute brightness pattern */
OBJREC  *m;
RAY  *r;
{
	double  bval;
	register MFUNC  *mf;

	if (m->oargs.nsargs < 2)
		objerror(m, USER, "bad # arguments");
	mf = getfunc(m, 1, 0x1, 0);
	setfunc(m, r);
	errno = 0;
	bval = evalue(mf->ep[0]);
	if (errno) {
		objerror(m, WARNING, "compute error");
		return(0);
	}
	scalecolor(r->pcol, bval);
	return(0);
}


p_cfunc(m, r)			/* compute color pattern */
OBJREC  *m;
RAY  *r;
{
	COLOR  cval;
	register MFUNC  *mf;

	if (m->oargs.nsargs < 4)
		objerror(m, USER, "bad # arguments");
	mf = getfunc(m, 3, 0x7, 0);
	setfunc(m, r);
	errno = 0;
	setcolor(cval, evalue(mf->ep[0]),
			evalue(mf->ep[1]),
			evalue(mf->ep[2]));
	if (errno) {
		objerror(m, WARNING, "compute error");
		return(0);
	}
	multcolor(r->pcol, cval);
	return(0);
}
