/* Copyright (C) 2000-2004 by George Williams */
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
#include "splinefont.h"
#include "gdraw.h"
#include "ustring.h"
#include <utype.h>
#include <unistd.h>
#include <locale.h>
#include <gwidget.h>

static const char *charset_names[] = {
    "custom",
    "iso8859_1", "iso8859_2", "iso8859_3", "iso8859_4", "iso8859_5",
    "iso8859_6", "iso8859_7", "iso8859_8", "iso8859_9", "iso8859_10",
    "iso8859_11", "iso8859_13", "iso8859_14", "iso8859_15",
    "koi8_r",
    "jis201",
    "win", "mac", "symbol", "zapfding", "adobestandard",
    "jis208", "jis212", "ksc5601", "gb2312", "big5", "big5hkscs", "johab",
    "unicode", "unicode4", "sjis", "wansung", "gb2312pk", NULL};

static const char *unicode_interp_names[] = { "none", "adobe", "greek",
    "japanese", "tradchinese", "simpchinese", "korean", NULL };

signed char inbase64[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static void utf7_encode(FILE *sfd,long ch) {
static char base64[64] = {
 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    putc(base64[(ch>>18)&0x3f],sfd);
    putc(base64[(ch>>12)&0x3f],sfd);
    putc(base64[(ch>>6)&0x3f],sfd);
    putc(base64[ch&0x3f],sfd);
}

static void SFDDumpUTF7Str(FILE *sfd, const unichar_t *str) {
    int ch, prev_cnt=0, prev=0, in=0;

    putc('"',sfd);
    if ( str!=NULL ) while ( (ch = *str++)!='\0' ) {
	if ( ch<127 && ch!='\n' && ch!='\r' && ch!='\\' && ch!='~' &&
		ch!='+' && ch!='=' && ch!='"' ) {
	    if ( prev_cnt!=0 ) {
		prev<<= (prev_cnt==1?16:8);
		utf7_encode(sfd,prev);
		prev_cnt=prev=0;
	    }
	    if ( in ) {
		if ( inbase64[ch]!=-1 )
		    putc('-',sfd);
		in = 0;
	    }
	    putc(ch,sfd);
	} else if ( ch=='+' && !in ) {
	    putc('+',sfd);
	    putc('-',sfd);
	} else if ( prev_cnt== 0 ) {
	    if ( !in ) {
		putc('+',sfd);
		in = 1;
	    }
	    prev = ch;
	    prev_cnt = 2;		/* 2 bytes */
	} else if ( prev_cnt==2 ) {
	    prev<<=8;
	    prev += (ch>>8)&0xff;
	    utf7_encode(sfd,prev);
	    prev = (ch&0xff);
	    prev_cnt=1;
	} else {
	    prev<<=16;
	    prev |= ch;
	    utf7_encode(sfd,prev);
	    prev_cnt = prev = 0;
	}
    }
    if ( prev_cnt==2 ) {
	prev<<=8;
	utf7_encode(sfd,prev);
    } else if ( prev_cnt==1 ) {
	prev<<=16;
	utf7_encode(sfd,prev);
    }
    putc('"',sfd);
    putc(' ',sfd);
}

static unichar_t *SFDReadUTF7Str(FILE *sfd) {
    int ch1, ch2, ch3, ch4, done;
    unichar_t buffer[1024], *pt, *end = buffer+sizeof(buffer)/sizeof(unichar_t)-1;
    int prev_cnt=0, prev=0, in=0;

    ch1 = getc(sfd);
    while ( isspace(ch1) && ch1!='\n' && ch1!='\r') ch1 = getc(sfd);
    if ( ch1=='\n' || ch1=='\r' )
	ungetc(ch1,sfd);
    if ( ch1!='"' )
return( NULL );
    pt = buffer;
    while ( (ch1=getc(sfd))!=EOF && ch1!='"' ) {
	done = 0;
	if ( !done && !in ) {
	    if ( ch1=='+' ) {
		ch1 = getc(sfd);
		if ( ch1=='-' ) {
		    if ( pt<end ) *pt++ = '+';
		    done = true;
		} else {
		    in = true;
		    prev_cnt = 0;
		}
	    } else
		done = true;
	}
	if ( !done ) {
	    if ( ch1=='-' ) {
		in = false;
	    } else if ( inbase64[ch1]==-1 ) {
		in = false;
		done = true;
	    } else {
		ch1 = inbase64[ch1];
		ch2 = inbase64[getc(sfd)];
		ch3 = inbase64[getc(sfd)];
		ch4 = inbase64[getc(sfd)];
		ch1 = (ch1<<18) | (ch2<<12) | (ch3<<6) | ch4;
		if ( prev_cnt==0 ) {
		    prev = ch1&0xff;
		    ch1 >>= 8;
		    prev_cnt = 1;
		} else /* if ( prev_cnt == 1 ) */ {
		    ch1 |= (prev<<24);
		    prev = (ch1&0xffff);
		    ch1 >>= 16;
		    prev_cnt = 2;
		}
		done = true;
	    }
	}
	if ( done && pt<end )
	    *pt++ = ch1;
	if ( prev_cnt==2 ) {
	    prev_cnt = 0;
	    if ( pt<end && prev!=0 )
		*pt++ = prev;
	}
    }
    *pt = '\0';

return( buffer[0]=='\0' ? NULL : u_copy(buffer) );
}

struct enc85 {
    FILE *sfd;
    unsigned char sofar[4];
    int pos;
    int ccnt;
};

static void SFDEnc85(struct enc85 *enc,int ch) {
    enc->sofar[enc->pos++] = ch;
    if ( enc->pos==4 ) {
	unsigned int val = (enc->sofar[0]<<24)|(enc->sofar[1]<<16)|(enc->sofar[2]<<8)|enc->sofar[3];
	if ( val==0 ) {
	    fputc('z',enc->sfd);
	    ++enc->ccnt;
	} else {
	    int ch2, ch3, ch4, ch5;
	    ch5 = val%85;
	    val /= 85;
	    ch4 = val%85;
	    val /= 85;
	    ch3 = val%85;
	    val /= 85;
	    ch2 = val%85;
	    val /= 85;
	    fputc('!'+val,enc->sfd);
	    fputc('!'+ch2,enc->sfd);
	    fputc('!'+ch3,enc->sfd);
	    fputc('!'+ch4,enc->sfd);
	    fputc('!'+ch5,enc->sfd);
	    enc->ccnt += 5;
	    if ( enc->ccnt > 70 ) { fputc('\n',enc->sfd); enc->ccnt=0; }
	}
	enc->pos = 0;
    }
}

static void SFDEnc85EndEnc(struct enc85 *enc) {
    int i;
    int ch2, ch3, ch4, ch5;
    unsigned val;
    if ( enc->pos==0 )
return;
    for ( i=enc->pos; i<4; ++i )
	enc->sofar[i] = 0;
    val = (enc->sofar[0]<<24)|(enc->sofar[1]<<16)|(enc->sofar[2]<<8)|enc->sofar[3];
    if ( val==0 ) {
	fputc('z',enc->sfd);
    } else {
	ch5 = val%85;
	val /= 85;
	ch4 = val%85;
	val /= 85;
	ch3 = val%85;
	val /= 85;
	ch2 = val%85;
	val /= 85;
	fputc('!'+val,enc->sfd);
	fputc('!'+ch2,enc->sfd);
	fputc('!'+ch3,enc->sfd);
	fputc('!'+ch4,enc->sfd);
	fputc('!'+ch5,enc->sfd);
    }
    enc->pos = 0;
}

static void SFDDumpHintMask(FILE *sfd,HintMask *hintmask) {
    int i, j;

    for ( i=HntMax/8-1; i>0; --i )
	if ( (*hintmask)[i]!=0 )
    break;
    for ( j=0; j<=i ; ++j ) {
	if ( ((*hintmask)[j]>>4)<10 )
	    putc('0'+((*hintmask)[j]>>4),sfd);
	else
	    putc('a'-10+((*hintmask)[j]>>4),sfd);
	if ( ((*hintmask)[j]&0xf)<10 )
	    putc('0'+((*hintmask)[j]&0xf),sfd);
	else
	    putc('a'-10+((*hintmask)[j]&0xf),sfd);
    }
}

static void SFDDumpSplineSet(FILE *sfd,SplineSet *spl) {
    SplinePoint *first, *sp;
    int order2 = spl->first->next==NULL || spl->first->next->order2;

    for ( ; spl!=NULL; spl=spl->next ) {
	first = NULL;
	for ( sp = spl->first; ; sp=sp->next->to ) {
	    if ( first==NULL )
		fprintf( sfd, "%g %g m ", sp->me.x, sp->me.y );
	    else if ( sp->prev->islinear )		/* Don't use known linear here. save control points if there are any */
		fprintf( sfd, " %g %g l ", sp->me.x, sp->me.y );
	    else
		fprintf( sfd, " %g %g %g %g %g %g c ",
			sp->prev->from->nextcp.x, sp->prev->from->nextcp.y,
			sp->prevcp.x, sp->prevcp.y,
			sp->me.x, sp->me.y );
	    fprintf(sfd, "%d", sp->pointtype|(sp->selected<<2)|
			(sp->nextcpdef<<3)|(sp->prevcpdef<<4)|
			(sp->roundx<<5)|(sp->roundy<<6)|
			(sp->ttfindex==0xffff?(1<<7):0) );
	    if ( order2 ) {
		if ( sp->ttfindex!=0xfffe && sp->nextcpindex!=0xfffe ) {
		    putc(',',sfd);
		    if ( sp->ttfindex==0xffff )
			fprintf(sfd,"-1");
		    else if ( sp->ttfindex!=0xfffe )
			fprintf(sfd,"%d",sp->ttfindex);
		    if ( sp->nextcpindex==0xffff )
			fprintf(sfd,",-1");
		    else if ( sp->nextcpindex!=0xfffe )
			fprintf(sfd,",%d",sp->nextcpindex);
		}
	    } else {
		if ( sp->hintmask!=NULL ) {
		    putc('x',sfd);
		    SFDDumpHintMask(sfd, sp->hintmask);
		}
	    }
	    putc('\n',sfd);
	    if ( sp==first )
	break;
	    if ( first==NULL ) first = sp;
	    if ( sp->next==NULL )
	break;
	}
    }
    fprintf( sfd, "EndSplineSet\n" );
}

static void SFDDumpMinimumDistances(FILE *sfd,SplineChar *sc) {
    MinimumDistance *md = sc->md;
    SplineSet *ss;
    SplinePoint *sp;
    int pt=0;

    if ( md==NULL )
return;
    for ( ss = sc->splines; ss!=NULL; ss=ss->next ) {
	for ( sp=ss->first; ; ) {
	    sp->ptindex = pt++;
	    if ( sp->next == NULL )
	break;
	    sp = sp->next->to;
	    if ( sp==ss->first )
	break;
	}
    }
    fprintf( sfd, "MinimumDistance: " );
    while ( md!=NULL ) {
	fprintf( sfd, "%c%d,%d ", md->x?'x':'y', md->sp1?md->sp1->ptindex:-1,
		md->sp2?md->sp2->ptindex:-1 );
	md = md->next;
    }
    fprintf( sfd, "\n" );
}

static void SFDDumpAnchorPoints(FILE *sfd,SplineChar *sc) {
    AnchorPoint *ap;

    for ( ap = sc->anchor; ap!=NULL; ap=ap->next ) {
	fprintf( sfd, "AnchorPoint: " );
	SFDDumpUTF7Str(sfd,ap->anchor->name);
	fprintf( sfd, "%g %g %s %d\n",
		ap->me.x, ap->me.y,
		ap->type==at_centry ? "entry" :
		ap->type==at_cexit ? "exit" :
		ap->type==at_mark ? "mark" :
		ap->type==at_basechar ? "basechar" :
		ap->type==at_baselig ? "baselig" : "basemark",
		ap->lig_index );
    }
}

/* Run length encoding */
/* We always start with a background pixel(1), each line is a series of counts */
/*  we alternate background/foreground. If we can't represent an entire run */
/*  as one count, then we can split it up into several smaller runs and put */
/*  0 counts in between */
/* counts 0-254 mean 0-254 pixels of the current color */
/* count 255 means that the next two bytes (bigendian) provide a two byte count */
/* count 255 0 n (n<255) means that the previous line should be repeated n+1 times */
/* count 255 0 255 means 255 pixels of the current color */
static uint8 *image2rle(struct _GImage *img, int *len) {
    int max = img->height*img->bytes_per_line;
    uint8 *rle, *pt, *end;
    int cnt, set;
    int i,j,k;

    *len = 0;
    if ( img->image_type!=it_mono || img->bytes_per_line<5 )
return( NULL );
    rle = gcalloc(max,sizeof(uint8)), pt = rle, end=rle+max-3;

    for ( i=0; i<img->height; ++i ) {
	if ( i!=0 ) {
	    if ( memcmp(img->data+i*img->bytes_per_line,
			img->data+(i-1)*img->bytes_per_line, img->bytes_per_line)== 0 ) {
		for ( k=1; k<img->height-i; ++k ) {
		    if ( memcmp(img->data+(i+k)*img->bytes_per_line,
				img->data+i*img->bytes_per_line, img->bytes_per_line)!= 0 )
		break;
		}
		i+=k;
		while ( k>0 ) {
		    if ( pt>end ) {
			free(rle);
return( NULL );
		    }
		    *pt++ = 255;
		    *pt++ = 0;
		    *pt++ = k>254 ? 254 : k;
		    k -= 254;
		}
		if ( i>=img->height )
    break;
	    }
	}

	set=1; cnt=0; j=0;
	while ( j<img->width ) {
	    for ( k=j; k<img->width; ++k ) {
		if (( set && !(img->data[i*img->bytes_per_line+(k>>3)]&(0x80>>(k&7))) ) ||
		    ( !set && (img->data[i*img->bytes_per_line+(k>>3)]&(0x80>>(k&7))) ))
	    break;
	    }
	    cnt = k-j;
	    j=k;
	    do {
		if ( pt>=end ) {
		    free(rle);
return( NULL );
		}
		if ( cnt<=254 )
		    *pt++ = cnt;
		else {
		    *pt++ = 255;
		    if ( cnt>65535 ) {
			*pt++ = 255;
			*pt++ = 255;
			*pt++ = 0;		/* nothing of the other color, we've still got more of this one */
		    } else {
			*pt++ = cnt>>8;
			*pt++ = cnt&0xff;
		    }
		}
		cnt -= 65535;
	    } while ( cnt>0 );
	    set = 1-set;
	}
    }
    *len = pt-rle;
return( rle );
}

static void SFDDumpImage(FILE *sfd,ImageList *img) {
    GImage *image = img->image;
    struct _GImage *base = image->list_len==0?image->u.image:image->u.images[0];
    struct enc85 enc;
    int rlelen;
    uint8 *rle;
    int i;

    rle = image2rle(base,&rlelen);
    fprintf(sfd, "Image: %d %d %d %d %d %x %g %g %g %g %d\n",
	    base->width, base->height, base->image_type,
	    base->image_type==it_true?3*base->width:base->bytes_per_line,
	    base->clut==NULL?0:base->clut->clut_len,base->trans,
	    img->xoff, img->yoff, img->xscale, img->yscale, rlelen );
    memset(&enc,'\0',sizeof(enc));
    enc.sfd = sfd;
    if ( base->clut!=NULL ) {
	for ( i=0; i<base->clut->clut_len; ++i ) {
	    SFDEnc85(&enc,base->clut->clut[i]>>16);
	    SFDEnc85(&enc,(base->clut->clut[i]>>8)&0xff);
	    SFDEnc85(&enc,base->clut->clut[i]&0xff);
	}
    }
    if ( rle!=NULL ) {
	uint8 *pt=rle, *end=rle+rlelen;
	while ( pt<end )
	    SFDEnc85(&enc,*pt++);
	free( rle );
    } else {
	for ( i=0; i<base->height; ++i ) {
	    if ( base->image_type==it_true ) {
		int *ipt = (int *) (base->data + i*base->bytes_per_line);
		int *iend = (int *) (base->data + (i+1)*base->bytes_per_line);
		while ( ipt<iend ) {
		    SFDEnc85(&enc,*ipt>>16);
		    SFDEnc85(&enc,(*ipt>>8)&0xff);
		    SFDEnc85(&enc,*ipt&0xff);
		    ++ipt;
		}
	    } else {
		uint8 *pt = (uint8 *) (base->data + i*base->bytes_per_line);
		uint8 *end = (uint8 *) (base->data + (i+1)*base->bytes_per_line);
		while ( pt<end ) {
		    SFDEnc85(&enc,*pt);
		    ++pt;
		}
	    }
	}
    }
    SFDEnc85EndEnc(&enc);
    fprintf(sfd,"\nEndImage\n" );
}

static void SFDDumpHintList(FILE *sfd,char *key, StemInfo *h) {
    HintInstance *hi;

    if ( h==NULL )
return;
    fprintf(sfd, "%s", key );
    for ( ; h!=NULL; h=h->next ) {
	fprintf(sfd, "%g %g", h->start,h->width );
	if ( h->ghost ) putc('G',sfd);
	if ( h->where!=NULL ) {
	    putc('<',sfd);
	    for ( hi=h->where; hi!=NULL; hi=hi->next )
		fprintf(sfd, "%g %g%c", hi->begin, hi->end, hi->next?' ':'>');
	}
	putc(h->next?' ':'\n',sfd);
    }
}

static void SFDDumpDHintList(FILE *sfd,char *key, DStemInfo *h) {

    if ( h==NULL )
return;
    fprintf(sfd, "%s", key );
    for ( ; h!=NULL; h=h->next ) {
	fprintf(sfd, "%g %g %g %g %g %g %g %g",
		h->leftedgetop.x, h->leftedgetop.y,
		h->rightedgetop.x, h->rightedgetop.y,
		h->leftedgebottom.x, h->leftedgebottom.y,
		h->rightedgebottom.x, h->rightedgebottom.y );
	putc(h->next?' ':'\n',sfd);
    }
}

static void SFDDumpTtfInstrs(FILE *sfd,SplineChar *sc) {
    struct enc85 enc;
    int i;

    memset(&enc,'\0',sizeof(enc));
    enc.sfd = sfd;

    fprintf( sfd, "TtfInstrs: %d\n", sc->ttf_instrs_len );
    for ( i=0; i<sc->ttf_instrs_len; ++i )
	SFDEnc85(&enc,sc->ttf_instrs[i]);
    SFDEnc85EndEnc(&enc);
    fprintf(sfd,"\nEndTtf\n" );
}

static void SFDDumpTtfTable(FILE *sfd,struct ttf_table *tab) {
    struct enc85 enc;
    int i;

    memset(&enc,'\0',sizeof(enc));
    enc.sfd = sfd;

    fprintf( sfd, "TtfTable: %c%c%c%c %d\n",
	    tab->tag>>24, (tab->tag>>16)&0xff, (tab->tag>>8)&0xff, tab->tag&0xff,
	    tab->len );
    for ( i=0; i<tab->len; ++i )
	SFDEnc85(&enc,tab->data[i]);
    SFDEnc85EndEnc(&enc);
    fprintf(sfd,"\nEndTtf\n" );
}

static int SFDOmit(SplineChar *sc) {
    if ( sc==NULL )
return( true );
    if ( sc->splines==NULL && sc->refs==NULL && sc->anchor==NULL &&
	    sc->backgroundsplines==NULL && sc->backimages==NULL ) {
	if ( strcmp(sc->name,".null")==0 || strcmp(sc->name,"nonmarkingreturn")==0 )
return(true);
	if ( !sc->widthset && sc->width==sc->parent->ascent+sc->parent->descent &&
		(strcmp(sc->name,".notdef")==0 || sc->enc==sc->unicodeenc ||
		 strcmp(sc->name,".null")==0 || strcmp(sc->name,"nonmarkingreturn")==0 ))
return(true);
    }
return( false );
}

static void SFDDumpChar(FILE *sfd,SplineChar *sc) {
    RefChar *ref;
    ImageList *img;
    KernPair *kp;
    PST *liga;
    int i;

    fprintf(sfd, "StartChar: %s\n", sc->name );
    fprintf(sfd, "Encoding: %d %d %d\n", sc->enc, sc->unicodeenc, sc->orig_pos);
    if ( sc->parent->compacted )
	fprintf(sfd, "OldEncoding: %d\n", sc->old_enc);
    fprintf(sfd, "Width: %d\n", sc->width );
    if ( sc->vwidth!=sc->parent->ascent+sc->parent->descent )
	fprintf(sfd, "VWidth: %d\n", sc->vwidth );
    if ( sc->glyph_class!=0 )
	fprintf(sfd, "GlyphClass: %d\n", sc->glyph_class );
    if ( sc->changedsincelasthinted|| sc->manualhints || sc->widthset )
	fprintf(sfd, "Flags: %s%s%s%s\n",
		sc->changedsincelasthinted?"H":"",
		sc->manualhints?"M":"",
		sc->widthset?"W":"",
		sc->views!=NULL?"O":"");
#if HANYANG
    if ( sc->compositionunit )
	fprintf( sfd, "CompositionUnit: %d %d\n", sc->jamo, sc->varient );
#endif
    SFDDumpHintList(sfd,"HStem: ", sc->hstem);
    SFDDumpHintList(sfd,"VStem: ", sc->vstem);
    SFDDumpDHintList(sfd,"DStem: ", sc->dstem);
    if ( sc->countermask_cnt!=0 ) {
	fprintf( sfd, "CounterMasks: %d", sc->countermask_cnt );
	for ( i=0; i<sc->countermask_cnt; ++i ) {
	    putc(' ',sfd);
	    SFDDumpHintMask(sfd,&sc->countermasks[i]);
	}
	putc('\n',sfd);
    }
    if ( sc->ttf_instrs_len!=0 )
	SFDDumpTtfInstrs(sfd,sc);
    if ( sc->splines!=NULL ) {
	fprintf(sfd, "Fore\n" );
	SFDDumpSplineSet(sfd,sc->splines);
	SFDDumpMinimumDistances(sfd,sc);
    }
    SFDDumpAnchorPoints(sfd,sc);
    for ( ref=sc->refs; ref!=NULL; ref=ref->next ) if ( ref->sc!=NULL ) {
	if ( ref->sc->enc==0 && ref->sc->splines==NULL )
	    fprintf( stderr, "Using a reference to character 0, removed. In %s\n", sc->name);
	else
	fprintf(sfd, "Ref: %d %d %c %g %g %g %g %g %g\n",
		ref->sc->enc, ref->sc->unicodeenc,
		ref->selected?'S':'N',
		ref->transform[0], ref->transform[1], ref->transform[2],
		ref->transform[3], ref->transform[4], ref->transform[5]);
    }
    if ( sc->backgroundsplines!=NULL ) {
	fprintf(sfd, "Back\n" );
	SFDDumpSplineSet(sfd,sc->backgroundsplines);
    }
    for ( img=sc->backimages; img!=NULL; img=img->next )
	SFDDumpImage(sfd,img);
    if ( sc->kerns!=NULL ) {
	fprintf( sfd, "KernsSLIF:" );
	for ( kp = sc->kerns; kp!=NULL; kp=kp->next )
	    if ( kp->off!=0 && !SFDOmit(kp->sc))
		fprintf( sfd, " %d %d %d %d", kp->sc->enc, kp->off, kp->sli, kp->flags );
	fprintf(sfd, "\n" );
    }
    if ( sc->vkerns!=NULL ) {
	fprintf( sfd, "VKernsSLIF:" );
	for ( kp = sc->vkerns; kp!=NULL; kp=kp->next )
	    if ( kp->off!=0 && !SFDOmit(kp->sc))
		fprintf( sfd, " %d %d %d %d", kp->sc->enc, kp->off, kp->sli, kp->flags );
	fprintf(sfd, "\n" );
    }
    for ( liga=sc->possub; liga!=NULL; liga=liga->next ) {
	if (( liga->tag==0 && liga->type!=pst_lcaret) || liga->type==pst_null )
	    /* Skip it */;
	else {
	    static char *keywords[] = { "Null:", "Position:", "PairPos:",
		    "Substitution:",
		    "AlternateSubs:", "MultipleSubs:", "Ligature:",
		    "LCarets:", NULL };
	    if ( liga->tag==0 ) liga->tag = CHR(' ',' ',' ',' ');
	    fprintf( sfd, "%s %d %d ",
		    keywords[liga->type], liga->flags,
		    liga->script_lang_index );
	    if ( liga->macfeature )
		fprintf( sfd, "<%d,%d> ",
			liga->tag>>16,
			liga->tag&0xffff);
	    else
		fprintf( sfd, "'%c%c%c%c' ",
			liga->tag>>24, (liga->tag>>16)&0xff,
			(liga->tag>>8)&0xff, liga->tag&0xff );
	    if ( liga->type==pst_position )
		fprintf( sfd, "dx=%d dy=%d dh=%d dv=%d\n",
			liga->u.pos.xoff, liga->u.pos.yoff,
			liga->u.pos.h_adv_off, liga->u.pos.v_adv_off);
	    else if ( liga->type==pst_pair )
		fprintf( sfd, "%s dx=%d dy=%d dh=%d dv=%d | dx=%d dy=%d dh=%d dv=%d\n",
			liga->u.pair.paired,
			liga->u.pair.vr[0].xoff, liga->u.pair.vr[0].yoff,
			liga->u.pair.vr[0].h_adv_off, liga->u.pair.vr[0].v_adv_off,
			liga->u.pair.vr[1].xoff, liga->u.pair.vr[1].yoff,
			liga->u.pair.vr[1].h_adv_off, liga->u.pair.vr[1].v_adv_off);
	    else if ( liga->type==pst_lcaret ) {
		int i;
		fprintf( sfd, "%d ", liga->u.lcaret.cnt );
		for ( i=0; i<liga->u.lcaret.cnt; ++i )
		    fprintf( sfd, "%d ", liga->u.lcaret.carets[i] );
		fprintf( sfd, "\n" );
	    } else
		fprintf( sfd, "%s\n", liga->u.lig.components );
	}
    }
    if ( sc->comment!=NULL ) {
	fprintf( sfd, "Comment: " );
	SFDDumpUTF7Str(sfd,sc->comment);
	putc('\n',sfd);
    }
    if ( sc->color!=COLOR_DEFAULT )
	fprintf( sfd, "Colour: %x\n", sc->color );
    fprintf(sfd,"EndChar\n" );
}

static void SFDDumpBitmapChar(FILE *sfd,BDFChar *bfc) {
    struct enc85 enc;
    int i;

    fprintf(sfd, "BDFChar: %d %d %d %d %d %d\n",
	    bfc->enc, bfc->width, bfc->xmin, bfc->xmax, bfc->ymin, bfc->ymax );
    memset(&enc,'\0',sizeof(enc));
    enc.sfd = sfd;
    for ( i=0; i<=bfc->ymax-bfc->ymin; ++i ) {
	uint8 *pt = (uint8 *) (bfc->bitmap + i*bfc->bytes_per_line);
	uint8 *end = pt + bfc->bytes_per_line;
	while ( pt<end ) {
	    SFDEnc85(&enc,*pt);
	    ++pt;
	}
    }
    SFDEnc85EndEnc(&enc);
#if 0
    fprintf(sfd,"\nEndBitmapChar\n" );
#else
    fputc('\n',sfd);
#endif
}

static void SFDDumpBitmapFont(FILE *sfd,BDFFont *bdf) {
    int i;

    GProgressNextStage();
    fprintf( sfd, "BitmapFont: %d %d %d %d %d %s\n", bdf->pixelsize, bdf->charcnt,
	    bdf->ascent, bdf->descent, BDFDepth(bdf), bdf->foundry?bdf->foundry:"" );
    for ( i=0; i<bdf->charcnt; ++i ) {
	if ( bdf->chars[i]!=NULL )
	    SFDDumpBitmapChar(sfd,bdf->chars[i]);
	GProgressNext();
    }
    fprintf( sfd, "EndBitmapFont\n" );
}

static void SFDDumpPrivate(FILE *sfd,struct psdict *private) {
    int i;
    char *pt;
    /* These guys should all be ascii text */
    fprintf( sfd, "BeginPrivate: %d\n", private->next );
    for ( i=0; i<private->next ; ++i ) {
	fprintf( sfd, "%s %d ", private->keys[i], strlen(private->values[i]));
	for ( pt = private->values[i]; *pt; ++pt )
	    putc(*pt,sfd);
	putc('\n',sfd);
    }
    fprintf( sfd, "EndPrivate\n" );
}

static void SFDDumpLangName(FILE *sfd, struct ttflangname *ln) {
    int i, end;
    fprintf( sfd, "LangName: %d ", ln->lang );
    for ( end = ttf_namemax; end>0 && ln->names[end-1]==NULL; --end );
    for ( i=0; i<end; ++i )
	SFDDumpUTF7Str(sfd,ln->names[i]);
    putc('\n',sfd);
}

static void putstring(FILE *sfd, char *header, char *body) {
    fprintf( sfd, "%s", header );
    while ( *body ) {
	if ( *body=='\n' || *body == '\\' || *body=='\r' ) {
	    putc('\\',sfd);
	    if ( *body=='\\' )
		putc('\\',sfd);
	    else {
		putc('n',sfd);
		if ( *body=='\r' && body[1]=='\n' )
		    ++body;
	    }
	} else
	    putc(*body,sfd);
	++body;
    }
    putc('\n',sfd);
}

const char *EncName(int encname) {
    static char buffer[40];

    if ( encname>=em_unicodeplanes && encname<=em_unicodeplanesmax ) {
	sprintf(buffer, "UnicodePlane%d", encname-em_unicodeplanes );
return( buffer );
    } else if ( encname>=em_base ) {
	Encoding *item;
	for ( item = enclist; item!=NULL && item->enc_num!=encname; item = item->next );
	if ( item!=NULL )
return( item->enc_name );

	fprintf(stderr, "Unknown encoding %d\n", encname );
return( NULL );
    } else if ( encname>=sizeof(charset_names)/sizeof(charset_names[0])-2 &&
	    encname!=em_none ) {
	fprintf(stderr, "Unknown encoding %d\n", encname );
return( NULL );
    } else
return( charset_names[encname+1]);
}

static void SFDDumpEncoding(FILE *sfd,int encname,char *keyword) {
    const char *name = EncName(encname);

    if ( name==NULL && encname>=em_base )
	name = "custom";
    if ( name==NULL ) {
	fprintf(sfd, "%s: %d\n", keyword, encname );
    } else
	fprintf(sfd, "%s: %s\n", keyword, name );
}

static void SFDDumpMacName(FILE *sfd,struct macname *mn) {
    char *pt;

    while ( mn!=NULL ) {
	fprintf( sfd, "MacName: %d %d %d \"", mn->enc, mn->lang, strlen(mn->name) );
	for ( pt=mn->name; *pt; ++pt ) {
	    if ( *pt<' ' || *pt>=0x7f || *pt=='\\' || *pt=='"' )
		fprintf( sfd, "\\%03o", *(uint8 *) pt );
	    else
		putc(*pt,sfd);
	}
	fprintf( sfd, "\"\n" );
	mn = mn->next;
    }
}

void SFDDumpMacFeat(FILE *sfd,MacFeat *mf) {
    struct macsetting *ms;

    if ( mf==NULL )
return;

    while ( mf!=NULL ) {
	if ( mf->featname!=NULL ) {
	    fprintf( sfd, "MacFeat: %d %d %d\n", mf->feature, mf->ismutex, mf->default_setting );
	    SFDDumpMacName(sfd,mf->featname);
	    for ( ms=mf->settings; ms!=NULL; ms=ms->next ) {
		if ( ms->setname!=NULL ) {
		    fprintf( sfd, "MacSetting: %d\n", ms->setting );
		    SFDDumpMacName(sfd,ms->setname);
		}
	    }
	}
	mf = mf->next;
    }
    fprintf( sfd,"EndMacFeatures\n" );
}

static void SFD_Dump(FILE *sfd,SplineFont *sf) {
    int i, j, realcnt;
    BDFFont *bdf;
    struct ttflangname *ln;
    struct table_ordering *ord;
    struct ttf_table *tab;
    KernClass *kc;
    FPST *fpst;
    ASM *sm;
    int isv;

    fprintf(sfd, "FontName: %s\n", sf->fontname );
    if ( sf->fullname!=NULL )
	fprintf(sfd, "FullName: %s\n", sf->fullname );
    if ( sf->familyname!=NULL )
	fprintf(sfd, "FamilyName: %s\n", sf->familyname );
    if ( sf->weight!=NULL )
	fprintf(sfd, "Weight: %s\n", sf->weight );
    if ( sf->copyright!=NULL )
	putstring(sfd, "Copyright: ", sf->copyright );
    if ( sf->comments!=NULL )
	putstring(sfd, "Comments: ", sf->comments );
    if ( sf->version!=NULL )
	fprintf(sfd, "Version: %s\n", sf->version );
    fprintf(sfd, "ItalicAngle: %g\n", sf->italicangle );
    fprintf(sfd, "UnderlinePosition: %g\n", sf->upos );
    fprintf(sfd, "UnderlineWidth: %g\n", sf->uwidth );
    fprintf(sfd, "Ascent: %d\n", sf->ascent );
    fprintf(sfd, "Descent: %d\n", sf->descent );
    if ( sf->order2 )
	fprintf(sfd, "Order2: %d\n", sf->order2 );
    if ( sf->hasvmetrics )
	fprintf(sfd, "VerticalOrigin: %d\n", sf->vertical_origin );
    if ( sf->changed_since_xuidchanged )
	fprintf(sfd, "NeedsXUIDChange: 1\n" );
    if ( sf->xuid!=NULL )
	fprintf(sfd, "XUID: %s\n", sf->xuid );
    if ( sf->uniqueid!=0 )
	fprintf(sfd, "UniqueID: %d\n", sf->uniqueid );
    if ( sf->pfminfo.fstype!=-1 )
	fprintf(sfd, "FSType: %d\n", sf->pfminfo.fstype );
    if ( sf->pfminfo.pfmset ) {
	fprintf(sfd, "PfmFamily: %d\n", sf->pfminfo.pfmfamily );
	fprintf(sfd, "TTFWeight: %d\n", sf->pfminfo.weight );
	fprintf(sfd, "TTFWidth: %d\n", sf->pfminfo.width );
	fprintf(sfd, "Panose:" );
	for ( i=0; i<10; ++i )
	    fprintf( sfd, " %d", sf->pfminfo.panose[i]);
	putc('\n',sfd);
	fprintf(sfd, "LineGap: %d\n", sf->pfminfo.linegap );
	fprintf(sfd, "VLineGap: %d\n", sf->pfminfo.vlinegap );
	/*putc('\n',sfd);*/
    }
    if ( sf->pfminfo.os2_typoascent!=0 ) {
	/*fprintf(sfd, "HheadAscent: %d\n", sf->pfminfo.hhead_ascent );*/
	/*fprintf(sfd, "HheadDescent: %d\n", sf->pfminfo.hhead_descent );*/
	fprintf(sfd, "OS2TypoAscent: %d\n", sf->pfminfo.os2_typoascent );
	fprintf(sfd, "OS2TypoDescent: %d\n", sf->pfminfo.os2_typodescent );
	fprintf(sfd, "OS2WinAscent: %d\n", sf->pfminfo.os2_winascent );
	fprintf(sfd, "OS2WinDescent: %d\n", sf->pfminfo.os2_windescent );
    }
    if ( sf->macstyle!=-1 )
	fprintf(sfd, "MacStyle: %d\n", sf->macstyle );
    if ( sf->script_lang ) {
	int i,j,k;
	for ( i=0; sf->script_lang[i]!=NULL; ++i );
	fprintf(sfd, "ScriptLang: %d\n", i );
	for ( i=0; sf->script_lang[i]!=NULL; ++i ) {
	    for ( j=0; sf->script_lang[i][j].script!=0; ++j );
	    fprintf( sfd, " %d ", j );		/* Script cnt */
	    for ( j=0; sf->script_lang[i][j].script!=0; ++j ) {
		for ( k=0; sf->script_lang[i][j].langs[k]!=0 ; ++k );
		fprintf( sfd, "%c%c%c%c %d ",
			sf->script_lang[i][j].script>>24,
			(sf->script_lang[i][j].script>>16)&0xff,
			(sf->script_lang[i][j].script>>8)&0xff,
			sf->script_lang[i][j].script&0xff,
			k );
		for ( k=0; sf->script_lang[i][j].langs[k]!=0 ; ++k )
		    fprintf( sfd, "%c%c%c%c ",
			    sf->script_lang[i][j].langs[k]>>24,
			    (sf->script_lang[i][j].langs[k]>>16)&0xff,
			    (sf->script_lang[i][j].langs[k]>>8)&0xff,
			    sf->script_lang[i][j].langs[k]&0xff );
	    }
	    fprintf( sfd,"\n");
	}
    }
    for ( isv=0; isv<2; ++isv ) {
	for ( kc=isv ? sf->vkerns : sf->kerns; kc!=NULL; kc = kc->next ) {
	    fprintf( sfd, "%s: %d %d %d %d\n", isv ? "VKernClass" : "KernClass",
		    kc->first_cnt, kc->second_cnt, kc->sli, kc->flags );
	    for ( i=1; i<kc->first_cnt; ++i )
		fprintf( sfd, " %d %s\n", strlen(kc->firsts[i]), kc->firsts[i]);
	    for ( i=1; i<kc->second_cnt; ++i )
		fprintf( sfd, " %d %s\n", strlen(kc->seconds[i]), kc->seconds[i]);
	    for ( i=0; i<kc->first_cnt*kc->second_cnt; ++i )
		fprintf( sfd, " %d", kc->offsets[i]);
	    fprintf( sfd, "\n" );
	}
    }
    for ( fpst=sf->possub; fpst!=NULL; fpst=fpst->next ) {
	static char *keywords[] = { "ContextPos:", "ContextSub:", "ChainPos:", "ChainSub:", "ReverseChain:", NULL };
	static char *formatkeys[] = { "glyph", "class", "coverage", "revcov", NULL };
	fprintf( sfd, "%s %s %d %d '%c%c%c%c' %d %d %d %d\n",
		keywords[fpst->type-pst_contextpos],
		formatkeys[fpst->format],
		fpst->flags,
		fpst->script_lang_index,
		fpst->tag>>24, (fpst->tag>>16)&0xff,
		(fpst->tag>>8)&0xff, fpst->tag&0xff,
		fpst->nccnt, fpst->bccnt, fpst->fccnt, fpst->rule_cnt );
	for ( i=1; i<fpst->nccnt; ++i )
	    fprintf( sfd, "  Class: %d %s\n", strlen(fpst->nclass[i]), fpst->nclass[i]);
	for ( i=1; i<fpst->bccnt; ++i )
	    fprintf( sfd, "  BClass: %d %s\n", strlen(fpst->bclass[i]), fpst->bclass[i]);
	for ( i=1; i<fpst->fccnt; ++i )
	    fprintf( sfd, "  FClass: %d %s\n", strlen(fpst->fclass[i]), fpst->fclass[i]);
	for ( i=0; i<fpst->rule_cnt; ++i ) {
	    switch ( fpst->format ) {
	      case pst_glyphs:
		fprintf( sfd, " String: %d %s\n", strlen(fpst->rules[i].u.glyph.names), fpst->rules[i].u.glyph.names);
		if ( fpst->rules[i].u.glyph.back!=NULL )
		    fprintf( sfd, " BString: %d %s\n", strlen(fpst->rules[i].u.glyph.back), fpst->rules[i].u.glyph.back);
		else
		    fprintf( sfd, " BString: 0\n");
		if ( fpst->rules[i].u.glyph.fore!=NULL )
		    fprintf( sfd, " FString: %d %s\n", strlen(fpst->rules[i].u.glyph.fore), fpst->rules[i].u.glyph.fore);
		else
		    fprintf( sfd, " FString: 0\n");
	      break;
	      case pst_class:
		fprintf( sfd, " %d %d %d\n  ClsList:", fpst->rules[i].u.class.ncnt, fpst->rules[i].u.class.bcnt, fpst->rules[i].u.class.fcnt );
		for ( j=0; j<fpst->rules[i].u.class.ncnt; ++j )
		    fprintf( sfd, " %d", fpst->rules[i].u.class.nclasses[j]);
		fprintf( sfd, "\n  BClsList:" );
		for ( j=0; j<fpst->rules[i].u.class.bcnt; ++j )
		    fprintf( sfd, " %d", fpst->rules[i].u.class.bclasses[j]);
		fprintf( sfd, "\n  FClsList:" );
		for ( j=0; j<fpst->rules[i].u.class.fcnt; ++j )
		    fprintf( sfd, " %d", fpst->rules[i].u.class.fclasses[j]);
		fprintf( sfd, "\n" );
	      break;
	      case pst_coverage:
	      case pst_reversecoverage:
		fprintf( sfd, " %d %d %d\n", fpst->rules[i].u.coverage.ncnt, fpst->rules[i].u.coverage.bcnt, fpst->rules[i].u.coverage.fcnt );
		for ( j=0; j<fpst->rules[i].u.coverage.ncnt; ++j )
		    fprintf( sfd, "  Coverage: %d %s\n", strlen(fpst->rules[i].u.coverage.ncovers[j]), fpst->rules[i].u.coverage.ncovers[j]);
		for ( j=0; j<fpst->rules[i].u.coverage.bcnt; ++j )
		    fprintf( sfd, "  BCoverage: %d %s\n", strlen(fpst->rules[i].u.coverage.bcovers[j]), fpst->rules[i].u.coverage.bcovers[j]);
		for ( j=0; j<fpst->rules[i].u.coverage.fcnt; ++j )
		    fprintf( sfd, "  FCoverage: %d %s\n", strlen(fpst->rules[i].u.coverage.fcovers[j]), fpst->rules[i].u.coverage.fcovers[j]);
	      break;
	    }
	    switch ( fpst->format ) {
	      case pst_glyphs:
	      case pst_class:
	      case pst_coverage:
		fprintf( sfd, " %d\n", fpst->rules[i].lookup_cnt );
		for ( j=0; j<fpst->rules[i].lookup_cnt; ++j )
		    fprintf( sfd, "  SeqLookup: %d '%c%c%c%c'\n",
			    fpst->rules[i].lookups[j].seq,
			    fpst->rules[i].lookups[j].lookup_tag>>24,
			    (fpst->rules[i].lookups[j].lookup_tag>>16)&0xff,
			    (fpst->rules[i].lookups[j].lookup_tag>>8)&0xff,
			    (fpst->rules[i].lookups[j].lookup_tag)&0xff);
	      break;
	      case pst_reversecoverage:
		fprintf( sfd, "  Replace: %d %s\n", strlen(fpst->rules[i].u.rcoverage.replacements), fpst->rules[i].u.rcoverage.replacements);
	      break;
	    }
	}
	fprintf( sfd, "EndFPST\n" );
    }
    for ( sm=sf->sm; sm!=NULL; sm=sm->next ) {
	static char *keywords[] = { "MacIndic:", "MacContext:", "MacLigature:", "unused", "MacSimple:", "MacInsert:",
	    "unused", "unused", "unused", "unused", "unused", "unused",
	    "unused", "unused", "unused", "unused", "unused", "MacKern:",
	    NULL };
	fprintf( sfd, "%s %d,%d %d %d %d\n",
		keywords[sm->type-asm_indic],
		sm->feature, sm->setting,
		sm->flags,
		sm->class_cnt, sm->state_cnt );
	for ( i=4; i<sm->class_cnt; ++i )
	    fprintf( sfd, "  Class: %d %s\n", strlen(sm->classes[i]), sm->classes[i]);
	for ( i=0; i<sm->class_cnt*sm->state_cnt; ++i ) {
	    fprintf( sfd, " %d %d ", sm->state[i].next_state, sm->state[i].flags );
	    if ( sm->type==asm_context ) {
		if ( sm->state[i].u.context.mark_tag==0 )
		    fprintf(sfd,"~ ");
		else
		    fprintf(sfd,"'%c%c%c%c' ",
			sm->state[i].u.context.mark_tag>>24,
			(sm->state[i].u.context.mark_tag>>16)&0xff,
			(sm->state[i].u.context.mark_tag>>8)&0xff,
			sm->state[i].u.context.mark_tag&0xff);
		if ( sm->state[i].u.context.cur_tag==0 )
		    fprintf(sfd,"~ ");
		else
		    fprintf(sfd,"'%c%c%c%c' ",
			sm->state[i].u.context.cur_tag>>24,
			(sm->state[i].u.context.cur_tag>>16)&0xff,
			(sm->state[i].u.context.cur_tag>>8)&0xff,
			sm->state[i].u.context.cur_tag&0xff);
	    } else if ( sm->type == asm_insert ) {
		if ( sm->state[i].u.insert.mark_ins==NULL )
		    fprintf( sfd, "0 ");
		else
		    fprintf( sfd, "%d %s ", strlen(sm->state[i].u.insert.mark_ins),
			    sm->state[i].u.insert.mark_ins );
		if ( sm->state[i].u.insert.cur_ins==NULL )
		    fprintf( sfd, "0 ");
		else
		    fprintf( sfd, "%d %s ", strlen(sm->state[i].u.insert.cur_ins),
			    sm->state[i].u.insert.cur_ins );
	    } else if ( sm->type == asm_kern ) {
		fprintf( sfd, "%d ", sm->state[i].u.kern.kcnt );
		for ( j=0; j<sm->state[i].u.kern.kcnt; ++j )
		    fprintf( sfd, "%d ", sm->state[i].u.kern.kerns[j]);
	    }
	    putc('\n',sfd);
	}
	fprintf( sfd, "EndASM\n" );
    }
    SFDDumpMacFeat(sfd,sf->features);
    if ( sf->gentags.tt_cur>0 ) {
	fprintf( sfd, "GenTags: %d", sf->gentags.tt_cur );
	for ( i=0; i<sf->gentags.tt_cur; ++i ) {
	    switch ( sf->gentags.tagtype[i].type ) {
	      case pst_position:	fprintf(sfd," ps"); break;
	      case pst_pair:		fprintf(sfd," pr"); break;
	      case pst_substitution:	fprintf(sfd," sb"); break;
	      case pst_alternate:	fprintf(sfd," as"); break;
	      case pst_multiple:	fprintf(sfd," ms"); break;
	      case pst_ligature:	fprintf(sfd," lg"); break;
	      case pst_anchors:		fprintf(sfd," an"); break;
	      case pst_contextpos:	fprintf(sfd," cp"); break;
	      case pst_contextsub:	fprintf(sfd," cs"); break;
	      case pst_chainpos:	fprintf(sfd," pc"); break;
	      case pst_chainsub:	fprintf(sfd," sc"); break;
	      case pst_reversesub:	fprintf(sfd," rs"); break;
	      case pst_null:		fprintf(sfd," nl"); break;
	    }
	    fprintf(sfd,"'%c%c%c%c'",
		    sf->gentags.tagtype[i].tag>>24,
		    (sf->gentags.tagtype[i].tag>>16)&0xff,
		    (sf->gentags.tagtype[i].tag>>8)&0xff,
		    sf->gentags.tagtype[i].tag&0xff );
	}
	putc('\n',sfd);
    }
    for ( ord = sf->orders; ord!=NULL ; ord = ord->next ) {
	for ( i=0; ord->ordered_features[i]!=0; ++i );
	fprintf( sfd, "TableOrder: %c%c%c%c %d\n",
		ord->table_tag>>24, (ord->table_tag>>16)&0xff, (ord->table_tag>>8)&0xff, ord->table_tag&0xff,
		i );
	for ( i=0; ord->ordered_features[i]!=0; ++i )
	    if ( (ord->ordered_features[i]>>24)<' ' || (ord->ordered_features[i]>>24)>=0x7f )
		fprintf( sfd, "\t<%d,%d>\n", ord->ordered_features[i]>>16, ord->ordered_features[i]&0xffff );
	    else
		fprintf( sfd, "\t'%c%c%c%c'\n",
			ord->ordered_features[i]>>24, (ord->ordered_features[i]>>16)&0xff, (ord->ordered_features[i]>>8)&0xff, ord->ordered_features[i]&0xff );
    }
    for ( tab = sf->ttf_tables; tab!=NULL ; tab = tab->next )
	SFDDumpTtfTable(sfd,tab);
    for ( ln = sf->names; ln!=NULL; ln=ln->next )
	SFDDumpLangName(sfd,ln);
    if ( sf->subfontcnt!=0 ) {
	/* CID fonts have no encodings, they have registry info instead */
	fprintf(sfd, "Registry: %s\n", sf->cidregistry );
	fprintf(sfd, "Ordering: %s\n", sf->ordering );
	fprintf(sfd, "Supplement: %d\n", sf->supplement );
	fprintf(sfd, "CIDVersion: %g\n", sf->cidversion );	/* This is a number whereas "version" is a string */
    } else if ( sf->compacted ) {
	fprintf(sfd, "Encoding: compacted\n" );
	SFDDumpEncoding(sfd,sf->old_encname,"OldEncoding");
    } else
	SFDDumpEncoding(sfd,sf->encoding_name,"Encoding");
    fprintf( sfd, "UnicodeInterp: %s\n", unicode_interp_names[sf->uni_interp]);
	
    if ( sf->remap!=NULL ) {
	struct remap *map;
	int n;
	for ( n=0,map = sf->remap; map->infont!=-1; ++n, ++map );
	fprintf( sfd, "RemapN: %d\n", n );
	for ( map = sf->remap; map->infont!=-1; ++map )
	    fprintf(sfd, "Remap: %x %x %d\n", map->firstenc, map->lastenc, map->infont );
    }
    if ( sf->display_size!=0 )
	fprintf( sfd, "DisplaySize: %d\n", sf->display_size );
    fprintf( sfd, "AntiAlias: %d\n", sf->display_antialias );
    fprintf( sfd, "FitToEm: %d\n", sf->display_bbsized );
    {
	int rc, cc, te;
	if ( (te = FVWinInfo(sf->fv,&cc,&rc))!= -1 )
	    fprintf( sfd, "WinInfo: %d %d %d\n", te, cc, rc );
	else if ( sf->top_enc!=-1 )
	    fprintf( sfd, "WinInfo: %d %d %d\n", sf->top_enc, sf->desired_col_cnt,
		sf->desired_row_cnt);
    }
    if ( sf->onlybitmaps!=0 )
	fprintf( sfd, "OnlyBitmaps: %d\n", sf->onlybitmaps );
    if ( sf->private!=NULL )
	SFDDumpPrivate(sfd,sf->private);
#if HANYANG
    if ( sf->rules!=NULL )
	SFDDumpCompositionRules(sfd,sf->rules);
#endif
    if ( sf->gridsplines!=NULL ) {
	fprintf(sfd, "Grid\n" );
	SFDDumpSplineSet(sfd,sf->gridsplines);
    }
    if ( sf->texdata.type!=tex_unset ) {
	fprintf(sfd, "TeXData: %d %d", sf->texdata.type, sf->texdata.designsize );
	for ( i=0; i<22; ++i )
	    fprintf(sfd, " %d", sf->texdata.params[i]);
	putc('\n',sfd);
    }
    if ( sf->anchor!=NULL ) {
	AnchorClass *an;
	fprintf(sfd, "AnchorClass: ");
	for ( an=sf->anchor; an!=NULL; an=an->next ) {
	    SFDDumpUTF7Str(sfd,an->name);
	    if ( an->feature_tag==0 )
		fprintf( sfd, "0 ");
	    else
		fprintf( sfd, "%c%c%c%c ",
			an->feature_tag>>24, (an->feature_tag>>16)&0xff,
			(an->feature_tag>>8)&0xff, an->feature_tag&0xff );
	    fprintf( sfd, "%d %d %d %d ", an->flags, an->script_lang_index,
		    an->merge_with, an->type );
	}
	putc('\n',sfd);
    }

    if ( sf->subfontcnt!=0 ) {
	int max;
	for ( i=max=0; i<sf->subfontcnt; ++i )
	    if ( max<sf->subfonts[i]->charcnt )
		max = sf->subfonts[i]->charcnt;
	fprintf(sfd, "BeginSubFonts: %d %d\n", sf->subfontcnt, max );
	for ( i=0; i<sf->subfontcnt; ++i )
	    SFD_Dump(sfd,sf->subfonts[i]);
	fprintf(sfd, "EndSubFonts\n" );
    } else {
	if ( sf->cidmaster!=NULL )
	    realcnt = -1;
	else {
	    realcnt = 0;
	    for ( i=0; i<sf->charcnt; ++i ) if ( !SFDOmit(sf->chars[i]) )
		++realcnt;
	}
	fprintf(sfd, "BeginChars: %d %d\n", sf->charcnt, realcnt );
	for ( i=0; i<sf->charcnt; ++i ) {
	    if ( !SFDOmit(sf->chars[i]) )
		SFDDumpChar(sfd,sf->chars[i]);
	    GProgressNext();
	}
	fprintf(sfd, "EndChars\n" );
    }
	
    if ( sf->bitmaps!=NULL )
	GProgressChangeLine2R(_STR_SavingBitmaps);
    for ( bdf = sf->bitmaps; bdf!=NULL; bdf=bdf->next )
	SFDDumpBitmapFont(sfd,bdf);
    fprintf(sfd, sf->cidmaster==NULL?"EndSplineFont\n":"EndSubSplineFont\n" );
}

static void SFD_MMDump(FILE *sfd,SplineFont *sf) {
    MMSet *mm = sf->mm;
    int max, i, j;

    fprintf( sfd, "MMCounts: %d %d\n", mm->instance_count, mm->axis_count );
    fprintf( sfd, "MMAxis:" );
    for ( i=0; i<mm->axis_count; ++i )
	fprintf( sfd, " %s", mm->axes[i]);
    putc('\n',sfd);
    fprintf( sfd, "MMPositions:" );
    for ( i=0; i<mm->axis_count*mm->instance_count; ++i )
	fprintf( sfd, " %g", mm->positions[i]);
    putc('\n',sfd);
    fprintf( sfd, "MMWeights:" );
    for ( i=0; i<mm->instance_count; ++i )
	fprintf( sfd, " %g", mm->defweights[i]);
    putc('\n',sfd);
    for ( i=0; i<mm->axis_count; ++i ) {
	fprintf( sfd, "MMAxisMap: %d %d", i, mm->axismaps[i].points );
	for ( j=0; j<mm->axismaps[i].points; ++j )
	    fprintf( sfd, " %g=>%g", mm->axismaps[i].blends[j], mm->axismaps[i].designs[j]);
	fputc('\n',sfd);
    }
    fprintf( sfd, "MMCDV:\n" );
    fputs(mm->cdv,sfd);
    fprintf( sfd, "\nEndMMSubroutine\n" );
    fprintf( sfd, "MMNDV:\n" );
    fputs(mm->ndv,sfd);
    fprintf( sfd, "\nEndMMSubroutine\n" );

    for ( i=max=0; i<mm->instance_count; ++i )
	if ( max<mm->instances[i]->charcnt )
	    max = mm->instances[i]->charcnt;
    fprintf(sfd, "BeginMMFonts: %d %d\n", mm->instance_count+1, max );
    for ( i=0; i<mm->instance_count; ++i )
	SFD_Dump(sfd,mm->instances[i]);
    SFD_Dump(sfd,mm->normal);
    fprintf(sfd, "EndMMFonts\n" );
}

static void SFDDump(FILE *sfd,SplineFont *sf) {
    int i, realcnt;
    BDFFont *bdf;

    realcnt = sf->charcnt;
    if ( sf->subfontcnt!=0 ) {
	for ( i=0; i<sf->subfontcnt; ++i )
	    if ( realcnt<sf->subfonts[i]->charcnt )
		realcnt = sf->subfonts[i]->charcnt;
    }
    for ( i=0, bdf = sf->bitmaps; bdf!=NULL; bdf=bdf->next, ++i );
    GProgressStartIndicatorR(10,_STR_Saving,_STR_SavingDb,_STR_SavingOutlines,
	    realcnt,i+1);
    GProgressEnableStop(false);
    fprintf(sfd, "SplineFontDB: %.1f\n", 1.0 );
    if ( sf->mm != NULL )
	SFD_MMDump(sfd,sf->mm->normal);
    else
	SFD_Dump(sfd,sf);
    GProgressEndIndicator();
}

int SFDWrite(char *filename,SplineFont *sf) {
    FILE *sfd = fopen(filename,"w");
    char *oldloc;

    if ( sfd==NULL )
return( 0 );
    oldloc = setlocale(LC_NUMERIC,"C");
    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;
    SFDDump(sfd,sf);
    setlocale(LC_NUMERIC,oldloc);
return( !ferror(sfd) && fclose(sfd)==0 );
}

int SFDWriteBak(SplineFont *sf) {
    char *buf/*, *pt, *bpt*/;

    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;
    buf = galloc(strlen(sf->filename)+10);
#if 1
    strcpy(buf,sf->filename);
    strcat(buf,"~");
#else
    pt = strrchr(sf->filename,'.');
    if ( pt==NULL || pt<strrchr(sf->filename,'/'))
	pt = sf->filename+strlen(sf->filename);
    strcpy(buf,sf->filename);
    bpt = buf + (pt-sf->filename);
    *bpt++ = '~';
    strcpy(bpt,pt);
#endif
    rename(sf->filename,buf);
    free(buf);

return( SFDWrite(sf->filename,sf));
}

/* ********************************* INPUT ********************************** */

static char *getquotedeol(FILE *sfd) {
    char *pt, *str, *end;
    int ch;

    pt = str = galloc(100); end = str+100;
    while ( isspace(ch = getc(sfd)) && ch!='\r' && ch!='\n' );
    while ( ch!='\n' && ch!='\r' && ch!=EOF ) {
	if ( ch=='\\' ) {
	    ch = getc(sfd);
	    if ( ch=='n' ) ch='\n';
	}
	if ( pt>=end ) {
	    pt = grealloc(str,end-str+100);
	    end = pt+(end-str)+100;
	    str = pt;
	    pt = end-100;
	}
	*pt++ = ch;
	ch = getc(sfd);
    }
    *pt='\0';
return( str );
}

static int geteol(FILE *sfd, char *tokbuf) {
    char *pt=tokbuf, *end = tokbuf+2000-2; int ch;

    while ( isspace(ch = getc(sfd)) && ch!='\r' && ch!='\n' );
    while ( ch!='\n' && ch!='\r' && ch!=EOF ) {
	if ( pt<end ) *pt++ = ch;
	ch = getc(sfd);
    }
    *pt='\0';
return( pt!=tokbuf?1:ch==EOF?-1: 0 );
}

static int getprotectedname(FILE *sfd, char *tokbuf) {
    char *pt=tokbuf, *end = tokbuf+100-2; int ch;

    while ( (ch = getc(sfd))==' ' || ch=='\t' );
    while ( ch!=EOF && !isspace(ch) && ch!='[' && ch!=']' && ch!='{' && ch!='}' && ch!='<' && ch!='%' ) {
	if ( pt<end ) *pt++ = ch;
	ch = getc(sfd);
    }
    if ( pt==tokbuf && ch!=EOF )
	*pt++ = ch;
    else
	ungetc(ch,sfd);
    *pt='\0';
return( pt!=tokbuf?1:ch==EOF?-1: 0 );
}

static int getname(FILE *sfd, char *tokbuf) {
    int ch;

    while ( isspace(ch = getc(sfd)));
    ungetc(ch,sfd);
return( getprotectedname(sfd,tokbuf));
}

static int getint(FILE *sfd, int *val) {
    char tokbuf[100]; int ch;
    char *pt=tokbuf, *end = tokbuf+100-2;

    while ( isspace(ch = getc(sfd)));
    if ( ch=='-' || ch=='+' ) {
	*pt++ = ch;
	ch = getc(sfd);
    }
    while ( isdigit(ch)) {
	if ( pt<end ) *pt++ = ch;
	ch = getc(sfd);
    }
    *pt='\0';
    ungetc(ch,sfd);
    *val = strtol(tokbuf,NULL,10);
return( pt!=tokbuf?1:ch==EOF?-1: 0 );
}

static int gethex(FILE *sfd, int *val) {
    char tokbuf[100]; int ch;
    char *pt=tokbuf, *end = tokbuf+100-2;

    while ( isspace(ch = getc(sfd)));
    if ( ch=='-' || ch=='+' ) {
	*pt++ = ch;
	ch = getc(sfd);
    }
    while ( isdigit(ch) || (ch>='a' && ch<='f') || (ch>='A' && ch<='F')) {
	if ( pt<end ) *pt++ = ch;
	ch = getc(sfd);
    }
    *pt='\0';
    ungetc(ch,sfd);
    *val = strtoul(tokbuf,NULL,16);
return( pt!=tokbuf?1:ch==EOF?-1: 0 );
}

static int getsint(FILE *sfd, int16 *val) {
    int val2;
    int ret = getint(sfd,&val2);
    *val = val2;
return( ret );
}

static int getusint(FILE *sfd, uint16 *val) {
    int val2;
    int ret = getint(sfd,&val2);
    *val = val2;
return( ret );
}

static int getreal(FILE *sfd, real *val) {
    char tokbuf[100];
    int ch;
    char *pt=tokbuf, *end = tokbuf+100-2, *nend;

    while ( isspace(ch = getc(sfd)));
    if ( ch!='e' && ch!='E' )		/* real's can't begin with exponants */
	while ( isdigit(ch) || ch=='-' || ch=='+' || ch=='e' || ch=='E' || ch=='.' || ch==',' ) {
	    if ( pt<end ) *pt++ = ch;
	    ch = getc(sfd);
	}
    *pt='\0';
    ungetc(ch,sfd);
    *val = strtod(tokbuf,&nend);
    /* Beware of different locals! */
    if ( *nend!='\0' ) {
	if ( *nend=='.' )
	    *nend = ',';
	else if ( *nend==',' )
	    *nend = '.';
	*val = strtod(tokbuf,&nend);
    }
return( pt!=tokbuf && *nend=='\0'?1:ch==EOF?-1: 0 );
}

static int Dec85(struct enc85 *dec) {
    int ch1, ch2, ch3, ch4, ch5;
    unsigned int val;

    if ( dec->pos<0 ) {
	while ( isspace(ch1=getc(dec->sfd)));
	if ( ch1=='z' ) {
	    dec->sofar[0] = dec->sofar[1] = dec->sofar[2] = dec->sofar[3] = 0;
	    dec->pos = 3;
	} else {
	    while ( isspace(ch2=getc(dec->sfd)));
	    while ( isspace(ch3=getc(dec->sfd)));
	    while ( isspace(ch4=getc(dec->sfd)));
	    while ( isspace(ch5=getc(dec->sfd)));
	    val = ((((ch1-'!')*85+ ch2-'!')*85 + ch3-'!')*85 + ch4-'!')*85 + ch5-'!';
	    dec->sofar[3] = val>>24;
	    dec->sofar[2] = val>>16;
	    dec->sofar[1] = val>>8;
	    dec->sofar[0] = val;
	    dec->pos = 3;
	}
    }
return( dec->sofar[dec->pos--] );
}

static void rle2image(struct enc85 *dec,int rlelen,struct _GImage *base) {
    uint8 *pt, *end;
    int r,c,set, cnt, ch, ch2;
    int i;

    r = c = 0; set = 1; pt = base->data; end = pt + base->bytes_per_line*base->height;
    memset(base->data,0xff,end-pt);
    while ( rlelen>0 ) {
	if ( pt>=end ) {
	    fprintf( stderr, "IError: RLE failure\n" );
	    while ( rlelen>0 ) { Dec85(dec); --rlelen; }
    break;
	}
	ch = Dec85(dec);
	--rlelen;
	if ( ch==255 ) {
	    ch2 = Dec85(dec);
	    cnt = (ch2<<8) + Dec85(dec);
	    rlelen -= 2;
	} else
	    cnt = ch;
	if ( ch==255 && ch2==0 && cnt<255 ) {
	    /* Line duplication */
	    for ( i=0; i<cnt; ++i ) {
		memcpy(pt,base->data+(r-1)*base->bytes_per_line,base->bytes_per_line);
		++r;
		pt += base->bytes_per_line;
	    }
	    set = 1;
	} else {
	    if ( !set ) {
		for ( i=0; i<cnt; ++i )
		    pt[(c+i)>>3] &= ((~0x80)>>((c+i)&7));
	    }
	    c += cnt;
	    set = 1-set;
	    if ( c>=base->width ) {
		++r;
		pt += base->bytes_per_line;
		c = 0; set = 1;
	    }
	}
    }
}

static ImageList *SFDGetImage(FILE *sfd) {
    /* We've read the image token */
    int width, height, image_type, bpl, clutlen, trans, rlelen;
    struct _GImage *base;
    GImage *image;
    ImageList *img;
    struct enc85 dec;
    int i, ch;

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;

    getint(sfd,&width);
    getint(sfd,&height);
    getint(sfd,&image_type);
    getint(sfd,&bpl);
    getint(sfd,&clutlen);
    gethex(sfd,&trans);
    image = GImageCreate(image_type,width,height);
    base = image->list_len==0?image->u.image:image->u.images[0];
    img = gcalloc(1,sizeof(ImageList));
    img->image = image;
    getreal(sfd,&img->xoff);
    getreal(sfd,&img->yoff);
    getreal(sfd,&img->xscale);
    getreal(sfd,&img->yscale);
    while ( (ch=getc(sfd))==' ' || ch=='\t' );
    ungetc(ch,sfd);
    rlelen = 0;
    if ( isdigit(ch))
	getint(sfd,&rlelen);
    base->trans = trans;
    if ( clutlen!=0 ) {
	if ( base->clut==NULL )
	    base->clut = gcalloc(1,sizeof(GClut));
	base->clut->clut_len = clutlen;
	base->clut->trans_index = trans;
	for ( i=0;i<clutlen; ++i ) {
	    int r,g,b;
	    r = Dec85(&dec);
	    g = Dec85(&dec);
	    b = Dec85(&dec);
	    base->clut->clut[i] = (r<<16)|(g<<8)|b;
	}
    }
    if ( rlelen!=0 ) {
	rle2image(&dec,rlelen,base);
    } else {
	for ( i=0; i<height; ++i ) {
	    if ( image_type==it_true ) {
		int *ipt = (int *) (base->data + i*base->bytes_per_line);
		int *iend = (int *) (base->data + (i+1)*base->bytes_per_line);
		int r,g,b;
		while ( ipt<iend ) {
		    r = Dec85(&dec);
		    g = Dec85(&dec);
		    b = Dec85(&dec);
		    *ipt++ = (r<<16)|(g<<8)|b;
		}
	    } else {
		uint8 *pt = (uint8 *) (base->data + i*base->bytes_per_line);
		uint8 *end = (uint8 *) (base->data + (i+1)*base->bytes_per_line);
		while ( pt<end ) {
		    *pt++ = Dec85(&dec);
		}
	    }
	}
    }
    img->bb.minx = img->xoff; img->bb.maxy = img->yoff;
    img->bb.maxx = img->xoff + GImageGetWidth(img->image)*img->xscale;
    img->bb.miny = img->yoff - GImageGetHeight(img->image)*img->yscale;
    /* In old sfd files I failed to recognize bitmap pngs as bitmap, so put */
    /*  in a little check here that converts things which should be bitmap to */
    /*  bitmap */ /* Eventually it can be removed as all old sfd files get */
    /*  converted. 22/10/2002 */
    if ( base->image_type==it_index && base->clut!=NULL && base->clut->clut_len==2 )
	img->image = ImageAlterClut(img->image);
return( img );
}

static void SFDGetType1(FILE *sfd, SplineChar *sc) {
    /* We've read the OrigType1 token (this is now obselete, but parse it in case there are any old sfds) */
    int len;
    struct enc85 dec;

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;

    getint(sfd,&len);
    while ( --len >= 0 )
	Dec85(&dec);
}

static void SFDGetTtfInstrs(FILE *sfd, SplineChar *sc) {
    /* We've read the TtfInstr token, it is followed by a byte count */
    /* and then the instructions in enc85 format */
    int i,len;
    struct enc85 dec;

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;

    getint(sfd,&len);
    sc->ttf_instrs = galloc(len);
    sc->ttf_instrs_len = len;
    for ( i=0; i<len; ++i )
	sc->ttf_instrs[i] = Dec85(&dec);
}

static struct ttf_table *SFDGetTtfTable(FILE *sfd, SplineFont *sf,struct ttf_table *lasttab) {
    /* We've read the TtfTable token, it is followed by a tag and a byte count */
    /* and then the instructions in enc85 format */
    int i,len, ch;
    struct enc85 dec;
    struct ttf_table *tab = chunkalloc(sizeof(struct ttf_table));

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;

    while ( (ch=getc(sfd))==' ' );
    tab->tag = (ch<<24)|(getc(sfd)<<16);
    tab->tag |= getc(sfd)<<8;
    tab->tag |= getc(sfd);

    getint(sfd,&len);
    tab->data = galloc(len);
    tab->len = len;
    for ( i=0; i<len; ++i )
	tab->data[i] = Dec85(&dec);

    if ( lasttab==NULL )
	sf->ttf_tables = tab;
    else
	lasttab->next = tab;
return( tab );
}

static int SFDCloseCheck(SplinePointList *spl,int order2) {
    if ( spl->first!=spl->last &&
	    RealNear(spl->first->me.x,spl->last->me.x) &&
	    RealNear(spl->first->me.y,spl->last->me.y)) {
	SplinePoint *oldlast = spl->last;
	spl->first->prevcp = oldlast->prevcp;
	spl->first->noprevcp = false;
	oldlast->prev->from->next = NULL;
	spl->last = oldlast->prev->from;
	chunkfree(oldlast->prev,sizeof(*oldlast));
	chunkfree(oldlast,sizeof(*oldlast));
	SplineMake(spl->last,spl->first,order2);
	spl->last = spl->first;
return( true );
    }
return( false );
}

static void SFDGetHintMask(FILE *sfd,HintMask *hintmask) {
    int nibble = 0, ch;

    memset(hintmask,0,sizeof(HintMask));
    forever {
	ch = getc(sfd);
	if ( isdigit(ch))
	    ch -= '0';
	else if ( ch>='a' && ch<='f' )
	    ch -= 'a'-10;
	else if ( ch>='A' && ch<='F' )
	    ch -= 'A'-10;
	else {
	    ungetc(ch,sfd);
    break;
	}
	if ( nibble<2*HntMax/8 )
	    (*hintmask)[nibble>>1] |= ch<<(4*(1-(nibble&1)));
	++nibble;
    }
}

static SplineSet *SFDGetSplineSet(SplineFont *sf,FILE *sfd) {
    SplinePointList *cur=NULL, *head=NULL;
    BasePoint current;
    real stack[100];
    int sp=0;
    SplinePoint *pt;
    int ch;
    char tok[100];
    int ttfindex = 0;

    current.x = current.y = 0;
    while ( 1 ) {
	while ( getreal(sfd,&stack[sp])==1 )
	    if ( sp<99 )
		++sp;
	while ( isspace(ch=getc(sfd)));
	if ( ch=='E' || ch=='e' || ch==EOF )
    break;
	pt = NULL;
	if ( ch=='l' || ch=='m' ) {
	    if ( sp>=2 ) {
		current.x = stack[sp-2];
		current.y = stack[sp-1];
		sp -= 2;
		pt = chunkalloc(sizeof(SplinePoint));
		pt->me = current;
		pt->noprevcp = true; pt->nonextcp = true;
		if ( ch=='m' ) {
		    SplinePointList *spl = chunkalloc(sizeof(SplinePointList));
		    spl->first = spl->last = pt;
		    if ( cur!=NULL ) {
			if ( SFDCloseCheck(cur,sf->order2))
			    --ttfindex;
			cur->next = spl;
		    } else
			head = spl;
		    cur = spl;
		} else {
		    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
			if ( cur->last->nextcpindex==0xfffe )
			    cur->last->nextcpindex = 0xffff;
			SplineMake(cur->last,pt,sf->order2);
			cur->last = pt;
		    }
		}
	    } else
		sp = 0;
	} else if ( ch=='c' ) {
	    if ( sp>=6 ) {
		current.x = stack[sp-2];
		current.y = stack[sp-1];
		if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
		    cur->last->nextcp.x = stack[sp-6];
		    cur->last->nextcp.y = stack[sp-5];
		    cur->last->nonextcp = false;
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->prevcp.x = stack[sp-4];
		    pt->prevcp.y = stack[sp-3];
		    pt->me = current;
		    pt->nonextcp = true;
		    if ( cur->last->nextcpindex==0xfffe )
			cur->last->nextcpindex = ttfindex++;
		    else if ( cur->last->nextcpindex!=0xffff )
			ttfindex = cur->last->nextcpindex+1;
		    SplineMake(cur->last,pt,sf->order2);
		    cur->last = pt;
		}
		sp -= 6;
	    } else
		sp = 0;
	}
	if ( pt!=NULL ) {
	    int val;
	    getint(sfd,&val);
	    pt->pointtype = (val&3);
	    pt->selected = val&4?1:0;
	    pt->nextcpdef = val&8?1:0;
	    pt->prevcpdef = val&0x10?1:0;
	    pt->roundx = val&0x20?1:0;
	    pt->roundy = val&0x40?1:0;
	    if ( val&0x80 )
		pt->ttfindex = 0xffff;
	    else
		pt->ttfindex = ttfindex++;
	    pt->nextcpindex = 0xfffe;
	    ch = getc(sfd);
	    if ( ch=='x' ) {
		pt->hintmask = chunkalloc(sizeof(HintMask));
		SFDGetHintMask(sfd,pt->hintmask);
	    } else if ( ch!=',' )
		ungetc(ch,sfd);
	    else {
		ch = getc(sfd);
		if ( ch==',' )
		    pt->ttfindex = 0xfffe;
		else {
		    ungetc(ch,sfd);
		    getint(sfd,&val);
		    pt->ttfindex = val;
		    getc(sfd);	/* skip comma */
		    if ( val!=-1 )
			ttfindex = val+1;
		}
		ch = getc(sfd);
		if ( ch=='\r' || ch=='\n' )
		    ungetc(ch,sfd);
		else {
		    ungetc(ch,sfd);
		    getint(sfd,&val);
		    pt->nextcpindex = val;
		    if ( val!=-1 )
			ttfindex = val+1;
		}
	    }
	}
    }
    if ( cur!=NULL )
	SFDCloseCheck(cur,sf->order2);
    getname(sfd,tok);
return( head );
}

static void SFDGetMinimumDistances(FILE *sfd, SplineChar *sc) {
    SplineSet *ss;
    SplinePoint *sp;
    int pt,i, val, err;
    int ch;
    SplinePoint **mapping=NULL;
    MinimumDistance *last, *md;

    for ( i=0; i<2; ++i ) {
	pt = 0;
	for ( ss = sc->splines; ss!=NULL; ss=ss->next ) {
	    for ( sp=ss->first; ; ) {
		if ( mapping!=NULL ) mapping[pt] = sp;
		pt++;
		if ( sp->next == NULL )
	    break;
		sp = sp->next->to;
		if ( sp==ss->first )
	    break;
	    }
	}
	if ( mapping==NULL )
	    mapping = gcalloc(pt,sizeof(SplineChar *));
    }

    last = NULL;
    for ( ch=getc(sfd); ch!=EOF && ch!='\n'; ch=getc(sfd)) {
	err = false;
	while ( isspace(ch) && ch!='\n' ) ch=getc(sfd);
	if ( ch=='\n' )
    break;
	md = chunkalloc(sizeof(MinimumDistance));
	if ( ch=='x' ) md->x = true;
	getint(sfd,&val);
	if ( val<-1 || val>=pt ) {
	    fprintf( stderr, "Internal Error: Minimum Distance specifies bad point (%d) in sfd file\n", val );
	    err = true;
	} else if ( val!=-1 ) {
	    md->sp1 = mapping[val];
	    md->sp1->dontinterpolate = true;
	}
	ch = getc(sfd);
	if ( ch!=',' ) {
	    fprintf( stderr, "Internal Error: Minimum Distance lacks a comma where expected\n" );
	    err = true;
	}
	getint(sfd,&val);
	if ( val<-1 || val>=pt ) {
	    fprintf( stderr, "Internal Error: Minimum Distance specifies bad point (%d) in sfd file\n", val );
	    err = true;
	} else if ( val!=-1 ) {
	    md->sp2 = mapping[val];
	    md->sp2->dontinterpolate = true;
	}
	if ( !err ) {
	    if ( last==NULL )
		sc->md = md;
	    else
		last->next = md;
	    last = md;
	} else
	    chunkfree(md,sizeof(MinimumDistance));
    }
    free(mapping);
}

static HintInstance *SFDReadHintInstances(FILE *sfd, StemInfo *stem) {
    HintInstance *head=NULL, *last=NULL, *cur;
    real begin, end;
    int ch;

    while ( (ch=getc(sfd))==' ' || ch=='\t' );
    if ( ch=='G' ) {
	stem->ghost = true;
	while ( (ch=getc(sfd))==' ' || ch=='\t' );
    }
    if ( ch!='<' ) {
	ungetc(ch,sfd);
return(NULL);
    }
    while ( getreal(sfd,&begin)==1 && getreal(sfd,&end)) {
	cur = chunkalloc(sizeof(HintInstance));
	cur->begin = begin;
	cur->end = end;
	if ( head == NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
    while ( (ch=getc(sfd))==' ' || ch=='\t' );
    if ( ch!='>' )
	ungetc(ch,sfd);
return( head );
}

static StemInfo *SFDReadHints(FILE *sfd) {
    StemInfo *head=NULL, *last=NULL, *cur;
    real start, width;

    while ( getreal(sfd,&start)==1 && getreal(sfd,&width)) {
	cur = chunkalloc(sizeof(StemInfo));
	cur->start = start;
	cur->width = width;
	cur->where = SFDReadHintInstances(sfd,cur);
	if ( head == NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
return( head );
}

static DStemInfo *SFDReadDHints(FILE *sfd) {
    DStemInfo *head=NULL, *last=NULL, *cur;
    DStemInfo d;

    memset(&d,'\0',sizeof(d));
    while ( getreal(sfd,&d.leftedgetop.x)==1 && getreal(sfd,&d.leftedgetop.y) &&
	    getreal(sfd,&d.rightedgetop.x)==1 && getreal(sfd,&d.rightedgetop.y) &&
	    getreal(sfd,&d.leftedgebottom.x)==1 && getreal(sfd,&d.leftedgebottom.y) &&
	    getreal(sfd,&d.rightedgebottom.x)==1 && getreal(sfd,&d.rightedgebottom.y) ) {
	cur = chunkalloc(sizeof(DStemInfo));
	*cur = d;
	if ( head == NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
return( head );
}

static AnchorPoint *SFDReadAnchorPoints(FILE *sfd,SplineChar *sc,AnchorPoint *lastap) {
    AnchorPoint *ap = chunkalloc(sizeof(AnchorPoint));
    AnchorClass *an;
    unichar_t *name;
    char tok[200];

    name = SFDReadUTF7Str(sfd);
    for ( an=sc->parent->anchor; an!=NULL && u_strcmp(an->name,name)!=0; an=an->next );
    free(name);
    ap->anchor = an;
    getreal(sfd,&ap->me.x);
    getreal(sfd,&ap->me.y);
    ap->type = -1;
    if ( getname(sfd,tok)==1 ) {
	if ( strcmp(tok,"mark")==0 )
	    ap->type = at_mark;
	else if ( strcmp(tok,"basechar")==0 )
	    ap->type = at_basechar;
	else if ( strcmp(tok,"baselig")==0 )
	    ap->type = at_baselig;
	else if ( strcmp(tok,"basemark")==0 )
	    ap->type = at_basemark;
	else if ( strcmp(tok,"entry")==0 )
	    ap->type = at_centry;
	else if ( strcmp(tok,"exit")==0 )
	    ap->type = at_cexit;
    }
    getint(sfd,&ap->lig_index);
    if ( ap->anchor==NULL || ap->type==-1 ) {
	chunkfree(ap,sizeof(AnchorPoint));
return( lastap );
    }
    if ( lastap==NULL )
	sc->anchor = ap;
    else
	lastap->next = ap;
return( ap );
}

static RefChar *SFDGetRef(FILE *sfd) {
    RefChar *rf;
    int enc=0, ch;

    rf = chunkalloc(sizeof(RefChar));
    getint(sfd,&enc);
    rf->local_enc = enc;
    if ( getint(sfd,&enc))
	rf->unicode_enc = enc;
    while ( isspace(ch=getc(sfd)));
    if ( ch=='S' ) rf->selected = true;
    getreal(sfd,&rf->transform[0]);
    getreal(sfd,&rf->transform[1]);
    getreal(sfd,&rf->transform[2]);
    getreal(sfd,&rf->transform[3]);
    getreal(sfd,&rf->transform[4]);
    getreal(sfd,&rf->transform[5]);
return( rf );
}

/* I used to create multiple ligatures by putting ";" between them */
/* that is the component string for "ffi" was "ff i ; f f i" */
/* Now I want to have seperate ligature structures for each */
static PST *LigaCreateFromOldStyleMultiple(PST *liga) {
    char *pt;
    PST *new, *last=liga;
    while ( (pt = strrchr(liga->u.lig.components,';'))!=NULL ) {
	new = chunkalloc(sizeof( PST ));
	*new = *liga;
	new->u.lig.components = copy(pt+1);
	last->next = new;
	last = new;
	*pt = '\0';
    }
return( last );
}

#ifdef PFAEDIT_CONFIG_CVT_OLD_MAC_FEATURES
static struct { int feature, setting; uint32 tag; } formertags[] = {
    { 1, 6, CHR('M','L','O','G') },
    { 1, 8, CHR('M','R','E','B') },
    { 1, 10, CHR('M','D','L','G') },
    { 1, 12, CHR('M','S','L','G') },
    { 1, 14, CHR('M','A','L','G') },
    { 8, 0, CHR('M','S','W','I') },
    { 8, 2, CHR('M','S','W','F') },
    { 8, 4, CHR('M','S','L','I') },
    { 8, 6, CHR('M','S','L','F') },
    { 8, 8, CHR('M','S','N','F') },
    { 22, 1, CHR('M','W','I','D') },
    { 27, 1, CHR('M','U','C','M') },
    { 103, 2, CHR('M','W','I','D') },
    { -1, -1, 0xffffffff },
};

static void CvtOldMacFeature(PST *pst) {
    int i;

    if ( pst->macfeature )
return;
    for ( i=0; formertags[i].feature!=-1 ; ++i ) {
	if ( pst->tag == formertags[i].tag ) {
	    pst->macfeature = true;
	    pst->tag = (formertags[i].feature<<16) | formertags[i].setting;
return;
	}
    }
}
#endif

static SplineChar *SFDGetChar(FILE *sfd,SplineFont *sf) {
    SplineChar *sc;
    char tok[2000], ch;
    RefChar *lastr=NULL, *ref;
    ImageList *lasti=NULL, *img;
    AnchorPoint *lastap = NULL;
    int isliga, ispos, issubs, ismult, islcar, ispair, temp, i;
    PST *last = NULL;
    uint32 script = 0;

    if ( getname(sfd,tok)!=1 )
return( NULL );
    if ( strcmp(tok,"StartChar:")!=0 )
return( NULL );
    if ( getname(sfd,tok)!=1 )
return( NULL );

    sc = SplineCharCreate();
    sc->name = copy(tok);
    sc->vwidth = sf->ascent+sf->descent;
    sc->parent = sf;
    while ( 1 ) {
	if ( getname(sfd,tok)!=1 ) {
	    SplineCharFree(sc);
return( NULL );
	}
	if ( strmatch(tok,"Encoding:")==0 ) {
	    getint(sfd,&sc->enc);
	    getint(sfd,&sc->unicodeenc);
	    while ( (ch=getc(sfd))==' ' || ch=='\t' );
	    ungetc(ch,sfd);
	    if ( ch!='\n' && ch!='\r' ) {
		getusint(sfd,&sc->orig_pos);
	    }
	} else if ( strmatch(tok,"OldEncoding:")==0 ) {
	    getint(sfd,&sc->old_enc);
        } else if ( strmatch(tok,"Script:")==0 ) {
	    /* Obsolete. But still used for parsing obsolete ligature/subs tags */
            while ( (ch=getc(sfd))==' ' || ch=='\t' );
            if ( ch=='\n' || ch=='\r' )
                script = 0;
            else {
                script = ch<<24;
                script |= (getc(sfd)<<16);
                script |= (getc(sfd)<<8);
                script |= getc(sfd);
            }
	} else if ( strmatch(tok,"Width:")==0 ) {
	    getsint(sfd,&sc->width);
	} else if ( strmatch(tok,"VWidth:")==0 ) {
	    getsint(sfd,&sc->vwidth);
	} else if ( strmatch(tok,"GlyphClass:")==0 ) {
	    getint(sfd,&temp);
	    sc->glyph_class = temp;
	} else if ( strmatch(tok,"Flags:")==0 ) {
	    while ( isspace(ch=getc(sfd)) && ch!='\n' && ch!='\r');
	    while ( ch!='\n' && ch!='\r' ) {
		if ( ch=='H' ) sc->changedsincelasthinted=true;
		else if ( ch=='M' ) sc->manualhints = true;
		else if ( ch=='W' ) sc->widthset = true;
		else if ( ch=='O' ) sc->wasopen = true;
		ch = getc(sfd);
	    }
#if HANYANG
	} else if ( strmatch(tok,"CompositionUnit:")==0 ) {
	    getsint(sfd,&sc->jamo);
	    getsint(sfd,&sc->varient);
	    sc->compositionunit = true;
#endif
	} else if ( strmatch(tok,"HStem:")==0 ) {
	    sc->hstem = SFDReadHints(sfd);
	    SCGuessHHintInstancesList(sc);		/* For reading in old .sfd files with no HintInstance data */
	    sc->hconflicts = StemListAnyConflicts(sc->hstem);
	} else if ( strmatch(tok,"VStem:")==0 ) {
	    sc->vstem = SFDReadHints(sfd);
	    SCGuessVHintInstancesList(sc);		/* For reading in old .sfd files */
	    sc->vconflicts = StemListAnyConflicts(sc->vstem);
	} else if ( strmatch(tok,"DStem:")==0 ) {
	    sc->dstem = SFDReadDHints(sfd);
	} else if ( strmatch(tok,"CounterMasks:")==0 ) {
	    getsint(sfd,&sc->countermask_cnt);
	    sc->countermasks = gcalloc(sc->countermask_cnt,sizeof(HintMask));
	    for ( i=0; i<sc->countermask_cnt; ++i )
		SFDGetHintMask(sfd,&sc->countermasks[i]);
	} else if ( strmatch(tok,"AnchorPoint:")==0 ) {
	    lastap = SFDReadAnchorPoints(sfd,sc,lastap);
	} else if ( strmatch(tok,"Fore")==0 ) {
	    sc->splines = SFDGetSplineSet(sf,sfd);
	} else if ( strmatch(tok,"MinimumDistance:")==0 ) {
	    SFDGetMinimumDistances(sfd,sc);
	} else if ( strmatch(tok,"Back")==0 ) {
	    sc->backgroundsplines = SFDGetSplineSet(sf,sfd);
	} else if ( strmatch(tok,"Ref:")==0 ) {
	    ref = SFDGetRef(sfd);
	    if ( lastr==NULL )
		sc->refs = ref;
	    else
		lastr->next = ref;
	    lastr = ref;
	} else if ( strmatch(tok,"OrigType1:")==0 ) {	/* Accept, slurp, ignore contents */
	    SFDGetType1(sfd,sc);
	} else if ( strmatch(tok,"TtfInstrs:")==0 ) {
	    SFDGetTtfInstrs(sfd,sc);
	} else if ( strmatch(tok,"Image:")==0 ) {
	    img = SFDGetImage(sfd);
	    if ( lasti==NULL )
		sc->backimages = img;
	    else
		lasti->next = img;
	    lasti = img;
	} else if ( strmatch(tok,"Kerns:")==0 ||
		strmatch(tok,"KernsSLI:")==0 ||
		strmatch(tok,"KernsSLIF:")==0 ||
		strmatch(tok,"VKernsSLIF:")==0 ) {
	    KernPair *kp, *last=NULL;
	    int index, off, sli, flags=0;
	    int hassli = (strmatch(tok,"KernsSLI:")==0);
	    int isv = *tok=='V';
	    if ( strmatch(tok,"KernsSLIF:")==0 ) hassli=2;
	    if ( strmatch(tok,"VKernsSLIF:")==0 ) hassli=2;
	    while ( (hassli==1 && fscanf(sfd,"%d %d %d", &index, &off, &sli )==3) ||
		    (hassli==2 && fscanf(sfd,"%d %d %d %d", &index, &off, &sli, &flags )==4) ||
		    (hassli==0 && fscanf(sfd,"%d %d", &index, &off )==2) ) {
		if ( !hassli )
		    sli = SFAddScriptLangIndex(sf,
			    script!=0?script:SCScriptFromUnicode(sc),DEFAULT_LANG);
		if ( sli>=sf->sli_cnt ) {
		    static int complained=false;
		    if ( !complained )
			GDrawError("'%s' in %s has a script index out of bounds: %d",
				isv ? "vkrn" : "kern",
				sc->name, sli );
		    else
			fprintf( stderr, "Internal Error: '%s' in %s has a script index out of bounds: %d",
				isv ? "vkrn" : "kern",
				sc->name, sli );
		    sli = SFAddScriptLangIndex(sf,
			    SCScriptFromUnicode(sc),DEFAULT_LANG);
		    complained = true;
		}
		kp = chunkalloc(sizeof(KernPair));
		kp->sc = (SplineChar *) index;
		kp->off = off;
		kp->sli = sli;
		kp->flags = flags;
		kp->next = NULL;
		if ( last != NULL )
		    last->next = kp;
		else if ( isv )
		    sc->vkerns = kp;
		else
		    sc->kerns = kp;
		last = kp;
	    }
	} else if ( (ispos = (strmatch(tok,"Position:")==0)) ||
		( ispair = (strmatch(tok,"PairPos:")==0)) ||
		( islcar = (strmatch(tok,"LCarets:")==0)) ||
		( isliga = (strmatch(tok,"Ligature:")==0)) ||
		( issubs = (strmatch(tok,"Substitution:")==0)) ||
		( ismult = (strmatch(tok,"MultipleSubs:")==0)) ||
		strmatch(tok,"AlternateSubs:")==0 ) {
	    PST *liga = chunkalloc(sizeof(PST));
	    if ( last==NULL )
		sc->possub = liga;
	    else
		last->next = liga;
	    last = liga;
	    liga->type = ispos ? pst_position :
			 ispair ? pst_pair :
			 islcar ? pst_lcaret :
			 isliga ? pst_ligature :
			 issubs ? pst_substitution :
			 ismult ? pst_multiple :
			 pst_alternate;
	    liga->tag = CHR('l','i','g','a');
	    liga->script_lang_index = 0xffff;
	    while ( (ch=getc(sfd))==' ' || ch=='\t' );
	    if ( isdigit(ch)) {
		int temp;
		ungetc(ch,sfd);
		getint(sfd,&temp);
		liga->flags = temp;
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
	    } else
		liga->flags = PSTDefaultFlags(liga->type,sc);
	    if ( isdigit(ch)) {
		ungetc(ch,sfd);
		getusint(sfd,&liga->script_lang_index);
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
	    } else
		liga->script_lang_index = SFAddScriptLangIndex(sf,
			script!=0?script:SCScriptFromUnicode(sc),DEFAULT_LANG);
	    if ( ch=='\'' ) {
		liga->tag = getc(sfd)<<24;
		liga->tag |= getc(sfd)<<16;
		liga->tag |= getc(sfd)<<8;
		liga->tag |= getc(sfd);
		getc(sfd);	/* Final quote */
	    } else if ( ch=='<' ) {
		getint(sfd,&temp);
		liga->tag = temp<<16;
		getc(sfd);	/* comma */
		getint(sfd,&temp);
		liga->tag |= temp;
		getc(sfd);	/* close '>' */
		liga->macfeature = true;
	    } else
		ungetc(ch,sfd);
	    if ( liga->script_lang_index>=sf->sli_cnt && liga->type!=pst_lcaret ) {
		static int complained=false;
		if ( !complained )
		    GDrawError("'%c%c%c%c' in %s has a script index out of bounds: %d",
			    (liga->tag>>24), (liga->tag>>16)&0xff, (liga->tag>>8)&0xff, liga->tag&0xff,
			    sc->name, liga->script_lang_index );
		else
		    fprintf( stderr, "Internal Error: '%c%c%c%c' in %s has a script index out of bounds: %d\n",
			    (liga->tag>>24), (liga->tag>>16)&0xff, (liga->tag>>8)&0xff, liga->tag&0xff,
			    sc->name, liga->script_lang_index );
		liga->script_lang_index = SFAddScriptLangIndex(sf,
			SCScriptFromUnicode(sc),DEFAULT_LANG);
		complained = true;
	    }
	    if ( liga->type==pst_position )
		fscanf( sfd, " dx=%hd dy=%hd dh=%hd dv=%hd\n",
			&liga->u.pos.xoff, &liga->u.pos.yoff,
			&liga->u.pos.h_adv_off, &liga->u.pos.v_adv_off);
	    else if ( liga->type==pst_pair ) {
		getname(sfd,tok);
		liga->u.pair.paired = copy(tok);
		liga->u.pair.vr = chunkalloc(sizeof(struct vr [2]));
		fscanf( sfd, " dx=%hd dy=%hd dh=%hd dv=%hd | dx=%hd dy=%hd dh=%hd dv=%hd\n",
			&liga->u.pair.vr[0].xoff, &liga->u.pair.vr[0].yoff,
			&liga->u.pair.vr[0].h_adv_off, &liga->u.pair.vr[0].v_adv_off,
			&liga->u.pair.vr[1].xoff, &liga->u.pair.vr[1].yoff,
			&liga->u.pair.vr[1].h_adv_off, &liga->u.pair.vr[1].v_adv_off);
	    } else if ( liga->type==pst_lcaret ) {
		int i;
		fscanf( sfd, " %d", &liga->u.lcaret.cnt );
		liga->u.lcaret.carets = galloc(liga->u.lcaret.cnt*sizeof(int16));
		for ( i=0; i<liga->u.lcaret.cnt; ++i )
		    fscanf( sfd, " %hd", &liga->u.lcaret.carets[i]);
		geteol(sfd,tok);
	    } else {
		geteol(sfd,tok);
		liga->u.lig.components = copy(tok);	/* it's in the same place for all formats */
		if ( isliga ) {
		    liga->u.lig.lig = sc;
		    last = LigaCreateFromOldStyleMultiple(liga);
		}
	    }
#ifdef PFAEDIT_CONFIG_CVT_OLD_MAC_FEATURES
	    CvtOldMacFeature(liga);
#endif
	} else if ( strmatch(tok,"Colour:")==0 ) {
	    int temp;
	    gethex(sfd,&temp);
	    sc->color = temp;
	} else if ( strmatch(tok,"Comment:")==0 ) {
	    sc->comment = SFDReadUTF7Str(sfd);
	} else if ( strmatch(tok,"EndChar")==0 ) {
	    if ( sc->enc<sf->charcnt )
		sf->chars[sc->enc] = sc;
#if 0		/* Auto recovery fails if we do this */
	    else {
		SplineCharFree(sc);
		sc = NULL;
	    }
#endif
return( sc );
	} else {
	    geteol(sfd,tok);
	}
    }
}

static int SFDGetBitmapChar(FILE *sfd,BDFFont *bdf) {
    BDFChar *bfc;
    struct enc85 dec;
    int i;

    bfc = chunkalloc(sizeof(BDFChar));
    
    if ( getint(sfd,&bfc->enc)!=1 || bfc->enc<0 )
return( 0 );
    if ( getsint(sfd,&bfc->width)!=1 )
return( 0 );
    if ( getsint(sfd,&bfc->xmin)!=1 )
return( 0 );
    if ( getsint(sfd,&bfc->xmax)!=1 || bfc->xmax<bfc->xmin )
return( 0 );
    if ( getsint(sfd,&bfc->ymin)!=1 )
return( 0 );
    if ( getsint(sfd,&bfc->ymax)!=1 || bfc->ymax<bfc->ymin )
return( 0 );

    bdf->chars[bfc->enc] = bfc;
    bfc->sc = bdf->sf->chars[bfc->enc];
    if ( bdf->clut==NULL ) {
	bfc->bytes_per_line = (bfc->xmax-bfc->xmin)/8 +1;
	bfc->depth = 1;
    } else {
	bfc->bytes_per_line = bfc->xmax-bfc->xmin +1;
	bfc->byte_data = true;
	bfc->depth = bdf->clut->clut_len==4 ? 2 : bdf->clut->clut_len==16 ? 4 : 8;
    }
    bfc->bitmap = gcalloc((bfc->ymax-bfc->ymin+1)*bfc->bytes_per_line,sizeof(char));

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;
    for ( i=0; i<=bfc->ymax-bfc->ymin; ++i ) {
	uint8 *pt = (uint8 *) (bfc->bitmap + i*bfc->bytes_per_line);
	uint8 *end = (uint8 *) (bfc->bitmap + (i+1)*bfc->bytes_per_line);
	while ( pt<end ) {
	    *pt++ = Dec85(&dec);
	}
    }
    if ( bfc->sc==NULL ) {
	bdf->chars[bfc->enc] = NULL;
	BDFCharFree(bfc);
    }
return( 1 );
}

static int SFDGetBitmapFont(FILE *sfd,SplineFont *sf) {
    BDFFont *bdf, *prev;
    char tok[200];
    int pixelsize, ascent, descent, depth=1;
    int ch;

    bdf = gcalloc(1,sizeof(BDFFont));
    bdf->encoding_name = sf->encoding_name;

    if ( getint(sfd,&pixelsize)!=1 || pixelsize<=0 )
return( 0 );
    if ( getint(sfd,&bdf->charcnt)!=1 || bdf->charcnt<0 )
return( 0 );
    if ( getint(sfd,&ascent)!=1 || ascent<0 )
return( 0 );
    if ( getint(sfd,&descent)!=1 || descent<0 )
return( 0 );
    if ( getint(sfd,&depth)!=1 )
	depth = 1;	/* old sfds don't have a depth here */
    else if ( depth!=1 && depth!=2 && depth!=4 && depth!=8 )
return( 0 );
    while ( (ch = getc(sfd))==' ' );
    ungetc(ch,sfd);		/* old sfds don't have a foundry */
    if ( ch!='\n' && ch!='\r' ) {
	getname(sfd,tok);
	bdf->foundry = copy(tok);
    }
    bdf->pixelsize = pixelsize;
    bdf->ascent = ascent;
    bdf->descent = descent;
    if ( depth!=1 )
	BDFClut(bdf,(1<<(depth/2)));

    if ( sf->bitmaps==NULL )
	sf->bitmaps = bdf;
    else {
	for ( prev=sf->bitmaps; prev->next!=NULL; prev=prev->next );
	prev->next = bdf;
    }
    bdf->sf = sf;
    bdf->chars = gcalloc(bdf->charcnt,sizeof(BDFChar *));

    while ( getname(sfd,tok)==1 ) {
	if ( strcmp(tok,"BDFChar:")==0 )
	    SFDGetBitmapChar(sfd,bdf);
	else if ( strcmp(tok,"EndBitmapFont")==0 )
    break;
    }
return( 1 );
}

static void SFDFixupRef(SplineChar *sc,RefChar *ref) {
    RefChar *rf;

    for ( rf = ref->sc->refs; rf!=NULL; rf=rf->next ) {
	if ( rf->sc==sc ) {	/* Huh? */
	    ref->sc->refs = NULL;
    break;
	}
	if ( rf->splines==NULL )
	    SFDFixupRef(ref->sc,rf);
    }
    SCReinstanciateRefChar(sc,ref);
    SCMakeDependent(sc,ref->sc);
}

static void SFDFixupRefs(SplineFont *sf) {
    int i, isv;
    RefChar *refs, *rnext, *rprev;
    /*int isautorecovery = sf->changed;*/
    KernPair *kp, *prev, *next;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	/* A changed character is one that has just been recovered */
	/*  unchanged characters will already have been fixed up */
	/* Er... maybe not. If the character being recovered is refered to */
	/*  by another character then we need to fix up that other char too*/
	/*if ( isautorecovery && !sf->chars[i]->changed )*/
    /*continue;*/
	rprev = NULL;
	for ( refs = sf->chars[i]->refs; refs!=NULL; refs=rnext ) {
	    rnext = refs->next;
	    if ( refs->local_enc<sf->charcnt )
		refs->sc = sf->chars[refs->local_enc];
	    if ( refs->sc==NULL )
		refs->sc = SFMakeChar(sf,refs->local_enc);
	    if ( refs->sc!=NULL ) {
		refs->unicode_enc = refs->sc->unicodeenc;
		refs->adobe_enc = getAdobeEnc(refs->sc->name);
		rprev = refs;
	    } else {
		RefCharFree(refs);
		if ( rprev!=NULL )
		    rprev->next = rnext;
		else
		    sf->chars[i]->refs = rnext;
	    }
	}
	/*if ( isautorecovery && !sf->chars[i]->changed )*/
    /*continue;*/
	for ( isv=0; isv<2; ++isv ) {
	    for ( prev = NULL, kp=isv?sf->chars[i]->vkerns : sf->chars[i]->kerns; kp!=NULL; kp=next ) {
		next = kp->next;
		if ( ((int) (kp->sc))>=sf->charcnt ) {
		    fprintf( stderr, "Warning: Bad kerning information in glyph %s\n", sf->chars[i]->name );
		    kp->sc = NULL;
		} else
		    kp->sc = sf->chars[(int) (kp->sc)];
		if ( kp->sc!=NULL )
		    prev = kp;
		else{
		    if ( prev!=NULL )
			prev->next = next;
		    else if ( isv )
			sf->chars[i]->vkerns = next;
		    else
			sf->chars[i]->kerns = next;
		    chunkfree(kp,sizeof(KernPair));
		}
	    }
	}
    }
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	for ( refs = sf->chars[i]->refs; refs!=NULL; refs=refs->next ) {
	    SFDFixupRef(sf->chars[i],refs);
	}
	GProgressNext();
    }
}

/* When we recover from an autosaved file we must be careful. If that file */
/*  contains a character that is refered to by another character then the */
/*  dependent list will contain a dead pointer without this routine. Similarly*/
/*  for kerning */
/* We might have needed to do something for references except they've already */
/*  got a local encoding field and passing through SFDFixupRefs will much their*/
/*  SplineChar pointer */
static void SFRemoveDependencies(SplineFont *sf) {
    int i;
    struct splinecharlist *dlist, *dnext;
    KernPair *kp;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	for ( dlist = sf->chars[i]->dependents; dlist!=NULL; dlist = dnext ) {
	    dnext = dlist->next;
	    chunkfree(dlist,sizeof(*dlist));
	}
	sf->chars[i]->dependents = NULL;
	for ( kp=sf->chars[i]->kerns; kp!=NULL; kp=kp->next )
	    kp->sc = (SplineChar *) (kp->sc->enc);
	for ( kp=sf->chars[i]->vkerns; kp!=NULL; kp=kp->next )
	    kp->sc = (SplineChar *) (kp->sc->enc);
    }
}

static void SFDGetPrivate(FILE *sfd,SplineFont *sf) {
    int i, cnt, len;
    char name[200];
    char *pt, *end;

    sf->private = gcalloc(1,sizeof(struct psdict));
    getint(sfd,&cnt);
    sf->private->next = sf->private->cnt = cnt;
    sf->private->values = gcalloc(cnt,sizeof(char *));
    sf->private->keys = gcalloc(cnt,sizeof(char *));
    for ( i=0; i<cnt; ++i ) {
	getname(sfd,name);
	sf->private->keys[i] = copy(name);
	getint(sfd,&len);
	getc(sfd);	/* skip space */
	pt = sf->private->values[i] = galloc(len+1);
	for ( end = pt+len; pt<end; ++pt )
	    *pt = getc(sfd);
	*pt='\0';
    }
}

static void SFDGetSubrs(FILE *sfd,SplineFont *sf) {
    /* Obselete, parse it in case there are any old sfds */
    int i, cnt, tot, len;
    struct enc85 dec;

    getint(sfd,&cnt);
    tot = 0;
    for ( i=0; i<cnt; ++i ) {
	getint(sfd,&len);
	tot += len;
    }

    memset(&dec,'\0', sizeof(dec)); dec.pos = -1;
    dec.sfd = sfd;
    for ( i=0; i<tot; ++i )
	Dec85(&dec);
}
    
static struct ttflangname *SFDGetLangName(FILE *sfd,struct ttflangname *old) {
    struct ttflangname *cur = gcalloc(1,sizeof(struct ttflangname)), *prev;
    int i;

    getint(sfd,&cur->lang);
    for ( i=0; i<ttf_namemax; ++i )
	cur->names[i] = SFDReadUTF7Str(sfd);
    if ( old==NULL )
return( cur );
    for ( prev = old; prev->next !=NULL; prev = prev->next );
    prev->next = cur;
return( old );
}

static enum charset SFDGetEncoding(FILE *sfd, char *tok, SplineFont *sf) {
    int encname = em_none;
    int i;

    if ( !getint(sfd,&encname) ) {
	Encoding *item; int val;
	geteol(sfd,tok);
	encname = em_none;
	for ( i=0; charset_names[i]!=NULL; ++i )
	    if ( strcmp(tok,charset_names[i])==0 ) {
		encname = i-1;
	break;
	    }
	if ( charset_names[i]==NULL ) {
	    for ( item = enclist; item!=NULL && strcmp(item->enc_name,tok)!=0; item = item->next );
	    if ( item!=NULL )
		encname = item->enc_num;
	}
	if ( encname == em_none &&
		sscanf(tok,"UnicodePlane%d",&val)==1 )
	    encname = val+em_unicodeplanes;
	if ( encname == em_none && strcmp(tok,"compacted")==0 ) {
	    encname = em_none;
	    sf->compacted = true;
	}
    }
return( encname );
}

static enum uni_interp SFDGetUniInterp(FILE *sfd, char *tok, SplineFont *sf) {
    int uniinterp = ui_none;
    int i;

    geteol(sfd,tok);
    uniinterp = em_none;
    for ( i=0; unicode_interp_names[i]!=NULL; ++i )
	if ( strcmp(tok,unicode_interp_names[i])==0 ) {
	    uniinterp = i;
    break;
	}

return( uniinterp );
}

static int SFAddScriptIndex(SplineFont *sf,uint32 *scripts,int scnt) {
    int i,j;
    struct script_record *sr;

    if ( scnt==0 )
	scripts[scnt++] = CHR('l','a','t','n');		/* Need a default script preference */
    for ( i=0; i<scnt-1; ++i ) for ( j=i+1; j<scnt; ++j ) {
	if ( scripts[i]>scripts[j] ) {
	    uint32 temp = scripts[i];
	    scripts[i] = scripts[j];
	    scripts[j] = temp;
	}
    }

    if ( sf->cidmaster ) sf = sf->cidmaster;
    if ( sf->script_lang==NULL )	/* It's an old sfd file */
	sf->script_lang = gcalloc(1,sizeof(struct script_record *));
    for ( i=0; sf->script_lang[i]!=NULL; ++i ) {
	sr = sf->script_lang[i];
	for ( j=0; sr[j].script!=0 && j<scnt &&
		sr[j].script==scripts[j]; ++j );
	if ( sr[j].script==0 && j==scnt )
return( i );
    }

    sf->script_lang = grealloc(sf->script_lang,(i+2)*sizeof(struct script_record *));
    sf->script_lang[i+1] = NULL;
    sr = sf->script_lang[i] = gcalloc(scnt+1,sizeof(struct script_record));
    for ( j = 0; j<scnt; ++j ) {
	sr[j].script = scripts[j];
	sr[j].langs = galloc(2*sizeof(uint32));
	sr[j].langs[0] = DEFAULT_LANG;
	sr[j].langs[1] = 0;
    }
return( i );
}

static void SFDParseChainContext(FILE *sfd,SplineFont *sf,FPST *fpst, char *tok) {
    int ch, i, j, k, temp;

    fpst->type = strmatch(tok,"ContextPos:")==0 ? pst_contextpos :
		strmatch(tok,"ContextSub:")==0 ? pst_contextsub :
		strmatch(tok,"ChainPos:")==0 ? pst_chainpos :
		strmatch(tok,"ChainSub:")==0 ? pst_chainsub : pst_reversesub;
    getname(sfd,tok);
    fpst->format = strmatch(tok,"glyph")==0 ? pst_glyphs :
		    strmatch(tok,"class")==0 ? pst_class :
		    strmatch(tok,"coverage")==0 ? pst_coverage : pst_reversecoverage;
    fscanf(sfd, "%hu %hu", &fpst->flags, &fpst->script_lang_index );
    if ( fpst->script_lang_index>=sf->sli_cnt ) {
	static int complained=false;
	if ( sf->sli_cnt==0 )
	    GDrawError("'%c%c%c%c' has a script index out of bounds: %d\nYou MUST fix this manually",
		    (fpst->tag>>24), (fpst->tag>>16)&0xff, (fpst->tag>>8)&0xff, fpst->tag&0xff,
		    fpst->script_lang_index );
	else if ( !complained )
	    GDrawError("'%c%c%c%c' has a script index out of bounds: %d",
		    (fpst->tag>>24), (fpst->tag>>16)&0xff, (fpst->tag>>8)&0xff, fpst->tag&0xff,
		    fpst->script_lang_index );
	else
	    fprintf( stderr, "Internal Error: '%c%c%c%c' has a script index out of bounds: %d\n",
		    (fpst->tag>>24), (fpst->tag>>16)&0xff, (fpst->tag>>8)&0xff, fpst->tag&0xff,
		    fpst->script_lang_index );
	if ( sf->sli_cnt!=0 )
	    fpst->script_lang_index = sf->sli_cnt-1;
	complained = true;
    }
    while ( (ch=getc(sfd))==' ' || ch=='\t' );
    if ( ch=='\'' ) {
	fpst->tag = getc(sfd)<<24;
	fpst->tag |= getc(sfd)<<16;
	fpst->tag |= getc(sfd)<<8;
	fpst->tag |= getc(sfd);
	getc(sfd);	/* Final quote */
    } else
	ungetc(ch,sfd);
    fscanf(sfd, "%hu %hu %hu %hu", &fpst->nccnt, &fpst->bccnt, &fpst->fccnt, &fpst->rule_cnt );
    if ( fpst->nccnt!=0 || fpst->bccnt!=0 || fpst->fccnt!=0 ) {
	fpst->nclass = galloc(fpst->nccnt*sizeof(char *));
	if ( fpst->nccnt!=0 ) fpst->nclass[0] = NULL;
	if (fpst->nccnt!=0 ) fpst->nclass[0] = NULL;
	if ( fpst->bccnt!=0 || fpst->fccnt!=0 ) {
	    fpst->bclass = galloc(fpst->bccnt*sizeof(char *));
	    if (fpst->bccnt!=0 ) fpst->bclass[0] = NULL;
	    fpst->fclass = galloc(fpst->fccnt*sizeof(char *));
	    if (fpst->fccnt!=0 ) fpst->fclass[0] = NULL;
	}
    }

    for ( j=0; j<3; ++j ) {
	for ( i=1; i<(&fpst->nccnt)[j]; ++i ) {
	    getname(sfd,tok);
	    getint(sfd,&temp);
	    (&fpst->nclass)[j][i] = galloc(temp+1); (&fpst->nclass)[j][i][temp] = '\0';
	    getc(sfd);	/* skip space */
	    fread((&fpst->nclass)[j][i],1,temp,sfd);
	}
    }

    fpst->rules = gcalloc(fpst->rule_cnt,sizeof(struct fpst_rule));
    for ( i=0; i<fpst->rule_cnt; ++i ) {
	switch ( fpst->format ) {
	  case pst_glyphs:
	    for ( j=0; j<3; ++j ) {
		getname(sfd,tok);
		getint(sfd,&temp);
		(&fpst->rules[i].u.glyph.names)[j] = galloc(temp+1);
		(&fpst->rules[i].u.glyph.names)[j][temp] = '\0';
		getc(sfd);	/* skip space */
		fread((&fpst->rules[i].u.glyph.names)[j],1,temp,sfd);
	    }
	  break;
	  case pst_class:
	    fscanf( sfd, "%d %d %d", &fpst->rules[i].u.class.ncnt, &fpst->rules[i].u.class.bcnt, &fpst->rules[i].u.class.fcnt );
	    for ( j=0; j<3; ++j ) {
		getname(sfd,tok);
		(&fpst->rules[i].u.class.nclasses)[j] = galloc((&fpst->rules[i].u.class.ncnt)[j]*sizeof(uint16));
		for ( k=0; k<(&fpst->rules[i].u.class.ncnt)[j]; ++k ) {
		    getusint(sfd,&(&fpst->rules[i].u.class.nclasses)[j][k]);
		}
	    }
	  break;
	  case pst_coverage:
	  case pst_reversecoverage:
	    fscanf( sfd, "%d %d %d", &fpst->rules[i].u.coverage.ncnt, &fpst->rules[i].u.coverage.bcnt, &fpst->rules[i].u.coverage.fcnt );
	    for ( j=0; j<3; ++j ) {
		(&fpst->rules[i].u.coverage.ncovers)[j] = galloc((&fpst->rules[i].u.coverage.ncnt)[j]*sizeof(char *));
		for ( k=0; k<(&fpst->rules[i].u.coverage.ncnt)[j]; ++k ) {
		    getname(sfd,tok);
		    getint(sfd,&temp);
		    (&fpst->rules[i].u.coverage.ncovers)[j][k] = galloc(temp+1);
		    (&fpst->rules[i].u.coverage.ncovers)[j][k][temp] = '\0';
		    getc(sfd);	/* skip space */
		    fread((&fpst->rules[i].u.coverage.ncovers)[j][k],1,temp,sfd);
		}
	    }
	  break;
	}
	switch ( fpst->format ) {
	  case pst_glyphs:
	  case pst_class:
	  case pst_coverage:
	    getint(sfd,&fpst->rules[i].lookup_cnt);
	    fpst->rules[i].lookups = galloc(fpst->rules[i].lookup_cnt*sizeof(struct seqlookup));
	    for ( j=0; j<fpst->rules[i].lookup_cnt; ++j ) {
		getname(sfd,tok);
		getint(sfd,&fpst->rules[i].lookups[j].seq);
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
		if ( ch=='\'' ) {
		    fpst->rules[i].lookups[j].lookup_tag = getc(sfd)<<24;
		    fpst->rules[i].lookups[j].lookup_tag |= getc(sfd)<<16;
		    fpst->rules[i].lookups[j].lookup_tag |= getc(sfd)<<8;
		    fpst->rules[i].lookups[j].lookup_tag |= getc(sfd);
		    getc(sfd);	/* Final quote */
		} else
		    ungetc(ch,sfd);
	    }
	  break;
	  case pst_reversecoverage:
	    getname(sfd,tok);
	    getint(sfd,&temp);
	    fpst->rules[i].u.rcoverage.replacements = galloc(temp+1);
	    fpst->rules[i].u.rcoverage.replacements[temp] = '\0';
	    getc(sfd);	/* skip space */
	    fread(fpst->rules[i].u.rcoverage.replacements,1,temp,sfd);
	  break;
	}
    }
    getname(sfd,tok);
}

static uint32 ASM_ParseSubTag(FILE *sfd) {
    uint32 tag;
    int ch;

    while ( (ch=getc(sfd))==' ' );
    if ( ch!='\'' )
return( 0 );

    tag = getc(sfd)<<24;
    tag |= getc(sfd)<<16;
    tag |= getc(sfd)<<8;
    tag |= getc(sfd);
    getc(sfd);		/* final quote */
return( tag );
}

static void SFDParseStateMachine(FILE *sfd,SplineFont *sf,ASM *sm, char *tok) {
    int i, temp;

    sm->type = strmatch(tok,"MacIndic:")==0 ? asm_indic :
		strmatch(tok,"MacContext:")==0 ? asm_context :
		strmatch(tok,"MacLigature:")==0 ? asm_lig :
		strmatch(tok,"MacSimple:")==0 ? asm_simple :
		strmatch(tok,"MacKern:")==0 ? asm_kern : asm_insert;
    getusint(sfd,&sm->feature);
    getc(sfd);		/* Skip comma */
    getusint(sfd,&sm->setting);
    getusint(sfd,&sm->flags);
    getusint(sfd,&sm->class_cnt);
    getusint(sfd,&sm->state_cnt);

    sm->classes = galloc(sm->class_cnt*sizeof(char *));
    sm->classes[0] = sm->classes[1] = sm->classes[2] = sm->classes[3] = NULL;
    for ( i=4; i<sm->class_cnt; ++i ) {
	getname(sfd,tok);
	getint(sfd,&temp);
	sm->classes[i] = galloc(temp+1); sm->classes[i][temp] = '\0';
	getc(sfd);	/* skip space */
	fread(sm->classes[i],1,temp,sfd);
    }

    sm->state = galloc(sm->class_cnt*sm->state_cnt*sizeof(struct asm_state));
    for ( i=0; i<sm->class_cnt*sm->state_cnt; ++i ) {
	getusint(sfd,&sm->state[i].next_state);
	getusint(sfd,&sm->state[i].flags);
	if ( sm->type == asm_context ) {
	    sm->state[i].u.context.mark_tag = ASM_ParseSubTag(sfd);
	    sm->state[i].u.context.cur_tag = ASM_ParseSubTag(sfd);
	} else if ( sm->type == asm_insert ) {
	    getint(sfd,&temp);
	    if ( temp==0 )
		sm->state[i].u.insert.mark_ins = NULL;
	    else {
		sm->state[i].u.insert.mark_ins = galloc(temp+1); sm->state[i].u.insert.mark_ins[temp] = '\0';
		getc(sfd);	/* skip space */
		fread(sm->state[i].u.insert.mark_ins,1,temp,sfd);
	    }
	    getint(sfd,&temp);
	    if ( temp==0 )
		sm->state[i].u.insert.cur_ins = NULL;
	    else {
		sm->state[i].u.insert.cur_ins = galloc(temp+1); sm->state[i].u.insert.cur_ins[temp] = '\0';
		getc(sfd);	/* skip space */
		fread(sm->state[i].u.insert.cur_ins,1,temp,sfd);
	    }
	} else if ( sm->type == asm_kern ) {
	    int j;
	    getint(sfd,&sm->state[i].u.kern.kcnt);
	    if ( sm->state[i].u.kern.kcnt!=0 )
		sm->state[i].u.kern.kerns = galloc(sm->state[i].u.kern.kcnt*sizeof(int16));
	    for ( j=0; j<sm->state[i].u.kern.kcnt; ++j ) {
		getint(sfd,&temp);
		sm->state[i].u.kern.kerns[j] = temp;
	    }
	}
    }
    getname(sfd,tok);			/* EndASM */
}

static struct macname *SFDParseMacNames(FILE *sfd, char *tok) {
    struct macname *head=NULL, *last=NULL, *cur;
    int enc, lang, len;
    char *pt;
    int ch;

    while ( strcmp(tok,"MacName:")==0 ) {
	cur = chunkalloc(sizeof(struct macname));
	if ( last==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;

	getint(sfd,&enc);
	getint(sfd,&lang);
	getint(sfd,&len);
	cur->enc = enc;
	cur->lang = lang;
	cur->name = pt = galloc(len+1);
	
	while ( (ch=getc(sfd))==' ');
	if ( ch=='"' )
	    ch = getc(sfd);
	while ( ch!='"' && ch!=EOF && pt<cur->name+len ) {
	    if ( ch=='\\' ) {
		*pt  = (getc(sfd)-'0')<<6;
		*pt |= (getc(sfd)-'0')<<3;
		*pt |= (getc(sfd)-'0');
	    } else
		*pt++ = ch;
	    ch = getc(sfd);
	}
	*pt = '\0';
	getname(sfd,tok);
    }
return( head );
}

MacFeat *SFDParseMacFeatures(FILE *sfd, char *tok) {
    MacFeat *cur, *head=NULL, *last=NULL;
    struct macsetting *slast, *scur;
    int feat, ism, def, set;

    while ( strcmp(tok,"MacFeat:")==0 ) {
	cur = chunkalloc(sizeof(MacFeat));
	if ( last==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;

	getint(sfd,&feat); getint(sfd,&ism); getint(sfd, &def);
	cur->feature = feat; cur->ismutex = ism; cur->default_setting = def;
	getname(sfd,tok);
	cur->featname = SFDParseMacNames(sfd,tok);
	slast = NULL;
	while ( strcmp(tok,"MacSetting:")==0 ) {
	    scur = chunkalloc(sizeof(struct macsetting));
	    if ( slast==NULL )
		cur->settings = scur;
	    else
		slast->next = scur;
	    slast = scur;

	    getint(sfd,&set);
	    scur->setting = set;
	    getname(sfd,tok);
	    scur->setname = SFDParseMacNames(sfd,tok);
	}
    }
return( head );
}

static char *SFDParseMMSubroutine(FILE *sfd) {
    char buffer[400], *sofar=gcalloc(1,1);
    const char *endtok = "EndMMSubroutine";
    int len = 0, blen, first=true;

    while ( fgets(buffer,sizeof(buffer),sfd)!=NULL ) {
	if ( strncmp(buffer,endtok,strlen(endtok))==0 )
    break;
	if ( first ) {
	    first = false;
	    if ( strcmp(buffer,"\n")==0 )
    continue;
	}
	blen = strlen(buffer);
	sofar = grealloc(sofar,len+blen+1);
	strcpy(sofar+len,buffer);
	len += blen;
    }
    if ( len>0 && sofar[len-1]=='\n' )
	sofar[len-1] = '\0';
return( sofar );
}

static void SFDCleanupAnchorClasses(SplineFont *sf) {
    AnchorClass *ac;
    AnchorPoint *ap;
    int i, j, scnt;
#define S_MAX	100
    uint32 scripts[S_MAX];
    int merge=0;

    for ( ac = sf->anchor; ac!=NULL; ac=ac->next ) {
	if ( ac->script_lang_index==0xffff ) {
	    scnt = 0;
	    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
		for ( ap = sf->chars[i]->anchor; ap!=NULL && ap->anchor!=ac; ap=ap->next );
		if ( ap!=NULL && scnt<S_MAX ) {
		    uint32 script = SCScriptFromUnicode(sf->chars[i]);
		    if ( script==0 )
	    continue;
		    for ( j=0; j<scnt; ++j )
			if ( scripts[j]==script )
		    break;
		    if ( j==scnt )
			scripts[scnt++] = script;
		}
	    }
	    ac->script_lang_index = SFAddScriptIndex(sf,scripts,scnt);
	}
	if ( ac->merge_with == 0xffff )
	    ac->merge_with = ++merge;
    }
#undef S_MAX
}

enum uni_interp interp_from_encoding(enum charset enc,enum uni_interp interp) {

    switch ( enc ) {
      case em_sjis: case em_jis208: case em_jis212: case em_jis201:
	interp = ui_japanese;
      break;
      case em_big5: case em_big5hkscs:
	interp = ui_trad_chinese;
      break;
      case em_johab: case em_wansung: case em_ksc5601:
	interp = ui_korean;
      break;
      case em_jisgb: case em_gb2312: case em_gb18030:
	interp = ui_simp_chinese;
      break;
      case em_iso8859_7:
	interp = ui_greek;
      break;
    }
return( interp );
}

static void SFDCleanupFont(SplineFont *sf) {
    SFDCleanupAnchorClasses(sf);
    if ( sf->uni_interp==ui_unset )
	sf->uni_interp = interp_from_encoding(sf->encoding_name,ui_none);
}

static SplineFont *SFD_GetFont(FILE *sfd,SplineFont *cidmaster,char *tok) {
    SplineFont *sf;
    SplineChar *sc;
    int realcnt, i, eof, mappos=-1, ch, ch2, glyph;
    struct table_ordering *lastord = NULL;
    struct ttf_table *lastttf = NULL;
    KernClass *lastkc=NULL, *kc, *lastvkc=NULL;
    FPST *lastfp=NULL;
    ASM *lastsm=NULL;

    sf = SplineFontEmpty();
    sf->cidmaster = cidmaster;
    sf->uni_interp = ui_unset;
    while ( 1 ) {
	if ( (eof = getname(sfd,tok))!=1 ) {
	    if ( eof==-1 )
    break;
	    geteol(sfd,tok);
    continue;
	}
	if ( strmatch(tok,"FontName:")==0 ) {
	    getname(sfd,tok);
	    sf->fontname = copy(tok);
	} else if ( strmatch(tok,"FullName:")==0 ) {
	    geteol(sfd,tok);
	    sf->fullname = copy(tok);
	} else if ( strmatch(tok,"FamilyName:")==0 ) {
	    geteol(sfd,tok);
	    sf->familyname = copy(tok);
	} else if ( strmatch(tok,"Weight:")==0 ) {
	    getprotectedname(sfd,tok);
	    sf->weight = copy(tok);
	} else if ( strmatch(tok,"Copyright:")==0 ) {
	    sf->copyright = getquotedeol(sfd);
	} else if ( strmatch(tok,"Comments:")==0 ) {
	    sf->comments = getquotedeol(sfd);
	} else if ( strmatch(tok,"Version:")==0 ) {
	    geteol(sfd,tok);
	    sf->version = copy(tok);
	} else if ( strmatch(tok,"ItalicAngle:")==0 ) {
	    getreal(sfd,&sf->italicangle);
	} else if ( strmatch(tok,"UnderlinePosition:")==0 ) {
	    getreal(sfd,&sf->upos);
	} else if ( strmatch(tok,"UnderlineWidth:")==0 ) {
	    getreal(sfd,&sf->uwidth);
	} else if ( strmatch(tok,"PfmFamily:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->pfminfo.pfmfamily = temp;
	    sf->pfminfo.pfmset = true;
	} else if ( strmatch(tok,"LangName:")==0 ) {
	    sf->names = SFDGetLangName(sfd,sf->names);
	} else if ( strmatch(tok,"PfmWeight:")==0 || strmatch(tok,"TTFWeight:")==0 ) {
	    getsint(sfd,&sf->pfminfo.weight);
	    sf->pfminfo.pfmset = true;
	} else if ( strmatch(tok,"TTFWidth:")==0 ) {
	    getsint(sfd,&sf->pfminfo.width);
	    sf->pfminfo.pfmset = true;
	} else if ( strmatch(tok,"Panose:")==0 ) {
	    int temp,i;
	    for ( i=0; i<10; ++i ) {
		getint(sfd,&temp);
		sf->pfminfo.panose[i] = temp;
	    }
	    sf->pfminfo.pfmset = true;
	} else if ( strmatch(tok,"LineGap:")==0 ) {
	    getsint(sfd,&sf->pfminfo.linegap);
	    sf->pfminfo.pfmset = true;
	} else if ( strmatch(tok,"VLineGap:")==0 ) {
	    getsint(sfd,&sf->pfminfo.vlinegap);
	    sf->pfminfo.pfmset = true;
#if 0
	} else if ( strmatch(tok,"HheadAscent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.hhead_ascent);
	} else if ( strmatch(tok,"HheadDescent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.hhead_descent);
#endif
	} else if ( strmatch(tok,"OS2TypoAscent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.os2_typoascent);
	} else if ( strmatch(tok,"OS2TypoDescent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.os2_typodescent);
	} else if ( strmatch(tok,"OS2WinAscent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.os2_winascent);
	} else if ( strmatch(tok,"OS2WinDescent:")==0 ) {
	    getsint(sfd,&sf->pfminfo.os2_windescent);
	} else if ( strmatch(tok,"MacStyle:")==0 ) {
	    getsint(sfd,&sf->macstyle);
	} else if ( strmatch(tok,"DisplaySize:")==0 ) {
	    getint(sfd,&sf->display_size);
	} else if ( strmatch(tok,"TopEncoding:")==0 ) {	/* Obsolete */
	    getint(sfd,&sf->top_enc);
	} else if ( strmatch(tok,"WinInfo:")==0 ) {
	    int temp1, temp2;
	    getint(sfd,&sf->top_enc);
	    getint(sfd,&temp1);
	    getint(sfd,&temp2);
	    if ( sf->top_enc<=0 ) sf->top_enc=-1;
	    if ( temp1<=0 ) temp1 = 16;
	    if ( temp2<=0 ) temp2 = 4;
	    sf->desired_col_cnt = temp1;
	    sf->desired_row_cnt = temp2;
	} else if ( strmatch(tok,"AntiAlias:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->display_antialias = temp;
	} else if ( strmatch(tok,"FitToEm:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->display_bbsized = temp;
	} else if ( strmatch(tok,"OnlyBitmaps:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->onlybitmaps = temp;
	} else if ( strmatch(tok,"Ascent:")==0 ) {
	    getint(sfd,&sf->ascent);
	} else if ( strmatch(tok,"Descent:")==0 ) {
	    getint(sfd,&sf->descent);
	} else if ( strmatch(tok,"Order2:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->order2 = temp;
	} else if ( strmatch(tok,"NeedsXUIDChange:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->changed_since_xuidchanged = temp;
	} else if ( strmatch(tok,"VerticalOrigin:")==0 ) {
	    getint(sfd,&sf->vertical_origin);
	    sf->hasvmetrics = true;
	} else if ( strmatch(tok,"FSType:")==0 ) {
	    getsint(sfd,&sf->pfminfo.fstype);
	} else if ( strmatch(tok,"UniqueID:")==0 ) {
	    getint(sfd,&sf->uniqueid);
	} else if ( strmatch(tok,"XUID:")==0 ) {
	    geteol(sfd,tok);
	    sf->xuid = copy(tok);
	} else if ( strmatch(tok,"Encoding:")==0 ) {
	    sf->encoding_name = SFDGetEncoding(sfd,tok,sf);
	} else if ( strmatch(tok,"OldEncoding:")==0 ) {
	    sf->old_encname = SFDGetEncoding(sfd,tok,sf);
	} else if ( strmatch(tok,"UnicodeInterp:")==0 ) {
	    sf->uni_interp = SFDGetUniInterp(sfd,tok,sf);
	} else if ( strmatch(tok,"Registry:")==0 ) {
	    geteol(sfd,tok);
	    sf->cidregistry = copy(tok);
	} else if ( strmatch(tok,"Ordering:")==0 ) {
	    geteol(sfd,tok);
	    sf->ordering = copy(tok);
	} else if ( strmatch(tok,"Supplement:")==0 ) {
	    getint(sfd,&sf->supplement);
	} else if ( strmatch(tok,"RemapN:")==0 ) {
	    int n;
	    getint(sfd,&n);
	    sf->remap = gcalloc(n+1,sizeof(struct remap));
	    sf->remap[n].infont = -1;
	    mappos = 0;
	} else if ( strmatch(tok,"Remap:")==0 ) {
	    int f, l, p;
	    gethex(sfd,&f);
	    gethex(sfd,&l);
	    getint(sfd,&p);
	    if ( sf->remap!=NULL && sf->remap[mappos].infont!=-1 ) {
		sf->remap[mappos].firstenc = f;
		sf->remap[mappos].lastenc = l;
		sf->remap[mappos].infont = p;
		mappos++;
	    }
	} else if ( strmatch(tok,"CIDVersion:")==0 ) {
	    real temp;
	    getreal(sfd,&temp);
	    sf->cidversion = temp;
	} else if ( strmatch(tok,"Grid")==0 ) {
	    sf->gridsplines = SFDGetSplineSet(sf,sfd);
	} else if ( strmatch(tok,"ScriptLang:")==0 ) {
	    int i,j,k;
	    int imax, jmax, kmax;
	    getint(sfd,&imax);
	    sf->sli_cnt = imax;
	    sf->script_lang = galloc((imax+1)*sizeof(struct script_record *));
	    sf->script_lang[imax] = NULL;
	    for ( i=0; i<imax; ++i ) {
		getint(sfd,&jmax);
		sf->script_lang[i] = galloc((jmax+1)*sizeof(struct script_record));
		sf->script_lang[i][jmax].script = 0;
		for ( j=0; j<jmax; ++j ) {
		    while ( (ch=getc(sfd))==' ' || ch=='\t' );
		    sf->script_lang[i][j].script = ch<<24;
		    sf->script_lang[i][j].script |= getc(sfd)<<16;
		    sf->script_lang[i][j].script |= getc(sfd)<<8;
		    sf->script_lang[i][j].script |= getc(sfd);
		    getint(sfd,&kmax);
		    sf->script_lang[i][j].langs = galloc((kmax+1)*sizeof(uint32));
		    sf->script_lang[i][j].langs[kmax] = 0;
		    for ( k=0; k<kmax; ++k ) {
			while ( (ch=getc(sfd))==' ' || ch=='\t' );
			sf->script_lang[i][j].langs[k] = ch<<24;
			sf->script_lang[i][j].langs[k] |= getc(sfd)<<16;
			sf->script_lang[i][j].langs[k] |= getc(sfd)<<8;
			sf->script_lang[i][j].langs[k] |= getc(sfd);
		    }
		}
	    }
	} else if ( strmatch(tok,"KernClass:")==0 || strmatch(tok,"VKernClass:")==0 ) {
	    int temp;
	    int isv = tok[0]=='V';
	    kc = chunkalloc(sizeof(KernClass));
	    if ( !isv ) {
		if ( lastkc==NULL )
		    sf->kerns = kc;
		else
		    lastkc->next = kc;
		lastkc = kc;
	    } else {
		if ( lastvkc==NULL )
		    sf->vkerns = kc;
		else
		    lastvkc->next = kc;
		lastvkc = kc;
	    }
	    getint(sfd,&kc->first_cnt);
	    getint(sfd,&kc->second_cnt);
	    getint(sfd,&temp); kc->sli = temp;
	    getint(sfd,&temp); kc->flags = temp;
	    kc->firsts = galloc(kc->first_cnt*sizeof(char *));
	    kc->seconds = galloc(kc->first_cnt*sizeof(char *));
	    kc->offsets = galloc(kc->first_cnt*kc->second_cnt*sizeof(int16));
	    kc->firsts[0] = NULL;
	    for ( i=1; i<kc->first_cnt; ++i ) {
		getint(sfd,&temp);
		kc->firsts[i] = galloc(temp+1); kc->firsts[i][temp] = '\0';
		getc(sfd);	/* skip space */
		fread(kc->firsts[i],1,temp,sfd);
	    }
	    kc->seconds[0] = NULL;
	    for ( i=1; i<kc->second_cnt; ++i ) {
		getint(sfd,&temp);
		kc->seconds[i] = galloc(temp+1); kc->seconds[i][temp] = '\0';
		getc(sfd);	/* skip space */
		fread(kc->seconds[i],1,temp,sfd);
	    }
	    for ( i=0; i<kc->first_cnt*kc->second_cnt; ++i ) {
		getint(sfd,&temp);
		kc->offsets[i] = temp;
	    }
	} else if ( strmatch(tok,"ContextPos:")==0 || strmatch(tok,"ContextSub:")==0 ||
		strmatch(tok,"ChainPos:")==0 || strmatch(tok,"ChainSub:")==0 ||
		strmatch(tok,"ReverseChain:")==0 ) {
	    FPST *fpst = chunkalloc(sizeof(FPST));
	    if ( lastfp==NULL )
		sf->possub = fpst;
	    else
		lastfp->next = fpst;
	    lastfp = fpst;
	    SFDParseChainContext(sfd,sf,fpst,tok);
	} else if ( strmatch(tok,"MacIndic:")==0 || strmatch(tok,"MacContext:")==0 ||
		strmatch(tok,"MacLigature:")==0 || strmatch(tok,"MacSimple:")==0 ||
		strmatch(tok,"MacKern:")==0 || strmatch(tok,"MacInsert:")==0 ) {
	    ASM *sm = chunkalloc(sizeof(ASM));
	    if ( lastsm==NULL )
		sf->sm = sm;
	    else
		lastsm->next = sm;
	    lastsm = sm;
	    SFDParseStateMachine(sfd,sf,sm,tok);
	} else if ( strmatch(tok,"MacFeat:")==0 ) {
	    sf->features = SFDParseMacFeatures(sfd,tok);
	} else if ( strmatch(tok,"TeXData:")==0 ) {
	    int temp;
	    getint(sfd,&temp);
	    sf->texdata.type = temp;
	    getint(sfd,&sf->texdata.designsize);
	    for ( i=0; i<22; ++i )
		getint(sfd,&sf->texdata.params[i]);
	} else if ( strmatch(tok,"AnchorClass:")==0 ) {
	    unichar_t *name;
	    AnchorClass *lastan = NULL, *an;
	    while ( (name=SFDReadUTF7Str(sfd))!=NULL ) {
		an = chunkalloc(sizeof(AnchorClass));
		an->name = name;
		getname(sfd,tok);
		if ( tok[0]=='0' && tok[1]=='\0' )
		    an->feature_tag = 0;
		else {
		    if ( tok[1]=='\0' ) { tok[1]=' '; tok[2] = 0; }
		    if ( tok[2]=='\0' ) { tok[2]=' '; tok[3] = 0; }
		    if ( tok[3]=='\0' ) { tok[3]=' '; tok[4] = 0; }
		    an->feature_tag = (tok[0]<<24) | (tok[1]<<16) | (tok[2]<<8) | tok[3];
		}
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
		ungetc(ch,sfd);
		if ( isdigit(ch)) {
		    int temp;
		    getint(sfd,&temp);
		    an->flags = temp;
		}
#if 0
		else if ( an->feature_tag==CHR('c','u','r','s'))
		    an->flags = pst_ignorecombiningmarks;
#endif
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
		ungetc(ch,sfd);
		if ( isdigit(ch)) {
		    int temp;
		    getint(sfd,&temp);
		    an->script_lang_index = temp;
		} else
		    an->script_lang_index = 0xffff;		/* Will be fixed up later */
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
		ungetc(ch,sfd);
		if ( isdigit(ch)) {
		    int temp;
		    getint(sfd,&temp);
		    an->merge_with = temp;
		} else
		    an->merge_with = 0xffff;			/* Will be fixed up later */
		while ( (ch=getc(sfd))==' ' || ch=='\t' );
		ungetc(ch,sfd);
		if ( isdigit(ch)) {
		    int temp;
		    getint(sfd,&temp);
		    an->type = temp;
		} else {
		    if ( an->feature_tag==CHR('c','u','r','s'))
			an->type = act_curs;
		    else if ( an->feature_tag==CHR('m','k','m','k'))
			an->type = act_mkmk;
		    else
			an->type = act_mark;
		}
		if ( lastan==NULL )
		    sf->anchor = an;
		else
		    lastan->next = an;
		lastan = an;
	    }
	} else if ( strmatch(tok,"GenTags:")==0 ) {
	    int temp; uint32 tag;
	    getint(sfd,&temp);
	    sf->gentags.tt_cur = temp;
	    sf->gentags.tt_max = sf->gentags.tt_cur+10;
	    sf->gentags.tagtype = galloc(sf->gentags.tt_max*sizeof(struct tagtype));
	    ch = getc(sfd);
	    i = 0;
	    while ( ch!='\n' ) {
		while ( ch==' ' ) ch = getc(sfd);
		if ( ch=='\n' || ch==EOF )
	    break;
		ch2 = getc(sfd);
		if ( ch=='p' && ch2=='s' ) sf->gentags.tagtype[i].type = pst_position;
		else if ( ch=='p' && ch2=='r' ) sf->gentags.tagtype[i].type = pst_pair;
		else if ( ch=='s' && ch2=='b' ) sf->gentags.tagtype[i].type = pst_substitution;
		else if ( ch=='a' && ch2=='s' ) sf->gentags.tagtype[i].type = pst_alternate;
		else if ( ch=='m' && ch2=='s' ) sf->gentags.tagtype[i].type = pst_multiple;
		else if ( ch=='l' && ch2=='g' ) sf->gentags.tagtype[i].type = pst_ligature;
		else if ( ch=='a' && ch2=='n' ) sf->gentags.tagtype[i].type = pst_anchors;
		else if ( ch=='c' && ch2=='p' ) sf->gentags.tagtype[i].type = pst_contextpos;
		else if ( ch=='c' && ch2=='s' ) sf->gentags.tagtype[i].type = pst_contextsub;
		else if ( ch=='p' && ch2=='c' ) sf->gentags.tagtype[i].type = pst_chainpos;
		else if ( ch=='s' && ch2=='c' ) sf->gentags.tagtype[i].type = pst_chainsub;
		else if ( ch=='r' && ch2=='s' ) sf->gentags.tagtype[i].type = pst_reversesub;
		else if ( ch=='n' && ch2=='l' ) sf->gentags.tagtype[i].type = pst_null;
		else ch2 = EOF;
		(void) getc(sfd);
		tag = getc(sfd)<<24;
		tag |= getc(sfd)<<16;
		tag |= getc(sfd)<<8;
		tag |= getc(sfd);
		(void) getc(sfd);
		if ( ch2!=EOF )
		    sf->gentags.tagtype[i++].tag = tag;
		ch = getc(sfd);
	    }
	} else if ( strmatch(tok,"TtfTable:")==0 ) {
	    lastttf = SFDGetTtfTable(sfd,sf,lastttf);
	} else if ( strmatch(tok,"TableOrder:")==0 ) {
	    int temp;
	    struct table_ordering *ord;
	    while ((ch=getc(sfd))==' ' );
	    ord = chunkalloc(sizeof(struct table_ordering));
	    ord->table_tag = (ch<<24) | (getc(sfd)<<16);
	    ord->table_tag |= getc(sfd)<<8;
	    ord->table_tag |= getc(sfd);
	    getint(sfd,&temp);
	    ord->ordered_features = galloc((temp+1)*sizeof(uint32));
	    ord->ordered_features[temp] = 0;
	    for ( i=0; i<temp; ++i ) {
		while ( isspace((ch=getc(sfd))) );
		if ( ch=='\'' ) {
		    ch = getc(sfd);
		    ord->ordered_features[i] = (ch<<24) | (getc(sfd)<<16);
		    ord->ordered_features[i] |= (getc(sfd)<<8);
		    ord->ordered_features[i] |= getc(sfd);
		    if ( (ch=getc(sfd))!='\'') ungetc(ch,sfd);
		} else if ( ch=='<' ) {
		    int f,s;
		    fscanf(sfd,"%d,%d>", &f, &s );
		    ord->ordered_features[i] = (f<<16)|s;
		}
	    }
	    if ( lastord==NULL )
		sf->orders = ord;
	    else
		lastord->next = ord;
	    lastord = ord;
	} else if ( strmatch(tok,"BeginPrivate:")==0 ) {
	    SFDGetPrivate(sfd,sf);
	} else if ( strmatch(tok,"BeginSubrs:")==0 ) {	/* leave in so we don't croak on old sfd files */
	    SFDGetSubrs(sfd,sf);
	} else if ( strmatch(tok,"MMCounts:")==0 ) {
	    MMSet *mm = sf->mm = chunkalloc(sizeof(MMSet));
	    getint(sfd,&mm->instance_count);
	    getint(sfd,&mm->axis_count);
	    mm->instances = galloc(mm->instance_count*sizeof(SplineFont *));
	    mm->positions = galloc(mm->instance_count*mm->axis_count*sizeof(real));
	    mm->defweights = galloc(mm->instance_count*sizeof(real));
	    mm->axismaps = galloc(mm->axis_count*sizeof(struct axismap));
	} else if ( strmatch(tok,"MMAxis:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL ) {
		for ( i=0; i<mm->axis_count; ++i ) {
		    getname(sfd,tok);
		    mm->axes[i] = copy(tok);
		}
	    }
	} else if ( strmatch(tok,"MMPositions:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL ) {
		for ( i=0; i<mm->axis_count*mm->instance_count; ++i )
		    getreal(sfd,&mm->positions[i]);
	    }
	} else if ( strmatch(tok,"MMWeights:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL ) {
		for ( i=0; i<mm->instance_count; ++i )
		    getreal(sfd,&mm->defweights[i]);
	    }
	} else if ( strmatch(tok,"MMAxisMap:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL ) {
		int index, points;
		getint(sfd,&index); getint(sfd,&points);
		mm->axismaps[index].points = points;
		mm->axismaps[index].blends = galloc(points*sizeof(real));
		mm->axismaps[index].designs = galloc(points*sizeof(real));
		for ( i=0; i<points; ++i ) {
		    getreal(sfd,&mm->axismaps[index].blends[i]);
		    while ( (ch=getc(sfd))!=EOF && isspace(ch));
		    ungetc(ch,sfd);
		    if ( (ch=getc(sfd))!='=' )
			ungetc(ch,sfd);
		    else if ( (ch=getc(sfd))!='>' )
			ungetc(ch,sfd);
		    getreal(sfd,&mm->axismaps[index].designs[i]);
		}
	    }
	} else if ( strmatch(tok,"MMCDV:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL )
		mm->cdv = SFDParseMMSubroutine(sfd);
	} else if ( strmatch(tok,"MMNDV:")==0 ) {
	    MMSet *mm = sf->mm;
	    if ( mm!=NULL )
		mm->ndv = SFDParseMMSubroutine(sfd);
	} else if ( strmatch(tok,"BeginMMFonts:")==0 ) {
	    int cnt;
	    getint(sfd,&cnt);
	    getint(sfd,&realcnt);
	    GProgressChangeStages(cnt);
	    GProgressChangeTotal(realcnt);
    break;
	} else if ( strmatch(tok,"BeginSubFonts:")==0 ) {
	    getint(sfd,&sf->subfontcnt);
	    sf->subfonts = gcalloc(sf->subfontcnt,sizeof(SplineFont *));
	    getint(sfd,&realcnt);
	    GProgressChangeStages(2);
	    GProgressChangeTotal(realcnt);
    break;
	} else if ( strmatch(tok,"BeginChars:")==0 ) {
	    getint(sfd,&sf->charcnt);
	    if ( getint(sfd,&realcnt)!=1 ) {
		GProgressChangeTotal(sf->charcnt);
	    } else if ( realcnt!=-1 ) {
		GProgressChangeTotal(realcnt);
	    }
	    sf->chars = gcalloc(sf->charcnt,sizeof(SplineChar *));
    break;
#if HANYANG
	} else if ( strmatch(tok,"BeginCompositionRules")==0 ) {
	    sf->rules = SFDReadCompositionRules(sfd);
#endif
	} else {
	    /* If we don't understand it, skip it */
	    geteol(sfd,tok);
	}
    }

    if ( sf->subfontcnt!=0 ) {
	int max,k;
	GProgressChangeStages(2*sf->subfontcnt);
	for ( i=0; i<sf->subfontcnt; ++i ) {
	    if ( i!=0 )
		GProgressNextStage();
	    sf->subfonts[i] = SFD_GetFont(sfd,sf,tok);
	}
	max = 0;
	for ( i=0; i<sf->subfontcnt; ++i )
	    if ( sf->subfonts[i]->charcnt>max ) max = sf->subfonts[i]->charcnt;
	for ( k=0; k<max; ++k ) {
	    for ( i=0; i<sf->subfontcnt; ++i )
		if ( k<sf->subfonts[i]->charcnt &&
			sf->subfonts[i]->chars[k]!=NULL ) {
		    sf->subfonts[i]->chars[k]->orig_pos = k;
	    break;
		}
	}
    } else if ( sf->mm!=NULL ) {
	MMSet *mm = sf->mm;
	GProgressChangeStages(2*(mm->instance_count+1));
	for ( i=0; i<mm->instance_count; ++i ) {
	    if ( i!=0 )
		GProgressNextStage();
	    mm->instances[i] = SFD_GetFont(sfd,NULL,tok);
	    mm->instances[i]->mm = mm;
	}
	GProgressNextStage();
	mm->normal = SFD_GetFont(sfd,NULL,tok);
	mm->normal->mm = mm;
	sf->mm = NULL;
	SplineFontFree(sf);
	sf = mm->normal;
    } else {
	glyph = 0;
	while ( (sc = SFDGetChar(sfd,sf))!=NULL ) {
	    if ( sc->orig_pos==0xffff )
		sc->orig_pos = glyph++;
	    else
		glyph = sc->orig_pos+1;
	    GProgressNext();
	}
	GProgressNextStage();
	GProgressChangeLine2R(_STR_InterpretingGlyphs);
	SFDFixupRefs(sf);
    }
    while ( getname(sfd,tok)==1 ) {
	if ( strcmp(tok,"EndSplineFont")==0 || strcmp(tok,"EndSubSplineFont")==0 )
    break;
	else if ( strcmp(tok,"BitmapFont:")==0 )
	    SFDGetBitmapFont(sfd,sf);
    }
    SFDCleanupFont(sf);
return( sf );
}

static int SFDStartsCorrectly(FILE *sfd,char *tok) {
    real dval;
    int ch;

    if ( getname(sfd,tok)!=1 )
return( false );
    if ( strcmp(tok,"SplineFontDB:")!=0 )
return( false );
    if ( getreal(sfd,&dval)!=1 || (dval!=0 && dval!=1))
return( false );
    ch = getc(sfd); ungetc(ch,sfd);
    if ( ch!='\r' && ch!='\n' )
return( false );

return( true );
}

SplineFont *SFDRead(char *filename) {
    FILE *sfd = fopen(filename,"r");
    SplineFont *sf=NULL;
    char *oldloc;
    char tok[2000];

    if ( sfd==NULL )
return( NULL );
    oldloc = setlocale(LC_NUMERIC,"C");
    GProgressChangeStages(2);
    if ( SFDStartsCorrectly(sfd,tok) )
	sf = SFD_GetFont(sfd,NULL,tok);
    setlocale(LC_NUMERIC,oldloc);
    if ( sf!=NULL ) {
	sf->filename = copy(filename);
	if ( sf->mm!=NULL ) {
	    int i;
	    for ( i=0; i<sf->mm->instance_count; ++i )
		sf->mm->instances[i]->filename = copy(filename);
	}
    }
    fclose(sfd);
return( sf );
}

SplineChar *SFDReadOneChar(char *filename,const char *name) {
    FILE *sfd = fopen(filename,"r");
    SplineChar *sc=NULL;
    char *oldloc;
    char tok[2000];
    uint32 pos;
    SplineFont sf;

    if ( sfd==NULL )
return( NULL );
    oldloc = setlocale(LC_NUMERIC,"C");

    memset(&sf,0,sizeof(sf));
    sf.ascent = 800; sf.descent = 200;
    if ( SFDStartsCorrectly(sfd,tok) ) {
	pos = ftell(sfd);
	while ( getname(sfd,tok)!=-1 ) {
	    if ( strcmp(tok,"StartChar:")==0 ) {
		if ( getname(sfd,tok)==1 && strcmp(tok,name)==0 ) {
		    fseek(sfd,pos,SEEK_SET);
		    sc = SFDGetChar(sfd,&sf);
	break;
		}
	    } else if ( strmatch(tok,"Order2:")==0 ) {
		int order2;
		getint(sfd,&order2);
		sf.order2 = order2;
	    } else if ( strmatch(tok,"Ascent:")==0 ) {
		getint(sfd,&sf.ascent);
	    } else if ( strmatch(tok,"Descent:")==0 ) {
		getint(sfd,&sf.descent);
	    }
	    pos = ftell(sfd);
	}
    }

    setlocale(LC_NUMERIC,oldloc);
    fclose(sfd);
return( sc );
}

static int ModSF(FILE *asfd,SplineFont *sf) {
    int newmap, cnt, order2=0;
    char tok[200];
    int i,k;
    SplineChar *sc;
    SplineFont *ssf;
    SplineFont temp;

    if ( getname(asfd,tok)!=1 || strcmp(tok,"Encoding:")!=0 )
return(false);
    getint(asfd,&newmap);
    if ( sf->encoding_name!=newmap )
	SFReencodeFont(sf,newmap);
    if ( getname(asfd,tok)!=1 )
return( false );
    if ( strcmp(tok,"Order2:")==0 ) {
	getint(asfd,&order2);
	if ( getname(asfd,tok)!=1 )
return( false );
    }
    if ( order2!=sf->order2 ) {
	if ( order2 )
	    SFConvertToOrder2(sf);
	else
	    SFConvertToOrder3(sf);
    }
    if ( strcmp(tok,"BeginChars:")!=0 )
return(false);
    SFRemoveDependencies(sf);

    memset(&temp,0,sizeof(temp));
    temp.ascent = sf->ascent; temp.descent = sf->descent;
    temp.order2 = sf->order2;

    getint(asfd,&cnt);
    if ( cnt>sf->charcnt ) {
	sf->chars = grealloc(sf->chars,cnt*sizeof(SplineChar *));
	for ( i=sf->charcnt; i<cnt; ++i )
	    sf->chars[i] = NULL;
    }
    while ( (sc = SFDGetChar(asfd,&temp))!=NULL ) {
	ssf = sf;
	for ( k=0; k<sf->subfontcnt; ++k ) {
	    if ( sc->enc<sf->subfonts[k]->charcnt ) {
		ssf = sf->subfonts[k];
		if ( SCWorthOutputting(ssf->chars[sc->enc]))
	break;
	    }
	}
	if ( sc->enc<ssf->charcnt ) {
	    if ( ssf->chars[sc->enc]!=NULL )
		SplineCharFree(ssf->chars[sc->enc]);
	    ssf->chars[sc->enc] = sc;
	    sc->parent = ssf;
	    sc->changed = true;
	}
    }
    sf->changed = true;
    SFDFixupRefs(sf);
return(true);
}

static SplineFont *SlurpRecovery(FILE *asfd,char *tok,int sizetok) {
    char *pt; int ch;
    SplineFont *sf;

    ch=getc(asfd);
    ungetc(ch,asfd);
    if ( ch=='B' ) {
	if ( getname(asfd,tok)!=1 )
return(NULL);
	if ( strcmp(tok,"Base:")!=0 )
return(NULL);
	while ( isspace(ch=getc(asfd)) && ch!=EOF && ch!='\n' );
	for ( pt=tok; ch!=EOF && ch!='\n'; ch = getc(asfd) )
	    if ( pt<tok+sizetok-2 )
		*pt++ = ch;
	*pt = '\0';
	sf = LoadSplineFont(tok,0);
    } else {
	sf = SplineFontNew();
	sf->onlybitmaps = false;
	strcpy(tok,"<New File>");
    }
    if ( sf==NULL )
return( NULL );

    if ( !ModSF(asfd,sf)) {
	SplineFontFree(sf);
return( NULL );
    }
return( sf );
}

SplineFont *SFRecoverFile(char *autosavename) {
    FILE *asfd = fopen( autosavename,"r");
    SplineFont *ret;
    char *oldloc;
    char tok[1025];

    if ( asfd==NULL )
return(NULL);
    oldloc = setlocale(LC_NUMERIC,"C");
    ret = SlurpRecovery(asfd,tok,sizeof(tok));
    if ( ret==NULL ) {
	static int buts[] = { _STR_ForgetIt, _STR_TryAgain, 0 };
	if ( GWidgetAskR(_STR_RecoveryFailed,buts,0,1,_STR_RecoveryOfFailed,tok)==0 )
	    unlink(autosavename);
    }
    setlocale(LC_NUMERIC,oldloc);
    fclose(asfd);
    if ( ret )
	ret->autosavename = copy(autosavename);
return( ret );
}

void SFAutoSave(SplineFont *sf) {
    int i, k, max;
    FILE *asfd;
    char *oldloc;
    SplineFont *ssf;

    if ( screen_display==NULL )		/* No autosaves when just scripting */
return;

    if ( sf->cidmaster!=NULL ) sf=sf->cidmaster;
    asfd = fopen(sf->autosavename,"w");
    if ( asfd==NULL )
return;

    max = sf->charcnt;
    for ( i=0; i<sf->subfontcnt; ++i )
	if ( sf->subfonts[i]->charcnt>max ) max = sf->subfonts[i]->charcnt;

    oldloc = setlocale(LC_NUMERIC,"C");
    if ( !sf->new && sf->origname!=NULL )	/* might be a new file */
	fprintf( asfd, "Base: %s\n", sf->origname );
    fprintf( asfd, "Encoding: %d\n", sf->encoding_name );
    if ( sf->order2 )
	fprintf( asfd, "Order2: %d\n", sf->order2 );
    fprintf( asfd, "BeginChars: %d\n", max );
    for ( i=0; i<max; ++i ) {
	ssf = sf;
	for ( k=0; k<sf->subfontcnt; ++k ) {
	    if ( i<sf->subfonts[k]->charcnt ) {
		ssf = sf->subfonts[k];
		if ( SCWorthOutputting(ssf->chars[i]))
	break;
	    }
	}
	if ( ssf->chars[i]!=NULL && ssf->chars[i]->changed )
	    SFDDumpChar( asfd,ssf->chars[i]);
    }
    fprintf( asfd, "EndChars\n" );
    fprintf( asfd, "EndSplineFont\n" );
    fclose(asfd);
    setlocale(LC_NUMERIC,oldloc);
    sf->changed_since_autosave = false;
}

void SFClearAutoSave(SplineFont *sf) {
    int i;
    SplineFont *ssf;

    if ( sf->cidmaster!=NULL ) sf = sf->cidmaster;
    sf->changed_since_autosave = false;
    for ( i=0; i<sf->subfontcnt; ++i ) {
	ssf = sf->subfonts[i];
	ssf->changed_since_autosave = false;
	if ( ssf->autosavename!=NULL ) {
	    unlink( ssf->autosavename );
	    free( ssf->autosavename );
	    ssf->autosavename = NULL;
	}
    }
    if ( sf->autosavename==NULL )
return;
    unlink(sf->autosavename);
    free(sf->autosavename);
    sf->autosavename = NULL;
}

char **NamesReadSFD(char *filename) {
    FILE *sfd = fopen(filename,"r");
    char *oldloc;
    char tok[2000];
    char **ret;
    int eof;

    if ( sfd==NULL )
return( NULL );
    oldloc = setlocale(LC_NUMERIC,"C");
    if ( SFDStartsCorrectly(sfd,tok) ) {
	while ( !feof(sfd)) {
	    if ( (eof = getname(sfd,tok))!=1 ) {
		if ( eof==-1 )
	break;
		geteol(sfd,tok);
	continue;
	    }
	    if ( strmatch(tok,"FontName:")==0 ) {
		getname(sfd,tok);
		ret = galloc(2*sizeof(char*));
		ret[0] = copy(tok);
		ret[1] = NULL;
	break;
	    }
	}
    }
    setlocale(LC_NUMERIC,oldloc);
    fclose(sfd);
return( ret );
}
