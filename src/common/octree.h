/* RCSid $Id: octree.h,v 2.7 2003/03/14 21:27:46 greg Exp $ */
/*
 *  octree.h - header file for routines using octrees.
 */

#include "copyright.h"

/*
 *	An octree is expressed as an integer which is either
 *	an index to eight other nodes, the empty tree, or an index
 *	to a set of objects.  If the octree has a value:
 *
 *		> -1:	it is an index to eight other nodes.
 *
 *		-1:	it is empty
 *
 *		< -1:	it is an index to a set of objects
 */

#ifndef  OCTREE
#define  OCTREE		int
#endif

#define  EMPTY		(-1)

#define  isempty(ot)	((ot) == EMPTY)
#define  isfull(ot)	((ot) < EMPTY)
#define  istree(ot)	((ot) > EMPTY)

#define  oseti(ot)	(-(ot)-2)	/* object set index */
#define  OCTBLKSIZ	04000		/* octree block size */
#define  octbi(ot)	((ot)>>11)	/* octree block index */
#define  octti(ot)	(((ot)&03777)<<3)/* octree index in block */

#ifndef  MAXOBLK
#ifdef  BIGMEM
#define  MAXOBLK	32767		/* maximum octree block */
#else
#define  MAXOBLK	4095		/* maximum octree block */
#endif
#endif

extern OCTREE  *octblock[MAXOBLK];	/* octree blocks */

#define  octkid(ot,br)	(octblock[octbi(ot)][octti(ot)+br])

/*
 *	The cube structure is used to hold an octree and its cubic
 *	boundaries.
 */

typedef struct {
	OCTREE  cutree;			/* the octree for this cube */
	FVECT  cuorg;			/* the cube origin */
	double  cusize;			/* the cube size */
}  CUBE;

extern CUBE  thescene;			/* the main scene */

				/* flags for reading and writing octrees */
#define  IO_CHECK	0		/* verify file type */
#define  IO_INFO	01		/* information header */
#define  IO_SCENE	02		/* objects */
#define  IO_TREE	04		/* octree */
#define  IO_FILES	010		/* object file names */
#define  IO_BOUNDS	020		/* octree boundary */
#define  IO_ALL		(~0)		/* everything */
				/* octree format identifier */
#define  OCTFMT		"Radiance_octree"
				/* magic number for octree files */
#define  MAXOBJSIZ	8		/* maximum sizeof(OBJECT) */
#define  OCTMAGIC	( 4 *MAXOBJSIZ+251)	/* increment first value */
				/* octree node types */
#define  OT_EMPTY	0
#define  OT_FULL	1
#define  OT_TREE	2
				/* return values for surface functions */
#define  O_MISS		0		/* no intersection */
#define  O_HIT		1		/* intersection */
#define  O_IN		2		/* cube contained entirely */

#ifdef NOPROTO

extern OCTREE	octalloc();
extern void	octfree();
extern void	octdone();
extern OCTREE	combine();
extern void	culocate();
extern void	cucopy();
extern int	incube();
extern int	readoct();
extern void	readscene();
extern void	writescene();

#else

extern OCTREE	octalloc(void);
extern void	octfree(OCTREE ot);
extern void	octdone(void);
extern OCTREE	combine(OCTREE ot);
extern void	culocate(CUBE *cu, FVECT pt);
extern void	cucopy(CUBE *cu1, CUBE *cu2);
extern int	incube(CUBE *cu, FVECT pt);

extern int	readoct(char *fname, int load, CUBE *scene, char *ofn[]);

extern void	readscene(FILE *fp, int objsiz);
extern void	writescene(int firstobj, int nobjs, FILE *fp);

#endif
