#ifndef lint
static const char	RCSid[] = "$Id: m_mirror.c,v 2.9 2003/02/25 02:47:22 greg Exp $";
#endif
/*
 * Routines for mirror material supporting virtual light sources
 */

#include "copyright.h"

#include  "ray.h"

#include  "otypes.h"

#include  "source.h"

/*
 * The real arguments for MAT_MIRROR are simply:
 *
 *	3 rrefl grefl brefl
 *
 * Additionally, the user may specify a single string argument
 * which is interpreted as the name of the material to use
 * instead of the mirror if the ray being considered is not
 * part of the direct calculation.
 */


static int  mir_proj(), mirrorproj();

VSMATERIAL  mirror_vs = {mir_proj, 1};


int
m_mirror(m, r)			/* shade mirrored ray */
register OBJREC  *m;
register RAY  *r;
{
	COLOR  mcolor;
	RAY  nr;
	int  rpure = 1;
	register int  i;
					/* check arguments */
	if (m->oargs.nfargs != 3 || m->oargs.nsargs > 1)
		objerror(m, USER, "bad number of arguments");
					/* check for substitute material */
	if (m->oargs.nsargs > 0 &&
			(r->rsrc < 0 || source[r->rsrc].so != r->ro)) {
		if (!strcmp(m->oargs.sarg[0], VOIDID)) {
			raytrans(r);
			return(1);
		}
		return(rayshade(r, lastmod(objndx(m), m->oargs.sarg[0])));
	}
					/* check for bad source ray */
	if (r->rsrc >= 0 && source[r->rsrc].so != r->ro)
		return(1);

	if (r->rod < 0.)		/* back is black */
		return(1);
					/* get modifiers */
	raytexture(r, m->omod);
					/* assign material color */
	setcolor(mcolor, m->oargs.farg[0],
			m->oargs.farg[1],
			m->oargs.farg[2]);
	multcolor(mcolor, r->pcol);
					/* compute reflected ray */
	if (r->rsrc >= 0) {			/* relayed light source */
		rayorigin(&nr, r, REFLECTED, 1.);
					/* ignore textures */
		for (i = 0; i < 3; i++)
			nr.rdir[i] = r->rdir[i] + 2.*r->rod*r->ron[i];
					/* source we're aiming for next */
		nr.rsrc = source[r->rsrc].sa.sv.sn;
	} else {				/* ordinary reflection */
		FVECT  pnorm;
		double  pdot;

		if (rayorigin(&nr, r, REFLECTED, bright(mcolor)) < 0)
			return(1);
		if (DOT(r->pert,r->pert) > FTINY*FTINY) {
			pdot = raynormal(pnorm, r);	/* use textures */
			for (i = 0; i < 3; i++)
				nr.rdir[i] = r->rdir[i] + 2.*pdot*pnorm[i];
			rpure = 0;
		}
						/* check for penetration */
		if (rpure || DOT(nr.rdir, r->ron) <= FTINY)
			for (i = 0; i < 3; i++)
				nr.rdir[i] = r->rdir[i] + 2.*r->rod*r->ron[i];
	}
	rayvalue(&nr);
	multcolor(nr.rcol, mcolor);
	addcolor(r->rcol, nr.rcol);
	if (rpure && r->ro != NULL && isflat(r->ro->otype))
		r->rt = r->rot + nr.rt;
	return(1);
}


static int
mir_proj(pm, o, s, n)		/* compute a mirror's projection */
MAT4  pm;
register OBJREC  *o;
SRCREC  *s;
int  n;
{
	FVECT  nv, sc;
	double  od;
	register int  i, j;
				/* get surface normal and offset */
	od = getplaneq(nv, o);
				/* check for extreme point for behind */
	VCOPY(sc, s->sloc);
	for (i = s->sflags & SFLAT ? SV : SW; i >= 0; i--)
		if (DOT(nv, s->ss[i]) > 0.)
			for (j = 0; j < 3; j++)
				sc[j] += s->ss[i][j];
		else
			for (j = 0; j < 3; j++)
				sc[j] -= s->ss[i][j];
	if (DOT(sc, nv) <= (s->sflags & SDISTANT ? FTINY : od+FTINY))
		return(0);
				/* everything OK -- compute projection */
	mirrorproj(pm, nv, od);
	return(1);
}


static int
mirrorproj(m, nv, offs)		/* get mirror projection for surface */
register MAT4  m;
FVECT  nv;
double  offs;
{
	register int  i, j;
					/* assign matrix */
	setident4(m);
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			m[i][j] -= 2.*nv[i]*nv[j];
		m[3][j] = 2.*offs*nv[j];
	}
}
