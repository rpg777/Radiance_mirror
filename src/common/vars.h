/* RCSid $Id: vars.h,v 2.4 2003/02/25 02:47:22 greg Exp $ */
/*
 *  Header for programs that load variable files.
 */

#include "copyright.h"

typedef struct {
	char	*name;		/* variable name */
	short	nick;		/* # characters required for nickname */
	short	nass;		/* # assignments made */
	char	*value;		/* assigned value(s) */
	void	(*fixval)();	/* assignment checking function */
} VARIABLE;		/* a variable-value pair */

/**** The following variables should be declared by calling program ****/

extern int	NVARS;		/* total number of variables */

extern VARIABLE	vv[];		/* variable-value pairs */

extern char	*progname;	/* global argv[0] from main */

extern int	nowarn;		/* global boolean to turn warnings off */

/**** The rest is declared in loadvars.c ****/

extern VARIABLE	*matchvar();
extern char	*nvalue();

#define UPPER(c)	((c)&~0x20)	/* ASCII trick */

#define vnam(vc)	(vv[vc].name)
#define vdef(vc)	(vv[vc].nass)
#define vval(vc)	(vv[vc].value)
#define vint(vc)	atoi(vval(vc))
#define vflt(vc)	atof(vval(vc))
#define vlet(vc)	UPPER(vval(vc)[0])
#define vscale		vlet
#define vbool(vc)	(vlet(vc)=='T')

#define HIGH		'H'
#define MEDIUM		'M'
#define LOW		'L'

#ifdef NOPROTO

extern void	loadvars();
extern int	setvariable();
extern VARIABLE	*matchvar();
extern char	*nvalue();
extern void	checkvalues();
extern void	onevalue();
extern void	catvalues();
extern int	badmatch();
extern void	boolvalue();
extern void	qualvalue();
extern void	intvalue();
extern void	fltvalue();
extern void	printvars();

#else

extern void	loadvars(char *rfname);
extern int	setvariable(char *ass, VARIABLE *(*mv)());
extern VARIABLE	*matchvar(char *nam);
extern char	*nvalue(int vn, int n);
extern void	checkvalues(void);
extern void	onevalue(VARIABLE *vp);
extern void	catvalues(VARIABLE *vp);
extern int	badmatch(char *tv, char *cv);
extern void	boolvalue(VARIABLE *vp);
extern void	qualvalue(VARIABLE *vp);
extern void	intvalue(VARIABLE *vp);
extern void	fltvalue(VARIABLE *vp);
extern void	printvars(FILE *fp);

#endif
