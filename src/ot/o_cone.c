#ifndef lint
static const char	RCSid[] = "$Id: o_cone.c,v 2.2 2003/02/22 02:07:26 greg Exp $";
#endif
/*
 *  o_cone.c - routines for intersecting cubes with cones.
 *
 *     2/3/86
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "cone.h"

#define  ROOT3		1.732050808

/*
 *     The algorithm used to detect cube intersection with cones is
 *  recursive.  First, we approximate the cube to be a sphere.  Then
 *  we test for cone intersection with the sphere by testing the
 *  segment of the cone which is nearest the sphere's center.
 *     If the cone has points within the cube's bounding sphere,
 *  we must check for intersection with the cube.  This is done with
 *  the 3D line clipper.  The same cone segment is used in this test.
 *  If the clip fails, we still cannot be sure there is no intersection,
 *  so we subdivide the cube and recurse.
 *     If none of the sub-cubes intersect, then our cube does not intersect.
 */

extern double  mincusize;		/* minimum cube size */


o_cone(o, cu)			/* determine if cone intersects cube */
OBJREC  *o;
register CUBE  *cu;
{
	double  dist2lseg(), findcseg();
	CONE  *co;
	FVECT  ep0, ep1;
	FVECT  cumin, cumax;
	CUBE  cukid;
	double  r;
	FVECT  p;
	register int  i, j;
					/* get cone arguments */
	co = getcone(o, 0);
					/* get cube center */
	r = cu->cusize * 0.5;
	for (i = 0; i < 3; i++)
		p[i] = cu->cuorg[i] + r;
	r *= ROOT3;			/* bounding radius for cube */

	if (findcseg(ep0, ep1, co, p) > 0.0) {
					/* check min. distance to cone */
		if (dist2lseg(p, ep0, ep1) > (r+FTINY)*(r+FTINY))
			return(O_MISS);
#ifdef  STRICT
					/* get cube boundaries */
		for (i = 0; i < 3; i++)
			cumax[i] = (cumin[i] = cu->cuorg[i]) + cu->cusize;
					/* closest segment intersects? */
		if (clip(ep0, ep1, cumin, cumax))
			return(O_HIT);
	}
					/* check sub-cubes */
	cukid.cusize = cu->cusize * 0.5;
	if (cukid.cusize < mincusize)
		return(O_HIT);		/* cube too small */
	cukid.cutree = EMPTY;

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 3; i++) {
			cukid.cuorg[i] = cu->cuorg[i];
			if (1<<i & j)
				cukid.cuorg[i] += cukid.cusize;
		}
		if (o_cone(o, &cukid))
			return(O_HIT);	/* sub-cube intersects */
	}
	return(O_MISS);			/* no intersection */
#else
	}
	return(O_HIT);			/* assume intersection */
#endif
}


double
findcseg(ep0, ep1, co, p)	/* find line segment from cone closest to p */
FVECT  ep0, ep1;
register CONE  *co;
FVECT  p;
{
	double  d;
	FVECT  v;
	register int  i;
					/* find direction from axis to point */
	for (i = 0; i < 3; i++)
		v[i] = p[i] - CO_P0(co)[i];
	d = DOT(v, co->ad);
	for (i = 0; i < 3; i++)
		v[i] = v[i] - d*co->ad[i];
	d = normalize(v);
	if (d > 0.0)			/* find endpoints of segment */
		for (i = 0; i < 3; i++) {
			ep0[i] = CO_R0(co)*v[i] + CO_P0(co)[i];
			ep1[i] = CO_R1(co)*v[i] + CO_P1(co)[i];
		}
	return(d);			/* return distance from axis */
}
