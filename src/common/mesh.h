/* RCSid $Id: mesh.h,v 2.3 2003/03/14 21:27:45 greg Exp $ */
/*
 * Header for compact triangle mesh geometry
 *
 *  Include after standard.h, object.h and octree.h
 */

#include "copyright.h"

#include "lookup.h"

#ifndef uint4
#define uint4	unsigned int4
#endif
#ifndef BYTE
#define BYTE	unsigned char
#endif

/*
 * Vertex space is minimized without compromising accuracy by using a
 * 4-byte unsigned int to indicate position in the enclosing octree cube.
 * The same trick is used for any local (u,v) coordinates, whose limits
 * are recorded separately in the parent MESH structure.  The uvlimit's
 * in the MESH structure are set such that (0,0) is out of range, so
 * we use this to indicate an unspecified local coordinate.
 * A vertex normal, if specified, is stored in a single 4-byte
 * integer using the codec in dircode.c.  The encodedir() function
 * never generates 0, so we can use this for unspecified normals.
 *
 * Vertex ID's are encoded using the bottom 8 bits of a 4-byte integer
 * to index a vertex in a patch indicated by the 22 bits above (8-29).
 * For triangle ID's, the top 22 bits (10-31) indicate the patch, and
 * the bit 9 (0x200) indicates whether the triangle joins patches.
 * If not, then the bottom 9 bits index into the local PTri array.
 * If it's a joiner, then the 8th bit indicates whether the triangle joins
 * two patches, in which case the bottom 8 bits index the PJoin2 array.
 * Otherwise, the bottom 8 bits index the PJoin1 array.
 *
 * These shenanigans minimize vertex reference memory requirements
 * in compiled mesh structures, where the octree leaves contain sets 
 * of triangle ID's rather than the more usual objects.  It seems like
 * a lot of effort, but it can reduce mesh storage by a factor of 3
 * or more.  This is important, as the whole point is to model very
 * complicated geometry with this structure, and memory is the main
 * limitation.
 */

/* A triangle mesh patch */
typedef struct {
	uint4		(*xyz)[3];	/* up to 256 patch vertices */
	int4		*norm;		/* vertex normals */
	uint4		(*uv)[2];	/* vertex local coordinates */
	struct PTri {
		BYTE		v1, v2, v3;	/* local vertices */
	}		*tri;		/* local triangles */
	short		solemat;	/* sole material */
	int2		*trimat;	/* or local material indices */
	struct PJoin1 {
		int4		v1j;		/* non-local vertex */
		int2		mat;		/* material index */
		BYTE		v2, v3;		/* local vertices */
	}		*j1tri;		/* joiner triangles */
	struct PJoin2 {
		int4		v1j, v2j;	/* non-local vertices */
		int2		mat;		/* material index */
		BYTE		v3;		/* local vertex */
	}		*j2tri;		/* double joiner triangles */
	short		nverts;		/* vertex count */
	short		ntris;		/* triangle count */
	short		nj1tris;	/* joiner triangle count */
	short		nj2tris;	/* double joiner triangle count */
} MESHPATCH;

/* A loaded mesh */
typedef struct mesh {
	char		*name;		/* mesh file name */
	int		nref;		/* reference count */
	int		ldflags;	/* what we've loaded */
	CUBE		mcube;		/* bounds and octree */
	FLOAT		uvlim[2][2];	/* local coordinate extrema */
	OBJECT		mat0;		/* base material index */
	OBJECT		nmats;		/* number of materials */
	MESHPATCH	*patch;		/* mesh patch list */
	int		npatches;	/* number of mesh patches */
	OBJREC		*pseudo;	/* mesh pseudo objects */
	LUTAB		lut;		/* vertex lookup table */
	struct mesh	*next;		/* next mesh in list */
} MESH;

/* A mesh instance */
typedef struct {
	FULLXF		x;		/* forward and backward transforms */
	MESH		*msh;		/* mesh object reference */
} MESHINST;

				/* vertex flags */
#define	MT_V		01
#define MT_N		02
#define MT_UV		04
#define MT_ALL		07

/* A mesh vertex */
typedef struct {
	int		fl;		/* setting flags */
	FVECT		v;		/* vertex location */
	FVECT		n;		/* vertex normal */
	FLOAT		uv[2];		/* local coordinates */
} MESHVERT;

				/* mesh format identifier */
#define MESHFMT		"Radiance_tmesh"
				/* magic number for mesh files */
#define MESHMAGIC	( 1 *MAXOBJSIZ+311)	/* increment first value */

#ifdef NOPROTO

extern MESH	*getmesh();
extern INSTANCE	*getmeshinst();
extern int	getmeshtrivid();
extern int	getmeshvert();
extern int	getmeshtri();
extern OBJREC	*getmeshpseudo();
extern int4	addmeshvert();
extern OBJECT	addmeshtri();
extern char	*checkmesh();
extern void	printmeshstats();
extern void	freemesh();
extern void	freemeshinst();
extern void	readmesh();
extern void	writemesh();

#else

extern MESH	*getmesh(char *mname, int flags);
extern MESHINST	*getmeshinst(OBJREC *o, int flags);
extern int	getmeshtrivid(int4 tvid[3], OBJECT *mo,
				MESH *mp, OBJECT ti);
extern int	getmeshvert(MESHVERT *vp, MESH *mp, int4 vid, int what);
extern int	getmeshtri(MESHVERT tv[3], OBJECT *mo,
				MESH *mp, OBJECT ti, int what);
extern OBJREC	*getmeshpseudo(MESH *mp, OBJECT mo);
extern int4	addmeshvert(MESH *mp, MESHVERT *vp);
extern OBJECT	addmeshtri(MESH *mp, MESHVERT tv[3], OBJECT mo);
extern char	*checkmesh(MESH *mp);
extern void	printmeshstats(MESH *ms, FILE *fp);
extern void	freemesh(MESH *ms);
extern void	freemeshinst(OBJREC *o);
extern void	readmesh(MESH *mp, char *path, int flags);
extern void	writemesh(MESH *mp, FILE *fp);

#endif /* NOPROTO */
