/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  Combine picture files according to calcomp functions.
 *
 *	1/4/89
 */

#include <stdio.h>

#include <errno.h>

#include "color.h"

#include "calcomp.h"

#define MAXINP		32		/* maximum number of input files */
#define WINSIZ		9		/* scanline window size */
#define MIDSCN		4		/* current scan position */

#define BRT		(-1)		/* special index for brightness */

struct {
	char	*name;		/* file name */
	FILE	*fp;		/* stream pointer */
	COLOR	*scan[WINSIZ];	/* input scanline window */
	COLOR	coef;		/* coefficient */
	COLOR	expos;		/* recorded exposure */
}	input[MAXINP];			/* input pictures */

int	nfiles;				/* number of input files */

char	*vcolin[3] = {"ri", "gi", "bi"};
char	*vcolout[3] = {"ro", "go", "bo"};
char	vbrtin[] = "li";
char	vbrtout[] = "lo";
char	*vcolexp[3] = {"re", "ge", "be"};
char	vbrtexp[] = "le";

char	vnfiles[] = "nfiles";
char	vxmax[] = "xmax";
char	vymax[] = "ymax";
char	vxres[] = "xres";
char	vyres[] = "yres";
char	vxpos[] = "x";
char	vypos[] = "y";

int	nowarn = 0;			/* no warning messages? */

int	xmax = 0, ymax = 0;		/* input resolution */

int	xscan, yscan;			/* input position */

int	xres, yres;			/* output resolution */

int	xpos, ypos;			/* output position */

int	wrongformat = 0;


main(argc, argv)
int	argc;
char	*argv[];
{
	char	buf[128];
	int	original;
	double	f;
	int	a, i;
						/* scan options */
	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'x':
			case 'y':
				a++;
				continue;
			case 'w':
				nowarn = !nowarn;
				continue;
			case 'f':
			case 'e':
				a++;
				continue;
			}
		break;
	}
					/* process files */
	for (nfiles = 0; nfiles < MAXINP; nfiles++) {
		setcolor(input[nfiles].coef, 1.0, 1.0, 1.0);
		setcolor(input[nfiles].expos, 1.0, 1.0, 1.0);
	}
	nfiles = 0;
	original = 0;
	for ( ; a < argc; a++) {
		if (nfiles >= MAXINP) {
			eputs(argv[0]);
			eputs(": too many picture files\n");
			quit(1);
		}
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case '\0':
				input[nfiles].name = "<stdin>";
				input[nfiles].fp = stdin;
				break;
			case 'o':
				original++;
				break;
			case 's':
				f = atof(argv[++a]);
				scalecolor(input[nfiles].coef, f);
				continue;
			case 'c':
				colval(input[nfiles].coef,RED)*=atof(argv[++a]);
				colval(input[nfiles].coef,GRN)*=atof(argv[++a]);
				colval(input[nfiles].coef,BLU)*=atof(argv[++a]);
				continue;
			default:
				goto usage;
			}
		else {
			input[nfiles].name = argv[a];
			input[nfiles].fp = fopen(argv[a], "r");
			if (input[nfiles].fp == NULL) {
				perror(argv[a]);
				quit(1);
			}
		}
		checkfile();
		if (original) {
			colval(input[nfiles].coef,RED) /=
					colval(input[nfiles].expos,RED);
			colval(input[nfiles].coef,GRN) /=
					colval(input[nfiles].expos,GRN);
			colval(input[nfiles].coef,BLU) /=
					colval(input[nfiles].expos,BLU);
		}
		nfiles++;
		original = 0;
	}
	init();				/* set constants */
					/* go back and get expressions */
	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-')
			switch (argv[a][1]) {
			case 'x':
				strcpy(buf, vxres);
				strcat(buf, ":");
				strcat(buf, argv[++a]);
				scompile(buf, NULL, 0);
				continue;
			case 'y':
				strcpy(buf, vyres);
				strcat(buf, ":");
				strcat(buf, argv[++a]);
				scompile(buf, NULL, 0);
				continue;
			case 'w':
				continue;
			case 'f':
				fcompile(argv[++a]);
				continue;
			case 'e':
				scompile(argv[++a], NULL, 0);
				continue;
			}
		break;
	}
						/* set/get output resolution */
	if (!vardefined(vxres))
		varset(vxres, ':', (double)xmax);
	if (!vardefined(vyres))
		varset(vyres, ':', (double)ymax);
	xres = varvalue(vxres) + .5;
	yres = varvalue(vyres) + .5;
	if (xres <= 0 || yres <= 0) {
		eputs(argv[0]);
		eputs(": illegal output resolution\n");
		quit(1);
	}
						/* complete header */
	printargs(argc, argv, stdout);
	fputformat(COLRFMT, stdout);
	putchar('\n');
	fputresolu(YMAJOR|YDECR, xres, yres, stdout);
						/* combine pictures */
	combine();
	quit(0);
usage:
	eputs("Usage: ");
	eputs(argv[0]);
	eputs(
" [-w][-h][-x xr][-y yr][-e expr][-f file] [ [-s f][-c r g b] pic ..]\n");
	quit(1);
}


tputs(s)			/* put out string preceded by a tab */
char	*s;
{
	char	fmt[32];
	double	d;
	COLOR	ctmp;

	if (isformat(s)) {			/* check format */
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
		return;		/* don't echo */
	}
	if (isexpos(s)) {			/* exposure */
		d = exposval(s);
		scalecolor(input[nfiles].expos, d);
	} else if (iscolcor(s)) {		/* color correction */
		colcorval(ctmp, s);
		multcolor(input[nfiles].expos, ctmp);
	}
						/* echo line */
	putchar('\t');
	fputs(s, stdout);
}


checkfile()			/* ready a file */
{
	int	xinp, yinp;
	register int	i;
					/* process header */
	fputs(input[nfiles].name, stdout);
	fputs(":\n", stdout);
	getheader(input[nfiles].fp, tputs, NULL);
	if (wrongformat) {
		eputs(input[nfiles].name);
		eputs(": not in Radiance picture format\n");
		quit(1);
	}
	if (fgetresolu(&xinp, &yinp, input[nfiles].fp) != (YMAJOR|YDECR)) {
		eputs(input[nfiles].name);
		eputs(": bad picture size\n");
		quit(1);
	}
	if (xmax == 0 && ymax == 0) {
		xmax = xinp;
		ymax = yinp;
	} else if (xinp != xmax || yinp != ymax) {
		eputs(input[nfiles].name);
		eputs(": resolution mismatch\n");
		quit(1);
	}
					/* allocate scanlines */
	for (i = 0; i < WINSIZ; i++)
		input[nfiles].scan[i] = (COLOR *)emalloc(xmax*sizeof(COLOR));
}


init()					/* perform final setup */
{
	double	l_colin(), l_expos();
	register int	i;
						/* define constants */
	varset(vnfiles, ':', (double)nfiles);
	varset(vxmax, ':', (double)xmax);
	varset(vymax, ':', (double)ymax);
						/* set functions */
	for (i = 0; i < 3; i++) {
		funset(vcolexp[i], 1, ':', l_expos);
		funset(vcolin[i], 1, '=', l_colin);
	}
	funset(vbrtexp, 1, ':', l_expos);
	funset(vbrtin, 1, '=', l_colin);
}


combine()			/* combine pictures */
{
	EPNODE	*coldef[3], *brtdef;
	COLOR	*scanout;
	double	d;
	register int	i, j;
						/* check defined variables */
	for (j = 0; j < 3; j++) {
		if (vardefined(vcolout[j]))
			coldef[j] = eparse(vcolout[j]);
		else
			coldef[j] = NULL;
	}
	if (vardefined(vbrtout))
		brtdef = eparse(vbrtout);
	else
		brtdef = NULL;
						/* allocate scanline */
	scanout = (COLOR *)emalloc(xres*sizeof(COLOR));
						/* set input position */
	yscan = ymax+MIDSCN;
						/* combine files */
	for (ypos = yres-1; ypos >= 0; ypos--) {
	    advance();
	    varset(vypos, '=', (double)ypos);
	    for (xpos = 0; xpos < xres; xpos++) {
		xscan = (long)xpos*xmax/xres;
		varset(vxpos, '=', (double)xpos);
		eclock++;
		if (brtdef != NULL) {
		    d = evalue(brtdef);
		    if (d < 0.0)
			d = 0.0;
		    setcolor(scanout[xpos], d, d, d);
		} else {
		    for (j = 0; j < 3; j++) {
			if (coldef[j] != NULL) {
				d = evalue(coldef[j]);
			} else {
			    d = 0.0;
			    for (i = 0; i < nfiles; i++)
				d += colval(input[i].scan[MIDSCN][xscan],j);
			}
			if (d < 0.0)
			    d = 0.0;
			colval(scanout[xpos],j) = d;
		    }
		}
	    }
	    if (fwritescan(scanout, xres, stdout) < 0) {
		    perror("write error");
		    quit(1);
	    }
	}
	efree(scanout);
}


advance()			/* read in data for next scanline */
{
	int	ytarget;
	register COLOR	*st;
	register int	i, j;

	for (ytarget = (long)ypos*ymax/yres; yscan > ytarget; yscan--)
		for (i = 0; i < nfiles; i++) {
			st = input[i].scan[WINSIZ-1];
			for (j = WINSIZ-1; j > 0; j--)	/* rotate window */
				input[i].scan[j] = input[i].scan[j-1];
			input[i].scan[0] = st;
			if (yscan <= MIDSCN)		/* hit bottom? */
				continue;
			if (freadscan(st, xmax, input[i].fp) < 0) {	/* read */
				eputs(input[i].name);
				eputs(": read error\n");
				quit(1);
			}
			for (j = 0; j < xmax; j++)	/* adjust color */
				multcolor(st[j], input[i].coef);
		}
}


double
l_expos(nam)			/* return picture exposure */
register char	*nam;
{
	register int	fn, n;

	fn = argument(1) - .5;
	if (fn < 0 || fn >= nfiles)
		return(1.0);
	if (nam == vbrtexp)
		return(bright(input[fn].expos));
	n = 3;
	while (n--)
		if (nam == vcolexp[n])
			return(colval(input[fn].expos,n));
	eputs("Bad call to l_expos()!\n");
	quit(1);
}


double
l_colin(nam)			/* return color value for picture */
register char	*nam;
{
	int	fn;
	register int	n, xoff, yoff;
	double	d;

	d = argument(1);
	if (d > -.5 && d < .5)
		return((double)nfiles);
	fn = d - .5;
	if (fn < 0 || fn >= nfiles) {
		errno = EDOM;
		return(0.0);
	}
	xoff = yoff = 0;
	n = nargum();
	if (n >= 2) {
		d = argument(2);
		if (d < 0.0) {
			xoff = d-.5;
			if (xscan+xoff < 0)
				xoff = -xscan;
		} else {
			xoff = d+.5;
			if (xscan+xoff >= xmax)
				xoff = xmax-1-xscan;
		}
	}
	if (n >= 3) {
		d = argument(3);
		if (d < 0.0) {
			yoff = d-.5;
			if (yoff+MIDSCN < 0)
				yoff = -MIDSCN;
			if (yscan+yoff < 0)
				yoff = -yscan;
		} else {
			yoff = d+.5;
			if (yoff+MIDSCN >= WINSIZ)
				yoff = WINSIZ-1-MIDSCN;
			if (yscan+yoff >= ymax)
				yoff = ymax-1-yscan;
		}
	}
	if (nam == vbrtin)
		return(bright(input[fn].scan[MIDSCN+yoff][xscan+xoff]));
	n = 3;
	while (n--)
	    if (nam == vcolin[n])
		return(colval(input[fn].scan[MIDSCN+yoff][xscan+xoff],n));
	eputs("Bad call to l_colin()!\n");
	quit(1);
}


wputs(msg)
char	*msg;
{
	if (!nowarn)
		eputs(msg);
}


eputs(msg)
char	*msg;
{
	fputs(msg, stderr);
}


quit(code)
int	code;
{
	exit(code);
}
