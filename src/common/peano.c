/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Generate an N-dimensional Peano space-filling curve
 * on the interval [0,1).
 *
 *	12 Aug 91	Greg Ward
 */

extern double	floor();


static double
peanoC(n, t, e)			/* compute Peano coordinate */
int	n;
double	t, e;
{
	register int	i;
	double	d;

	if (e >= 1./3.)
		return(t);
	d = 0.;
	while (t-d >= 1./3.)
		d += 1./3.;
	i = n;
	while (--i > 0)
		t *= 3.;
	t = 3.*(t - floor(t));
	i = 0;
	while (t >= 1.) {
		t -= 1.;
		i ^= 1;
	}
	t = peanoC(n, t, 3.*e);
	if (i)
		t = 1. - t;
	return(d + t/3.);
}


peano(p, n, t, e)		/* compute Peano point */
register double  p[];
int	n;
double	t, e;
{
	register int	i;
	register int  neg = 0;

	i = n;
	while (i-- > 0) {
		p[i] = peanoC(n, t, e);
		if (neg)
			p[i] = 1. - p[i];
		t *= 3.;
		while (t >= 1.) {
			t -= 1.;
			neg ^= 1;
		}
	}
}
