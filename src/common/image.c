/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  image.c - routines for image generation.
 *
 *     10/17/85
 */

#include  "standard.h"

#include  "view.h"

VIEW  stdview = STDVIEW;		/* default view parameters */


char *
setview(v)		/* set hvec and vvec, return message on error */
register VIEW  *v;
{
	static char  ill_horiz[] = "illegal horizontal view size";
	static char  ill_vert[] = "illegal vertical view size";
	
	if (normalize(v->vdir) == 0.0)		/* normalize direction */
		return("zero view direction");

	if (normalize(v->vup) == 0.0)		/* normalize view up */
		return("zero view up vector");

	fcross(v->hvec, v->vdir, v->vup);	/* compute horiz dir */

	if (normalize(v->hvec) == 0.0)
		return("view up parallel to view direction");

	fcross(v->vvec, v->hvec, v->vdir);	/* compute vert dir */

	if (v->horiz <= FTINY)
		return(ill_horiz);
	if (v->vert <= FTINY)
		return(ill_vert);

	switch (v->type) {
	case VT_PAR:				/* parallel view */
		v->hn2 = v->horiz;
		v->vn2 = v->vert;
		break;
	case VT_PER:				/* perspective view */
		if (v->horiz >= 180.0-FTINY)
			return(ill_horiz);
		if (v->vert >= 180.0-FTINY)
			return(ill_vert);
		v->hn2 = 2.0 * tan(v->horiz*(PI/180.0/2.0));
		v->vn2 = 2.0 * tan(v->vert*(PI/180.0/2.0));
		break;
	case VT_ANG:				/* angular fisheye */
		if (v->horiz > 360.0+FTINY)
			return(ill_horiz);
		if (v->vert > 360.0+FTINY)
			return(ill_vert);
		v->hn2 = v->horiz / 90.0;
		v->vn2 = v->vert / 90.0;
		break;
	case VT_HEM:				/* hemispherical fisheye */
		if (v->horiz > 180.0+FTINY)
			return(ill_horiz);
		if (v->vert > 180.0+FTINY)
			return(ill_vert);
		v->hn2 = 2.0 * sin(v->horiz*(PI/180.0/2.0));
		v->vn2 = 2.0 * sin(v->vert*(PI/180.0/2.0));
		break;
	default:
		return("unknown view type");
	}
	if (v->type != VT_ANG) {
		v->hvec[0] *= v->hn2;
		v->hvec[1] *= v->hn2;
		v->hvec[2] *= v->hn2;
		v->vvec[0] *= v->vn2;
		v->vvec[1] *= v->vn2;
		v->vvec[2] *= v->vn2;
	}
	v->hn2 *= v->hn2;
	v->vn2 *= v->vn2;

	return(NULL);
}


normaspect(va, ap, xp, yp)		/* fix pixel aspect or resolution */
double  va;			/* view aspect ratio */
double  *ap;			/* pixel aspect in (or out if 0) */
int  *xp, *yp;			/* x and y resolution in (or out if *ap!=0) */
{
	if (*ap <= FTINY)
		*ap = va * *xp / *yp;		/* compute pixel aspect */
	else if (va * *xp > *ap * *yp)
		*xp = *yp / va * *ap + .5;	/* reduce x resolution */
	else
		*yp = *xp * va / *ap + .5;	/* reduce y resolution */
}


viewray(orig, direc, v, x, y)		/* compute ray origin and direction */
FVECT  orig, direc;
register VIEW  *v;
double  x, y;
{
	double	d, z;
	
	x += v->hoff - 0.5;
	y += v->voff - 0.5;

	switch(v->type) {
	case VT_PAR:			/* parallel view */
		orig[0] = v->vp[0] + x*v->hvec[0] + y*v->vvec[0];
		orig[1] = v->vp[1] + x*v->hvec[1] + y*v->vvec[1];
		orig[2] = v->vp[2] + x*v->hvec[2] + y*v->vvec[2];
		VCOPY(direc, v->vdir);
		return(0);
	case VT_PER:			/* perspective view */
		VCOPY(orig, v->vp);
		direc[0] = v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		normalize(direc);
		return(0);
	case VT_HEM:			/* hemispherical fisheye */
		z = 1.0 - x*x*v->hn2 - y*y*v->vn2;
		if (z < 0.0)
			return(-1);
		z = sqrt(z);
		VCOPY(orig, v->vp);
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		return(0);
	case VT_ANG:			/* angular fisheye */
		x *= v->horiz/180.0;
		y *= v->vert/180.0;
		d = x*x + y*y;
		if (d > 1.0)
			return(-1);
		VCOPY(orig, v->vp);
		if (d <= FTINY) {
			VCOPY(direc, v->vdir);
			return(0);
		}
		d = sqrt(d);
		z = cos(PI*d);
		d = sqrt(1 - z*z)/d;
		x *= d;
		y *= d;
		direc[0] = z*v->vdir[0] + x*v->hvec[0] + y*v->vvec[0];
		direc[1] = z*v->vdir[1] + x*v->hvec[1] + y*v->vvec[1];
		direc[2] = z*v->vdir[2] + x*v->hvec[2] + y*v->vvec[2];
		return(0);
	}
	return(-1);
}


viewpixel(xp, yp, zp, v, p)		/* find image location for point */
double  *xp, *yp, *zp;
register VIEW  *v;
FVECT  p;
{
	double  d;
	FVECT  disp;

	disp[0] = p[0] - v->vp[0];
	disp[1] = p[1] - v->vp[1];
	disp[2] = p[2] - v->vp[2];

	switch (v->type) {
	case VT_PAR:			/* parallel view */
		if (zp != NULL)
			*zp = DOT(disp,v->vdir);
		break;
	case VT_PER:			/* perspective view */
		d = DOT(disp,v->vdir);
		if (zp != NULL) {
			*zp = sqrt(DOT(disp,disp));
			if (d < 0.0)
				*zp = -*zp;
		}
		if (d < 0.0)		/* fold pyramid */
			d = -d;
		if (d > FTINY) {
			d = 1.0/d;
			disp[0] *= d;
			disp[1] *= d;
			disp[2] *= d;
		}
		break;
	case VT_HEM:			/* hemispherical fisheye */
		d = normalize(disp);
		if (zp != NULL) {
			if (DOT(disp,v->vdir) < 0.0)
				*zp = -d;
			else
				*zp = d;
		}
		break;
	case VT_ANG:			/* angular fisheye */
		d = normalize(disp);
		if (zp != NULL)
			*zp = d;
		*xp = 0.5 - v->hoff;
		*yp = 0.5 - v->voff;
		d = DOT(disp,v->vdir);
		if (d >= 1.0-FTINY)
			return;
		if (d <= -(1.0-FTINY)) {
			*xp += 180.0/v->horiz;
			*yp += 180.0/v->vert;
			return;
		}
		d = acos(d)/PI / sqrt(1.0 - d*d);
		*xp += DOT(disp,v->hvec)*d*180.0/v->horiz;
		*yp += DOT(disp,v->vvec)*d*180.0/v->vert;
		return;
	}
	*xp = DOT(disp,v->hvec)/v->hn2 + 0.5 - v->hoff;
	*yp = DOT(disp,v->vvec)/v->vn2 + 0.5 - v->voff;
}


int
getviewopt(v, ac, av)			/* process view argument */
register VIEW  *v;
int  ac;
register char  *av[];
{
#define check(c,n)	if ((av[0][c]&&av[0][c]!=' ') || n>=ac) return(-1)
	extern double  atof();

	if (ac <= 0 || av[0][0] != '-' || av[0][1] != 'v')
		return(-1);
	switch (av[0][2]) {
	case 't':			/* type */
		if (!av[0][3] || av[0][3]==' ')
			return(-1);
		check(4,0);
		v->type = av[0][3];
		return(0);
	case 'p':			/* point */
		check(3,3);
		v->vp[0] = atof(av[1]);
		v->vp[1] = atof(av[2]);
		v->vp[2] = atof(av[3]);
		return(3);
	case 'd':			/* direction */
		check(3,3);
		v->vdir[0] = atof(av[1]);
		v->vdir[1] = atof(av[2]);
		v->vdir[2] = atof(av[3]);
		return(3);
	case 'u':			/* up */
		check(3,3);
		v->vup[0] = atof(av[1]);
		v->vup[1] = atof(av[2]);
		v->vup[2] = atof(av[3]);
		return(3);
	case 'h':			/* horizontal size */
		check(3,1);
		v->horiz = atof(av[1]);
		return(1);
	case 'v':			/* vertical size */
		check(3,1);
		v->vert = atof(av[1]);
		return(1);
	case 's':			/* shift */
		check(3,1);
		v->hoff = atof(av[1]);
		return(1);
	case 'l':			/* lift */
		check(3,1);
		v->voff = atof(av[1]);
		return(1);
	default:
		return(-1);
	}
#undef check
}


int
sscanview(vp, s)			/* get view parameters from string */
VIEW  *vp;
register char  *s;
{
	int  ac;
	char  *av[4];
	int  na;
	int  nvopts = 0;

	while (*s == ' ')
		s++;
	while (*s) {
		ac = 0;
		do {
			av[ac++] = s;
			while (*s && *s != ' ')
				s++;
			while (*s == ' ')
				s++;
		} while (*s && ac < 4);
		if ((na = getviewopt(vp, ac, av)) >= 0) {
			if (na+1 < ac)
				s = av[na+1];
			nvopts++;
		} else if (ac > 1)
			s = av[1];
	}
	return(nvopts);
}


fprintview(vp, fp)			/* write out view parameters */
register VIEW  *vp;
FILE  *fp;
{
	fprintf(fp, " -vt%c", vp->type);
	fprintf(fp, " -vp %.6g %.6g %.6g", vp->vp[0], vp->vp[1], vp->vp[2]);
	fprintf(fp, " -vd %.6g %.6g %.6g", vp->vdir[0], vp->vdir[1], vp->vdir[2]);
	fprintf(fp, " -vu %.6g %.6g %.6g", vp->vup[0], vp->vup[1], vp->vup[2]);
	fprintf(fp, " -vh %.6g -vv %.6g", vp->horiz, vp->vert);
	fprintf(fp, " -vs %.6g -vl %.6g", vp->hoff, vp->voff);
}


static VIEW  *hview;			/* view from header */
static int  gothview;			/* success indicator */
static char  *altname[] = {NULL,"rpict","rview","pinterp",VIEWSTR,NULL};


static
gethview(s)				/* get view from header */
char  *s;
{
	register char  **an;

	for (an = altname; *an != NULL; an++)
		if (!strncmp(*an, s, strlen(*an))) {
			if (sscanview(hview, s+strlen(*an)) > 0)
				gothview++;
			return;
		}
}


int
viewfile(fname, vp, xp, yp)		/* get view from file */
char  *fname;
VIEW  *vp;
int  *xp, *yp;
{
	extern char  *progname;
	FILE  *fp;

	if ((fp = fopen(fname, "r")) == NULL)
		return(-1);

	altname[0] = progname;
	hview = vp;
	gothview = 0;

	getheader(fp, gethview);

	if (xp != NULL && yp != NULL
			&& fgetresolu(xp, yp, fp) == -1)
		gothview = 0;

	fclose(fp);

	return(gothview);
}
