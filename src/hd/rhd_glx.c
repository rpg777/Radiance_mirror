/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * OpenGL GLX driver for holodeck display.
 * Based on x11 driver.
 */

#include "standard.h"
#include "rhd_qtree.h"

#include  <GL/glx.h>

#include  "x11icon.h"

#ifndef RAYQLEN
#define RAYQLEN		50000		/* max. rays to queue before flush */
#endif

#ifndef FEQ
#define FEQ(a,b)	((a)-(b) <= FTINY && (a)-(b) >= -FTINY)
#endif

#ifndef MAXCONE
#define MAXCONE		16		/* number of different cone sizes */
#endif
#ifndef MAXVERT
#define MAXVERT		32		/* maximum number of cone vertices */
#endif
#ifndef MINVERT
#define MINVERT		4		/* minimum number of cone vertices */
#endif
#ifndef DEPTHFACT
#define DEPTHFACT	16.		/* multiplier for depth tests */
#endif

#define GAMMA		1.4		/* default gamma correction */

#define MOVPCT		7		/* percent distance to move /frame */
#define MOVDIR(b)	((b)==Button1 ? 1 : (b)==Button2 ? 0 : -1)
#define MOVDEG		(-5)		/* degrees to orbit CW/down /frame */
#define MOVORB(s)	((s)&ShiftMask ? 1 : (s)&ControlMask ? -1 : 0)

#define MINWIDTH	480		/* minimum graphics window width */
#define MINHEIGHT	400		/* minimum graphics window height */

#define VIEWDIST	356		/* assumed viewing distance (mm) */

#define BORWIDTH	5		/* border width */

#define  ourscreen	DefaultScreen(ourdisplay)
#define  ourroot	RootWindow(ourdisplay,ourscreen)
#define  ourmask	(StructureNotifyMask|ExposureMask|KeyPressMask|\
			ButtonPressMask|ButtonReleaseMask)

#define  levptr(etype)	((etype *)&currentevent)

struct driver	odev;			/* global device driver structure */

static XEvent  currentevent;		/* current event */

static int  mapped = 0;			/* window is mapped? */
static unsigned long  ourblack=0, ourwhite=~0;

static Display  *ourdisplay = NULL;	/* our display */
static XVisualInfo  *ourvinf;		/* our visual information */
static Window  gwind = 0;		/* our graphics window */
static GLXContext	gctx;		/* our GLX context */

static double	pwidth, pheight;	/* pixel dimensions (mm) */

static double	curzmax = 1e4;		/* current depth upper limit */
static double	nxtzmax = 0.;		/* maximum (finite) depth so far */

static struct {
	double	rad;		/* cone radius */
	int	nverts;		/* number of vertices */
	FVECT	*va;		/* allocated vertex array */
} cone[MAXCONE];	/* precomputed cones for drawing */

static int	inpresflags;		/* input result flags */

static int	headlocked = 0;		/* lock vertical motion */

static int  resizewindow(), getevent(), getkey(), moveview(),
		initcones(), freecones(),
		getmove(), fixwindow(), mytmflags();


dev_open(id)			/* initialize X11 driver */
char  *id;
{
	extern char	*getenv();
	static int	atlBest[] = {GLX_RGBA, GLX_RED_SIZE,8,
				GLX_GREEN_SIZE,8, GLX_BLUE_SIZE,8,
				GLX_DEPTH_SIZE,15, None};
	char	*gv;
	double	gamval = GAMMA;
	XSetWindowAttributes	ourwinattr;
	XWMHints	ourxwmhints;
	XSizeHints	oursizhints;
					/* set quadtree globals */
	qtMinNodesiz = 3;
	qtDepthEps = 0.07;
					/* open display server */
	ourdisplay = XOpenDisplay(NULL);
	if (ourdisplay == NULL)
		error(USER, "cannot open X-windows; DISPLAY variable set?\n");
					/* find a usable visual */
	ourvinf = glXChooseVisual(ourdisplay, ourscreen, atlBest);
	if (ourvinf == NULL)
		error(USER, "no suitable visuals available");
					/* get a context */
	gctx = glXCreateContext(ourdisplay, ourvinf, NULL, GL_TRUE);
					/* set gamma and tone mapping */
	if ((gv = XGetDefault(ourdisplay, "radiance", "gamma")) != NULL
			|| (gv = getenv("DISPLAY_GAMMA")) != NULL)
		gamval = atof(gv);
	if (tmInit(mytmflags(), stdprims, gamval) == NULL)
		error(SYSTEM, "not enough memory in dev_open");
					/* open window */
	ourwinattr.background_pixel = ourblack;
	ourwinattr.border_pixel = ourblack;
	ourwinattr.event_mask = ourmask;
					/* this is stupid */
	ourwinattr.colormap = XCreateColormap(ourdisplay, ourroot,
				ourvinf->visual, AllocNone);
	gwind = XCreateWindow(ourdisplay, ourroot, 0, 0,
		DisplayWidth(ourdisplay,ourscreen)-2*BORWIDTH,
		DisplayHeight(ourdisplay,ourscreen)-2*BORWIDTH,
		BORWIDTH, ourvinf->depth, InputOutput, ourvinf->visual,
		CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &ourwinattr);
	if (gwind == 0)
		error(SYSTEM, "cannot create window\n");
   	XStoreName(ourdisplay, gwind, id);
					/* set window manager hints */
	ourxwmhints.flags = InputHint|IconPixmapHint;
	ourxwmhints.input = True;
	ourxwmhints.icon_pixmap = XCreateBitmapFromData(ourdisplay,
			gwind, x11icon_bits, x11icon_width, x11icon_height);
	XSetWMHints(ourdisplay, gwind, &ourxwmhints);
	oursizhints.min_width = MINWIDTH;
	oursizhints.min_height = MINHEIGHT;
	oursizhints.flags = PMinSize;
	XSetNormalHints(ourdisplay, gwind, &oursizhints);
					/* set GLX context */
	glXMakeCurrent(ourdisplay, gwind, gctx);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_FLAT);
	glDisable(GL_DITHER);
	glDisable(GL_CULL_FACE);
	glMatrixMode(GL_PROJECTION);
	glOrtho(0., 1., 0., 1., -.01, 1.01);
	glTranslated(0., 0., -1.01);
					/* figure out sensible view */
	pwidth = (double)DisplayWidthMM(ourdisplay, ourscreen) /
			DisplayWidth(ourdisplay, ourscreen);
	pheight = (double)DisplayHeightMM(ourdisplay, ourscreen) /
			DisplayHeight(ourdisplay, ourscreen);
	copystruct(&odev.v, &stdview);
	odev.v.type = VT_PER;
					/* map the window */
	XMapWindow(ourdisplay, gwind);
	dev_input();			/* sets size and view angles */
					/* allocate our leaf pile */
	if (!qtAllocLeaves(DisplayWidth(ourdisplay,ourscreen) *
			DisplayHeight(ourdisplay,ourscreen) * 3 /
			(qtMinNodesiz*qtMinNodesiz*2)))
		error(SYSTEM, "insufficient memory for value storage");
	odev.name = id;
	odev.ifd = ConnectionNumber(ourdisplay);
					/* initialize cone array */
	initcones();
}


dev_close()			/* close our display and free resources */
{
	glXMakeCurrent(ourdisplay, None, NULL);
	glXDestroyContext(ourdisplay, gctx);
	XDestroyWindow(ourdisplay, gwind);
	gwind = 0;
	XCloseDisplay(ourdisplay);
	ourdisplay = NULL;
	qtFreeLeaves();
	tmDone(NULL);
	freecones();
	odev.v.type = 0;
	odev.hres = odev.vres = 0;
	odev.ifd = -1;
}


int
dev_view(nv)			/* assign new driver view */
register VIEW	*nv;
{
	if (nv->type == VT_PAR ||		/* check view legality */
			nv->horiz > 160. || nv->vert > 160.) {
		error(COMMAND, "illegal view type/angle");
		nv->type = odev.v.type;
		nv->horiz = odev.v.horiz;
		nv->vert = odev.v.vert;
		return(0);
	}
	if (nv->vfore > FTINY) {
		error(COMMAND, "cannot handle fore clipping");
		nv->vfore = 0.;
		return(0);
	}
	if (nv != &odev.v) {
		if (!FEQ(nv->horiz,odev.v.horiz) ||	/* resize window? */
				!FEQ(nv->vert,odev.v.vert)) {
			int	dw = DisplayWidth(ourdisplay,ourscreen);
			int	dh = DisplayHeight(ourdisplay,ourscreen);

			dw -= 25;	/* for window frame */
			dh -= 50;
			odev.hres = 2.*VIEWDIST/pwidth *
					tan(PI/180./2.*nv->horiz);
			odev.vres = 2.*VIEWDIST/pheight *
					tan(PI/180./2.*nv->vert);
			if (odev.hres > dw) {
				odev.vres = dw * odev.vres / odev.hres;
				odev.hres = dw;
			}
			if (odev.vres > dh) {
				odev.hres = dh * odev.hres / odev.vres;
				odev.vres = dh;
			}
			XResizeWindow(ourdisplay, gwind, odev.hres, odev.vres);
			dev_input();	/* get resize event */
		}
		copystruct(&odev.v, nv);
	}
	if (nxtzmax > FTINY) {
		curzmax = nxtzmax;
		nxtzmax = 0.;
	}
	glClear(GL_DEPTH_BUFFER_BIT);
	qtReplant();
	return(1);
}


int
dev_input()			/* get X11 input */
{
	inpresflags = 0;

	do
		getevent();

	while (XQLength(ourdisplay) > 0);

	return(inpresflags);
}


int
dev_flush()			/* flush output */
{
	qtUpdate();
	glFlush();
	rayqleft = RAYQLEN;
	return(XPending(ourdisplay));
}


dev_cone(rgb, ip, rad)		/* render a cone in view coordinates */
BYTE	rgb[3];
FVECT	ip;
double	rad;
{
	register int	ci, j;
	double	apexh, basez;
					/* is window mapped? */
	if (!mapped)
		return;
					/* compute apex height (0. to 1.) */
	if (ip[2] > 1e6)
		apexh = 1. - 1./DEPTHFACT;
	else {
		if (ip[2] > nxtzmax)
			nxtzmax = ip[2];
		if (ip[2] >= curzmax)
			apexh = 1. - 1./DEPTHFACT;
		else
			apexh = 1. - ip[2]/(curzmax*DEPTHFACT);
	}
	rad *= 1.25;			/* find conservative cone match */
	for (ci = 0; ci < MAXCONE-1; ci++)
		if (cone[ci].rad >= rad)
			break;
					/* draw it */
	glColor3ub(rgb[0], rgb[1], rgb[2]);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3d(ip[0], ip[1], apexh);	/* start with apex */
	basez = apexh*cone[ci].va[0][2];	/* base z's all the same */
	for (j = 0; j < cone[ci].nverts; j++)	/* draw each face */
		glVertex3d(ip[0]+cone[ci].va[j][0], ip[1]+cone[ci].va[j][1],
				basez);
						/* connect last to first */
	glVertex3d(ip[0]+cone[ci].va[0][0], ip[1]+cone[ci].va[0][1], basez);
	glEnd();				/* all done */
}


static int
mytmflags()			/* figure out tone mapping flags */
{
	extern char	*progname;
	register char	*cp, *tail;
					/* find basic name */
	for (cp = tail = progname; *cp; cp++)
		if (*cp == '/')
			tail = cp+1;
	for (cp = tail; *cp && *cp != '.'; cp++)
		;
	if (cp-tail == 3 && !strncmp(tail, "glx", 3))
		return(TM_F_CAMERA|TM_F_NOSTDERR);
	if (cp-tail == 4 && !strncmp(tail, "glxh", 4))
		return(TM_F_HUMAN|TM_F_NOSTDERR);
	error(USER, "illegal driver name");
}


static
initcones()			/* initialize cone vertices */
{
	register int	i, j;
	double	minrad, d;

	if (cone[0].nverts)
		freecones();
	minrad = 2.*qtMinNodesiz/(double)(DisplayWidth(ourdisplay,ourscreen) +
					DisplayHeight(ourdisplay,ourscreen));
	for (i = 0; i < MAXCONE; i++) {
		d = (double)i/(MAXCONE-1); d *= d;	/* x^2 distribution */
		cone[i].rad = minrad + (1.-minrad)*d;
		cone[i].nverts = MINVERT + (MAXVERT-MINVERT)*d;
		cone[i].va = (FVECT *)malloc(cone[i].nverts*sizeof(FVECT));
		if (cone[i].va == NULL)
			error(SYSTEM, "out of memory in initcones");
		for (j = cone[i].nverts; j--; ) {
			d = 2.*PI * (j+.5) / (cone[i].nverts);
			cone[i].va[j][0] = cos(d) * cone[i].rad;
			cone[i].va[j][1] = sin(d) * cone[i].rad;
			cone[i].va[j][2] = 1. - cone[i].rad;
		}
	}
}


static
freecones()			/* free cone vertices */
{
	register int	i;

	for (i = MAXCONE; i--; )
		if (cone[i].nverts) {
			free((char *)cone[i].va);
			cone[i].va = NULL;
			cone[i].nverts = 0;
		}
}


static
getevent()			/* get next event */
{
	XNextEvent(ourdisplay, levptr(XEvent));
	switch (levptr(XEvent)->type) {
	case ConfigureNotify:
		resizewindow(levptr(XConfigureEvent));
		break;
	case UnmapNotify:
		mapped = 0;
		break;
	case MapNotify:
		mapped = 1;
		break;
	case Expose:
		fixwindow(levptr(XExposeEvent));
		break;
	case KeyPress:
		getkey(levptr(XKeyPressedEvent));
		break;
	case ButtonPress:
		getmove(levptr(XButtonPressedEvent));
		break;
	}
}


static
draw3dline(wp)			/* draw 3d line in world coordinates */
register FVECT	wp[2];
{
	glVertex3d(wp[0][0], wp[0][1], wp[0][2]);
	glVertex3d(wp[1][0], wp[1][1], wp[1][2]);
}


static
draw_grids()			/* draw holodeck section grids */
{
	static BYTE	gridrgba[4] = {0x0, 0xff, 0xff, 0x00};
	double	xmin, xmax, ymin, ymax, zmin, zmax;
	double	d, cx, sx, crad;
	FVECT	vx, vy;
	register int	i, j;
					/* can we even do it? */
	if (!mapped || odev.v.type != VT_PER)
		return;
					/* compute view frustum */
	if (normalize(odev.v.vdir) == 0.0)
		return;
	zmin = 0.01;
	zmax = 10000.;
	if (odev.v.vfore > FTINY)
		zmin = odev.v.vfore;
	if (odev.v.vaft > FTINY)
		zmax = odev.v.vaft;
	xmax = zmin * tan(PI/180./2. * odev.v.horiz);
	xmin = -xmax;
	d = odev.v.hoff * (xmax - xmin);
	xmin += d; xmax += d;
	ymax = zmin * tan(PI/180./2. * odev.v.vert);
	ymin = -ymax;
	d = odev.v.voff * (ymax - ymin);
	ymin += d; ymax += d;
					/* set view matrix */
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glFrustum(xmin, xmax, ymin, ymax, zmin, zmax);
	gluLookAt(odev.v.vp[0], odev.v.vp[1], odev.v.vp[2],
		odev.v.vp[0] + odev.v.vdir[0],
		odev.v.vp[1] + odev.v.vdir[1],
		odev.v.vp[2] + odev.v.vdir[2],
		odev.v.vup[0], odev.v.vup[1], odev.v.vup[2]);
	glDisable(GL_DEPTH_TEST);	/* write no depth values */
	glColor4ub(gridrgba[0], gridrgba[1], gridrgba[2], gridrgba[3]);
	glBegin(GL_LINES);		/* draw each grid line */
	gridlines(draw3dline);
	glEnd();
	glEnable(GL_DEPTH_TEST);	/* restore rendering params */
	glPopMatrix();
}


static
moveview(dx, dy, mov, orb)	/* move our view */
int	dx, dy, mov, orb;
{
	VIEW	nv;
	FVECT	odir, v1;
	double	d;
	register int	li;
				/* start with old view */
	copystruct(&nv, &odev.v);
				/* change view direction */
	if (mov | orb) {
		if ((li = qtFindLeaf(dx, dy)) < 0)
			return(0);	/* not on window */
		VSUM(odir, qtL.wp[li], nv.vp, -1.);
	} else {
		if (viewray(nv.vp, nv.vdir, &odev.v,
				(dx+.5)/odev.hres, (dy+.5)/odev.vres) < -FTINY)
			return(0);	/* outside view */
	}
	if (orb && mov) {		/* orbit left/right */
		spinvector(odir, odir, nv.vup, d=MOVDEG*PI/180.*mov);
		VSUM(nv.vp, qtL.wp[li], odir, -1.);
		spinvector(nv.vdir, nv.vdir, nv.vup, d);
	} else if (orb) {		/* orbit up/down */
		fcross(v1, odir, nv.vup);
		if (normalize(v1) == 0.)
			return(0);
		spinvector(odir, odir, v1, d=MOVDEG*PI/180.*orb);
		VSUM(nv.vp, qtL.wp[li], odir, -1.);
		spinvector(nv.vdir, nv.vdir, v1, d);
	} else if (mov) {		/* move forward/backward */
		d = MOVPCT/100. * mov;
		VSUM(nv.vp, nv.vp, odir, d);
	}
	if (!mov ^ !orb && headlocked) {	/* restore head height */
		VSUM(v1, odev.v.vp, nv.vp, -1.);
		d = DOT(v1, odev.v.vup);
		VSUM(nv.vp, nv.vp, odev.v.vup, d);
	}
	if (setview(&nv) != NULL)
		return(0);	/* illegal view */
	dev_view(&nv);
	inpresflags |= DFL(DC_SETVIEW);
	return(1);
}


static
getmove(ebut)				/* get view change */
XButtonPressedEvent	*ebut;
{
	int	movdir = MOVDIR(ebut->button);
	int	movorb = MOVORB(ebut->state);
	int	oldnodesiz = qtMinNodesiz;
	Window	rootw, childw;
	int	rootx, rooty, wx, wy;
	unsigned int	statemask;

	qtMinNodesiz = 24;		/* accelerate update rate */
	XNoOp(ourdisplay);

	while (!XCheckMaskEvent(ourdisplay,
			ButtonReleaseMask, levptr(XEvent))) {

		if (!XQueryPointer(ourdisplay, gwind, &rootw, &childw,
				&rootx, &rooty, &wx, &wy, &statemask))
			break;		/* on another screen */

		if (!moveview(wx, odev.vres-1-wy, movdir, movorb)) {
			sleep(1);
			continue;
		}
		glClear(GL_COLOR_BUFFER_BIT);
		qtUpdate();
		draw_grids();
		glFlush();
	}
	if (!(inpresflags & DFL(DC_SETVIEW))) {	/* do final motion */
		movdir = MOVDIR(levptr(XButtonReleasedEvent)->button);
		wx = levptr(XButtonReleasedEvent)->x;
		wy = levptr(XButtonReleasedEvent)->y;
		moveview(wx, odev.vres-1-wy, movdir, movorb);
	}
	dev_flush();

	qtMinNodesiz = oldnodesiz;	/* restore quadtree resolution */
}


static
getkey(ekey)				/* get input key */
register XKeyPressedEvent  *ekey;
{
	int  n;
	char	buf[8];

	n = XLookupString(ekey, buf, sizeof(buf), NULL, NULL);
	if (n != 1)
		return;
	switch (buf[0]) {
	case 'h':			/* turn on height motion lock */
		headlocked = 1;
		return;
	case 'H':			/* turn off height motion lock */
		headlocked = 0;
		return;
	case 'l':			/* retrieve last view */
		inpresflags |= DFL(DC_LASTVIEW);
		return;
	case 'p':			/* pause computation */
		inpresflags |= DFL(DC_PAUSE);
		return;
	case 'v':			/* spit out view */
		inpresflags |= DFL(DC_GETVIEW);
		return;
	case '\n':
	case '\r':			/* resume computation */
		inpresflags |= DFL(DC_RESUME);
		return;
	case CTRL('R'):			/* redraw screen */
		if (nxtzmax > FTINY) {
			curzmax = nxtzmax;
			nxtzmax = 0.;
		}
		glClear(GL_DEPTH_BUFFER_BIT);
		qtRedraw(0, 0, odev.hres, odev.vres);
		return;
	case CTRL('L'):			/* refresh from server */
		if (inpresflags & DFL(DC_REDRAW))
			return;
		if (nxtzmax > FTINY) {
			curzmax = nxtzmax;
			nxtzmax = 0.;
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		draw_grids();
		glFlush();
		qtCompost(100);			/* get rid of old values */
		inpresflags |= DFL(DC_REDRAW);	/* resend values from server */
		return;
	case 'K':			/* kill rtrace process(es) */
		inpresflags |= DFL(DC_KILL);
		break;
	case 'R':			/* restart rtrace */
		inpresflags |= DFL(DC_RESTART);
		break;
	case 'C':			/* clobber holodeck */
		inpresflags |= DFL(DC_CLOBBER);
		break;
	case 'q':			/* quit the program */
		inpresflags |= DFL(DC_QUIT);
		return;
	default:
		XBell(ourdisplay, 0);
		return;
	}
}


static
fixwindow(eexp)				/* repair damage to window */
register XExposeEvent  *eexp;
{
	int	xmin, xmax, ymin, ymax;

	if (odev.hres == 0 || odev.vres == 0)	/* first exposure */
		resizewindow((XConfigureEvent *)eexp);
	xmin = eexp->x; xmax = eexp->x + eexp->width;
	ymin = odev.vres - eexp->y - eexp->height; ymax = odev.vres - eexp->y;
						/* clear portion of depth */
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthFunc(GL_ALWAYS);
	glBegin(GL_POLYGON);
	glVertex3d((double)xmin/odev.hres, (double)ymin/odev.vres, 0.);
	glVertex3d((double)xmax/odev.hres, (double)ymin/odev.vres, 0.);
	glVertex3d((double)xmax/odev.hres, (double)ymax/odev.vres, 0.);
	glVertex3d((double)xmin/odev.hres, (double)ymax/odev.vres, 0.);
	glEnd();
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	qtRedraw(xmin, ymin, xmax, ymax);
}


static
resizewindow(ersz)			/* resize window */
register XConfigureEvent  *ersz;
{
	glViewport(0, 0, ersz->width, ersz->height);

	if (ersz->width == odev.hres && ersz->height == odev.vres)
		return;

	odev.hres = ersz->width;
	odev.vres = ersz->height;

	odev.v.horiz = 2.*180./PI * atan(0.5/VIEWDIST*pwidth*odev.hres);
	odev.v.vert = 2.*180./PI * atan(0.5/VIEWDIST*pheight*odev.vres);

	inpresflags |= DFL(DC_SETVIEW);
}
