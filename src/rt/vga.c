#ifndef lint
static const char	RCSid[] = "$Id: vga.c,v 2.9 2003/02/25 02:47:23 greg Exp $";
#endif
/*
 *  vga.c - driver for VGA graphics adaptor under MS-DOS
 */

#include "copyright.h"

#include  "standard.h"

#include  <graph.h>

#include  "driver.h"

#include  "color.h"

#define	 GAMMA		2.2		/* exponent for color correction */


int  vga_close(), vga_clear(), vga_paintr(), vga_comout(), vga_errout(),
		vga_comin();

static struct driver  vga_driver = {
	vga_close, vga_clear, vga_paintr, NULL,
	vga_comout, vga_comin, NULL, 1.0
};

static char  fatalerr[128];

static struct videoconfig  config;

extern char  *getenv();


struct driver *
vga_init(name, id)			/* open VGA */
char  *name, *id;
{
	static short  mode_pref[] = {_MRES256COLOR, -1};
	static short  smode_pref[] = {_XRES256COLOR, _SVRES256COLOR,
					_VRES256COLOR, _MRES256COLOR, -1};
	char  *ep;
	register short	*mp;

	mp = !strcmp(name, "vga") ? mode_pref : smode_pref;
	for ( ; *mp != -1; mp++)
		if (_setvideomode(*mp))
			break;
	if (*mp == -1) {
		_setvideomode(_DEFAULTMODE);
		eputs(name);
		eputs(": card not present or insufficient VGA memory\n");
		return(NULL);
	}
	_getvideoconfig(&config);
	_settextwindow(config.numtextrows-2, 1,
			config.numtextrows, config.numtextcols);
	vga_driver.xsiz = config.numxpixels;
	vga_driver.ysiz = (long)config.numypixels*(config.numtextrows-3)
				/config.numtextrows;
	switch (config.mode) {			/* correct problems */
	case _XRES256COLOR:
		vga_driver.ysiz -= 16;
		break;
	case _MRES256COLOR:
		vga_driver.pixaspect = 1.2;
		break;
	}
	_setviewport(0, 0, vga_driver.xsiz, vga_driver.ysiz);
	if ((ep = getenv("GAMMA")) != NULL)	/* make gamma map */
		make_gmap(atof(ep));
	else
		make_gmap(GAMMA);
	_remappalette(1, _BRIGHTWHITE);
	_settextcolor(1);
	ms_gcinit(&vga_driver);
	erract[USER].pf =
	erract[SYSTEM].pf =
	erract[INTERNAL].pf =
	erract[CONSISTENCY].pf = vga_errout;
	erract[COMMAND].pf = vga_comout;
	if (erract[WARNING].pf != NULL)
		erract[WARNING].pf = vga_comout;
	return(&vga_driver);
}


static
vga_close()					/* close VGA */
{
	ms_gcdone(&vga_driver);
	_setvideomode(_DEFAULTMODE);
	erract[USER].pf = 			/* reset error vector */
	erract[SYSTEM].pf =
	erract[INTERNAL].pf =
	erract[CONSISTENCY].pf = eputs;
	erract[COMMAND].pf = NULL;
	if (erract[WARNING].pf != NULL)
		erract[WARNING].pf = wputs;
	if (fatalerr[0])
		eputs(fatalerr);		/* repeat error message */
}


static
vga_clear(x, y)					/* clear VGA */
int  x, y;
{
	_clearscreen(_GCLEARSCREEN);
	new_ctab(config.numcolors-2);		/* init color table */
}


static
vga_paintr(col, xmin, ymin, xmax, ymax)		/* paint a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern int  vgacolr();

	_setcolor(get_pixel(col, vgacolr)+2);
	_rectangle(_GFILLINTERIOR, xmin, vga_driver.ysiz-ymax,
			xmax-1, vga_driver.ysiz-1-ymin);
	vga_driver.inpready = kbhit();
}


static
vga_comout(s)				/* put s to text output */
register char  *s;
{
	struct rccoord	tpos;
	char  buf[128];
	register char  *cp;

	for (cp = buf; ; s++) {
		switch (*s) {
		case '\b':
		case '\r':
		case '\0':
			if (cp > buf) {
				*cp = '\0';
				_outtext(cp = buf);
			}
			if (*s == '\0')
				break;
			tpos = _gettextposition();
			_settextposition(tpos.row, *s=='\r' ? 1 : tpos.col-1);
			continue;
		default:
			*cp++ = *s;
			continue;
		}
		return(0);
	}
	fatalerr[0] = '\0';
}


static
vga_errout(msg)
register char  *msg;
{
	static char  *fep = fatalerr;

	_outtext(msg);
	while (*msg)
		*fep++ = *msg++;
	*fep = '\0';
	if (fep > fatalerr && fep[-1] == '\n')
		fep = fatalerr;
}


static
vga_comin(buf, prompt)			/* get input line from console */
char  *buf;
char  *prompt;
{
	extern int  getch();

	if (prompt != NULL)
		_outtext(prompt);
	editline(buf, getch, vga_comout);
	vga_driver.inpready = kbhit();
}


static
vgacolr(index, r, g, b)		       /* enter a color into our table */
int  index;
int  r, g, b;
{
	register long  cl;

	cl = (long)b<<14 & 0x3f0000L;
	cl |= g<<6 & 0x3f00;
	cl |= r>>2;
	_remappalette(index+2, cl);
}
