/* RCSid $Id: ranimove.h,v 3.2 2003/02/25 02:47:24 greg Exp $ */
/*
 *  ranimove.h
 *
 * Radiance object animation program
 *
 * The main difference between this program and ranimate is that
 * ranimove is optimized for object motion, and includes a complete
 * blur simulation.  We also have a number of clever schemes
 * for optimizing the computation, allowing rendering time
 * per frame and noticeable difference threshold to be specified.
 * Parallel rendering uses multiple processors on the local host,
 * and network rendering is not directly supported.  (However, no
 * one says you can't run ranimove on other machines at the
 * same time; just be careful not to overlap frames.)
 *
 * See the ranimove(1) man page for further details.
 */

#include "copyright.h"

#include "ray.h"
#include "view.h"
#include "vars.h"
				/* input variables (alphabetical by name) */
#define BASENAME	0		/* output image base name */
#define END		1		/* number of animation frames */
#define EXPOSURE	2		/* how to compute exposure */
#define HIGHQ		3		/* high quality setting */
#define LOWQ		4		/* low quality setting */
#define MBLUR		5		/* motion blur parameter */
#define MOVE		6		/* object movement */
#define OCONV		7		/* oconv options */
#define OCTREEF		8		/* octree file name */
#define RATE		9		/* frame rate (fps) */
#define RESOLUTION	10		/* desired final resolution */
#define RIF		11		/* rad input file */
#define VIEWFILE	12		/* animation frame views */

#define NV_INIT		13		/* number of variables */

#define VV_INIT { \
		{"BASENAME",	3,	0,	NULL,	onevalue}, \
		{"END",		3,	0,	NULL,	intvalue}, \
		{"EXPOSURE",	3,	0,	NULL,	onevalue}, \
		{"highq",	2,	0,	NULL,	catvalues}, \
		{"lowq",	2,	0,	NULL,	catvalues}, \
		{"MBLUR",	2,	0,	NULL,	fltvalue}, \
		{"move",	2,	0,	NULL,	NULL}, \
		{"oconv",	2,	0,	NULL,	catvalues}, \
		{"OCTREE",	3,	0,	NULL,	onevalue}, \
		{"RATE",	2,	0,	NULL,	fltvalue}, \
		{"RESOLUTION",	3,	0,	NULL,	onevalue}, \
		{"RIF",		3,	0,	NULL,	onevalue}, \
		{"VIEWFILE",	2,	0,	NULL,	onevalue} \
	}

struct ObjMove {
	int		parent;		/* parent object index */
	char		name[64];	/* object name */
	char		xf_file[128];	/* transform file name */
	char		spec[512];	/* object file or command */
	char		prio_file[128];	/* priority file name */
	int		cfm;		/* current frame number */
	char		xfs[512];	/* part transform arguments */
	MAT4		xfm;		/* part transform matrix */
	MAT4		cxfm;		/* combined transform matrix */
	MAT4		bxfm;		/* transform to previous frame */
	double		prio;		/* part priority */
	double		cprio;		/* combined priority */
};

extern int		silent;		/* run silently? */

extern int		quickstart;	/* time initial frame as well? */

extern int		nprocs;		/* number of rendering processes */

extern int		rtperfrm;	/* seconds to spend per frame */

extern double		ndthresh;	/* noticeable difference threshold */
extern int		ndtset;		/* did user set ndthresh? */

extern int		fbeg;		/* starting frame */
extern int		fend;		/* ending frame */
extern int		fcur;		/* current frame being rendered */

extern char		lorendoptf[];	/* LQ options file */
extern RAYPARAMS	lorendparams;	/* LQ rendering parameters */
extern char		hirendoptf[];	/* HQ options file */
extern RAYPARAMS	hirendparams;	/* HQ rendering parameters */
extern RAYPARAMS	*curparams;	/* current parameter settings */
extern int		twolevels;	/* low and high quality differ */

extern double	mblur;			/* vflt(MBLUR) */
extern double	rate;			/* vflt(RATE) */

extern char		objtmpf[];	/* object temporary file */

extern struct ObjMove	*obj_move;	/* object movements */

extern int		haveprio;	/* high-level saliency specified */

extern int		gargc;		/* global argc for printargs */
extern char		**gargv;	/* global argv for printargs */

VIEW	*getview();
int	countviews();
int	getmove();
char	*getexp(), *getoctspec(), *getobjname(), *getxf();
double	expspec_val(), obj_prio();
void	setdefaults(), setmove(), animate(), getradfile(), setrendparams();
void	init_frame(), filter_frame(), send_frame(), free_frame();
int	refine_frame();
double	getTime();

/*************************************************************************
 * Frame rendering stuff (defined in ranimove1.c and ranimove2.c)
 */
			/* enumerated accuracy map values */
#define ANOVAL		0	/* unevaluated pixel */
#define ALOWQ		1	/* single low-quality sample */
#define AHIGHQ		2	/* single high-quality sample */
#define AMIN		3	/* start of error lookup table */
#define ADISTANT	255	/* ray went off to infinity */

extern double	acctab[256];	/* accuracy value table */

extern int	hres, vres;	/* frame resolution (fcur) */
extern double	pixaspect;	/* pixel aspect ratio */

extern VIEW	vw;		/* view for this frame */
extern COLOR	*cbuffer;	/* color at each pixel */
extern float	*zbuffer;	/* depth at each pixel */
extern OBJECT	*obuffer;	/* object id at each pixel */
extern short	*xmbuffer;	/* x motion at each pixel */
extern short	*ymbuffer;	/* y motion at each pixel */
extern BYTE	*abuffer;	/* accuracy at each pixel */
extern BYTE	*sbuffer;	/* sample count per pixel */

extern VIEW	vwprev;		/* last frame's view */
extern COLOR	*cprev;		/* last frame colors */
extern float	*zprev;		/* last frame depth */
extern OBJECT	*oprev;		/* last frame objects */
extern BYTE	*aprev;		/* last frame accuracy */

extern float	*cerrmap;	/* conspicuous error map */
extern int	cerrzero;	/* is all of cerrmap zero? */
extern COLOR	*val2map;	/* value-squared map for variance */

extern double	frm_stop;	/* when to stop rendering this frame */

extern double	hlsmax;		/* maximum high-level saliency */

#define CSF_SMN		(1./0.82)	/* 1/avg_tracking_efficacy */

#define outbuffer	cprev	/* used to hold final output */
#define wbuffer		zprev	/* used for final filtering */

#define fndx(x,y)	((y)*hres + (x))

#define MO_UNK		-32768	/* unknown motion value */

#define FOV_DEG		1.0	/* foveal radius (degrees) */

#define LOG_E1		(-0.0233)	/* ln(0.977) */
#define errorf(i)	exp(LOG_E1*((i)-AMIN))
#define errori(e)	(int)(log(e)*(1./LOG_E1) + (AMIN+.5))

#define NSAMPOK		5	/* samples enough for error estimation */

#define NPINTERP	4	/* number of pixels to interpolate */

#define ATIDIFF		7	/* error difference for time extrapolation */

void	write_map(), sample_pos(), comp_frame_error(), conspicuity();
int	getclosest(), getambcolor(), refine_first();
double	sample_wt(), estimaterr(), comperr(); 
