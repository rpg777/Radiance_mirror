/* Copyright (c) 1997 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for 3D warping routines.
 */

#include "lookup.h"

				/* interpolation flags */
#define W3EXACT		01		/* no interpolation (slow) */
#define W3FAST		02		/* discontinuous approx. (fast) */

				/* return flags for warp3d() */
#define W3OK		0		/* normal return status */
#define W3GAMUT		01		/* out of gamut */
#define W3BADMAP	02		/* singular map */
#define W3ERROR		04		/* system error (check errno) */

#define GNBITS	6		/* number of bits per grid size <= 8 */
#define MAXGN	(1<<GNBITS)	/* maximum grid dimension */

typedef unsigned char	GNDX[3];	/* grid index type */

typedef float	W3VEC[3];	/* vector type for 3D warp maps */

struct grid3d {
	W3VEC	gmin, gmax;		/* grid boundaries */
	GNDX	gn;			/* grid dimensions */
	W3VEC	gstep;			/* grid voxel size */
	unsigned char	flags;		/* interpolation flags */
	LUTAB	gtab;			/* grid lookup table */
};				/* a regular, sparse warping grid */

typedef struct {
	W3VEC	*ip, *op;		/* discrete input/output pairs */
	int	npts;			/* number of point pairs */
	W3VEC	llim, ulim;		/* lower and upper input limits */
	double	d2min, d2max;		/* min. and max. point distance^2 */
	struct grid3d	grid;		/* point conversion grid */
} WARP3D;			/* a warp map */

extern WARP3D	*new3dw(), *load3dw();

#define  W3VCPY(v1,v2)	((v1)[0]=(v2)[0],(v1)[1]=(v2)[1],(v1)[2]=(v2)[2])
