#ifndef lint
static const char RCSid[] = "$Id: writeoct.c,v 2.4 2003/03/14 21:27:46 greg Exp $";
#endif
/*
 *  writeoct.c - routines for writing octree information to stdout.
 *
 *     7/30/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

static int  oputint(), oputstr(), puttree();


writeoct(store, scene, ofn)		/* write octree structures to stdout */
int  store;
CUBE  *scene;
char  *ofn[];
{
	char  sbuf[64];
	int  i;
					/* write format number */
	oputint((long)(OCTMAGIC+sizeof(OBJECT)), 2);

	if (!(store & IO_BOUNDS))
		return;
					/* write boundaries */
	for (i = 0; i < 3; i++) {
		sprintf(sbuf, "%.12g", scene->cuorg[i]);
		oputstr(sbuf);
	}
	sprintf(sbuf, "%.12g", scene->cusize);
	oputstr(sbuf);
					/* write object file names */
	if (store & IO_FILES)
		for (i = 0; ofn[i] != NULL; i++)
			oputstr(ofn[i]);
	oputstr("");
					/* write number of objects */
	oputint((long)nobjects, sizeof(OBJECT));

	if (!(store & IO_TREE))
		return;
					/* write the octree */
	puttree(scene->cutree);

	if (store & IO_FILES || !(store & IO_SCENE))
		return;
					/* write the scene */
	writescene(0, nobjects, stdout);
}


static
oputstr(s)			/* write null-terminated string to stdout */
register char  *s;
{
	putstr(s, stdout);
	if (ferror(stdout))
		error(SYSTEM, "write error in putstr");
}


static
putfullnode(fn)			/* write out a full node */
OCTREE  fn;
{
	OBJECT  oset[MAXSET+1];
	register int  i;

	objset(oset, fn);
	for (i = 0; i <= oset[0]; i++)
		oputint((long)oset[i], sizeof(OBJECT));
}


static
oputint(i, siz)			/* write a siz-byte integer to stdout */
register long  i;
register int  siz;
{
	putint(i, siz, stdout);
	if (ferror(stdout))
		error(SYSTEM, "write error in putint");
}


static
oputflt(f)			/* put out floating point number */
double  f;
{
	putflt(f, stdout);
	if (ferror(stdout))
		error(SYSTEM, "write error in putflt");
}


static
puttree(ot)			/* write octree to stdout in pre-order form */
register OCTREE  ot;
{
	register int  i;
	
	if (istree(ot)) {
		putc(OT_TREE, stdout);		/* indicate tree */
		for (i = 0; i < 8; i++)		/* write tree */
			puttree(octkid(ot, i));
	} else if (isfull(ot)) {
		putc(OT_FULL, stdout);		/* indicate fullnode */
		putfullnode(ot);		/* write fullnode */
	} else
		putc(OT_EMPTY, stdout);		/* indicate empty */
}
