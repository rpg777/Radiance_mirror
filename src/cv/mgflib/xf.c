/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for 4x4 homogeneous, rigid-body transformations
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "parser.h"

#define  d2r(a)		((PI/180.)*(a))

#define  checkarg(a,l)	if (av[i][a] || badarg(ac-i-1,av+i+1,l)) goto done

MAT4  m4ident = MAT4IDENT;

static MAT4  m4tmp;		/* for efficiency */

int	xf_argc;			/* total # transform args. */
char	**xf_argv;			/* transform arguments */
XF_SPEC	*xf_context;			/* current context */

static int	xf_maxarg;		/* # allocated arguments */

static XF_SPEC	*new_xf();
static long	comp_xfid();
static int	put_oname();


int
xf_handler(ac, av)		/* handle xf entity */
int	ac;
char	**av;
{
	register XF_SPEC	*spec;
	register int	n;
	int	rv;
	XF	thisxf;

	if (ac == 1) {			/* something with existing transform */
		if ((spec = xf_context) == NULL)
			return(MG_OK);		/* should be error? */
		n = -1;
		if (spec->xarr != NULL) {	/* check for iteration */
			register struct xf_array	*ap = spec->xarr;

			(void)put_oname((struct xf_array *)NULL);
			n = ap->ndim;
			while (n--) {
				if (++ap->aarg[n].i < ap->aarg[n].n)
					break;
				(void)strcpy(ap->aarg[n].arg, "0");
				ap->aarg[n].i = 0;
			}
			if (n >= 0) {
				if ((rv = mg_fgoto(&ap->spos)) != MG_OK)
					return(rv);
				sprintf(ap->aarg[n].arg, "%d", ap->aarg[n].i);
				(void)put_oname(ap);
			} else
				free((MEM_PTR)ap);
		}
		if (n < 0) {			/* pop transform */
			xf_argv[xf_argc=spec->xav0] = NULL;
			xf_context = spec->prev;
			free((MEM_PTR)spec);
			return(MG_OK);
		}
	} else {			/* else allocate transform */
		if ((spec = new_xf(ac-1, av+1)) == NULL)
			return(MG_EMEM);
		spec->prev = xf_context;	/* push onto stack */
		xf_context = spec;
	}
					/* translate new specification */
	if (xf(&thisxf, spec->xac, &xf_argv[spec->xav0]) != spec->xac)
		return(MG_ETYPE);
					/* compute total transformation */
	if (spec->prev != NULL) {
		multmat4(spec->xf.xfm, thisxf.xfm, spec->prev->xf.xfm);
		spec->xf.sca = thisxf.sca * spec->prev->xf.sca;
	} else
		spec->xf = thisxf;
	spec->xid = comp_xfid(spec->xf.xfm);	/* compute unique ID */
	return(MG_OK);
}


static XF_SPEC *
new_xf(ac, av)			/* allocate new transform structure */
int	ac;
char	**av;
{
	register XF_SPEC	*spec;
	register int	i;
	char	*cp;
	int	n, ndim;

	ndim = 0;
	n = 0;				/* compute space req'd by arguments */
	for (i = 0; i < ac; i++)
		if (!strcmp(av[i], "-a")) {
			ndim++;
			i++;
		} else
			n += strlen(av[i]) + 1;
	if (ndim > XF_MAXDIM)
		return(NULL);
	spec = (XF_SPEC *)malloc(sizeof(XF_SPEC) + n);
	if (spec == NULL)
		return(NULL);
	if (ndim) {
		spec->xarr = (struct xf_array *)malloc(sizeof(struct xf_array));
		if (spec->xarr == NULL)
			return(NULL);
		mg_fgetpos(&spec->xarr->spos);
		spec->xarr->ndim = 0;		/* incremented below */
	} else
		spec->xarr = NULL;
	spec->xav0 = xf_argc;
	spec->xac = ac;
					/* and store new xf arguments */
	if (xf_argc+ac+1 > xf_maxarg) {
		if (!xf_maxarg)
			xf_argv = (char **)malloc(
					(xf_maxarg=ac+1)*sizeof(char *));
		else
			xf_argv = (char **)realloc((MEM_PTR)xf_argv,
				(xf_maxarg=xf_argc+ac+1)*sizeof(char *));
		if (xf_argv == NULL)
			return(NULL);
	}
	cp = (char *)(spec + 1);	/* use memory allocated above */
	for (i = 0; i < ac; i++)
		if (!strcmp(av[i], "-a")) {
			xf_argv[xf_argc++] = "-i";
			xf_argv[xf_argc++] = strcpy(
					spec->xarr->aarg[spec->xarr->ndim].arg,
					"0");
			spec->xarr->aarg[spec->xarr->ndim].i = 0;
			spec->xarr->aarg[spec->xarr->ndim++].n = atoi(av[++i]);
		} else {
			xf_argv[xf_argc++] = strcpy(cp, av[i]);
			cp += strlen(av[i]) + 1;
		}
	xf_argv[xf_argc] = NULL;
	if (spec->xarr != NULL)
		(void)put_oname(spec->xarr);
	return(spec);
}


static int
put_oname(ap)			/* put out name for this instance */
register struct xf_array	*ap;
{
	static char	oname[10*XF_MAXDIM];
	static char	*oav[3] = {mg_ename[MG_E_OBJECT], oname};
	register int	i;
	register char	*cp1, *cp2;

	if (ap == NULL)
		return(mg_handle(MG_E_OBJECT, 1, oav));
	cp1 = oname;
	*cp1 = 'a';
	for (i = 0; i < ap->ndim; i++) {
		for (cp2 = ap->aarg[i].arg; *cp2; )
			*++cp1 = *cp2++;
		*++cp1 = '.';
	}
	*cp1 = '\0';
	return(mg_handle(MG_E_OBJECT, 2, oav));
}


static long
comp_xfid(xfm)			/* compute unique ID from matrix */
register MAT4	xfm;
{
	static char	shifttab[64] = { 15, 5, 11, 5, 6, 3,
				9, 15, 13, 2, 13, 5, 2, 12, 14, 11,
				11, 12, 12, 3, 2, 11, 8, 12, 1, 12,
				5, 4, 15, 9, 14, 5, 13, 14, 2, 10,
				10, 14, 12, 3, 5, 5, 14, 6, 12, 11,
				13, 9, 12, 8, 1, 6, 5, 12, 7, 13,
				15, 8, 9, 2, 6, 11, 9, 11 };
	register int	i;
	register long	xid;

	xid = 0;			/* compute unique transform id */
	for (i = 0; i < sizeof(MAT4)/sizeof(unsigned short); i++)
		xid ^= (long)(((unsigned short *)xfm)[i]) << shifttab[i&63];
	return(xid);
}


void
xf_clear()			/* clear transform stack */
{
	register XF_SPEC	*spec;

	if (xf_maxarg) {
		free((MEM_PTR)xf_argv);
		xf_argv = NULL;
		xf_maxarg = 0;
	}
	xf_argc = 0;
	while ((spec = xf_context) != NULL) {
		xf_context = spec->prev;
		if (spec->xarr != NULL)
			free((MEM_PTR)spec->xarr);
		free((MEM_PTR)spec);
	}
}


void
xf_xfmpoint(v1, v2)		/* transform a point by the current matrix */
FVECT	v1, v2;
{
	if (xf_context == NULL) {
		v1[0] = v2[0];
		v1[1] = v2[1];
		v1[2] = v2[2];
		return;
	}
	multp3(v1, v2, xf_context->xf.xfm);
}


void
xf_xfmvect(v1, v2)		/* transform a vector using current matrix */
FVECT	v1, v2;
{
	if (xf_context == NULL) {
		v1[0] = v2[0];
		v1[1] = v2[1];
		v1[2] = v2[2];
		return;
	}
	multv3(v1, v2, xf_context->xf.xfm);
}


void
xf_rotvect(v1, v2)		/* rotate a vector using current matrix */
FVECT	v1, v2;
{
	xf_xfmvect(v1, v2);
	if (xf_context == NULL)
		return;
	v1[0] /= xf_context->xf.sca;
	v1[1] /= xf_context->xf.sca;
	v1[2] /= xf_context->xf.sca;
}


double
xf_scale(d)			/* scale a number by the current transform */
double	d;
{
	if (xf_context == NULL)
		return(d);
	return(d*xf_context->xf.sca);
}


void
multmat4(m4a, m4b, m4c)		/* multiply m4b X m4c and put into m4a */
MAT4  m4a;
register MAT4  m4b, m4c;
{
	register int  i, j;
	
	for (i = 4; i--; )
		for (j = 4; j--; )
			m4tmp[i][j] = m4b[i][0]*m4c[0][j] +
				      m4b[i][1]*m4c[1][j] +
				      m4b[i][2]*m4c[2][j] +
				      m4b[i][3]*m4c[3][j];
	
	copymat4(m4a, m4tmp);
}


void
multv3(v3a, v3b, m4)	/* transform vector v3b by m4 and put into v3a */
FVECT  v3a;
register FVECT  v3b;
register MAT4  m4;
{
	m4tmp[0][0] = v3b[0]*m4[0][0] + v3b[1]*m4[1][0] + v3b[2]*m4[2][0];
	m4tmp[0][1] = v3b[0]*m4[0][1] + v3b[1]*m4[1][1] + v3b[2]*m4[2][1];
	m4tmp[0][2] = v3b[0]*m4[0][2] + v3b[1]*m4[1][2] + v3b[2]*m4[2][2];
	
	v3a[0] = m4tmp[0][0];
	v3a[1] = m4tmp[0][1];
	v3a[2] = m4tmp[0][2];
}


void
multp3(p3a, p3b, m4)		/* transform p3b by m4 and put into p3a */
register FVECT  p3a;
FVECT  p3b;
register MAT4  m4;
{
	multv3(p3a, p3b, m4);	/* transform as vector */
	p3a[0] += m4[3][0];	/* translate */
	p3a[1] += m4[3][1];
	p3a[2] += m4[3][2];
}


int
xf(ret, ac, av)			/* get transform specification */
register XF  *ret;
int  ac;
char  **av;
{
	MAT4  xfmat, m4;
	double  xfsca, dtmp;
	int  i, icnt;

	setident4(ret->xfm);
	ret->sca = 1.0;

	icnt = 1;
	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg(2,"fff");
			m4[3][0] = atof(av[++i]);
			m4[3][1] = atof(av[++i]);
			m4[3][2] = atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[1][1] = m4[2][2] = cos(dtmp);
				m4[2][1] = -(m4[1][2] = sin(dtmp));
				break;
			case 'y':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[2][2] = cos(dtmp);
				m4[0][2] = -(m4[2][0] = sin(dtmp));
				break;
			case 'z':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[1][1] = cos(dtmp);
				m4[1][0] = -(m4[0][1] = sin(dtmp));
				break;
			default:
				goto done;
			}
			break;

		case 's':			/* scale */
			checkarg(2,"f");
			dtmp = atof(av[i+1]);
			if (dtmp == 0.0) goto done;
			i++;
			xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = dtmp;
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,"");
				xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg(3,"");
				xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg(3,"");
				xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				goto done;
			}
			break;

		case 'i':			/* iterate */
			checkarg(2,"i");
			while (icnt-- > 0) {
				multmat4(ret->xfm, ret->xfm, xfmat);
				ret->sca *= xfsca;
			}
			icnt = atoi(av[++i]);
			setident4(xfmat);
			xfsca = 1.0;
			continue;

		default:
			goto done;

		}
		multmat4(xfmat, xfmat, m4);
	}
done:
	while (icnt-- > 0) {
		multmat4(ret->xfm, ret->xfm, xfmat);
		ret->sca *= xfsca;
	}
	return(i);
}
