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
#include "pfaeditui.h"
#include <gfile.h>
#include <math.h>
#include "utype.h"
#include "ustring.h"
#include <unistd.h>

/* The pcf code is adapted from... */
/*

Copyright 1991, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * Author:  Keith Packard, MIT X Consortium
 */

/* ******************************** BDF ************************************* */

static int gettoken(FILE *bdf, char *tokbuf, int size) {
    char *pt=tokbuf, *end = tokbuf+size-2; int ch;

    while ( isspace(ch = getc(bdf)));
    while ( ch!=EOF && !isspace(ch) && ch!='[' && ch!=']' && ch!='{' && ch!='}' && ch!='<' && ch!='%' ) {
	if ( pt<end ) *pt++ = ch;
	ch = getc(bdf);
    }
    if ( pt==tokbuf && ch!=EOF )
	*pt++ = ch;
    else
	ungetc(ch,bdf);
    *pt='\0';
return( pt!=tokbuf?1:ch==EOF?-1: 0 );
}

static void ExtendSF(SplineFont *sf, int enc, int set) {
    BDFFont *b;
    FontView *fvs;

    if ( enc>=sf->charcnt ) {
	int i,n = enc;
	if ( !set )
	    n += 100;
	sf->chars = grealloc(sf->chars,n*sizeof(SplineChar *));
	for ( i=sf->charcnt; i<n; ++i )
	    sf->chars[i] = NULL;
	sf->charcnt = n;
	for ( b=sf->bitmaps; b!=NULL; b=b->next ) if ( b->charcnt<n ) {
	    b->chars = grealloc(b->chars,n*sizeof(BDFChar *));
	    for ( i=b->charcnt; i<n; ++i )
		b->chars[i] = NULL;
	    b->charcnt = n;
	}
	if ( sf->fv!=NULL ) {
	    for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
		free(fvs->selected);
		fvs->selected = gcalloc(sf->charcnt,1);
	    }
	    FontViewReformatAll(sf);
	}
    }
}

static void MakeEncChar(SplineFont *sf,int enc,char *name) {

    ExtendSF(sf,enc,false);

    if ( sf->chars[enc]==NULL ) {
	sf->chars[enc] = SplineCharCreate();
	sf->chars[enc]->parent = sf;
	sf->chars[enc]->width = sf->chars[enc]->vwidth = sf->ascent+sf->descent;
    }
    free(sf->chars[enc]->name);
    sf->chars[enc]->name = copy(name);
    sf->chars[enc]->unicodeenc = UniFromName(name);
    sf->chars[enc]->script = ScriptFromUnicode(sf->chars[enc]->unicodeenc,sf);
    sf->chars[enc]->enc = enc;
    /*sf->encoding_name = em_none;*/
}

static int figureProperEncoding(SplineFont *sf,BDFFont *b, int enc,char *name,
	int swidth, int swidth1) {
    int i = -1;

    /* Need to do something about 94x94 encodings where the encoding we get */
    /*  is not the same as what we use internally !!! */
    if ( strcmp(name,".notdef")==0 ) {
	if ( enc<32 || (enc>=127 && enc<0xa0)) i=enc;
	else if ( sf->encoding_name==em_none ) i = enc;
	else if ( sf->onlybitmaps && ((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) ) i = enc;
	if ( i>=sf->charcnt ) i = -1;
	if ( i!=-1 && (sf->chars[i]==NULL || strcmp(sf->chars[i]->name,name)!=0 )) {
	    MakeEncChar(sf,enc,name);
	    if ( sf->onlybitmaps ) {
		sf->chars[enc]->width = swidth;
		sf->chars[enc]->widthset = true;
		if ( swidth1!=-1 )
		    sf->chars[enc]->vwidth = swidth1;
	    }
	}
    } else {
	if ( enc!=-1 && enc<sf->charcnt && sf->chars[enc]!=NULL &&
		strcmp(sf->chars[enc]->name,name)==0 )
	    i = enc;
	else for ( i=sf->charcnt-1; i>=0 ; --i ) if ( sf->chars[i]!=NULL ) {
	    if ( strcmp(name,sf->chars[i]->name)==0 )
	break;
	}
	if ( i==-1 && (sf->encoding_name==em_unicode || sf->encoding_name==em_unicode4 ||
		sf->encoding_name==em_iso8859_1)) {
	    i = UniFromName(name);
	    if ( i==-1 || i>sf->charcnt || sf->chars[i]!=NULL )
		i = -1;
	    if ( i!=-1 )
		SFMakeChar(sf,i);
	}
	if ( sf->onlybitmaps && i!=-1 &&
		((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) &&
		i!=enc && enc!=-1 && (enc>=b->charcnt || b->chars[enc]==NULL)) {
	    i = -1;
	    if ( !sf->dupnamewarn ) {
		/*GWidgetErrorR(_STR_DuplicateName,_STR_DuplicateCharName,name);*/
		/* happens too often, no break space for example */
		sf->dupnamewarn = true;
	    }
	}
	if ( i==-1 && sf->onlybitmaps && enc!=-1 &&
		((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) ) {
	    MakeEncChar(sf,enc,name);
	    i = enc;
	}
	if ( i!=-1 && sf->onlybitmaps && swidth!=-1 &&
		((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) ) {
	    sf->chars[i]->width = swidth;
	    sf->chars[i]->widthset = true;
	    if ( swidth1!=-1 )
		sf->chars[i]->vwidth = swidth1;
	} else if ( i!=-1 && swidth!=-1 && sf->chars[i]!=NULL &&
		sf->chars[i]->splines==NULL && sf->chars[i]->refs==NULL &&
		!sf->chars[i]->widthset ) {
	    sf->chars[i]->width = swidth;
	    sf->chars[i]->widthset = true;
	    if ( swidth1!=-1 )
		sf->chars[i]->vwidth = swidth1;
	}
    }
    if ( i==-1 && sf->onlybitmaps &&
	    ((sf->bitmaps==b && b->next==NULL ) || sf->bitmaps==NULL) ) {
	/* try adding it to the end of the font */
	if ( enc>=sf->charcnt ) i=enc;
	else {
	    int cnt = CountOfEncoding(sf->encoding_name);
	    i = sf->charcnt;
	    while ( i>cnt && i>0 && sf->chars[i-1]==NULL )
		--i;
	}
	MakeEncChar(sf,i,name);
    }
    if ( i==-1 )	/* Can't guess the proper encoding, ignore it */
return(-1);
    if ( i!=enc && enc!=-1 && sf->onlybitmaps && sf->chars[enc]!=NULL &&
	    ((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) ) {
	free(sf->chars[enc]->name);
	sf->chars[enc]->name = copy( ".notdef" );
	sf->chars[enc]->unicodeenc = -1;
    }
return( i );
}

struct metrics {
    int swidth, dwidth, swidth1, dwidth1;	/* Font wide width defaults */
    int metricsset, vertical_origin;
    int res;
};

static void AddBDFChar(FILE *bdf, SplineFont *sf, BDFFont *b,int depth, struct metrics *defs) {
    BDFChar *bc;
    char name[40], tok[100];
    int enc=-1, width=defs->dwidth, xmin=0, xmax=0, ymin=0, ymax=0, hsz, vsz;
    int swidth= defs->swidth, swidth1=defs->swidth1;
    int i,ch;
    BitmapView *bv;
    uint8 *pt, *end, *eol;

    gettoken(bdf,name,sizeof(tok));
    while ( gettoken(bdf,tok,sizeof(tok))!=-1 ) {
	if ( strcmp(tok,"ENCODING")==0 ) {
	    fscanf(bdf,"%d",&enc);
	    /* Adobe says that enc is value for Adobe Standard */
	    /* But people don't use it that way. Adobe also says that if */
	    /* there is no mapping in adobe standard the -1 may be followed */
	    /* by another value, the local encoding. */
	    if ( enc==-1 ) {
		ch = getc(bdf);
		if ( ch==' ' || ch=='\t' )
		    fscanf(bdf,"%d",&enc);
		else
		    ungetc(ch,bdf);
	    }
	} else if ( strcmp(tok,"DWIDTH")==0 )
	    fscanf(bdf,"%d %*d",&width);
	else if ( strcmp(tok,"SWIDTH")==0 )
	    fscanf(bdf,"%d %*d",&swidth);
	else if ( strcmp(tok,"SWIDTH1")==0 )
	    fscanf(bdf,"%d %*d",&swidth1);
	else if ( strcmp(tok,"BBX")==0 ) {
	    fscanf(bdf,"%d %d %d %d",&hsz, &vsz, &xmin, &ymin );
	    xmax = hsz+xmin-1;
	    ymax = vsz+ymin-1;
	} else if ( strcmp(tok,"BITMAP")==0 )
    break;
    }
    i = figureProperEncoding(sf,b,enc,name,swidth,swidth1);
    if ( i!=-1 ) {
	if ( (bc=b->chars[i])!=NULL ) {
	    free(bc->bitmap);
	    BDFFloatFree(bc->selection);
	} else {
	    b->chars[i] = bc = chunkalloc(sizeof(BDFChar));
	    bc->sc = sf->chars[i];
	    bc->enc = i;
	}
	bc->xmin = xmin;
	bc->ymin = ymin;
	bc->xmax = xmax;
	bc->ymax = ymax;
	bc->width = width;
	if ( depth==1 ) {
	    bc->bytes_per_line = ((xmax-xmin)>>3) + 1;
	    bc->byte_data = false;
	} else {
	    bc->bytes_per_line = xmax-xmin + 1;
	    bc->byte_data = true;
	}
	bc->depth = depth;
	bc->bitmap = galloc(bc->bytes_per_line*(ymax-ymin+1));

	pt = bc->bitmap; end = pt + bc->bytes_per_line*(ymax-ymin+1);
	eol = pt + bc->bytes_per_line;
	while ( pt<end ) {
	    int ch1, ch2, val;
	    while ( isspace(ch1=getc(bdf)) );
	    ch2 = getc(bdf);
	    val = 0;
	    if ( ch1>='0' && ch1<='9' ) val = (ch1-'0')<<4;
	    else if ( ch1>='a' && ch1<='f' ) val = (ch1-'a'+10)<<4;
	    else if ( ch1>='A' && ch1<='F' ) val = (ch1-'A'+10)<<4;
	    if ( ch2>='0' && ch2<='9' ) val |= (ch2-'0');
	    else if ( ch2>='a' && ch2<='f' ) val |= (ch2-'a'+10);
	    else if ( ch2>='A' && ch2<='F' ) val |= (ch2-'A'+10);
	    if ( depth==1 || depth==8 )
		*pt++ = val;
	    else if ( depth==2 ) {		/* Internal representation is unpacked, one byte per pixel */
		*pt++ = (val>>6);
		if ( pt<eol ) *pt++ = (val>>4)&3;
		if ( pt<eol ) *pt++ = (val>>2)&3;
		if ( pt<eol ) *pt++ = val&3;
	    } else if ( depth==4 ) {
		*pt++ = (val>>4);
		if ( pt<eol ) *pt++ = val&0xf;
	    } else if ( depth==16 || depth==32 ) {
		int i,j;
		*pt++ = val;
		/* I only deal with 8 bit pixels, so if they give me more */
		/*  just ignore the low order bits */
		j = depth==16?2:6;
		for ( i=0; i<j; ++j )
		    ch2 = getc(bdf);
	    }
	    if ( pt>=eol ) eol += bc->bytes_per_line;
	    if ( ch2==EOF )
	break;
	}

	for ( bv=bc->views; bv!=NULL; bv=bv->next )
	    GDrawRequestExpose(bv->v,NULL,false);
    } else {
	int cnt;
	if ( depth==1 )
	    cnt = 2*(((xmax-xmin)>>3) + 1) * (ymax-ymin+1);
	else if ( depth==2 )
	    cnt = 2*(((xmax-xmin)>>2) + 1) * (ymax-ymin+1);
	else if ( depth==4 )
	    cnt = (xmax-xmin + 1) * (ymax-ymin+1);
	else if ( depth==8 )
	    cnt = 2*(xmax-xmin + 1) * (ymax-ymin+1);
	else if ( depth==16 )
	    cnt = 4*(xmax-xmin + 1) * (ymax-ymin+1);
	else
	    cnt = 8*(xmax-xmin + 1) * (ymax-ymin+1);
	for ( i=0; i<cnt; ++i ) {
	    while ( isspace(getc(bdf)) );
	    getc(bdf);
	}
    }
}

static int BDFParseEnc(char *encname, int encoff) {
    int enc;

    enc = em_none;
    if ( strcmp(encname,"ISO8859")==0 || strcmp(encname,"ISO-8859")==0 ) {
	enc = em_iso8859_1+encoff;
	if ( enc>em_iso8859_11 )
	    --enc;
	if ( enc>em_iso8859_15 ) enc = em_iso8859_15;
    } else if ( strcmp(encname,"ISOLatin1Encoding")==0 ) {
	enc = em_iso8859_1;
    } else if ( strcmp(encname,"ISO10646")==0 || strcmp(encname,"ISO-10646")==0 ||
	    strcmp(encname,"Unicode")==0 || strcmp(encname,"UNICODE")==0 ) {
	enc = em_unicode;
	if ( encoff>1 )
	    enc = em_unicodeplanes+encoff-1;
    } else if ( strstrmatch(encname,"AdobeStandard")!=NULL ) {
	enc = em_adobestandard;
    } else if ( strstrmatch(encname,"Mac")!=NULL ) {
	enc = em_mac;
    } else if ( strstrmatch(encname,"Win")!=NULL || strstrmatch(encname,"ANSI")!=NULL ) {
	enc = em_win;
    } else if ( strstrmatch(encname,"koi8")!=NULL ) {
	enc = em_koi8_r;
    } else if ( strstrmatch(encname,"JISX208")!=NULL ) {
	enc = em_jis208;
    } else if ( strstrmatch(encname,"JISX212")!=NULL ) {
	enc = em_jis212;
    } else if ( strstrmatch(encname,"KSC5601")!=NULL ) {
	enc = em_ksc5601;
    } else if ( strstrmatch(encname,"GB2312")!=NULL ) {
	enc = em_gb2312;
    } else {
	Encoding *item;
	for ( item=enclist; item!=NULL && strstrmatch(encname,item->enc_name)==NULL; item=item->next );
	if ( item!=NULL )
	    enc = item->enc_num;
    }
return( enc );
}

static int slurp_header(FILE *bdf, int *_as, int *_ds, int *_enc,
	char *family, char *mods, char *full, int *depth, char *foundry,
	char *fontname, char *comments, struct metrics *defs) {
    int pixelsize = -1;
    int ascent= -1, descent= -1, enc, cnt;
    char tok[100], encname[100], weight[100], italic[100];
    int ch;
    int found_copyright=0;

    *depth = 1;
    encname[0]= '\0'; family[0] = '\0'; weight[0]='\0'; italic[0]='\0'; full[0]='\0';
    foundry[0]= '\0'; comments[0] = '\0';
    while ( gettoken(bdf,tok,sizeof(tok))!=-1 ) {
	if ( strcmp(tok,"CHARS")==0 ) {
	    cnt=0;
	    fscanf(bdf,"%d",&cnt);
	    GProgressChangeTotal(cnt);
    break;
	}
	if ( strcmp(tok,"FONT")==0 ) {
	    if ( fscanf(bdf," -%*[^-]-%[^-]-%[^-]-%[^-]-%*[^-]-", family, weight, italic )!=0 ) {
		while ( (ch = getc(bdf))!='-' && ch!='\n' && ch!=EOF );
		if ( ch=='-' ) {
		    fscanf(bdf,"%d", &pixelsize );
		    if ( pixelsize<0 ) pixelsize = -pixelsize;	/* An extra - screwed things up once... */
		}
	    } else {
		gettoken(bdf,tok,sizeof(tok));
		if ( *tok!='\0' && !isdigit(*tok))
		    strcpy(family,tok);
	    }
	} else if ( strcmp(tok,"SIZE")==0 ) {
	    int size, res;
	    fscanf(bdf, "%d %d %d", &size, &res, &res );
	    if ( pixelsize==-1 )
		pixelsize = rint( size*res/72.0 );
	    while ((ch = getc(bdf))==' ' || ch=='\t' );
	    ungetc(ch,bdf);
	    if ( isdigit(ch))
		fscanf(bdf, "%d", depth);
	} else if ( strcmp(tok,"BITSPERPIXEL")==0 ||
		strcmp(tok,"BITS_PER_PIXEL")==0 ) {
	    fscanf(bdf, "%d", depth);
	} else if ( strcmp(tok,"QUAD_WIDTH")==0 && pixelsize==-1 )
	    fscanf(bdf, "%d", &pixelsize );
	    /* For Courier the quad is not an em */
	else if ( strcmp(tok,"RESOLUTION_X")==0 )
	    fscanf(bdf, "%d", &defs->res );
	else if ( strcmp(tok,"FONT_ASCENT")==0 )
	    fscanf(bdf, "%d", &ascent );
	else if ( strcmp(tok,"FONT_DESCENT")==0 )
	    fscanf(bdf, "%d", &descent );
	else if ( strcmp(tok,"SWIDTH")==0 )
	    fscanf(bdf, "%d", &defs->swidth );
	else if ( strcmp(tok,"SWIDTH1")==0 )
	    fscanf(bdf, "%d", &defs->swidth1 );
	else if ( strcmp(tok,"DWIDTH")==0 )
	    fscanf(bdf, "%d", &defs->dwidth );
	else if ( strcmp(tok,"DWIDTH1")==0 )
	    fscanf(bdf, "%d", &defs->dwidth1 );
	else if ( strcmp(tok,"METRICSSET")==0 )
	    fscanf(bdf, "%d", &defs->metricsset );
	else if ( strcmp(tok,"VVECTOR")==0 )
	    fscanf(bdf, "%*d %d", &defs->vertical_origin );
	else if ( strcmp(tok,"FOUNDRY")==0 )
	    fscanf(bdf, " \"%[^\"]", foundry );
	else if ( strcmp(tok,"FONT_NAME")==0 )
	    fscanf(bdf, " \"%[^\"]", fontname );
	else if ( strcmp(tok,"CHARSET_REGISTRY")==0 )
	    fscanf(bdf, " \"%[^\"]", encname );
	else if ( strcmp(tok,"CHARSET_ENCODING")==0 ) {
	    enc = 0;
	    if ( fscanf(bdf, " \"%d", &enc )!=1 )
		fscanf(bdf, "%d", &enc );
	} else if ( strcmp(tok,"FAMILY_NAME")==0 ) {
	    ch = getc(bdf);
	    ch = getc(bdf); ungetc(ch,bdf);
	    fscanf(bdf, " \"%[^\"]", family );
	} else if ( strcmp(tok,"FULL_NAME")==0 ) {
	    ch = getc(bdf);
	    ch = getc(bdf); ungetc(ch,bdf);
	    fscanf(bdf, " \"%[^\"]", full );
	} else if ( strcmp(tok,"WEIGHT_NAME")==0 )
	    fscanf(bdf, " \"%[^\"]", weight );
	else if ( strcmp(tok,"SLANT")==0 )
	    fscanf(bdf, " \"%[^\"]", italic );
	else if ( strcmp(tok,"COPYRIGHT")==0 ) {
	    char *pt = comments;
	    char *eoc = comments+1000-1;
	    while ((ch=getc(bdf))==' ' || ch=='\t' );
	    while ( ch!='\n' && ch!='\r' && ch!=EOF ) {
		if ( pt<eoc )
		    *pt++ = ch;
		ch = getc(bdf);
	    }
	    *pt = '\0';
	    ungetc('\n',bdf);
	    found_copyright = true;
	} else if ( strcmp(tok,"COMMENT")==0 && !found_copyright ) {
	    char *pt = comments+strlen(comments);
	    char *eoc = comments+1000-1;
	    while ((ch=getc(bdf))==' ' || ch=='\t' );
	    while ( ch!='\n' && ch!='\r' && ch!=EOF ) {
		if ( pt<eoc )
		    *pt++ = ch;
		ch = getc(bdf);
	    }
	    if ( pt<eoc )
		*pt++ = '\n';
	    *pt = '\0';
	    ungetc('\n',bdf);
	}
	while ( (ch=getc(bdf))!='\n' && ch!='\r' && ch!=EOF );
    }
    if ( pixelsize==-1 && ascent!=-1 && descent!=-1 )
	pixelsize = ascent+descent;
    else if ( pixelsize!=-1 ) {
	if ( ascent==-1 && descent!=-1 )
	    ascent = pixelsize - descent;
	else if ( ascent!=-1 )
	    descent = pixelsize -ascent;
    }
    *_as = ascent; *_ds=descent;
    *_enc = BDFParseEnc(encname,enc);

    if ( strmatch(italic,"I")==0 )
	strcpy(italic,"Italic");
    else if ( strmatch(italic,"O")==0 )
	strcpy(italic,"Oblique");
    else if ( strmatch(italic,"R")==0 )
	strcpy(italic,"");		/* Ignore roman */
    sprintf(mods,"%s%s", weight, italic );
    if ( comments[0]!='\0' && comments[strlen(comments)-1]=='\n' )
	comments[strlen(comments)-1] = '\0';

    if ( *depth!=1 && *depth!=2 && *depth!=4 && *depth!=8 && *depth!=16 && *depth!=32 )
	fprintf( stderr, "PfaEdit does not support this bit depth %d (must be 1,2,4,8,16,32)\n", *depth);

return( pixelsize );
}

/* ******************************** GF (TeX) ******************************** */
enum gf_cmd { gf_paint_0=0, gf_paint_1=1, gf_paint_63=63,
	gf_paint1b=64, gf_paint2b, gf_paint3b,	/* followed by 1,2,or3 bytes saying how far to paint */
	gf_boc=67, /* 4bytes of character code, 4bytes of -1, min_col 4, min_row 4, max_col 4, max_row 4 */
	    /* set row=max_row, col=min_col, paint=white */
	gf_boc1=68, /* 1 byte quantities, and no -1 field, cc, width, max_col, height, max_row */
	gf_eoc=69,
	gf_skip0=70,	/* start next row (don't need to complete current) as white*/
	gf_skip1=71, gf_skip2=72, gf_skip3=73, /* followed by 1,2,3 bytes saying number rows to skip (leave white) */
	gf_newrow_0=74,	/* Same as skip, start drawing with black */
	gf_newrow_1=75, gf_newrow_164=238,
	gf_xxx1=239, /* followed by a 1 byte count, and then that many bytes of ignored data */
	gf_xxx2, gf_xxx3, gf_xxx4,
	gf_yyy=243, /* followed by 4 bytes of numeric data */
	gf_no_op=244,
	gf_char_loc=245, /* 1?byte encoding, 4byte dx, 4byte dy (<<16), 4byte advance width/design size <<24, 4byte offset to character's boc */
	gf_char_loc0=246, /* 1?byte encoding, 1byte dx, 4byte advance width/design size <<24, 4byte offset to character's boc */
	gf_pre=247, /* one byte containing gf_version_number, one byte count, that many bytes data */
	gf_post=248,	/* start of post amble */
	gf_post_post=249,	/* End of postamble, 4byte offset to start of preamble, 1byte gf_version_number, 4 or more bytes of 0xdf */
	/* rest 250..255 are undefined */
	gf_version_number=131 };
/* The postamble command is followed by 9 4byte quantities */
/*  the first is an offset to a list of xxx commands which I'm ignoring */
/*  the second is the design size */
/*  the third is a check sum */
/*  the fourth is horizontal pixels per point<<16 */
/*  the fifth is vertical pixels per point<<16 */
/*  the sixth..ninth give the font bounding box */
/* Then a bunch of char_locs (with back pointers to the characters and advance width info) */
/* Then a post_post with a back pointer to start of postamble + gf_version */
/* Then 4-7 bytes of 0xdf */

static BDFChar *SFGrowTo(SplineFont *sf,BDFFont *b, int cc) {
    char buf[20];
    int i;
    BDFChar *bc;

    if ( cc >= sf->charcnt ) {
	int new = ((sf->charcnt+255)>>8)<<8;
	sf->chars = grealloc(sf->chars,new*sizeof(SplineChar *));
	b->chars = grealloc(b->chars,new*sizeof(BDFChar *));
	for ( i=sf->charcnt; i<new; ++i ) {
	    sf->chars[i] = NULL;
	    b->chars[i] = NULL;
	}
	sf->charcnt = b->charcnt = new;
    }
    if ( sf->chars[cc]==NULL )
	SFMakeChar(sf,cc);
    if ( sf->onlybitmaps && ((sf->bitmaps==b && b->next==NULL) || sf->bitmaps==NULL) ) {
	free(sf->chars[cc]->name);
	sprintf( buf, "enc-%d", cc);
	sf->chars[cc]->name = copy( buf );
	sf->chars[cc]->unicodeenc = -1;
    }
    if ( cc>=b->charcnt )
	bc = chunkalloc(sizeof(BDFChar));
    else if ( (bc=b->chars[cc])!=NULL ) {
	free(bc->bitmap);
	BDFFloatFree(bc->selection);
    } else {
	b->chars[cc] = bc = chunkalloc(sizeof(BDFChar));
	bc->sc = sf->chars[cc];
	bc->enc = cc;
    }
return(bc);
}

static void gf_skip_noops(FILE *gf,char *char_name) {
    uint8 cmd;
    int32 val;
    int i;
    char buffer[257];

    if ( char_name ) *char_name = '\0';

    while ( 1 ) {
	cmd = getc(gf);
	switch( cmd ) {
	  case gf_no_op:		/* One byte no-op */
	  break;
	  case gf_yyy:			/* followed by a 4 byte value */
	    getc(gf); getc(gf); getc(gf); getc(gf);
	  break;
	  case gf_xxx1:
	    val = getc(gf);
	    for ( i=0; i<val; ++i ) buffer[i] = getc(gf);
	    buffer[i] = 0;
	    if (strncmp(buffer,"title",5)==0 && char_name!=NULL ) {
		char *pt = buffer+6, *to = char_name;
		while ( *pt ) {
		    if ( *pt=='(' ) {
			while ( *pt!=')' && *pt!='\0' ) ++pt;
		    } else if ( *pt==' ' || *pt==')' ) {
			if ( to==char_name || to[-1]!='-' )
			    *to++ = '-';
			++pt;
		    } else
			*to++ = *pt++;
		}
		if ( to!=char_name && to[-1]=='-' )
		    --to;
		*to = '\0';
	    }
	  break;
	  case gf_xxx2:
	    val = getc(gf);
	    val = (val<<8) | getc(gf);
	    for ( i=0; i<val; ++i ) getc(gf);
	  break;
	  case gf_xxx3:
	    val = getc(gf);
	    val = (val<<8) | getc(gf);
	    val = (val<<8) | getc(gf);
	    for ( i=0; i<val; ++i ) getc(gf);
	  break;
	  case gf_xxx4:
	    val = getlong(gf);
	    for ( i=0; i<val; ++i ) getc(gf);
	  break;
	  default:
	    ungetc(cmd,gf);
return;
	}
    }
}

static int gf_postamble(FILE *gf, int *_as, int *_ds, int *_enc, char *family,
	char *mods, char *full, char *filename) {
    int pixelsize=-1;
    int ch;
    int design_size, pixels_per_point;
    double size;
    char *pt, *fpt;
    int32 pos, off;

    fseek(gf,-4,SEEK_END);
    pos = ftell(gf);
    ch = getc(gf);
    if ( ch!=0xdf )
return( -2 );
    while ( ch==0xdf ) {
	--pos;
	fseek(gf,-2,SEEK_CUR);
	ch = getc(gf);
    }
    if ( ch!=gf_version_number )
return( -2 );
    pos -= 4;
    fseek(gf,pos,SEEK_SET);
    off = getlong(gf);
    fseek(gf,off,SEEK_SET);
    ch = getc(gf);
    if ( ch!=gf_post )
return( -2 );
    /* offset to comments */ getlong(gf);
    design_size = getlong(gf);
    /* checksum = */ getlong(gf);
    pixels_per_point = getlong(gf);
    /* vert pixels per point = */ getlong(gf);
    /* min_col = */ getlong(gf);
    /* min_row = */ getlong(gf);
    /* max_col = */ getlong(gf);
    /* max_row = */ getlong(gf);

    size = (pixels_per_point / (double) (0x10000)) * (design_size / (double) (0x100000));
    pixelsize = size+.5;
    *_enc = em_none;
    *_as = *_ds = -1;
    *mods = '\0';
    pt = strrchr(filename, '/');
    if ( pt==NULL ) pt = filename; else ++pt;
    for ( fpt=family; isalpha(*pt);)
	*fpt++ = *pt++;
    *fpt = '\0';
    strcpy(full,family);

return( pixelsize );
}

static int gf_char(FILE *gf, SplineFont *sf, BDFFont *b) {
    int32 pos, to;
    int ch, enc, dx, dy, aw;
    int min_c, max_c, min_r, max_r, w;
    int r,c, col,cnt,i;
    BDFChar *bc;
    char charname[256];

    /* We assume we are positioned within the postamble. we read one char_loc */
    /*  then read the char it points to, then return to postamble */
    ch = getc(gf);
    if ( ch== gf_char_loc ) {
	enc = getc(gf);
	dx = getlong(gf)>>16;
	dy = getlong(gf)>>16;
	aw = (getlong(gf)*(sf->ascent+sf->descent))>>20;
	to = getlong(gf);
    } else if ( ch==gf_char_loc0 ) {
	enc = getc(gf);
	dx = getc(gf);
	aw = (getlong(gf)*(sf->ascent+sf->descent))>>20;
	to = getlong(gf);
    } else
return( false );
    pos = ftell(gf);
    fseek(gf,to,SEEK_SET);

    gf_skip_noops(gf,charname);
    ch = getc(gf);
    if ( ch==gf_boc ) {
	/* encoding = */ getlong(gf);
	/* backpointer = */ getlong(gf);
	min_c = getlong(gf);
	max_c = getlong(gf);
	min_r = getlong(gf);
	max_r = getlong(gf);
    } else if ( ch==gf_boc1 ) {
	/* encoding = */ getc(gf);
	w = getc(gf);
	max_c = getc(gf);
	min_c = max_c-w+1;
	w = getc(gf);
	max_r = getc(gf);
	min_r = max_r-w+1;
    } else
return( false );

    bc = SFGrowTo(sf,b,enc);
    if ( charname[0]!='\0' && sf->onlybitmaps && (sf->bitmaps==NULL ||
	    (sf->bitmaps==b && b->next==NULL )) ) {
	free(sf->chars[enc]->name);
	sf->chars[enc]->name = copy(charname);
	sf->chars[enc]->unicodeenc = -1;
    }
    bc->xmin = min_c;
    bc->xmax = max_c>min_c? max_c : min_c;
    bc->ymin = min_r;
    bc->ymax = max_r>min_r? max_r : min_r;
    bc->width = dx;
    bc->bytes_per_line = ((bc->xmax-bc->xmin+8)>>3);
    bc->bitmap = gcalloc(bc->bytes_per_line*(bc->ymax-bc->ymin+1),1);

    if ( sf->chars[enc]->splines==NULL && sf->chars[enc]->refs==NULL &&
	    !sf->chars[enc]->widthset ) {
	sf->chars[enc]->width = aw;
	sf->chars[enc]->widthset = true;
    }

    for ( r=min_r, c=min_c, col=0; r<=max_r; ) {
	gf_skip_noops(gf,NULL);
	ch = getc(gf);
	if ( ch==gf_eoc )
    break;
	if ( ch>=gf_paint_0 && ch<=gf_paint3b ) {
	    if ( ch>=gf_paint_0 && ch<=gf_paint_63 )
		cnt = ch-gf_paint_0;
	    else if ( ch==gf_paint1b )
		cnt = getc(gf);
	    else if ( ch==gf_paint2b )
		cnt = getushort(gf);
	    else
		cnt = get3byte(gf);
	    if ( col ) {
		for ( i=0; i<cnt && c<=max_c; ++i ) {
		    bc->bitmap[(r-min_r)*bc->bytes_per_line+((c-min_c)>>3)]
			    |= (0x80>>((c-min_c)&7));
		    ++c;
		    /*if ( c>max_c ) { c-=max_c-min_c; ++r; }*/
		}
	    } else {
		c+=cnt;
		/*while ( c>max_c ) { c-=max_c-min_c; ++r; }*/
	    }
	    col = !col;
	} else if ( ch>=gf_newrow_0 && ch<=gf_newrow_164 ) {
	    ++r;
	    c = min_c + ch-gf_newrow_0;
	    col = 1;
	} else if ( ch>=gf_skip0 && ch<=gf_skip3 ) {
	    col = 0;
	    c = min_c;
	    if ( ch==gf_skip0 )
		++r;
	    else if ( ch==gf_skip1 )
		r += getc(gf)+1;
	    else if ( ch==gf_skip2 )
		r += getushort(gf)+1;
	    else
		r += get3byte(gf)+1;
	} else if ( ch==EOF ) {
	    fprintf( stderr, "Unexpected EOF in gf\n" );
    break;
	} else
	    fprintf( stderr, "Uninterpreted code in gf: %d\n", ch);
    }
    fseek(gf,pos,SEEK_SET);
return( true );
}

/* ******************************** PK (TeX) ******************************** */

enum pk_cmd { pk_rrr1=240, pk_rrr2, pk_rrr3, pk_rrr4, pk_yyy, pk_post, pk_no_op,
	pk_pre, pk_version_number=89 };
static void pk_skip_noops(FILE *pk) {
    uint8 cmd;
    int32 val;
    int i;

    while ( 1 ) {
	cmd = getc(pk);
	switch( cmd ) {
	  case pk_no_op:		/* One byte no-op */
	  break;
	  case pk_post:			/* Signals start of noop section */
	  break;
	  case pk_yyy:			/* followed by a 4 byte value */
	    getc(pk); getc(pk); getc(pk); getc(pk);
	  break;
	  case pk_rrr1:
	    val = getc(pk);
	    for ( i=0; i<val; ++i ) getc(pk);
	  break;
	  case pk_rrr2:
	    val = getc(pk);
	    val = (val<<8) | getc(pk);
	    for ( i=0; i<val; ++i ) getc(pk);
	  break;
	  case pk_rrr3:
	    val = getc(pk);
	    val = (val<<8) | getc(pk);
	    val = (val<<8) | getc(pk);
	    for ( i=0; i<val; ++i ) getc(pk);
	  break;
	  case pk_rrr4:
	    val = getc(pk);
	    val = (val<<8) | getc(pk);
	    val = (val<<8) | getc(pk);
	    val = (val<<8) | getc(pk);
	    for ( i=0; i<val; ++i ) getc(pk);
	  break;
	  default:
	    ungetc(cmd,pk);
return;
	}
    }
}

static int pk_header(FILE *pk, int *_as, int *_ds, int *_enc, char *family,
	char *mods, char *full, char *filename) {
    int pixelsize=-1;
    int ch,i;
    int design_size, pixels_per_point;
    double size;
    char *pt, *fpt;

    pk_skip_noops(pk);
    ch = getc(pk);
    if ( ch!=pk_pre )
return( -2 );
    ch = getc(pk);
    if ( ch!=pk_version_number )
return( -2 );
    ch = getc(pk);
    for ( i=0; i<ch; ++i ) getc(pk);		/* Skip comment. Perhaps that should be the family? */
    design_size = getlong(pk);
    /* checksum = */ getlong(pk);
    pixels_per_point = getlong(pk);
    /* vert pixels per point = */ getlong(pk);

    size = (pixels_per_point / 65536.0) * (design_size / (double) (0x100000));
    pixelsize = size+.5;
    *_enc = em_none;
    *_as = *_ds = -1;
    *mods = '\0';
    pt = strrchr(filename, '/');
    if ( pt==NULL ) pt = filename; else ++pt;
    for ( fpt=family; isalpha(*pt);)
	*fpt++ = *pt++;
    *fpt = '\0';
    strcpy(full,family);

return( pixelsize );
}

struct pkstate {
    int byte, hold;
    int rpt;
    int dyn_f;
    int cc;		/* for debug purposes */
};

static int pkgetcount(FILE *pk, struct pkstate *st) {
    int i,j;
#define getnibble(pk,st) (st->hold==1?(st->hold=0,(st->byte&0xf)):(st->hold=1,(((st->byte=getc(pk))>>4))) )

    while ( 1 ) {
	i = getnibble(pk,st);
	if ( i==0 ) {
	    j=0;
	    while ( i==0 ) { ++j; i=getnibble(pk,st); }
	    while ( j>0 ) { --j; i = (i<<4) + getnibble(pk,st); }
return( i-15 + (13-st->dyn_f)*16 + st->dyn_f );
	} else if ( i<=st->dyn_f ) {
return( i );
	} else if ( i<14 ) {
return( (i-st->dyn_f-1)*16 + getnibble(pk,st) + st->dyn_f + 1 );
	} else {
	    if ( st->rpt!=0 )
		fprintf( stderr, "Duplicate repeat row count in char %d of pk file\n", st->cc );
	    if ( i==15 ) st->rpt = 1;
	    else st->rpt = pkgetcount(pk,st);
 /*printf( "[%d]", st->rpt );*/
	}
    }
}

static int pk_char(FILE *pk, SplineFont *sf, BDFFont *b) {
    int flag = getc(pk);
    int black, size_is_2;
    int pl, cc, tfm, w, h, hoff, voff, dm, dx, dy;
    int i, ch, j,r,c,cnt;
    BDFChar *bc;
    struct pkstate st;
    int32 char_end;

    memset(&st,'\0', sizeof(st));

    /* flag byte */
    st.dyn_f = (flag>>4);
    if ( st.dyn_f==15 ) {
	ungetc(flag,pk);
return( 0 );
    }
    black = flag&8 ? 1 : 0;
    size_is_2 = flag&4 ? 1 : 0;

    if ( (flag&7)==7 ) {		/* long preamble, 4 byte sizes */
	pl = getlong(pk);
	cc = getlong(pk);
	char_end = ftell(pk) + pl;
	tfm = get3byte(pk);
	dx = getlong(pk)>>16;
	dy = getlong(pk)>>16;
	w = getlong(pk);
	h = getlong(pk);
	hoff = getlong(pk);
	voff = getlong(pk);
    } else if ( flag & 4 ) {		/* extended preamble, 2 byte sizes */
	pl = getushort(pk) + ((flag&3)<<16);
	cc = getc(pk);
	char_end = ftell(pk) + pl;
	tfm = get3byte(pk);
	dm = getushort(pk);
	dx = dm; dy = 0;
	w = getushort(pk);
	h = getushort(pk);
	hoff = (short) getushort(pk);
	voff = (short) getushort(pk);
    } else {				/* short, 1 byte sizes */
	pl = getc(pk) + ((flag&3)<<8);
	cc = getc(pk);
	char_end = ftell(pk) + pl;
	tfm = get3byte(pk);
	dm = getc(pk);
	dx = dm; dy = 0;
	w = getc(pk);
	h = getc(pk);
	hoff = (signed char) getc(pk);
	voff = (signed char) getc(pk);
    }
    st.cc = cc;			/* We can give better errors with this in st */
    /* hoff is -xmin, voff is ymax */
    /* w,h is the width,height of the bounding box */
    /* dx is the advance width? */
    /* cc is the character code */
    /* tfm is the advance width as a fraction of an em/2^20 so multiply by 1000/2^20 to get postscript values */
    /* dy is the advance height for vertical text? */

    bc = SFGrowTo(sf,b,cc);

    bc->xmin = -hoff;
    bc->ymax = voff;
    bc->xmax = w-1-hoff;
    bc->ymin = voff-h+1;
    bc->width = dx;
    bc->bytes_per_line = ((w+7)>>3);
    bc->bitmap = gcalloc(bc->bytes_per_line*h,1);

    if ( sf->chars[cc]->splines==NULL && sf->chars[cc]->refs==NULL &&
	    !sf->chars[cc]->widthset ) {
	sf->chars[cc]->width = (sf->ascent+sf->descent)*(double) tfm/(0x100000);
	sf->chars[cc]->widthset = true;
    }

    if ( w==0 && h==0 )
	/* Nothing */;
    else if ( st.dyn_f==14 ) {
	/* We've got raster data in the file */
	for ( i=0; i<((w*h+7)>>3); ++i ) {
	    ch = getc(pk);
	    for ( j=0; j<8; ++j ) {
		r = ((i<<3)+j)/w;
		c = ((i<<3)+j)%w;
		if ( r<h && (ch&(1<<(7-j))) )
		    bc->bitmap[r*bc->bytes_per_line+(c>>3)] |= (1<<(7-(c&7)));
	    }
	}
    } else {
	/* We've got run-length encoded data */
	r = c = 0;
	while ( r<h ) {
	    cnt = pkgetcount(pk,&st);
  /* if ( black ) printf( "%d", cnt ); else printf( "(%d)", cnt );*/
	    if ( c+cnt>=w && st.rpt!=0 ) {
		if ( black ) {
		    while ( c<w ) {
			bc->bitmap[r*bc->bytes_per_line+(c>>3)] |= (1<<(7-(c&7)));
			--cnt;
			++c;
		    }
		} else
		    cnt -= (w-c);
		for ( i=0; i<st.rpt && r+i+1<h; ++i )
		    memcpy(bc->bitmap+(r+i+1)*bc->bytes_per_line,
			    bc->bitmap+r*bc->bytes_per_line,
			    bc->bytes_per_line );
		r += st.rpt+1;
		c = 0;
		st.rpt = 0;
	    }
	    while ( cnt>0 && r<h ) {
		while ( c<w && cnt>0) {
		    if ( black )
			bc->bitmap[r*bc->bytes_per_line+(c>>3)] |= (1<<(7-(c&7)));
		    --cnt;
		    ++c;
		}
		if ( c==w ) {
		    c = 0;
		    ++r;
		}
	    }
	    black = !black;
	}
    }
    if ( cc>=b->charcnt )
	BDFCharFree(bc);
    if ( ftell(pk)!=char_end ) {
	fprintf( stderr, "The character, %d, was not read properly (or pk file is in bad format)\n At %d should be %d, off by %d\n", cc, ftell(pk), char_end, ftell(pk)-char_end );
	fseek(pk,char_end,SEEK_SET);
    }
 /* printf( "\n" ); */
return( 1 );
}

/* ****************************** PCF *************************************** */

struct toc {
    int type;
    int format;
    int size;		/* in 32bit words */
    int offset;
};

#define PCF_FILE_VERSION	(('p'<<24)|('c'<<16)|('f'<<8)|1)

    /* table types */
#define PCF_PROPERTIES		    (1<<0)
#define PCF_ACCELERATORS	    (1<<1)
#define PCF_METRICS		    (1<<2)
#define PCF_BITMAPS		    (1<<3)
#define PCF_INK_METRICS		    (1<<4)
#define	PCF_BDF_ENCODINGS	    (1<<5)
#define PCF_SWIDTHS		    (1<<6)
#define PCF_GLYPH_NAMES		    (1<<7)
#define PCF_BDF_ACCELERATORS	    (1<<8)

    /* formats */
#define PCF_DEFAULT_FORMAT	0x00000000
#define PCF_INKBOUNDS		0x00000200
#define PCF_ACCEL_W_INKBOUNDS	0x00000100
#define PCF_COMPRESSED_METRICS	0x00000100

#define PCF_GLYPH_PAD_MASK	(3<<0)
#define PCF_BYTE_MASK		(1<<2)
#define PCF_BIT_MASK		(1<<3)
#define PCF_SCAN_UNIT_MASK	(3<<4)

#define	PCF_FORMAT_MASK		0xffffff00
#define PCF_FORMAT_MATCH(a,b) (((a)&PCF_FORMAT_MASK) == ((b)&PCF_FORMAT_MASK))

#define MSBFirst	1
#define LSBFirst	0

#define PCF_BYTE_ORDER(f)	(((f) & PCF_BYTE_MASK)?MSBFirst:LSBFirst)
#define PCF_BIT_ORDER(f)	(((f) & PCF_BIT_MASK)?MSBFirst:LSBFirst)
#define PCF_GLYPH_PAD_INDEX(f)	((f) & PCF_GLYPH_PAD_MASK)
#define PCF_GLYPH_PAD(f)	(1<<PCF_GLYPH_PAD_INDEX(f))
#define PCF_SCAN_UNIT_INDEX(f)	(((f) & PCF_SCAN_UNIT_MASK) >> 4)
#define PCF_SCAN_UNIT(f)	(1<<PCF_SCAN_UNIT_INDEX(f))

#define GLYPHPADOPTIONS	4

struct pcfmetrics {
    short lsb;
    short rsb;
    short width;
    short ascent;
    short descent;
    short attrs;
};

struct pcfaccel {
    unsigned int noOverlap:1;
    unsigned int constantMetrics:1;
    unsigned int terminalFont:1;
    unsigned int constantWidth:1;
    unsigned int inkInside:1;
    unsigned int inkMetrics:1;
    unsigned int drawDirection:1;
    int ascent;
    int descent;
    int maxOverlap;
    struct pcfmetrics minbounds;
    struct pcfmetrics maxbounds;
    struct pcfmetrics ink_minbounds;
    struct pcfmetrics ink_maxbounds;
};
/* End declarations */

/* Code based on Keith Packard's pcfread.c in the X11 distribution */

static int getint32(FILE *file) {
    int val = getc(file);
    val |= (getc(file)<<8);
    val |= (getc(file)<<16);
    val |= (getc(file)<<24);
return( val );
}

static int getformint32(FILE *file,int format) {
    int val;
    if ( format&PCF_BYTE_MASK ) {
	val = getc(file);
	val = (val<<8) | getc(file);
	val = (val<<8) | getc(file);
	val = (val<<8) | getc(file);
    } else {
	val = getc(file);
	val |= (getc(file)<<8);
	val |= (getc(file)<<16);
	val |= (getc(file)<<24);
    }
return( val );
}

static int getformint16(FILE *file,int format) {
    int val;
    if ( format&PCF_BYTE_MASK ) {
	val = getc(file);
	val = (val<<8) | getc(file);
    } else {
	val = getc(file);
	val |= (getc(file)<<8);
    }
return( val );
}

static struct toc *pcfReadTOC(FILE *file) {
    int cnt, i;
    struct toc *toc;

    if ( getint32(file)!=PCF_FILE_VERSION )
return( NULL );
    cnt = getint32(file);
    toc = gcalloc(cnt+1,sizeof(struct toc));
    for ( i=0; i<cnt; ++i ) {
	toc[i].type = getint32(file);
	toc[i].format = getint32(file);
	toc[i].size = getint32(file);
	toc[i].offset = getint32(file);
    }

return( toc );
}

static int pcfSeekToType(FILE *file, struct toc *toc, int type) {
    int i;

    for ( i=0; toc[i].type!=0 && toc[i].type!=type; ++i );
    if ( toc[i].type==0 )
return( false );
    fseek(file,toc[i].offset,SEEK_SET);
return( true );
}

static void pcfGetMetrics(FILE *file,int compressed,int format,struct pcfmetrics *metric) {
    if ( compressed ) {
	metric->lsb = getc(file)-0x80;
	metric->rsb = getc(file)-0x80;
	metric->width = getc(file)-0x80;
	metric->ascent = getc(file)-0x80;
	metric->descent = getc(file)-0x80;
	metric->attrs = 0;
    } else {
	metric->lsb = getformint16(file,format);
	metric->rsb = getformint16(file,format);
	metric->width = getformint16(file,format);
	metric->ascent = getformint16(file,format);
	metric->descent = getformint16(file,format);
	metric->attrs = getformint16(file,format);
    }
}

static int pcfGetAccel(FILE *file, struct toc *toc,int which, struct pcfaccel *accel) {
    int format;

    if ( !pcfSeekToType(file,toc,which))
return(false);
    format = getint32(file);
    if ( (format&PCF_FORMAT_MASK)!=PCF_DEFAULT_FORMAT &&
	    (format&PCF_FORMAT_MASK)!=PCF_ACCEL_W_INKBOUNDS )
return(false);
    accel->noOverlap = getc(file);
    accel->constantMetrics = getc(file);
    accel->terminalFont = getc(file);
    accel->constantWidth = getc(file);
    accel->inkInside = getc(file);
    accel->inkMetrics = getc(file);
    accel->drawDirection = getc(file);
    /* padding = */ getc(file);
    accel->ascent = getformint32(file,format);
    accel->descent = getformint32(file,format);
    accel->maxOverlap = getformint32(file,format);
    pcfGetMetrics(file,false,format,&accel->minbounds);
    pcfGetMetrics(file,false,format,&accel->maxbounds);
    if ( (format&PCF_FORMAT_MASK)==PCF_ACCEL_W_INKBOUNDS ) {
	pcfGetMetrics(file,false,format,&accel->ink_minbounds);
	pcfGetMetrics(file,false,format,&accel->ink_maxbounds);
    } else {
	accel->ink_minbounds = accel->minbounds;
	accel->ink_maxbounds = accel->maxbounds;
    }
return( true );
}

static int pcf_properties(FILE *file,struct toc *toc, int *_as, int *_ds,
	int *_enc, char *family, char *mods, char *full) {
    int pixelsize = -1;
    int ascent= -1, descent= -1, enc;
    char encname[100], weight[100], italic[100];
    int cnt, i, format, strl;
    struct props { int name_offset; int isStr; int val; char *name; char *value; } *props;
    char *strs;

    if ( !pcfSeekToType(file,toc,PCF_PROPERTIES))
return(-2);
    format = getint32(file);
    if ( (format&PCF_FORMAT_MASK)!=PCF_DEFAULT_FORMAT )
return(-2);
    cnt = getformint32(file,format);
    props = galloc(cnt*sizeof(struct props));
    for ( i=0; i<cnt; ++i ) {
	props[i].name_offset = getformint32(file,format);
	props[i].isStr = getc(file);
	props[i].val = getformint32(file,format);
    }
    if ( cnt&3 )
	fseek(file,4-(cnt&3),SEEK_CUR);
    strl = getformint32(file,format);
    strs = galloc(strl);
    fread(strs,1,strl,file);
    for ( i=0; i<cnt; ++i ) {
	props[i].name = strs+props[i].name_offset;
	if ( props[i].isStr )
	    props[i].value = strs+props[i].val;
	else
	    props[i].value = NULL;
    }

    /* the properties here are almost exactly the same as the bdf ones */
    for ( i=0; i<cnt; ++i ) {
	if ( props[i].isStr ) {
	    if ( strcmp(props[i].name,"FAMILY_NAME")==0 )
		strcpy(family,props[i].value);
	    else if ( strcmp(props[i].name,"WEIGHT_NAME")==0 )
		strcpy(weight,props[i].value);
	    else if ( strcmp(props[i].name,"FULL_NAME")==0 )
		strcpy(full,props[i].value);
	    else if ( strcmp(props[i].name,"SLANT")==0 )
		strcpy(italic,props[i].value);
	    else if ( strcmp(props[i].name,"CHARSET_REGISTRY")==0 )
		strcpy(encname,props[i].value);
	    else if ( strcmp(props[i].name,"CHARSET_ENCODING")==0 )
		enc = strtol(props[i].value,NULL,10);
	} else {
	    if ( strcmp(props[i].name,"PIXEL_SIZE")==0 ||
		    ( pixelsize==-1 && strcmp(props[i].name,"QUAD_WIDTH")==0 ))
		pixelsize = props[i].val;
	    else if ( strcmp(props[i].name,"FONT_ASCENT")==0 )
		ascent = props[i].val;
	    else if ( strcmp(props[i].name,"FONT_DESCENT")==0 )
		descent = props[i].val;
	    else if ( strcmp(props[i].name,"CHARSET_ENCODING")==0 )
		enc = props[i].val;
	}
    }
    if ( ascent==-1 || descent==-1 ) {
	struct pcfaccel accel;
	if ( pcfGetAccel(file,toc,PCF_BDF_ACCELERATORS,&accel) ||
		pcfGetAccel(file,toc,PCF_ACCELERATORS,&accel)) {
	    ascent = accel.ascent;
	    descent = accel.descent;
	    if ( pixelsize==-1 )
		pixelsize = ascent+descent;
	}
    }
    *_as = ascent; *_ds=descent;
    *_enc = BDFParseEnc(encname,enc);

    if ( strmatch(italic,"I")==0 )
	strcpy(italic,"Italic");
    else if ( strmatch(italic,"O")==0 )
	strcpy(italic,"Oblique");
    else if ( strmatch(italic,"R")==0 )
	strcpy(italic,"");		/* Ignore roman */
    sprintf(mods,"%s%s", weight, italic );

    free(strs);
    free(props);

return( pixelsize );
}

static struct pcfmetrics *pcfGetMetricsTable(FILE *file, struct toc *toc,int which, int *metrics_cnt) {
    int format, cnt, i;
    struct pcfmetrics *metrics;

    if ( !pcfSeekToType(file,toc,which))
return(NULL);
    format = getint32(file);
    if ( (format&PCF_FORMAT_MASK)!=PCF_DEFAULT_FORMAT &&
	    (format&PCF_FORMAT_MASK)!=PCF_COMPRESSED_METRICS )
return(NULL);
    if ( (format&PCF_FORMAT_MASK)==PCF_COMPRESSED_METRICS ) {
	cnt = getformint16(file,format);
	metrics = galloc(cnt*sizeof(struct pcfmetrics));
	for ( i=0; i<cnt; ++i )
	    pcfGetMetrics(file,true,format,&metrics[i]);
    } else {
	cnt = getformint32(file,format);
	metrics = galloc(cnt*sizeof(struct pcfmetrics));
	for ( i=0; i<cnt; ++i )
	    pcfGetMetrics(file,false,format,&metrics[i]);
    }
    *metrics_cnt = cnt;
return( metrics );
}

static uint8 bitinvert[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static void BitOrderInvert(uint8 *bitmap,int sizebitmaps) {
    int i;

    for ( i=0; i<sizebitmaps; ++i )
	bitmap[i] = bitinvert[bitmap[i]];
}

static void TwoByteSwap(uint8 *bitmap,int sizebitmaps) {
    int i, t;

    for ( i=0; i<sizebitmaps-1; i+=2 ) {
	t = bitmap[i];
	bitmap[i] = bitmap[i+1];
	bitmap[i+1] = t;
    }
}

static void FourByteSwap(uint8 *bitmap,int sizebitmaps) {
    int i, t;

    for ( i=0; i<sizebitmaps-1; i+=2 ) {
	t = bitmap[i];
	bitmap[i] = bitmap[i+3];
	bitmap[i+3] = t;
	t = bitmap[i+1];
	bitmap[i+1] = bitmap[i+2];
	bitmap[i+2] = t;
    }
}

static int PcfReadBitmaps(FILE *file,struct toc *toc,BDFFont *b) {
    int format, cnt, i, sizebitmaps, j;
    int *offsets;
    uint8 *bitmap;
    int bitmapSizes[GLYPHPADOPTIONS];

    if ( !pcfSeekToType(file,toc,PCF_BITMAPS))
return(false);
    format = getint32(file);
    if ( (format&PCF_FORMAT_MASK)!=PCF_DEFAULT_FORMAT )
return(false);

    cnt = getformint32(file,format);
    if ( cnt!=b->charcnt )
return( false );
    offsets = galloc(cnt*sizeof(int));
    for ( i=0; i<cnt; ++i )
	offsets[i] = getformint32(file,format);
    for ( i=0; i<GLYPHPADOPTIONS; ++i )
	bitmapSizes[i] = getformint32(file, format);
    sizebitmaps = bitmapSizes[PCF_GLYPH_PAD_INDEX(format)];
    bitmap = galloc(sizebitmaps==0 ? 1 : sizebitmaps );
    fread(bitmap,1,sizebitmaps,file);
    if (PCF_BIT_ORDER(format) != MSBFirst )
	BitOrderInvert(bitmap,sizebitmaps);
    if ( PCF_SCAN_UNIT(format)==1 )
	/* Nothing to do */;
    else if ( PCF_BYTE_ORDER(format)==MSBFirst )
	/* Nothing to do */;
    else if ( PCF_SCAN_UNIT(format)==2 )
	TwoByteSwap(bitmap, sizebitmaps);
    else if ( PCF_SCAN_UNIT(format)==2 )
	FourByteSwap(bitmap, sizebitmaps);
    if ( PCF_GLYPH_PAD(format)==1 ) {
	for ( i=0; i<cnt; ++i ) {
	    BDFChar *bc = b->chars[i];
	    memcpy(bc->bitmap,bitmap+offsets[i],
		    bc->bytes_per_line * (bc->ymax-bc->ymin+1));
	    GProgressNext();
	}
    } else {
	int pad = PCF_GLYPH_PAD(format);
	for ( i=0; i<cnt; ++i ) {
	    BDFChar *bc = b->chars[i];
	    int bpl = ((bc->bytes_per_line+pad-1)/pad)*pad;
	    for ( j=bc->ymin; j<=bc->ymax; ++j )
		memcpy(bc->bitmap+(j-bc->ymin)*bc->bytes_per_line,
			bitmap+offsets[i]+(j-bc->ymin)*bpl,
			bc->bytes_per_line);
	    GProgressNext();
	}
    }
    free(bitmap);
    free(offsets);
return( true );
}

static void PcfReadEncodingsNames(FILE *file,struct toc *toc,SplineFont *sf, BDFFont *b) {
    int format, cnt, i, stringsize;
    int *offsets=NULL;
    char *string=NULL;

    if ( pcfSeekToType(file,toc,PCF_GLYPH_NAMES) &&
	    ((format = getint32(file))&PCF_FORMAT_MASK)==PCF_DEFAULT_FORMAT &&
	    (cnt = getformint32(file,format))==b->charcnt ) {
	offsets = galloc(cnt*sizeof(int));
	for ( i=0; i<cnt; ++i )
	    offsets[i] = getformint32(file,format);
	stringsize = getformint32(file,format);
	string = galloc(stringsize);
	fread(string,1,stringsize,file);
    }
    if ( pcfSeekToType(file,toc,PCF_BDF_ENCODINGS) &&
	    ((format = getint32(file))&PCF_FORMAT_MASK)==PCF_DEFAULT_FORMAT ) {
	int min2, max2, min1, max1, tot, def, glyph;
	min2 = getformint16(file,format);
	max2 = getformint16(file,format);
	min1 = getformint16(file,format);
	max1 = getformint16(file,format);
	def = getformint16(file,format);
	tot = (max2-min2+1)*(max1-min1+1);
	for ( i=0; i<b->charcnt; ++i )
	    b->chars[i]->enc = -1;
	for ( i=0; i<tot; ++i ) {
	    glyph = getformint16(file,format);
	    if ( glyph!=0xffff && glyph<b->charcnt ) {
		b->chars[glyph]->enc = (i%(max2-min2+1)) + min2 +
					(i/(max2-min2+1) + min1)*(max2-min2+1);
	    }
	}
	if ( def<b->charcnt && def!=0xffff )
	    b->chars[def]->enc = 0;		/* my standard location for .notdef */
    }
    for ( i=0; i<b->charcnt; ++i ) {
	char *name;
	if ( string!=NULL ) name = string+offsets[i];
	else name = ".notdef";
	b->chars[i]->enc = figureProperEncoding(sf,b,b->chars[i]->enc,name,-1,-1);
	if ( b->chars[i]->enc!=-1 )
	    b->chars[i]->sc = sf->chars[b->chars[i]->enc];
    }
}

static int PcfReadSWidths(FILE *file,struct toc *toc,BDFFont *b) {
    int format, cnt, i;

    if ( !pcfSeekToType(file,toc,PCF_SWIDTHS))
return(false);
    format = getint32(file);
    if ( (format&PCF_FORMAT_MASK)!=PCF_DEFAULT_FORMAT )
return(false);

    cnt = getformint32(file,format);
    if ( cnt!=b->charcnt )
return( false );
    for ( i=0; i<cnt; ++i ) {
	int swidth = getformint32(file,format);
	if ( b->chars[i]->sc!=NULL )
	    b->chars[i]->sc->width = swidth;
    }
return( true );
}

static int PcfParse(FILE *file,struct toc *toc,SplineFont *sf,BDFFont *b) {
    int metrics_cnt;
    struct pcfmetrics *metrics = pcfGetMetricsTable(file,toc,PCF_METRICS,&metrics_cnt);
    int mcnt = metrics_cnt;
    BDFChar **new;
    int i;

    if ( metrics==NULL )
return( false );
    b->charcnt = mcnt;
    b->chars = gcalloc(mcnt,sizeof(BDFChar *));
    for ( i=0; i<mcnt; ++i ) {
	b->chars[i] = chunkalloc(sizeof(BDFChar));
	b->chars[i]->xmin = metrics[i].lsb;
	b->chars[i]->xmax = metrics[i].rsb;
	b->chars[i]->ymin = -metrics[i].descent;
	b->chars[i]->ymax = metrics[i].ascent-1;
	b->chars[i]->width = metrics[i].width;
	b->chars[i]->bytes_per_line = ((b->chars[i]->xmax-b->chars[i]->xmin)>>3) + 1;
	b->chars[i]->bitmap = galloc(b->chars[i]->bytes_per_line*(b->chars[i]->ymax-b->chars[i]->ymin+1));
	b->chars[i]->enc = i;
    }
    free(metrics);

    if ( !PcfReadBitmaps(file,toc,b))
return( false );
    PcfReadEncodingsNames(file,toc,sf,b);
    if ( sf->onlybitmaps )
	PcfReadSWidths(file,toc,b);
    new = gcalloc(sf->charcnt,sizeof(BDFChar *));
    for ( i=0; i<b->charcnt; ++i ) {
	BDFChar *bc = b->chars[i];
	if ( bc->enc==-1 || bc->enc>=sf->charcnt )
	    BDFCharFree(bc);
	else
	    new[bc->enc] = bc;
    }
    free( b->chars );
    b->chars = new;
    b->charcnt = sf->charcnt;
return( true );
}

/* ************************* End Bitmap Formats ***************************** */

static int askusersize(char *filename) {
    char *pt;
    int guess;
    unichar_t *ret, *end;
    char def[10];
    unichar_t udef[10];

    for ( pt=filename; *pt && !isdigit(*pt); ++pt );
    guess = strtol(pt,NULL,10);
    if ( guess!=0 )
	sprintf(def,"%d",guess);
    else
	*def = '\0';
    uc_strcpy(udef,def);
  retry:
    ret = GWidgetAskStringR(_STR_PixelSize,udef,_STR_PixelSizeFont);
    if ( ret==NULL )
	guess = -1;
    else {
	guess = u_strtol(ret,&end,10);
	free(ret);
	if ( guess<=0 || *end!='\0' ) {
	    GWidgetErrorR(_STR_BadNumber,_STR_BadNumber);
  goto retry;
	}
    }
return( guess );
}

static int alreadyexists(int pixelsize) {
    char buffer[10];
    unichar_t ubuf[200];
    const unichar_t *buts[3]; unichar_t oc[2];
    int ret;

    buts[2]=NULL;
    buts[0] = GStringGetResource( _STR_OK, &oc[0]);
    buts[1] = GStringGetResource( _STR_Cancel, &oc[1]);

    sprintf(buffer,"%d",pixelsize);
    u_strcpy(ubuf, GStringGetResource(_STR_Duppixelsizepre,NULL));
    uc_strcat(ubuf,buffer);
    u_strcat(ubuf, GStringGetResource(_STR_Duppixelsizepost,NULL));

    ret = GWidgetAsk(GStringGetResource(_STR_Duppixelsize,NULL),buts,oc,0,1,ubuf)==0;

return( ret );
}

BDFFont *SFImportBDF(SplineFont *sf, char *filename,int ispk, int toback) {
    FILE *bdf;
    char tok[100];
    int pixelsize, ascent, descent, enc;
    BDFFont *b;
    char family[100], mods[200], full[300], foundry[100], comments[1000], fontname[300];
    struct toc *toc=NULL;
    int depth=1;
    struct metrics defs;

    defs.swidth = defs.swidth1 = -1; defs.dwidth=defs.dwidth1=0;
    defs.metricsset = 0; defs.vertical_origin = 0;
    defs.res = -1;
    foundry[0] = '\0';
    fontname[0] = '\0';

    if ( ispk==1 && strcmp(filename+strlen(filename)-2,"gf")==0 )
	ispk = 3;
    bdf = fopen(filename,"r");
    if ( bdf==NULL ) {
	GWidgetErrorR(_STR_CouldNotOpenFile, _STR_CouldNotOpenFileName, filename );
return( NULL );
    }
    if ( ispk==1 ) {
	pixelsize = pk_header(bdf,&ascent,&descent,&enc,family,mods,full, filename);
	if ( pixelsize==-2 ) {
	    fclose(bdf);
	    GWidgetErrorR(_STR_NotPkFile, _STR_NotPkFileName, filename );
return( NULL );
	}
    } else if ( ispk==3 ) {		/* gf */
	pixelsize = gf_postamble(bdf,&ascent,&descent,&enc,family,mods,full, filename);
	if ( pixelsize==-2 ) {
	    fclose(bdf);
	    GWidgetErrorR(_STR_NotGfFile, _STR_NotGfFileName, filename );
return( NULL );
	}
    } else if ( ispk==2 ) {		/* pcf */
	if (( toc = pcfReadTOC(bdf))== NULL ) {
	    fclose(bdf);
	    GWidgetErrorR(_STR_NotPcfFile, _STR_NotPcfFileName, filename );
return( NULL );
	}
	pixelsize = pcf_properties(bdf,toc,&ascent,&descent,&enc,family,mods,full);
	if ( pixelsize==-2 ) {
	    fclose(bdf); free(toc);
	    GWidgetErrorR(_STR_NotPcfFile, _STR_NotPcfFileName, filename );
return( NULL );
	}
    } else {
	if ( gettoken(bdf,tok,sizeof(tok))==-1 || strcmp(tok,"STARTFONT")!=0 ) {
	    fclose(bdf);
	    GWidgetErrorR(_STR_NotBdfFile, _STR_NotBdfFileName, filename );
return( NULL );
	}
	pixelsize = slurp_header(bdf,&ascent,&descent,&enc,family,mods,full,
		&depth,foundry,fontname,comments,&defs);
    }
    if ( pixelsize==-1 )
	pixelsize = askusersize(filename);
    if ( pixelsize==-1 ) {
	fclose(bdf); free(toc);
return( NULL );
    }
    if ( !toback && sf->bitmaps==NULL && sf->onlybitmaps ) {
	/* Loading first bitmap into onlybitmap font sets the name and encoding */
	SFSetFontName(sf,family,mods,full);
	if ( fontname[0]!='\0' ) {
	    free(sf->fontname);
	    sf->fontname = copy(fontname);
	}
	SFReencodeFont(sf,enc);
	if ( defs.metricsset!=0 ) {
	    sf->hasvmetrics = true;
	    sf->vertical_origin = defs.vertical_origin==0?sf->ascent:defs.vertical_origin;
	}
	sf->display_size = pixelsize;
	if ( comments[0]!='\0' )
	    sf->copyright = copy(comments);
    }

    b = NULL;
    if ( !toback )
	for ( b=sf->bitmaps; b!=NULL && (b->pixelsize!=pixelsize || BDFDepth(b)!=depth); b=b->next );
    if ( b!=NULL ) {
	if ( !alreadyexists(pixelsize)) {
	    fclose(bdf); free(toc);
return( NULL );
	}
    }
    if ( b==NULL ) {
	if ( ascent==-1 && descent==-1 )
	    ascent = rint(pixelsize*sf->ascent/(real) (sf->ascent+sf->descent));
	if ( ascent==-1 && descent!=-1 )
	    ascent = pixelsize - descent;
	else if ( ascent!=-1 )
	    descent = pixelsize -ascent;
	b = gcalloc(1,sizeof(BDFFont));
	b->sf = sf;
	b->charcnt = sf->charcnt;
	b->pixelsize = pixelsize;
	b->chars = gcalloc(sf->charcnt,sizeof(BDFChar *));
	b->ascent = ascent;
	b->descent = pixelsize-b->ascent;
	b->encoding_name = sf->encoding_name;
	b->res = defs.res;
	if ( depth!=1 )
	    BDFClut(b,(1<<(depth/2)));
	if ( !toback ) {
	    b->next = sf->bitmaps;
	    sf->bitmaps = b;
	    SFOrderBitmapList(sf);
	}
    }
    free(b->foundry);
    b->foundry = ( foundry[0]=='\0' ) ? NULL : copy(foundry);
    if ( ispk==1 ) {
	while ( pk_char(bdf,sf,b));
    } else if ( ispk==3 ) {
	while ( gf_char(bdf,sf,b));
    } else if ( ispk==2 ) {
	if ( !PcfParse(bdf,toc,sf,b) ) {
	    GWidgetErrorR(_STR_NotPcfFile, _STR_NotPcfFileName, filename );
	}
    } else {
	while ( gettoken(bdf,tok,sizeof(tok))!=-1 ) {
	    if ( strcmp(tok,"STARTCHAR")==0 ) {
		AddBDFChar(bdf,sf,b,depth,&defs);
		GProgressNext();
	    }
	}
    }
    fclose(bdf); free(toc);
    sf->changed = true;
return( b );
}

static BDFFont *_SFImportBDF(SplineFont *sf, char *filename,int ispk, int toback) {
    static struct { char *ext, *decomp, *recomp; } compressors[] = {
	{ "gz", "gunzip", "gzip" },
	{ "bz2", "bunzip2", "bzip2" },
	{ "Z", "gunzip", "compress" },
	NULL
    };
    int i;
    char *pt, *temp=NULL;
    char buf[1500];
    BDFFont *ret;

    pt = strrchr(filename,'.');
    i = -1;
    if ( pt!=NULL ) for ( i=0; compressors[i].ext!=NULL; ++i )
	if ( strcmp(compressors[i].ext,pt+1)==0 )
    break;
    if ( i==-1 || compressors[i].ext==NULL ) i=-1;
    else {
	sprintf( buf, "%s %s", compressors[i].decomp, filename );
	if ( system(buf)==0 )
	    *pt='\0';
	else {
	    /* Assume no write access to file */
	    char *dir = getenv("TMPDIR");
	    if ( dir==NULL ) dir = P_tmpdir;
	    temp = galloc(strlen(dir)+strlen(GFileNameTail(filename))+2);
	    strcpy(temp,dir);
	    strcat(temp,"/");
	    strcat(temp,GFileNameTail(filename));
	    *strrchr(temp,'.') = '\0';
	    sprintf( buf, "%s -c %s > %s", compressors[i].decomp, filename, temp );
	    if ( system(buf)==0 )
		filename = temp;
	    else {
		free(temp);
		GDrawError("Decompress failed" );
return( NULL );
	    }
	}
    }
    ret = SFImportBDF(sf, filename,ispk, toback);
    if ( temp!=NULL ) {
	unlink(temp);
	free(temp);
    } else if ( i!=-1 ) {
	sprintf( buf, "%s %s", compressors[i].recomp, filename );
	system(buf);
    }
return( ret );
}

static void SFSetupBitmap(SplineFont *sf,BDFFont *strike) {
    int i;

    strike->sf = sf;
    if ( strike->charcnt>sf->charcnt )
	ExtendSF(sf,strike->charcnt,true);
    for ( i=0; i<strike->charcnt; ++i ) if ( strike->chars[i]!=NULL ) {
	if ( sf->chars[i]==NULL )
	    MakeEncChar(sf,i,strike->chars[i]->sc->name);
	strike->chars[i]->sc = sf->chars[i];
    }
}

static void SFMergeBitmaps(SplineFont *sf,BDFFont *strikes) {
    BDFFont *b, *prev, *snext;

    while ( strikes ) {
	snext = strikes->next;
	strikes->next = NULL;
	for ( prev=NULL,b=sf->bitmaps; b!=NULL &&
		(b->pixelsize!=strikes->pixelsize || BDFDepth(b)!=BDFDepth(strikes));
		b=b->next );
	if ( b==NULL ) {
	    strikes->next = sf->bitmaps;
	    sf->bitmaps = strikes;
	    SFSetupBitmap(sf,strikes);
	} else if ( !alreadyexists(strikes->pixelsize)) {
	    BDFFontFree(strikes);
	} else {
	    strikes->next = b->next;
	    if ( prev==NULL )
		sf->bitmaps = strikes;
	    else
		prev->next = strikes;
	    BDFFontFree(b);
	    SFSetupBitmap(sf,strikes);
	}
	strikes = snext;
    }
    SFOrderBitmapList(sf);
}

static void SFAddToBackground(SplineFont *sf,BDFFont *bdf);

int FVImportBDF(FontView *fv, char *filename, int ispk, int toback) {
    BDFFont *b, *anyb=NULL;
    unichar_t ubuf[140];
    char *eod, *fpt, *file, *full;
    int fcnt, any = 0;
    int oldcharcnt = fv->sf->charcnt;

    eod = strrchr(filename,'/');
    *eod = '\0';
    fcnt = 1;
    fpt = eod+1;
    while (( fpt=strstr(fpt,"; "))!=NULL )
	{ ++fcnt; fpt += 2; }

    u_sprintf(ubuf, GStringGetResource(_STR_LoadingFrom,NULL), filename);
    GProgressStartIndicator(10,GStringGetResource(_STR_Loading,NULL),ubuf,GStringGetResource(_STR_ReadingGlyphs,NULL),0,fcnt);
    GProgressEnableStop(false);

    file = eod+1;
    do {
	fpt = strstr(file,"; ");
	if ( fpt!=NULL ) *fpt = '\0';
	full = galloc(strlen(filename)+1+strlen(file)+1);
	strcpy(full,filename); strcat(full,"/"); strcat(full,file);
	u_sprintf(ubuf, GStringGetResource(_STR_LoadingFrom,NULL), filename);
	GProgressChangeLine1(ubuf);
	b = _SFImportBDF(fv->sf,full,ispk,toback);
	free(full);
	if ( fpt!=NULL ) GProgressNextStage();
	if ( b!=NULL ) {
	    anyb = b;
	    any = true;
	    if ( b==fv->show && fv->v!=NULL )
		GDrawRequestExpose(fv->v,NULL,false);
	}
	file = fpt+2;
    } while ( fpt!=NULL );
    GProgressEndIndicator();
    if ( oldcharcnt != fv->sf->charcnt ) {
	FontView *fvs;
	for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	    free(fvs->selected);
	    fvs->selected = gcalloc(fvs->sf->charcnt,sizeof(char));
	}
	FontViewReformatAll(fv->sf);
    }
    if ( anyb==NULL ) {
	GWidgetErrorR( _STR_NoBitmapFont, _STR_NoBitmapFontIn, filename );
    } else if ( toback )
	SFAddToBackground(fv->sf,anyb);
return( any );
}

static void SFAddToBackground(SplineFont *sf,BDFFont *bdf) {
    struct _GImage *base;
    GClut *clut;
    GImage *img;
    int i;
    SplineChar *sc; BDFChar *bdfc;
    real scale;

    for ( i=0; i<sf->charcnt && i<bdf->charcnt; ++i ) {
	if ( bdf->chars[i]!=NULL ) {
	    if ( sf->chars[i]==NULL )
		SFMakeChar(sf,i);
	    sc = sf->chars[i];
	    bdfc = bdf->chars[i];

	    base = gcalloc(1,sizeof(struct _GImage));
	    base->image_type = it_mono;
	    base->data = bdfc->bitmap;
	    base->bytes_per_line = bdfc->bytes_per_line;
	    base->width = bdfc->xmax-bdfc->xmin+1;
	    base->height = bdfc->ymax-bdfc->ymin+1;
	    bdfc->bitmap = NULL;

	    clut = gcalloc(1,sizeof(GClut));
	    clut->clut_len = 2;
	    clut->clut[0] = screen_display==NULL? 0xffffff : GDrawGetDefaultBackground(NULL);
	    clut->clut[1] = 0x808080;
	    clut->trans_index = 0;
	    base->trans = 0;
	    base->clut = clut;

	    img = gcalloc(1,sizeof(GImage));
	    img->u.image = base;

	    scale = (sf->ascent+sf->descent)/(double) (bdf->ascent+bdf->descent);
	    SCInsertBackImage(sc,img,scale,(bdfc->ymax+1)*scale,bdfc->xmin*scale);
	}
    }
    BDFFontFree(bdf);
}

int FVImportMult(FontView *fv, char *filename, int toback, int bf) {
    SplineFont *strikeholder, *sf = fv->sf;
    BDFFont *strikes;
    unichar_t ubuf[100];

    u_snprintf(ubuf, sizeof(ubuf)/sizeof(ubuf[0]), GStringGetResource(_STR_LoadingFrom,NULL), filename);
    GProgressStartIndicator(10,GStringGetResource(_STR_Loading,NULL),ubuf,GStringGetResource(_STR_ReadingGlyphs,NULL),0,2);
    GProgressEnableStop(false);

    if ( bf == bf_ttf )
	strikeholder = SFReadTTF(filename,toback?ttf_onlyonestrike|ttf_onlystrikes:ttf_onlystrikes);
    else if ( bf == bf_fon )
	strikeholder = SFReadWinFON(filename,toback);
    else
	strikeholder = SFReadMacBinary(filename,toback?ttf_onlyonestrike|ttf_onlystrikes:ttf_onlystrikes);

    if ( strikeholder==NULL || (strikes = strikeholder->bitmaps)==NULL ) {
	SplineFontFree(strikeholder);
	GProgressEndIndicator();
return( false );
    }
    SFMatchEncoding(strikeholder,sf);
    if ( toback )
	SFAddToBackground(sf,strikes);
    else
	SFMergeBitmaps(sf,strikes);

    strikeholder->bitmaps =NULL;
    SplineFontFree(strikeholder);
    GProgressEndIndicator();
return( true );
}

SplineFont *SFFromBDF(char *filename,int ispk,int toback) {
    SplineFont *sf = SplineFontNew();
    BDFFont *bdf = SFImportBDF(sf,filename,ispk,toback);

    if ( toback )
	SFAddToBackground(sf,bdf);
    else
	sf->changed = false;
return( sf );
}
