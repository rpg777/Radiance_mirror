#ifndef lint
static const char	RCSid[] = "$Id: ra_xyze.c,v 2.6 2003/02/22 02:07:28 greg Exp $";
#endif
/*
 *  Program to convert between RADIANCE RGBE and XYZE formats
 *  Added white-balance adjustment 10/01 (GW).
 */

#include  <stdio.h>
#include  <string.h>
#include  <math.h>
#include  <time.h>
#include  "color.h"
#include  "resolu.h"

#ifdef MSDOS
#include  <fcntl.h>
#endif

int  rgbinp = -1;			/* input is RGBE? */

int  rgbout = 0;			/* output should be RGBE? */

RGBPRIMS  inprims = STDPRIMS;		/* input primaries */

RGBPRIMS  outprims = STDPRIMS;		/* output primaries */

double	expcomp = 1.0;			/* exposure compensation */

int  doflat = -1;			/* produce flat file? */

char  *progname;


int
headline(s)				/* process header line */
char	*s;
{
	char	fmt[32];

	if (formatval(fmt, s)) {	/* check if format string */
		if (!strcmp(fmt,COLRFMT))
			rgbinp = 1;
		else if (!strcmp(fmt,CIEFMT))
			rgbinp = 0;
		else
			rgbinp = -2;
		return(0);		/* don't echo */
	}
	if (isprims(s)) {		/* get input primaries */
		primsval(inprims, s);
		return(0);		/* don't echo */
	}
					/* should I grok colcorr also? */
	return(fputs(s, stdout));
}


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i;
#ifdef MSDOS
	extern int  _fmode;
	_fmode = O_BINARY;
	setmode(fileno(stdin), O_BINARY);
	setmode(fileno(stdout), O_BINARY);
#endif
	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'c':		/* rle-compressed output */
				doflat = 0;
				break;
			case 'u':		/* flat output */
				doflat = 1;
				break;
			case 'r':		/* RGBE output */
				rgbout = 1;
				break;
			case 'p':		/* RGB primaries */
				if (i+8 >= argc)
					goto userr;
				outprims[RED][CIEX] = atof(argv[++i]);
				outprims[RED][CIEY] = atof(argv[++i]);
				outprims[GRN][CIEX] = atof(argv[++i]);
				outprims[GRN][CIEY] = atof(argv[++i]);
				outprims[BLU][CIEX] = atof(argv[++i]);
				outprims[BLU][CIEY] = atof(argv[++i]);
				outprims[WHT][CIEX] = atof(argv[++i]);
				outprims[WHT][CIEY] = atof(argv[++i]);
				break;
			case 'e':		/* exposure compensation */
				expcomp = atof(argv[++i]);
				if (argv[i][0] == '+' || argv[i][0] == '-')
					expcomp = pow(2., expcomp);
				break;
			default:
				goto userr;
			}
		else
			break;

	if (doflat < 0)
		doflat = !rgbout;
	if (i < argc-2)
		goto userr;
	if (i <= argc-1 && freopen(argv[i], "r", stdin) == NULL) {
		fprintf(stderr, "%s: can't open input \"%s\"\n",
				progname, argv[i]);
		exit(1);
	}
	if (i == argc-2 && freopen(argv[i+1], "w", stdout) == NULL) {
		fprintf(stderr, "%s: can't open output \"%s\"\n",
				progname, argv[i+1]);
		exit(1);
	}
	getheader(stdin, headline, NULL);
	if (rgbinp == -2)
		quiterr("unrecognized input file format");
	if (rgbinp == -1)
		rgbinp = !rgbout;
	printargs(argc, argv, stdout);		/* add to header */
	convert();				/* convert picture */
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-r][-e exp][-c|-u]", progname);
	fprintf(stderr, "[-p rx ry gx gy bx by wx wy] [input [output]]\n");
	exit(1);
}


quiterr(err)		/* print message and exit */
char  *err;
{
	if (err != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	exit(0);
}


convert()				/* convert to XYZE or RGBE picture */
{
	int	order;
	int	xmax, ymax;
	COLORMAT	xfm;
	register COLOR	*scanin;
	register COLR	*scanout;
	double	ourexp = expcomp;
	int	y;
	register int	x;
						/* compute transform */
	if (rgbout) {
		if (rgbinp) {			/* RGBE -> RGBE */
			comprgb2rgbWBmat(xfm, inprims, outprims);
		} else {			/* XYZE -> RGBE */
			compxyz2rgbWBmat(xfm, outprims);
			ourexp *= WHTEFFICACY;
		}
	} else {
		if (rgbinp) {			/* RGBE -> XYZE */
			comprgb2xyzWBmat(xfm, inprims);
			ourexp /= WHTEFFICACY;
		} else {			/* XYZE -> XYZE */
			for (y = 0; y < 3; y++)
				for (x = 0; x < 3; x++)
					xfm[y][x] = x==y ? 1. : 0.;
		}
	}
	for (y = 0; y < 3; y++)
		for (x = 0; x < 3; x++)
			xfm[y][x] *= expcomp;
						/* get input resolution */
	if ((order = fgetresolu(&xmax, &ymax, stdin)) < 0)
		quiterr("bad picture format");
						/* complete output header */
	if (ourexp < 0.99 || ourexp > 1.01)
		fputexpos(ourexp, stdout);
	if (rgbout) {
		fputprims(outprims, stdout);
		fputformat(COLRFMT, stdout);
	} else
		fputformat(CIEFMT, stdout);
	putc('\n', stdout);
	fputresolu(order, xmax, ymax, stdout);
						/* allocate scanline */
	scanin = (COLOR *)malloc(xmax*sizeof(COLOR));
	if (scanin == NULL)
		quiterr("out of memory in convert");
	scanout = doflat ? (COLR *)malloc(xmax*sizeof(COLR)) : NULL;
						/* convert image */
	for (y = 0; y < ymax; y++) {
		if (freadscan(scanin, xmax, stdin) < 0)
			quiterr("error reading input picture");
		for (x = 0; x < xmax; x++) {
			colortrans(scanin[x], xfm, scanin[x]);
			if (rgbout)
				clipgamut(scanin[x], bright(scanin[x]),
						CGAMUT_LOWER, cblack, cwhite);
		}
		if (scanout != NULL) {
			for (x = 0; x < xmax; x++)
				setcolr(scanout[x], colval(scanin[x],RED),
						colval(scanin[x],GRN),
						colval(scanin[x],BLU));
			fwrite((char *)scanout, sizeof(COLR), xmax, stdout);
		} else
			fwritescan(scanin, xmax, stdout);
		if (ferror(stdout))
			quiterr("error writing output picture");
	}
						/* free scanline */
	free((void *)scanin);
	if (scanout != NULL)
		free((void *)scanout);
}
