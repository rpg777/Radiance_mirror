/* RCSid: $Id: rhdisp.h,v 3.13 2003/02/22 02:07:24 greg Exp $ */
/*
 * Header for holodeck display drivers.
 */

				/* display requests */
#define DR_NOOP		0		/* to release from vain DR_ATTEN */
#define DR_BUNDLE	1		/* lone bundle request */
#define DR_ATTEN	2		/* request for attention */
#define DR_SHUTDOWN	3		/* shutdown request */
#define DR_NEWSET	4		/* new bundle set */
#define DR_ADDSET	5		/* add to current set */
#define DR_ADJSET	6		/* adjust set quantities */
#define DR_DELSET	7		/* delete from current set */
#define DR_KILL		8		/* kill rtrace process(es) */
#define DR_RESTART	9		/* restart rtrace */
#define DR_CLOBBER	10		/* clobber holodeck */
#define DR_VIEWPOINT	11		/* set desired eye position */

				/* server responses */
#define DS_BUNDLE	32		/* computed bundle */
#define DS_ACKNOW	33		/* acknowledge request for attention */
#define DS_SHUTDOWN	34		/* end process and close connection */
#define DS_ADDHOLO	35		/* register new holodeck */
#define DS_STARTIMM	36		/* begin immediate bundle set */
#define DS_ENDIMM	37		/* end immediate bundle set */
#define DS_OUTSECT	38		/* render from outside sections */
#define DS_EYESEP	39		/* eye separation distance */

/*
 * Normally, the server channel has priority, with the display process
 * checking it frequently for new data.  However, when the display process
 * makes a request for attention (DR_ATTEN), the server will finish its
 * current operations and flush its buffers, sending an acknowledge
 * message (DS_ACKNOW) when it's done.  This then allows the
 * display process to send a long request to the holodeck server.
 * Priority returns to normal after the following request.
 */

#ifndef BIGREQSIZ
#define BIGREQSIZ	512		/* big request size (bytes) */
#endif

typedef struct {
	int2	type;		/* message type */
	int4	nbytes;		/* number of additional bytes */
} MSGHEAD;		/* message head */

/*
 * The display process is started with three arguments.  The first argument
 * is the short name of the holodeck file, appropriate for window naming, etc.
 * The second and third arguments are the file descriptor numbers assigned to
 * the server's standard output and input, respectively.  The stdin and stdout
 * of the display process are used for normal communication with the server,
 * and are connected to pipes going each way.  It is entirely appropriate
 * for the display process to borrow the server's stdin and stdout for reading
 * and writing user commands from the list in rhdriver.h.  If standard input
 * is not available for reading, then a descriptor of -1 will be passed.
 * The standard output will always be available for writing, though it
 * may go to /dev/null.
 */
