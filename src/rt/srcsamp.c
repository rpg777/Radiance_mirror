/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Source sampling routines
 */

#include  "standard.h"

#include  "object.h"

#include  "source.h"

#include  "random.h"


extern int  dimlist[];		/* dimension list for distribution */
extern int  ndims;		/* number of dimensions so far */
extern int  samplendx;		/* index for this sample */


double
nextssamp(org, dir, si)		/* compute sample for source, rtn. distance */
FVECT  org, dir;		/* origin is read only, direction is set */
register SRCINDEX  *si;		/* source index (modified to current) */
{
	int  cent[3], size[3], parr[2];
	FVECT  vpos;
	double  d;
	register int  i;
tryagain:
	while (++si->sp >= si->np) {	/* get next sample */
		if (++si->sn >= nsources)
			return(0.0);	/* no more */
		if (srcsizerat <= FTINY)
			nopart(si, org);
		else {
			for (i = si->sn; source[i].sflags & SVIRTUAL;
					i = source[i].sa.sv.sn)
				;		/* partition source */
			(*sfun[source[i].so->otype].of->partit)(si, org);
		}
		si->sp = -1;
	}
					/* get partition */
	cent[0] = cent[1] = cent[2] = 0;
	size[0] = size[1] = size[2] = MAXSPART;
	parr[0] = 0; parr[1] = si->sp;
	if (!skipparts(cent, size, parr, si->spt))
		error(CONSISTENCY, "bad source partition in nextssamp");
					/* compute sample */
	if (dstrsrc > FTINY) {			/* jitter sample */
		dimlist[ndims] = si->sn + 8831;
		dimlist[ndims+1] = si->sp + 3109;
		d = urand(ilhash(dimlist,ndims+2)+samplendx);
		if (source[si->sn].sflags & SFLAT) {
			multisamp(vpos, 2, d);
			vpos[2] = 0.5;
		} else
			multisamp(vpos, 3, d);
		for (i = 0; i < 3; i++)
			vpos[i] = dstrsrc * (1. - 2.*vpos[i]) *
					(double)size[i]/MAXSPART;
	} else
		vpos[0] = vpos[1] = vpos[2] = 0.0;

	for (i = 0; i < 3; i++)
		vpos[i] += (double)cent[i]/MAXSPART;
					/* compute direction */
	for (i = 0; i < 3; i++)
		dir[i] = source[si->sn].sloc[i] +
				vpos[SU]*source[si->sn].ss[SU][i] +
				vpos[SV]*source[si->sn].ss[SV][i] +
				vpos[SW]*source[si->sn].ss[SW][i];

	if (!(source[si->sn].sflags & SDISTANT))
		for (i = 0; i < 3; i++)
			dir[i] -= org[i];
					/* compute distance */
	if ((d = normalize(dir)) == 0.0)
		goto tryagain;	/* at source! */

					/* compute sample size */
	si->dom  = source[si->sn].ss2;
	if (source[si->sn].sflags & SFLAT) {
		si->dom *= sflatform(si->sn, dir);
		if (si->dom <= FTINY) {		/* behind source */
			si->np = 0;
			goto tryagain;
		}
		si->dom *= (double)(size[SU]*size[SV])/(MAXSPART*MAXSPART);
	} else if (source[si->sn].sflags & SCYL) {
		si->dom *= scylform(si->sn, dir);
		si->dom *= (double)size[SU]/MAXSPART;
	} else {
		si->dom *= (double)(size[SU]*size[SV]*size[SW]) /
				(MAXSPART*MAXSPART*MAXSPART) ;
	}
	if (source[si->sn].sflags & SDISTANT)
		return(FHUGE);
	si->dom /= d*d;
	return(d);		/* sample OK, return distance */
}


skipparts(ct, sz, pp, pt)		/* skip to requested partition */
int  ct[3], sz[3];		/* center and size of partition (returned) */
register int  pp[2];		/* current index, number to skip (modified) */
unsigned char  *pt;		/* partition array */
{
	register int  p;
					/* check this partition */
	p = spart(pt, pp[0]);
	pp[0]++;
	if (p == S0)			/* leaf partition */
		if (pp[1]) {
			pp[1]--;
			return(0);	/* not there yet */
		} else
			return(1);	/* we've arrived */
				/* else check lower */
	sz[p] >>= 1;
	ct[p] -= sz[p];
	if (skipparts(ct, sz, pp, pt))
		return(1);	/* return hit */
				/* else check upper */
	ct[p] += sz[p] << 1;
	if (skipparts(ct, sz, pp, pt))
		return(1);	/* return hit */
				/* else return to starting position */
	ct[p] -= sz[p];
	sz[p] <<= 1;
	return(0);		/* return miss */
}


nopart(si, ro)			/* single source partition */
register SRCINDEX  *si;
FVECT  ro;
{
	clrpart(si->spt);
	setpart(si->spt, 0, S0);
	si->np = 1;
}


cylpart(si, ro)			/* partition a cylinder */
SRCINDEX  *si;
FVECT  ro;
{
	double  dist2, safedist2, dist2cent, rad2;
	FVECT  v;
	register SRCREC  *sp;
	int  pi;
					/* first check point location */
	clrpart(si->spt);
	sp = &source[si->sn];
	rad2 = 1.365 * DOT(sp->ss[SV],sp->ss[SV]);
	v[0] = ro[0] - sp->sloc[0];
	v[1] = ro[1] - sp->sloc[1];
	v[2] = ro[2] - sp->sloc[2];
	dist2 = DOT(v,sp->ss[SU]);
	safedist2 = DOT(sp->ss[SU],sp->ss[SU]);
	dist2 *= dist2 / safedist2;
	dist2cent = DOT(v,v);
	dist2 = dist2cent - dist2;
	if (dist2 <= rad2) {		/* point inside extended cylinder */
		si->np = 0;
		return;
	}
	safedist2 *= 4./(srcsizerat*srcsizerat);
	if (dist2 <= 4.*rad2 ||		/* point too close to subdivide? */
			dist2cent >= safedist2) {
		setpart(si->spt, 0, S0);
		si->np = 1;
		return;
	}
	pi = 0;
	si->np = cyl_partit(ro, si->spt, &pi, MAXSPART,
			sp->sloc, sp->ss[SU], safedist2);
}


static int
cyl_partit(ro, pt, pi, mp, cent, axis, d2)	/* slice a cylinder */
FVECT  ro;
unsigned char  *pt;
register int  *pi;
int  mp;
FVECT  cent, axis;
double  d2;
{
	FVECT  newct, newax;
	int  npl, npu;

	if (mp < 2 || dist2(ro, cent) >= d2) {	/* hit limit? */
		setpart(pt, *pi, S0);
		(*pi)++;
		return(1);
	}
					/* subdivide */
	setpart(pt, *pi, SU);
	(*pi)++;
	newax[0] = .5*axis[0];
	newax[1] = .5*axis[1];
	newax[2] = .5*axis[2];
	d2 *= 0.25;
					/* lower half */
	newct[0] = cent[0] - newax[0];
	newct[1] = cent[1] - newax[1];
	newct[2] = cent[2] - newax[2];
	npl = cyl_partit(ro, pt, pi, mp/2, newct, newax, d2);
					/* upper half */
	newct[0] = cent[0] + newax[0];
	newct[1] = cent[1] + newax[1];
	newct[2] = cent[2] + newax[2];
	npu = cyl_partit(ro, pt, pi, mp/2, newct, newax, d2);
					/* return total */
	return(npl + npu);
}


flatpart(si, ro)			/* partition a flat source */
register SRCINDEX  *si;
FVECT  ro;
{
	register double  *vp;
	double  du2, dv2;
	int  pi;

	vp = source[si->sn].ss[SU];
	du2 = 4./(srcsizerat*srcsizerat) * DOT(vp,vp);
	vp = source[si->sn].ss[SV];
	dv2 = 4./(srcsizerat*srcsizerat) * DOT(vp,vp);
	clrpart(si->spt);
	pi = 0;
	si->np = flt_partit(ro, si->spt, &pi, MAXSPART, source[si->sn].sloc,
		source[si->sn].ss[SU], source[si->sn].ss[SV], du2, dv2);
}


static int
flt_partit(ro, pt, pi, mp, cent, u, v, du2, dv2)	/* partition flatty */
FVECT  ro;
unsigned char  *pt;
register int  *pi;
int  mp;
FVECT  cent, u, v;
double  du2, dv2;
{
	double  d2;
	FVECT  newct, newax;
	int  npl, npu;

	if (mp < 2 || ((d2 = dist2(ro, cent)) >= du2
			&& d2 >= dv2)) {	/* hit limit? */
		setpart(pt, *pi, S0);
		(*pi)++;
		return(1);
	}
	if (du2 > dv2) {			/* subdivide in U */
		setpart(pt, *pi, SU);
		(*pi)++;
		newax[0] = .5*u[0];
		newax[1] = .5*u[1];
		newax[2] = .5*u[2];
		u = newax;
		du2 *= 0.25;
	} else {				/* subdivide in V */
		setpart(pt, *pi, SV);
		(*pi)++;
		newax[0] = .5*v[0];
		newax[1] = .5*v[1];
		newax[2] = .5*v[2];
		v = newax;
		dv2 *= 0.25;
	}
					/* lower half */
	newct[0] = cent[0] - newax[0];
	newct[1] = cent[1] - newax[1];
	newct[2] = cent[2] - newax[2];
	npl = flt_partit(ro, pt, pi, mp/2, newct, u, v, du2, dv2);
					/* upper half */
	newct[0] = cent[0] + newax[0];
	newct[1] = cent[1] + newax[1];
	newct[2] = cent[2] + newax[2];
	npu = flt_partit(ro, pt, pi, mp/2, newct, u, v, du2, dv2);
				/* return total */
	return(npl + npu);
}


double
scylform(sn, dir)		/* compute cosine for cylinder's projection */
int  sn;
register FVECT  dir;		/* assume normalized */
{
	register double  *dv;
	double  d;

	dv = source[sn].ss[SU];
	d = DOT(dir, dv);
	d *= d / DOT(dv,dv);
	return(sqrt(1. - d));
}
