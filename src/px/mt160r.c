/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  mt160r.c - program to dump pixel file to Mannesman-Tally 160.
 *
 *     8/16/85
 */

#include  <stdio.h>

#include  "color.h"

#define  NCOLS		880		/* for wide carriage */


main(argc, argv)
int  argc;
char  *argv[];
{
	int  i;
	int  status = 0;
	
	if (argc < 2)
		status += printp(NULL) == -1;
	else
		for (i = 1; i < argc; i++)
			status += printp(argv[i]) == -1;
	exit(status);
}


printp(fname)				/* print a picture */
char  *fname;
{
	FILE  *input;
	int  xres, yres;
	COLR  scanline[NCOLS];
	int  i;

	if (fname == NULL) {
		input = stdin;
		fname = "<stdin>";
	} else if ((input = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fname);
		return(-1);
	}
				/* discard header */
	if (checkheader(input, COLRFMT, NULL) < 0) {
		fprintf(stderr, "%s: not a Radiance picture\n", fname);
		return(-1);
	}
				/* get picture dimensions */
	if (fgetresolu(&xres, &yres, input) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: bad picture size\n", fname);
		return(-1);
	}
	if (xres > NCOLS) {
		fprintf(stderr, "%s: resolution mismatch\n", fname);
		return(-1);
	}

	fputs("\033[6~\033[7z", stdout);
	
	for (i = yres-1; i >= 0; i--) {
		if (freadcolrs(scanline, xres, input) < 0) {
			fprintf(stderr, "%s: read error (y=%d)\n", fname, i);
			return(-1);
		}
		normcolrs(scanline, xres, 0);
		plotscan(scanline, xres, i);
	}

	fputs("\f\033[6~", stdout);

	fclose(input);

	return(0);
}


plotscan(scan, len, y)			/* plot a scanline */
COLR  scan[];
int  len;
int  y;
{
	static BYTE  pat[NCOLS];
	int  bpos;
	register int  i;

	if (bpos = y & 7) {

		for (i = 0; i < len; i++)
			pat[i] |= bit(scan[i], i) << bpos;

	} else {

		fputs("\033%5", stdout);
		putchar(len & 255);
		putchar(len >> 8);

		for (i = 0; i < len; i++) {
			pat[i] |= bit(scan[i], i);
			putchar(pat[i]);
			pat[i] = 0;
		}
		putchar('\r');
		putchar('\n');
	}
}


bit(clr, x)				/* return bit for color at x */
COLR  clr;
register int  x;
{
	static int  cerr[NCOLS];
	static int  err;
	int  b;
	register int  isblack;

	b = normbright(clr);
	err += b + cerr[x];
	isblack = err < 128;
	if (!isblack) err -= 256;
	cerr[x] = err /= 2;
	return(isblack);
}
