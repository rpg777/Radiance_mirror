/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Definitions for paths on different machines
 */

#ifdef MSDOS

#define DIRSEP		'/'
#define ISDIRSEP(c)	((c)=='/' || (c)=='\\')
#define CASEDIRSEP	case '/': case '\\'
#define PATHSEP		';'
#define MAXPATH		128
#define TEMPLATE	"c:\\tmp\\rtXXXXXX"
#define TEMPLEN		15
#define ULIBVAR		"RAYPATH"
#ifndef DEFPATH
#define DEFPATH		";c:/ray/lib"
#endif
extern char  *fixargv0();

#else
#ifdef AMIGA

#define DIRSEP		'/'
#define PATHSEP		';'
#define MAXPATH		128
#define TEMPLATE	"/tmp/rtXXXXXX"
#define TEMPLEN		13
#define ULIBVAR		"RAYPATH"
#ifndef DEFPATH
#define DEFPATH		";/ray/lib"
#endif
#define	 fixargv0(a0)	(a0)

#else

#define DIRSEP		'/'
#define PATHSEP		':'
#define MAXPATH		256
#define TEMPLATE	"/usr/tmp/rtXXXXXX"
#define TEMPLEN		17
#define ULIBVAR		"RAYPATH"
#ifndef DEFPATH
#define DEFPATH		":/usr/local/lib/ray"
#endif
#define	 fixargv0(a0)	(a0)

#endif
#endif

#ifndef ISDIRSEP
#define ISDIRSEP(c)	((c)==DIRSEP)
#endif
#ifndef CASEDIRSEP
#define CASEDIRSEP	case DIRSEP
#endif

extern char  *mktemp(), *getenv();
