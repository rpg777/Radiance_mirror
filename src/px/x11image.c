/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  x11image.c - driver for X-windows
 *
 *     3/1/90
 */

/*
 *  Modified for X11
 *
 *  January 1990
 *
 *  Anat Grynberg  and Greg Ward
 */


#include  "standard.h"

#include  <signal.h>
#include  <X11/Xlib.h>
#include  <X11/cursorfont.h>
#include  <X11/Xutil.h>
#include  <X11/Xatom.h>

#include  "color.h"
#include  "view.h"
#include  "x11raster.h"
#include  "random.h"
#include  "resolu.h"

#ifdef  __alpha
#define  int4		int
#endif
#ifndef  int4
#define  int4		long
#endif

#define  FONTNAME	"8x13"		/* text font we'll use */

#define  CTRL(c)	((c)-'@')

#define  BORWIDTH	5		/* border width */

#define  ICONSIZ	(8*10)		/* maximum icon dimension (even 8) */

#define  ourscreen	DefaultScreen(thedisplay)
#define  ourroot	RootWindow(thedisplay,ourscreen)

#define  revline(x0,y0,x1,y1)	XDrawLine(thedisplay,wind,revgc,x0,y0,x1,y1)

#define  redraw(x,y,w,h) patch_raster(wind,(x)-xoff,(y)-yoff,x,y,w,h,ourras)

double  gamcor = 2.2;			/* gamma correction */

int  dither = 1;			/* dither colors? */
int  fast = 0;				/* keep picture in Pixmap? */

char	*dispname = NULL;		/* our display name */

Window  wind = 0;			/* our output window */
unsigned long  ourblack=0, ourwhite=1;	/* black and white for this visual */
int  maxcolors = 0;			/* maximum colors */
int  greyscale = 0;			/* in grey */

int  scale = 0;				/* scalefactor; power of two */

int  xoff = 0;				/* x image offset */
int  yoff = 0;				/* y image offset */

int  parent = 0;			/* number of children, -1 if child */

VIEW  ourview = STDVIEW;		/* image view parameters */
int  gotview = 0;			/* got parameters from file */

COLR  *scanline;			/* scan line buffer */

RESOLU  inpres;				/* input resolution and ordering */
int  xmax, ymax;			/* picture dimensions */
int  width, height;			/* window size */
char  *fname = NULL;			/* input file name */
FILE  *fin = stdin;			/* input file */
long  *scanpos = NULL;			/* scan line positions in file */
int  cury = 0;				/* current scan location */

double  exposure = 1.0;			/* exposure compensation used */

int  wrongformat = 0;			/* input in another format? */

GC	ourgc;				/* standard graphics context */
GC	revgc;				/* graphics context with GXinvert */

int		*ourrank;		/* our visual class ranking */
XVisualInfo	ourvis;			/* our visual */
XRASTER		*ourras;		/* our stored image */
unsigned char	*ourdata;		/* our image data */

struct {
	int  xmin, ymin, xsiz, ysiz;
}  box = {0, 0, 0, 0};			/* current box */

char  *geometry = NULL;			/* geometry specification */

char  icondata[ICONSIZ*ICONSIZ/8];	/* icon bitmap data */
int  iconwidth = 0, iconheight = 0;

char  *progname;

char  errmsg[128];

extern BYTE  clrtab[256][3];		/* global color map */

extern long  ftell();

Display  *thedisplay;
Atom  closedownAtom, wmProtocolsAtom;

int  noop() {}


main(argc, argv)
int  argc;
char  *argv[];
{
	extern char  *getenv();
	char  *gv;
	int  headline();
	int  i;
	int  pid;
	
	progname = argv[0];
	if ((gv = getenv("GAMMA")) != NULL)
		gamcor = atof(gv);

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'c':
				maxcolors = atoi(argv[++i]);
				break;
			case 'b':
				greyscale = !greyscale;
				break;
			case 'm':
				maxcolors = 2;
				break;
			case 'd':
				if (argv[i][2] == 'i')
					dispname = argv[++i];
				else
					dither = !dither;
				break;
			case 'f':
				fast = !fast;
				break;
			case 'e':
				if (argv[i+1][0] != '+' && argv[i+1][0] != '-')
					goto userr;
				scale = atoi(argv[++i]);
				break;
			case 'g':
				if (argv[i][2] == 'e')
					geometry = argv[++i];
				else
					gamcor = atof(argv[++i]);
				break;
			default:
				goto userr;
			}
		else if (argv[i][0] == '=')
			geometry = argv[i];
		else
			break;

	if (i > argc)
		goto userr;
	while (i < argc-1) {
		if ((pid=fork()) == 0) {	/* a child for each picture */
			parent = -1;
			break;
		}
		if (pid < 0)
			quiterr("fork failed");
		parent++;
		signal(SIGCONT, noop);
		pause();		/* wait for wake-up call */
		i++;
	}
	if (i < argc) {			/* open picture file */
		fname = argv[i];
		fin = fopen(fname, "r");
		if (fin == NULL) {
			sprintf(errmsg, "cannot open file \"%s\"", fname);
			quiterr(errmsg);
		}
	}
				/* get header */
	getheader(fin, headline, NULL);
				/* get picture dimensions */
	if (wrongformat || !fgetsresolu(&inpres, fin))
		quiterr("bad picture format");
	xmax = scanlen(&inpres);
	ymax = numscans(&inpres);
				/* set view parameters */
	if (gotview && setview(&ourview) != NULL)
		gotview = 0;
	if ((scanline = (COLR *)malloc(xmax*sizeof(COLR))) == NULL)
		quiterr("out of memory");

	init(argc, argv);			/* get file and open window */

	if (parent < 0)
		kill(getppid(), SIGCONT);	/* signal parent if child */

	for ( ; ; )
		getevent();		/* main loop */
userr:
	fprintf(stderr,
"Usage: %s [-di disp][[-ge] spec][-b][-m][-d][-f][-c nclrs][-e +/-stops] pic ..\n",
			progname);
	exit(1);
}


headline(s)		/* get relevant info from header */
char  *s;
{
	char  fmt[32];

	if (isexpos(s))
		exposure *= exposval(s);
	else if (isformat(s)) {
		formatval(fmt, s);
		wrongformat = strcmp(fmt, COLRFMT);
	} else if (isview(s) && sscanview(&ourview, s) > 0)
		gotview++;
}


init(argc, argv)			/* get data and open window */
int argc;
char **argv;
{
	XSetWindowAttributes	ourwinattr;
	XClassHint	xclshints;
	XWMHints	xwmhints;
	XSizeHints	xszhints;
	XTextProperty	windowName, iconName;
	XGCValues	xgcv;
	char	*name;
	register int	i;
	
	if (fname != NULL) {
		scanpos = (long *)malloc(ymax*sizeof(long));
		if (scanpos == NULL)
			quiterr("out of memory");
		for (i = 0; i < ymax; i++)
			scanpos[i] = -1;
		name = fname;
	} else
		name = progname;
				/* remove directory prefix from name */
	for (i = strlen(name); i-- > 0; )
		if (name[i] == '/')
			break;
	name += i+1;
	if ((thedisplay = XOpenDisplay(dispname)) == NULL)
		quiterr("cannot open display");
				/* get best visual for default screen */
	getbestvis();
				/* store image */
	getras();
				/* get size and position */
	xszhints.flags = 0;
	xszhints.width = xmax; xszhints.height = ymax;
	if (geometry != NULL) {
		i = XParseGeometry(geometry, &xszhints.x, &xszhints.y,
				(unsigned *)&xszhints.width,
				(unsigned *)&xszhints.height);
		if ((i&(WidthValue|HeightValue)) == (WidthValue|HeightValue))
			xszhints.flags |= USSize;
		else
			xszhints.flags |= PSize;
		if ((i&(XValue|YValue)) == (XValue|YValue)) {
			xszhints.flags |= USPosition;
			if (i & XNegative)
				xszhints.x += DisplayWidth(thedisplay,
				ourscreen)-1-xszhints.width-2*BORWIDTH;
			if (i & YNegative)
				xszhints.y += DisplayHeight(thedisplay,
				ourscreen)-1-xszhints.height-2*BORWIDTH;
		}
	}
	/* open window */
	ourwinattr.border_pixel = ourwhite;
	ourwinattr.background_pixel = ourblack;
	ourwinattr.colormap = XCreateColormap(thedisplay, ourroot,
			ourvis.visual, AllocNone);
	ourwinattr.event_mask = ExposureMask|KeyPressMask|ButtonPressMask|
			ButtonReleaseMask|ButtonMotionMask|StructureNotifyMask;
	ourwinattr.cursor = XCreateFontCursor(thedisplay, XC_diamond_cross);
	wind = XCreateWindow(thedisplay, ourroot, xszhints.x, xszhints.y,
			xszhints.width, xszhints.height, BORWIDTH,
			ourvis.depth, InputOutput, ourvis.visual, CWEventMask|
			CWCursor|CWBackPixel|CWBorderPixel|CWColormap, &ourwinattr);
	if (wind == 0)
		quiterr("cannot create window");
	width = xmax;
	height = ymax;
	xgcv.foreground = ourblack;
	xgcv.background = ourwhite;
	if ((xgcv.font = XLoadFont(thedisplay, FONTNAME)) == 0)
		quiterr("cannot get font");
	ourgc = XCreateGC(thedisplay, wind, GCForeground|GCBackground|
			GCFont, &xgcv);
	xgcv.function = GXinvert;
	revgc = XCreateGC(thedisplay, wind, GCForeground|GCBackground|
			GCFunction, &xgcv);

	/* set up the window manager */
	xwmhints.flags = InputHint|IconPixmapHint;
	xwmhints.input = True;
	xwmhints.icon_pixmap = XCreateBitmapFromData(thedisplay,
			wind, icondata, iconwidth, iconheight);

   	windowName.encoding = iconName.encoding = XA_STRING;
	windowName.format = iconName.format = 8;
	windowName.value = (u_char *)name;
	windowName.nitems = strlen(windowName.value);
	iconName.value = (u_char *)name;
	iconName.nitems = strlen(windowName.value);

	xclshints.res_name = NULL;
	xclshints.res_class = "Ximage";
	XSetWMProperties(thedisplay, wind, &windowName, &iconName,
			argv, argc, &xszhints, &xwmhints, &xclshints);
	closedownAtom = XInternAtom(thedisplay, "WM_DELETE_WINDOW", False);
	wmProtocolsAtom = XInternAtom(thedisplay, "WM_PROTOCOLS", False);
	XSetWMProtocols(thedisplay, wind, &closedownAtom, 1);

	XMapWindow(thedisplay, wind);
	return;
} /* end of init */


quiterr(err)		/* print message and exit */
char  *err;
{
	if (err != NULL)
		fprintf(stderr, "%s: %s: %s\n", progname, 
				fname==NULL?"<stdin>":fname, err);
	if (wind) {
		XDestroyWindow(thedisplay, wind);
		XFlush(thedisplay);
	}
	while (parent > 0 && wait(0) != -1)	/* wait for any children */
		parent--;
	exit(err != NULL);
}


static int
viscmp(v1,v2)		/* compare visual to see which is better, descending */
register XVisualInfo	*v1, *v2;
{
	int	bad1 = 0, bad2 = 0;
	register int  *rp;

	if (v1->class == v2->class) {
		if (v1->class == TrueColor || v1->class == DirectColor) {
					/* prefer 24-bit to 32-bit */
			if (v1->depth == 24 && v2->depth == 32)
				return(-1);
			if (v1->depth == 32 && v2->depth == 24)
				return(1);
			return(0);
		}
					/* don't be too greedy */
		if (maxcolors <= 1<<v1->depth && maxcolors <= 1<<v2->depth)
			return(v1->depth - v2->depth);
		return(v2->depth - v1->depth);
	}
					/* prefer Pseudo when < 24-bit */
	if ((v1->class == TrueColor || v1->class == DirectColor) &&
			v1->depth < 24)
		bad1 = 1;
	if ((v2->class == TrueColor || v2->class == DirectColor) &&
			v2->depth < 24)
		bad2 = -1;
	if (bad1 | bad2)
		return(bad1+bad2);
					/* otherwise, use class ranking */
	for (rp = ourrank; *rp != -1; rp++) {
		if (v1->class == *rp)
			return(-1);
		if (v2->class == *rp)
			return(1);
	}
	return(0);
}


getbestvis()			/* get the best visual for this screen */
{
#ifdef DEBUG
static char  vistype[][12] = {
		"StaticGray",
		"GrayScale",
		"StaticColor",
		"PseudoColor",
		"TrueColor",
		"DirectColor"
};
#endif
	static int	rankings[3][6] = {
		{TrueColor,DirectColor,PseudoColor,GrayScale,StaticGray,-1},
		{PseudoColor,GrayScale,StaticGray,-1},
		{PseudoColor,GrayScale,StaticGray,-1}
	};
	XVisualInfo	*xvi;
	int	vismatched;
	register int	i, j;

	if (greyscale) {
		ourrank = rankings[2];
		if (maxcolors < 2) maxcolors = 256;
	} else if (maxcolors >= 2 && maxcolors <= 256)
		ourrank = rankings[1];
	else {
		ourrank = rankings[0];
		maxcolors = 256;
	}
					/* find best visual */
	ourvis.screen = ourscreen;
	xvi = XGetVisualInfo(thedisplay,VisualScreenMask,&ourvis,&vismatched);
	if (xvi == NULL)
		quiterr("no visuals for this screen!");
#ifdef DEBUG
	fprintf(stderr, "Supported visuals:\n");
	for (i = 0; i < vismatched; i++)
		fprintf(stderr, "\ttype %s, depth %d\n",
				vistype[xvi[i].class], xvi[i].depth);
#endif
	for (i = 0, j = 1; j < vismatched; j++)
		if (viscmp(&xvi[i],&xvi[j]) > 0)
			i = j;
					/* compare to least acceptable */
	for (j = 0; ourrank[j++] != -1; )
		;
	ourvis.class = ourrank[--j];
	ourvis.depth = 1;
	if (viscmp(&xvi[i],&ourvis) > 0)
		quiterr("inadequate visuals on this screen");
					/* OK, we'll use it */
	copystruct(&ourvis, &xvi[i]);
#ifdef DEBUG
	fprintf(stderr, "Selected visual type %s, depth %d\n",
			vistype[ourvis.class], ourvis.depth);
#endif
					/* make appropriate adjustments */
	if (ourvis.class == GrayScale || ourvis.class == StaticGray)
		greyscale = 1;
	if (ourvis.depth <= 8 && ourvis.colormap_size < maxcolors)
		maxcolors = ourvis.colormap_size;
	if (ourvis.class == StaticGray) {
		ourblack = 0;
		ourwhite = 255;
	} else if (ourvis.class == PseudoColor) {
		ourblack = BlackPixel(thedisplay,ourscreen);
		ourwhite = WhitePixel(thedisplay,ourscreen);
		if ((ourblack|ourwhite) & ~255L) {
			ourblack = 0;
			ourwhite = 1;
		}
		if (maxcolors > 4)
			maxcolors -= 2;
	} else {
		ourblack = 0;
		ourwhite = ourvis.red_mask|ourvis.green_mask|ourvis.blue_mask;
	}
	XFree((char *)xvi);
}


getras()				/* get raster file */
{
	XVisualInfo	vinfo;

	if (maxcolors <= 2) {		/* monochrome */
		ourdata = (unsigned char *)malloc(ymax*((xmax+7)/8));
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, 1, ourdata,
				xmax, ymax, 8);
		if (ourras == NULL)
			goto fail;
		getmono();
	} else if (ourvis.class == TrueColor | ourvis.class == DirectColor) {
		ourdata = (unsigned char *)malloc(sizeof(int4)*xmax*ymax);
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, sizeof(int4)*8,
				ourdata, xmax, ymax, 32);
		if (ourras == NULL)
			goto fail;
		getfull();
	} else {
		ourdata = (unsigned char *)malloc(xmax*ymax);
		if (ourdata == NULL)
			goto fail;
		ourras = make_raster(thedisplay, &ourvis, 8, ourdata,
				xmax, ymax, 8);
		if (ourras == NULL)
			goto fail;
		if (greyscale | ourvis.class == StaticGray)
			getgrey();
		else
			getmapped();
		if (ourvis.class != StaticGray && !init_rcolors(ourras,clrtab))
			goto fail;
	}
	return;
fail:
	quiterr("could not create raster image");
}


getevent()				/* process the next event */
{
	XEvent xev;

	XNextEvent(thedisplay, &xev);
	switch ((int)xev.type) {
	case KeyPress:
		docom(&xev.xkey);
		break;
	case ConfigureNotify:
		width = xev.xconfigure.width;
		height = xev.xconfigure.height;
		break;
	case MapNotify:
		map_rcolors(ourras, wind);
		if (fast)
			make_rpixmap(ourras, wind);
		break;
	case UnmapNotify:
		if (!fast)
			unmap_rcolors(ourras);
		break;
	case Expose:
		redraw(xev.xexpose.x, xev.xexpose.y,
				xev.xexpose.width, xev.xexpose.height);
		break;
	case ButtonPress:
		if (xev.xbutton.state & (ShiftMask|ControlMask))
			moveimage(&xev.xbutton);
		else if (xev.xbutton.button == Button2)
			traceray(xev.xbutton.x, xev.xbutton.y);
		else
			getbox(&xev.xbutton);
		break;
	case ClientMessage:
		if ((xev.xclient.message_type == wmProtocolsAtom) &&
				(xev.xclient.data.l[0] == closedownAtom))
			quiterr(NULL);
		break;
	}
}


traceray(xpos, ypos)			/* print ray corresponding to pixel */
int  xpos, ypos;
{
	FLOAT  hv[2];
	FVECT  rorg, rdir;

	if (!gotview) {		/* no view, no can do */
		XBell(thedisplay, 0);
		return(-1);
	}
	pix2loc(hv, &inpres, xpos-xoff, ypos-yoff);
	if (viewray(rorg, rdir, &ourview, hv[0], hv[1]) < 0)
		return(-1);
	printf("%e %e %e ", rorg[0], rorg[1], rorg[2]);
	printf("%e %e %e\n", rdir[0], rdir[1], rdir[2]);
	fflush(stdout);
	return(0);
}


docom(ekey)				/* execute command */
XKeyPressedEvent  *ekey;
{
	char  buf[80];
	COLOR  cval;
	XColor  cvx;
	int  com, n;
	double  comp;
	FLOAT  hv[2];

	n = XLookupString(ekey, buf, sizeof(buf), NULL, NULL); 
	if (n == 0)
		return(0);
	com = buf[0];
	switch (com) {			/* interpret command */
	case 'q':
	case 'Q':
	case CTRL('D'):				/* quit */
		quiterr(NULL);
	case '\n':
	case '\r':
	case 'l':
	case 'c':				/* value */
		if (avgbox(cval) == -1)
			return(-1);
		switch (com) {
		case '\n':
		case '\r':				/* radiance */
			sprintf(buf, "%.3f", intens(cval)/exposure);
			break;
		case 'l':				/* luminance */
			sprintf(buf, "%.0fL", luminance(cval)/exposure);
			break;
		case 'c':				/* color */
			comp = pow(2.0, (double)scale);
			sprintf(buf, "(%.2f,%.2f,%.2f)",
					colval(cval,RED)*comp,
					colval(cval,GRN)*comp,
					colval(cval,BLU)*comp);
			break;
		}
		XDrawImageString(thedisplay, wind, ourgc,
				box.xmin, box.ymin+box.ysiz, buf, strlen(buf)); 
		return(0);
	case 'i':				/* identify (contour) */
		if (ourras->pixels == NULL)
			return(-1);
		n = ourdata[ekey->x-xoff+xmax*(ekey->y-yoff)];
		n = ourras->pmap[n];
		cvx.pixel = ourras->cdefs[n].pixel;
		cvx.red = random() & 65535;
		cvx.green = random() & 65535;
		cvx.blue = random() & 65535;
		cvx.flags = DoRed|DoGreen|DoBlue;
		XStoreColor(thedisplay, ourras->cmap, &cvx);
		return(0);
	case 'p':				/* position */
		pix2loc(hv, &inpres, ekey->x-xoff, ekey->y-yoff);
		sprintf(buf, "(%d,%d)", (int)(hv[0]*inpres.xr),
				(int)(hv[1]*inpres.yr));
		XDrawImageString(thedisplay, wind, ourgc, ekey->x, ekey->y,
					buf, strlen(buf));
		return(0);
	case 't':				/* trace */
		return(traceray(ekey->x, ekey->y));
	case '=':				/* adjust exposure */
	case '@':				/* adaptation level */
		if (avgbox(cval) == -1)
			return(-1);
		comp = com=='@'
		? 106./pow(1.219+pow(luminance(cval)/exposure,.4),2.5)/exposure
		: .5/bright(cval) ;
		comp = log(comp)/.69315 - scale;
		n = comp < 0 ? comp-.5 : comp+.5 ;	/* round */
		if (n == 0)
			return(0);
		scale_rcolors(ourras, pow(2.0, (double)n));
		scale += n;
		sprintf(buf, "%+d", scale);
		XDrawImageString(thedisplay, wind, ourgc,
				box.xmin, box.ymin+box.ysiz, buf, strlen(buf));
		XFlush(thedisplay);
		free(ourdata);
		free_raster(ourras);
		getras();
	/* fall through */
	case CTRL('R'):				/* redraw */
	case CTRL('L'):
		unmap_rcolors(ourras);
		XClearWindow(thedisplay, wind);
		map_rcolors(ourras, wind);
		if (fast)
			make_rpixmap(ourras, wind);
		redraw(0, 0, width, height);
		return(0);
	case 'f':				/* turn on fast redraw */
		fast = 1;
		make_rpixmap(ourras, wind);
		return(0);
	case 'F':				/* turn off fast redraw */
		fast = 0;
		free_rpixmap(ourras);
		return(0);
	case '0':				/* recenter origin */
		if (xoff == 0 & yoff == 0)
			return(0);
		xoff = yoff = 0;
		XClearWindow(thedisplay, wind);
		redraw(0, 0, width, height);
		return(0);
	case ' ':				/* clear */
		redraw(box.xmin, box.ymin, box.xsiz, box.ysiz);
		return(0);
	default:
		XBell(thedisplay, 0);
		return(-1);
	}
}


moveimage(ebut)				/* shift the image */
XButtonPressedEvent  *ebut;
{
	XEvent	e;
	int	mxo, myo;

	XMaskEvent(thedisplay, ButtonReleaseMask|ButtonMotionMask, &e);
	while (e.type == MotionNotify) {
		mxo = e.xmotion.x;
		myo = e.xmotion.y;
		revline(ebut->x, ebut->y, mxo, myo);
		revbox(xoff+mxo-ebut->x, yoff+myo-ebut->y,
				xoff+mxo-ebut->x+xmax, yoff+myo-ebut->y+ymax);
		XMaskEvent(thedisplay,ButtonReleaseMask|ButtonMotionMask,&e);
		revline(ebut->x, ebut->y, mxo, myo);
		revbox(xoff+mxo-ebut->x, yoff+myo-ebut->y,
				xoff+mxo-ebut->x+xmax, yoff+myo-ebut->y+ymax);
	}
	xoff += e.xbutton.x - ebut->x;
	yoff += e.xbutton.y - ebut->y;
	XClearWindow(thedisplay, wind);
	redraw(0, 0, width, height);
}


getbox(ebut)				/* get new box */
XButtonPressedEvent  *ebut;
{
	XEvent	e;

	XMaskEvent(thedisplay, ButtonReleaseMask|ButtonMotionMask, &e);
	while (e.type == MotionNotify) {
		revbox(ebut->x, ebut->y, box.xmin = e.xmotion.x, box.ymin = e.xmotion.y);
		XMaskEvent(thedisplay,ButtonReleaseMask|ButtonMotionMask,&e);
		revbox(ebut->x, ebut->y, box.xmin, box.ymin);
	}
	box.xmin = e.xbutton.x<0 ? 0 : (e.xbutton.x>=width ? width-1 : e.xbutton.x);
	box.ymin = e.xbutton.y<0 ? 0 : (e.xbutton.y>=height ? height-1 : e.xbutton.y);
	if (box.xmin > ebut->x) {
		box.xsiz = box.xmin - ebut->x + 1;
		box.xmin = ebut->x;
	} else {
		box.xsiz = ebut->x - box.xmin + 1;
	}
	if (box.ymin > ebut->y) {
		box.ysiz = box.ymin - ebut->y + 1;
		box.ymin = ebut->y;
	} else {
		box.ysiz = ebut->y - box.ymin + 1;
	}
}


revbox(x0, y0, x1, y1)			/* draw box with reversed lines */
int  x0, y0, x1, y1;
{
	revline(x0, y0, x1, y0);
	revline(x0, y1, x1, y1);
	revline(x0, y0, x0, y1);
	revline(x1, y0, x1, y1);
}


avgbox(clr)				/* average color over current box */
COLOR  clr;
{
	static COLOR  lc;
	static int  ll, lr, lt, lb;
	int  left, right, top, bottom;
	int  y;
	double  d;
	COLOR  ctmp;
	register int  x;

	setcolor(clr, 0.0, 0.0, 0.0);
	left = box.xmin - xoff;
	right = left + box.xsiz;
	if (left < 0)
		left = 0;
	if (right > xmax)
		right = xmax;
	if (left >= right)
		return(-1);
	top = box.ymin - yoff;
	bottom = top + box.ysiz;
	if (top < 0)
		top = 0;
	if (bottom > ymax)
		bottom = ymax;
	if (top >= bottom)
		return(-1);
	if (left == ll && right == lr && top == lt && bottom == lb) {
		copycolor(clr, lc);
		return(0);
	}
	for (y = top; y < bottom; y++) {
		if (getscan(y) == -1)
			return(-1);
		for (x = left; x < right; x++) {
			colr_color(ctmp, scanline[x]);
			addcolor(clr, ctmp);
		}
	}
	d = 1.0/((right-left)*(bottom-top));
	scalecolor(clr, d);
	ll = left; lr = right; lt = top; lb = bottom;
	copycolor(lc, clr);
	return(0);
}


getmono()			/* get monochrome data */
{
	register unsigned char	*dp;
	register int	x, err;
	int	y, errp;
	short	*cerr;

	if ((cerr = (short *)calloc(xmax,sizeof(short))) == NULL)
		quiterr("out of memory in getmono");
	dp = ourdata - 1;
	for (y = 0; y < ymax; y++) {
		getscan(y);
		add2icon(y, scanline);
		normcolrs(scanline, xmax, scale);
		err = 0;
		for (x = 0; x < xmax; x++) {
			if (!(x&7))
				*++dp = 0;
			errp = err;
			err += normbright(scanline[x]) + cerr[x];
			if (err > 127)
				err -= 255;
			else
				*dp |= 1<<(7-(x&07));
			err /= 3;
			cerr[x] = err + errp;
		}
	}
	free((char *)cerr);
}


add2icon(y, scan)		/* add a scanline to our icon data */
int  y;
COLR  *scan;
{
	static short  cerr[ICONSIZ];
	static int  ynext;
	static char  *dp;
	COLR  clr;
	register int  err;
	register int	x, ti;
	int  errp;

	if (iconheight == 0) {		/* initialize */
		if (xmax <= ICONSIZ && ymax <= ICONSIZ) {
			iconwidth = xmax;
			iconheight = ymax;
		} else if (xmax > ymax) {
			iconwidth = ICONSIZ;
			iconheight = ICONSIZ*ymax/xmax;
			if (iconheight < 1)
				iconheight = 1;
		} else {
			iconwidth = ICONSIZ*xmax/ymax;
			if (iconwidth < 1)
				iconwidth = 1;
			iconheight = ICONSIZ;
		}
		ynext = 0;
		dp = icondata - 1;
	}
	if (y < ynext*ymax/iconheight)	/* skip this one */
		return;
	err = 0;
	for (x = 0; x < iconwidth; x++) {
		if (!(x&7))
			*++dp = 0;
		errp = err;
		ti = x*xmax/iconwidth;
		copycolr(clr, scan[ti]);
		normcolrs(clr, 1, scale);
		err += normbright(clr) + cerr[x];
		if (err > 127)
			err -= 255;
		else
			*dp |= 1<<(x&07);
		err /= 3;
		cerr[x] = err + errp;
	}
	ynext++;
}


getfull()			/* get full (24-bit) data */
{
	int	y;
	register unsigned int4	*dp;
	register int	x;
					/* set gamma correction */
	setcolrgam(gamcor);
					/* read and convert file */
	dp = (unsigned int4 *)ourdata;
	for (y = 0; y < ymax; y++) {
		getscan(y);
		add2icon(y, scanline);
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		colrs_gambs(scanline, xmax);
		if (ourras->image->blue_mask & 1)
			for (x = 0; x < xmax; x++)
				*dp++ =	scanline[x][RED] << 16 |
					scanline[x][GRN] << 8 |
					scanline[x][BLU] ;
		else
			for (x = 0; x < xmax; x++)
				*dp++ =	scanline[x][RED] |
					scanline[x][GRN] << 8 |
					scanline[x][BLU] << 16 ;
	}
}


getgrey()			/* get greyscale data */
{
	int	y;
	register unsigned char	*dp;
	register int	x;
					/* set gamma correction */
	setcolrgam(gamcor);
					/* read and convert file */
	dp = ourdata;
	for (y = 0; y < ymax; y++) {
		getscan(y);
		add2icon(y, scanline);
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		for (x = 0; x < xmax; x++)
			scanline[x][GRN] = normbright(scanline[x]);
		colrs_gambs(scanline, xmax);
		if (maxcolors < 256)
			for (x = 0; x < xmax; x++)
				*dp++ =	((long)scanline[x][GRN] *
					maxcolors + maxcolors/2) >> 8;
		else
			for (x = 0; x < xmax; x++)
				*dp++ =	scanline[x][GRN];
	}
	for (x = 0; x < maxcolors; x++)
		clrtab[x][RED] = clrtab[x][GRN] =
			clrtab[x][BLU] = ((long)x*256 + 128)/maxcolors;
}


getmapped()			/* get color-mapped data */
{
	int	y;
					/* set gamma correction */
	setcolrgam(gamcor);
					/* make histogram */
	new_histo();
	for (y = 0; y < ymax; y++) {
		if (getscan(y) < 0)
			quiterr("seek error in getmapped");
		add2icon(y, scanline);
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		colrs_gambs(scanline, xmax);
		cnt_colrs(scanline, xmax);
	}
					/* map pixels */
	if (!new_clrtab(maxcolors))
		quiterr("cannot create color map");
	for (y = 0; y < ymax; y++) {
		if (getscan(y) < 0)
			quiterr("seek error in getmapped");
		if (scale)
			shiftcolrs(scanline, xmax, scale);
		colrs_gambs(scanline, xmax);
		if (dither)
			dith_colrs(ourdata+y*xmax, scanline, xmax);
		else
			map_colrs(ourdata+y*xmax, scanline, xmax);
	}
}


scale_rcolors(xr, sf)			/* scale color map */
register XRASTER	*xr;
double	sf;
{
	register int	i;
	long	maxv;

	if (xr->pixels == NULL)
		return;

	sf = pow(sf, 1.0/gamcor);
	maxv = 65535/sf;

	for (i = xr->ncolors; i--; ) {
		xr->cdefs[i].red = xr->cdefs[i].red > maxv ?
				65535 :
				xr->cdefs[i].red * sf;
		xr->cdefs[i].green = xr->cdefs[i].green > maxv ?
				65535 :
				xr->cdefs[i].green * sf;
		xr->cdefs[i].blue = xr->cdefs[i].blue > maxv ?
				65535 :
				xr->cdefs[i].blue * sf;
	}
	XStoreColors(thedisplay, xr->cmap, xr->cdefs, xr->ncolors);
}


getscan(y)
int  y;
{
	if (y != cury) {
		if (scanpos == NULL || scanpos[y] == -1)
			return(-1);
		if (fseek(fin, scanpos[y], 0) == -1)
			quiterr("fseek error");
		cury = y;
	} else if (scanpos != NULL && scanpos[y] == -1)
		scanpos[y] = ftell(fin);

	if (freadcolrs(scanline, xmax, fin) < 0)
		quiterr("read error");

	cury++;
	return(0);
}
