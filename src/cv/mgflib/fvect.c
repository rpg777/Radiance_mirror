/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for 3-d vectors
 */

#include <stdio.h>
#include <math.h>
#include "parser.h"


double
normalize(v)			/* normalize a vector, return old magnitude */
register FVECT  v;
{
	static double  len;
	
	len = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	
	if (len <= 0.0)
		return(0.0);
	
	len = sqrt(len);

	v[0] /= len;
	v[1] /= len;
	v[2] /= len;

	return(len);
}


fcross(vres, v1, v2)		/* vres = v1 X v2 */
register FVECT  vres, v1, v2;
{
	vres[0] = v1[1]*v2[2] - v1[2]*v2[1];
	vres[1] = v1[2]*v2[0] - v1[0]*v2[2];
	vres[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
