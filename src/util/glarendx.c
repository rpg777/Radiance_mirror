/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Compute Glare Index given by program name:
 *
 *	gnuth_dgr -	Gnuth discomfort glare rating
 *	gnuth_vcp -	Gnuth visual comfort probability
 *
 *		12 April 1991	Greg Ward	EPFL
 */

#include "standard.h"
#include "view.h"

extern double	erfc();

double	posindex();
int	headline();

double	direct(), gnuth_dgr(), gnuth_vcp();

struct named_func {
	char	*name;
	double	(*func)();
} all_funcs[] = {
	{"direct", direct},
	{"gnuth_dgr", gnuth_dgr},
	{"gnuth_vcp", gnuth_vcp},
	{NULL}
};

struct glare_src {
	FVECT	dir;		/* source direction */
	double	dom;		/* solid angle */
	double	lum;		/* average luminance */
	struct glare_src	*next;
} *all_srcs = NULL;

struct glare_dir {
	double	ang;		/* angle (in radians) */
	double	indirect;	/* indirect illuminance */
	struct glare_dir	*next;
} *all_dirs = NULL;

#define newp(type)	(type *)malloc(sizeof(type))

char	*progname;
int	print_header = 1;

VIEW	midview = STDVIEW;


main(argc, argv)
int	argc;
char	*argv[];
{
	extern char	*rindex();
	struct named_func	*funp;
	char	*progtail;
	int	i;
					/* get program name */
	progname = argv[0];
	progtail = rindex(progname, '/');	/* final component */
	if (progtail == NULL)
		progtail = progname;
	else
		progtail++;
					/* get options */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 't':
			progtail = argv[++i];
			break;
		case 'h':
			print_header = 0;
			break;
		default:
			goto userr;
		}
	if (i < argc-1)
		goto userr;
	if (i == argc-1)		/* open file */
		if (freopen(argv[i], "r", stdin) == NULL) {
			perror(argv[i]);
			exit(1);
		}
					/* read header */
	getheader(stdin, headline);
	if (print_header) {		/* add to header */
		printargs(i, argv, stdout);
		putchar('\n');
	}
					/* set view */
	if (setview(&midview) != NULL) {
		fprintf(stderr, "%s: bad view information in input\n");
		exit(1);
	}
					/* get findglare data */
	read_input();
					/* find calculation */
	for (funp = all_funcs; funp->name != NULL; funp++)
		if (!strcmp(funp->name, progtail)) {
			print_values(funp->func);
			exit(0);		/* we're done */
		}
					/* invalid function */
	fprintf(stderr, "%s: unknown function!\n", progtail);
	exit(1);
userr:
	fprintf(stderr, "Usage: %s [-t type][-h] [input]\n", progname);
	exit(1);
}


headline(s)			/* get line from header */
char	*s;
{
	if (print_header)		/* copy to output */
		fputs(s, stdout);
	if (!strncmp(s, VIEWSTR, VIEWSTRL))
		sscanview(&midview, s+VIEWSTRL);
}


read_input()			/* read glare sources from stdin */
{
#define	S_SEARCH	0
#define S_SOURCE	1
#define S_DIREC		2
	int	state = S_SEARCH;
	char	buf[128];
	register struct glare_src	*gs;
	register struct glare_dir	*gd;

	while (fgets(buf, sizeof(buf), stdin) != NULL)
		switch (state) {
		case S_SEARCH:
			if (!strcmp(buf, "BEGIN glare source\n"))
				state = S_SOURCE;
			else if (!strcmp(buf, "BEGIN indirect illuminance\n"))
				state = S_DIREC;
			break;
		case S_SOURCE:
			if (!strncmp(buf, "END", 3)) {
				state = S_SEARCH;
				break;
			}
			if ((gs = newp(struct glare_src)) == NULL)
				goto memerr;
			if (sscanf(buf, "%lf %lf %lf %lf %lf",
					&gs->dir[0], &gs->dir[1], &gs->dir[2],
					&gs->dom, &gs->lum) != 5)
				goto readerr;
			gs->next = all_srcs;
			all_srcs = gs;
			break;
		case S_DIREC:
			if (!strncmp(buf, "END", 3)) {
				state = S_SEARCH;
				break;
			}
			if ((gd = newp(struct glare_dir)) == NULL)
				goto memerr;
			if (sscanf(buf, "%lf %lf",
					&gd->ang, &gd->indirect) != 2)
				goto readerr;
			gd->ang *= PI/180.0;	/* convert to radians */
			gd->next = all_dirs;
			all_dirs = gd;
			break;
		}
	return;
memerr:
	perror(progname);
	exit(1);
readerr:
	fprintf(stderr, "%s: read error on input\n", progname);
	exit(1);
#undef S_SEARCH
#undef S_SOURCE
#undef S_DIREC
}


print_values(funp)		/* print out calculations */
double	(*funp)();
{
	register struct glare_dir	*gd;

	for (gd = all_dirs; gd != NULL; gd = gd->next)
		printf("%f\t%f\n", gd->ang*(180.0/PI), (*funp)(gd));
}


double
direct(gd)			/* compute direct illuminance */
struct glare_dir	*gd;
{
	FVECT	mydir;
	double	d, dval;
	register struct glare_src	*gs;

	spinvector(mydir, midview.vdir, midview.vup, gd->ang);
	dval = 0.0;
	for (gs = all_srcs; gs != NULL; gs = gs->next) {
		d = DOT(mydir,gs->dir);
		if (d > FTINY)
			dval += d * gs->dom * gs->lum;
	}
	return(dval);
}


/*
 * posindex -	compute glare position index from:
 *
 *	Source Direction
 *	View Direction
 *	View Up Direction
 *
 * All vectors are assumed to be normalized.
 * This function is an implementation of the method proposed by
 * Robert Levin in his 1975 JIES article.
 * We return a value less than zero for improper positions.
 */

double
posindex(sd, vd, vu)			/* compute position index */
FVECT	sd, vd, vu;
{
	double	sigma, tau;
	double	d;

	d = DOT(sd,vd);
	if (d <= 0.0)
		return(-1.0);
	sigma = acos(d) * (180./PI);
	tau = acos(DOT(sd,vu)/sqrt(1.0-d*d)) * (180./PI);
	return( exp( sigma*( (35.2 - tau*.31889 - 1.22*exp(-.22222*tau))*1e-3
			+ sigma*(21. + tau*(.26667 + tau*-.002963))*1e-5 )
		) );
}


double
gnuth_dgr(gd)		/* compute Gnuth discomfort glare rating */
struct glare_dir	*gd;
{
#define q(w)	(20.4*w+1.52*pow(w,.2)-.075)
	register struct glare_src	*gs;
	double	p;
	double	sum;
	int	n;

	sum = 0.0; n = 0;
	for (gs = all_srcs; gs != NULL; gs = gs->next) {
		p = posindex(gs->dir, midview.vdir, midview.vup);
		if (p <= FTINY)
			continue;
		sum += gs->lum * q(gs->dom) / p;
		n++;
	}
	if (n == 0)
		return(0.0);
	else
		return( pow(
			.5*sum/pow(direct(gd)+gd->indirect,.44),
			pow((double)n, -.0914) ) );
#undef q
}


extern double	erf(), erfc();

#ifndef M_SQRT2
#define	M_SQRT2	1.41421356237309504880
#endif

#define norm_integral(z)	(1.-.5*erfc((z)/M_SQRT2))


double
gnuth_vcp(gd)		/* compute Gnuth visual comfort probability */
struct glare_dir	*gd;
{
	return(100.*norm_integral(-6.374+1.3227*log(gnuth_dgr(gd))));
}
