/* RCSid $Id: normcodec.h,v 2.1 2019/07/26 16:18:06 greg Exp $ */
/*
 * Definitions and declarations for 32-bit vector normal encode/decode
 *
 *  Include after stdio.h and fvect.h
 *  Includes resolu.h
 */

#ifndef _RAD_NORMALCODEC_H_
#define _RAD_NORMALCODEC_H_

#include "resolu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NORMAL32FMT	"32-bit_encoded_normal"

#define	HF_HEADIN	0x1
#define HF_HEADOUT	0x2
#define HF_RESIN	0x4
#define HF_RESOUT	0x8
#define HF_ALL		0xf
#define HF_ENCODE	0x10

/* Structure for encoding/decoding normals */
typedef struct {
	FILE		*finp;			/* input stream */
	const char	*inpname;		/* input name */
	int		format;			/* decoded format */
	long		dstart;			/* start of data */
	long		curpos;			/* current input position */
	int		hdrflags;		/* header i/o flags */
	char		inpfmt[MAXFMTLEN];	/* format from header */
	RESOLU		res;			/* input resolution */
} NORMCODEC;

/* Set codec defaults */
extern void	set_nc_defaults(NORMCODEC *ncp);

/* Load/copy header */
extern int	process_nc_header(NORMCODEC *ncp, int ac, char *av[]);

/* Check that we have what we need to decode normals */
extern int	check_decode_normals(NORMCODEC *ncp);

/* Decode next normal from input */
extern int	decode_normal_next(FVECT nrm, NORMCODEC *ncp);

/* Seek to the indicated pixel position */
extern int	seek_nc_pix(NORMCODEC *ncp, int x, int y);

/* Read and decode normal for the given pixel */
extern int	decode_normal_pix(FVECT nrm, NORMCODEC *ncp, int x, int y);

extern char	*progname;	/* global argv[0] (set by main) */

#ifdef __cplusplus
}
#endif
#endif		/* _RAD_NORMALCODEC_H_ */
