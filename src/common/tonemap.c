/* Copyright (c) 1997 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Tone mapping functions.
 * See tonemap.h for detailed function descriptions.
 */

#include	<stdio.h>
#include	<math.h>
#include	"tmprivat.h"
#include	"tmerrmsg.h"

#define	exp10(x)	exp(M_LN10*(x))

struct tmStruct	*tmTop = NULL;		/* current tone mapping stack */


int
tmErrorReturn(func, err)		/* error return (with message) */
char	*func;
int	err;
{
	if (tmTop != NULL && tmTop->flags & TM_F_NOSTDERR)
		return(err);
	fputs(func, stderr);
	fputs(": ", stderr);
	fputs(tmErrorMessage[err], stderr);
	fputs("!\n", stderr);
	return(err);
}


struct tmStruct *
tmInit(flags, monpri, gamval)		/* initialize new tone mapping */
int	flags;
RGBPRIMP	monpri;
double	gamval;
{
	static char	funcName[] = "tmInit";
	COLORMAT	cmat;
	register struct tmStruct	*tmnew;
	register int	i;
						/* allocate structure */
	tmnew = (struct tmStruct *)malloc(sizeof(struct tmStruct));
	if (tmnew == NULL)
		return(NULL);

	tmnew->flags = flags & ~TM_F_UNIMPL;
						/* set monitor transform */
	if (monpri == NULL || monpri == stdprims || tmnew->flags & TM_F_BW) {
		tmnew->monpri = stdprims;
		tmnew->clf[RED] = rgb2xyzmat[1][0];
		tmnew->clf[GRN] = rgb2xyzmat[1][1];
		tmnew->clf[BLU] = rgb2xyzmat[1][2];
	} else {
		comprgb2xyzmat(cmat, tmnew->monpri=monpri);
		tmnew->clf[RED] = cmat[1][0];
		tmnew->clf[GRN] = cmat[1][1];
		tmnew->clf[BLU] = cmat[1][2];
	}
	tmnew->clfb[RED] = 256.*tmnew->clf[RED] + .5;
	tmnew->clfb[GRN] = 256.*tmnew->clf[GRN] + .5;
	tmnew->clfb[BLU] = 256.*tmnew->clf[BLU] + .5;
	tmnew->clfb[EXP] = COLXS;
						/* set gamma value */
	if (gamval < MINGAM)
		tmnew->mongam = DEFGAM;
	else
		tmnew->mongam = gamval;
	for (i = TM_GAMTSZ; i--; )
		tmnew->gamb[i] = 256.*pow((i+.5)/TM_GAMTSZ, 1./tmnew->mongam);
						/* set input transform */
	tmnew->inppri = tmnew->monpri;
	tmnew->cmat[0][0] = tmnew->cmat[1][1] = tmnew->cmat[2][2] =
			tmnew->inpsf = WHTEFFICACY;
	tmnew->inpsfb = TM_BRTSCALE*log(tmnew->inpsf) + .5;
	tmnew->cmat[0][1] = tmnew->cmat[0][2] = tmnew->cmat[1][0] =
	tmnew->cmat[1][2] = tmnew->cmat[2][0] = tmnew->cmat[2][1] = 0.;
	tmnew->flags &= ~TM_F_NEEDMAT;
	tmnew->brmin = tmnew->brmax = 0;
	tmnew->histo = NULL;
	tmnew->lumap = NULL;
	tmnew->tmprev = NULL;

	tmnew->flags |= TM_F_INITED;
						/* make it current */
	tmnew->tmprev = tmTop;
	return(tmTop = tmnew);
}


int
tmSetSpace(pri, sf)		/* set input color space for conversions */
RGBPRIMP	pri;
double	sf;
{
	static char	funcName[] = "tmSetSpace";
	register int	i, j;
						/* error check */
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (sf <= 1e-12)
		returnErr(TM_E_ILLEGAL);
						/* check if no change */
	if (pri == tmTop->inppri && FEQ(sf, tmTop->inpsf))
		returnOK;
	tmTop->inppri = pri;			/* let's set it */
	tmTop->inpsf = sf;
	tmTop->inpsfb = TM_BRTSCALE*log(sf) + (sf>=1. ? .5 : -.5);

	if (tmTop->flags & TM_F_BW) {		/* color doesn't matter */
		tmTop->monpri = tmTop->inppri;		/* eliminate xform */
		if (tmTop->inppri == TM_XYZPRIM) {
			tmTop->clf[CIEX] = tmTop->clf[CIEZ] = 0.;
			tmTop->clf[CIEY] = 1.;
			tmTop->clfb[CIEX] = tmTop->clfb[CIEZ] = 0;
			tmTop->clfb[CIEY] = 255;
		} else {
			comprgb2xyzmat(tmTop->cmat, tmTop->monpri);
			tmTop->clf[RED] = tmTop->cmat[1][0];
			tmTop->clf[GRN] = tmTop->cmat[1][1];
			tmTop->clf[BLU] = tmTop->cmat[1][2];
		}
		tmTop->cmat[0][0] = tmTop->cmat[1][1] = tmTop->cmat[2][2] =
				tmTop->inpsf;
		tmTop->cmat[0][1] = tmTop->cmat[0][2] = tmTop->cmat[1][0] =
		tmTop->cmat[1][2] = tmTop->cmat[2][0] = tmTop->cmat[2][1] = 0.;

	} else if (tmTop->inppri == TM_XYZPRIM)	/* input is XYZ */
		compxyz2rgbmat(tmTop->cmat, tmTop->monpri);

	else {					/* input is RGB */
		if (tmTop->inppri != tmTop->monpri &&
				PRIMEQ(tmTop->inppri, tmTop->monpri))
			tmTop->inppri = tmTop->monpri;	/* no xform */
		comprgb2rgbmat(tmTop->cmat, tmTop->inppri, tmTop->monpri);
	}
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			tmTop->cmat[i][j] *= tmTop->inpsf;
	if (tmTop->inppri != tmTop->monpri)
		tmTop->flags |= TM_F_NEEDMAT;
	else
		tmTop->flags &= ~TM_F_NEEDMAT;
	returnOK;
}


void
tmClearHisto()			/* clear current histogram */
{
	if (tmTop == NULL || tmTop->histo == NULL)
		return;
	free((char *)tmTop->histo);
	tmTop->histo = NULL;
}


int
tmCvColors(ls, cs, scan, len)		/* convert float colors */
TMbright	*ls;
BYTE	*cs;
COLOR	*scan;
int	len;
{
	static char	funcName[] = "tmCvColors";
	COLOR	cmon;
	double	lum, slum;
	register double	d;
	register int	i;

	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
	if (ls == NULL | scan == NULL | len <= 0)
		returnErr(TM_E_ILLEGAL);
	for (i = len; i--; ) {
		if (tmTop->flags & TM_F_NEEDMAT)	/* get monitor RGB */
			colortrans(cmon, tmTop->cmat, scan[i]);
		else {
			cmon[RED] = tmTop->inpsf*scan[i][RED];
			cmon[GRN] = tmTop->inpsf*scan[i][GRN];
			cmon[BLU] = tmTop->inpsf*scan[i][BLU];
		}
							/* world luminance */
		lum =	tmTop->clf[RED]*cmon[RED] +
			tmTop->clf[GRN]*cmon[GRN] +
			tmTop->clf[BLU]*cmon[BLU] ;
							/* check range */
		if (clipgamut(cmon, lum, CGAMUT_LOWER, cblack, cwhite))
			lum =	tmTop->clf[RED]*cmon[RED] +
				tmTop->clf[GRN]*cmon[GRN] +
				tmTop->clf[BLU]*cmon[BLU] ;
		if (lum < MINLUM) {
			ls[i] = MINBRT-1;		/* bogus value */
			lum = MINLUM;
		} else {
			d = TM_BRTSCALE*log(lum);	/* encode it */
			ls[i] = d>0. ? (int)(d+.5) : (int)(d-.5);
		}
		if (cs == TM_NOCHROM)			/* no color? */
			continue;
		if (tmTop->flags & TM_F_MESOPIC && lum < LMESUPPER) {
			slum = scotlum(cmon);		/* mesopic adj. */
			if (lum < LMESLOWER)
				cmon[RED] = cmon[GRN] = cmon[BLU] = slum;
			else {
				d = (lum - LMESLOWER)/(LMESUPPER - LMESLOWER);
				if (tmTop->flags & TM_F_BW)
					cmon[RED] = cmon[GRN] =
							cmon[BLU] = d*lum;
				else
					scalecolor(cmon, d);
				d = (1.-d)*slum;
				cmon[RED] += d;
				cmon[GRN] += d;
				cmon[BLU] += d;
			}
		} else if (tmTop->flags & TM_F_BW) {
			cmon[RED] = cmon[GRN] = cmon[BLU] = lum;
		}
		d = tmTop->clf[RED]*cmon[RED]/lum;
		/* cs[3*i  ] = d>.999 ? 255 : 256.*pow(d, 1./tmTop->mongam); */
		cs[3*i  ] = d>.999 ? 255 : tmTop->gamb[(int)(d*TM_GAMTSZ)];
		d = tmTop->clf[GRN]*cmon[GRN]/lum;
		/* cs[3*i+1] = d>.999 ? 255 : 256.*pow(d, 1./tmTop->mongam); */
		cs[3*i+1] = d>.999 ? 255 : tmTop->gamb[(int)(d*TM_GAMTSZ)];
		d = tmTop->clf[BLU]*cmon[BLU]/lum;
		/* cs[3*i+2] = d>.999 ? 255 : 256.*pow(d, 1./tmTop->mongam); */
		cs[3*i+2] = d>.999 ? 255 : tmTop->gamb[(int)(d*TM_GAMTSZ)];
	}
	returnOK;
}


int
tmAddHisto(ls, len, wt)			/* add values to histogram */
register TMbright	*ls;
int	len;
int	wt;
{
	static char	funcName[] = "tmAddHisto";
	int	sum, oldorig, oldlen, horig, hlen;
	register int	i, j;

	if (len <= 0)
		returnErr(TM_E_ILLEGAL);
	if (tmTop == NULL)
		returnErr(TM_E_TMINVAL);
						/* first, grow limits */
	if (tmTop->histo == NULL) {
		for (i = len; i-- && ls[i] < MINBRT; )
			;
		if (i < 0)
			returnOK;
		tmTop->brmin = tmTop->brmax = ls[i];
		oldlen = 0;
	} else {
		oldorig = (tmTop->brmin-MINBRT)/HISTEP;
		oldlen = (tmTop->brmax-MINBRT)/HISTEP + 1 - oldorig;
	}
	for (i = len; i--; ) {
		if ((j = ls[i]) < MINBRT)
			continue;
		if (j < tmTop->brmin)
			tmTop->brmin = j;
		else if (j > tmTop->brmax)
			tmTop->brmax = j;
	}
	horig = (tmTop->brmin-MINBRT)/HISTEP;
	hlen = (tmTop->brmax-MINBRT)/HISTEP + 1 - horig;
	if (hlen > oldlen) {			/* (re)allocate histogram */
		register int	*newhist = (int *)calloc(hlen, sizeof(int));
		if (newhist == NULL)
			returnErr(TM_E_NOMEM);
		if (oldlen) {			/* copy and free old */
			for (i = oldlen, j = i+oldorig-horig; i; )
				newhist[--j] = tmTop->histo[--i];
			free((char *)tmTop->histo);
		}
		tmTop->histo = newhist;
		if (tmTop->lumap != NULL) {	/* invalid tone map */
			free((char *)tmTop->lumap);
			tmTop->lumap = NULL;
		}
	}
	if (wt == 0)
		returnOK;
	for (i = len; i--; )			/* add in new counts */
		if (ls[i] >= MINBRT)
			tmTop->histo[ (ls[i]-MINBRT)/HISTEP - horig ] += wt;
	returnOK;
}


static double
htcontrs(La)		/* human threshold contrast sensitivity, dL(La) */
double	La;
{
	double	l10La, l10dL;
				/* formula taken from Ferwerda et al. [SG96] */
	if (La < 1.148e-4)
		return(1.38e-3);
	l10La = log10(La);
	if (l10La < -1.44)		/* rod response regime */
		l10dL = pow(.405*l10La + 1.6, 2.18) - 2.86;
	else if (l10La < -.0184)
		l10dL = l10La - .395;
	else if (l10La < 1.9)		/* cone response regime */
		l10dL = pow(.249*l10La + .65, 2.7) - .72;
	else
		l10dL = l10La - 1.255;

	return(exp10(l10dL));
}


int
tmComputeMapping(gamval, Lddyn, Ldmax)
double	gamval;
double	Lddyn;
double	Ldmax;
{
	static char	funcName[] = "tmComputeMapping";
	int	*histo;
	float	*cumf;
	int	brt0, histlen, histot, threshold, ceiling, trimmings;
	double	logLddyn, Ldmin, Ldavg, Lwavg, Tr, Lw, Ld;
	int4	sum;
	register double	d;
	register int	i, j;

	if (tmTop == NULL || tmTop->histo == NULL)
		returnErr(TM_E_TMINVAL);
					/* check arguments */
	if (Lddyn < MINLDDYN) Lddyn = DEFLDDYN;
	if (Ldmax < MINLDMAX) Ldmax = DEFLDMAX;
	if (gamval < MINGAM) gamval = tmTop->mongam;
					/* compute handy values */
	Ldmin = Ldmax/Lddyn;
	logLddyn = log(Lddyn);
	Ldavg = sqrt(Ldmax*Ldmin);
	i = (tmTop->brmin-MINBRT)/HISTEP;
	brt0 = MINBRT + HISTEP/2 + i*HISTEP;
	histlen = (tmTop->brmax-MINBRT)/HISTEP + 1 - i;
					/* histogram total and mean */
	histot = 0; sum = 0;
	j = brt0 + histlen*HISTEP;
	for (i = histlen; i--; ) {
		histot += tmTop->histo[i];
		sum += (j -= HISTEP) * tmTop->histo[i];
	}
	threshold = histot*.025 + .5;
	if (threshold < 4)
		returnErr(TM_E_TMFAIL);
	Lwavg = tmLuminance( (double)sum / histot );
	if (!(tmTop->flags & TM_F_LINEAR)) {	/* clamp histogram */
		histo = (int *)malloc(histlen*sizeof(int));
		cumf = (float *)malloc((histlen+1)*sizeof(float));
		if (histo == NULL | cumf == NULL)
			returnErr(TM_E_NOMEM);
		for (i = histlen; i--; )	/* make malleable copy */
			histo[i] = tmTop->histo[i];
		do {				/* iterate to solution */
			sum = 0;		/* cumulative probability */
			for (i = 0; i < histlen; i++) {
				cumf[i] = (double)sum/histot;
				sum += histo[i];
			}
			cumf[i] = 1.;
			Tr = histot * (double)(tmTop->brmax - tmTop->brmin) /
				((double)histlen*TM_BRTSCALE) / logLddyn;
			ceiling = Tr + 1.;
			trimmings = 0;			/* clip to envelope */
			for (i = histlen; i--; ) {
				if (tmTop->flags & TM_F_HCONTR) {
					Lw = tmLuminance(brt0 + i*HISTEP);
					Ld = Ldmin * exp( logLddyn *
						.5*(cumf[i]+cumf[i+1]) );
					ceiling = Tr * (htcontrs(Ld) * Lw) /
						(htcontrs(Lw) * Ld) + 1.;
				}
				if (histo[i] > ceiling) {
					trimmings += histo[i] - ceiling;
					histo[i] = ceiling;
				}
			}
		} while ((histot -= trimmings) > threshold &&
						trimmings > threshold);
	}
						/* allocate luminance map */
	if (tmTop->lumap == NULL) {
		tmTop->lumap = (unsigned short *)malloc(
			(tmTop->brmax-tmTop->brmin+1)*sizeof(unsigned short) );
		if (tmTop->lumap == NULL)
			returnErr(TM_E_NOMEM);
	}
	if (tmTop->flags & TM_F_LINEAR || histot <= threshold) {
						/* linear tone mapping */
		if (tmTop->flags & TM_F_HCONTR)
			d = htcontrs(Ldavg) / htcontrs(Lwavg);
		else
			d = Ldavg / Lwavg;
		d = log(d/Ldmax);
		for (i = tmTop->brmax-tmTop->brmin+1; i--; )
			tmTop->lumap[i] = 256. * exp(
				( d + (tmTop->brmin+i)/(double)TM_BRTSCALE )
				/ gamval );
	} else {
						/* histogram adjustment */
		for (i = tmTop->brmax-tmTop->brmin+1; i--; ) {
			j = d = (double)i/(tmTop->brmax-tmTop->brmin)*histlen;
			d -= (double)j;
			Ld = Ldmin*exp(logLddyn*((1.-d)*cumf[j]+d*cumf[j+1]));
			d = (Ld - Ldmin)/(Ldmax - Ldmin);
			tmTop->lumap[i] = 256.*pow(d, 1./gamval);
		}
	}
	if (!(tmTop->flags & TM_F_LINEAR)) {
		free((char *)histo);
		free((char *)cumf);
	}
	returnOK;
}


int
tmMapPixels(ps, ls, cs, len)
register BYTE	*ps;
TMbright	*ls;
register BYTE	*cs;
int	len;
{
	static char	funcName[] = "tmMapPixels";
	int	rdiv, gdiv, bdiv;
	register int	li, pv;

	if (tmTop == NULL || tmTop->lumap == NULL)
		returnErr(TM_E_TMINVAL);
	if (ps == NULL | ls == NULL | len <= 0)
		returnErr(TM_E_ILLEGAL);
	rdiv = tmTop->gamb[((int4)TM_GAMTSZ*tmTop->clfb[RED])>>8];
	gdiv = tmTop->gamb[((int4)TM_GAMTSZ*tmTop->clfb[GRN])>>8];
	bdiv = tmTop->gamb[((int4)TM_GAMTSZ*tmTop->clfb[BLU])>>8];
	while (len--) {
		if ((li = *ls++) < tmTop->brmin)
			li = tmTop->brmin;
		else if (li > tmTop->brmax)
			li = tmTop->brmax;
		li = tmTop->lumap[li - tmTop->brmin];
		if (cs == TM_NOCHROM)
			*ps++ = li>255 ? 255 : li;
		else {
			pv = *cs++ * li / rdiv;
			*ps++ = pv>255 ? 255 : pv;
			pv = *cs++ * li / gdiv;
			*ps++ = pv>255 ? 255 : pv;
			pv = *cs++ * li / bdiv;
			*ps++ = pv>255 ? 255 : pv;
		}
	}
	returnOK;
}


struct tmStruct *
tmPop()				/* pop top tone mapping off stack */
{
	register struct tmStruct	*tms;

	if ((tms = tmTop) != NULL)
		tmTop = tms->tmprev;
	return(tms);
}


int
tmPull(tms)			/* pull a tone mapping from stack */
register struct tmStruct	*tms;
{
	register struct tmStruct	*tms2;
					/* special cases first */
	if (tms == NULL | tmTop == NULL)
		return(0);
	if (tms == tmTop) {
		tmTop = tms->tmprev;
		tms->tmprev = NULL;
		return(1);
	}
	for (tms2 = tmTop; tms2->tmprev != NULL; tms2 = tms2->tmprev)
		if (tms == tms2->tmprev) {	/* remove it */
			tms2->tmprev = tms->tmprev;
			tms->tmprev = NULL;
			return(1);
		}
	return(0);			/* not found on stack */
}


struct tmStruct *
tmDup()				/* duplicate top tone mapping */
{
	int	len;
	register int	i;
	register struct tmStruct	*tmnew;

	if (tmTop == NULL)		/* anything to duplicate? */
		return(NULL);
	tmnew = (struct tmStruct *)malloc(sizeof(struct tmStruct));
	if (tmnew == NULL)
		return(NULL);
	*tmnew = *tmTop;		/* copy everything */
	if (tmnew->histo != NULL) {	/* duplicate histogram */
		len = (tmnew->brmax-MINBRT)/HISTEP + 1 -
				(tmnew->brmin-MINBRT)/HISTEP;
		tmnew->histo = (int *)malloc(len*sizeof(int));
		if (tmnew->histo != NULL)
			for (i = len; i--; )
				tmnew->histo[i] = tmTop->histo[i];
	}
	if (tmnew->lumap != NULL) {	/* duplicate luminance mapping */
		len = tmnew->brmax-tmnew->brmin+1;
		tmnew->lumap = (unsigned short *)malloc(
						len*sizeof(unsigned short) );
		if (tmnew->lumap != NULL)
			for (i = len; i--; )
				tmnew->lumap[i] = tmTop->lumap[i];
	}
	tmnew->tmprev = tmTop;		/* make copy current */
	return(tmTop = tmnew);
}


int
tmPush(tms)			/* push tone mapping on top of stack */
register struct tmStruct	*tms;
{
	static char	funcName[] = "tmPush";
					/* check validity */
	if (tms == NULL || !(tms->flags & TM_F_INITED))
		returnErr(TM_E_ILLEGAL);
	if (tms == tmTop)		/* check necessity */
		returnOK;
					/* pull if already in stack */
	(void)tmPull(tms);
					/* push it on top */
	tms->tmprev = tmTop;
	tmTop = tms;
	returnOK;
}


void
tmDone(tms)			/* done with tone mapping -- destroy it */
register struct tmStruct	*tms;
{
					/* NULL arg. is equiv. to tmTop */
	if (tms == NULL && (tms = tmTop) == NULL)
		return;
					/* take out of stack if present */
	(void)tmPull(tms);
					/* free tables */
	if (tms->histo != NULL)
		free((char *)tms->histo);
	if (tms->lumap != NULL)
		free((char *)tms->lumap);
	tms->flags = 0;
	free((char *)tms);		/* free basic structure */
}
