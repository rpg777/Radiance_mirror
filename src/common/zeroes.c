/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  zeroes.c - compute roots for various equations.
 *
 *     8/19/85
 */


#include  <math.h>

#include  "fvect.h"


int
quadratic(r, a, b, c)		/* find real roots of quadratic equation */
double  *r;			/* roots in ascending order */
double  a, b, c; 
{
	double  disc;
	int  first;

	if (a < -FTINY)
		first = 1;
	else if (a > FTINY)
		first = 0;
	else if (fabs(b) > FTINY) {	/* solve linearly */
		r[0] = -c/b;
		return(1);
	} else
		return(0);		/* equation is c == 0 ! */
		
	b *= 0.5;			/* simplifies formula */
	
	disc = b*b - a*c;		/* discriminant */

	if (disc < -FTINY*FTINY)	/* no real roots */
		return(0);

	if (disc <= FTINY*FTINY) {	/* double root */
		r[0] = -b/a;
		return(1);
	}
	
	disc = sqrt(disc);

	r[first] = (-b - disc)/a;
	r[1-first] = (-b + disc)/a;

	return(2);
}
