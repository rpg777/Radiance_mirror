/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Basic linear regression calculation.
 */

#include <math.h>

#include "linregr.h"


lrclear(l)			/* initialize sum */
register LRSUM	*l;
{
	l->xs = l->ys = l->xxs = l->yys = l->xys = 0.0;
	l->n = 0;
}


flrpoint(x, y, l)		/* add point (x,y) to sum */
double	x, y;
register LRSUM	*l;
{
	l->xs += x;
	l->ys += y;
	l->xxs += x*x;
	l->yys += y*y;
	l->xys += x*y;
	return(++l->n);
}


lrfit(r, l)			/* compute linear regression */
register LRLIN	*r;
register LRSUM	*l;
{
	double	nxvar, nyvar;

	if (l->n < 2)
		return(-1);
	nxvar = l->xxs - l->xs*l->xs/l->n;
	nyvar = l->yys - l->ys*l->ys/l->n;
	if (nxvar == 0.0 || nyvar == 0.0)
		return(-1);
	r->slope = (l->xys - l->xs*l->ys/l->n) / nxvar;
	r->intercept = (l->ys - r->slope*l->xs) / l->n;
	r->correlation = r->slope*sqrt(nxvar/nyvar);
	return(0);
}
