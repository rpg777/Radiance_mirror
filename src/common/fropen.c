/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Find and open a Radiance library file.
 */

#include <stdio.h>

#include "paths.h"


FILE *
frlibopen(fname)		/* find file and open for reading */
register char  *fname;
{
	extern char  *strcpy(), *getlibpath();
	FILE  *fp;
	char  pname[MAXPATH];
	register char  *sp, *cp;

	if (fname == NULL)
		return(NULL);

	if (ISDIRSEP(fname[0]) || fname[0] == '.')	/* absolute path */
		return(fopen(fname, "r"));
						/* check search path */
	sp = getlibpath();
	do {
		cp = pname;
		while (*sp && (*cp = *sp++) != PATHSEP)
			cp++;
		if (cp > pname && !ISDIRSEP(cp[-1]))
			*cp++ = DIRSEP;
		strcpy(cp, fname);
		if ((fp = fopen(pname, "r")) != NULL)
			return(fp);			/* got it! */
	} while (*sp);
						/* not found */
	return(NULL);
}
