/* Copyright (C) 2001 by George Williams */
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
#include <math.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <ustring.h>
#include "utype.h"
#include <sys/types.h>
#include <sys/wait.h>

enum { pt_lp, pt_lpr, pt_ghostview, pt_file, pt_unknown=-1 };
int pagewidth = 595, pageheight=792; 	/* Minimum size for US Letter, A4 paper, should work for either */
static char *lastprinter=NULL;
int printtype = pt_unknown;

typedef struct printinfo {
    FontView *fv;
    MetricsView *mv;
    SplineFont *sf;
    SplineChar *sc;
    enum printtype { pt_fontdisplay, pt_chars, pt_fontsample } pt;
    int pointsize;
    int extrahspace, extravspace;
    FILE *out;
    FILE *fontfile;
    char psfontname[300];
    unsigned int showvm: 1;
    unsigned int twobyte: 1;
    unsigned int iscjk: 1;
    unsigned int overflow: 1;
    unsigned int done: 1;
    int ypos;
    int max;		/* max chars per line */
    int chline;		/* High order bits of characters we're outputting */
    int page;
    int lastbase;
    double xoff, yoff, scale;
    GWindow gw;
    GWindow setup;
    char *printer;
    int copies;
    int pagewidth, pageheight, printtype;
} PI;

static struct printdefaults {
    enum charset last_cs;
    enum printtype pt;
    int pointsize;
    unichar_t *text;
} pdefs[] = { { em_none, pt_fontdisplay }, { em_none, pt_chars }, { em_none, pt_fontsample }};
/* defaults for print from fontview, charview, metricsview */

static void dump_prologue(PI *pi) {
    time_t now;
    struct passwd *pwd;
    char *pt;
    int ch, i, j, base;

    fprintf( pi->out, "%%!PS-Adobe-3.0\n" );
    fprintf( pi->out, "%%%%BoundingBox: 40 20 %d %d\n", pi->pagewidth-30, pi->pageheight-20 );
    fprintf( pi->out, "%%%%Creator: PfaEdit\n" );
    time(&now);
    fprintf( pi->out, "%%%%CreationDate: %s", ctime(&now) );
    fprintf( pi->out, "%%%%DocumentData: Clean7Bit\n" );
/* Can all be commented out if no pwd routines */
    pwd = getpwuid(getuid());
    if ( pwd!=NULL && pwd->pw_gecos!=NULL && *pwd->pw_gecos!='\0' )
	fprintf( pi->out, "%%%%For: %s\n", pwd->pw_gecos );
    else if ( pwd!=NULL && pwd->pw_name!=NULL && *pwd->pw_name!='\0' )
	fprintf( pi->out, "%%%%For: %s\n", pwd->pw_name );
    else if ( (pt=getenv("USER"))!=NULL )
	fprintf( pi->out, "%%%%For: %s\n", pt );
    endpwent();
/* End pwd section */
    fprintf( pi->out, "%%%%LanguageLevel: 2\n" );
    fprintf( pi->out, "%%%%Orientation: Portrait\n" );
    fprintf( pi->out, "%%%%Pages: atend\n" );
    if ( pi->pt==pt_fontdisplay ) {
	fprintf( pi->out, "%%%%Title: Font Display for %s\n", pi->sf->fullname );
	fprintf( pi->out, "%%%%DocumentSuppliedResources: font %s\n", pi->sf->fontname );
    } else if ( pi->pt==pt_fontdisplay ) {
	fprintf( pi->out, "%%%%Title: Text Sample of %s\n", pi->sf->fullname );
	fprintf( pi->out, "%%%%DocumentSuppliedResources: font %s\n", pi->sf->fontname );
    } else if ( pi->sc!=NULL )
	fprintf( pi->out, "%%%%Title: Character Display for %s from %s\n", pi->sc->name, pi->sf->fullname );
    else
	fprintf( pi->out, "%%%%Title: Character Displays from %s\n", pi->sf->fullname );
    fprintf( pi->out, "%%%%DocumentNeededResources: font Times-Bold\n" );
    fprintf( pi->out, "%%%%EndComments\n\n" );

    fprintf( pi->out, "%%%%BeginSetup\n" );
    fprintf( pi->out, "<< /PageSize [%d %d] >> setpagedevice\n", pi->pagewidth, pi->pageheight );

    fprintf( pi->out, "%% <font> <encoding> font_remap <font>	; from the cookbook\n" );
    fprintf( pi->out, "/reencodedict 5 dict def\n" );
    fprintf( pi->out, "/font_remap { reencodedict begin\n" );
    fprintf( pi->out, "  /newencoding exch def\n" );
    fprintf( pi->out, "  /basefont exch def\n" );
    fprintf( pi->out, "  /newfont basefont  maxlength dict def\n" );
    fprintf( pi->out, "  basefont {\n" );
    fprintf( pi->out, "    exch dup dup /FID ne exch /Encoding ne and\n" );
    fprintf( pi->out, "	{ exch newfont 3 1 roll put }\n" );
    fprintf( pi->out, "	{ pop pop }\n" );
    fprintf( pi->out, "    ifelse\n" );
    fprintf( pi->out, "  } forall\n" );
    fprintf( pi->out, "  newfont /Encoding newencoding put\n" );
    fprintf( pi->out, "  /foo newfont definefont	%%Leave on stack\n" );
    fprintf( pi->out, "  end } def\n" );
    fprintf( pi->out, "/n_show { moveto show } bind def\n" );

    fprintf( pi->out, "%%%%IncludeResource: font Times-Bold\n" );
    fprintf( pi->out, "/MyFontDict 100 dict def\n" );
    fprintf( pi->out, "/Times-Bold-ISO-8859-1 /Times-Bold findfont ISOLatin1Encoding font_remap definefont\n" );
    fprintf( pi->out, "MyFontDict /Times-Bold__12 /Times-Bold-ISO-8859-1 findfont 12 scalefont put\n" );

    if ( pi->fontfile!=NULL ) {
	fprintf( pi->out, "%%%%BeginResource: font %s\n", pi->sf->fontname );
	if ( pi->showvm )
	    fprintf( pi->out, " vmstatus pop /VM1 exch def pop\n" );
	while ( (ch=getc(pi->fontfile))!=EOF )
	    putc(ch,pi->out);
	fprintf( pi->out, "%%%%EndResource\n" );
	sprintf(pi->psfontname,"%s__%d", pi->sf->fontname, pi->pointsize );
	fprintf(pi->out,"MyFontDict /%s /%s findfont %d scalefont put\n", pi->psfontname, pi->sf->fontname, pi->pointsize );
	if ( pi->showvm )
	    fprintf( pi->out, "vmstatus pop dup VM1 sub (Max VMusage: ) print == flush\n" );

	/* Now see if there are any unencoded characters in the font, and if so */
	/*  reencode enough fonts to display them all. These will all be 256 char fonts */
	if ( pi->iscjk )
	    i = 96*94;
	else if ( pi->twobyte )
	    i = 65536;
	else
	    i = 256;
	for ( ; i<pi->sf->charcnt; ++i ) {
	    if ( SCWorthOutputting(pi->sf->chars[i]) ) {
		base = i&~0xff;
		fprintf( pi->out, "MyFontDict /%s-%x__%d /%s-%x /%s%s findfont [\n",
			pi->sf->fontname, base>>8, pi->pointsize,
			pi->sf->fontname, base>>8,
			pi->sf->fontname, pi->twobyte?"Base":"" );
		for ( j=0; j<0x100 && base+j<pi->sf->charcnt; ++j )
		    if ( SCWorthOutputting(pi->sf->chars[base+j]))
			fprintf( pi->out, "\t/%s\n", pi->sf->chars[base+j]->name );
		    else
			fprintf( pi->out, "\t/.notdef\n" );
		for ( ;j<0x100; ++j )
		    fprintf( pi->out, "\t/.notdef\n" );
		fprintf( pi->out, " ] font_remap definefont %d scalefont put\n",
			pi->pointsize );
		i = base+0xff;
	    }
	}
    }

    fprintf( pi->out, "%%%%EndSetup\n\n" );
}

static int PIDownloadFont(PI *pi) {
    static unichar_t print[] = { 'P','r','i','n','t','i','n','g',' ','f','o','n','t',  '\0' };
    static unichar_t saveps[] = { 'G','e','n','e','r','a','t','i','n','g',' ','P','o','s','t','s','c','r','i','p','t',' ','F','o','n','t', '\0' };

    pi->fontfile = tmpfile();
    if ( pi->fontfile==NULL ) {
	GDrawError("Failed to open temporary output file" );
return(false);
    }
    GProgressStartIndicator(10,print,print, saveps,pi->sf->charcnt,1);
    GProgressEnableStop(false);
    if ( !_WritePSFont(pi->fontfile,pi->sf,pi->twobyte?ff_ptype0:ff_pfa)) {
	GProgressEndIndicator();
	GDrawError("Failed to generate postscript font" );
	fclose(pi->fontfile);
return(false);
    }
    GProgressEndIndicator();
    rewind(pi->fontfile);
    dump_prologue(pi);
    fclose(pi->fontfile); pi->fontfile = NULL;
return( true );
}

static void endpage(PI *pi ) {
    fprintf(pi->out,"showpage cleartomark restore\t\t%%End of Page\n" );
}

static void dump_trailer(PI *pi) {
    if ( pi->page!=0 )
	endpage(pi);
    fprintf( pi->out, "%%%%Trailer\n" );
    fprintf( pi->out, "%%%%Pages: %d\n", pi->page );
    fprintf( pi->out, "%%%%EOF\n" );
}

/* ************************************************************************** */
/* ************************* Code for full font dump ************************ */
/* ************************************************************************** */

static void startpage(PI *pi ) {
    int i;
    /* I used to have a progress indicator here showing pages. But they went */
    /*  by so fast that even for CaslonRoman it was pointless */

    if ( pi->page!=0 )
	endpage(pi);
    ++pi->page;
    fprintf(pi->out,"%%%%Page: %d %d\n", pi->page, pi->page );
    fprintf(pi->out,"%%%%PageResources: font Times-Bold font %s\n", pi->sf->fontname );
    fprintf(pi->out,"save mark\n" );
    fprintf(pi->out,"40 %d translate\n", pi->pageheight-54 );
    fprintf(pi->out,"MyFontDict /Times-Bold__12 get setfont\n" );
    fprintf(pi->out,"(Font Display for %s) 193.2 -10.92 n_show\n", pi->sf->fontname);

    if ( pi->iscjk )
	for ( i=0; i<pi->max; ++i )
	    fprintf(pi->out,"(%d) %d -54.84 n_show\n", i, 60+(pi->pointsize+pi->extrahspace)*i );
    else
	for ( i=0; i<pi->max; ++i )
	    fprintf(pi->out,"(%X) %d -54.84 n_show\n", i, 60+(pi->pointsize+pi->extrahspace)*i );
    pi->ypos = -76;
}

static int DumpLine(PI *pi) {
    int i, line;

    /* First find the next line with stuff on it */
    for ( line = pi->chline ; line<pi->sf->charcnt; line += pi->max ) {
	for ( i=0; i<pi->max && line+i<pi->sf->charcnt; ++i )
	    if ( SCWorthOutputting(pi->sf->chars[line+i]) )
	break;
	if ( i!=pi->max )
    break;
    }
    if ( line+i>=pi->sf->charcnt )		/* Nothing more */
return(0);

    if ( (pi->twobyte && line>=65536) || (pi->iscjk && line>=96*94) || ( !pi->twobyte && line>=256 ) ) {
	/* Nothing more encoded. Can't use the normal font, must use one of */
	/*  the funky reencodings we built up at the beginning */
	if ( pi->lastbase!=(line>>8) ) {
	    if ( !pi->overflow ) {
		fprintf( pi->out, "%d %d moveto %d %d rlineto stroke\n",
		    100, pi->ypos+8*pi->pointsize/10-1, 300, 0 );
		pi->ypos -= 4;
	    }
	    pi->overflow = true;
	    pi->lastbase = (line>>8);
	    sprintf(pi->psfontname,"%s-%x__%d", pi->sf->fontname, pi->lastbase,
		    pi->pointsize );
	}
    }

    if ( pi->chline==0 ) {
	/* start the first page */
	startpage(pi);
    } else {
	/* start subsequent pages by displaying the one before */
	if ( pi->ypos - pi->pointsize < -(pi->pageheight-90) ) {
	    startpage(pi);
	}
    }
    pi->chline = line;

    if ( !pi->overflow ) {
	fprintf(pi->out,"MyFontDict /Times-Bold__12 get setfont\n" );
	if ( pi->iscjk )
	    fprintf(pi->out,"(%d,%d) 26.88 %d n_show\n",
		    pi->chline/96 + 1, pi->chline%96, pi->ypos );
	else
	    fprintf(pi->out,"(%04X) 26.88 %d n_show\n", pi->chline, pi->ypos );
    }
    fprintf(pi->out,"MyFontDict /%s get setfont\n", pi->psfontname );
    for ( i=0; i<pi->max ; ++i ) {
	if ( i+pi->chline<pi->sf->charcnt && SCWorthOutputting(pi->sf->chars[i+pi->chline])) {
	    if ( pi->overflow ) {
		fprintf( pi->out, "<%02x> %d %d n_show\n", pi->chline +i-(pi->lastbase<<8),
			56+26*i, pi->ypos );
	    } else if ( pi->iscjk ) {
		fprintf( pi->out, "<%02x%02X> %d %d n_show\n",
			(pi->chline+i)/96 + '!', (pi->chline+i)%96 + ' ',
			56+(pi->extrahspace+pi->pointsize)*i, pi->ypos );
	    } else if ( pi->twobyte ) {
		fprintf( pi->out, "<%04x> %d %d n_show\n", pi->chline +i,
			56+(pi->extrahspace+pi->pointsize)*i, pi->ypos );
	    } else {
		fprintf( pi->out, "<%02x> %d %d n_show\n", pi->chline +i,
			56+26*i, pi->ypos );
	    }
	}
    }
    pi->ypos -= pi->pointsize+pi->extravspace;
    pi->chline += pi->max;
return(true);
}

static void PIFontDisplay(PI *pi) {

    pi->extravspace = pi->pointsize/6;
    pi->extrahspace = pi->pointsize/3;
    pi->max = (pi->pagewidth-100)/(pi->extrahspace+pi->pointsize);
    if ( pi->max>=16 ) pi->max = 16;
    else if ( pi->max>=8 ) pi->max = 8;
    else pi->max = 4;
    if ( !PIDownloadFont(pi))
return;

    while ( DumpLine(pi))
	;

    if ( pi->chline==0 )
	GDrawError( "Warning: Font contained no characters" );
    else
	dump_trailer(pi);
}

/* ************************************************************************** */
/* ********************* Code for single character dump ********************* */
/* ************************************************************************** */

static void PIDumpSPL(PI *pi,SplinePointList *spl) {
    SplinePoint *first, *sp;

    for ( ; spl!=NULL; spl=spl->next ) {
	first = NULL;
	for ( sp = spl->first; ; sp=sp->next->to ) {
	    if ( first==NULL )
		fprintf( pi->out, "%g %g moveto\n", sp->me.x*pi->scale+pi->xoff, sp->me.y*pi->scale+pi->yoff );
	    else if ( sp->prev->islinear )
		fprintf( pi->out, " %g %g lineto\n", sp->me.x*pi->scale+pi->xoff, sp->me.y*pi->scale+pi->yoff );
	    else
		fprintf( pi->out, " %g %g %g %g %g %g curveto\n",
			sp->prev->from->nextcp.x*pi->scale+pi->xoff, sp->prev->from->nextcp.y*pi->scale+pi->yoff,
			sp->prevcp.x*pi->scale+pi->xoff, sp->prevcp.y*pi->scale+pi->yoff,
			sp->me.x*pi->scale+pi->xoff, sp->me.y*pi->scale+pi->yoff );
	    if ( sp==first )
	break;
	    if ( first==NULL ) first = sp;
	    if ( sp->next==NULL )
	break;
	}
	fprintf( pi->out, "closepath\n" );
    }
    fprintf( pi->out, "fill\n" );
}

static void SCPrintPage(PI *pi,SplineChar *sc) {
    DBounds b, page;
    double scalex, scaley;
    RefChar *r;

    if ( pi->page!=0 )
	endpage(pi);
    ++pi->page;
    fprintf(pi->out,"%%%%Page: %d %d\n", pi->page, pi->page );
    fprintf(pi->out,"%%%%PageResources: font Times-Bold\n", pi->sf->fontname );
    fprintf(pi->out,"save mark\n" );

    SplineCharFindBounds(sc,&b);
    if ( b.maxy<sc->parent->ascent+5 ) b.maxy = sc->parent->ascent + 5;
    if ( b.miny>-sc->parent->descent ) b.miny =-sc->parent->descent - 5;
    if ( b.minx>00 ) b.minx = -5;
    if ( b.maxx<=0 ) b.maxx = 5;
    if ( b.maxx<=sc->width+5 ) b.maxx = sc->width+5;

    /* From the default bounding box */
    page.minx = 40; page.maxx = 580;
    page.miny = 20; page.maxy = 770;

    fprintf(pi->out,"MyFontDict /Times-Bold__12 get setfont\n" );
    fprintf(pi->out,"(%s from %s) 80 758 n_show\n", sc->name, sc->parent->fullname );
    page.maxy = 750;

    scalex = (page.maxx-page.minx)/(b.maxx-b.minx);
    scaley = (page.maxy-page.miny)/(b.maxy-b.miny);
    pi->scale = (scalex<scaley)?scalex:scaley;
    pi->xoff = page.minx - b.minx*pi->scale;
    pi->yoff = page.miny - b.miny*pi->scale;

    fprintf(pi->out,".2 setlinewidth\n" );
    fprintf(pi->out,"%g %g moveto %g %g lineto stroke\n", page.minx, pi->yoff, page.maxx, pi->yoff );
    fprintf(pi->out,"%g %g moveto %g %g lineto stroke\n", pi->xoff, page.miny, pi->xoff, page.maxy );
    fprintf(pi->out,"%g %g moveto %g %g lineto stroke\n", page.minx, sc->parent->ascent*pi->scale+pi->yoff, page.maxx, sc->parent->ascent*pi->scale+pi->yoff );
    fprintf(pi->out,"%g %g moveto %g %g lineto stroke\n", page.minx, -sc->parent->descent*pi->scale+pi->yoff, page.maxx, -sc->parent->descent*pi->scale+pi->yoff );
    fprintf(pi->out,"%g %g moveto %g %g lineto stroke\n", pi->xoff+sc->width*pi->scale, page.miny, pi->xoff+sc->width*pi->scale, page.maxy );

    PIDumpSPL(pi,sc->splines);
    for ( r=sc->refs; r!=NULL; r=r->next )
	PIDumpSPL(pi,r->splines);

}

static void PIChars(PI *pi) {
    int i;

    dump_prologue(pi);
    if ( pi->fv!=NULL ) {
	for ( i=0; i<pi->sf->charcnt; ++i )
	    if ( pi->fv->selected[i] && SCWorthOutputting(pi->sf->chars[i]) )
		SCPrintPage(pi,pi->sf->chars[i]);
    } else if ( pi->sc!=NULL )
	SCPrintPage(pi,pi->sc);
    else {
	for ( i=0; i<pi->mv->charcnt; ++i )
	    if ( SCWorthOutputting(pi->mv->perchar[i].sc))
		SCPrintPage(pi,pi->mv->perchar[i].sc);
    }
    dump_trailer(pi);
}

/* ************************************************************************** */
/* ************************** Code for sample text ************************** */
/* ************************************************************************** */

static void samplestartpage(PI *pi ) {

    if ( pi->page!=0 )
	endpage(pi);
    ++pi->page;
    fprintf(pi->out,"%%%%Page: %d %d\n", pi->page, pi->page );
    fprintf(pi->out,"%%%%PageResources: font %s\n", pi->sf->fontname );
    fprintf(pi->out,"save mark\n" );
    fprintf(pi->out,"MyFontDict /Times-Bold__12 get setfont\n" );
    fprintf(pi->out,"(Sample Text from %s) 80 758 n_show\n", pi->sf->fullname );
    fprintf(pi->out,"40 %d translate\n", pi->pageheight-54 );
    fprintf(pi->out,"MyFontDict /%s get setfont\n", pi->psfontname );

    pi->ypos = -30;
}

static SplineChar *findchar(PI *pi, int ch) {
    SplineFont *sf = pi->sf;
    int i, max;

    if ( ch<0 || ch>=65536 )
return(NULL);
    if ( sf->encoding_name==em_unicode ) {
	if ( SCWorthOutputting(sf->chars[ch]))
return( sf->chars[ch]);
    } else {
	max = 256;
	if ( pi->iscjk ) max = 96*94;
	for ( i=0 ; i<sf->charcnt && i<max; ++i )
	    if ( sf->chars[i]!=NULL && sf->chars[i]->unicodeenc==ch ) {
		if ( SCWorthOutputting(sf->chars[i]))
return( sf->chars[i]);
		else
return( NULL );
	    }
    }
return( NULL );
}

static void outputchar(PI *pi, SplineChar *sc) {

    if ( sc==NULL )
return;
    if ( pi->iscjk ) {
	fprintf( pi->out, "%02x%02X", (sc->enc)/96 + '!', (sc->enc)%96 + ' ' );
    } else if ( pi->twobyte ) {
	fprintf( pi->out, "%04X", sc->unicodeenc );
    } else {
	fprintf( pi->out, "%02X", sc->enc );
    }
}

static int AnyKerns(SplineChar *first, SplineChar *second) {
    KernPair *kp;

    if ( first==NULL || second==NULL )
return( 0 );
    for ( kp = first->kerns; kp!=NULL; kp=kp->next )
	if ( kp->sc==second )
return( kp->off );

return( 0 );
}

static unichar_t *PIFindEnd(PI *pi, unichar_t *pt, unichar_t *ept) {
    double len = 0, max = pi->pagewidth-100, chlen;
    SplineChar *sc, *nsc;
    unichar_t *npt;

    nsc = findchar(pi,*pt);
    while ( pt<ept ) {
	sc = nsc;
	nsc = NULL;
	for ( npt=pt+1; npt<ept && iscombining(*npt); ++npt );
	nsc = findchar(pi,*npt);
	if ( sc!=NULL ) {
	    chlen = sc->width*pi->scale;
	    chlen += AnyKerns(sc,nsc)*pi->scale;
	    if ( chlen+len>max )
return( pt );
	    len += chlen;
	}
	pt = npt;
    }
return( ept );
}

static int PIFindLen(PI *pi, unichar_t *pt, unichar_t *ept) {
    double len = 0, chlen;
    SplineChar *sc, *nsc;
    unichar_t *npt;

    nsc = findchar(pi,*pt);
    while ( pt<ept ) {
	sc = nsc;
	nsc = NULL;
	for ( npt=pt+1; npt<ept && iscombining(*npt); ++npt );
	nsc = findchar(pi,*npt);
	if ( sc!=NULL ) {
	    chlen = sc->width*pi->scale;
	    chlen += AnyKerns(sc,nsc)*pi->scale;
	    len += chlen;
	}
	pt = npt;
    }
return( len );
}

/* Nothing fancy here. No special cases, no italic fixups */
static void PIDoCombiners(PI *pi, SplineChar *sc, unichar_t *accents) {
    DBounds bb, rbb;
    SplineChar *asc;
    double xmove=sc->width*pi->scale, ymove=0;
    double spacing = (pi->sf->ascent+pi->sf->descent)/25;
    double xoff, yoff;
    int first=true, pos;

    SplineCharFindBounds(sc,&bb);
    while ( iscombining(*accents)) {
	if ((asc=findchar(pi,*accents))!=NULL ) {
	    if ( first ) { fprintf( pi->out, "> show " ); first=false;}
	    SplineCharFindBounds(asc,&rbb);
	    pos = ____utype2[1+*accents];
	    if ( (pos&____ABOVE) && (pos&(____LEFT|____RIGHT)) )
		yoff = bb.maxy - rbb.maxy;
	    else if ( pos&____ABOVE )
		yoff = bb.maxy + spacing - rbb.miny;
	    else if ( pos&____BELOW ) {
		yoff = bb.miny - rbb.maxy;
		if ( !( pos&____TOUCHING) )
		    yoff -= spacing;
	    } else if ( pos&____OVERSTRIKE )
		yoff = bb.miny - rbb.miny + ((bb.maxy-bb.miny)-(rbb.maxy-rbb.miny))/2;
	    else /* If neither Above, Below, nor overstrike then should use the same baseline */
		yoff = bb.miny - rbb.miny;
	    if ( pos&____LEFT )
		xoff = bb.minx - spacing - rbb.maxx;
	    else if ( pos&____RIGHT ) {
		xoff = bb.maxx - rbb.minx;
		if ( !( pos&____TOUCHING) )
		    xoff += spacing;
	    } else {
		if ( pos&____CENTERLEFT )
		    xoff = bb.minx + (bb.maxx-bb.minx)/2 - rbb.maxx;
		else if ( pos&____LEFTEDGE )
		    xoff = bb.minx - rbb.minx;
		else if ( pos&____CENTERRIGHT )
		    xoff = bb.minx + (bb.maxx-bb.minx)/2 - rbb.minx;
		else if ( pos&____RIGHTEDGE )
		    xoff = bb.maxx - rbb.maxx;
		else
		    xoff = bb.minx - rbb.minx + ((bb.maxx-bb.minx)-(rbb.maxx-rbb.minx))/2;
	    }
	    fprintf( pi->out, "%g %g rmoveto <",
		    xoff*pi->scale-xmove, yoff*pi->scale-ymove);
	    outputchar(pi,asc);
	    fprintf( pi->out, "> show " );
	    xmove = (xoff+asc->width)*pi->scale;
	    ymove = yoff*pi->scale;
	    if ( bb.maxx<rbb.maxx+xoff ) bb.maxx = rbb.maxx+xoff;
	    if ( bb.minx>rbb.minx+xoff ) bb.minx = rbb.minx+xoff;
	    if ( bb.maxy<rbb.maxy+yoff ) bb.maxy = rbb.maxy+yoff;
	    if ( bb.miny>rbb.miny+yoff ) bb.miny = rbb.miny+yoff;
	}
	++accents;
    }
    if ( !first )
	fprintf( pi->out, "%g %g rmoveto <", sc->width*pi->scale-xmove, -ymove );
}

static void PIDumpChars(PI *pi, unichar_t *pt, unichar_t *ept, int xstart) {
    SplineChar *sc, *nsc;
    unichar_t *npt;
    int off;

    if ( pi->ypos - pi->pointsize < -(pi->pageheight-90) ) {
	samplestartpage(pi);
    }
    fprintf(pi->out,"%d %d moveto ", xstart, pi->ypos );	/* 20 for left->right */
    putc('<',pi->out);

    nsc = findchar(pi,*pt);
    while ( pt<ept ) {
	sc = nsc;
	nsc = NULL;
	for ( npt=pt+1; npt<ept && iscombining(*npt); ++npt );
	nsc = findchar(pi,*npt);
	if ( sc!=NULL ) {
	    outputchar(pi,sc);
	    if ( npt>pt+1 )
		PIDoCombiners(pi,sc,pt+1);
	    off = AnyKerns(sc,nsc);
	    if ( off!=0 )
		fprintf(pi->out,"> show %g 0 rmoveto <", off*pi->scale );
	}
	pt = npt;
    }
    fprintf(pi->out, "> show\n" );
    pi->ypos -= pi->pointsize+pi->extravspace;
}

static int SetupBiText(GBiText *bi,unichar_t *pt, unichar_t *ept,int bilen) {
    int cnt = ept-pt;

    if ( cnt>= bilen ) {
	bilen = cnt + 50;
	free(bi->text); free(bi->level);
	free(bi->override); free(bi->type);
	free(bi->original);
	++bilen;
	bi->text = galloc(bilen*sizeof(unichar_t));
	bi->level = galloc(bilen*sizeof(uint8));
	bi->override = galloc(bilen*sizeof(uint8));
	bi->type = galloc(bilen*sizeof(uint16));
	bi->original = galloc(bilen*sizeof(unichar_t *));
	--bilen;
    }
    bi->base_right_to_left = GDrawIsAllLeftToRight(pt,cnt)==-1;
    GDrawBiText1(bi,pt,cnt);
return( bilen );
}

/* We handle kerning, composits, and bidirectional text */
static void PIFontSample(PI *pi,unichar_t *sample) {
    unichar_t *pt, *base, *ept, *end, *temp;
    GBiText bi;
    int bilen;
    int xstart;

    memset(&bi,'\0',sizeof(bi)); bilen = 0;

    pi->extravspace = pi->pointsize/6;
    if ( !PIDownloadFont(pi))
return;
    pi->scale = pi->pointsize/(double) (pi->sf->ascent+pi->sf->descent);

    samplestartpage(pi);
    pt = sample;
    do {
	if ( ( ept = u_strchr(pt,'\n'))==NULL )
	    ept = pt+u_strlen(pt);
	bilen = SetupBiText(&bi,pt,ept,bilen);
	base = pt;
	while ( pt<=ept ) {
	    end = PIFindEnd(pi,pt,ept);
	    if ( end!=ept && !isbreakbetweenok(*end,end[1]) ) {
		for ( temp=end; temp>pt && !isbreakbetweenok(*temp,temp[1]); --temp );
		if ( temp==pt )
		    for ( temp=end; temp<ept && !isbreakbetweenok(*temp,temp[1]); ++temp );
		end = temp;
	    }
	    GDrawBiText2(&bi,pt-base,end-base);
	    if ( !bi.base_right_to_left )
		xstart = 20;
	    else
		xstart = 20+pi->pagewidth-100-PIFindLen(pi,pt,ept);
	    PIDumpChars(pi,bi.text+(pt-base),bi.text+(end-base),xstart);
	    if ( *end=='\0' )
   goto break_2_loops;
	    pt = end+1;
	}
    } while ( *ept!='\0' );
   break_2_loops:;
	
    dump_trailer(pi);
}

#if 0
static void PIFontSample(PI *pi,unichar_t *sample) {
    unichar_t *upt;

    for ( upt=sample; *upt; ++upt ) {
	if ( *upt=='\n' )
	    fprintf( pi->out,"  '\\0'}\n");
	else if ( *upt<256 && *upt!='\\' && *upt!='\'' )
	    fprintf( pi->out,"'%c',", *upt );
	else
	    fprintf( pi->out, "0x%04x,", *upt );
    }
}
#endif

/* ************************************************************************** */
/* *********************** Code for Page Setup dialog *********************** */
/* ************************************************************************** */
static void PIGetPrinterDefs(PI *pi) {
    pi->pagewidth = pagewidth;
    pi->pageheight = pageheight;
    pi->printtype = printtype;
    pi->printer = copy(lastprinter);
    pi->copies = 1;
}

#define CID_lp		1001
#define CID_lpr		1002
#define	CID_ghostview	1003
#define CID_File	1004
#define	CID_Pagesize	1005
#define CID_CopiesLab	1006
#define CID_Copies	1007
#define CID_PrinterLab	1008
#define CID_Printer	1009

static void PG_SetEnabled(PI *pi) {
    int enable;

    enable = GGadgetIsChecked(GWidgetGetControl(pi->setup,CID_lp)) ||
	    GGadgetIsChecked(GWidgetGetControl(pi->setup,CID_lpr));

    GGadgetSetEnabled(GWidgetGetControl(pi->setup,CID_CopiesLab),enable);
    GGadgetSetEnabled(GWidgetGetControl(pi->setup,CID_Copies),enable);
    GGadgetSetEnabled(GWidgetGetControl(pi->setup,CID_PrinterLab),enable);
    GGadgetSetEnabled(GWidgetGetControl(pi->setup,CID_Printer),enable);
}

static int PG_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	const unichar_t *ret;
	int err=false;
	int copies, pgwidth, pgheight;

	copies = GetInt(pi->setup,CID_Copies,"Copies",&err);
	if ( err )
return(true);

	ret = _GGadgetGetTitle(GWidgetGetControl(pi->setup,CID_Pagesize));
	if ( uc_strstr(ret,"Letter")!=NULL ) {
	    pgwidth = 612; pgheight = 792;
	} else if ( uc_strstr(ret,"Legal")!=NULL ) {
	    pgwidth = 612; pgheight = 1008;
	} else if ( uc_strstr(ret,"A4")!=NULL ) {
	    pgwidth = 595; pgheight = 842;
	} else if ( uc_strstr(ret,"A3")!=NULL ) {
	    pgwidth = 842; pgheight = 1191;
	} else if ( uc_strstr(ret,"B4")!=NULL ) {
	    pgwidth = 708; pgheight = 1000;
	} else if ( uc_strstr(ret,"B5")!=NULL ) {
	    pgwidth = 516; pgheight = 728;
	} else {
	    char *cret = cu_copy(ret), *pt;
	    double pw,ph, scale;
	    if ( sscanf(cret,"%gx%g",&pw,&ph)!=2 ) {
		GDrawIError("Bad Pagesize must be a known name or <num>x<num><units>\nWhere <units> is one of pt (points), mm, cm, in" );
return( true );
	    }
	    pt = cret+strlen(cret)-1;
	    while ( isspace(*pt) ) --pt;
	    if ( strncmp(pt-2,"in",2)==0)
		scale = 72;
	    else if ( strncmp(pt-2,"cm",2)==0 )
		scale = 72/2.54;
	    else if ( strncmp(pt-2,"mm",2)==0 )
		scale = 72/25.4;
	    else if ( strncmp(pt-2,"pt",2)==0 )
		scale = 1;
	    else {
		GDrawIError("Bad Pagesize units are unknown\nMust be one of pt (points), mm, cm, in" );
return( true );
	    }
	    pgwidth = pw*scale; pgheight = ph*scale;
	    free(cret);
	}

	ret = _GGadgetGetTitle(GWidgetGetControl(pi->setup,CID_Printer));
	if ( uc_strcmp(ret,"<default>")==0 || *ret=='\0' )
	    ret = NULL;
	pi->printer = cu_copy(ret);
	pi->pagewidth = pgwidth; pi->pageheight = pgheight;
	pi->copies = copies;

	if ( GGadgetIsChecked(GWidgetGetControl(pi->setup,CID_lp)))
	    pi->printtype = pt_lp;
	else if ( GGadgetIsChecked(GWidgetGetControl(pi->setup,CID_lpr)))
	    pi->printtype = pt_lpr;
	else if ( GGadgetIsChecked(GWidgetGetControl(pi->setup,CID_ghostview)))
	    pi->printtype = pt_ghostview;
	else
	    pi->printtype = pt_file;

	printtype = pi->printtype;
	free(lastprinter); lastprinter = copy(pi->printer);
	pagewidth = pgwidth; pageheight = pgheight;

	pi->done = true;
    }
return( true );
}

static int PG_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	pi->done = true;
    }
return( true );
}

static int PG_RadioSet(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_radiochanged ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	PG_SetEnabled(pi);
    }
return( true );
}

static int pg_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	PI *pi = GDrawGetUserData(gw);
	pi->done = true;
    }
return( event->type!=et_char );
}

static GTextInfo *PrinterList() {
    char line[400];
    FILE *printcap;
    GTextInfo *tis=NULL;
    int cnt;
    char *bpt, *cpt;

    printcap = fopen("/etc/printcap","r");
    if ( printcap==NULL ) {
	tis = gcalloc(2,sizeof(GTextInfo));
	tis[0].text = uc_copy("<default>");
return( tis );
    }

    while ( 1 ) {
	cnt=1;		/* leave room for default printer */
	while ( fgets(line,sizeof(line),printcap)!=NULL ) {
	    if ( !isspace(*line) && *line!='#' ) {
		if ( tis!=NULL ) {
		    bpt = strchr(line,'|');
		    cpt = strchr(line,':');
		    if ( cpt==NULL && bpt==NULL )
			cpt = line+strlen(line)-1;
		    else if ( cpt!=NULL && bpt!=NULL && bpt<cpt )
			cpt = bpt;
		    else if ( cpt==NULL )
			cpt = bpt;
		    tis[cnt].text = uc_copyn(line,cpt-line);
		}
		++cnt;
	    }
	}
	if ( tis!=NULL ) {
	    fclose(printcap);
return( tis );
	}
	tis = gcalloc((cnt+1),sizeof(GTextInfo));
	tis[0].text = uc_copy("<default>");
	rewind(printcap);
    }
}
    
static int PageSetup(PI *pi) {
    GRect pos;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[14];
    GTextInfo label[14];
    static unichar_t title[] = { 'P','a','g','e',' ','S','e','t','u','p',  '\0' };
    int cnt;
    char buf[10], pb[30];
    int pt;
    static GTextInfo pagesizes[] = {
	{ (unichar_t *) "US Letter", NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ (unichar_t *) "US Legal", NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ (unichar_t *) "A3", NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ (unichar_t *) "A4", NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ (unichar_t *) "B4", NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
	{ NULL }
    };

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = title;
    pos.x = pos.y = 0;
    pos.width =GDrawPointsToPixels(NULL,250);
    pos.height = GDrawPointsToPixels(NULL,150);
    pi->setup = GDrawCreateTopWindow(NULL,&pos,pg_e_h,pi,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    label[0].text = (unichar_t *) "lp";
    label[0].text_is_1byte = true;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.mnemonic = 'l';
    gcd[0].gd.pos.x = 40; gcd[0].gd.pos.y = 6; 
    gcd[0].gd.flags = gg_visible | gg_enabled;
    gcd[0].gd.cid = CID_lp;
    gcd[0].gd.handle_controlevent = PG_RadioSet;
    gcd[0].creator = GRadioCreate;

    label[1].text = (unichar_t *) "lpr";
    label[1].text_is_1byte = true;
    gcd[1].gd.label = &label[1];
    gcd[1].gd.mnemonic = 'r';
    gcd[1].gd.pos.x = gcd[0].gd.pos.x; gcd[1].gd.pos.y = 18+gcd[0].gd.pos.y; 
    gcd[1].gd.flags = (cnt==0 ? gg_visible : (gg_visible | gg_enabled));
    gcd[1].gd.cid = CID_lpr;
    gcd[1].gd.handle_controlevent = PG_RadioSet;
    gcd[1].creator = GRadioCreate;

    label[2].text = (unichar_t *) "ghostview";
    label[2].text_is_1byte = true;
    gcd[2].gd.label = &label[2];
    gcd[2].gd.mnemonic = 'g';
    gcd[2].gd.pos.x = gcd[0].gd.pos.x+50; gcd[2].gd.pos.y = gcd[0].gd.pos.y; 
    gcd[2].gd.flags = gg_visible | gg_enabled;
    gcd[2].gd.cid = CID_ghostview;
    gcd[2].gd.handle_controlevent = PG_RadioSet;
    gcd[2].creator = GRadioCreate;

    label[3].text = (unichar_t *) "To File";
    label[3].text_is_1byte = true;
    gcd[3].gd.label = &label[3];
    gcd[3].gd.mnemonic = 'F';
    gcd[3].gd.pos.x = gcd[1].gd.pos.x+50; gcd[3].gd.pos.y = gcd[1].gd.pos.y; 
    gcd[3].gd.flags = gg_visible | gg_enabled;
    gcd[3].gd.cid = CID_File;
    gcd[3].gd.handle_controlevent = PG_RadioSet;
    gcd[3].creator = GRadioCreate;

    if ( (pt=pi->printtype)==pt_unknown ) pt = pt_lp;
    gcd[pt].gd.flags |= gg_cb_on;


    label[4].text = (unichar_t *) "PageSize:";
    label[4].text_is_1byte = true;
    gcd[4].gd.label = &label[4];
    gcd[4].gd.mnemonic = 'S';
    gcd[4].gd.pos.x = 5; gcd[4].gd.pos.y = 22+gcd[3].gd.pos.y+6; 
    gcd[4].gd.flags = gg_visible | gg_enabled;
    gcd[4].creator = GLabelCreate;

    if ( pi->pagewidth==595 && pi->pageheight==792 )
	strcpy(pb,"US Letter");		/* Pick a name, this is the default case */
    else if ( pi->pagewidth==612 && pi->pageheight==792 )
	strcpy(pb,"US Letter");
    else if ( pi->pagewidth==612 && pi->pageheight==1008 )
	strcpy(pb,"US Legal");
    else if ( pi->pagewidth==595 && pi->pageheight==842 )
	strcpy(pb,"A4");
    else if ( pi->pagewidth==842 && pi->pageheight==1191 )
	strcpy(pb,"A3");
    else if ( pi->pagewidth==708 && pi->pageheight==1000 )
	strcpy(pb,"B4");
    else
	sprintf(pb,"%dx%d mm", (int) (pi->pagewidth*25.4/72),(int) (pi->pageheight*25.4/72));
    label[5].text = (unichar_t *) pb;
    label[5].text_is_1byte = true;
    gcd[5].gd.label = &label[5];
    gcd[5].gd.mnemonic = 'S';
    gcd[5].gd.pos.x = 60; gcd[5].gd.pos.y = gcd[4].gd.pos.y-6;
    gcd[5].gd.pos.width = 90;
    gcd[5].gd.flags = gg_visible | gg_enabled;
    gcd[5].gd.cid = CID_Pagesize;
    gcd[5].gd.u.list = pagesizes;
    gcd[5].creator = GListFieldCreate;


    label[6].text = (unichar_t *) "Copies:";
    label[6].text_is_1byte = true;
    gcd[6].gd.label = &label[6];
    gcd[6].gd.mnemonic = 'C';
    gcd[6].gd.pos.x = 160; gcd[6].gd.pos.y = gcd[4].gd.pos.y; 
    gcd[6].gd.flags = gg_visible | gg_enabled;
    gcd[6].gd.cid = CID_CopiesLab;
    gcd[6].creator = GLabelCreate;

    sprintf(buf,"%d",pi->copies);
    label[7].text = (unichar_t *) buf;
    label[7].text_is_1byte = true;
    gcd[7].gd.label = &label[7];
    gcd[7].gd.mnemonic = 'C';
    gcd[7].gd.pos.x = 200; gcd[7].gd.pos.y = gcd[5].gd.pos.y;
    gcd[7].gd.pos.width = 40;
    gcd[7].gd.flags = gg_visible | gg_enabled;
    gcd[7].gd.cid = CID_Copies;
    gcd[7].creator = GTextFieldCreate;


    label[8].text = (unichar_t *) "Printer:";
    label[8].text_is_1byte = true;
    gcd[8].gd.label = &label[8];
    gcd[8].gd.mnemonic = 'P';
    gcd[8].gd.pos.x = 5; gcd[8].gd.pos.y = 30+gcd[5].gd.pos.y+6; 
    gcd[8].gd.flags = gg_visible | gg_enabled;
    gcd[8].gd.cid = CID_PrinterLab;
    gcd[8].creator = GLabelCreate;

    label[9].text = (unichar_t *) pi->printer;
    label[9].text_is_1byte = true;
    if ( pi->printer!=NULL )
	gcd[9].gd.label = &label[9];
    gcd[9].gd.mnemonic = 'P';
    gcd[9].gd.pos.x = 60; gcd[9].gd.pos.y = gcd[8].gd.pos.y-6;
    gcd[9].gd.pos.width = 90;
    gcd[9].gd.flags = gg_visible | gg_enabled;
    gcd[9].gd.cid = CID_Printer;
    gcd[9].gd.u.list = PrinterList();
    gcd[9].creator = GListFieldCreate;


    gcd[10].gd.pos.x = 30-3; gcd[10].gd.pos.y = gcd[9].gd.pos.y+36;
    gcd[10].gd.pos.width = 55; gcd[10].gd.pos.height = 0;
    gcd[10].gd.flags = gg_visible | gg_enabled | gg_but_default;
    label[10].text = (unichar_t *) "OK";
    label[10].text_is_1byte = true;
    gcd[10].gd.mnemonic = 'O';
    gcd[10].gd.label = &label[10];
    gcd[10].gd.handle_controlevent = PG_OK;
    gcd[10].creator = GButtonCreate;

    gcd[11].gd.pos.x = 250-55-30; gcd[11].gd.pos.y = gcd[10].gd.pos.y+3;
    gcd[11].gd.pos.width = 55; gcd[11].gd.pos.height = 0;
    gcd[11].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[11].text = (unichar_t *) "Cancel";
    label[11].text_is_1byte = true;
    gcd[11].gd.label = &label[11];
    gcd[11].gd.mnemonic = 'C';
    gcd[11].gd.handle_controlevent = PG_Cancel;
    gcd[11].creator = GButtonCreate;

    gcd[12].gd.pos.x = 2; gcd[12].gd.pos.y = 2;
    gcd[12].gd.pos.width = pos.width-4; gcd[12].gd.pos.height = pos.height-2;
    gcd[12].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
    gcd[12].creator = GGroupCreate;

    GGadgetsCreate(pi->setup,gcd);
    GTextInfoListFree(gcd[9].gd.u.list);
    PG_SetEnabled(pi);
    GDrawSetVisible(pi->setup,true);
    while ( !pi->done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(pi->setup);
    pi->done = false;
return( pi->printtype!=pt_unknown );
}

/* ************************************************************************** */
/* ************************* Code for Print dialog ************************** */
/* ************************************************************************** */

/* Slightly different from one in fontview */
static int FVSelCount(FontView *fv) {
    int i, cnt=0;
    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] && SCWorthOutputting(fv->sf->chars[i])) ++cnt;
return( cnt);
}

static void QueueIt(PI *pi) {
    int pid;
    int stdinno, i, status;
    char *argv[8], buf[10];

    if ( (pid=fork())==0 ) {
	stdinno = fileno(stdin);
	close(stdinno);
	dup2(fileno(pi->out),stdinno);
	i = 0;
	if ( pi->printtype == pt_ghostview ) {
	    argv[i++] = "ghostview";
	    argv[i++] = "-";		/* read from stdin */
	} else if ( pi->printtype == pt_lp ) {
	    argv[i++] = "lp";
	    if ( pi->printer!=NULL ) {
		argv[i++] = "-d";
		argv[i++] = pi->printer;
	    }
	    if ( pi->copies>1 ) {
		argv[i++] = "-n";
		sprintf(buf,"%d", pi->copies );
		argv[i++] = buf;
	    }
	} else {
	    argv[i++] = "lpr";
	    if ( pi->printer!=NULL ) {
		argv[i++] = "-P";
		argv[i++] = pi->printer;
	    }
	    if ( pi->copies>1 ) {
		sprintf(buf,"-#%d", pi->copies );
		argv[i++] = buf;
	    }
	}
	argv[i] = NULL;
 /*for ( i=0; argv[i]!=NULL; ++i ) printf( "%s ", argv[i]); printf("\n" );*/
	execvp(argv[0],argv);
	GDrawIError("Failed to exec print job");
	_exit(1);
    } else if ( pid==-1 )
	GDrawIError("Failed to fork print job");
    else if ( pi->printtype != pt_ghostview ) {
	waitpid(pid,&status,0);
	if ( !WIFEXITED(status) )
	    GDrawIError("Failed to queue print job");
    }
    waitpid(-1,&status,WNOHANG);	/* Clean up any zombie ghostviews */
}

#define CID_Display	1001
#define CID_Chars	1002
#define	CID_Sample	1003
#define CID_PSLab	1004
#define	CID_PointSize	1005
#define CID_SmpLab	1006
#define CID_SampleText	1007

static void PRT_SetEnabled(PI *pi) {
    int enable_ps, enable_sample;

    enable_ps = !GGadgetIsChecked(GWidgetGetControl(pi->gw,CID_Chars));
    enable_sample = GGadgetIsChecked(GWidgetGetControl(pi->gw,CID_Sample));

    GGadgetSetEnabled(GWidgetGetControl(pi->gw,CID_PSLab),enable_ps);
    GGadgetSetEnabled(GWidgetGetControl(pi->gw,CID_PointSize),enable_ps);
    GGadgetSetEnabled(GWidgetGetControl(pi->gw,CID_SmpLab),enable_sample);
    GGadgetSetEnabled(GWidgetGetControl(pi->gw,CID_SampleText),enable_sample);
}

static int PRT_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	int err = false;
	unichar_t *sample;
	int di = pi->fv!=NULL?0:pi->mv!=NULL?2:1;
	unichar_t *ret;
	char *file;
	static unichar_t title[] = { 'P','r','i','n','t',' ','t','o',' ','F','i','l','e','.','.','.',  '\0' };
	static unichar_t filter[] = { '*','.','p','s',  '\0' };
	char buf[100];
	unichar_t ubuf[100];

	pi->pt = GGadgetIsChecked(GWidgetGetControl(pi->gw,CID_Chars))? pt_chars:
		GGadgetIsChecked(GWidgetGetControl(pi->gw,CID_Sample))? pt_fontsample:
		pt_fontdisplay;
	if ( pi->pt!=pt_chars ) {
	    pi->pointsize = GetInt(pi->gw,CID_PointSize,"Pointsize",&err);
	    if ( err )
return(true);
	}
	if ( pi->printtype==pt_unknown )
	    if ( !PageSetup(pi))
return(true);

	if ( pi->printtype==pt_file ) {
	    sprintf(buf,"pr-%.90s.ps", pi->sf->fontname );
	    uc_strcpy(ubuf,buf);
	    ret = GWidgetSaveAsFile(title,ubuf,filter,NULL);
	    if ( ret==NULL )
return(true);
	    file = cu_copy(ret);
	    free(ret);
	    pi->out = fopen(file,"w");
	    if ( pi->out==NULL ) {
		GDrawError("Failed to open file %s for output", file );
		free(file);
return(true);
	    }
	} else {
	    file = NULL;
	    pi->out = tmpfile();
	    if ( pi->out==NULL ) {
		GDrawError("Failed to open temporary output file" );
return(true);
	    }
	}

	sample = NULL;
	if ( pi->pt==pt_fontsample )
	    sample = GGadgetGetTitle(GWidgetGetControl(pi->gw,CID_SampleText));
	pdefs[di].last_cs = pi->sf->encoding_name;
	pdefs[di].pt = pi->pt;
	pdefs[di].pointsize = pi->pointsize;
	free( pdefs[di].text );
	pdefs[di].text = sample;

	if ( pi->pt==pt_fontdisplay )
	    PIFontDisplay(pi);
	else if ( pi->pt==pt_fontsample )
	    PIFontSample(pi,sample);
	else
	    PIChars(pi);
	rewind(pi->out);
	if ( ferror(pi->out) )
	    GDrawError("Failed to generate postscript in file %s", file==NULL?"temporary":file );
	if ( pi->printtype!=pt_file )
	    QueueIt(pi);
	if ( fclose(pi->out)!=0 )
	    GDrawError("Failed to generate postscript in file %s", file==NULL?"temporary":file );
	free(file);

	pi->done = true;
    }
return( true );
}

static int PRT_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	pi->done = true;
    }
return( true );
}

static int PRT_Setup(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	PageSetup(pi);
    }
return( true );
}

static int PRT_RadioSet(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_radiochanged ) {
	PI *pi = GDrawGetUserData(GGadgetGetWindow(g));
	PRT_SetEnabled(pi);
    }
return( true );
}

static int e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	PI *pi = GDrawGetUserData(gw);
	pi->done = true;
    }
return( event->type!=et_char );
}

static unichar_t _simple[] = { ' ','T','h','e',' ','q','u','i','c','k',' ','b',
	'r','o','w','n',' ','f','o','x',' ','j','u','m','p','s',' ','o','v','e',
	'r',' ','t','h','e',' ','l','a','z','y',' ','d','o','g','.',  '\0' };
static unichar_t *simple[] = { _simple, NULL };
static unichar_t _annakarenena1[] = { ' ',0x412,0x441,0x463,' ',0x441,0x447,0x430,0x441,
	0x442,0x43b,0x438,0x432,0x44b,0x44f,' ',0x441,0x435,0x43c,0x44c,0x438,
	' ',0x43d,0x43e,0x445,0x43e,0x436,0x438,' ',0x434,0x440,0x443,0x433,
	0x44a,' ',0x43d,0x430,' ',0x434,0x440,0x443,0x433,0x430,',',' ',0x43a,
	0x430,0x436,0x434,0x430,0x44f,' ',0x43d,0x435,0x441,0x447,0x430,0x441,
	0x442,0x43b,0x438,0x432,0x430,0x44f,' ',0x441,0x435,0x43c,0x44c,0x44f,
	' ',0x43d,0x435,0x441,0x447,0x430,0x441,0x442,0x43b,0x438,0x432,0x430,
	' ',0x43d,0x43e,'-',0x441,0x432,0x43e,0x435,0x43c,0x443,'.',  '\0' };
static unichar_t _annakarenena2[] = { ' ',0x0412,0x0441,0x0435,' ',0x0441,0x043c,
	0x0463,0x0448,0x0430,0x043b,0x043e,0x0441,0x044c,' ',0x0432,0x044a,' ',
	0x0434,0x043e,0x043c,0x0463,' ',0x041e,0x0431,0x043b,0x043e,0x043d,
	0x0441,0x043a,0x0438,0x0445,0x044a,'.',' ',0x0416,0x0435,0x043d,0x0430,
	' ',0x0443,0x0437,0x043d,0x0430,0x043b,0x0430,',',' ',0x0447,0x0442,
	0x043e,' ',0x043c,0x0443,0x0436,0x044a,' ',0x0431,0x044b,0x043b,0x044a,
	' ',0x0441,0x0432,0x044f,0x0437,0x0438,' ',0x0441,0x044a,' ',0x0431,
	0x044b,0x0432,0x0448,0x0435,0x044e,' ',0x0432,0x044a,' ',0x0438,0x0445,
	0x044a,' ',0x0434,0x043e,0x043c,0x0463,' ',0x0444,0x0440,0x0430,0x043d,
	0x0446,0x0443,0x0436,0x0435,0x043d,0x043a,0x043e,0x044e,'-',0x0433,
	0x0443,0x0432,0x0435,0x0440,0x043d,0x0430,0x043d,0x0442,0x043a,0x043e,
	0x0439,',',' ',0x0438,' ',0x043e,0x0431,0x044a,0x044f,0x0432,0x0438,
	0x043b,0x0430,' ',0x043c,0x0443,0x0436,0x0443,',',' ',0x0447,0x0442,
	0x043e,' ',0x043d,0x0435,' ',0x043c,0x043e,0x0436,0x0435,0x0442,0x044a,
	' ',0x0436,0x0438,0x0442,0x044c,' ',0x0441,0x044a,' ',0x043d,0x0438,
	0x043c,0x044a,' ',0x0432,0x044a,' ',0x043e,0x0434,0x043d,0x043e,0x043c,
	0x044a,' ',0x0434,0x043e,0x043c,0x0463,'.',  '\0' };
static unichar_t *annakarenena[] = { _annakarenena1, _annakarenena2, NULL };
static unichar_t _donquixote[] = { ' ','E','n',' ','u','n',' ','l','u','g','a','r',
	' ','d','e',' ','l','a',' ','M','a','n','c','h','a',',',' ','d','e',
	' ','c','u','y','o',' ','n','o','m','b','r','e',' ','n','o',' ','q',
	'u','i','e','r','o',' ','a','c','o','r','d','a','r','m','e',',',' ',
	'n','o',' ','h','a',' ','m','u','c','h','o',' ','t','i','e','m','p',
	'o',' ','q','u','e',' ','v','i','v','�','a',' ','u','n',' ','h','i',
	'd','a','l','g','o',' ','d','e',' ','l','o','s',' ','d','e',' ','l',
	'a','n','z','a',' ','e','n',' ','a','s','t','i','l','l','e','r','o',
	',',' ','a','d','a','r','g','a',' ','a','n','t','i','g','u','a',',',
	' ','r','o','c','�','n',' ','f','l','a','c','o',' ','y',' ','g','a',
	'l','g','o',' ','c','o','r','r','e','d','o','r','.',  '\0' };
static unichar_t *donquixote[] = { _donquixote, NULL };
static unichar_t _faust1[] = { 'I','h','r',' ','n','a','h','t',' ','e','u','c',
	'h',' ','w','i','e','d','e','r',',',' ','s','c','h','w','a','n','k',
	'e','n','d','e',' ','G','e','s','t','a','l','t','e','n',',',  '\0'};
static unichar_t _faust2[] = { 'D','i','e',' ','f','r','�','h',' ','s','i','c',
	'h',' ','e','i','n','s','t',' ','d','e','m',' ','t','r','�','b','e',
	'n',' ','B','l','i','c','k',' ','g','e','z','e','i','g','t','.',  '\0' };
static unichar_t _faust3[] = { 'V','e','r','s','u','c','h',' ','i','c','h',' ',
	'w','o','h','l',',',' ','e','u','c','h',' ','d','i','e','s','m','a',
	'l',' ','f','e','s','t','z','u','h','a','l','t','e','n','?',  '\0'};
static unichar_t _faust4[] = { 'F','�','h','l',' ','i','c','h',' ','m','e','i',
	'n',' ','H','e','r','z',' ','n','o','c','h',' ','j','e','n','e','m',
	' ','W','a','h','n',' ','g','e','n','e','i','g','t','?',  '\0' };
static unichar_t _faust5[] = { 'I','h','r',' ','d','r','�','n','g','t',' ','e',
	'u','c','h',' ','z','u','!',' ','N','u','n',' ','g','u','t',',',' ','s'
	,'o',' ','m','�','g','t',' ','i','h','r',' ','w','a','l','t','e','n',
	',',  '\0'};
static unichar_t _faust6[] = { 'W','i','e',' ','i','h','r',' ','a','u','s',' ',
	'D','u','n','s','t',' ','u','n','d',' ','N','e','b','e','l',' ','u',
	'm',' ','m','i','c','h',' ','s','t','e','i','g','t',';',  '\0'};
static unichar_t _faust7[] = { 'M','e','i','n',' ','B','u','s','e','n',' ','f',
	'�','h','l','t',' ','s','i','c','h',' ','j','u','g','e','n','d','l',
	'i','c','h',' ','e','r','s','c','h','�','t','t','e','r','t',  '\0' };
static unichar_t _faust8[] = { 'V','o','m',' ','Z','a','u','b','e','r','h','a',
	'u','c','h',',',' ','d','e','r',' ','e','u','r','e','n',' ','Z','u',
	'g',' ','u','m','w','i','t','t','e','r','t','.',  '\0' };
static unichar_t *faust[] = { _faust1, _faust2, _faust3, _faust4, _faust5,
	_faust6, _faust7, _faust8, NULL };
static unichar_t _beorwulf1[] = { 'H','w','�','t',',',' ','w','e',' ','G','a',
	'r','-','D','e','n','a',' ',' ','i','n',' ','g','e','a','r','d','a',
	'g','u','m',  '\0' };
static unichar_t _beorwulf1_5[] = {'�','e','o','d','c','y','n','i','n','g','a',' ',' ',
	'�','r','y','m',' ','g','e','f','r','u','n','o','n',',',  '\0' };
static unichar_t _beorwulf2[] = { 'h','u',' ','�','a',' ','�','�','e','l','i',
	'n','g','a','s',' ',' ','e','l','l','e','n',' ','f','r','e','m','e',
	'd','o','n','.',  '\0' };
static unichar_t _beorwulf3[] = { ' ','O','f','t',' ','S','c','y','l','d',' ',
	'S','c','e','f','i','n','g',' ',' ','s','c','e','a','�','e','n','a',
	' ','�','r','e','a','t','u','m',',',  '\0' };
static unichar_t _beorwulf4[] = { 'm','o','n','e','g','u','m',' ','m','�','g',
	'�','u','m',' ',' ','m','e','o','d','o','s','e','t','l','a',' ','o',
	'f','t','e','a','h',';',  '\0' };
static unichar_t _beorwulf5[] = { 'e','g','s','o','d','e',' ','E','o','r','l',
	'e','.',' ',' ','S','y','�','�','a','n',' ','�','r','e','s','t',' ',
	'w','e','a','r','�',  '\0' };
static unichar_t _beorwulf6[] = { 'f','e','a','s','c','e','a','f','t',' ','f',
	'u','n','d','e','n',',',' ',' ','(','h','e',' ','�','�','s',' ','f',
	'r','o','f','r','e',' ','g','e','b','a','d',')',  '\0' };
static unichar_t _beorwulf7[] = { 'w','e','o','x',' ','u','n','d','e','r',' ',
	'w','o','l','c','n','u','m',',',' ',' ','w','e','o','r','�','m','y',
	'n','d','u','m',' ','�','a','h',',',  '\0' };
static unichar_t _beorwulf8[] = { 'o','�','�','�','t',' ','h','i','m',' ','�',
	'g','h','w','y','l','e',' ',' ','�','a','r','a',' ','y','m','b','s',
	'i','t','t','e','n','d','r','a',  '\0' };
static unichar_t _beorwulf9[] = { 'o','f','e','r',' ','h','r','o','n','r','a',
	'd','e',' ',' ','h','y','a','n',' ','s','c','o','l','d','e',',',  '\0'};
static unichar_t _beorwulf10[] = { 'g','o','m','b','a','n',' ','g','y','l','d',
	'a','n',':',' ','�','�','t',' ','w','�','s',' ','g','o','d',' ','c',
	'y','n','i','n','g','!',  '\0' };
static unichar_t *beorwulf[] = { _beorwulf1, _beorwulf1_5, _beorwulf2, _beorwulf3,
	_beorwulf4,  _beorwulf5, _beorwulf6, _beorwulf7, _beorwulf8,
	_beorwulf9, _beorwulf10, NULL };
static unichar_t _debello1[] = { ' ','G','a','l','l','i','a',' ','e','s','t',
	' ','o','m','n','i','s',' ','d',0x012b,'v',0x012b,'s','a',' ','i','n',
	' ','p','a','r','t',0x0113,'s',' ','t','r',0x0113,'s',',',' ','q','u',
	0x0101,'r','u','m',' ',0x016b,'n','u','m',' ','i','n','c','o','l','u',
	'n','t',' ','B','e','l','g','a','e',',',' ','a','l','i','a','m',' ',
	'A','q','u',0x012b,'t',0x0101,'n',0x012b,',',' ','t','e','r','t','i',
	'a','m',',',' ','q','u',0x012b,' ','i','p','s',0x014d,'r','u','m',' ',
	'l','i','n','g','u',0x0101,' ','C','e','l','t','a','e',',',' ','n',
	'o','s','t','r',0x0101,' ','G','a','l','l',0x012b,' ','a','p','p','e',
	'l','a','n','t','u','r','.',' ','H',0x012b,' ','o','m','n',0x0113,'s',
	' ','l','i','n','g','u',0x0101,',',' ',0x012b,'n','s','t','i','t',
	0x016b,'t',0x012b,'s',',',' ','l',0x0113,'g','i','b','u','s',' ','i',
	'n','t','e','r',' ','s',0x0113,' ','d','i','f','f','e','r','u','n',
	't','.',' ','G','a','l','l',0x014d,'s',' ','a','b',' ','A','q','u',
	0x012b,'t',0x0101,'n',0x012b,'s',' ','G','a','r','u','m','n','a',' ',
	'f','l',0x016b,'m','e','n',',',' ',0x0101,' ','B','e','l','g',0x012b,
	's',' ','M','a','t','r','o','n','a',' ','e','t',' ','S',0x0113,'q',
	'u','a','n','a',' ','d',0x012b,'v','i','d','i','t','.',  '\0' };
static unichar_t *debello[] = { _debello1, NULL };
static unichar_t _pheadra1[] = { 'L','e',' ','d','e','s','s','e','i','n',' ',
	'e','n',' ','e','s','t',' ','p','r','i','s',':',' ','j','e',' ','p',
	'a','r','s',',',' ','c','h','e','r',' ','T','h','�','r','a','m','�','n'
	,'e',',',  '\0' };
static unichar_t _pheadra2[] = { 'E','t',' ','q','u','i','t','t','e',' ','l',
	'e',' ','s','�','j','o','u','r',' ','d','e',' ','l',0x0027,'a','i','m',
	'a','b','l','e',' ','T','r','�','z','�','n','e','.',  '\0' };
static unichar_t _pheadra3[] = { 'D','a','n','s',' ','l','e',' ','d','o','u',
	't','e',' ','m','o','r','t','e','l',' ','d','o','n','t',' ','j','e',
	' ','s','u','i','s',' ','a','g','i','t','�',',',  '\0' };
static unichar_t _pheadra4[] = { 'J','e',' ','c','o','m','m','e','n','c','e',
	' ','�',' ','r','o','u','g','i','r',' ','d','e',' ','m','o','n',' ',
	'o','i','s','i','v','e','t','�','.',  '\0' };
static unichar_t _pheadra5[] = { 'D','e','p','u','i','s',' ','p','l','u','s',
	' ','d','e',' ','s','i','x',' ','m','o','i','s',' ','�','l','o','i',
	'g','n','�',' ','d','e',' ','m','o','n',' ','p','�','r','e',',',  '\0' };
static unichar_t _pheadra6[] = { 'J',0x0027,'i','g','n','o','r','e',' ','l',
	'e',' ','d','e','s','t','i','n',' ','d',0x0027,'u','n','e',' ','t',
	'�','t','e',' ','s','i',' ','c','h','�','r','e',';',  '\0' };
static unichar_t _pheadra7[] = { 'J',0x0027,'i','g','n','o','r','e',' ','j',
	'u','s','q','u',0x0027,'a','u','x',' ','l','i','e','u','x',' ','q',
	'u','i',' ','l','e',' ','p','e','u','v','e','n','t',' ','c','a','c',
	'h','e','r','.',  '\0' };
static unichar_t *pheadra[] = { _pheadra1,  _pheadra2, _pheadra3, _pheadra4,
	_pheadra5, _pheadra6, _pheadra7, NULL };
static unichar_t _antigone1[] = { 0x1f6e,' ',0x03ba,0x03bf,0x03b9,0x03bd,0x1f78,
    0x03bd,' ',0x03b1,0x1f50,0x03c4,0x1f71,0x03b4,0x03b5,0x03bb,0x03c6,0x03bf,
    0x03bd,' ',0x1f38,0x03c3,0x03bc,0x1f75,0x03bd,0x03b7,0x03c2,' ',0x03ba,
    0x1f71,0x03c1,0x03b1,',',  '\0'};
static unichar_t _antigone2[] = { 0x1f06,0x1fe4,' ',0x03bf,0x1f36,0x03c3,0x03b8,
	0x1fbd,' ',0x1f45,0x03c4,0x03b9,' ',0x0396,0x03b5,0x1f7a,0x03c2,' ',
	0x03c4,0x1ff6,0x03bd,' ',0x1f00,0x03c0,0x1fbd,' ',0x039f,0x1f30,0x03b4,
	0x1f77,0x03c0,0x03bf,0x03c5,' ',0x03ba,0x03b1,0x03ba,0x1ff6,0x03bd,  '\0'};
static unichar_t _antigone3[] = { 0x1f41,0x03c0,0x03bf,0x1fd6,0x03bf,0x03bd,' ',
	0x03bf,0x1f50,0x03c7,0x1f76,' ',0x03bd,0x1ff7,0x03bd,' ',0x1f14,0x03c4,
	0x03b9,' ',0x03b6,0x03c3,0x03b1,0x03b9,0x03bd,' ',0x03c4,0x03b5,0x03bb,
	0x03b5,0x1fd6,';',  '\0' };
static unichar_t *antigone[] = { _antigone1, _antigone2, _antigone3, NULL };
static unichar_t _hebrew[] = { 0x5d0, 0x5d1, 0x5d2, 0x5d3, 0x5d4, 0x5d5, 0 };
static unichar_t *hebrew[] = { _hebrew, NULL };
static unichar_t _muchado[] = {' ','B','u','t',' ','t','i','l','l',' ','a','l',
	'l',' ','g','r','a','c','e','s',' ','b','e',' ','i','n',' ','o','n','e'
	,' ','w','o','m','a','n',',',' ','o','n','e',' ','w','o','m','�',' ',
	0x017f,'h','a','l',' ','n','o','t',' ','c','o','m',' ','i','n',' ','m',
	'y',' ','g','r','a','c','e',':',' ','r','i','c','h',' ',0x017f,'h',
	'e',' ',0x017f,'h','a','l','l',' ','b','e',' ','t','h','a','t','s',
	' ','c','e','r','t','a','i','n',',',' ','w','i',0x017f,'e',',',' ',
	'o','r',' ','i','l','e',' ','n','o','n','e',',',' ','v','e','r','t',
	'u','o','u','s',',',' ','o','r',' ','i','l','e',' ','n','e','u','e',
	'r',' ','c','h','e','a','p','e','n',' ','h','e','r','.',  '\0' };
/* contains long-s, u used as v, tilde over vowel used as nasal, and misspellings */
static unichar_t *muchado[] = { _muchado, NULL };
static unichar_t **sample[] = { simple, faust, pheadra, antigone, annakarenena,
	debello, hebrew, donquixote, beorwulf, muchado, NULL };

static int AllChars( SplineFont *sf, unichar_t *str) {
    int i, ch;
    /* The accented characters above get sign-extended, which is wrong for */
    /*  unicode. Just truncate them. We won't use any characters in the 0xff00 */
    /*  area anyway so this is save */

    if ( sf->encoding_name==em_unicode ) {
	while ( (ch = *str)!='\0' ) {
	    if ( (ch&0xff00)==0xff00 ) ch &= 0xff;
	    if ( !SCWorthOutputting(sf->chars[ch]))
return( false );
	    ++str;
	}
    } else {
	while ( (ch = *str)!='\0' ) {
	    if ( (ch&0xff00)==0xff00 ) ch &= 0xff;
	    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
		if ( sf->chars[i]->unicodeenc == ch )
	    break;
	    }
	    if ( i==sf->charcnt || !SCWorthOutputting(sf->chars[i]))
return( false );
	    ++str;
	}
    }
return( true );
}

static void u_stupidstrcpy( unichar_t *pt1, unichar_t *pt2 ) {
    int ch;

    while ( (ch = *pt2++)!='\0' ) {
	if ( (ch&0xff00) == 0xff00 ) ch &= 0xff;
	*pt1 ++ = ch;
    }
    *pt1++ = '\0';
}

static unichar_t *BuildDef( SplineFont *sf) {
    int i, j, gotem, len;
    unichar_t *ret=NULL;

    while ( 1 ) {
	len = 0;
	for ( i=0; sample[i]!=NULL; ++i ) {
	    gotem = true;
	    for ( j=0; sample[i][j]!=NULL && gotem; ++j )
		gotem = AllChars(sf,sample[i][j]);
	    if ( gotem ) {
		for ( j=0; sample[i][j]!=NULL; ++j ) {
		    if ( ret )
			u_stupidstrcpy(ret+len,sample[i][j]);
		    len += u_strlen(sample[i][j]);
		    if ( ret )
			ret[len] = '\n';
		    ++len;
		}
		if ( ret )
		    ret[len] = '\n';
		++len;
	    }
	}
	if ( ret ) {
	    ret[len-1]='\0';
return( ret );
	}
	if ( len == 0 ) {
	    /* Er... We didn't find anything?! */
return( gcalloc(1,sizeof(unichar_t)));
	}
	ret = galloc((len+1)*sizeof(unichar_t));
    }
}

void PrintDlg(FontView *fv,SplineChar *sc,MetricsView *mv) {
    GRect pos;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[12];
    GTextInfo label[12];
    static unichar_t title[] = { 'P','r','i','n','t',  '\0' };
    int di = fv!=NULL?0:sc!=NULL?1:2;
    PI pi;
    int cnt;
    char buf[10];

    memset(&pi,'\0',sizeof(pi));
    pi.fv = fv;
    pi.mv = mv;
    pi.sc = sc;
    if ( fv!=NULL )
	pi.sf = fv->sf;
    else if ( sc!=NULL )
	pi.sf = sc->parent;
    else
	pi.sf = mv->fv->sf;
    pi.twobyte = pi.sf->encoding_name>=e_first2byte;
    pi.iscjk = pi.sf->encoding_name>=e_first2byte && pi.sf->encoding_name!=em_unicode;
    pi.pointsize = pdefs[di].pointsize;
    if ( pi.pointsize==0 )
	pi.pointsize = 20;
    PIGetPrinterDefs(&pi);

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = title;
    pos.x = pos.y = 0;
    pos.width =GDrawPointsToPixels(NULL,310);
    pos.height = GDrawPointsToPixels(NULL,310);
    pi.gw = GDrawCreateTopWindow(NULL,&pos,e_h,&pi,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    label[0].text = (unichar_t *) "Full Font Display";
    label[0].text_is_1byte = true;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.mnemonic = 'F';
    gcd[0].gd.pos.x = 12; gcd[0].gd.pos.y = 6; 
    gcd[0].gd.flags = gg_visible | gg_enabled;
    gcd[0].gd.cid = CID_Display;
    gcd[0].gd.handle_controlevent = PRT_RadioSet;
    gcd[0].creator = GRadioCreate;

    cnt = 1;
    if ( fv!=NULL )
	cnt = FVSelCount(fv);
    else if ( mv!=NULL )
	cnt = mv->charcnt;
    label[1].text = (unichar_t *) (cnt==1?"Full Page Character":"Full Page Characters");
    label[1].text_is_1byte = true;
    gcd[1].gd.label = &label[1];
    gcd[1].gd.mnemonic = 'C';
    gcd[1].gd.pos.x = gcd[0].gd.pos.x; gcd[1].gd.pos.y = 18+gcd[0].gd.pos.y; 
    gcd[1].gd.flags = (cnt==0 ? gg_visible : (gg_visible | gg_enabled));
    gcd[1].gd.cid = CID_Chars;
    gcd[1].gd.handle_controlevent = PRT_RadioSet;
    gcd[1].creator = GRadioCreate;

    label[2].text = (unichar_t *) "Sample Text";
    label[2].text_is_1byte = true;
    gcd[2].gd.label = &label[2];
    gcd[2].gd.mnemonic = 'S';
    gcd[2].gd.pos.x = gcd[0].gd.pos.x; gcd[2].gd.pos.y = 18+gcd[1].gd.pos.y; 
    gcd[2].gd.flags = gg_visible | gg_enabled;
    gcd[2].gd.cid = CID_Sample;
    gcd[2].gd.handle_controlevent = PRT_RadioSet;
    gcd[2].creator = GRadioCreate;

    if ( pdefs[di].pt==pt_chars && cnt==0 )
	pdefs[di].pt = (fv!=NULL?pt_fontdisplay:pt_fontsample);
    gcd[pdefs[di].pt].gd.flags |= gg_cb_on;


    label[3].text = (unichar_t *) "Pointsize:";
    label[3].text_is_1byte = true;
    gcd[3].gd.label = &label[3];
    gcd[3].gd.mnemonic = 'P';
    gcd[3].gd.pos.x = 5; gcd[3].gd.pos.y = 22+gcd[2].gd.pos.y+6; 
    gcd[3].gd.flags = gg_visible | gg_enabled;
    gcd[3].gd.cid = CID_PSLab;
    gcd[3].creator = GLabelCreate;

    sprintf(buf,"%d",pi.pointsize);
    label[4].text = (unichar_t *) buf;
    label[4].text_is_1byte = true;
    gcd[4].gd.label = &label[4];
    gcd[4].gd.mnemonic = 'P';
    gcd[4].gd.pos.x = 60; gcd[4].gd.pos.y = gcd[3].gd.pos.y-6;
    gcd[4].gd.pos.width = 60;
    gcd[4].gd.flags = gg_visible | gg_enabled;
    gcd[4].gd.cid = CID_PointSize;
    gcd[4].creator = GTextFieldCreate;


    label[5].text = (unichar_t *) "Sample Text:";
    label[5].text_is_1byte = true;
    gcd[5].gd.label = &label[5];
    gcd[5].gd.mnemonic = 'T';
    gcd[5].gd.pos.x = 5; gcd[5].gd.pos.y = 30+gcd[4].gd.pos.y;
    gcd[5].gd.flags = gg_visible | gg_enabled;
    gcd[5].gd.cid = CID_SmpLab;
    gcd[5].creator = GLabelCreate;

    label[6].text = pdefs[di].text;
    if ( label[6].text==NULL || pi.sf->encoding_name!=pdefs[di].last_cs )
	label[6].text = BuildDef(pi.sf);
    gcd[6].gd.label = &label[6];
    gcd[6].gd.mnemonic = 'T';
    gcd[6].gd.pos.x = 5; gcd[6].gd.pos.y = 13+gcd[5].gd.pos.y; 
    gcd[6].gd.pos.width = 300; gcd[6].gd.pos.height = 160; 
    gcd[6].gd.flags = gg_visible | gg_enabled | gg_textarea_wrap;
    gcd[6].gd.cid = CID_SampleText;
    gcd[6].creator = GTextAreaCreate;


    gcd[7].gd.pos.x = 235; gcd[7].gd.pos.y = 12;
    gcd[7].gd.pos.width = 55; gcd[7].gd.pos.height = 0;
    gcd[7].gd.flags = gg_visible | gg_enabled ;
    label[7].text = (unichar_t *) "Setup";
    label[7].text_is_1byte = true;
    gcd[7].gd.mnemonic = 'e';
    gcd[7].gd.label = &label[7];
    gcd[7].gd.handle_controlevent = PRT_Setup;
    gcd[7].creator = GButtonCreate;


    gcd[8].gd.pos.x = 30-3; gcd[8].gd.pos.y = gcd[6].gd.pos.y+gcd[6].gd.pos.height+6;
    gcd[8].gd.pos.width = 55; gcd[8].gd.pos.height = 0;
    gcd[8].gd.flags = gg_visible | gg_enabled | gg_but_default;
    label[8].text = (unichar_t *) "OK";
    label[8].text_is_1byte = true;
    gcd[8].gd.mnemonic = 'O';
    gcd[8].gd.label = &label[8];
    gcd[8].gd.handle_controlevent = PRT_OK;
    gcd[8].creator = GButtonCreate;

    gcd[9].gd.pos.x = 310-55-30; gcd[9].gd.pos.y = gcd[8].gd.pos.y+3;
    gcd[9].gd.pos.width = 55; gcd[9].gd.pos.height = 0;
    gcd[9].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[9].text = (unichar_t *) "Cancel";
    label[9].text_is_1byte = true;
    gcd[9].gd.label = &label[9];
    gcd[9].gd.mnemonic = 'C';
    gcd[9].gd.handle_controlevent = PRT_Cancel;
    gcd[9].creator = GButtonCreate;

    gcd[10].gd.pos.x = 2; gcd[10].gd.pos.y = 2;
    gcd[10].gd.pos.width = pos.width-4; gcd[10].gd.pos.height = pos.height-2;
    gcd[10].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
    gcd[10].creator = GGroupCreate;

    GGadgetsCreate(pi.gw,gcd);
    if ( label[6].text != pdefs[di].text )
	free( label[6].text );
    PRT_SetEnabled(&pi);
    GDrawSetVisible(pi.gw,true);
    while ( !pi.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(pi.gw);
    free(pi.printer);
}
