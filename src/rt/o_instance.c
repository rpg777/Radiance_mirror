/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  o_instance.c - routines for computing ray intersections with octrees.
 *
 *     11/11/88
 */

#include  "ray.h"

#include  "instance.h"


o_instance(o, r)		/* compute ray intersection with octree */
OBJREC  *o;
register RAY  *r;
{
	RAY  rcont;
	register INSTANCE  *in;
	register int  i;
					/* get the octree */
	in = getinstance(o, IO_ALL);
					/* duplicate and clear ray */
	copystruct(&rcont, r);
	rayclear(&rcont);
					/* transform ray to new space */
	multp3(rcont.rorg, r->rorg, in->x.b.xfm);
	multv3(rcont.rdir, r->rdir, in->x.b.xfm);
	for (i = 0; i < 3; i++)
		rcont.rdir[i] /= in->x.b.sca;
					/* trace it */
	if (!localhit(&rcont, &in->obj->scube))
		return(0);			/* missed */
	if (rcont.rot * in->x.f.sca >= r->rot)
		return(0);			/* not close enough */

	if (o->omod != OVOID) {		/* if we have modifier, use it */
		r->ro = o;
		r->rox = NULL;
	} else {			/* else use theirs */
		r->ro = rcont.ro;
		if (rcont.rox != NULL) {
			newrayxf(r);		/* allocate transformation */
					/* NOTE: r->rox may equal rcont.rox! */
			multmat4(r->rox->f.xfm, rcont.rox->f.xfm, in->x.f.xfm);
			r->rox->f.sca = rcont.rox->f.sca * in->x.f.sca;
			multmat4(r->rox->b.xfm, in->x.b.xfm, rcont.rox->b.xfm);
			r->rox->b.sca = in->x.b.sca * rcont.rox->b.sca;
		} else
			r->rox = &in->x;
	}
					/* transform it back */
	r->rot = rcont.rot * in->x.f.sca;
	multp3(r->rop, rcont.rop, in->x.f.xfm);
	multv3(r->ron, rcont.ron, in->x.f.xfm);
	for (i = 0; i < 3; i++)
		r->ron[i] /= in->x.f.sca;
	r->rod = rcont.rod;
					/* return hit */
	return(1);
}
