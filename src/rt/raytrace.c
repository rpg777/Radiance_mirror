/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  raytrace.c - routines for tracing and shading rays.
 *
 *     8/7/85
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "otspecial.h"

#define  MAXCSET	((MAXSET+1)*2-1)	/* maximum check set size */

extern CUBE  thescene;			/* our scene */
extern int  maxdepth;			/* maximum recursion depth */
extern double  minweight;		/* minimum ray weight */
extern int  do_irrad;			/* compute irradiance? */

long  raynum = 0L;			/* next unique ray number */
long  nrays = 0L;			/* number of calls to localhit */

static FLOAT  Lambfa[5] = {PI, PI, PI, 0.0, 0.0};
OBJREC  Lamb = {
	OVOID, MAT_PLASTIC, "Lambertian",
	{0, 5, NULL, Lambfa}, NULL,
};					/* a Lambertian surface */

#define  MAXLOOP	128		/* modifier loop detection */

#define  RAYHIT		(-1)		/* return value for intercepted ray */


rayorigin(r, ro, rt, rw)		/* start new ray from old one */
register RAY  *r, *ro;
int  rt;
double  rw;
{
	if ((r->parent = ro) == NULL) {		/* primary ray */
		r->rlvl = 0;
		r->rweight = rw;
		r->crtype = r->rtype = rt;
		r->rsrc = -1;
		r->clipset = NULL;
		r->revf = raytrace;
	} else {				/* spawned ray */
		r->rlvl = ro->rlvl;
		if (rt & RAYREFL) {
			r->rlvl++;
			r->rsrc = -1;
			r->clipset = ro->clipset;
		} else {
			r->rsrc = ro->rsrc;
			r->clipset = ro->newcset;
		}
		r->revf = ro->revf;
		r->rweight = ro->rweight * rw;
		r->crtype = ro->crtype | (r->rtype = rt);
		VCOPY(r->rorg, ro->rop);
	}
	rayclear(r);
	return(r->rlvl <= maxdepth && r->rweight >= minweight ? 0 : -1);
}


rayclear(r)			/* clear a ray for (re)evaluation */
register RAY  *r;
{
	r->rno = raynum++;
	r->newcset = r->clipset;
	r->ro = NULL;
	r->rot = FHUGE;
	r->pert[0] = r->pert[1] = r->pert[2] = 0.0;
	setcolor(r->pcol, 1.0, 1.0, 1.0);
	setcolor(r->rcol, 0.0, 0.0, 0.0);
	r->rt = 0.0;
}


raytrace(r)			/* trace a ray and compute its value */
RAY  *r;
{
	extern int  (*trace)();

	if (localhit(r, &thescene))
		raycont(r);
	else if (sourcehit(r))
		rayshade(r, r->ro->omod);

	if (trace != NULL)
		(*trace)(r);		/* trace execution */
}


raycont(r)			/* check for clipped object and continue */
register RAY  *r;
{
	if (r->clipset != NULL && inset(r->clipset, r->ro->omod))
		raytrans(r);
	else
		rayshade(r, r->ro->omod);
}


raytrans(r)			/* transmit ray as is */
register RAY  *r;
{
	RAY  tr;

	if (rayorigin(&tr, r, TRANS, 1.0) == 0) {
		VCOPY(tr.rdir, r->rdir);
		rayvalue(&tr);
		copycolor(r->rcol, tr.rcol);
		r->rt = r->rot + tr.rt;
	}
}


rayshade(r, mod)		/* shade ray r with material mod */
register RAY  *r;
int  mod;
{
	static int  depth = 0;
	register OBJREC  *m;
					/* check for infinite loop */
	if (depth++ >= MAXLOOP)
		objerror(r->ro, USER, "possible modifier loop");
	r->rt = r->rot;			/* set effective ray length */
	for ( ; mod != OVOID; mod = m->omod) {
		m = objptr(mod);
		/****** unnecessary test since modifier() is always called
		if (!ismodifier(m->otype)) {
			sprintf(errmsg, "illegal modifier \"%s\"", m->oname);
			error(USER, errmsg);
		}
		******/
					/* hack for irradiance calculation */
		if (do_irrad && !(r->crtype & ~(PRIMARY|TRANS))) {
			if (irr_ignore(m->otype)) {
				depth--;
				raytrans(r);
				return;
			}
			if (!islight(m->otype))
				m = &Lamb;
		}
		(*ofun[m->otype].funp)(m, r);	/* execute function */
		if (ismaterial(m->otype)) {	/* materials call raytexture */
			depth--;
			return;		/* we're done */
		}
	}
	objerror(r->ro, USER, "material not found");
}


raytexture(r, mod)			/* get material modifiers */
RAY  *r;
int  mod;
{
	static int  depth = 0;
	register OBJREC  *m;
					/* check for infinite loop */
	if (depth++ >= MAXLOOP)
		objerror(r->ro, USER, "modifier loop");
					/* execute textures and patterns */
	for ( ; mod != OVOID; mod = m->omod) {
		m = objptr(mod);
		if (!istexture(m->otype)) {
			sprintf(errmsg, "illegal modifier \"%s\"", m->oname);
			error(USER, errmsg);
		}
		(*ofun[m->otype].funp)(m, r);
	}
	depth--;			/* end here */
}


raymixture(r, fore, back, coef)		/* mix modifiers */
register RAY  *r;
OBJECT  fore, back;
double  coef;
{
	FVECT  curpert, forepert, backpert;
	COLOR  curpcol, forepcol, backpcol;
	register int  i;
					/* clip coefficient */
	if (coef > 1.0)
		coef = 1.0;
	else if (coef < 0.0)
		coef = 0.0;
					/* save current mods */
	VCOPY(curpert, r->pert);
	copycolor(curpcol, r->pcol);
					/* compute new mods */
						/* foreground */
	r->pert[0] = r->pert[1] = r->pert[2] = 0.0;
	setcolor(r->pcol, 1.0, 1.0, 1.0);
	if (fore != OVOID && coef > FTINY)
		raytexture(r, fore);
	VCOPY(forepert, r->pert);
	copycolor(forepcol, r->pcol);
						/* background */
	r->pert[0] = r->pert[1] = r->pert[2] = 0.0;
	setcolor(r->pcol, 1.0, 1.0, 1.0);
	if (back != OVOID && coef < 1.0-FTINY)
		raytexture(r, back);
	VCOPY(backpert, r->pert);
	copycolor(backpcol, r->pcol);
					/* sum perturbations */
	for (i = 0; i < 3; i++)
		r->pert[i] = curpert[i] + coef*forepert[i] +
				(1.0-coef)*backpert[i];
					/* multiply colors */
	setcolor(r->pcol, coef*colval(forepcol,RED) +
				(1.0-coef)*colval(backpcol,RED),
			coef*colval(forepcol,GRN) +
				(1.0-coef)*colval(backpcol,GRN),
			coef*colval(forepcol,BLU) +
				(1.0-coef)*colval(backpcol,BLU));
	multcolor(r->pcol, curpcol);
}


double
raynormal(norm, r)		/* compute perturbed normal for ray */
FVECT  norm;
register RAY  *r;
{
	double  newdot;
	register int  i;

	/*	The perturbation is added to the surface normal to obtain
	 *  the new normal.  If the new normal would affect the surface
	 *  orientation wrt. the ray, a correction is made.  The method is
	 *  still fraught with problems since reflected rays and similar
	 *  directions calculated from the surface normal may spawn rays behind
	 *  the surface.  The only solution is to curb textures at high
	 *  incidence (namely, keep DOT(rdir,pert) < Rdot).
	 */

	for (i = 0; i < 3; i++)
		norm[i] = r->ron[i] + r->pert[i];

	if (normalize(norm) == 0.0) {
		objerror(r->ro, WARNING, "illegal normal perturbation");
		VCOPY(norm, r->ron);
		return(r->rod);
	}
	newdot = -DOT(norm, r->rdir);
	if ((newdot > 0.0) != (r->rod > 0.0)) {		/* fix orientation */
		for (i = 0; i < 3; i++)
			norm[i] += 2.0*newdot*r->rdir[i];
		newdot = -newdot;
	}
	return(newdot);
}


newrayxf(r)			/* get new tranformation matrix for ray */
RAY  *r;
{
	static struct xfn {
		struct xfn  *next;
		FULLXF  xf;
	}  xfseed = { &xfseed }, *xflast = &xfseed;
	register struct xfn  *xp;
	register RAY  *rp;

	/*
	 * Search for transform in circular list that
	 * has no associated ray in the tree.
	 */
	xp = xflast;
	for (rp = r->parent; rp != NULL; rp = rp->parent)
		if (rp->rox == &xp->xf) {		/* xp in use */
			xp = xp->next;			/* move to next */
			if (xp == xflast) {		/* need new one */
				xp = (struct xfn *)bmalloc(sizeof(struct xfn));
				if (xp == NULL)
					error(SYSTEM,
						"out of memory in newrayxf");
							/* insert in list */
				xp->next = xflast->next;
				xflast->next = xp;
				break;			/* we're done */
			}
			rp = r;			/* start check over */
		}
					/* got it */
	r->rox = &xp->xf;
	xflast = xp;
}


flipsurface(r)			/* reverse surface orientation */
register RAY  *r;
{
	r->rod = -r->rod;
	r->ron[0] = -r->ron[0];
	r->ron[1] = -r->ron[1];
	r->ron[2] = -r->ron[2];
	r->pert[0] = -r->pert[0];
	r->pert[1] = -r->pert[1];
	r->pert[2] = -r->pert[2];
}


localhit(r, scene)		/* check for hit in the octree */
register RAY  *r;
register CUBE  *scene;
{
	OBJECT  cxset[MAXCSET+1];	/* set of checked objects */
	FVECT  curpos;			/* current cube position */
	int  sflags;			/* sign flags */
	double  t, dt;
	register int  i;

	nrays++;			/* increment trace counter */
	sflags = 0;
	for (i = 0; i < 3; i++) {
		curpos[i] = r->rorg[i];
		if (r->rdir[i] > FTINY)
			sflags |= 1 << i;
		else if (r->rdir[i] < -FTINY)
			sflags |= 0x10 << i;
	}
	if (sflags == 0)
		error(CONSISTENCY, "zero ray direction in localhit");
	t = 0.0;
	if (!incube(scene, curpos)) {
					/* find distance to entry */
		for (i = 0; i < 3; i++) {
					/* plane in our direction */
			if (sflags & 1<<i)
				dt = scene->cuorg[i];
			else if (sflags & 0x10<<i)
				dt = scene->cuorg[i] + scene->cusize;
			else
				continue;
					/* distance to the plane */
			dt = (dt - r->rorg[i])/r->rdir[i];
			if (dt > t)
				t = dt;	/* farthest face is the one */
		}
		t += FTINY;		/* fudge to get inside cube */
					/* advance position */
		for (i = 0; i < 3; i++)
			curpos[i] += r->rdir[i]*t;

		if (!incube(scene, curpos))	/* non-intersecting ray */
			return(0);
	}
	cxset[0] = 0;
	return(raymove(curpos, cxset, sflags, r, scene) == RAYHIT);
}


static int
raymove(pos, cxs, dirf, r, cu)		/* check for hit as we move */
FVECT  pos;			/* current position, modified herein */
OBJECT  *cxs;			/* checked objects, modified by checkhit */
int  dirf;			/* direction indicators to speed tests */
register RAY  *r;
register CUBE  *cu;
{
	int  ax;
	double  dt, t;

	if (istree(cu->cutree)) {		/* recurse on subcubes */
		CUBE  cukid;
		register int  br, sgn;

		cukid.cusize = cu->cusize * 0.5;	/* find subcube */
		VCOPY(cukid.cuorg, cu->cuorg);
		br = 0;
		if (pos[0] >= cukid.cuorg[0]+cukid.cusize) {
			cukid.cuorg[0] += cukid.cusize;
			br |= 1;
		}
		if (pos[1] >= cukid.cuorg[1]+cukid.cusize) {
			cukid.cuorg[1] += cukid.cusize;
			br |= 2;
		}
		if (pos[2] >= cukid.cuorg[2]+cukid.cusize) {
			cukid.cuorg[2] += cukid.cusize;
			br |= 4;
		}
		for ( ; ; ) {
			cukid.cutree = octkid(cu->cutree, br);
			if ((ax = raymove(pos,cxs,dirf,r,&cukid)) == RAYHIT)
				return(RAYHIT);
			sgn = 1 << ax;
			if (sgn & dirf)			/* positive axis? */
				if (sgn & br)
					return(ax);	/* overflow */
				else {
					cukid.cuorg[ax] += cukid.cusize;
					br |= sgn;
				}
			else
				if (sgn & br) {
					cukid.cuorg[ax] -= cukid.cusize;
					br &= ~sgn;
				} else
					return(ax);	/* underflow */
		}
		/*NOTREACHED*/
	}
	if (isfull(cu->cutree) && checkhit(r, cu, cxs))
		return(RAYHIT);
					/* advance to next cube */
	if (dirf&0x11) {
		dt = dirf&1 ? cu->cuorg[0] + cu->cusize : cu->cuorg[0];
		t = (dt - pos[0])/r->rdir[0];
		ax = 0;
	} else
		t = FHUGE;
	if (dirf&0x22) {
		dt = dirf&2 ? cu->cuorg[1] + cu->cusize : cu->cuorg[1];
		dt = (dt - pos[1])/r->rdir[1];
		if (dt < t) {
			t = dt;
			ax = 1;
		}
	}
	if (dirf&0x44) {
		dt = dirf&4 ? cu->cuorg[2] + cu->cusize : cu->cuorg[2];
		dt = (dt - pos[2])/r->rdir[2];
		if (dt < t) {
			t = dt;
			ax = 2;
		}
	}
	pos[0] += r->rdir[0]*t;
	pos[1] += r->rdir[1]*t;
	pos[2] += r->rdir[2]*t;
	return(ax);
}


static
checkhit(r, cu, cxs)		/* check for hit in full cube */
register RAY  *r;
CUBE  *cu;
OBJECT  *cxs;
{
	OBJECT  oset[MAXSET+1];
	register OBJREC  *o;
	register int  i;

	objset(oset, cu->cutree);
	checkset(oset, cxs);			/* eliminate double-checking */
	for (i = oset[0]; i > 0; i--) {
		o = objptr(oset[i]);
		if (o->omod == OVOID && issurface(o->otype))
			continue;		/* ignore void surfaces */
		(*ofun[o->otype].funp)(o, r);
	}
	if (r->ro == NULL)
		return(0);			/* no scores yet */

	return(incube(cu, r->rop));		/* hit OK if in current cube */
}


static
checkset(os, cs)		/* modify checked set and set to check */
register OBJECT  *os;			/* os' = os - cs */
register OBJECT  *cs;			/* cs' = cs + os */
{
	OBJECT  cset[MAXCSET+MAXSET+1];
	register int  i, j;
	int  k;
					/* copy os in place, cset <- cs */
	cset[0] = 0;
	k = 0;
	for (i = j = 1; i <= os[0]; i++) {
		while (j <= cs[0] && cs[j] < os[i])
			cset[++cset[0]] = cs[j++];
		if (j > cs[0] || os[i] != cs[j]) {	/* object to check */
			os[++k] = os[i];
			cset[++cset[0]] = os[i];
		}
	}
	if (!(os[0] = k))		/* new "to check" set size */
		return;			/* special case */
	while (j <= cs[0])		/* get the rest of cs */
		cset[++cset[0]] = cs[j++];
	if (cset[0] > MAXCSET)		/* truncate "checked" set if nec. */
		cset[0] = MAXCSET;
	/* setcopy(cs, cset); */	/* copy cset back to cs */
	os = cset;
	for (i = os[0]; i-- >= 0; )
		*cs++ = *os++;
}
