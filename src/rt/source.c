/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  source.c - routines dealing with illumination sources.
 *
 *     8/20/85
 */

#include  "ray.h"

#include  "octree.h"

#include  "otypes.h"

#include  "source.h"

/*
 * Structures used by direct()
 */

typedef struct {
	int  sno;		/* source number */
	FVECT  dir;		/* source direction */
	COLOR  coef;		/* material coefficient */
	COLOR  val;		/* contribution */
}  CONTRIB;		/* direct contribution */

typedef struct {
	int  sndx;		/* source index (to CONTRIB array) */
	float  brt;		/* brightness (for comparison) */
}  CNTPTR;		/* contribution pointer */

static CONTRIB  *srccnt;		/* source contributions in direct() */
static CNTPTR  *cntord;			/* source ordering in direct() */
static int  maxcntr = 0;		/* size of contribution arrays */


marksources()			/* find and mark source objects */
{
	int  i;
	register OBJREC  *o, *m;
	register int  ns;
					/* initialize dispatch table */
	initstypes();
					/* find direct sources */
	for (i = 0; i < nobjects; i++) {
	
		o = objptr(i);

		if (!issurface(o->otype) || o->omod == OVOID)
			continue;

		m = objptr(o->omod);

		if (!islight(m->otype))
			continue;
	
		if (m->oargs.nfargs != (m->otype == MAT_GLOW ? 4 :
				m->otype == MAT_SPOT ? 7 : 3))
			objerror(m, USER, "bad # arguments");

		if (m->otype == MAT_GLOW &&
				o->otype != OBJ_SOURCE &&
				m->oargs.farg[3] <= FTINY)
			continue;			/* don't bother */

		if (sfun[o->otype].of == NULL ||
				sfun[o->otype].of->setsrc == NULL)
			objerror(o, USER, "illegal material");

		if ((ns = newsource()) < 0)
			goto memerr;

		setsource(&source[ns], o);

		if (m->otype == MAT_GLOW) {
			source[ns].sflags |= SPROX;
			source[ns].sl.prox = m->oargs.farg[3];
			if (o->otype == OBJ_SOURCE)
				source[ns].sflags |= SSKIP;
		} else if (m->otype == MAT_SPOT) {
			source[ns].sflags |= SSPOT;
			if ((source[ns].sl.s = makespot(m)) == NULL)
				goto memerr;
			if (source[ns].sflags & SFLAT &&
				!checkspot(source[ns].sl.s,source[ns].snorm)) {
				objerror(o, WARNING,
					"invalid spotlight direction");
				source[ns].sflags |= SSKIP;
			}
		}
	}
	if (nsources <= 0) {
		error(WARNING, "no light sources found");
		return;
	}
	markvirtuals();			/* find and add virtual sources */
				/* allocate our contribution arrays */
	maxcntr = nsources + MAXSPART*2;	/* start with this many */
	srccnt = (CONTRIB *)malloc(maxcntr*sizeof(CONTRIB));
	cntord = (CNTPTR *)malloc(maxcntr*sizeof(CNTPTR));
	if (srccnt == NULL || cntord == NULL)
		goto memerr;
	return;
memerr:
	error(SYSTEM, "out of memory in marksources");
}


srcray(sr, r, si)		/* send a ray to a source, return domega */
register RAY  *sr;		/* returned source ray */
RAY  *r;			/* ray which hit object */
SRCINDEX  *si;			/* source sample index */
{
    double  d;				/* distance to source */
    FVECT  vd;
    register SRCREC  *srcp;
    register int  i;

    rayorigin(sr, r, SHADOW, 1.0);		/* ignore limits */

    while ((d = nextssamp(sr, si)) != 0.0) {
	sr->rsrc = si->sn;			/* remember source */
	srcp = source + si->sn;
	if (srcp->sflags & SDISTANT) {
		if (srcp->sflags & SSPOT) {	/* check location */
			for (i = 0; i < 3; i++)
			    vd[i] = srcp->sl.s->aim[i] - sr->rorg[i];
			d = DOT(sr->rdir,vd);
			if (d <= FTINY)
				continue;
			d = DOT(vd,vd) - d*d;
			if (PI*d > srcp->sl.s->siz)
				continue;
		}
		return(1);		/* sample OK */
	}
				/* local source */
						/* check proximity */
	if (srcp->sflags & SPROX && d > srcp->sl.prox)
		continue;
						/* check angle */
	if (srcp->sflags & SSPOT) {
		if (srcp->sl.s->siz < 2.0*PI *
				(1.0 + DOT(srcp->sl.s->aim,sr->rdir)))
			continue;
					/* adjust solid angle */
		si->dom *= d*d;
		d += srcp->sl.s->flen;
		si->dom /= d*d;
	}
	return(1);			/* sample OK */
    }
    return(0);			/* no more samples */
}


srcvalue(r)			/* punch ray to source and compute value */
RAY  *r;
{
	register SRCREC  *sp;

	sp = &source[r->rsrc];
	if (sp->sflags & SVIRTUAL) {	/* virtual source */
					/* check intersection */
		if (!(*ofun[sp->so->otype].funp)(sp->so, r))
			return;
		raycont(r);		/* compute contribution */
		return;
	}
					/* compute intersection */
	if (sp->sflags & SDISTANT ? sourcehit(r) :
			(*ofun[sp->so->otype].funp)(sp->so, r)) {
		if (sp->sa.success >= 0)
			sp->sa.success++;
		raycont(r);		/* compute contribution */
		return;
	}
	if (sp->sa.success < 0)
		return;			/* bitched already */
	sp->sa.success -= AIMREQT;
	if (sp->sa.success >= 0)
		return;			/* leniency */
	sprintf(errmsg, "aiming failure for light source \"%s\"",
			sp->so->oname);
	error(WARNING, errmsg);		/* issue warning */
}


static int
cntcmp(sc1, sc2)			/* contribution compare (descending) */
register CNTPTR  *sc1, *sc2;
{
	if (sc1->brt > sc2->brt)
		return(-1);
	if (sc1->brt < sc2->brt)
		return(1);
	return(0);
}


direct(r, f, p)				/* add direct component */
RAY  *r;			/* ray that hit surface */
int  (*f)();			/* direct component coefficient function */
char  *p;			/* data for f */
{
	extern int  (*trace)();
	extern double  pow();
	register int  sn;
	SRCINDEX  si;
	int  nshadcheck, ncnts;
	int  nhits;
	double  prob, ourthresh, hwt;
	RAY  sr;
			/* NOTE: srccnt and cntord global so no recursion */
	if (nsources <= 0)
		return;		/* no sources?! */
						/* potential contributions */
	initsrcindex(&si);
	for (sn = 0; srcray(&sr, r, &si); sn++) {
		if (sn >= maxcntr) {
			maxcntr = sn + MAXSPART;
			srccnt = (CONTRIB *)realloc((char *)srccnt,
					maxcntr*sizeof(CONTRIB));
			cntord = (CNTPTR *)realloc((char *)cntord,
					maxcntr*sizeof(CNTPTR));
			if (srccnt == NULL || cntord == NULL)
				error(SYSTEM, "out of memory in direct");
		}
		cntord[sn].sndx = sn;
		srccnt[sn].sno = sr.rsrc;
						/* compute coefficient */
		(*f)(srccnt[sn].coef, p, sr.rdir, si.dom);
		cntord[sn].brt = bright(srccnt[sn].coef);
		if (cntord[sn].brt <= 0.0)
			continue;
		VCOPY(srccnt[sn].dir, sr.rdir);
						/* compute potential */
		sr.revf = srcvalue;
		rayvalue(&sr);
		copycolor(srccnt[sn].val, sr.rcol);
		multcolor(srccnt[sn].val, srccnt[sn].coef);
		cntord[sn].brt = bright(srccnt[sn].val);
	}
						/* sort contributions */
	qsort(cntord, sn, sizeof(CNTPTR), cntcmp);
	{					/* find last */
		register int  l, m;

		ncnts = l = sn;
		sn = 0;
		while ((m = (sn + ncnts) >> 1) != l) {
			if (cntord[m].brt > 0.0)
				sn = m;
			else
				ncnts = m;
			l = m;
		}
	}
                                                /* accumulate tail */
        for (sn = ncnts-1; sn > 0; sn--)
                cntord[sn-1].brt += cntord[sn].brt;
						/* compute number to check */
	nshadcheck = pow((double)ncnts, shadcert) + .5;
						/* modify threshold */
	ourthresh = shadthresh / r->rweight;
						/* test for shadows */
	nhits = 0;
	for (sn = 0; sn < ncnts; sn++) {
						/* check threshold */
		if ((sn+nshadcheck>=ncnts ? cntord[sn].brt :
				cntord[sn].brt-cntord[sn+nshadcheck].brt)
				< ourthresh*bright(r->rcol))
			break;
						/* test for hit */
		rayorigin(&sr, r, SHADOW, 1.0);
		VCOPY(sr.rdir, srccnt[cntord[sn].sndx].dir);
		sr.rsrc = srccnt[cntord[sn].sndx].sno;
		source[sr.rsrc].ntests++;	/* keep statistics */
		if (localhit(&sr, &thescene) &&
				( sr.ro != source[sr.rsrc].so ||
				source[sr.rsrc].sflags & SFOLLOW )) {
						/* follow entire path */
			raycont(&sr);
			if (trace != NULL)
				(*trace)(&sr);	/* trace execution */
			if (bright(sr.rcol) <= FTINY)
				continue;	/* missed! */
			copycolor(srccnt[cntord[sn].sndx].val, sr.rcol);
			multcolor(srccnt[cntord[sn].sndx].val,
					srccnt[cntord[sn].sndx].coef);
		}
						/* add contribution if hit */
		addcolor(r->rcol, srccnt[cntord[sn].sndx].val);
		nhits++;
		source[sr.rsrc].nhits++;
	}
					/* surface hit rate */
	if (sn > 0)
		hwt = (double)nhits / (double)sn;
	else
		hwt = 0.5;
#ifdef DEBUG
	sprintf(errmsg, "%d tested, %d untested, %f hit rate\n",
			sn, ncnts-sn, hwt);
	eputs(errmsg);
#endif
					/* add in untested sources */
	for ( ; sn < ncnts; sn++) {
		sr.rsrc = srccnt[cntord[sn].sndx].sno;
		prob = hwt * (double)source[sr.rsrc].nhits /
				(double)source[sr.rsrc].ntests;
		scalecolor(srccnt[cntord[sn].sndx].val, prob);
		addcolor(r->rcol, srccnt[cntord[sn].sndx].val);
	}
}
