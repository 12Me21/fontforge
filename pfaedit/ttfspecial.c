/* Copyright (C) 2000-2003 by George Williams */
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
#include "pfaedit.h"
#include <utype.h>
#include <ustring.h>

#include "ttf.h"

/* This file contains routines to generate non-standard true/opentype tables */
/* The first is the 'PfEd' table containing PfaEdit specific information */
/*	glyph comments & colours ... perhaps other info later */

/* ************************************************************************** */
/* *************************    The 'PfEd' table    ************************* */
/* *************************         Output         ************************* */
/* ************************************************************************** */

/* 'PfEd' table format is as follows...				 */
/* uint32  version number 0x00010000				 */
/* uint32  subtable count					 */
/* struct { uint32 tab, offset } tag/offset for first subtable	 */
/* struct { uint32 tab, offset } tag/offset for second subtable	 */
/* ...								 */

/* 'PfEd' 'fcmt' font comment subtable format			 */
/*  short version number 0					 */
/*  short string length						 */
/*  String in latin1 (ASCII is better)				 */

/* 'PfEd' 'cmnt' glyph comment subtable format			 */
/*  short  version number 0					 */
/*  short  count-of-ranges					 */
/*  struct { short start-glyph, end-glyph, short offset }	 */
/*  ...								 */
/*  foreach glyph >=start-glyph, <=end-glyph(+1)		 */
/*   uint32 offset		to glyph comment string (in UCS2) */
/*  ...								 */
/*  And one last offset pointing beyong the end of the last string to enable length calculations */
/*  String table in UCS2 (NUL terminated). All offsets from start*/
/*   of subtable */

/* 'PfEd' 'colr' glyph colour subtable				 */
/*  short  version number 0					 */
/*  short  count-of-ranges					 */
/*  struct { short start-glyph, end-glyph, uint32 colour (rgb) } */

#define MAX_SUBTABLE_TYPES	3

struct PfEd_subtabs {
    int next;
    struct {
	FILE *data;
	uint32 tag;
	uint32 offset;
    } subtabs[MAX_SUBTABLE_TYPES];
};

static void PfEd_FontComment(SplineFont *sf, struct PfEd_subtabs *pfed ) {
    FILE *fcmt;
    char *pt;

    if ( sf->comments==NULL || *sf->comments=='\0' )
return;
    pfed->subtabs[pfed->next].tag = CHR('f','c','m','t');
    pfed->subtabs[pfed->next++].data = fcmt = tmpfile();

    putshort(fcmt,0);			/* sub-table version number */
    putshort(fcmt,strlen(sf->comments));
    for ( pt = sf->comments; *pt; ++pt )
	putshort(fcmt,*pt);
    putshort(fcmt,0);
    if ( ftell(fcmt)&2 ) putshort(fcmt,0);
}

static SplineChar *SCFromCID(SplineFont *_sf,int cid) {
    int k;

    if ( _sf->subfonts==NULL )
return( _sf->chars[cid] );

    for ( k=0; k<_sf->subfontcnt; ++k )
	if ( _sf->subfonts[k]->chars[cid]!=NULL )
return( _sf->subfonts[k]->chars[cid] );

return( NULL );
}

static void PfEd_GlyphComments(SplineFont *_sf, struct PfEd_subtabs *pfed ) {
    int i, j, k, any, max, cnt, last, skipped, subcnt;
    uint32 offset;
    SplineFont *sf;
    SplineChar *sc, *sc2, *sclast;
    FILE *cmnt;
    unichar_t *upt;

    max = any = k = 0;
    do {
	sf = _sf->subfontcnt==0 ? _sf : _sf->subfonts[k++];
	if ( !any ) {
	    for ( i=0; i<sf->charcnt; ++i )
		if ( sf->chars[i]!=NULL && sf->chars[i]->ttf_glyph!=-1 &&
			sf->chars[i]->comment!=NULL ) {
		    any = true;
	    break;
		}
	}
	if ( sf->charcnt>max ) max = sf->charcnt;
    } while ( k<_sf->subfontcnt );

    if ( !any )
return;

    pfed->subtabs[pfed->next].tag = CHR('c','m','n','t');
    pfed->subtabs[pfed->next++].data = cmnt = tmpfile();

    putshort(cmnt,0);			/* sub-table version number */

    offset = 0;
    for ( j=0; j<4; ++j ) {
	cnt = 0;
	for ( i=0; i<max; ++i ) {
	    sc=SCFromCID(_sf,i);
	    if ( sc!=NULL && sc->comment!=NULL ) {
		last = i; skipped = false; sclast = sc;
		subcnt = 1;
		for ( k=i+1; k<max; ++k ) {
		    sc2 = SCFromCID(_sf,k);
		    if ( sc2==NULL || sc2->ttf_glyph==-1 )
		continue;
		    if ( sc2->comment==NULL && skipped )
		break;
		    if ( sc2->comment!=NULL ) {
			last = k;
			sclast = sc2;
			skipped = false;
		    } else
			skipped = true;
		    ++subcnt;
		}
		++cnt;
		if ( j==1 ) {
		    putshort(cmnt,sc->ttf_glyph);
		    putshort(cmnt,sclast->ttf_glyph);
		    putlong(cmnt,offset);
		    offset += sizeof(uint32)*(subcnt+1);
		} else if ( j==2 ) {
		    for ( ; i<=last; ++i ) {
			sc = SCFromCID(_sf,i);
			if ( sc==NULL || sc->ttf_glyph==-1 )
		    continue;
			if ( sc->comment==NULL )
			    putlong(cmnt,0);
			else {
			    putlong(cmnt,offset);
			    offset += sizeof(unichar_t)*(u_strlen(sc->comment)+1);
			}
		    }
		    putlong(cmnt,offset);	/* Guard data, to let us calculate the string lengths */
		} else if ( j==3 ) {
		    for ( ; i<=last; ++i ) {
			sc = SCFromCID(_sf,i);
			if ( sc==NULL || sc->ttf_glyph==-1 || sc->comment==NULL )
		    continue;
			for ( upt = sc->comment; *upt; ++upt )
			    putshort(cmnt,*upt);
			putshort(cmnt,0);
		    }
		}
		i = last;
	    }
	}
	if ( j==0 ) {
	    putshort(cmnt,cnt);
	    offset = 2*sizeof(short) + cnt*(2*sizeof(short)+sizeof(uint32));
	}
    }
    if ( ftell(cmnt) & 2 )
	putshort(cmnt,0);
}

static void PfEd_Colours(SplineFont *_sf, struct PfEd_subtabs *pfed ) {
    int i, j, k, any, max, cnt, last;
    SplineFont *sf;
    SplineChar *sc, *sc2, *sclast;
    FILE *colr;

    max = any = k = 0;
    do {
	sf = _sf->subfontcnt==0 ? _sf : _sf->subfonts[k++];
	if ( !any ) {
	    for ( i=0; i<sf->charcnt; ++i )
		if ( sf->chars[i]!=NULL && sf->chars[i]->ttf_glyph!=-1 &&
			sf->chars[i]->color!=COLOR_DEFAULT ) {
		    any = true;
	    break;
		}
	}
	if ( sf->charcnt>max ) max = sf->charcnt;
    } while ( k<_sf->subfontcnt );

    if ( !any )
return;

    pfed->subtabs[pfed->next].tag = CHR('c','o','l','r');
    pfed->subtabs[pfed->next++].data = colr = tmpfile();

    putshort(colr,0);			/* sub-table version number */
    for ( j=0; j<2; ++j ) {
	cnt = 0;
	for ( i=0; i<max; ++i ) {
	    sc=SCFromCID(_sf,i);
	    if ( sc!=NULL && sc->color!=COLOR_DEFAULT ) {
		last = i; sclast = sc;
		for ( k=i+1; k<max; ++k ) {
		    sc2 = SCFromCID(_sf,k);
		    if ( sc2==NULL || sc2->ttf_glyph==-1 )
		continue;
		    if ( sc2->color != sc->color )
		break;
		    last = k;
		    sclast = sc2;
		}
		++cnt;
		if ( j==1 ) {
		    putshort(colr,sc->ttf_glyph);
		    putshort(colr,sclast->ttf_glyph);
		    putlong(colr,sc->color);
		}
		i = last;
	    }
	}
	if ( j==0 )
	    putshort(colr,cnt);
    }
    if ( ftell(colr) & 2 )
	putshort(colr,0);
}

void pfed_dump(struct alltabs *at, SplineFont *sf) {
    struct PfEd_subtabs pfed;
    FILE *file;
    int i;
    uint32 offset;

    memset(&pfed,0,sizeof(pfed));
    if ( at->gi.flags & ttf_flag_pfed_comments ) {
	PfEd_FontComment(sf, &pfed );
	PfEd_GlyphComments(sf, &pfed );
    }
    if ( at->gi.flags & ttf_flag_pfed_colors )
	PfEd_Colours(sf, &pfed );

    if ( pfed.next==0 )
return;		/* No subtables */

    at->pfed = file = tmpfile();
    putlong(file, 0x00010000);		/* Version number */
    putlong(file, pfed.next);		/* sub-table count */
    offset = 2*sizeof(uint32) + 2*pfed.next*sizeof(uint32);
    for ( i=0; i<pfed.next; ++i ) {
	putlong(file,pfed.subtabs[i].tag);
	putlong(file,offset);
	fseek(pfed.subtabs[i].data,0,SEEK_END);
	pfed.subtabs[i].offset = offset;
	offset += ftell(pfed.subtabs[i].data);
    }
    for ( i=0; i<pfed.next; ++i ) {
	fseek(pfed.subtabs[i].data,0,SEEK_SET);
	ttfcopyfile(file,pfed.subtabs[i].data,pfed.subtabs[i].offset);
    }
    if ( ftell(file)&3 )
	GDrawIError("'PfEd' table not properly aligned");
    at->pfedlen = ftell(file);
}

/* *************************    The 'PfEd' table    ************************* */
/* *************************          Input         ************************* */

static void pfed_readfontcomment(FILE *ttf,struct ttfinfo *info,uint32 base) {
    int len;
    char *pt, *end;

    fseek(ttf,base,SEEK_SET);
    if ( getushort(ttf)!=0 )
return;			/* Bad version number */
    len = getushort(ttf);
    pt = galloc(len+1);		/* data are stored as UCS2, but currently are ASCII */
    info->fontcomments = pt;
    end = pt+len;
    while ( pt<end )
	*pt++ = getushort(ttf);
    *pt = '\0';
}

static unichar_t *ReadUnicodeStr(FILE *ttf,uint32 offset,int len) {
    unichar_t *pt, *str, *end;

    pt = str = galloc(len);
    end = str+len/2;
    fseek(ttf,offset,SEEK_SET);
    while ( pt<end )
	*pt++ = getushort(ttf);
    *pt = 0;
return( str );
}
    
static void pfed_readglyphcomments(FILE *ttf,struct ttfinfo *info,uint32 base) {
    int n, i, j;
    struct grange { int start, end; uint32 offset; } *grange;
    uint32 offset, next;

    fseek(ttf,base,SEEK_SET);
    if ( getushort(ttf)!=0 )
return;			/* Bad version number */
    n = getushort(ttf);
    grange = galloc(n*sizeof(struct grange));
    for ( i=0; i<n; ++i ) {
	grange[i].start = getushort(ttf);
	grange[i].end = getushort(ttf);
	grange[i].offset = getlong(ttf);
	if ( grange[i].start>grange[i].end || grange[i].end>info->glyph_cnt ) {
	    fprintf( stderr, "Bad glyph range specified in glyph comment subtable of PfEd table\n" );
	    grange[i].start = 1; grange[i].end = 0;
	}
    }
    for ( i=0; i<n; ++i ) {
	for ( j=grange[i].start; j<=grange[i].end; ++j ) {
	    fseek( ttf,base+grange[i].offset+(j-grange[i].start)*sizeof(uint32),SEEK_SET);
	    offset = getlong(ttf);
	    next = getlong(ttf);
	    info->chars[j]->comment = ReadUnicodeStr(ttf,base+offset,next-offset);
	}
    }
    free(grange);
}

static void pfed_readcolours(FILE *ttf,struct ttfinfo *info,uint32 base) {
    int n, i, j, start, end;
    uint32 col;

    fseek(ttf,base,SEEK_SET);
    if ( getushort(ttf)!=0 )
return;			/* Bad version number */
    n = getushort(ttf);
    for ( i=0; i<n; ++i ) {
	start = getushort(ttf);
	end = getushort(ttf);
	col = getlong(ttf);
	if ( start>end || end>info->glyph_cnt )
	    fprintf( stderr, "Bad glyph range specified in colour subtable of PfEd table\n" );
	else {
	    for ( j=start; j<=end; ++j )
		info->chars[j]->color = col;
	}
    }
}

void pfed_read(FILE *ttf,struct ttfinfo *info) {
    int n,i;
    struct tagoff { uint32 tag, offset; } tagoff[MAX_SUBTABLE_TYPES+30];

    fseek(ttf,info->pfed_start,SEEK_SET);

    if ( getlong(ttf)!=0x00010000 )
return;
    n = getlong(ttf);
    if ( n>=MAX_SUBTABLE_TYPES+30 )
	n = MAX_SUBTABLE_TYPES+30;
    for ( i=0; i<n; ++i ) {
	tagoff[i].tag = getlong(ttf);
	tagoff[i].offset = getlong(ttf);
    }
    for ( i=0; i<n; ++i ) switch ( tagoff[i].tag ) {
      case CHR('f','c','m','t'):
	pfed_readfontcomment(ttf,info,info->pfed_start+tagoff[i].offset);
      break;
      case CHR('c','m','n','t'):
	pfed_readglyphcomments(ttf,info,info->pfed_start+tagoff[i].offset);
      break;
      case CHR('c','o','l','r'):
	pfed_readcolours(ttf,info,info->pfed_start+tagoff[i].offset);
      break;
      default:
	fprintf( stderr, "Unknown subtable '%c%c%c%c' in 'PfEd' table, ignored\n",
		tagoff[i].tag>>24, (tagoff[i].tag>>16)&0xff, (tagoff[i].tag>>8)&0xff, tagoff[i].tag&0xff );
      break;
    }
}
