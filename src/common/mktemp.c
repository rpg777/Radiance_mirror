/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Replacement mktemp(3) function for systems without
 */

#define  NULL		0


char *
mktemp(template)		/* make a unique filename from template */
char	*template;
{
	register char	*tb, *te, *p;
	int	pid;
					/* find string of 6 (opt) X's */
	for (te = template; *te; te++)
		;
	while (te > template && te[-1] != 'X')
		te--;
	if (te == template)
		return(template);	/* no X's! */
	for (tb = te; tb > template && tb[-1] == 'X'; tb--)
		;
	if (te-tb > 6)			/* only need 6 chars */
		tb = te-6;
	pid = getpid();			/* 5 (opt) chars of pid */
	for (p = te-2; p >= tb; p--) {
		*p = pid%10 + '0';
		pid /= 10;
	}
	p = te-1;			/* final character */
	for (*p = 'a'; *p <= 'z'; (*p)++)
		if (access(template, 0) == -1)
			return(template);	/* found unique name */
	return(NULL);			/* failure! */
}
