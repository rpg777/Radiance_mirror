/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  pvalue.c - program to print pixel values.
 *
 *     4/23/86
 */

#include  "standard.h"

#include  "color.h"

#include  "resolu.h"

#define	 min(a,b)		((a)<(b)?(a):(b))

RESOLU	picres;			/* resolution of picture */

int  uniq = 0;			/* print only unique values? */

int  doexposure = 0;		/* exposure change? (>100 to print) */

int  dataonly = 0;		/* data only format? */

int  brightonly = 0;		/* only brightness values? */

int  reverse = 0;		/* reverse conversion? */

int  format = 'a';		/* input/output format */
char  *fmtid = "ascii";		/* format identifier for header */

int  header = 1;		/* do header? */

int  resolution = 1;		/* put/get resolution string? */

int  original = 0;		/* convert to original values? */

int  wrongformat = 0;		/* wrong input format? */

double	gamcor = 1.0;		/* gamma correction */

int  ord[3] = {RED, GRN, BLU};	/* RGB ordering */
int  rord[4];			/* reverse ordering */

COLOR  exposure = WHTCOLOR;

char  *progname;

FILE  *fin;

int  (*getval)(), (*putval)();


main(argc, argv)
int  argc;
char  **argv;
{
	extern int  checkhead();
	double  d, expval = 1.0;
	int  i;

	progname = argv[0];

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-' || argv[i][0] == '+')
			switch (argv[i][1]) {
			case 'h':		/* header */
				header = argv[i][0] == '+';
				break;
			case 'H':		/* resolution string */
				resolution = argv[i][0] == '+';
				break;
			case 'u':		/* unique values */
				uniq = argv[i][0] == '-';
				break;
			case 'o':		/* original values */
				original = argv[i][0] == '-';
				break;
			case 'g':		/* gamma correction */
				gamcor = atof(argv[i+1]);
				if (argv[i][0] == '+')
					gamcor = 1.0/gamcor;
				i++;
				break;
			case 'e':		/* exposure correction */
				d = atof(argv[i+1]);
				if (argv[i+1][0] == '-' || argv[i+1][0] == '+')
					d = pow(2.0, d);
				if (argv[i][0] == '-')
					doexposure = 100;
				scalecolor(exposure, d);
				expval *= d;
				doexposure++;
				i++;
				break;
			case 'R':		/* reverse byte sequence */
				if (argv[i][0] == '-') {
					ord[0]=BLU; ord[1]=GRN; ord[2]=RED;
				} else {
					ord[0]=RED; ord[1]=GRN; ord[2]=BLU;
				}
				break;
			case 'r':		/* reverse conversion */
				reverse = argv[i][0] == '-';
				break;
			case 'b':		/* brightness values */
				brightonly = argv[i][0] == '-';
				break;
			case 'd':		/* data only (no indices) */
				dataonly = argv[i][0] == '-';
				switch (argv[i][2]) {
				case '\0':
				case 'a':		/* ascii */
					format = 'a';
					fmtid = "ascii";
					break;
				case 'i':		/* integer */
					format = 'i';
					fmtid = "ascii";
					break;
				case 'b':		/* byte */
					dataonly = 1;
					format = 'b';
					fmtid = "byte";
					break;
				case 'f':		/* float */
					dataonly = 1;
					format = 'f';
					fmtid = "float";
					break;
				case 'd':		/* double */
					dataonly = 1;
					format = 'd';
					fmtid = "double";
					break;
				default:
					goto unkopt;
				}
				break;
			case 'x':		/* x resolution */
			case 'X':		/* x resolution */
				resolution = 0;
				if (argv[i][0] == '-')
					picres.or |= XDECR;
				picres.xr = atoi(argv[++i]);
				break;
			case 'y':		/* y resolution */
			case 'Y':		/* y resolution */
				resolution = 0;
				if (argv[i][0] == '-')
					picres.or |= YDECR;
				if (picres.xr == 0)
					picres.or |= YMAJOR;
				picres.yr = atoi(argv[++i]);
				break;
			default:
unkopt:
				fprintf(stderr, "%s: unknown option: %s\n",
						progname, argv[i]);
				quit(1);
				break;
			}
		else
			break;
					/* recognize special formats */
	if (dataonly && format == 'b')
		if (brightonly)
			fmtid = "8-bit_grey";
		else
			fmtid = "24-bit_rgb";
					/* assign reverse ordering */
	rord[ord[0]] = 0;
	rord[ord[1]] = 1;
	rord[ord[2]] = 2;
					/* get input */
	if (i == argc) {
		fin = stdin;
	} else if (i == argc-1) {
		if ((fin = fopen(argv[i], "r")) == NULL) {
			fprintf(stderr, "%s: can't open file \"%s\"\n",
						progname, argv[i]);
			quit(1);
		}
	} else {
		fprintf(stderr, "%s: bad # file arguments\n", progname);
		quit(1);
	}

	set_io();

	if (reverse) {
#ifdef MSDOS
		setmode(fileno(stdout), O_BINARY);
		if (format != 'a' && format != 'i')
			setmode(fileno(fin), O_BINARY);
#endif
					/* get header */
		if (header) {
			if (checkheader(fin, fmtid, stdout) < 0) {
				fprintf(stderr, "%s: wrong input format\n",
						progname);
				quit(1);
			}
		} else
			newheader("RADIANCE", stdout);
					/* get resolution */
		if ((resolution && !fgetsresolu(&picres, fin)) ||
				picres.xr <= 0 || picres.yr <= 0) {
			fprintf(stderr, "%s: missing resolution\n", progname);
			quit(1);
		}
						/* add to header */
		printargs(i, argv, stdout);
		if (doexposure > 100)
			fputexpos(expval, stdout);
		fputformat(COLRFMT, stdout);
		putchar('\n');
		fputsresolu(&picres, stdout);	/* always put resolution */
		valtopix();
	} else {
#ifdef MSDOS
		setmode(fileno(fin), O_BINARY);
		if (format != 'a' && format != 'i')
			setmode(fileno(stdout), O_BINARY);
#endif
						/* get header */
		getheader(fin, checkhead, NULL);
		if (wrongformat) {
			fprintf(stderr, "%s: input not a Radiance picture\n",
					progname);
			quit(1);
		}
		if (!fgetsresolu(&picres, fin)) {
			fprintf(stderr, "%s: missing resolution\n", progname);
			quit(1);
		}
		if (header) {
			printargs(i, argv, stdout);
			if (doexposure > 100)
				fputexpos(expval, stdout);
			fputformat(fmtid, stdout);
			putchar('\n');
		}
		if (resolution)			/* put resolution */
			fputsresolu(&picres, stdout);
		pixtoval();
	}

	quit(0);
}


checkhead(line)				/* deal with line from header */
char  *line;
{
	char	fmt[32];
	double	d;
	COLOR	ctmp;

	if (formatval(fmt, line))
		wrongformat = strcmp(fmt, COLRFMT);
	else if (original && isexpos(line)) {
		d = 1.0/exposval(line);
		scalecolor(exposure, d);
		doexposure++;
	} else if (original && iscolcor(line)) {
		colcorval(ctmp, line);
		setcolor(exposure, colval(exposure,RED)/colval(ctmp,RED),
				colval(exposure,GRN)/colval(ctmp,GRN),
				colval(exposure,BLU)/colval(ctmp,BLU));
		doexposure++;
	} else if (header)
		fputs(line, stdout);
}


pixtoval()				/* convert picture to values */
{
	register COLOR	*scanln;
	int  dogamma;
	COLOR  lastc;
	FLOAT  hv[2];
	int  y;
	register int  x;

	scanln = (COLOR *)malloc(scanlen(&picres)*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	dogamma = gamcor < .95 || gamcor > 1.05;
	setcolor(lastc, 0.0, 0.0, 0.0);
	for (y = 0; y < numscans(&picres); y++) {
		if (freadscan(scanln, scanlen(&picres), fin) < 0) {
			fprintf(stderr, "%s: read error\n", progname);
			quit(1);
		}
		for (x = 0; x < scanlen(&picres); x++) {
			if (uniq)
				if (	colval(scanln[x],RED) ==
						colval(lastc,RED) &&
					colval(scanln[x],GRN) ==
						colval(lastc,GRN) &&
					colval(scanln[x],BLU) ==
						colval(lastc,BLU)	)
					continue;
				else
					copycolor(lastc, scanln[x]);
			if (doexposure)
				multcolor(scanln[x], exposure);
			if (dogamma)
				setcolor(scanln[x],
				pow(colval(scanln[x],RED), 1.0/gamcor),
				pow(colval(scanln[x],GRN), 1.0/gamcor),
				pow(colval(scanln[x],BLU), 1.0/gamcor));
			if (!dataonly) {
				pix2loc(hv, &picres, x, y);
				printf("%7d %7d ", (int)(hv[0]*picres.xr),
						(int)(hv[1]*picres.yr));
			}
			if ((*putval)(scanln[x], stdout) < 0) {
				fprintf(stderr, "%s: write error\n", progname);
				quit(1);
			}
		}
	}
	free((char *)scanln);
}


valtopix()			/* convert values to a pixel file */
{
	int  dogamma;
	register COLOR	*scanln;
	int  y;
	register int  x;

	scanln = (COLOR *)malloc(scanlen(&picres)*sizeof(COLOR));
	if (scanln == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		quit(1);
	}
	dogamma = gamcor < .95 || gamcor > 1.05;
	for (y = 0; y < numscans(&picres); y++) {
		for (x = 0; x < scanlen(&picres); x++) {
			if (!dataonly)
				fscanf(fin, "%*d %*d");
			if ((*getval)(scanln[x], fin) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				quit(1);
			}
			if (dogamma)
				setcolor(scanln[x],
					pow(colval(scanln[x],RED), gamcor),
					pow(colval(scanln[x],GRN), gamcor),
					pow(colval(scanln[x],BLU), gamcor));
			if (doexposure)
				multcolor(scanln[x], exposure);
		}
		if (fwritescan(scanln, scanlen(&picres), stdout) < 0) {
			fprintf(stderr, "%s: write error\n", progname);
			quit(1);
		}
	}
	free((char *)scanln);
}


quit(code)
int  code;
{
	exit(code);
}


getcascii(col, fp)		/* get an ascii color value from fp */
COLOR  col;
FILE  *fp;
{
	double	vd[3];

	if (fscanf(fp, "%lf %lf %lf", &vd[0], &vd[1], &vd[2]) != 3)
		return(-1);
	setcolor(col, vd[rord[RED]], vd[rord[GRN]], vd[rord[BLU]]);
	return(0);
}


getcdouble(col, fp)		/* get a double color value from fp */
COLOR  col;
FILE  *fp;
{
	double	vd[3];

	if (fread((char *)vd, sizeof(double), 3, fp) != 3)
		return(-1);
	setcolor(col, vd[rord[RED]], vd[rord[GRN]], vd[rord[BLU]]);
	return(0);
}


getcfloat(col, fp)		/* get a float color value from fp */
COLOR  col;
FILE  *fp;
{
	float  vf[3];

	if (fread((char *)vf, sizeof(float), 3, fp) != 3)
		return(-1);
	setcolor(col, vf[rord[RED]], vf[rord[GRN]], vf[rord[BLU]]);
	return(0);
}


getcint(col, fp)		/* get an int color value from fp */
COLOR  col;
FILE  *fp;
{
	int  vi[3];

	if (fscanf(fp, "%d %d %d", &vi[0], &vi[1], &vi[2]) != 3)
		return(-1);
	setcolor(col, (vi[rord[RED]]+.5)/256.,
			(vi[rord[GRN]]+.5)/256., (vi[rord[BLU]]+.5)/256.);
	return(0);
}


getcbyte(col, fp)		/* get a byte color value from fp */
COLOR  col;
FILE  *fp;
{
	BYTE  vb[3];

	if (fread((char *)vb, sizeof(BYTE), 3, fp) != 3)
		return(-1);
	setcolor(col, (vb[rord[RED]]+.5)/256.,
			(vb[rord[GRN]]+.5)/256., (vb[rord[BLU]]+.5)/256.);
	return(0);
}


getbascii(col, fp)		/* get an ascii brightness value from fp */
COLOR  col;
FILE  *fp;
{
	double	vd;

	if (fscanf(fp, "%lf", &vd) != 1)
		return(-1);
	setcolor(col, vd, vd, vd);
	return(0);
}


getbdouble(col, fp)		/* get a double brightness value from fp */
COLOR  col;
FILE  *fp;
{
	double	vd;

	if (fread((char *)&vd, sizeof(double), 1, fp) != 1)
		return(-1);
	setcolor(col, vd, vd, vd);
	return(0);
}


getbfloat(col, fp)		/* get a float brightness value from fp */
COLOR  col;
FILE  *fp;
{
	float  vf;

	if (fread((char *)&vf, sizeof(float), 1, fp) != 1)
		return(-1);
	setcolor(col, vf, vf, vf);
	return(0);
}


getbint(col, fp)		/* get an int brightness value from fp */
COLOR  col;
FILE  *fp;
{
	int  vi;
	double	d;

	if (fscanf(fp, "%d", &vi) != 1)
		return(-1);
	d = (vi+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


getbbyte(col, fp)		/* get a byte brightness value from fp */
COLOR  col;
FILE  *fp;
{
	BYTE  vb;
	double	d;

	if (fread((char *)&vb, sizeof(BYTE), 1, fp) != 1)
		return(-1);
	d = (vb+.5)/256.;
	setcolor(col, d, d, d);
	return(0);
}


putcascii(col, fp)			/* put an ascii color to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%15.3e %15.3e %15.3e\n",
			colval(col,ord[0]),
			colval(col,ord[1]),
			colval(col,ord[2]));

	return(ferror(fp) ? -1 : 0);
}


putcfloat(col, fp)			/* put a float color to fp */
COLOR  col;
FILE  *fp;
{
	float  vf[3];

	vf[0] = colval(col,ord[0]);
	vf[1] = colval(col,ord[1]);
	vf[2] = colval(col,ord[2]);
	fwrite((char *)vf, sizeof(float), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putcdouble(col, fp)			/* put a double color to fp */
COLOR  col;
FILE  *fp;
{
	double	vd[3];

	vd[0] = colval(col,ord[0]);
	vd[1] = colval(col,ord[1]);
	vd[2] = colval(col,ord[2]);
	fwrite((char *)vd, sizeof(double), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putcint(col, fp)			/* put an int color to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%d %d %d\n",
			(int)(colval(col,ord[0])*256.),
			(int)(colval(col,ord[1])*256.),
			(int)(colval(col,ord[2])*256.));

	return(ferror(fp) ? -1 : 0);
}


putcbyte(col, fp)			/* put a byte color to fp */
COLOR  col;
FILE  *fp;
{
	register int  i;
	BYTE  vb[3];

	i = colval(col,ord[0])*256.;
	vb[0] = min(i,255);
	i = colval(col,ord[1])*256.;
	vb[1] = min(i,255);
	i = colval(col,ord[2])*256.;
	vb[2] = min(i,255);
	fwrite((char *)vb, sizeof(BYTE), 3, fp);

	return(ferror(fp) ? -1 : 0);
}


putbascii(col, fp)			/* put an ascii brightness to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%15.3e\n", bright(col));

	return(ferror(fp) ? -1 : 0);
}


putbfloat(col, fp)			/* put a float brightness to fp */
COLOR  col;
FILE  *fp;
{
	float  vf;

	vf = bright(col);
	fwrite((char *)&vf, sizeof(float), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


putbdouble(col, fp)			/* put a double brightness to fp */
COLOR  col;
FILE  *fp;
{
	double	vd;

	vd = bright(col);
	fwrite((char *)&vd, sizeof(double), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


putbint(col, fp)			/* put an int brightness to fp */
COLOR  col;
FILE  *fp;
{
	fprintf(fp, "%d\n", (int)(bright(col)*256.));

	return(ferror(fp) ? -1 : 0);
}


putbbyte(col, fp)			/* put a byte brightness to fp */
COLOR  col;
FILE  *fp;
{
	register int  i;
	BYTE  vb;

	i = bright(col)*256.;
	vb = min(i,255);
	fwrite((char *)&vb, sizeof(BYTE), 1, fp);

	return(ferror(fp) ? -1 : 0);
}


set_io()			/* set put and get functions */
{
	switch (format) {
	case 'a':					/* ascii */
		if (brightonly) {
			getval = getbascii;
			putval = putbascii;
		} else {
			getval = getcascii;
			putval = putcascii;
		}
		return;
	case 'f':					/* binary float */
		if (brightonly) {
			getval = getbfloat;
			putval = putbfloat;
		} else {
			getval = getcfloat;
			putval = putcfloat;
		}
		return;
	case 'd':					/* binary double */
		if (brightonly) {
			getval = getbdouble;
			putval = putbdouble;
		} else {
			getval = getcdouble;
			putval = putcdouble;
		}
		return;
	case 'i':					/* integer */
		if (brightonly) {
			getval = getbint;
			putval = putbint;
		} else {
			getval = getcint;
			putval = putcint;
		}
		return;
	case 'b':					/* byte */
		if (brightonly) {
			getval = getbbyte;
			putval = putbbyte;
		} else {
			getval = getcbyte;
			putval = putcbyte;
		}
		return;
	}
}
