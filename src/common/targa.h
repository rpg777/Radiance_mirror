/* RCSid: $Id: targa.h,v 2.2 2003/02/22 02:07:22 greg Exp $ */
/*
 *  tardev.h - header file for reading and writing Targa format files.
 */
/* ====================================================================
 * The Radiance Software License, Version 1.0
 *
 * Copyright (c) 1990 - 2002 The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *           if any, must include the following acknowledgment:
 *             "This product includes Radiance software
 *                 (http://radsite.lbl.gov/)
 *                 developed by the Lawrence Berkeley National Laboratory
 *               (http://www.lbl.gov/)."
 *       Alternately, this acknowledgment may appear in the software itself,
 *       if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Radiance," "Lawrence Berkeley National Laboratory"
 *       and "The Regents of the University of California" must
 *       not be used to endorse or promote products derived from this
 *       software without prior written permission. For written
 *       permission, please contact radiance@radsite.lbl.gov.
 *
 * 5. Products derived from this software may not be called "Radiance",
 *       nor may "Radiance" appear in their name, without prior written
 *       permission of Lawrence Berkeley National Laboratory.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.   IN NO EVENT SHALL Lawrence Berkeley National Laboratory OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of Lawrence Berkeley National Laboratory.   For more
 * information on Lawrence Berkeley National Laboratory, please see
 * <http://www.lbl.gov/>.
 */

			/* header structure adapted from tardev.h */
struct hdStruct {
	char textSize;			/* size of info. line ( < 256) */
	char mapType;			/* color map type */
	char dataType;			/* data type */
	int  mapOrig;			/* first color index */
	int  mapLength;			/* length of file map */
	char CMapBits;			/* bits per map entry */
	int  XOffset;			/* picture offset */
	int  YOffset;
	int  x;				/* picture size */
	int  y;
	int  dataBits;			/* bits per pixel */
	int  imType;			/* image descriptor byte */
};

#define  IM_NODATA	0		/* no data included */
#define  IM_CMAP	1		/* color-mapped */
#define  IM_RGB		2		/* straight RGB */
#define  IM_MONO	3		/* straight monochrome */
#define  IM_CCMAP	9		/* compressed color-mapped */
#define  IM_CRGB	10		/* compressed RGB */
#define  IM_CMONO	11		/* compressed monochrome */

				/* color map types */
#define  CM_NOMAP	0		/* no color map */
#define  CM_HASMAP	1		/* has color map */

#define  bits_bytes(n)	(((n)+7)>>3)	/* number of bits to number of bytes */
