/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Executive program for oconv, rpict and pfilt
 */

#include "standard.h"
#include "paths.h"
#include <ctype.h>


typedef struct {
	char	*name;		/* variable name */
	short	nick;		/* # characters required for nickname */
	short	nass;		/* # assignments made */
	char	*value;		/* assigned value(s) */
	int	(*fixval)();	/* assignment checking function */
} VARIABLE;

int	onevalue(), catvalues(), boolvalue(),
	qualvalue(), fltvalue(), intvalue();

				/* variables */
#define OBJECT		0		/* object files */
#define SCENE		1		/* scene files */
#define MATERIAL	2		/* material files */
#define ILLUM		3		/* mkillum input files */
#define MKILLUM		4		/* mkillum options */
#define RENDER		5		/* rendering options */
#define OCONV		6		/* oconv options */
#define PFILT		7		/* pfilt options */
#define VIEW		8		/* view(s) for picture(s) */
#define ZONE		9		/* simulation zone */
#define QUALITY		10		/* desired rendering quality */
#define OCTREE		11		/* octree file name */
#define PICTURE		12		/* picture file name */
#define AMBFILE		13		/* ambient file name */
#define OPTFILE		14		/* rendering options file */
#define EXPOSURE	15		/* picture exposure setting */
#define RESOLUTION	16		/* maximum picture resolution */
#define UP		17		/* view up (X, Y or Z) */
#define INDIRECT	18		/* indirection in lighting */
#define DETAIL		19		/* level of scene detail */
#define PENUMBRAS	20		/* shadow penumbras are desired */
#define VARIABILITY	21		/* level of light variability */
#define REPORT		22		/* report frequency and errfile */
				/* total number of variables */
#define NVARS		23

VARIABLE	vv[NVARS] = {		/* variable-value pairs */
	{"objects",	3,	0,	NULL,	catvalues},
	{"scene",	3,	0,	NULL,	catvalues},
	{"materials",	3,	0,	NULL,	catvalues},
	{"illum",	3,	0,	NULL,	catvalues},
	{"mkillum",	3,	0,	NULL,	catvalues},
	{"render",	3,	0,	NULL,	catvalues},
	{"oconv",	3,	0,	NULL,	catvalues},
	{"pfilt",	2,	0,	NULL,	catvalues},
	{"view",	2,	0,	NULL,	NULL},
	{"ZONE",	2,	0,	NULL,	onevalue},
	{"QUALITY",	3,	0,	NULL,	qualvalue},
	{"OCTREE",	3,	0,	NULL,	onevalue},
	{"PICTURE",	3,	0,	NULL,	onevalue},
	{"AMBFILE",	3,	0,	NULL,	onevalue},
	{"OPTFILE",	3,	0,	NULL,	onevalue},
	{"EXPOSURE",	3,	0,	NULL,	fltvalue},
	{"RESOLUTION",	3,	0,	NULL,	onevalue},
	{"UP",		2,	0,	NULL,	onevalue},
	{"INDIRECT",	3,	0,	NULL,	intvalue},
	{"DETAIL",	3,	0,	NULL,	qualvalue},
	{"PENUMBRAS",	3,	0,	NULL,	boolvalue},
	{"VARIABILITY",	3,	0,	NULL,	qualvalue},
	{"REPORT",	3,	0,	NULL,	onevalue},
};

VARIABLE	*matchvar();
char	*nvalue();

#define UPPER(c)	((c)&~0x20)	/* ASCII trick */

#define vnam(vc)	(vv[vc].name)
#define vdef(vc)	(vv[vc].nass)
#define vval(vc)	(vv[vc].value)
#define vint(vc)	atoi(vval(vc))
#define vlet(vc)	UPPER(vval(vc)[0])
#define vscale		vlet
#define vbool(vc)	(vlet(vc)=='T')

#define HIGH		'H'
#define MEDIUM		'M'
#define LOW		'L'

				/* overture calculation file */
#ifdef NIX
char	overfile[] = "overture.raw";
#else
char	overfile[] = "/dev/null";
#endif

extern unsigned long	fdate(), time();

unsigned long	scenedate;	/* date of latest scene or object file */
unsigned long	octreedate;	/* date of octree */
unsigned long	matdate;	/* date of latest material file */
unsigned long	illumdate;	/* date of last illum file */

char	*oct0name;		/* name of pre-mkillum octree */
unsigned long	oct0date;	/* date of pre-mkillum octree */
char	*oct1name;		/* name of post-mkillum octree */
unsigned long	oct1date;	/* date of post-mkillum octree (>= matdate) */

int	explicate = 0;		/* explicate variables */
int	silent = 0;		/* do work silently */
int	noaction = 0;		/* don't do anything */
int	sayview = 0;		/* print view out */
char	*rvdevice = NULL;	/* rview output device */
char	*viewselect = NULL;	/* specific view only */

int	overture = 0;		/* overture calculation needed */

char	*progname;		/* global argv[0] */
char	*rifname;		/* global rad input file name */

char	radname[MAXPATH];	/* root Radiance file name */


main(argc, argv)
int	argc;
char	*argv[];
{
	char	ropts[512];
	int	i;

	progname = argv[0];
				/* get options */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 's':
			silent++;
			break;
		case 'n':
			noaction++;
			break;
		case 'e':
			explicate++;
			break;
		case 'o':
			rvdevice = argv[++i];
			break;
		case 'V':
			sayview++;
			break;
		case 'v':
			viewselect = argv[++i];
			break;
		default:
			goto userr;
		}
	if (i >= argc)
		goto userr;
	rifname = argv[i];
				/* assign Radiance root file name */
	rootname(radname, rifname);
				/* load variable values */
	load(rifname);
				/* get any additional assignments */
	for (i++; i < argc; i++)
		setvariable(argv[i]);
				/* check assignments */
	checkvalues();
				/* check files and dates */
	checkfiles();
				/* set default values as necessary */
	setdefaults();
				/* print all values if requested */
	if (explicate)
		printvals();
				/* build octree (and run mkillum) */
	oconv();
				/* check date on ambient file */
	checkambfile();
				/* run simulation */
	renderopts(ropts);
	xferopts(ropts);
	if (rvdevice != NULL)
		rview(ropts);
	else
		rpict(ropts);
	exit(0);
userr:
	fprintf(stderr,
	"Usage: %s [-s][-n][-e][-V][-v view][-o dev] rfile [VAR=value ..]\n",
			progname);
	exit(1);
}


rootname(rn, fn)		/* remove tail from end of fn */
register char	*rn, *fn;
{
	char	*tp, *dp;

	for (tp = NULL, dp = rn; *rn = *fn++; rn++)
		if (ISDIRSEP(*rn))
			dp = rn;
		else if (*rn == '.')
			tp = rn;
	if (tp != NULL && tp > dp)
		*tp = '\0';
}


#define NOCHAR	127		/* constant for character to delete */


load(rfname)			/* load Radiance simulation file */
char	*rfname;
{
	FILE	*fp;
	char	buf[512];
	register char	*cp;

	if (rfname == NULL)
		fp = stdin;
	else if ((fp = fopen(rfname, "r")) == NULL)
		syserr(rfname);
	while (fgetline(buf, sizeof(buf), fp) != NULL) {
		for (cp = buf; *cp; cp++) {
			switch (*cp) {
			case '\\':
				*cp++ = NOCHAR;
				continue;
			case '#':
				*cp = '\0';
				break;
			default:
				continue;
			}
			break;
		}
		setvariable(buf);
	}
	fclose(fp);
}


setvariable(ass)		/* assign variable according to string */
register char	*ass;
{
	char	varname[32];
	int	n;
	register char	*cp;
	register VARIABLE	*vp;
	register int	i;

	while (isspace(*ass))		/* skip leading space */
		ass++;
	cp = varname;			/* extract name */
	while (cp < varname+sizeof(varname)-1
			&& *ass && !isspace(*ass) && *ass != '=')
		*cp++ = *ass++;
	*cp = '\0';
	if (!varname[0])
		return;		/* no variable name! */
					/* trim value */
	while (isspace(*ass) || *ass == '=')
		ass++;
	for (n = strlen(ass); n > 0; n--)
		if (!isspace(ass[n-1]))
			break;
	if (!n) {
		fprintf(stderr, "%s: warning - missing value for variable '%s'\n",
				progname, varname);
		return;
	}
					/* match variable from list */
	vp = matchvar(varname);
	if (vp == NULL) {
		fprintf(stderr, "%s: unknown variable '%s'\n",
				progname, varname);
		exit(1);
	}
					/* assign new value */
	if (i = vp->nass) {
		cp = vp->value;
		while (i--)
			while (*cp++)
				;
		i = cp - vp->value;
		vp->value = realloc(vp->value, i+n+1);
	} else
		vp->value = malloc(n+1);
	if (vp->value == NULL)
		syserr(progname);
	cp = vp->value+i;		/* copy value, squeezing spaces */
	*cp = *ass;
	for (i = 1; i <= n; i++) {
		if (ass[i] == NOCHAR)
			continue;
		if (isspace(*cp))
			while (isspace(ass[i]))
				i++;
		*++cp = ass[i];
	}
	if (isspace(*cp))		/* remove trailing space */
		*cp = '\0';
	vp->nass++;
}


VARIABLE *
matchvar(nam)			/* match a variable by its name */
char	*nam;
{
	int	n = strlen(nam);
	register int	i;

	for (i = 0; i < NVARS; i++)
		if (n >= vv[i].nick && !strncmp(nam, vv[i].name, n))
			return(vv+i);
	return(NULL);
}


char *
nvalue(vp, n)			/* return nth variable value */
VARIABLE	*vp;
register int	n;
{
	register char	*cp;

	if (vp == NULL | n < 0 | n >= vp->nass)
		return(NULL);
	cp = vp->value;
	while (n--)
		while (*cp++)
			;
	return(cp);
}


checkvalues()			/* check assignments */
{
	register int	i;

	for (i = 0; i < NVARS; i++)
		if (vv[i].fixval != NULL)
			(*vv[i].fixval)(vv+i);
}


onevalue(vp)			/* only one assignment for this variable */
register VARIABLE	*vp;
{
	if (vp->nass < 2)
		return;
	fprintf(stderr, "%s: warning - multiple assignment of variable '%s'\n",
			progname, vp->name);
	do
		vp->value += strlen(vp->value)+1;
	while (--vp->nass > 1);
}


catvalues(vp)			/* concatenate variable values */
register VARIABLE	*vp;
{
	register char	*cp;

	if (vp->nass < 2)
		return;
	for (cp = vp->value; vp->nass > 1; vp->nass--) {
		while (*cp)
			cp++;
		*cp++ = ' ';
	}
}


int
badmatch(tv, cv)		/* case insensitive truncated comparison */
register char	*tv, *cv;
{
	if (!*tv) return(1);		/* null string cannot match */
	do
		if (UPPER(*tv) != *cv++)
			return(1);
	while (*++tv);
	return(0);		/* OK */
}


boolvalue(vp)			/* check boolean for legal values */
register VARIABLE	*vp;
{
	if (!vp->nass) return;
	onevalue(vp);
	switch (UPPER(vp->value[0])) {
	case 'T':
		if (badmatch(vp->value, "TRUE")) break;
		return;
	case 'F':
		if (badmatch(vp->value, "FALSE")) break;
		return;
	}
	fprintf(stderr, "%s: illegal value for boolean variable '%s'\n",
			progname, vp->name);
	exit(1);
}


qualvalue(vp)			/* check qualitative var. for legal values */
register VARIABLE	*vp;
{
	if (!vp->nass) return;
	onevalue(vp);
	switch (UPPER(vp->value[0])) {
	case 'L':
		if (badmatch(vp->value, "LOW")) break;
		return;
	case 'M':
		if (badmatch(vp->value, "MEDIUM")) break;
		return;
	case 'H':
		if (badmatch(vp->value, "HIGH")) break;
		return;
	}
	fprintf(stderr, "%s: illegal value for qualitative variable '%s'\n",
			progname, vp->name);
	exit(1);
}


intvalue(vp)			/* check integer variable for legal values */
register VARIABLE	*vp;
{
	if (!vp->nass) return;
	onevalue(vp);
	if (isint(vp->value)) return;
	fprintf(stderr, "%s: illegal value for integer variable '%s'\n",
			progname, vp->name);
	exit(1);
}


fltvalue(vp)			/* check float variable for legal values */
register VARIABLE	*vp;
{
	if (!vp->nass) return;
	onevalue(vp);
	if (isflt(vp->value)) return;
	fprintf(stderr, "%s: illegal value for real variable '%s'\n",
			progname, vp->name);
	exit(1);
}


unsigned long
checklast(fnames)			/* check files and find most recent */
register char	*fnames;
{
	char	thisfile[MAXPATH];
	unsigned long	thisdate, lastdate = 0;
	register char	*cp;

	if (fnames == NULL)
		return(0);
	while (*fnames) {
		while (isspace(*fnames)) fnames++;
		cp = thisfile;
		while (*fnames && !isspace(*fnames))
			*cp++ = *fnames++;
		*cp = '\0';
		if (!(thisdate = fdate(thisfile)))
			syserr(thisfile);
		if (thisdate > lastdate)
			lastdate = thisdate;
	}
	return(lastdate);
}


char *
newfname(orig, pred)		/* create modified file name */
char	*orig;
int	pred;
{
	extern char	*rindex();
	register char	*cp;
	register int	n;
	int	suffix;

	n = 0; cp = orig; suffix = -1;		/* suffix position, length */
	while (*cp) {
		if (*cp == '.') suffix = n;
		else if (ISDIRSEP(*cp)) suffix = -1;
		cp++; n++;
	}
	if (suffix == -1) suffix = n;
	if ((cp = bmalloc(n+2)) == NULL)
		syserr(progname);
	strncpy(cp, orig, suffix);
	cp[suffix] = pred;			/* root name + pred + suffix */
	strcpy(cp+suffix+1, orig+suffix);
	return(cp);
}


checkfiles()			/* check for existence and modified times */
{
	unsigned long	objdate;

	if (!vdef(OCTREE)) {
		if ((vval(OCTREE) = bmalloc(strlen(radname)+5)) == NULL)
			syserr(progname);
		sprintf(vval(OCTREE), "%s.oct", radname);
		vdef(OCTREE)++;
	}
	octreedate = fdate(vval(OCTREE));
	if (vdef(ILLUM)) {		/* illum requires secondary octrees */
		oct0name = newfname(vval(OCTREE), '0');
		oct1name = newfname(vval(OCTREE), '1');
		oct0date = fdate(oct0name);
		oct1date = fdate(oct1name);
	} else
		oct0name = oct1name = vval(OCTREE);
	if ((scenedate = checklast(vval(SCENE))) &&
			(objdate = checklast(vval(OBJECT))) > scenedate)
		scenedate = objdate;
	illumdate = checklast(vval(ILLUM));
	if (!octreedate & !scenedate & !illumdate) {
		fprintf(stderr, "%s: need '%s' or '%s' or '%s'\n", progname,
				vnam(OCTREE), vnam(SCENE), vnam(ILLUM));
		exit(1);
	}
	matdate = checklast(vval(MATERIAL));
}	


getoctcube(org, sizp)		/* get octree bounding cube */
double	org[3], *sizp;
{
	extern FILE	*popen();
	static double	oorg[3], osiz = 0.;
	double	min[3], max[3];
	char	buf[512];
	FILE	*fp;
	register int	i;

	if (osiz <= FTINY)
		if (noaction && fdate(oct1name) <
				(scenedate>illumdate?scenedate:illumdate)) {
							/* run getbbox */
			sprintf(buf, "getbbox -w -h %s",
				vdef(SCENE) ? vval(SCENE) : vval(ILLUM));
			if ((fp = popen(buf, "r")) == NULL)
				syserr("getbbox");
			if (fscanf(fp, "%lf %lf %lf %lf %lf %lf",
					&min[0], &max[0], &min[1], &max[1],
					&min[2], &max[2]) != 6) {
				fprintf(stderr,
			"%s: error reading bounding box from getbbox\n",
						progname);
				exit(1);
			}
			for (i = 0; i < 3; i++)
				if (max[i] - min[i] > osiz)
					osiz = max[i] - min[i];
			for (i = 0; i < 3; i++)
				oorg[i] = (max[i]+min[i]-osiz)*.5;
			pclose(fp);
		} else {				/* from octree */
			oconv();	/* does nothing if done already */
			sprintf(buf, "getinfo -d < %s", oct1name);
			if ((fp = popen(buf, "r")) == NULL)
				syserr("getinfo");
			if (fscanf(fp, "%lf %lf %lf %lf", &oorg[0], &oorg[1],
					&oorg[2], &osiz) != 4) {
				fprintf(stderr,
			"%s: error reading bounding cube from getinfo\n",
						progname);
				exit(1);
			}
			pclose(fp);
		}
	org[0] = oorg[0]; org[1] = oorg[1]; org[2] = oorg[2]; *sizp = osiz;
}


setdefaults()			/* set default values for unassigned var's */
{
	double	org[3], size;
	char	buf[128];

	if (!vdef(ZONE)) {
		getoctcube(org, &size);
		sprintf(buf, "E %g %g %g %g %g %g", org[0], org[0]+size,
				org[1], org[1]+size, org[2], org[2]+size);
		vval(ZONE) = savqstr(buf);
		vdef(ZONE)++;
	}
	if (!vdef(INDIRECT)) {
		vval(INDIRECT) = "0";
		vdef(INDIRECT)++;
	}
	if (!vdef(QUALITY)) {
		vval(QUALITY) = "L";
		vdef(QUALITY)++;
	}
	if (!vdef(RESOLUTION)) {
		vval(RESOLUTION) = "512";
		vdef(RESOLUTION)++;
	}
	if (!vdef(PICTURE)) {
		vval(PICTURE) = radname;
		vdef(PICTURE)++;
	}
	if (!vdef(VIEW)) {
		vval(VIEW) = "X";
		vdef(VIEW)++;
	}
	if (!vdef(DETAIL)) {
		vval(DETAIL) = "M";
		vdef(DETAIL)++;
	}
	if (!vdef(PENUMBRAS)) {
		vval(PENUMBRAS) = "F";
		vdef(PENUMBRAS)++;
	}
	if (!vdef(VARIABILITY)) {
		vval(VARIABILITY) = "L";
		vdef(VARIABILITY)++;
	}
}


printvals()			/* print variable values */
{
	register int	i, j;

	for (i = 0; i < NVARS; i++)
		for (j = 0; j < vdef(i); j++)
			printf("%s= %s\n", vnam(i), nvalue(vv+i, j));
	fflush(stdout);
}


oconv()				/* run oconv and mkillum if necessary */
{
	static char	illumtmp[] = "ilXXXXXX";
	char	combuf[512], ocopts[64], mkopts[64];

	oconvopts(ocopts);		/* get options */
	if (octreedate < scenedate) {	/* check date on original octree */
						/* build command */
		if (vdef(MATERIAL))
			sprintf(combuf, "oconv%s %s %s > %s", ocopts,
				vval(MATERIAL), vval(SCENE), vval(OCTREE));
		else
			sprintf(combuf, "oconv%s %s > %s", ocopts,
					vval(SCENE), vval(OCTREE));
		
		if (runcom(combuf)) {		/* run it */
			fprintf(stderr,
				"%s: error generating octree\n\t%s removed\n",
					progname, vval(OCTREE));
			unlink(vval(OCTREE));
			exit(1);
		}
		octreedate = time((unsigned long *)NULL);
	}
	if (oct1name == vval(OCTREE))		/* no mkillum? */
		oct1date = octreedate > matdate ? octreedate : matdate;
	if (oct1date >= octreedate & oct1date >= matdate
			& oct1date >= illumdate)	/* all done */
		return;
						/* make octree0 */
	if (oct0date < scenedate | oct0date < illumdate) {
						/* build command */
		if (octreedate)
			sprintf(combuf, "oconv%s -i %s %s > %s", ocopts,
				vval(OCTREE), vval(ILLUM), oct0name);
		else if (vdef(MATERIAL))
			sprintf(combuf, "oconv%s %s %s > %s", ocopts,
				vval(MATERIAL), vval(ILLUM), oct0name);
		else
			sprintf(combuf, "oconv%s %s > %s", ocopts,
				vval(ILLUM), oct0name);
		if (runcom(combuf)) {		/* run it */
			fprintf(stderr,
				"%s: error generating octree\n\t%s removed\n",
					progname, oct0name);
			unlink(oct0name);
			exit(1);
		}
		oct0date = time((unsigned long *)NULL);
	}
	mkillumopts(mkopts);			/* build mkillum command */
	mktemp(illumtmp);
	sprintf(combuf, "mkillum%s %s \"<\" %s > %s", mkopts,
			oct0name, vval(ILLUM), illumtmp);
	if (runcom(combuf)) {			/* run it */
		fprintf(stderr, "%s: error running mkillum\n", progname);
		unlink(illumtmp);
		exit(1);
	}
						/* make octree1 (frozen) */
	if (octreedate)
		sprintf(combuf, "oconv%s -f -i %s %s > %s", ocopts,
			vval(OCTREE), illumtmp, oct1name);
	else if (vdef(MATERIAL))
		sprintf(combuf, "oconv%s -f %s %s > %s", ocopts,
			vval(MATERIAL), illumtmp, oct1name);
	else
		sprintf(combuf, "oconv%s -f %s > %s", ocopts,
			illumtmp, oct1name);
	if (runcom(combuf)) {		/* run it */
		fprintf(stderr,
			"%s: error generating octree\n\t%s removed\n",
				progname, oct1name);
		unlink(oct1name);
		exit(1);
	}
	oct1date = time((unsigned long *)NULL);
	rmfile(illumtmp);
}


char *
addarg(op, arg)				/* add argument and advance pointer */
register char	*op, *arg;
{
	*op = ' ';
	while (*++op = *arg++)
		;
	return(op);
}


oconvopts(oo)				/* get oconv options */
register char	*oo;
{
	/* BEWARE:  This may be called via setdefaults(), so no assumptions */

	*oo = '\0';
	if (vdef(OCONV))
		addarg(oo, vval(OCONV));
}


mkillumopts(mo)				/* get mkillum options */
register char	*mo;
{
	/* BEWARE:  This may be called via setdefaults(), so no assumptions */

	*mo = '\0';
	if (vdef(MKILLUM))
		addarg(mo, vval(MKILLUM));
}


checkambfile()			/* check date on ambient file */
{
	unsigned long	afdate;

	if (!vdef(AMBFILE))
		return;
	if (!(afdate = fdate(vval(AMBFILE))))
		return;
	if (oct1date > afdate)
		rmfile(vval(AMBFILE));
}


double
ambval()				/* compute ambient value */
{
	if (vdef(EXPOSURE)) {
		if (vval(EXPOSURE)[0] == '+' || vval(EXPOSURE)[0] == '-')
			return(.5/pow(2.,atof(vval(EXPOSURE))));
		return(.5/atof(vval(EXPOSURE)));
	}
	if (vlet(ZONE) == 'E')
		return(10.);
	if (vlet(ZONE) == 'I')
		return(.01);
	badvalue(ZONE);
}


renderopts(op)				/* set rendering options */
char	*op;
{
	switch(vscale(QUALITY)) {
	case LOW:
		lowqopts(op);
		break;
	case MEDIUM:
		medqopts(op);
		break;
	case HIGH:
		hiqopts(op);
		break;
	}
}


lowqopts(op)				/* low quality rendering options */
register char	*op;
{
	double	d, org[3], siz[3];

	*op = '\0';
	if (sscanf(vval(ZONE), "%*s %lf %lf %lf %lf %lf %lf", &org[0],
			&siz[0], &org[1], &siz[1], &org[2], &siz[2]) != 6)
		badvalue(ZONE);
	siz[0] -= org[0]; siz[1] -= org[1]; siz[2] -= org[2];
	getoctcube(org, &d);
	d *= 3./(siz[0]+siz[1]+siz[2]);
	switch (vscale(DETAIL)) {
	case LOW:
		op = addarg(op, "-ps 16 -dp 64");
		sprintf(op, " -ar %d", (int)(4*d));
		op += strlen(op);
		break;
	case MEDIUM:
		op = addarg(op, "-ps 8 -dp 128");
		sprintf(op, " -ar %d", (int)(8*d));
		op += strlen(op);
		break;
	case HIGH:
		op = addarg(op, "-ps 4 -dp 256");
		sprintf(op, " -ar %d", (int)(16*d));
		op += strlen(op);
		break;
	}
	op = addarg(op, "-pt .16");
	if (vbool(PENUMBRAS))
		op = addarg(op, "-ds .4");
	else
		op = addarg(op, "-ds 0");
	op = addarg(op, "-dt .2 -dc .25 -dr 0 -sj 0 -st .5");
	if (vdef(AMBFILE)) {
		sprintf(op, " -af %s", vval(AMBFILE));
		op += strlen(op);
	} else
		overture = 0;
	switch (vscale(VARIABILITY)) {
	case LOW:
		op = addarg(op, "-aa .4 -ad 64");
		break;
	case MEDIUM:
		op = addarg(op, "-aa .3 -ad 128");
		break;
	case HIGH:
		op = addarg(op, "-aa .25 -ad 256");
		break;
	}
	op = addarg(op, "-as 0");
	d = ambval();
	sprintf(op, " -av %.2g %.2g %.2g", d, d, d);
	op += strlen(op);
	op = addarg(op, "-lr 3 -lw .02");
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


medqopts(op)				/* medium quality rendering options */
register char	*op;
{
	double	d, org[3], siz[3];

	*op = '\0';
	if (sscanf(vval(ZONE), "%*s %lf %lf %lf %lf %lf %lf", &org[0],
			&siz[0], &org[1], &siz[1], &org[2], &siz[2]) != 6)
		badvalue(ZONE);
	siz[0] -= org[0]; siz[1] -= org[1]; siz[2] -= org[2];
	getoctcube(org, &d);
	d *= 3./(siz[0]+siz[1]+siz[2]);
	switch (vscale(DETAIL)) {
	case LOW:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 4" : "-ps 8");
		op = addarg(op, "-dp 256");
		sprintf(op, " -ar %d", (int)(8*d));
		op += strlen(op);
		break;
	case MEDIUM:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 3" : "-ps 6");
		op = addarg(op, "-dp 512");
		sprintf(op, " -ar %d", (int)(16*d));
		op += strlen(op);
		break;
	case HIGH:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 2" : "-ps 4");
		op = addarg(op, "-dp 1024");
		sprintf(op, " -ar %d", (int)(32*d));
		op += strlen(op);
		break;
	}
	op = addarg(op, "-pt .08");
	if (vbool(PENUMBRAS))
		op = addarg(op, "-ds .2 -dj .5");
	else
		op = addarg(op, "-ds .3");
	op = addarg(op, "-dt .1 -dc .5 -dr 1 -sj .7 -st .1");
	if (overture = vint(INDIRECT)) {
		sprintf(op, " -ab %d", overture);
		op += strlen(op);
	}
	if (vdef(AMBFILE)) {
		sprintf(op, " -af %s", vval(AMBFILE));
		op += strlen(op);
	} else
		overture = 0;
	switch (vscale(VARIABILITY)) {
	case LOW:
		op = addarg(op, "-aa .25 -ad 196 -as 0");
		break;
	case MEDIUM:
		op = addarg(op, "-aa .2 -ad 400 -as 64");
		break;
	case HIGH:
		op = addarg(op, "-aa .15 -ad 768 -as 196");
		break;
	}
	d = ambval();
	sprintf(op, " -av %.2g %.2g %.2g", d, d, d);
	op += strlen(op);
	op = addarg(op, "-lr 6 -lw .002");
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


hiqopts(op)				/* high quality rendering options */
register char	*op;
{
	double	d, org[3], siz[3];

	*op = '\0';
	if (sscanf(vval(ZONE), "%*s %lf %lf %lf %lf %lf %lf", &org[0],
			&siz[0], &org[1], &siz[1], &org[2], &siz[2]) != 6)
		badvalue(ZONE);
	siz[0] -= org[0]; siz[1] -= org[1]; siz[2] -= org[2];
	getoctcube(org, &d);
	d *= 3./(siz[0]+siz[1]+siz[2]);
	switch (vscale(DETAIL)) {
	case LOW:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 1" : "-ps 8");
		op = addarg(op, "-dp 1024");
		sprintf(op, " -ar %d", (int)(16*d));
		op += strlen(op);
		break;
	case MEDIUM:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 1" : "-ps 5");
		op = addarg(op, "-dp 2048");
		sprintf(op, " -ar %d", (int)(32*d));
		op += strlen(op);
		break;
	case HIGH:
		op = addarg(op, vbool(PENUMBRAS) ? "-ps 1" : "-ps 3");
		op = addarg(op, "-dp 4096");
		sprintf(op, " -ar %d", (int)(64*d));
		op += strlen(op);
		break;
	}
	op = addarg(op, "-pt .04");
	if (vbool(PENUMBRAS))
		op = addarg(op, "-ds .1 -dj .7");
	else
		op = addarg(op, "-ds .2");
	op = addarg(op, "-dt .05 -dc .75 -dr 3 -sj 1 -st .01");
	sprintf(op, " -ab %d", overture=vint(INDIRECT)+1);
	op += strlen(op);
	if (vdef(AMBFILE)) {
		sprintf(op, " -af %s", vval(AMBFILE));
		op += strlen(op);
	} else
		overture = 0;
	switch (vscale(VARIABILITY)) {
	case LOW:
		op = addarg(op, "-aa .15 -ad 256 -as 0");
		break;
	case MEDIUM:
		op = addarg(op, "-aa .125 -ad 512 -as 256");
		break;
	case HIGH:
		op = addarg(op, "-aa .08 -ad 1024 -as 512");
		break;
	}
	d = ambval();
	sprintf(op, " -av %.2g %.2g %.2g", d, d, d);
	op += strlen(op);
	op = addarg(op, "-lr 12 -lw .0005");
	if (vdef(RENDER))
		op = addarg(op, vval(RENDER));
}


xferopts(ro)				/* transfer options if indicated */
char	*ro;
{
	int	fd, n;
	register char	*cp;
	
	n = strlen(ro);
	if (n < 2)
		return;
	if (vdef(OPTFILE)) {
		for (cp = ro; cp[1]; cp++)
			if (isspace(cp[1]) && cp[2] == '-' && isalpha(cp[3]))
				*cp = '\n';
			else
				*cp = cp[1];
		*cp = '\n';
		fd = open(vval(OPTFILE), O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (fd < 0 || write(fd, ro, n) != n || close(fd) < 0)
			syserr(vval(OPTFILE));
		sprintf(ro, " @%s", vval(OPTFILE));
	}
#ifdef MSDOS
	else if (n > 50) {
		setenv("ROPT", ro+1);
		strcpy(ro, " $ROPT");
	}
#endif
}


pfiltopts(po)				/* get pfilt options */
register char	*po;
{
	*po = '\0';
	if (vdef(EXPOSURE)) {
		po = addarg(po, "-1 -e");
		po = addarg(po, vval(EXPOSURE));
	}
	switch (vscale(QUALITY)) {
	case MEDIUM:
		po = addarg(po, "-r 1");
		break;
	case HIGH:
		po = addarg(po, "-m .25");
		break;
	}
	if (vdef(PFILT))
		po = addarg(po, vval(PFILT));
}


matchword(s1, s2)			/* match white-delimited words */
register char	*s1, *s2;
{
	while (isspace(*s1)) s1++;
	while (isspace(*s2)) s2++;
	while (*s1 && !isspace(*s1))
		if (*s1++ != *s2++)
			return(0);
	return(!*s2 || isspace(*s2));
}


char *
specview(vs)				/* get proper view spec from vs */
register char	*vs;
{
	static char	vup[7][12] = {"-vu 0 0 -1","-vu 0 -1 0","-vu -1 0 0",
			"-vu 0 0 1", "-vu 1 0 0","-vu 0 1 0","-vu 0 0 1"};
	static char	viewopts[128];
	register char	*cp;
	int	xpos, ypos, zpos, viewtype, upax;
	register int	i;
	double	cent[3], dim[3], mult, d;

	if (vs == NULL || *vs == '-')
		return(vs);
	upax = 0;			/* get the up vector */
	if (vdef(UP)) {
		if (vval(UP)[0] == '-' || vval(UP)[0] == '+')
			upax = 1-'X'+UPPER(vval(UP)[1]);
		else
			upax = 1-'X'+vlet(UP);
		if (upax < 1 | upax > 3)
			badvalue(UP);
		if (vval(UP)[0] == '-')
			upax = -upax;
	}
					/* check standard view names */
	xpos = ypos = zpos = 0;
	if (*vs == 'X') {
		xpos = 1; vs++;
	} else if (*vs == 'x') {
		xpos = -1; vs++;
	}
	if (*vs == 'Y') {
		ypos = 1; vs++;
	} else if (*vs == 'y') {
		ypos = -1; vs++;
	}
	if (*vs == 'Z') {
		zpos = 1; vs++;
	} else if (*vs == 'z') {
		zpos = -1; vs++;
	}
	viewtype = 'v';
	if (*vs == 'v' | *vs == 'l' | *vs == 'a' | *vs == 'h')
		viewtype = *vs++;
	cp = viewopts;
	if ((!*vs || isspace(*vs)) && (xpos|ypos|zpos)) {	/* got one! */
		*cp++ = '-'; *cp++ = 'v'; *cp++ = 't'; *cp++ = viewtype;
		if (sscanf(vval(ZONE), "%*s %lf %lf %lf %lf %lf %lf",
				&cent[0], &dim[0], &cent[1], &dim[1],
				&cent[2], &dim[2]) != 6)
			badvalue(ZONE);
		for (i = 0; i < 3; i++) {
			dim[i] -= cent[i];
			cent[i] += .5*dim[i];
		}
		mult = vlet(ZONE)=='E' ? 2. : .45 ;
		sprintf(cp, " -vp %.2g %.2g %.2g -vd %.2g %.2g %.2g",
				cent[0]+xpos*mult*dim[0],
				cent[1]+ypos*mult*dim[1],
				cent[2]+zpos*mult*dim[2],
				-xpos*dim[0], -ypos*dim[1], -zpos*dim[2]);
		cp += strlen(cp);
					/* redirect up axis if necessary */
		switch (upax) {
		case 3:			/* plus or minus Z axis */
		case -3:
		case 0:
			if (!(xpos|ypos))
				upax = 2;
			break;
		case 2:			/* plus or minus Y axis */
		case -2:
			if (!(xpos|zpos))
				upax = 1;
			break;
		case 1:			/* plus or minus X axis */
		case -1:
			if (!(ypos|zpos))
				upax = 3;
			break;
		}
		cp = addarg(cp, vup[upax+3]);
		switch (viewtype) {
		case 'v':
			cp = addarg(cp, "-vh 45 -vv 45");
			break;
		case 'l':
			d = sqrt(dim[0]*dim[0]+dim[1]*dim[1]+dim[2]*dim[2]);
			sprintf(cp, " -vh %.2g -vv %.2g", d, d);
			cp += strlen(cp);
			break;
		case 'a':
		case 'h':
			cp = addarg(cp, "-vh 180 -vv 180");
			break;
		}
	} else {
		while (!isspace(*vs))		/* else skip id */
			if (!*vs++)
				return(NULL);
		if (upax) {			/* specify up vector */
			strcpy(cp, vup[upax+3]);
			cp += strlen(cp);
		}
	}
	if (cp == viewopts)		/* append any additional options */
		vs++;		/* skip prefixed space if unneeded */
	strcpy(cp, vs);
#ifdef MSDOS
	if (strlen(viewopts) > 40) {
		setenv("VIEW", viewopts);
		return("$VIEW");
	}
#endif
	return(viewopts);
}


char *
getview(n, vn)				/* get view n, or NULL if none */
int	n;
char	*vn;		/* returned view name */
{
	register char	*mv;

	if (viewselect != NULL) {		/* command-line selected */
		if (n)				/* only do one */
			return(NULL);
		if (viewselect[0] == '-') {	/* already specified */
			if (vn != NULL) *vn = '\0';
			return(viewselect);
		}
		if (vn != NULL) {
			for (mv = viewselect; *mv && !isspace(*mv);
					*vn++ = *mv++)
				;
			*vn = '\0';
		}
						/* view number? */
		if (isint(viewselect))
			return(specview(nvalue(vv+VIEW, atoi(viewselect)-1)));
						/* check list */
		while ((mv = nvalue(vv+VIEW, n++)) != NULL)
			if (matchword(viewselect, mv))
				return(specview(mv));
		return(specview(viewselect));	/* standard view? */
	}
	mv = nvalue(vv+VIEW, n);		/* use view n */
	if (vn != NULL & mv != NULL) {
		register char	*mv2 = mv;
		if (*mv2 != '-')
			while (*mv2 && !isspace(*mv2))
				*vn++ = *mv2++;
		*vn = '\0';
	}
	return(specview(mv));
}


printview(vopts)			/* print out selected view */
register char	*vopts;
{
	extern char	*atos(), *getenv();
	char	buf[256];
	FILE	*fp;
	register char	*cp;

	if (vopts == NULL)
		return(-1);
	fputs("VIEW=", stdout);
	do {
		if (matchword(vopts, "-vf")) {		/* expand view file */
			vopts = sskip(vopts);
			if (!*atos(buf, sizeof(buf), vopts))
				return(-1);
			if ((fp = fopen(buf, "r")) == NULL)
				return(-1);
			for (buf[sizeof(buf)-2] = '\n';
					fgets(buf, sizeof(buf), fp) != NULL &&
						buf[0] != '\n';
					buf[sizeof(buf)-2] = '\n') {
				if (buf[sizeof(buf)-2] != '\n') {
					ungetc(buf[sizeof(buf)-2], fp);
					buf[sizeof(buf)-2] = '\0';
				}
				if (matchword(buf, "VIEW=") ||
						matchword(buf, "rview")) {
					for (cp = sskip(buf); *cp && *cp != '\n'; cp++)
						putchar(*cp);
				}
			}
			fclose(fp);
			vopts = sskip(vopts);
		} else {
			while (isspace(*vopts))
				vopts++;
			putchar(' ');
#ifdef MSDOS
			if (*vopts == '$') {		/* expand env. var. */
				if (!*atos(buf, sizeof(buf), vopts+1))
					return(-1);
				if ((cp = getenv(buf)) == NULL)
					return(-1);
				fputs(cp, stdout);
				vopts = sskip(vopts);
			} else
#endif
				while (*vopts && !isspace(*vopts))
					putchar(*vopts++);
		}
	} while (*vopts++);
	putchar('\n');
	return(0);
}


rview(opts)				/* run rview with first view */
char	*opts;
{
	char	*vw;
	char	combuf[512];
					/* build command */
	if ((vw = getview(0, NULL)) == NULL)
		return;
	if (sayview)
		printview(vw);
	sprintf(combuf, "rview %s%s -R %s ", vw, opts, rifname);
	if (rvdevice != NULL)
		sprintf(combuf+strlen(combuf), "-o %s ", rvdevice);
	if (vdef(EXPOSURE))
		sprintf(combuf+strlen(combuf), "-pe %s ", vval(EXPOSURE));
	strcat(combuf, oct1name);
	if (runcom(combuf)) {		/* run it */
		fprintf(stderr, "%s: error running rview\n", progname);
		exit(1);
	}
}


rpict(opts)				/* run rpict and pfilt for each view */
char	*opts;
{
	char	combuf[1024];
	char	rawfile[MAXPATH], picfile[MAXPATH], rep[MAXPATH+16], res[32];
	char	pfopts[128];
	char	vs[32], *vw;
	int	vn, mult;
					/* get pfilt options */
	pfiltopts(pfopts);
					/* get resolution, reporting */
	switch (vscale(QUALITY)) {
	case LOW:
		mult = 1;
		break;
	case MEDIUM:
		mult = 2;
		break;
	case HIGH:
		mult = 3;
		break;
	}
	{
		int	xres, yres;
		double	aspect;
		int	n;
		n = sscanf(vval(RESOLUTION), "%d %d %lf", &xres, &yres, &aspect);
		if (n == 3)
			sprintf(res, "-x %d -y %d -pa %.3f",
					mult*xres, mult*yres, aspect);
		else if (n) {
			if (n == 1) yres = xres;
			sprintf(res, "-x %d -y %d", mult*xres, mult*yres);
		} else
			badvalue(RESOLUTION);
	}
	rep[0] = '\0';
	if (vdef(REPORT)) {
		double	minutes;
		int	n;
		n = sscanf(vval(REPORT), "%lf %s", &minutes, rawfile);
		if (n == 2)
			sprintf(rep, " -t %d -e %s", (int)(minutes*60), rawfile);
		else if (n == 1)
			sprintf(rep, " -t %d", (int)(minutes*60));
		else
			badvalue(REPORT);
	}
					/* do each view */
	vn = 0;
	while ((vw = getview(vn++, vs)) != NULL) {
		if (sayview)
			printview(vw);
		if (!vs[0])
			sprintf(vs, "%d", vn);
		sprintf(picfile, "%s_%s.pic", vval(PICTURE), vs);
						/* check date on picture */
		if (fdate(picfile) >= oct1date)
			continue;
						/* build rpict command */
		sprintf(rawfile, "%s_%s.raw", vval(PICTURE), vs);
		if (fdate(rawfile) >= oct1date)		/* recover */
			sprintf(combuf, "rpict%s%s -ro %s %s",
					rep, opts, rawfile, oct1name);
		else {
			if (overture) {		/* run overture calculation */
				sprintf(combuf,
				"rpict%s %s%s -x 64 -y 64 -ps 1 %s > %s",
						rep, vw, opts,
						oct1name, overfile);
				if (runcom(combuf)) {
					fprintf(stderr,
					"%s: error in overture for view %s\n",
						progname, vs);
					exit(1);
				}
#ifdef NIX
				rmfile(overfile);
#endif
			}
			sprintf(combuf, "rpict%s %s %s%s %s > %s",
					rep, vw, res, opts,
				oct1name, rawfile);
		}
		if (runcom(combuf)) {		/* run rpict */
			fprintf(stderr, "%s: error rendering view %s\n",
					progname, vs);
			exit(1);
		}
						/* build pfilt command */
		if (mult > 1)
			sprintf(combuf, "pfilt%s -x /%d -y /%d %s > %s",
					pfopts, mult, mult, rawfile, picfile);
		else
			sprintf(combuf, "pfilt%s %s > %s", pfopts,
					rawfile, picfile);
		if (runcom(combuf)) {		/* run pfilt */
			fprintf(stderr,
			"%s: error filtering view %s\n\t%s removed\n",
					progname, vs, picfile);
			unlink(picfile);
			exit(1);
		}
						/* remove raw file */
		rmfile(rawfile);
	}
}


runcom(cs)			/* run command */
char	*cs;
{
	if (!silent)		/* echo it */
		printf("\t%s\n", cs);
	if (noaction)
		return(0);
	fflush(stdout);		/* flush output and pass to shell */
	return(system(cs));
}


rmfile(fn)			/* remove a file */
char	*fn;
{
	if (!silent)
#ifdef MSDOS
		printf("\tdel %s\n", fn);
#else
		printf("\trm -f %s\n", fn);
#endif
	if (noaction)
		return(0);
	return(unlink(fn));
}


#ifdef MSDOS
setenv(vname, value)		/* set an environment variable */
char	*vname, *value;
{
	register char	*evp;

	evp = bmalloc(strlen(vname)+strlen(value)+2);
	if (evp == NULL)
		syserr(progname);
	sprintf(evp, "%s=%s", vname, value);
	if (putenv(evp) != 0) {
		fprintf(stderr, "%s: out of environment space\n", progname);
		exit(1);
	}
	if (!silent)
		printf("set %s\n", evp);
}
#endif


badvalue(vc)			/* report bad variable value and exit */
int	vc;
{
	fprintf(stderr, "%s: bad value for variable '%s'\n",
			progname, vnam(vc));
	exit(1);
}


syserr(s)			/* report a system error and exit */
char	*s;
{
	perror(s);
	exit(1);
}
