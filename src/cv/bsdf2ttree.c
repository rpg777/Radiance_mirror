#ifndef lint
static const char RCSid[] = "$Id: bsdf2ttree.c,v 2.29 2014/08/21 10:33:48 greg Exp $";
#endif
/*
 * Load measured BSDF interpolant and write out as XML file with tensor tree.
 *
 *	G. Ward
 */

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "random.h"
#include "platform.h"
#include "rtprocess.h"
#include "calcomp.h"
#include "bsdfrep.h"
				/* global argv[0] */
char			*progname;
				/* percentage to cull (<0 to turn off) */
double			pctcull = 90.;
				/* sampling order */
int			samp_order = 6;
				/* super-sampling threshold */
const double		ssamp_thresh = 0.35;
				/* number of super-samples */
#ifndef NSSAMP
#define	NSSAMP		100
#endif
				/* limit on number of RBF lobes */
static int		lobe_lim = 15000;
				/* progress bar length */
static int		do_prog = 79;


/* Start new progress bar */
#define prog_start(s)	if (do_prog) fprintf(stderr, "%s: %s...\n", progname, s); else

/* Draw progress bar of the appropriate length */
static void
prog_show(double frac)
{
	char	pbar[256];
	int	nchars;

	if (do_prog <= 0) return;
	if (do_prog > sizeof(pbar)-2)
		do_prog = sizeof(pbar)-2;
	if (frac < 0) frac = 0;
	else if (frac > 1) frac = 1;
	nchars = do_prog*frac + .5;
	pbar[0] = '\r';
	memset(pbar+1, '*', nchars);
	memset(pbar+1+nchars, '-', do_prog-nchars);
	pbar[do_prog+1] = '\0';
	fputs(pbar, stderr);
}

/* Finish progress bar */
static void
prog_done(void)
{
	int	n = do_prog;

	if (n <= 1) return;
	fputc('\r', stderr);
	while (n--)
		fputc(' ', stderr);
	fputc('\r', stderr);
}

/* Output XML prologue to stdout */
static void
xml_prologue(int ac, char *av[])
{
	puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
	puts("<WindowElement xmlns=\"http://windows.lbl.gov\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://windows.lbl.gov/BSDF-v1.4.xsd\">");
	fputs("<!-- File produced by:", stdout);
	while (ac-- > 0) {
		fputc(' ', stdout);
		fputs(*av++, stdout);
	}
	puts(" -->");
	puts("<WindowElementType>System</WindowElementType>");
	puts("<FileType>BSDF</FileType>");
	puts("<Optical>");
	puts("<Layer>");
	puts("\t<Material>");
	printf("\t\t<Name>%s</Name>\n", bsdf_name[0] ? bsdf_name : "Unknown");
	printf("\t\t<Manufacturer>%s</Manufacturer>\n",
			bsdf_manuf[0] ? bsdf_manuf : "Unknown");
	puts("\t\t<DeviceType>Other</DeviceType>");
	puts("\t</Material>");
	puts("\t<DataDefinition>");
	printf("\t\t<IncidentDataStructure>TensorTree%c</IncidentDataStructure>\n",
			single_plane_incident ? '3' : '4');
	puts("\t</DataDefinition>");
}

/* Output XML data prologue to stdout */
static void
data_prologue()
{
	static const char	*bsdf_type[4] = {
					"Reflection Front",
					"Transmission Front",
					"Transmission Back",
					"Reflection Back"
				};

	puts("\t<WavelengthData>");
	puts("\t\t<LayerNumber>System</LayerNumber>");
	puts("\t\t<Wavelength unit=\"Integral\">Visible</Wavelength>");
	puts("\t\t<SourceSpectrum>CIE Illuminant D65 1nm.ssp</SourceSpectrum>");
	puts("\t\t<DetectorSpectrum>ASTM E308 1931 Y.dsp</DetectorSpectrum>");
	puts("\t\t<WavelengthDataBlock>");
	printf("\t\t\t<WavelengthDataDirection>%s</WavelengthDataDirection>\n",
			bsdf_type[(input_orient>0)<<1 | (output_orient>0)]);
	puts("\t\t\t<AngleBasis>LBNL/Shirley-Chiu</AngleBasis>");
	puts("\t\t\t<ScatteringDataType>BTDF</ScatteringDataType>");
	puts("\t\t\t<ScatteringData>");
}

/* Output XML data epilogue to stdout */
static void
data_epilogue(void)
{
	puts("\t\t\t</ScatteringData>");
	puts("\t\t</WavelengthDataBlock>");
	puts("\t</WavelengthData>");
}

/* Output XML epilogue to stdout */
static void
xml_epilogue(void)
{
	puts("</Layer>");
	puts("</Optical>");
	puts("</WindowElement>");
}

/* Compute absolute relative difference */
static double
abs_diff(double v1, double v0)
{
	if ((v0 < 0) | (v1 < 0))
		return(.0);
	v1 = (v1-v0)*2./(v0+v1+.0001);
	if (v1 < 0)
		return(-v1);
	return(v1);
}

/* Interpolate and output isotropic BSDF data */
static void
eval_isotropic(char *funame)
{
	const int	sqres = 1<<samp_order;
	FILE		*ofp = NULL;
	int		assignD = 0;
	char		cmd[128];
	int		ix, ox, oy;
	double		iovec[6];
	float		bsdf;

	data_prologue();			/* begin output */
	if (pctcull >= 0) {
		sprintf(cmd, "rttree_reduce -a -h -ff -r 3 -t %f -g %d",
				pctcull, samp_order);
		fflush(stdout);
		ofp = popen(cmd, "w");
		if (ofp == NULL) {
			fprintf(stderr, "%s: cannot create pipe to rttree_reduce\n",
					progname);
			exit(1);
		}
		SET_FILE_BINARY(ofp);
#ifdef getc_unlocked				/* avoid lock/unlock overhead */
		flockfile(ofp);
#endif
	} else
		fputs("{\n", stdout);
						/* need to assign Dx, Dy, Dz? */
	if (funame != NULL)
		assignD = (fundefined(funame) < 6);
						/* run through directions */
	for (ix = 0; ix < sqres/2; ix++) {
		RBFNODE	*rbf = NULL;
		iovec[0] = 2.*(ix+.5)/sqres - 1.;
		iovec[1] = .0;
		iovec[2] = input_orient * sqrt(1. - iovec[0]*iovec[0]);
		if (funame == NULL)
			rbf = advect_rbf(iovec, lobe_lim);
		for (ox = 0; ox < sqres; ox++) {
		    float	last_bsdf = -1;
		    for (oy = 0; oy < sqres; oy++) {
			SDsquare2disk(iovec+3, (ox+.5)/sqres, (oy+.5)/sqres);
			iovec[5] = output_orient *
				sqrt(1. - iovec[3]*iovec[3] - iovec[4]*iovec[4]);
			if (funame == NULL)
			    bsdf = eval_rbfrep(rbf, iovec+3);
			else {
			    if (assignD) {
				varset("Dx", '=', -iovec[3]);
				varset("Dy", '=', -iovec[4]);
				varset("Dz", '=', -iovec[5]);
				++eclock;
			    }
			    bsdf = funvalue(funame, 6, iovec);
#if (NSSAMP > 0)
			    if (abs_diff(bsdf, last_bsdf) > ssamp_thresh) {
				int	ssi;
				double	ssa[3], ssvec[6], sum = 0;
						/* super-sample voxel */
				for (ssi = NSSAMP; ssi--; ) {
				    SDmultiSamp(ssa, 3, (ssi+frandom()) *
							(1./NSSAMP));
				    ssvec[0] = 2.*(ix+ssa[0])/sqres - 1.;
				    ssvec[1] = .0;
				    ssvec[2] = input_orient *
						sqrt(1. - ssvec[0]*ssvec[0]);
				    SDsquare2disk(ssvec+3, (ox+ssa[1])/sqres,
						(oy+ssa[2])/sqres);
				    ssvec[5] = output_orient *
						sqrt(1. - ssvec[3]*ssvec[3] -
							ssvec[4]*ssvec[4]);
				    if (assignD) {
					varset("Dx", '=', -ssvec[3]);
					varset("Dy", '=', -ssvec[4]);
					varset("Dz", '=', -ssvec[5]);
					++eclock;
				    }
				    sum += funvalue(funame, 6, ssvec);
				}
				bsdf = sum/NSSAMP;
			    }
#endif
			}
			if (pctcull >= 0)
				fwrite(&bsdf, sizeof(bsdf), 1, ofp);
			else
				printf("\t%.3e\n", bsdf);
			last_bsdf = bsdf;
		    }
		}
		if (rbf != NULL)
			free(rbf);
		prog_show((ix+1.)*(2./sqres));
	}
	if (pctcull >= 0) {			/* finish output */
		if (pclose(ofp)) {
			fprintf(stderr, "%s: error running '%s'\n",
					progname, cmd);
			exit(1);
		}
	} else {
		for (ix = sqres*sqres*sqres/2; ix--; )
			fputs("\t0\n", stdout);
		fputs("}\n", stdout);
	}
	data_epilogue();
	prog_done();
}

/* Interpolate and output anisotropic BSDF data */
static void
eval_anisotropic(char *funame)
{
	const int	sqres = 1<<samp_order;
	FILE		*ofp = NULL;
	int		assignD = 0;
	char		cmd[128];
	int		ix, iy, ox, oy;
	double		iovec[6];
	float		bsdf;

	data_prologue();			/* begin output */
	if (pctcull >= 0) {
		sprintf(cmd, "rttree_reduce%s -h -ff -r 4 -t %f -g %d",
				(input_orient>0 ^ output_orient>0) ? "" : " -a",
				pctcull, samp_order);
		fflush(stdout);
		ofp = popen(cmd, "w");
		if (ofp == NULL) {
			fprintf(stderr, "%s: cannot create pipe to rttree_reduce\n",
					progname);
			exit(1);
		}
		SET_FILE_BINARY(ofp);
#ifdef getc_unlocked				/* avoid lock/unlock overhead */
		flockfile(ofp);
#endif
	} else
		fputs("{\n", stdout);
						/* need to assign Dx, Dy, Dz? */
	if (funame != NULL)
		assignD = (fundefined(funame) < 6);
						/* run through directions */
	for (ix = 0; ix < sqres; ix++)
	    for (iy = 0; iy < sqres; iy++) {
		RBFNODE	*rbf = NULL;		/* Klems reversal */
		SDsquare2disk(iovec, 1.-(ix+.5)/sqres, 1.-(iy+.5)/sqres);
		iovec[2] = input_orient *
				sqrt(1. - iovec[0]*iovec[0] - iovec[1]*iovec[1]);
		if (funame == NULL)
			rbf = advect_rbf(iovec, lobe_lim);
		for (ox = 0; ox < sqres; ox++) {
		    float	last_bsdf = -1;
		    for (oy = 0; oy < sqres; oy++) {
			SDsquare2disk(iovec+3, (ox+.5)/sqres, (oy+.5)/sqres);
			iovec[5] = output_orient *
				sqrt(1. - iovec[3]*iovec[3] - iovec[4]*iovec[4]);
			if (funame == NULL)
			    bsdf = eval_rbfrep(rbf, iovec+3);
			else {
			    if (assignD) {
				varset("Dx", '=', -iovec[3]);
				varset("Dy", '=', -iovec[4]);
				varset("Dz", '=', -iovec[5]);
				++eclock;
			    }
			    bsdf = funvalue(funame, 6, iovec);
#if (NSSAMP > 0)
			    if (abs_diff(bsdf, last_bsdf) > ssamp_thresh) {
				int	ssi;
				double	ssa[4], ssvec[6], sum = 0;
						/* super-sample voxel */
				for (ssi = NSSAMP; ssi--; ) {
				    SDmultiSamp(ssa, 4, (ssi+frandom()) *
							(1./NSSAMP));
				    SDsquare2disk(ssvec, 1.-(ix+ssa[0])/sqres,
						1.-(iy+ssa[1])/sqres);
				    ssvec[2] = input_orient *
						sqrt(1. - ssvec[0]*ssvec[0] -
							ssvec[1]*ssvec[1]);
				    SDsquare2disk(ssvec+3, (ox+ssa[2])/sqres,
						(oy+ssa[3])/sqres);
				    ssvec[5] = output_orient *
						sqrt(1. - ssvec[3]*ssvec[3] -
							ssvec[4]*ssvec[4]);
				    if (assignD) {
					varset("Dx", '=', -ssvec[3]);
					varset("Dy", '=', -ssvec[4]);
					varset("Dz", '=', -ssvec[5]);
					++eclock;
				    }
				    sum += funvalue(funame, 6, ssvec);
				}
				bsdf = sum/NSSAMP;
			    }
#endif
			}
			if (pctcull >= 0)
				fwrite(&bsdf, sizeof(bsdf), 1, ofp);
			else
				printf("\t%.3e\n", bsdf);
			last_bsdf = bsdf;
		    }
		}
		if (rbf != NULL)
			free(rbf);
		prog_show((ix*sqres+iy+1.)/(sqres*sqres));
	    }
	if (pctcull >= 0) {			/* finish output */
		if (pclose(ofp)) {
			fprintf(stderr, "%s: error running '%s'\n",
					progname, cmd);
			exit(1);
		}
	} else
		fputs("}\n", stdout);
	data_epilogue();
	prog_done();
}

/* Read in BSDF and interpolate as tensor tree representation */
int
main(int argc, char *argv[])
{
	int	dofwd = 0, dobwd = 1;
	int	i, na;

	progname = argv[0];
	esupport |= E_VARIABLE|E_FUNCTION|E_RCONST;
	esupport &= ~(E_INCHAN|E_OUTCHAN);
	scompile("PI:3.14159265358979323846", NULL, 0);
	biggerlib();
	for (i = 1; i < argc-1 && (argv[i][0] == '-') | (argv[i][0] == '+'); i++)
		switch (argv[i][1]) {		/* get options */
		case 'e':
			scompile(argv[++i], NULL, 0);
			break;
		case 'f':
			if (!argv[i][2])
				fcompile(argv[++i]);
			else
				dofwd = (argv[i][0] == '+');
			break;
		case 'b':
			dobwd = (argv[i][0] == '+');
			break;
		case 't':
			switch (argv[i][2]) {
			case '3':
				single_plane_incident = 1;
				break;
			case '4':
				single_plane_incident = 0;
				break;
			case '\0':
				pctcull = atof(argv[++i]);
				break;
			default:
				goto userr;
			}
			break;
		case 'g':
			samp_order = atoi(argv[++i]);
			break;
		case 'l':
			lobe_lim = atoi(argv[++i]);
			break;
		case 'p':
			do_prog = atoi(argv[i]+2);
			break;
		default:
			goto userr;
		}
	if (single_plane_incident >= 0) {	/* function-based BSDF? */
		void	(*evf)(char *s) = single_plane_incident ?
				&eval_isotropic : &eval_anisotropic;
		if (i != argc-1 || fundefined(argv[i]) < 3) {
			fprintf(stderr,
	"%s: need single function with 6 arguments: bsdf(ix,iy,iz,ox,oy,oz)\n",
					progname);
			fprintf(stderr, "\tor 3 arguments using Dx,Dy,Dz: bsdf(ix,iy,iz)\n");
			goto userr;
		}
		++eclock;
		xml_prologue(argc, argv);	/* start XML output */
		if (dofwd) {
			input_orient = -1;
			output_orient = -1;
			prog_start("Evaluating outside reflectance");
			(*evf)(argv[i]);
			output_orient = 1;
			prog_start("Evaluating outside->inside transmission");
			(*evf)(argv[i]);
		}
		if (dobwd) {
			input_orient = 1;
			output_orient = 1;
			prog_start("Evaluating inside reflectance");
			(*evf)(argv[i]);
			output_orient = -1;
			prog_start("Evaluating inside->outside transmission");
			(*evf)(argv[i]);
		}
		xml_epilogue();			/* finish XML output & exit */
		return(0);
	}
	if (i < argc) {				/* open input files if given */
		int	nbsdf = 0;
		for ( ; i < argc; i++) {	/* interpolate each component */
			char	pbuf[256];
			FILE	*fpin = fopen(argv[i], "rb");
			if (fpin == NULL) {
				fprintf(stderr, "%s: cannot open BSDF interpolant '%s'\n",
						progname, argv[i]);
				return(1);
			}
			if (!load_bsdf_rep(fpin))
				return(1);
			fclose(fpin);
			if (!nbsdf++)		/* start XML on first dist. */
				xml_prologue(argc, argv);
			sprintf(pbuf, "Interpolating component '%s'", argv[i]);
			prog_start(pbuf);
			if (single_plane_incident)
				eval_isotropic(NULL);
			else
				eval_anisotropic(NULL);
		}
		xml_epilogue();			/* finish XML output & exit */
		return(0);
	}
	SET_FILE_BINARY(stdin);			/* load from stdin */
	if (!load_bsdf_rep(stdin))
		return(1);
	xml_prologue(argc, argv);		/* start XML output */
	prog_start("Interpolating from standard input");
	if (single_plane_incident)		/* resample dist. */
		eval_isotropic(NULL);
	else
		eval_anisotropic(NULL);
	xml_epilogue();				/* finish XML output & exit */
	return(0);
userr:
	fprintf(stderr,
	"Usage: %s [-g Nlog2][-t pctcull][-l maxlobes] [bsdf.sir ..] > bsdf.xml\n",
				progname);
	fprintf(stderr,
	"   or: %s -t{3|4} [-g Nlog2][-t pctcull][{+|-}for[ward]][{+|-}b[ackward]][-e expr][-f file] bsdf_func > bsdf.xml\n",
				progname);
	return(1);
}
