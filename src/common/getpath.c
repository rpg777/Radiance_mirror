/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  getpath.c - function to search for file in a list of directories
 */

#define  NULL		0

#include  <pwd.h>

extern char  *strcpy(), *strcat(), *getenv();
extern struct passwd  *getpwnam();


char *
getpath(fname, searchpath, mode)	/* expand fname, return full path */
register char  *fname;
register char  *searchpath;
int  mode;
{
	static char  pname[256];
	struct passwd  *pwent;
	register char  *cp;

	if (fname == NULL)
		return(NULL);

	switch (*fname) {
	case '/':				/* relative to root */
	case '.':				/* relative to cwd */
		strcpy(pname, fname);
		return(pname);
	case '~':				/* relative to home directory */
		fname++;
		if (*fname == '\0' || *fname == '/') {		/* ours */
			if ((cp = getenv("HOME")) == NULL)
				return(NULL);
			strcpy(pname, cp);
			strcat(pname, fname);
			return(pname);
		}
		cp = pname;					/* user */
		do
			*cp++ = *fname++;
		while (*fname && *fname != '/');
		*cp = '\0';
		if ((pwent = getpwnam(pname)) == NULL)
			return(NULL);
		strcpy(pname, pwent->pw_dir);
		strcat(pname, fname);
		return(pname);
	}

	if (searchpath == NULL) {			/* don't search */
		strcpy(pname, fname);
		return(pname);
	}
							/* check search path */
	do {
		cp = pname;
		while (*searchpath && (*cp = *searchpath++) != ':')
			cp++;
		if (cp > pname && cp[-1] != '/')
			*cp++ = '/';
		strcpy(cp, fname);
		if (access(pname, mode) == 0)		/* file accessable? */
			return(pname);
	} while (*searchpath);
							/* not found */
	return(NULL);
}
