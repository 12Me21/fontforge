/* Copyright (C) 2000,2001 by George Williams */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _NO_LIBTIFF
static int a_file_must_define_something=0;	/* ANSI says so */
#else
#include <dlfcn.h>
#include <tiffio.h>

#define int32 _int32
#define uint32 _uint32
#define int16 _int16
#define uint16 _uint16
#define int8 _int8
#define uint8 _uint8

#include "gdraw.h"

static void *libtiff=NULL;
static TIFF *(*_TIFFOpen)(char *, char *);
static void (*_TIFFGetField)(TIFF *, int, uint32 *);
static int (*_TIFFReadRGBAImage)(TIFF *, int, int, uint32 *, int);
static int (*_TIFFClose)(TIFF *);

static int loadtiff() {
    libtiff = dlopen("libtiff.so",RTLD_LAZY);
    if ( libtiff==NULL ) {
	GDrawIError("%s", dlerror());
return( 0 );
    }
    _TIFFOpen = dlsym(libtiff,"TIFFOpen");
    _TIFFGetField = dlsym(libtiff,"TIFFGetField");
    _TIFFReadRGBAImage = dlsym(libtiff,"TIFFReadRGBAImage");
    _TIFFClose = dlsym(libtiff,"TIFFClose");
    if ( _TIFFOpen && _TIFFGetField && _TIFFReadRGBAImage && _TIFFClose )
return( 1 );
    dlclose(libtiff);
    GDrawIError("%s", dlerror());
return( 0 );
}
    
GImage *GImageReadTiff(char *filename) {
    TIFF* tif;
    uint32 w, h, i,j;
    uint32 *ipt, *fpt;
    size_t npixels;
    uint32* raster;
    GImage *ret=NULL;
    struct _GImage *base;

    if ( libtiff==NULL )
	if ( !loadtiff())
return( NULL );

    tif = _TIFFOpen(filename, "r");

    if (tif==NULL )
return( ret );

    _TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    _TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    npixels = w * h;
    raster = (uint32*) galloc(npixels * sizeof (uint32));
    if (raster != NULL) {
	if (_TIFFReadRGBAImage(tif, w, h, raster, 0)) {
	    ret = GImageCreate(it_true,w,h);
	    if ( ret!=NULL ) {
		base = ret->u.image;
		for ( i=0; i<h; ++i ) {
		    ipt = (uint32 *) (base->data+i*base->bytes_per_line);
		    fpt = raster+(h-1-i)*w;
		    for ( j=0; j<w; ++j )
			*ipt++ = COLOR_CREATE(
				TIFFGetR(fpt[j]), TIFFGetG(fpt[j]), TIFFGetB(fpt[j]));
		}
	    }
	} 
	gfree(raster);
    }
    _TIFFClose(tif);
return( ret );
}
#endif
