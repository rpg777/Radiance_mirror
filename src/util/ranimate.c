/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Radiance animation control program
 */

#include "standard.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "view.h"
#include "vars.h"
#include "netproc.h"
				/* input variables */
#define HOST		0		/* rendering host machine */
#define RENDER		1		/* rendering options */
#define PFILT		2		/* pfilt options */
#define PINTERP		3		/* pinterp options */
#define OCTREE		4		/* octree file name */
#define DIRECTORY	5		/* working (sub)directory */
#define BASENAME	6		/* output image base name */
#define VIEWFILE	7		/* animation frame views */
#define START		8		/* starting frame number */
#define END		9		/* ending frame number */
#define RIF		10		/* rad input file */
#define NEXTANIM	11		/* next animation file */
#define ANIMATE		12		/* animation command */
#define TRANSFER	13		/* frame transfer command */
#define ARCHIVE		14		/* archiving command */
#define INTERP		15		/* # frames to interpolate */
#define OVERSAMP	16		/* # times to oversample image */
#define MBLUR		17		/* samples for motion blur */
#define RTRACE		18		/* use rtrace with pinterp? */
#define DISKSPACE	19		/* how much disk space to use */
#define RESOLUTION	20		/* desired final resolution */
#define EXPOSURE	21		/* how to compute exposure */

int	NVARS = 22;		/* total number of variables */

VARIABLE	vv[] = {		/* variable-value pairs */
	{"host",	4,	0,	NULL,	NULL},
	{"render",	3,	0,	NULL,	catvalues},
	{"pfilt",	2,	0,	NULL,	catvalues},
	{"pinterp",	2,	0,	NULL,	catvalues},
	{"OCTREE",	3,	0,	NULL,	onevalue},
	{"DIRECTORY",	3,	0,	NULL,	onevalue},
	{"BASENAME",	3,	0,	NULL,	onevalue},
	{"VIEWFILE",	2,	0,	NULL,	onevalue},
	{"START",	2,	0,	NULL,	intvalue},
	{"END",		2,	0,	NULL,	intvalue},
	{"RIF",		3,	0,	NULL,	onevalue},
	{"NEXTANIM",	3,	0,	NULL,	onevalue},
	{"ANIMATE",	2,	0,	NULL,	onevalue},
	{"TRANSFER",	2,	0,	NULL,	onevalue},
	{"ARCHIVE",	2,	0,	NULL,	onevalue},
	{"INTERPOLATE",	3,	0,	NULL,	intvalue},
	{"OVERSAMPLE",	2,	0,	NULL,	fltvalue},
	{"MBLUR",	2,	0,	NULL,	onevalue},
	{"RTRACE",	2,	0,	NULL,	boolvalue},
	{"DISKSPACE",	3,	0,	NULL,	fltvalue},
	{"RESOLUTION",	3,	0,	NULL,	onevalue},
	{"EXPOSURE",	3,	0,	NULL,	onevalue},
};

#define SFNAME	"STATUS"		/* status file name */

struct {
	char	host[64];		/* control host name */
	int	pid;			/* control process id */
	char	cfname[128];		/* control file name */
	int	rnext;			/* next frame to render */
	int	fnext;			/* next frame to filter */
	int	tnext;			/* next frame to transfer */
}	astat;			/* animation status */

char	*progname;		/* our program name */
char	*cfname;		/* our control file name */

int	nowarn = 0;		/* turn warnings off? */
int	silent = 0;		/* silent mode? */
int	noaction = 0;		/* take no action? */

char	rendopt[2048] = "";	/* rendering options */
char	rresopt[32];		/* rendering resolution options */
char	fresopt[32];		/* filter resolution options */
int	pfiltalways;		/* always use pfilt? */

char	arcargs[10240];		/* files to archive */
char	*arcfirst, *arcnext;	/* pointers to first and next argument */

struct pslot {
	int	pid;			/* process ID (0 if empty) */
	int	fout;			/* output frame number */
	int	(*rcvf)();		/* recover function */
}	*pslot;			/* process slots */
int	npslots;		/* number of process slots */

int	lastpid;		/* ID of last completed background process */
PSERVER	*lastpserver;		/* last process server used */

#define phostname(ps)	((ps)->hostname[0] ? (ps)->hostname : astat.host)

struct pslot	*findpslot();

VIEW	*getview();
char	*getexp();


main(argc, argv)
int	argc;
char	*argv[];
{
	int	explicate = 0;
	int	i;

	progname = argv[0];			/* get arguments */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'e':			/* print variables */
			explicate++;
			break;
		case 'w':			/* turn off warnings */
			nowarn++;
			break;
		case 's':			/* silent mode */
			silent++;
			break;
		case 'n':			/* take no action */
			noaction++;
			break;
		default:
			goto userr;
		}
	if (i != argc-1)
		goto userr;
	cfname = argv[i];
						/* load variables */
	loadvars(cfname);
						/* did we get DIRECTORY? */
	checkdir();
						/* check status */
	if (getastat() < 0) {
		fprintf(stderr, "%s: exiting\n", progname);
		quit(1);
	}
						/* pfilt always if options given */
	pfiltalways = vdef(PFILT);
						/* load RIF if any */
	if (vdef(RIF))
		getradfile(vval(RIF));
						/* set defaults */
	setdefaults();
						/* print variables */
	if (explicate)
		printvars(stdout);
						/* set up process servers */
	sethosts();
						/* run animation */
	animate();
						/* all done */
	if (vdef(NEXTANIM)) {
		argv[i] = vval(NEXTANIM);	/* just change input file */
		if (!silent)
			printargs(argc, argv, stdout);
		execvp(progname, argv);		/* pass to next */
		quit(1);			/* shouldn't return */
	}
	quit(0);
userr:
	fprintf(stderr, "Usage: %s [-s][-n][-w][-e] anim_file\n", progname);
	quit(1);
}


getastat()			/* check/set animation status */
{
	char	buf[256];
	FILE	*fp;

	sprintf(buf, "%s/%s", vval(DIRECTORY), SFNAME);
	if ((fp = fopen(buf, "r")) == NULL) {
		if (errno != ENOENT) {
			perror(buf);
			return(-1);
		}
		astat.rnext = astat.fnext = astat.tnext = 0;
		goto setours;
	}
	if (fscanf(fp, "Control host: %s\n", astat.host) != 1)
		goto fmterr;
	if (fscanf(fp, "Control PID: %d\n", &astat.pid) != 1)
		goto fmterr;
	if (fscanf(fp, "Control file: %s\n", astat.cfname) != 1)
		goto fmterr;
	if (fscanf(fp, "Next render: %d\n", &astat.rnext) != 1)
		goto fmterr;
	if (fscanf(fp, "Next filter: %d\n", &astat.fnext) != 1)
		goto fmterr;
	if (fscanf(fp, "Next transfer: %d\n", &astat.tnext) != 1)
		goto fmterr;
	fclose(fp);
	if (astat.pid != 0) {			/* thinks it's still running */
		if (strcmp(myhostname(), astat.host)) {
			fprintf(stderr,
			"%s: process %d may still be running on host %s\n",
					progname, astat.pid, astat.host);
			return(-1);
		}
		if (kill(astat.pid, 0) != -1 || errno != ESRCH) {
			fprintf(stderr, "%s: process %d is still running\n",
					progname, astat.pid);
			return(-1);
		}
		/* assume it is dead */
	}
	if (strcmp(cfname, astat.cfname) && astat.tnext != 0) {	/* other's */
		fprintf(stderr, "%s: unfinished job \"%s\"\n",
				progname, astat.cfname);
		return(-1);
	}
setours:					/* set our values */
	strcpy(astat.host, myhostname());
	astat.pid = getpid();
	strcpy(astat.cfname, cfname);
	return(0);
fmterr:
	fprintf(stderr, "%s: format error in status file \"%s\"\n",
			progname, buf);
	fclose(fp);
	return(-1);
}


putastat()			/* put out current status */
{
	char	buf[256];
	FILE	*fp;

	if (noaction)
		return;
	sprintf(buf, "%s/%s", vval(DIRECTORY), SFNAME);
	if ((fp = fopen(buf, "w")) == NULL) {
		perror(buf);
		quit(1);
	}
	fprintf(fp, "Control host: %s\n", astat.host);
	fprintf(fp, "Control PID: %d\n", astat.pid);
	fprintf(fp, "Control file: %s\n", astat.cfname);
	fprintf(fp, "Next render: %d\n", astat.rnext);
	fprintf(fp, "Next filter: %d\n", astat.fnext);
	fprintf(fp, "Next transfer: %d\n", astat.tnext);
	fclose(fp);
}


checkdir()			/* make sure we have our directory */
{
	struct stat	stb;

	if (!vdef(DIRECTORY)) {
		fprintf(stderr, "%s: %s undefined\n",
				progname, vnam(DIRECTORY));
		quit(1);
	}
	if (stat(vval(DIRECTORY), &stb) == -1) {
		if (errno == ENOENT && mkdir(vval(DIRECTORY), 0777) == 0)
			return;
		perror(vval(DIRECTORY));
		quit(1);
	}
	if (!(stb.st_mode & S_IFDIR)) {
		fprintf(stderr, "%s: not a directory\n", vval(DIRECTORY));
		quit(1);
	}
}


setdefaults()			/* set default values */
{
	char	buf[256];

	if (vdef(ANIMATE)) {
		vval(OCTREE) = NULL;
		vdef(OCTREE) = 0;
	} else if (!vdef(OCTREE)) {
		fprintf(stderr, "%s: either %s or %s must be defined\n",
				progname, vnam(OCTREE), vnam(ANIMATE));
		quit(1);
	}
	if (!vdef(VIEWFILE)) {
		fprintf(stderr, "%s: %s undefined\n", progname, vnam(VIEWFILE));
		quit(1);
	}
	if (!vdef(HOST)) {
		vval(HOST) = LHOSTNAME;
		vdef(HOST)++;
	}
	if (!vdef(START)) {
		vval(START) = "1";
		vdef(START)++;
	}
	if (!vdef(END)) {
		sprintf(buf, "%d", countviews()+vint(START)-1);
		vval(END) = savqstr(buf);
		vdef(END)++;
	}
	if (vint(END) < vint(START)) {
		fprintf(stderr, "%s: ending frame less than starting frame\n",
				progname);
		quit(1);
	}
	if (!vdef(BASENAME)) {
		sprintf(buf, "%s/frame%%03d", vval(DIRECTORY));
		vval(BASENAME) = savqstr(buf);
		vdef(BASENAME)++;
	}
	if (!vdef(RESOLUTION)) {
		vval(RESOLUTION) = "640";
		vdef(RESOLUTION)++;
	}
	if (!vdef(OVERSAMP)) {
		vval(OVERSAMP) = "2";
		vdef(OVERSAMP)++;
	}
	if (!vdef(INTERP)) {
		vval(INTERP) = "0";
		vdef(INTERP)++;
	}
	if (!vdef(MBLUR)) {
		vval(MBLUR) = "0";
		vdef(MBLUR)++;
	}
	if (!vdef(RTRACE)) {
		vval(RTRACE) = "F";
		vdef(RTRACE)++;
	}
	if (!vdef(DISKSPACE)) {
		if (!nowarn)
			fprintf(stderr,
		"%s: warning - no %s setting, assuming 100 Mbytes available\n",
					progname, vnam(DISKSPACE));
		vval(DISKSPACE) = "100";
		vdef(DISKSPACE)++;
	}
				/* append rendering options */
	if (vdef(RENDER))
		sprintf(rendopt+strlen(rendopt), " %s", vval(RENDER));
}


sethosts()			/* set up process servers */
{
	extern char	*iskip();
	char	buf[256], *dir, *uname;
	int	np;
	register char	*cp;
	int	i;

	npslots = 0;
	if (noaction)
		return;
	for (i = 0; i < vdef(HOST); i++) {	/* add each host */
		dir = uname = NULL;
		np = 1;
		strcpy(cp=buf, nvalue(HOST, i));	/* copy to buffer */
		cp = sskip(cp);				/* skip host name */
		while (isspace(*cp))
			*cp++ = '\0';
		if (*cp) {				/* has # processes? */
			np = atoi(cp);
			if ((cp = iskip(cp)) == NULL || (*cp && !isspace(*cp)))
				badvalue(HOST);
			while (isspace(*cp))
				cp++;
			if (*cp) {			/* has directory? */
				dir = cp;
				cp = sskip(cp);			/* skip dir. */
				while (isspace(*cp))
					*cp++ = '\0';
				if (*cp) {			/* has user? */
					uname = cp;
					if (*sskip(cp))
						badvalue(HOST);
				}
			}
		}
		if (addpserver(buf, dir, uname, np) == NULL) {
			if (!nowarn)
				fprintf(stderr,
					"%s: cannot execute on host \"%s\"\n",
						progname, buf);
		} else
			npslots += np;
	}
	if (npslots == 0) {
		fprintf(stderr, "%s: no working process servers\n", progname);
		quit(1);
	}
	pslot = (struct pslot *)calloc(npslots, sizeof(struct pslot));
	if (pslot == NULL) {
		perror("malloc");
		quit(1);
	}
}


getradfile(rfargs)		/* run rad and get needed variables */
char	*rfargs;
{
	static short	mvar[] = {OCTREE,PFILT,RESOLUTION,EXPOSURE,-1};
	char	combuf[256];
	register int	i;
	register char	*cp;
					/* create rad command */
	sprintf(rendopt, " @%s/render.opt", vval(DIRECTORY));
	sprintf(combuf,
	"rad -v 0 -s -e -w %s OPTFILE=%s | egrep '^[ \t]*(NOMATCH",
			rfargs, rendopt+2);
	cp = combuf;
	while (*cp) cp++;		/* match unset variables */
	for (i = 0; mvar[i] >= 0; i++)
		if (!vdef(mvar[i])) {
			*cp++ = '|';
			strcpy(cp, vnam(mvar[i]));
			while (*cp) cp++;
		}
	sprintf(cp, ")[ \t]*=' > %s/radset.var", vval(DIRECTORY));
	cp += 11;			/* point to file name */
	if (system(combuf)) {
		fprintf(stderr, "%s: error executing rad command:\n\t%s\n",
				progname, combuf);
		quit(1);
	}
	loadvars(cp);			/* load variables and remove file */
	unlink(cp);
}


animate()			/* run animation */
{
	int	xres, yres;
	float	pa, mult;
	int	frames_batch;
	register int	i;
	double	d1, d2;
					/* compute rpict resolution */
	i = sscanf(vval(RESOLUTION), "%d %d %f", &xres, &yres, &pa);
	mult = vflt(OVERSAMP);
	if (i == 3) {
		sprintf(rresopt, "-x %d -y %d -pa %f", (int)(mult*xres),
				(int)(mult*yres), pa);
		sprintf(fresopt, "-x %d -y %d -pa %f", xres, yres, pa);
	} else if (i) {
		if (i == 1) yres = xres;
		sprintf(rresopt, "-x %d -y %d", (int)(mult*xres),
				(int)(mult*yres));
		sprintf(fresopt, "-x %d -y %d -pa 1", xres, yres);
	} else
		badvalue(RESOLUTION);
					/* consistency checks */
	if (vdef(ANIMATE)) {
		if (vint(INTERP)) {
			if (!nowarn)
				fprintf(stderr,
					"%s: resetting %s=0 for animation\n",
						progname, vnam(INTERP));
			vval(INTERP) = "0";
		}
		if (atoi(vval(MBLUR))) {	/* can't handle this yet */
			if (!nowarn)
				fprintf(stderr,
					"%s: resetting %s=0 for animation\n",
						progname, vnam(MBLUR));
			vval(MBLUR) = "0";
		}
	}
					/* figure # frames per batch */
	d1 = mult*xres*mult*yres*4;		/* space for orig. picture */
	if ((i=vint(INTERP)) || atoi(vval(MBLUR)))
		d1 += mult*xres*mult*yres*4;	/* space for z-buffer */
	d2 = xres*yres*4;			/* space for final picture */
	frames_batch = (i+1)*(vflt(DISKSPACE)*1048576.-d1)/(d1+i*d2);
	if (frames_batch < i+2) {
		fprintf(stderr, "%s: insufficient disk space allocated\n",
				progname);
		quit(1);
	}
					/* initialize archive argument list */
	i = 16;
	if (vdef(ARCHIVE) && strlen(vval(ARCHIVE)) > i)
		i = strlen(vval(ARCHIVE));
	arcnext = arcfirst = arcargs + i;
					/* initialize status file */
	if (astat.rnext == 0)
		astat.rnext = astat.fnext = astat.tnext = vint(START);
	putastat();
					/* render in batches */
	while (astat.tnext <= vint(END)) {
		renderframes(frames_batch);
		filterframes();
		transferframes();
	}
					/* mark status as finished */
	astat.pid = 0;
	putastat();
					/* close open files */
	getview(0);
	getexp(0);
}


renderframes(nframes)		/* render next nframes frames */
int	nframes;
{
	static char	vendbuf[16];
	VIEW	*vp;
	FILE	*fp = NULL;
	char	vfname[128];
	int	lastframe;
	register int	i;

	if (astat.tnext < astat.rnext)	/* other work to do first */
		return;
					/* create batch view file */
	if (!vdef(ANIMATE)) {
		sprintf(vfname, "%s/anim.vf", vval(DIRECTORY));
		if ((fp = fopen(vfname, "w")) == NULL) {
			perror(vfname);
			quit(1);
		}
	}
					/* bound batch properly */
	lastframe = astat.rnext + nframes - 1;
	if ((lastframe-1) % (vint(INTERP)+1))	/* need even interval */
		lastframe += vint(INTERP)+1 - ((lastframe-1)%(vint(INTERP)+1));
	if (lastframe > vint(END))		/* check for end */
		lastframe = vint(END);
					/* render each view */
	for (i = astat.rnext; i <= lastframe; i++) {
		if ((vp = getview(i)) == NULL) {
			if (!nowarn)
				fprintf(stderr,
				"%s: ran out of views before last frame\n",
					progname);
			sprintf(vval(END)=vendbuf, "%d", i-1);
			lastframe = i - 1;
			break;
		}
		if (vdef(ANIMATE))		/* animate frame */
			animrend(i, vp);
		else {				/* else record it */
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp);
		}
	}
	if (vdef(ANIMATE))		/* wait for renderings to finish */
		bwait(0);
	else {				/* else if walk-through */
		fclose(fp);		/* close view file */
		walkwait(astat.rnext, lastframe, vfname);	/* walk it */
		unlink(vfname);		/* remove view file */
	}
	astat.rnext = i;		/* update status */
	putastat();
}


filterframes()				/* catch up with filtering */
{
	VIEW	*vp;
	register int	i;

	if (astat.tnext < astat.fnext)	/* other work to do first */
		return;
					/* filter each view */
	for (i = astat.fnext; i < astat.rnext; i++) {
		if ((vp = getview(i)) == NULL) {	/* get view i */
			fprintf(stderr,
			"%s: unexpected error reading view for frame %d\n",
					progname, i);
			quit(1);
		}
		dofilt(i, vp, getexp(i), 0);		/* filter frame */
	}
	bwait(0);			/* wait for filter processes */
	archive();			/* archive originals */
	astat.fnext = i;		/* update status */
	putastat();
}


transferframes()			/* catch up with picture transfers */
{
	char	combuf[10240];
	register char	*cp;
	register int	i;

	if (astat.tnext >= astat.fnext)	/* nothing to do, yet */
		return;
	if (!vdef(TRANSFER)) {		/* no transfer function -- leave 'em */
		astat.tnext = astat.fnext;
		putastat();		/* update status */
		return;
	}
	strcpy(combuf, vval(TRANSFER));	/* start transfer command */
	cp = combuf + strlen(combuf);
					/* make argument list */
	for (i = astat.tnext; i < astat.fnext; i++) {
		*cp++ = ' ';
		sprintf(cp, vval(BASENAME), i);
		while (*cp) cp++;
		strcpy(cp, ".pic");
		cp += 4;
	}
	if (runcom(combuf)) {		/* transfer frames */
		fprintf(stderr, "%s: error running transfer command\n",
				progname);
		quit(1);
	}
	astat.tnext = i;		/* update status */
	putastat();
}


animrend(frame, vp)			/* start animation frame */
int	frame;
VIEW	*vp;
{
	extern int	recover();
	char	combuf[2048];
	char	fname[128];

	sprintf(fname, vval(BASENAME), frame);
	strcat(fname, ".unf");
	if (access(fname, F_OK) == 0)
		return;
	sprintf(combuf, "%s %d | rpict%s%s -w0 %s > %s", vval(ANIMATE), frame,
			rendopt, viewopt(vp), rresopt, fname);
	bruncom(combuf, frame, recover);	/* run in background */
}


walkwait(first, last, vfn)		/* walk-through frames */
int	first, last;
char	*vfn;
{
	char	combuf[2048];
	char	*inspoint;
	register int	i;

	if (!noaction && vint(INTERP))		/* create dummy frames */
		for (i = first; i <= last; i++)
			if (i < vint(END) && (i-1) % (vint(INTERP)+1)) {
				sprintf(combuf, vval(BASENAME), i);
				strcat(combuf, ".unf");
				close(open(combuf, O_RDONLY|O_CREAT, 0666));
			}
					/* create command */
	sprintf(combuf, "rpict%s -w0", rendopt);
	if (vint(INTERP) || atoi(vval(MBLUR)))
		sprintf(combuf+strlen(combuf), " -z %s.zbf", vval(BASENAME));
	sprintf(combuf+strlen(combuf), " -o %s.unf %s -S %d",
			vval(BASENAME), rresopt, first);
	inspoint = combuf + strlen(combuf);
	sprintf(inspoint, " %s < %s", vval(OCTREE), vfn);
					/* run in parallel */
	i = (last-first+1)/(vint(INTERP)+1);
	if (i < 1) i = 1;
	if (pruncom(combuf, inspoint, i)) {
		fprintf(stderr, "%s: error rendering frames %d through %d\n",
				progname, first, last);
		quit(1);
	}
	if (!noaction && vint(INTERP))		/* remove dummy frames */
		for (i = first; i <= last; i++)
			if (i < vint(END) && (i-1) % (vint(INTERP)+1)) {
				sprintf(combuf, vval(BASENAME), i);
				strcat(combuf, ".unf");
				unlink(combuf);
			}
}


int
recover(frame)				/* recover the specified frame */
int	frame;
{
	static int	*rfrm;		/* list of recovered frames */
	static int	nrfrms = 0;
	char	combuf[2048];
	char	fname[128];
	register char	*cp;
	register int	i;
					/* check to see if recovered already */
	for (i = nrfrms; i--; )
		if (rfrm[i] == frame)
			return(0);
					/* build command */
	sprintf(fname, vval(BASENAME), frame);
	if (vdef(ANIMATE))
		sprintf(combuf, "%s %d | rpict%s -w0",
				vval(ANIMATE), frame, rendopt);
	else
		sprintf(combuf, "rpict%s -w0", rendopt);
	cp = combuf + strlen(combuf);
	if (vint(INTERP) || atoi(vval(MBLUR))) {
		sprintf(cp, " -z %s.zbf", fname);
		while (*cp) cp++;
	}
	sprintf(cp, " -ro %s.unf", fname);
	while (*cp) cp++;
	if (!vdef(ANIMATE)) {
		*cp++ = ' ';
		strcpy(cp, vval(OCTREE));
	}
	if (runcom(combuf))		/* run command */
		return(1);
					/* add frame to recovered list */
	if (nrfrms)
		rfrm = (int *)realloc((char *)rfrm, (nrfrms+1)*sizeof(int));
	else
		rfrm = (int *)malloc(sizeof(int));
	if (rfrm == NULL) {
		perror("malloc");
		quit(1);
	}
	rfrm[nrfrms++] = frame;
	return(0);
}


int
frecover(frame)				/* recover filtered frame */
int	frame;
{
	VIEW	*vp;
	char	*ex;

	vp = getview(frame);
	ex = getexp(frame);
	if (dofilt(frame, vp, ex, 2) && dofilt(frame, vp, ex, 1))
		return(1);
	return(0);
}


archive()			/* archive and remove renderings */
{
#define RMCOML	(sizeof(rmcom)-1)
	static char	rmcom[] = "rm -f";
	register int	i;

	if (arcnext == arcfirst)
		return;				/* nothing to do */
	if (vdef(ARCHIVE)) {			/* run archive command */
		i = strlen(vval(ARCHIVE));
		strncpy(arcfirst-i, vval(ARCHIVE), i);
		if (runcom(arcfirst-i)) {
			fprintf(stderr, "%s: error running archive command\n",
					progname);
			quit(1);
		}
	}
						/* run remove command */
	strncpy(arcfirst-RMCOML, rmcom, RMCOML);
	runcom(arcfirst-RMCOML);
	arcnext = arcfirst;			/* reset argument list */
#undef RMCOML
}


int
dofilt(frame, vp, ep, rvr)			/* filter frame */
int	frame;
VIEW	*vp;
char	*ep;
int	rvr;
{
	extern int	frecover();
	static int	iter = 0;
	char	fnbefore[128], fnafter[128];
	char	combuf[1024], fname0[128], fname1[128];
	int	usepinterp, usepfilt, nora_rgbe;
	int	frseq[2];
						/* check what is needed */
	usepinterp = atoi(vval(MBLUR));
	usepfilt = pfiltalways | ep==NULL;
	if (ep != NULL && !strcmp(ep, "1"))
		ep = "+0";
	nora_rgbe = strcmp(vval(OVERSAMP),"1") || ep==NULL ||
			*ep != '+' || *ep != '-' || !isint(ep);
						/* compute rendered views */
	frseq[0] = frame - ((frame-1) % (vint(INTERP)+1));
	frseq[1] = frseq[0] + vint(INTERP) + 1;
	if (frseq[1] > vint(END))
		frseq[1] = vint(END);
	if (frseq[1] == frame) {			/* pfilt only */
		frseq[0] = frseq[1];
		usepinterp = 0;			/* update what's needed */
		usepfilt |= nora_rgbe;
	} else if (frseq[0] == frame) {		/* no interpolation needed */
		if (!rvr && frame > 1+vint(INTERP)) {	/* archive previous */
			*arcnext++ = ' ';
			sprintf(arcnext, vval(BASENAME), frame-vint(INTERP)-1);
			while (*arcnext) arcnext++;
			strcpy(arcnext, ".unf");
			arcnext += 4;
			if (usepinterp || vint(INTERP)) {	/* and z-buf */
				*arcnext++ = ' ';
				sprintf(arcnext, vval(BASENAME),
						frame-vint(INTERP)-1);
				while (*arcnext) arcnext++;
				strcpy(arcnext, ".zbf");
				arcnext += 4;
			}
		}
		if (!usepinterp)		/* update what's needed */
			usepfilt |= nora_rgbe;
	} else					/* interpolation needed */
		usepinterp++;
	if (frseq[1] >= astat.rnext)		/* next batch unavailable */
		frseq[1] = frseq[0];
	sprintf(fnbefore, vval(BASENAME), frseq[0]);
	sprintf(fnafter, vval(BASENAME), frseq[1]);
	if (rvr == 1 && recover(frseq[0]))	/* recover before frame? */
		return(1);
						/* generate command */
	if (usepinterp) {			/* using pinterp */
		if (rvr == 2 && recover(frseq[1]))	/* recover after? */
			return(1);
		if (atoi(vval(MBLUR))) {
			FILE	*fp;		/* motion blurring */
			sprintf(fname0, "%s/vw0%c", vval(DIRECTORY),
					'a'+(iter%26));
			if ((fp = fopen(fname0, "w")) == NULL) {
				perror(fname0); quit(1);
			}
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp); fclose(fp);
			if ((vp = getview(frame+1)) == NULL) {
				fprintf(stderr,
			"%s: unexpected error reading view for frame %d\n",
						progname, frame+1);
				quit(1);
			}
			sprintf(fname1, "%s/vw1%c", vval(DIRECTORY),
					'a'+(iter%26));
			if ((fp = fopen(fname1, "w")) == NULL) {
				perror(fname1); quit(1);
			}
			fputs(VIEWSTR, fp);
			fprintview(vp, fp);
			putc('\n', fp); fclose(fp);
			sprintf(combuf,
			"(pmblur %s %d %s %s; rm -f %s %s) | pinterp -B",
			*sskip(vval(MBLUR)) ? sskip2(vval(MBLUR),1) : "1",
					atoi(vval(MBLUR)),
					fname0, fname1, fname0, fname1);
			iter++;
		} else				/* no blurring */
			strcpy(combuf, "pinterp");
		strcat(combuf, viewopt(vp));
		if (vbool(RTRACE))
			sprintf(combuf+strlen(combuf), " -ff -fr '%s -w0 %s'",
					rendopt, vval(OCTREE));
		if (vdef(PINTERP))
			sprintf(combuf+strlen(combuf), " %s", vval(PINTERP));
		if (usepfilt)
			sprintf(combuf+strlen(combuf), " %s", rresopt);
		else
			sprintf(combuf+strlen(combuf), " %s -e %s",
					fresopt, ep);
		sprintf(combuf+strlen(combuf), " %s.unf %s.zbf",
				fnbefore, fnbefore);
		if (frseq[1] != frseq[0])
			 sprintf(combuf+strlen(combuf), " %s.unf %s.zbf",
					fnafter, fnafter);
		if (usepfilt) {			/* also pfilt */
			if (vdef(PFILT))
				sprintf(combuf+strlen(combuf), " | pfilt %s",
						vval(PFILT));
			else
				strcat(combuf, " | pfilt");
			if (ep != NULL)
				sprintf(combuf+strlen(combuf), " -1 -e %s %s",
						ep, fresopt);
			else
				sprintf(combuf+strlen(combuf), " %s", fresopt);
		}
	} else if (usepfilt) {			/* pfilt only */
		if (rvr == 2)
			return(1);
		if (vdef(PFILT))
			sprintf(combuf, "pfilt %s", vval(PFILT));
		else
			strcpy(combuf, "pfilt");
		if (ep != NULL)
			sprintf(combuf+strlen(combuf), " -1 -e %s %s %s.unf",
				ep, fresopt, fnbefore);
		else
			sprintf(combuf+strlen(combuf), " %s %s.unf",
					fresopt, fnbefore);
	} else {				/* else just check it */
		if (rvr == 2)
			return(1);
		sprintf(combuf, "ra_rgbe -e %s -r %s.unf", ep, fnbefore);
	}
						/* output file name */
	sprintf(fname0, vval(BASENAME), frame);
	sprintf(combuf+strlen(combuf), " > %s.pic", fname0);
	if (rvr)				/* in recovery */
		return(runcom(combuf));
	bruncom(combuf, frame, frecover);	/* else run in background */
	return(0);
}


VIEW *
getview(n)			/* get view number n */
int	n;
{
	static FILE	*viewfp = NULL;		/* view file pointer */
	static int	viewnum = 0;		/* current view number */
	static VIEW	curview = STDVIEW;	/* current view */
	char	linebuf[256];

	if (n == 0) {			/* signal to close file and clean up */
		if (viewfp != NULL) {
			fclose(viewfp);
			viewfp = NULL;
			viewnum = 0;
			copystruct(&curview, &stdview);
		}
		return(NULL);
	}
	if (viewfp == NULL) {		/* open file */
		if ((viewfp = fopen(vval(VIEWFILE), "r")) == NULL) {
			perror(vval(VIEWFILE));
			quit(1);
		}
	} else if (n < viewnum) {	/* rewind file */
		if (viewnum == 1 && feof(viewfp))
			return(&curview);		/* just one view */
		if (fseek(viewfp, 0L, 0) == EOF) {
			perror(vval(VIEWFILE));
			quit(1);
		}
		copystruct(&curview, &stdview);
		viewnum = 0;
	}
	while (n > viewnum) {		/* scan to desired view */
		if (fgets(linebuf, sizeof(linebuf), viewfp) == NULL)
			return(viewnum==1 ? &curview : NULL);
		if (isview(linebuf) && sscanview(&curview, linebuf) > 0)
			viewnum++;
	}
	return(&curview);		/* return it */
}


int
countviews()			/* count views in view file */
{
	register int	n = 0;

	while (getview(n+1) != NULL)
		n++;
	return(n);
}


char *
getexp(n)			/* get exposure for nth frame */
int	n;
{
	extern char	*fskip();
	static char	expval[32];
	static FILE	*expfp = NULL;
	static long	*exppos;
	static int	curfrm;
	register char	*cp;

	if (n == 0) {				/* signal to close file */
		if (expfp != NULL) {
			fclose(expfp);
			expfp = NULL;
		}
		return(NULL);
	}
	if (!vdef(EXPOSURE))			/* no setting (auto) */
		return(NULL);
	if (isflt(vval(EXPOSURE)))		/* always the same */
		return(vval(EXPOSURE));
	if (expfp == NULL) {			/* open exposure file */
		if ((expfp = fopen(vval(EXPOSURE), "r")) == NULL) {
			fprintf(stderr,
			"%s: cannot open exposure file \"%s\"\n",
					progname, vval(EXPOSURE));
			quit(1);
		}
		curfrm = vint(END) + 1;		/* init lookup tab. */
		exppos = (long *)malloc(curfrm*sizeof(long *));
		if (exppos == NULL) {
			perror(progname);
			quit(1);
		}
		while (curfrm--)
			exppos[curfrm] = -1L;
		curfrm = 0;
	}
						/* find position in file */
	if (n-1 != curfrm && n != curfrm && exppos[n-1] >= 0 &&
				fseek(expfp, exppos[curfrm=n-1], 0) == EOF) {
		fprintf(stderr, "%s: seek error on exposure file\n", progname);
		quit(1);
	}
	while (n > curfrm) {			/* read exposure */
		if (exppos[curfrm] < 0)
			exppos[curfrm] = ftell(expfp);
		if (fgets(expval, sizeof(expval), expfp) == NULL) {
			fprintf(stderr, "%s: too few exposures\n",
					vval(EXPOSURE));
			quit(1);
		}
		curfrm++;
		cp = fskip(expval);			/* check format */
		if (cp == NULL || *cp != '\n') {
			fprintf(stderr,
				"%s: exposure format error on line %d\n",
					vval(EXPOSURE), curfrm);
			quit(1);
		}
		*cp = '\0';
	}
	return(expval);				/* return value */
}


struct pslot *
findpslot(pid)			/* find or allocate a process slot */
int	pid;
{
	register struct pslot	*psempty = NULL;
	register int	i;

	for (i = 0; i < npslots; i++) {		/* look for match */
		if (pslot[i].pid == pid)
			return(pslot+i);
		if (psempty == NULL && pslot[i].pid == 0)
			psempty = pslot+i;
	}
	return(psempty);		/* return emtpy slot (error if NULL) */
}


int
donecom(ps, pn, status)		/* clean up after finished process */
PSERVER	*ps;
int	pn;
int	status;
{
	register PROC	*pp;

	pp = ps->proc + pn;
	if (pp->elen) {			/* pass errors */
		if (ps->hostname[0])
			fprintf(stderr, "%s: ", ps->hostname);
		fprintf(stderr, "Error output from: %s\n", pp->com);
		fputs(pp->errs, stderr);
		fflush(stderr);
		if (ps->hostname[0])
			status = 1;	/* because rsh doesn't return status */
	}
	freestr(pp->com);		/* free command string */
	lastpid = pp->pid;		/* record PID for bwait() */
	lastpserver = ps;		/* record server for serverdown() */
	return(status);
}


int
serverdown()			/* check status of last process server */
{
	if (pserverOK(lastpserver))	/* server still up? */
		return(0);
	delpserver(lastpserver);	/* else delete it */
	if (pslist == NULL) {
		fprintf(stderr, "%s: all process servers are down\n",
				progname);
		quit(1);
	}
	return(1);
}


int
bruncom(com, fout, rf)		/* run a command in the background */
char	*com;
int	fout;
int	(*rf)();
{
	int	pid;
	register struct pslot	*psl;

	if (noaction) {
		if (!silent)
			printf("\t%s\n", com);	/* echo command */
		return(0);
	}
					/* else start it when we can */
	while ((pid = startjob(NULL, savestr(com), donecom)) == -1)
		bwait(1);
	if (!silent) {				/* echo command */
		PSERVER	*ps;
		int	psn = pid;
		ps = findjob(&psn);
		printf("\t%s\n", com);
		printf("\tProcess started on %s\n", phostname(ps));
		fflush(stdout);
	}
	psl = findpslot(pid);		/* record info. in appropriate slot */
	psl->pid = pid;
	psl->fout = fout;
	psl->rcvf = rf;
	return(pid);
}


bwait(ncoms)				/* wait for batch job(s) to finish */
int	ncoms;
{
	int	status;
	register struct pslot	*psl;

	if (noaction)
		return;
	while ((status = wait4job(NULL, -1)) != -1) {
		psl = findpslot(lastpid);
		if (status) {		/* attempt recovery */
			serverdown();	/* check server */
			if (psl->rcvf == NULL || (*psl->rcvf)(psl->fout)) {
				fprintf(stderr,
					"%s: error rendering frame %d\n",
						progname, psl->fout);
				quit(1);
			}
		}
		psl->pid = 0;		/* free process slot */
		if (!--ncoms)
			return;		/* done enough */
	}
}


int
pruncom(com, ppins, maxcopies)	/* run a command in parallel over network */
char	*com, *ppins;
int	maxcopies;
{
	int	retstatus = 0;
	int	hostcopies;
	char	com1buf[10240], *com1, *endcom1;
	int	status;
	register PSERVER	*ps;

	if (!silent)
		printf("\t%s\n", com);	/* echo command */
	if (noaction)
		return(0);
	fflush(stdout);
					/* start jobs on each server */
	for (ps = pslist; ps != NULL; ps = ps->next) {
		hostcopies = 0;
		if (maxcopies > 1 && ps->nprocs > 1 && ppins != NULL) {
			strcpy(com1=com1buf, com);	/* build -PP command */
			sprintf(com1+(ppins-com), " -PP %s/%s.persist",
					vval(DIRECTORY), phostname(ps));
			strcat(com1, ppins);
			endcom1 = com1 + strlen(com1);
			sprintf(endcom1, "; kill `sed -n '1s/^[^ ]* //p' %s/%s.persist`",
					vval(DIRECTORY), phostname(ps));
		} else {
			com1 = com;
			endcom1 = NULL;
		}
		while (maxcopies > 0 &&
				startjob(ps, savestr(com1), donecom) != -1) {
			sleep(10);
			hostcopies++;
			maxcopies--;
			if (endcom1 != NULL)
				*endcom1 = '\0';
		}
		if (!silent && hostcopies) {
			if (hostcopies > 1)
				printf("\t%d duplicate processes", hostcopies);
			else
				printf("\tProcess");
			printf(" started on %s\n", phostname(ps));
			fflush(stdout);
		}
	}
					/* wait for jobs to finish */
	while ((status = wait4job(NULL, -1)) != -1)
		if (status)
			retstatus += !serverdown();	/* check server */
	return(retstatus);
}


runcom(cs)			/* run a command locally and wait for it */
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


badvalue(vc)			/* report bad variable value and exit */
int	vc;
{
	fprintf(stderr, "%s: bad value for variable '%s'\n",
			progname, vnam(vc));
	quit(1);
}
