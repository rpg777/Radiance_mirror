/* RCSid $Id: resolu.h,v 2.4 2003/02/25 02:47:22 greg Exp $ */
/*
 * Definitions for resolution line in image file.
 *
 * Include after <stdio.h>, <string.h>, and <time.h>
 *
 * True image orientation is defined by an xy coordinate system
 * whose origin is at the lower left corner of the image, with
 * x increasing to the right and y increasing in the upward direction.
 * This true orientation is independent of how the pixels are actually
 * ordered in the file, which is indicated by the resolution line.
 * This line is of the form "{+-}{XY} xyres {+-}{YX} yxres\n".
 * A typical line for a 1024x600 image might be "-Y 600 +X 1024\n",
 * indicating that the scanlines are in English text order (PIXSTANDARD).
 */

#include "copyright.h"

#ifdef __cplusplus
extern "C" {
#endif

			/* flags for scanline ordering */
#define  XDECR			1
#define  YDECR			2
#define  YMAJOR			4

			/* standard scanline ordering */
#define  PIXSTANDARD		(YMAJOR|YDECR)
#define  PIXSTDFMT		"-Y %d +X %d\n"

			/* structure for image dimensions */
typedef struct {
	int	rt;		/* orientation (from flags above) */
	int	xr, yr;		/* x and y resolution */
} RESOLU;

			/* macros to get scanline length and number */
#define  scanlen(rs)		((rs)->rt & YMAJOR ? (rs)->xr : (rs)->yr)
#define  numscans(rs)		((rs)->rt & YMAJOR ? (rs)->yr : (rs)->xr)

			/* resolution string buffer and its size */
#define  RESOLU_BUFLEN		32
extern char  resolu_buf[RESOLU_BUFLEN];

			/* macros for reading/writing resolution struct */
#define  fputsresolu(rs,fp)	fputs(resolu2str(resolu_buf,rs),fp)
#define  fgetsresolu(rs,fp)	str2resolu(rs, \
					fgets(resolu_buf,RESOLU_BUFLEN,fp))

			/* reading/writing of standard ordering */
#define  fprtresolu(sl,ns,fp)	fprintf(fp,PIXSTDFMT,ns,sl)
#define  fscnresolu(sl,ns,fp)	(fscanf(fp,PIXSTDFMT,ns,sl)==2)

#ifdef NOPROTO
					/* defined in resolu.c */
extern void	fputresolu();
extern int	fgetresolu();
extern char	*resolu2str();
extern int	str2resolu();
					/* defined in header.c */
extern void	newheader();
extern int	isheadid();
extern int	headidval();
extern int	dateval();
extern int	isdate();
extern void	fputdate();
extern void	fputnow();
extern void	printargs();
extern int	isformat();
extern int	formatval();
extern void	fputformat();
extern int	getheader();
extern int	globmatch();
extern int	checkheader();

#else
					/* defined in resolu.c */
extern void	fputresolu(int ord, int sl, int ns, FILE *fp);
extern int	fgetresolu(int *sl, int *ns, FILE *fp);
extern char *	resolu2str(char *buf, RESOLU *rp);
extern int	str2resolu(RESOLU *rp, char *buf);
					/* defined in header.c */
extern void	newheader(char *t, FILE *fp);
extern int	isheadid(char *s);
extern int	headidval(char *r, char *s);
extern int	dateval(time_t *t, char *s);
extern int	isdate(char *s);
extern void	fputdate(time_t t, FILE *fp);
extern void	fputnow(FILE *fp);
extern void	printargs(int ac, char **av, FILE *fp);
extern int	isformat(char *s);
extern int	formatval(char *r, char *s);
extern void	fputformat(char *s, FILE *fp);
extern int	getheader(FILE *fp, int (*f)(), char *p);
extern int	globmatch(char *pat, char *str);
extern int	checkheader(FILE *fin, char *fmt, FILE *fout);

#endif

#ifdef __cplusplus
}
#endif
