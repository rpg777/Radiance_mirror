#ifndef lint
static const char RCSid[] = "$Id: instance.c,v 2.7 2003/03/04 05:49:21 greg Exp $";
#endif
/*
 *  instance.c - routines for octree objects.
 */

#include "copyright.h"

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "instance.h"

#define  IO_ILLEGAL	(IO_FILES|IO_INFO)

static SCENE  *slist = NULL;		/* list of loaded octrees */


SCENE *
getscene(sname, flags)			/* load octree sname */
char  *sname;
int  flags;
{
	char  *pathname;
	register SCENE  *sc;

	flags &= ~IO_ILLEGAL;		/* not allowed */
	for (sc = slist; sc != NULL; sc = sc->next)
		if (!strcmp(sname, sc->name)) {
			if ((sc->ldflags & flags) == flags) {
				sc->nref++;
				return(sc);		/* loaded */
			}
			break;			/* load the rest */
		}
	if (sc == NULL) {
		sc = (SCENE *)malloc(sizeof(SCENE));
		if (sc == NULL)
			error(SYSTEM, "out of memory in getscene");
		sc->name = savestr(sname);
		sc->nref = 1;
		sc->ldflags = 0;
		sc->scube.cutree = EMPTY;
		sc->scube.cuorg[0] = sc->scube.cuorg[1] =
				sc->scube.cuorg[2] = 0.;
		sc->scube.cusize = 0.;
		sc->firstobj = sc->nobjs = 0;
		sc->next = slist;
		slist = sc;
	}
	if ((pathname = getpath(sname, getlibpath(), R_OK)) == NULL) {
		sprintf(errmsg, "cannot find octree file \"%s\"", sname);
		error(USER, errmsg);
	}
	flags &= ~sc->ldflags;		/* skip what's already loaded */
	if (flags & IO_SCENE)
		sc->firstobj = nobjects;
	readoct(pathname, flags, &sc->scube, NULL);
	if (flags & IO_SCENE)
		sc->nobjs = nobjects - sc->firstobj;
	sc->ldflags |= flags;
	return(sc);
}


INSTANCE *
getinstance(o, flags)			/* get instance structure */
register OBJREC  *o;
int  flags;
{
	register INSTANCE  *ins;

	flags &= ~IO_ILLEGAL;		/* not allowed */
	if ((ins = (INSTANCE *)o->os) == NULL) {
		if ((ins = (INSTANCE *)malloc(sizeof(INSTANCE))) == NULL)
			error(SYSTEM, "out of memory in getinstance");
		if (o->oargs.nsargs < 1)
			objerror(o, USER, "bad # of arguments");
		if (fullxf(&ins->x, o->oargs.nsargs-1,
				o->oargs.sarg+1) != o->oargs.nsargs-1)
			objerror(o, USER, "bad transform");
		if (ins->x.f.sca < 0.0) {
			ins->x.f.sca = -ins->x.f.sca;
			ins->x.b.sca = -ins->x.b.sca;
		}
		ins->obj = NULL;
		o->os = (char *)ins;
	}
	if (ins->obj == NULL || (ins->obj->ldflags & flags) != flags)
		ins->obj = getscene(o->oargs.sarg[0], flags);
	return(ins);
}


void
freescene(sc)		/* release a scene reference */
SCENE *sc;
{
	SCENE  shead;
	register SCENE  *scp;

	if (sc == NULL)
		return;
	if (sc->nref <= 0)
		error(CONSISTENCY, "unreferenced scene in freescene");
	sc->nref--;
	if (sc->nref)			/* still in use? */
		return;
	shead.next = slist;		/* else remove from our list */
	for (scp = &shead; scp->next != NULL; scp = scp->next)
		if (scp->next == sc) {
			scp->next = sc->next;
			sc->next = NULL;
			break;
		}
	if (sc->next != NULL)		/* can't be in list anymore */
		error(CONSISTENCY, "unlisted scene in freescene");
	slist = shead.next;
	freestr(sc->name);		/* free memory */
	octfree(sc->scube.cutree);
	freeobjects(sc->firstobj, sc->nobjs);
	free((void *)sc);
}


void
freeinstance(o)		/* free memory associated with instance */
OBJREC  *o;
{
	if (o->os == NULL)
		return;
	freescene((*(INSTANCE *)o->os).obj);
	free((void *)o->os);
	o->os = NULL;
}
