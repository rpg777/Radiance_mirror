/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif
/*
 * prot.c - program to rotate picture file 90 degrees clockwise.
 *
 *	2/26/88
 */

#include "standard.h"

#include "color.h"

int	xres, yres;			/* input resolution */
double	inpaspect = 1.0;		/* input aspect ratio */

char	buf[1<<20];			/* output buffer */

int	nrows;				/* number of rows output at once */

int	wrongformat = 0;

#define scanbar		((COLR *)buf)

char	*progname;


headline(s)				/* process line from header */
char	*s;
{
	char	fmt[32];

	fputs(s, stdout);
	if (isaspect(s))
		inpaspect *= aspectval(s);
	else if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	}
}


main(argc, argv)
int	argc;
char	*argv[];
{
	FILE	*fin;

	progname = argv[0];

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s infile [outfile]\n", progname);
		exit(1);
	}
	if ((fin = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[1]);
		exit(1);
	}
	if (argc == 3 && freopen(argv[2], "w", stdout) == NULL) {
		fprintf(stderr, "%s: cannot open\n", argv[2]);
		exit(1);
	}
					/* transfer header */
	getheader(fin, headline, NULL);
	if (wrongformat) {
		fprintf(stderr, "%s: wrong picture format\n", progname);
		exit(1);
	}
					/* add new header info. */
	if (inpaspect < .99 || inpaspect > 1.01)
		fputaspect(1./inpaspect/inpaspect, stdout);
	printf("%s\n\n", progname);
					/* get picture size */
	if (fgetresolu(&xres, &yres, fin) != (YMAJOR|YDECR)) {
		fprintf(stderr, "%s: bad picture size\n", progname);
		exit(1);
	}
					/* write new picture size */
	fputresolu(YMAJOR|YDECR, yres, xres, stdout);
					/* compute buffer capacity */
	nrows = sizeof(buf)/sizeof(COLR)/yres;
	rotate(fin);			/* rotate the image */
	exit(0);
}


rotate(fp)			/* rotate picture */
FILE	*fp;
{
	register COLR	*inln;
	register int	xoff, inx, iny;
	long	start, ftell();

	if ((inln = (COLR *)malloc(xres*sizeof(COLR))) == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	start = ftell(fp);
	for (xoff = 0; xoff < xres; xoff += nrows) {
		if (fseek(fp, start, 0) < 0) {
			fprintf(stderr, "%s: seek error\n", progname);
			exit(1);
		}
		for (iny = yres-1; iny >= 0; iny--) {
			if (freadcolrs(inln, xres, fp) < 0) {
				fprintf(stderr, "%s: read error\n", progname);
				exit(1);
			}
			for (inx = 0; inx < nrows && xoff+inx < xres; inx++)
				bcopy((char *)inln[xoff+inx],
						(char *)scanbar[inx*yres+iny],
						sizeof(COLR));
		}
		for (inx = 0; inx < nrows && xoff+inx < xres; inx++)
			if (fwritecolrs(scanbar+inx*yres, yres, stdout) < 0) {
				fprintf(stderr, "%s: write error\n", progname);
				exit(1);
			}
	}
	free((char *)inln);
}
