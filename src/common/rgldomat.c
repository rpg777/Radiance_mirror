/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Invocation routines for Radiance -> OpenGL materials.
 */

#include "radogl.h"


domatobj(mp, cent)		/* generate OpenGL material for object */
register MATREC	*mp;
FVECT	cent;
{
	GLfloat	vec[4];

	if (mp == NULL | !domats)
		return;
	if (islight(mp->type)) {
		vec[0] = colval(mp->u.l.emission,RED);
		vec[1] = colval(mp->u.l.emission,GRN);
		vec[2] = colval(mp->u.l.emission,BLU);
		vec[3] = 1.;
		glMaterialfv(GL_FRONT, GL_EMISSION, vec);
		vec[0] = vec[1] = vec[2] = 0.; vec[3] = 1.;
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, vec);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, vec);
	} else {
		vec[0] = colval(mp->u.m.ambdiff,RED);
		vec[1] = colval(mp->u.m.ambdiff,GRN);
		vec[2] = colval(mp->u.m.ambdiff,BLU);
		vec[3] = 1.;
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, vec);
		vec[0] = colval(mp->u.m.specular,RED);
		vec[1] = colval(mp->u.m.specular,GRN);
		vec[2] = colval(mp->u.m.specular,BLU);
		vec[3] = 1.;
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, vec);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mp->u.m.specexp);
		vec[0] = vec[1] = vec[2] = 0.; vec[3] = 1.;
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, vec);
	}
	rgl_checkerr("in domatobj");
}


domatvert(mp, v, n)		/* generate OpenGL material for vertex */
MATREC	*mp;
FVECT	v, n;
{
}
