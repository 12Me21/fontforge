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
#include "psfont.h"
#include <ustring.h>
#include <gkeysym.h>
#include <utype.h>
#include <gfile.h>
#include <chardata.h>
#include <math.h>
#include <gresource.h>
#include <unistd.h>

int onlycopydisplayed = 0;
int copymetadata = 0;

#define XOR_COLOR	0x505050
#define	FV_LAB_HEIGHT	15

#ifdef BIGICONS
#define fontview_width 32
#define fontview_height 32
static unsigned char fontview_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x02, 0x20, 0x80, 0x00,
   0x82, 0x20, 0x86, 0x08, 0x42, 0x21, 0x8a, 0x14, 0xc2, 0x21, 0x86, 0x04,
   0x42, 0x21, 0x8a, 0x14, 0x42, 0x21, 0x86, 0x08, 0x02, 0x20, 0x80, 0x00,
   0xaa, 0xaa, 0xaa, 0xaa, 0x02, 0x20, 0x80, 0x00, 0x82, 0xa0, 0x8f, 0x18,
   0x82, 0x20, 0x91, 0x24, 0x42, 0x21, 0x91, 0x02, 0x42, 0x21, 0x91, 0x02,
   0x22, 0x21, 0x8f, 0x02, 0xe2, 0x23, 0x91, 0x02, 0x12, 0x22, 0x91, 0x02,
   0x3a, 0x27, 0x91, 0x24, 0x02, 0xa0, 0x8f, 0x18, 0x02, 0x20, 0x80, 0x00,
   0xfe, 0xff, 0xff, 0xff, 0x02, 0x20, 0x80, 0x00, 0x42, 0x20, 0x86, 0x18,
   0xa2, 0x20, 0x8a, 0x04, 0xa2, 0x20, 0x86, 0x08, 0xa2, 0x20, 0x8a, 0x10,
   0x42, 0x20, 0x8a, 0x0c, 0x82, 0x20, 0x80, 0x00, 0x02, 0x20, 0x80, 0x00,
   0xaa, 0xaa, 0xaa, 0xaa, 0x02, 0x20, 0x80, 0x00};
#else
#define fontview2_width 16
#define fontview2_height 16
static unsigned char fontview2_bits[] = {
   0x00, 0x07, 0x80, 0x08, 0x40, 0x17, 0x40, 0x15, 0x60, 0x09, 0x10, 0x02,
   0xa0, 0x01, 0xa0, 0x00, 0xa0, 0x00, 0xa0, 0x00, 0x50, 0x00, 0x52, 0x00,
   0x55, 0x00, 0x5d, 0x00, 0x22, 0x00, 0x1c, 0x00};
#endif

extern int _GScrollBar_Width;

int default_fv_font_size = 24, default_fv_antialias=true,
	default_fv_bbsized=true,
	default_fv_showhmetrics=false, default_fv_showvmetrics=false;
#define METRICS_BASELINE 0x0000c0
#define METRICS_ORIGIN	 0xc00000
#define METRICS_ADVANCE	 0x008000
FontView *fv_list=NULL;

void FVToggleCharChanged(SplineChar *sc) {
    int i, j;
    int pos;
    FontView *fv;

    fv = sc->parent->fv;
    if ( fv==NULL )		/* Can happen when loading bitmaps, we might not have an fv yet */
return;
    if ( fv->sf!=sc->parent )		/* Can happen in CID fonts if char's parent is not currently active */
return;
    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;
    for ( ; fv!=NULL ; fv = fv->nextsame ) {
	pos = sc->enc;
	if ( fv->mapping!=NULL ) {
	    for ( i=0; i<fv->mapcnt; ++i )
		if ( fv->mapping[i]==pos )
	    break;
	    if ( i==fv->mapcnt )		/* Not currently displayed */
    continue;
	    pos = i;
	}
	i = pos / fv->colcnt;
	j = pos - i*fv->colcnt;
	i -= fv->rowoff;
	if ( i>=0 && i<fv->rowcnt ) {
	    GRect r;
	    r.x = j*fv->cbw+1; r.width = fv->cbw-1;
	    r.y = i*fv->cbh+1; r.height = FV_LAB_HEIGHT-1;
	    GDrawSetXORBase(fv->v,GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v)));
	    GDrawSetXORMode(fv->v);
	    GDrawFillRect(fv->v,&r,0x000000);
	    GDrawSetCopyMode(fv->v);
	}
    }
}

static void FVToggleCharSelected(FontView *fv,int enc) {
    int i, j;

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;

    i = enc / fv->colcnt;
    j = enc - i*fv->colcnt;
    i -= fv->rowoff;
    if ( i>=0 && i<fv->rowcnt ) {
	GRect r;
	r.x = j*fv->cbw+1; r.width = fv->cbw-1;
	r.y = i*fv->cbh+FV_LAB_HEIGHT+1; r.height = fv->cbw;
	GDrawSetXORBase(fv->v,GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v)));
	GDrawSetXORMode(fv->v);
	GDrawFillRect(fv->v,&r,XOR_COLOR);
	GDrawSetCopyMode(fv->v);
    }
}

static void FVDeselectAll(FontView *fv) {
    int i;

    for ( i=0; i<fv->sf->charcnt; ++i ) {
	if ( fv->selected[i] ) {
	    fv->selected[i] = false;
	    FVToggleCharSelected(fv,i);
	}
    }
}

static void FVSelectAll(FontView *fv) {
    int i;

    for ( i=0; i<fv->sf->charcnt; ++i ) {
	if ( !fv->selected[i] ) {
	    fv->selected[i] = true;
	    FVToggleCharSelected(fv,i);
	}
    }
}

static void FVSelectColor(FontView *fv, uint32 col, int door) {
    int i;
    uint32 sccol;
    SplineChar **chars = fv->sf->chars;

    for ( i=0; i<fv->sf->charcnt; ++i ) {
	sccol =  ( chars[i]==NULL ) ? COLOR_DEFAULT : chars[i]->color;
	if ( (door && !fv->selected[i] && sccol==col) ||
		(!door && fv->selected[i]!=(sccol==col)) ) {
	    fv->selected[i] = !fv->selected[i];
	    FVToggleCharSelected(fv,i);
	}
    }
}

static void FVReselect(FontView *fv, int newpos) {
    int i, start, end;

    start = fv->pressed_pos; end = fv->end_pos;

    if ( fv->pressed_pos<fv->end_pos ) {
	if ( newpos>fv->end_pos ) {
	    for ( i=fv->end_pos+1; i<=newpos; ++i ) if ( !fv->selected[i] ) {
		fv->selected[i] = fv->sel_index;
		FVToggleCharSelected(fv,i);
	    }
	} else if ( newpos<fv->pressed_pos ) {
	    for ( i=fv->end_pos; i>fv->pressed_pos; --i ) if ( fv->selected[i] ) {
		fv->selected[i] = false;
		FVToggleCharSelected(fv,i);
	    }
	    for ( i=fv->pressed_pos-1; i>=newpos; --i ) if ( !fv->selected[i] ) {
		fv->selected[i] = fv->sel_index;
		FVToggleCharSelected(fv,i);
	    }
	} else {
	    for ( i=fv->end_pos; i>newpos; --i ) if ( fv->selected[i] ) {
		fv->selected[i] = false;
		FVToggleCharSelected(fv,i);
	    }
	}
    } else {
	if ( newpos<fv->end_pos ) {
	    for ( i=fv->end_pos-1; i>=newpos; --i ) if ( !fv->selected[i] ) {
		fv->selected[i] = fv->sel_index;
		FVToggleCharSelected(fv,i);
	    }
	} else if ( newpos>fv->pressed_pos ) {
	    for ( i=fv->end_pos; i<fv->pressed_pos; ++i ) if ( fv->selected[i] ) {
		fv->selected[i] = false;
		FVToggleCharSelected(fv,i);
	    }
	    for ( i=fv->pressed_pos+1; i<=newpos; ++i ) if ( !fv->selected[i] ) {
		fv->selected[i] = fv->sel_index;
		FVToggleCharSelected(fv,i);
	    }
	} else {
	    for ( i=fv->end_pos; i<newpos; ++i ) if ( fv->selected[i] ) {
		fv->selected[i] = false;
		FVToggleCharSelected(fv,i);
	    }
	}
    }
    fv->end_pos = newpos;
    if ( newpos>=0 && newpos<fv->sf->charcnt && fv->sf->chars[newpos]!=NULL &&
	    fv->sf->chars[newpos]->unicodeenc>=0 && fv->sf->chars[newpos]->unicodeenc<0x10000 )
	GInsCharSetChar(fv->sf->chars[newpos]->unicodeenc);
}

static void _SplineFontSetUnChanged(SplineFont *sf) {
    int i, was = sf->changed;
    BDFFont *bdf;

    sf->changed = false;
    SFClearAutoSave(sf);
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL )
	if ( sf->chars[i]->changed ) {
	    sf->chars[i]->changed = false;
	    SCRefreshTitles(sf->chars[i]);
	}
    for ( bdf=sf->bitmaps; bdf!=NULL; bdf=bdf->next )
	for ( i=0; i<bdf->charcnt; ++i ) if ( bdf->chars[i]!=NULL )
	    bdf->chars[i]->changed = false;
    if ( was && sf->fv->v!=NULL )
	GDrawRequestExpose(sf->fv->v,NULL,false);
    for ( i=0; i<sf->subfontcnt; ++i )
	_SplineFontSetUnChanged(sf->subfonts[i]);
}

void SplineFontSetUnChanged(SplineFont *sf) {
    if ( sf->cidmaster!=NULL ) sf = sf->cidmaster;
    _SplineFontSetUnChanged(sf);
}

static void FVFlattenAllBitmapSelections(FontView *fv) {
    BDFFont *bdf;
    int i;

    for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	for ( i=0; i<bdf->charcnt; ++i )
	    if ( bdf->chars[i]!=NULL && bdf->chars[i]->selection!=NULL )
		BCFlattenFloat(bdf->chars[i]);
    }
}

static int AskChanged(SplineFont *sf) {
    unichar_t ubuf[300];
    int ret;
    static int buts[] = { _STR_Save, _STR_Dontsave, _STR_Cancel, 0 };
    char *filename, *fontname;

    if ( sf->cidmaster!=NULL )
	sf = sf->cidmaster;

    filename = sf->filename;
    fontname = sf->fontname;

    if ( filename==NULL && sf->origname!=NULL &&
	    sf->onlybitmaps && sf->bitmaps!=NULL && sf->bitmaps->next==NULL )
	filename = sf->origname;
    if ( filename==NULL ) filename = "untitled.sfd";
    filename = GFileNameTail(filename);
    u_strcpy(ubuf, GStringGetResource( _STR_Fontchangepre,NULL ));
    uc_strncat(ubuf,fontname, 70);
    u_strcat(ubuf, GStringGetResource( _STR_Fontchangemid,NULL ));
    uc_strncat(ubuf,filename, 70);
    u_strcat(ubuf, GStringGetResource( _STR_Fontchangepost,NULL ));
    ret = GWidgetAskR_( _STR_Fontchange,buts,0,2,ubuf);
return( ret );
}

static int RevertAskChanged(char *fontname,char *filename) {
    unichar_t ubuf[350];
    int ret;
    static int buts[] = { _STR_Revert, _STR_Cancel, 0 };

    if ( filename==NULL ) filename = "untitled.sfd";
    filename = GFileNameTail(filename);
    u_strcpy(ubuf, GStringGetResource( _STR_Fontchangepre,NULL ));
    uc_strncat(ubuf,fontname, 70);
    u_strcat(ubuf, GStringGetResource( _STR_Fontchangemid,NULL ));
    uc_strncat(ubuf,filename, 70);
    u_strcat(ubuf, GStringGetResource( _STR_Fontchangerevertpost,NULL ));
    ret = GWidgetAskR_( _STR_Fontchange,buts,0,1,ubuf);
return( ret==0 );
}

int _FVMenuGenerate(FontView *fv,int family) {
    FVFlattenAllBitmapSelections(fv);
return( SFGenerateFont(fv->sf,family) );
}

static void FVMenuGenerate(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuGenerate(fv,false);
}

static void FVMenuGenerateFamily(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuGenerate(fv,true);
}

int _FVMenuSaveAs(FontView *fv) {
    unichar_t *temp;
    unichar_t *ret;
    char *filename;
    int ok;

    if ( fv->cidmaster!=NULL && fv->cidmaster->filename!=NULL )
	temp=def2u_copy(fv->cidmaster->filename);
    else if ( fv->sf->filename!=NULL )
	temp=def2u_copy(fv->sf->filename);
    else {
	SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf;
	temp = galloc(sizeof(unichar_t)*(strlen(sf->fontname)+8));
	uc_strcpy(temp,sf->fontname);
	uc_strcat(temp,".sfd");
    }
    ret = GWidgetSaveAsFile(GStringGetResource(_STR_Saveas,NULL),temp,NULL,NULL,NULL);
    free(temp);
    if ( ret==NULL )
return( 0 );
    filename = u2def_copy(ret);
    free(ret);
    FVFlattenAllBitmapSelections(fv);
    ok = SFDWrite(filename,fv->sf);
    if ( ok ) {
	SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf;
	free(sf->filename);
	sf->filename = filename;
	free(sf->origname);
	sf->origname = copy(filename);
	sf->new = false;
	SplineFontSetUnChanged(sf);
	FVSetTitle(fv);
    } else
	free(filename);
return( ok );
}

static void FVMenuSaveAs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuSaveAs(fv);
}

int _FVMenuSave(FontView *fv) {
    int ret = 0;
    SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf;

    if ( sf->filename==NULL && sf->origname!=NULL &&
	    sf->onlybitmaps && sf->bitmaps!=NULL && sf->bitmaps->next==NULL ) {
	/* If it's a single bitmap font then just save back to the bdf file */
	FVFlattenAllBitmapSelections(fv);
	ret = BDFFontDump(sf->origname,sf->bitmaps,EncodingName(sf->encoding_name),sf->bitmaps->res);
	if ( ret )
	    SplineFontSetUnChanged(sf);
    } else if ( sf->filename==NULL )
	ret = _FVMenuSaveAs(fv);
    else {
	FVFlattenAllBitmapSelections(fv);
	if ( !SFDWriteBak(sf) )
	    GWidgetErrorR(_STR_SaveFailed,_STR_SaveFailed);
	else {
	    SplineFontSetUnChanged(sf);
	    ret = true;
	}
    }
return( ret );
}

static void FVMenuSave(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    _FVMenuSave(fv);
}

static int _FVMenuClose(FontView *fv) {
    int i, j;
    BDFFont *bdf;
    MetricsView *mv, *mnext;
    SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf;

    if ( !SFCloseAllInstrs(fv->sf) )
return( false );

    if ( fv->nextsame!=NULL || fv->sf->fv!=fv ) {
	/* There's another view, can close this one with no problems */
    } else if ( sf->changed ) {
	i = AskChanged(fv->sf);
	if ( i==2 )	/* Cancel */
return( false );
	if ( i==0 && !_FVMenuSave(fv))		/* Save */
return(false);
	else
	    SFClearAutoSave(sf);		/* if they didn't save it, remove change record */
    }
    if ( fv->nextsame==NULL && fv->sf->fv==fv && fv->sf->kcld!=NULL )
	KCLD_End(fv->sf->kcld);
    if ( fv->nextsame==NULL && fv->sf->fv==fv && fv->sf->vkcld!=NULL )
	KCLD_End(fv->sf->vkcld);
    
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	CharView *cv, *next;
	for ( cv = sf->chars[i]->views; cv!=NULL; cv = next ) {
	    next = cv->next;
	    GDrawDestroyWindow(cv->gw);
	}
	if ( sf->chars[i]->charinfo )
	    CharInfoDestroy(sf->chars[i]->charinfo);
    }
    if ( sf->subfontcnt!=0 ) {
	for ( j=0; j<sf->subfontcnt; ++j ) {
	    for ( i=0; i<sf->subfonts[j]->charcnt; ++i ) if ( sf->subfonts[j]->chars[i]!=NULL ) {
		CharView *cv, *next;
		for ( cv = sf->subfonts[j]->chars[i]->views; cv!=NULL; cv = next ) {
		    next = cv->next;
		    GDrawDestroyWindow(cv->gw);
		}
	    }
	}
    }
    for ( bdf = sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	for ( i=0; i<bdf->charcnt; ++i ) if ( bdf->chars[i]!=NULL ) {
	    BitmapView *bv, *next;
	    for ( bv = bdf->chars[i]->views; bv!=NULL; bv = next ) {
		next = bv->next;
		GDrawDestroyWindow(bv->gw);
	    }
	}
    }
    for ( mv=fv->metrics; mv!=NULL; mv = mnext ) {
	mnext = mv->next;
	GDrawDestroyWindow(mv->gw);
    }
    if ( fv->fontinfo!=NULL )
	FontInfoDestroy(fv);
    SVDetachFV(fv);
    if ( sf->filename!=NULL )
	RecentFilesRemember(sf->filename);
    GDrawDestroyWindow(fv->gw);
return( true );
}

void MenuNew(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontNew();
}

static void FVMenuClose(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    DelayEvent((void (*)(void *)) _FVMenuClose, fv);
}

static void FVReattachCVs(SplineFont *old,SplineFont *new) {
    int i, j, enc;
    CharView *cv, *cvnext;
    SplineFont *sub;

    for ( i=0; i<old->charcnt; ++i ) {
	if ( old->chars[i]!=NULL && old->chars[i]->views!=NULL ) {
	    if ( new->subfontcnt==0 ) {
		enc = SFFindExistingChar(new,old->chars[i]->unicodeenc,old->chars[i]->name);
		sub = new;
	    } else {
		enc = -1;
		for ( j=0; j<new->subfontcnt && enc==-1 ; ++j ) {
		    sub = new->subfonts[j];
		    enc = SFFindExistingChar(sub,old->chars[i]->unicodeenc,old->chars[i]->name);
		}
	    }
	    if ( enc==-1 ) {
		for ( cv=old->chars[i]->views; cv!=NULL; cv = cvnext ) {
		    cvnext = cv->next;
		    GDrawDestroyWindow(cv->gw);
		}
	    } else {
		for ( cv=old->chars[i]->views; cv!=NULL; cv = cvnext ) {
		    cvnext = cv->next;
		    CVChangeSC(cv,sub->chars[enc]);
		    cv->heads[dm_grid] = &new->gridsplines;
		}
	    }
	    GDrawProcessPendingEvents(NULL);
	}
    }
}

void FVRevert(FontView *fv) {
    SplineFont *temp, *old = fv->cidmaster?fv->cidmaster:fv->sf;
    BDFFont *tbdf, *fbdf;
    BDFChar *bc;
    BitmapView *bv, *bvnext;
    MetricsView *mv, *mvnext;
    int i, enc;
    FontView *fvs;

    if ( old->origname==NULL )
return;
    if ( old->changed )
	if ( !RevertAskChanged(old->fontname,old->origname))
return;
    temp = ReadSplineFont(old->origname,0);
    if ( temp==NULL ) {
return;
    }
    FVReattachCVs(old,temp);
    for ( i=0; i<old->subfontcnt; ++i )
	FVReattachCVs(old->subfonts[i],temp);
    for ( fbdf=old->bitmaps; fbdf!=NULL; fbdf=fbdf->next ) {
	for ( tbdf=temp->bitmaps; tbdf!=NULL; tbdf=tbdf->next )
	    if ( tbdf->pixelsize==fbdf->pixelsize )
	break;
	for ( i=0; i<fv->sf->charcnt; ++i ) {
	    if ( fbdf->chars[i]!=NULL && fbdf->chars[i]->views!=NULL ) {
		enc = SFFindChar(temp,fv->sf->chars[i]->unicodeenc,fv->sf->chars[i]->name);
		bc = enc==-1 || tbdf==NULL?NULL:tbdf->chars[enc];
		if ( bc==NULL ) {
		    for ( bv=fbdf->chars[i]->views; bv!=NULL; bv = bvnext ) {
			bvnext = bv->next;
			GDrawDestroyWindow(bv->gw);
		    }
		} else {
		    for ( bv=fbdf->chars[i]->views; bv!=NULL; bv = bvnext ) {
			bvnext = bv->next;
			BVChangeBC(bv,bc,true);
		    }
		}
		GDrawProcessPendingEvents(NULL);
	    }
	}
    }
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	for ( mv=fvs->metrics; mv!=NULL; mv = mvnext ) {
	    /* Don't bother trying to fix up metrics views, just not worth it */
	    mvnext = mv->next;
	    GDrawDestroyWindow(mv->gw);
	}
	if ( fvs->fontinfo )
	    FontInfoDestroy(fvs);
    }
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    if ( temp->charcnt>fv->sf->charcnt ) {
	fv->selected = grealloc(fv->selected,temp->charcnt);
	for ( i=fv->sf->charcnt; i<temp->charcnt; ++i )
	    fv->selected[i] = false;
    }
    SFClearAutoSave(old);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    temp->fv = fv->sf->fv;
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	fvs->sf = temp;
    FontViewReformatAll(fv->sf);
    SplineFontFree(old);
}

static void FVMenuRevert(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVRevert(fv);
}

static void FVMenuRevertGlyph(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i;
    int nc_state = -1, refs_state=-1, ret;
    SplineFont *sf = fv->sf;
    SplineChar *sc, *tsc;
    SplineChar temp;
    static int buts[] = { _STR_Yes, _STR_YesToAll, _STR_NoToAll, _STR_No, _STR_Cancel, 0 };

    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] && sf->chars[i]!=NULL ) {
	tsc = sf->chars[i];
	if ( tsc->namechanged ) {
	    if ( nc_state==-1 ) {
		GWidgetErrorR(_STR_NameChanged,_STR_NameChangedGlyph,tsc->name);
		nc_state = 0;
	    }
	} else {
	    sc = SFDReadOneChar(sf->filename,tsc->name);
	    if ( sc==NULL ) {
		GWidgetErrorR(_STR_CantFindGlyph,_STR_CantRevertGlyph,tsc->name);
		tsc->namechanged = true;
	    } else {
		if ( sc->refs!=NULL && sf->encodingchanged ) {
		    if ( refs_state==-1 ) {
			ret = GWidgetAskR(_STR_GlyphHasRefs,buts,0,1,_STR_GlyphHasRefsQuestion,tsc->name);
			if ( ret == 1 || ret == 2 )
			    refs_state = ret;
		    } else
			ret = refs_state;
		    if ( ret >= 2 ) 	/* No, No to All, Cancel */
			SplineCharFree(sc);
		    if ( ret==4 )		/* Cancel */
return;
		} else
		    ret = 0;
		if ( ret==0 || ret==1 ) {
		    SCPreserveState(tsc,true);
		    SCPreserveBackground(tsc);
		    temp = *tsc;
		    tsc->dependents = NULL;
		    tsc->undoes[0] = tsc->undoes[1] = NULL;
		    SplineCharFreeContents(tsc);
		    *tsc = *sc;
		    chunkfree(sc,sizeof(SplineChar));
		    tsc->parent = sf;
		    tsc->dependents = temp.dependents;
		    tsc->undoes[0] = temp.undoes[0];
		    tsc->undoes[1] = temp.undoes[1];
		    tsc->views = temp.views;
		    tsc->changed = temp.changed;
		    tsc->enc = temp.enc;
		    RevertedGlyphReferenceFixup(tsc, sf);
		    _SCCharChangedUpdate(tsc,false);
		}
	    }
	}
    }
}

void MenuPrefs(GWindow base,struct gmenuitem *mi,GEvent *e) {
    DoPrefs();
}

void MenuSaveAll(GWindow base,struct gmenuitem *mi,GEvent *e) {
    FontView *fv;

    for ( fv = fv_list; fv!=NULL; fv = fv->next ) {
	if ( fv->sf->changed && !_FVMenuSave(fv))
return;
    }
}

static void _MenuExit(void *junk) {
    FontView *fv, *next;

    for ( fv = fv_list; fv!=NULL; fv = next ) {
	next = fv->next;
	if ( !_FVMenuClose(fv))
return;
	if ( fv->nextsame!=NULL || fv->sf->fv!=fv ) {
	    GDrawSync(NULL);
	    GDrawProcessPendingEvents(NULL);
	}
    }
    exit(0);
}

static void FVMenuExit(GWindow base,struct gmenuitem *mi,GEvent *e) {
    _MenuExit(NULL);
}

void MenuExit(GWindow base,struct gmenuitem *mi,GEvent *e) {
    if ( e==NULL )	/* Not from the menu directly, but a shortcut */
	_MenuExit(NULL);
    else
	DelayEvent(_MenuExit,NULL);
}

char *GetPostscriptFontName(char *dir, int mult) {
    /* Some people use pf3 as an extension for postscript type3 fonts */
    static unichar_t fontmacsuit[] = { 'a','p','p','l','i','c','a','t','i','o','n','/','x','-','m','a','c','-','s','u','i','t', '\0' };
    static unichar_t wild[] = { '*', '.', '{', 'p','f','a',',',
					       'p','f','b',',',
			                       's','f','d',',',
			                       't','t','f',',',
			                       'b','d','f',',',
			                       'o','t','f',',',
			                       'o','t','b',',',
#ifndef _NO_LIBXML
			                       's','v','g',',',
#endif
			                       'p','f','3',',',
			                       't','t','c',',',
			                       'g','s','f',',',
			                       'c','i','d',',',
			                       'b','i','n',',',
			                       'h','q','x',',',
			                       'd','f','o','n','t',',',
			                       'm','f',',',
			                       'i','k',',',
			                       'f','o','n',',',
			                       'f','n','t','}', 
	     '{','.','g','z',',','.','Z',',','.','b','z','2',',','}',  '\0' };
    static unichar_t *mimes[] = { fontmacsuit, NULL };
    unichar_t *ret, *u_dir;
    char *temp;

    u_dir = uc_copy(dir);
    ret = FVOpenFont(GStringGetResource(_STR_OpenPostscript,NULL),
	    u_dir,wild,mimes,mult,true);
    temp = u2def_copy(ret);

    free(ret);
return( temp );
}

void MergeKernInfo(SplineFont *sf) {
#ifndef __Mac
    static unichar_t wild[] = { '*', '.', '{','a','f','m',',','t','f','m',',','b','i','n',',','h','q','x','}',  '\0' };
#else
    static unichar_t wild[] = { '*', 0 };	/* Mac resource files generally don't have extensions */
#endif
    unichar_t *ret = GWidgetOpenFile(GStringGetResource(_STR_MergeKernInfo,NULL),NULL,wild,NULL,NULL);
    char *temp = u2def_copy(ret);
    int isafm, istfm;

    if ( temp==NULL )		/* Cancelled */
return;

    isafm = strstrmatch(temp,".afm")!=NULL;
    istfm = strstrmatch(temp,".tfm")!=NULL;
    if ( (isafm && !LoadKerningDataFromAfm(sf,temp)) ||
	    (istfm && !LoadKerningDataFromTfm(sf,temp)) ||
	    (!isafm && !istfm && !LoadKerningDataFromMacFOND(sf,temp)) )
	GDrawError( "Failed to load kern data from %s", temp);
    free(ret); free(temp);
}

static void FVMenuMergeKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    MergeKernInfo(fv->sf);
}

void MenuOpen(GWindow base,struct gmenuitem *mi,GEvent *e) {
    char *temp;
    char *eod, *fpt, *file, *full;
    FontView *test; int fvcnt, fvtest;

    for ( fvcnt=0, test=fv_list; test!=NULL; ++fvcnt, test=test->next );
    do {
	temp = GetPostscriptFontName(NULL,true);
	if ( temp==NULL )
return;
	eod = strrchr(temp,'/');
	*eod = '\0';
	file = eod+1;
	do {
	    fpt = strstr(file,"; ");
	    if ( fpt!=NULL ) *fpt = '\0';
	    full = galloc(strlen(temp)+1+strlen(file)+1);
	    strcpy(full,temp); strcat(full,"/"); strcat(full,file);
	    ViewPostscriptFont(full);
	    file = fpt+2;
	} while ( fpt!=NULL );
	free(temp);
	for ( fvtest=0, test=fv_list; test!=NULL; ++fvtest, test=test->next );
    } while ( fvtest==fvcnt );	/* did the load fail for some reason? try again */
}

void MenuHelp(GWindow base,struct gmenuitem *mi,GEvent *e) {
    help("overview.html");
}

void MenuIndex(GWindow base,struct gmenuitem *mi,GEvent *e) {
    help("IndexFS.html");
}

void MenuLicense(GWindow base,struct gmenuitem *mi,GEvent *e) {
    help("license.html");
}

void MenuAbout(GWindow base,struct gmenuitem *mi,GEvent *e) {
    ShowAboutScreen();
}

static void FVMenuImport(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVImport(fv);
}

static int FVSelCount(FontView *fv) {
    int i, cnt=0;
    static int buts[] = { _STR_OK, _STR_Cancel, 0 };

    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] ) ++cnt;
    if ( cnt>10 ) {
	if ( GWidgetAskR(_STR_Manywin,buts,0,1,_STR_Toomany)==1 )
return( false );
    }
return( true );
}
	
static void FVMenuOpenOutline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i;
    SplineChar *sc;

    if ( !FVSelCount(fv))
return;
    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] ) {
	    sc = SFMakeChar(fv->sf,i);
	    CharViewCreate(sc,fv);
	}
}

static void FVMenuOpenBitmap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i;

    if ( fv->cidmaster==NULL ? (fv->sf->bitmaps==NULL) : (fv->cidmaster->bitmaps==NULL) )
return;
    if ( !FVSelCount(fv))
return;
    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] ) {
	    if ( fv->sf->chars[i]==NULL )
		SFMakeChar(fv->cidmaster?fv->cidmaster:fv->sf,i);
	    BitmapViewCreatePick(i,fv);
	}
}

static void FVMenuOpenMetrics(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    MetricsViewCreate(fv,NULL,fv->filled==fv->show?NULL:fv->show);
}

static void FVMenuFontInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FontMenuFontInfo(fv);
}

static void FVMenuFindProblems(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FindProblems(fv,NULL,NULL);
}

static void FVMenuMetaFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    MetaFont(fv,NULL,NULL);
}

#define MID_24	2001
#define MID_36	2002
#define MID_48	2004
#define MID_72	2014
#define MID_96	2015
#define MID_AntiAlias	2005
#define MID_Next	2006
#define MID_Prev	2007
#define MID_NextDef	2012
#define MID_PrevDef	2013
#define MID_ShowHMetrics 2016
#define MID_ShowVMetrics 2017
#define MID_CompactedView 2018
#define MID_EncodedView 2019
#define MID_Ligatures	2020
#define MID_KernPairs	2021
#define MID_AnchorPairs	2022
#define MID_FitToEm	2022
#define MID_CharInfo	2201
#define MID_FindProblems 2216
#define MID_MetaFont	2217
#define MID_Transform	2202
#define MID_Stroke	2203
#define MID_RmOverlap	2204
#define MID_Simplify	2205
#define MID_Correct	2206
#define MID_BuildAccent	2208
#define MID_AvailBitmaps	2210
#define MID_RegenBitmaps	2211
#define MID_Autotrace	2212
#define MID_Round	2213
#define MID_MergeFonts	2214
#define MID_InterpolateFonts	2215
#define MID_ShowDependents	2222
#define MID_AddExtrema	2224
#define MID_CleanupChar	2225
#define MID_TilePath	2226
#define MID_BuildComposite	2227
#define MID_NLTransform	2228
#define MID_Intersection	2229
#define MID_FindInter	2230
#define MID_Effects	2231
#define MID_Center	2600
#define MID_Thirds	2601
#define MID_SetWidth	2602
#define MID_SetLBearing	2603
#define MID_SetRBearing	2604
#define MID_SetVWidth	2605
#define MID_RmHKern	2606
#define MID_RmVKern	2607
#define MID_VKernByClass	2608
#define MID_VKernFromH	2609
#define MID_AutoHint	2501
#define MID_ClearHints	2502
#define MID_ClearWidthMD	2503
#define MID_AutoInstr	2504
#define MID_EditInstructions	2505
#define MID_Editfpgm	2506
#define MID_Editprep	2507
#define MID_ClearInstrs	2508
#define MID_HStemHist	2509
#define MID_VStemHist	2510
#define MID_BlueValuesHist	2511
#define MID_OpenBitmap	2700
#define MID_OpenOutline	2701
#define MID_Revert	2702
#define MID_Recent	2703
#define MID_Print	2704
#define MID_ScriptMenu	2705
#define MID_Display	2706
#define MID_RevertGlyph	2707
#define MID_Cut		2101
#define MID_Copy	2102
#define MID_Paste	2103
#define MID_Clear	2104
#define MID_SelAll	2106
#define MID_CopyRef	2107
#define MID_UnlinkRef	2108
#define MID_Undo	2109
#define MID_Redo	2110
#define MID_CopyWidth	2111
#define MID_AllFonts		2122
#define MID_DisplayedFont	2123
#define	MID_CharName		2124
#define MID_RemoveUndoes	2114
#define MID_CopyFgToBg	2115
#define MID_ClearBackground	2116
#define MID_CopyLBearing	2125
#define MID_CopyRBearing	2126
#define MID_CopyVWidth	2127
#define MID_Join	2128
#define MID_PasteInto	2129
#define MID_SameGlyphAs	2130
#define MID_Convert2CID	2800
#define MID_Flatten	2801
#define MID_InsertFont	2802
#define MID_InsertBlank	2803
#define MID_CIDFontInfo	2804
#define MID_RemoveFromCID 2805
#define MID_ConvertByCMap	2806
#define MID_FlattenByCMap	2807
#define MID_ModifyComposition	20902
#define MID_BuildSyllables	20903

/* returns -1 if nothing selected, the char if exactly one, -2 if more than one */
static int FVAnyCharSelected(FontView *fv) {
    int i, val=-1;

    for ( i=0; i<fv->sf->charcnt; ++i ) {
	if ( fv->selected[i]) {
	    if ( val==-1 )
		val = i;
	    else
return( -2 );
	}
    }
return( val );
}

static void FVMenuCopyFrom(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    /*FontView *fv = (FontView *) GDrawGetUserData(gw);*/

    if ( mi->mid==MID_CharName )
	copymetadata = !copymetadata;
    else
	onlycopydisplayed = (mi->mid==MID_DisplayedFont);
    SavePrefs();
}

static void FVMenuCopy(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopy(fv,true);
}

static void FVMenuCopyRef(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopy(fv,false);
}

static void FVMenuCopyWidth(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( mi->mid==MID_CopyVWidth && !fv->sf->hasvmetrics )
return;
    FVCopyWidth(fv,mi->mid==MID_CopyWidth?ut_width:
		   mi->mid==MID_CopyVWidth?ut_vwidth:
		   mi->mid==MID_CopyLBearing?ut_lbearing:
					 ut_rbearing);
}

static void FVMenuPaste(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    PasteIntoFV(fv,true);
}

static void FVMenuPasteInto(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    PasteIntoFV(fv,false);
}

static void FVSameGlyphAs(FontView *fv) {
    SplineFont *sf = fv->sf;
    RefChar *base = CopyContainsRef(sf);
    int i;

    if ( FVAnyCharSelected(fv)==-1 || base==NULL || fv->cidmaster!=NULL )
return;
    PasteIntoFV(fv,true);
    for ( i=0; i<sf->charcnt; ++i )
	    if ( sf->chars[i]!=NULL && fv->selected[i] && base->local_enc!=i) {
	free(sf->chars[i]->name);
	sf->chars[i]->name = copy(sf->chars[base->local_enc]->name);
    }
}

static void FVMenuSameGlyphAs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVSameGlyphAs(fv);
}

static void FVCopyFgtoBg(FontView *fv) {
    int i;

    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->sf->chars[i]!=NULL && fv->selected[i] && fv->sf->chars[i]->splines!=NULL )
	    SCCopyFgToBg(fv->sf->chars[i],true);
}

static void FVMenuCopyFgBg(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVCopyFgtoBg( (FontView *) GDrawGetUserData(gw));
}

void BCClearAll(BDFChar *bc) {
    if ( bc==NULL )
return;
    BCPreserveState(bc);
    BCFlattenFloat(bc);
    memset(bc->bitmap,'\0',bc->bytes_per_line*(bc->ymax-bc->ymin+1));
    BCCompressBitmap(bc);
    BCCharChangedUpdate(bc);
}

void SCClearAll(SplineChar *sc) {
    RefChar *refs, *next;

    if ( sc==NULL )
return;
    if ( sc->splines==NULL && sc->refs==NULL && !sc->widthset &&
	    sc->hstem==NULL && sc->vstem==NULL && sc->anchor==NULL &&
	    (!copymetadata ||
		(sc->unicodeenc==-1 && strcmp(sc->name,".notdef")==0)))
return;
    SCPreserveState(sc,2);
    if ( copymetadata ) {
	sc->unicodeenc = -1;
	free(sc->name);
	sc->name = copy(".notdef");
	PSTFree(sc->possub);
	sc->possub = NULL;
    }
    sc->widthset = false;
    sc->width = sc->parent->ascent+sc->parent->descent;
    SplinePointListsFree(sc->splines);
    sc->splines = NULL;
    AnchorPointsFree(sc->anchor);
    sc->anchor = NULL;
    for ( refs=sc->refs; refs!=NULL; refs = next ) {
	next = refs->next;
	SCRemoveDependent(sc,refs);
    }
    sc->refs = NULL;
    StemInfosFree(sc->hstem); sc->hstem = NULL;
    StemInfosFree(sc->vstem); sc->vstem = NULL;
    DStemInfosFree(sc->dstem); sc->dstem = NULL;
    MinimumDistancesFree(sc->md); sc->md = NULL;
    free(sc->ttf_instrs);
    sc->ttf_instrs = NULL;
    sc->ttf_instrs_len = 0;
    SCOutOfDateBackground(sc);
    SCCharChangedUpdate(sc);
}

void SCClearBackground(SplineChar *sc) {

    if ( sc==NULL )
return;
    if ( sc->backgroundsplines==NULL && sc->backimages==NULL )
return;
    SCPreserveBackground(sc);
    SplinePointListsFree(sc->backgroundsplines);
    sc->backgroundsplines = NULL;
    ImageListsFree(sc->backimages);
    sc->backimages = NULL;
    SCOutOfDateBackground(sc);
    SCCharChangedUpdate(sc);
}

static int UnselectedDependents(FontView *fv, SplineChar *sc) {
    struct splinecharlist *dep;

    for ( dep=sc->dependents; dep!=NULL; dep=dep->next ) {
	if ( !fv->selected[dep->sc->enc] )
return( true );
	if ( UnselectedDependents(fv,dep->sc))
return( true );
    }
return( false );
}

void UnlinkThisReference(FontView *fv,SplineChar *sc) {
    /* We are about to clear out sc. But somebody refers to it and that we */
    /*  aren't going to delete. So (if the user asked us to) instanciate sc */
    /*  into all characters which refer to it and which aren't about to be */
    /*  cleared out */
    struct splinecharlist *dep, *dnext;

    for ( dep=sc->dependents; dep!=NULL; dep=dnext ) {
	dnext = dep->next;
	if ( fv==NULL || !fv->selected[dep->sc->enc]) {
	    SplineChar *dsc = dep->sc;
	    RefChar *rf, *rnext;
	    /* May be more than one reference to us, colon has two refs to period */
	    /*  but only one dlist entry */
	    for ( rf = dsc->refs; rf!=NULL; rf=rnext ) {
		rnext = rf->next;
		if ( rf->sc == sc ) {
		    /* Even if we were to preserve the state there would be no */
		    /*  way to undo the operation until we undid the delete... */
		    SCRefToSplines(dsc,rf);
		    SCUpdateAll(dsc);
		}
	    }
	}
    }
}

static void FVClear(FontView *fv) {
    int i;
    BDFFont *bdf;
    int refstate = 0;
    static int buts[] = { _STR_Yes, _STR_YesToAll, _STR_UnlinkAll, _STR_NoToAll, _STR_No, 0 };
    int yes, unsel;
    /* refstate==0 => ask, refstate==1 => clearall, refstate==-1 => skip all */

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
	if ( !onlycopydisplayed || fv->filled==fv->show ) {
	    /* If we are messing with the outline character, check for dependencies */
	    if ( refstate<=0 && fv->sf->chars[i]!=NULL &&
		    fv->sf->chars[i]->dependents!=NULL ) {
		unsel = UnselectedDependents(fv,fv->sf->chars[i]);
		if ( refstate==-2 && unsel ) {
		    UnlinkThisReference(fv,fv->sf->chars[i]);
		} else if ( unsel ) {
		    if ( refstate<0 )
    continue;
		    yes = GWidgetAskCenteredR(_STR_BadReference,buts,2,4,_STR_ClearDependent,fv->sf->chars[i]->name);
		    if ( yes==1 )
			refstate = 1;
		    else if ( yes==2 ) {
			UnlinkThisReference(fv,fv->sf->chars[i]);
			refstate = -2;
		    } else if ( yes==3 )
			refstate = -1;
		    if ( yes>=3 )
    continue;
		}
	    }
	}

	if ( onlycopydisplayed && fv->filled==fv->show ) {
	    SCClearAll(fv->sf->chars[i]);
	} else if ( onlycopydisplayed ) {
	    BCClearAll(fv->show->chars[i]);
	} else {
	    SCClearAll(fv->sf->chars[i]);
	    for ( bdf=fv->sf->bitmaps; bdf!=NULL; bdf = bdf->next )
		BCClearAll(bdf->chars[i]);
	}
    }
}

static void FVMenuClear(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVClear((FontView *) GDrawGetUserData(gw));
}

static void FVClearBackground(FontView *fv) {
    SplineFont *sf = fv->sf;
    int i;

    if ( onlycopydisplayed && fv->filled!=fv->show )
return;

    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] && sf->chars[i]!=NULL ) {
	SCClearBackground(sf->chars[i]);
    }
}

static void FVMenuClearBackground(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVClearBackground( (FontView *) GDrawGetUserData(gw) );
}

static void FVJoin(FontView *fv) {
    SplineFont *sf = fv->sf;
    int i,changed;
    extern float joinsnap;

    if ( onlycopydisplayed && fv->filled!=fv->show )
return;

    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] && sf->chars[i]!=NULL ) {
	SCPreserveState(sf->chars[i],false);
	sf->chars[i]->splines = SplineSetJoin(sf->chars[i]->splines,true,joinsnap,&changed);
	if ( changed )
	    SCCharChangedUpdate(sf->chars[i]);
    }
}

static void FVMenuJoin(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVJoin( (FontView *) GDrawGetUserData(gw) );
}

static void FVUnlinkRef(FontView *fv) {
    int i;
    SplineChar *sc;
    RefChar *rf, *next;

    for ( i=0; i<fv->sf->charcnt; ++i )
	    if ( fv->selected[i] && (sc=fv->sf->chars[i])!=NULL && sc->refs!=NULL ) {
	SCPreserveState(sc,false);
	for ( rf=sc->refs; rf!=NULL ; rf=next ) {
	    next = rf->next;
	    SCRefToSplines(sc,rf);
	}
	SCCharChangedUpdate(sc);
    }
}

static void FVMenuUnlinkRef(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVUnlinkRef( (FontView *) GDrawGetUserData(gw));
}

void SFRemoveUndoes(SplineFont *sf,uint8 *selected) {
    SplineFont *main = sf->cidmaster? sf->cidmaster : sf, *ssf;
    int i,k, max;
    SplineChar *sc;
    BDFFont *bdf;

    if ( selected!=NULL || main->subfontcnt==0 )
	max = sf->charcnt;
    else {
	max = 0;
	for ( k=0; k<main->subfontcnt; ++k )
	    if ( main->subfonts[k]->charcnt>max ) max = main->subfonts[k]->charcnt;
    }
    for ( i=0; i<max; ++i ) if ( selected==NULL || selected[i] ) {
	for ( bdf=main->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	    if ( bdf->chars[i]!=NULL ) {
		UndoesFree(bdf->chars[i]->undoes); bdf->chars[i]->undoes = NULL;
		UndoesFree(bdf->chars[i]->redoes); bdf->chars[i]->redoes = NULL;
	    }
	}
	k = 0;
	do {
	    ssf = main->subfontcnt==0? main: main->subfonts[k];
	    if ( i<ssf->charcnt && ssf->chars[i]!=NULL ) {
		sc = ssf->chars[i];
		UndoesFree(sc->undoes[0]); sc->undoes[0] = NULL;
		UndoesFree(sc->undoes[1]); sc->undoes[1] = NULL;
		UndoesFree(sc->redoes[0]); sc->redoes[0] = NULL;
		UndoesFree(sc->redoes[1]); sc->redoes[1] = NULL;
	    }
	    ++k;
	} while ( k<main->subfontcnt );
    }
}

static void FVMenuRemoveUndoes(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFRemoveUndoes(fv->sf,fv->selected);
}

static void FVMenuUndo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i;
    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] && fv->sf->chars[i]!=NULL && fv->sf->chars[i]->undoes[0]!=NULL )
	    SCDoUndo(fv->sf->chars[i],dm_fore);
}

static void FVMenuRedo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i;
    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->selected[i] && fv->sf->chars[i]!=NULL && fv->sf->chars[i]->redoes[0]!=NULL )
	    SCDoRedo(fv->sf->chars[i],dm_fore);
}

static void FVMenuCut(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVCopy(fv,true);
    FVClear(fv);
}

static void FVMenuSelectAll(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVSelectAll(fv);
}

static void FVMenuDeselectAll(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVDeselectAll(fv);
}

static void FVMenuSelectColor(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVSelectColor(fv,(uint32) (mi->ti.userdata),(e->u.chr.state&ksm_shift)?1:0);
}

static void FVMenuSelectByPST(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVSelectByPST(fv);
}

static void FVMenuFindRpl(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    SVCreate(fv);
}

static void cflistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    /*FontView *fv = (FontView *) GDrawGetUserData(gw);*/

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_AllFonts:
	    mi->ti.checked = !onlycopydisplayed;
	  break;
	  case MID_DisplayedFont:
	    mi->ti.checked = onlycopydisplayed;
	  break;
	  case MID_CharName:
	    mi->ti.checked = copymetadata;
	  break;
	}
    }
}

static void sllistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    fv = fv;
}

static void edlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int pos = FVAnyCharSelected(fv);
    int not_pasteable = pos==-1 ||
		    (!CopyContainsSomething() &&
#ifndef _NO_LIBPNG
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/png") &&
#endif
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/bmp") &&
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/eps"));
    RefChar *base = CopyContainsRef(fv->sf);


    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Paste: case MID_PasteInto:
	    mi->ti.disabled = not_pasteable;
	  break;
	  case MID_SameGlyphAs:
	    mi->ti.disabled = not_pasteable || base==NULL || fv->cidmaster!=NULL ||
		    fv->selected[base->local_enc];	/* Can't be self-referential */
	  break;
	  case MID_Join:
	  case MID_Cut: case MID_Copy: case MID_Clear:
	  case MID_CopyWidth: case MID_CopyLBearing: case MID_CopyRBearing:
	  case MID_CopyRef: case MID_UnlinkRef:
	  case MID_RemoveUndoes: case MID_CopyFgToBg:
	    mi->ti.disabled = pos==-1;
	  break;
	  case MID_CopyVWidth: 
	    mi->ti.disabled = pos==-1 || !fv->sf->hasvmetrics;
	  break;
	  case MID_ClearBackground:
	    mi->ti.disabled = true;
	    if ( pos!=-1 && !( onlycopydisplayed && fv->filled!=fv->show )) {
		for ( pos=0; pos<fv->sf->charcnt; ++pos )
		    if ( fv->selected[pos] && fv->sf->chars[pos]!=NULL )
			if ( fv->sf->chars[pos]->backimages!=NULL ||
				fv->sf->chars[pos]->backgroundsplines!=NULL ) {
			    mi->ti.disabled = false;
		break;
			}
	    }
	  break;
	  case MID_Undo:
	    for ( pos=0; pos<fv->sf->charcnt; ++pos )
		if ( fv->selected[pos] && fv->sf->chars[pos]!=NULL )
		    if ( fv->sf->chars[pos]->undoes[0]!=NULL )
	    break;
	    mi->ti.disabled = pos==fv->sf->charcnt;
	  break;
	  case MID_Redo:
	    for ( pos=0; pos<fv->sf->charcnt; ++pos )
		if ( fv->selected[pos] && fv->sf->chars[pos]!=NULL )
		    if ( fv->sf->chars[pos]->redoes[0]!=NULL )
	    break;
	    mi->ti.disabled = pos==fv->sf->charcnt;
	  break;
	}
    }
}

static void FVMenuCharInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int pos = FVAnyCharSelected(fv);
    if ( pos<0 || fv->cidmaster!=NULL )
return;
    SFMakeChar(fv->sf,pos);
    SCCharInfo(fv->sf->chars[pos]);
}

static void FVMenuAAT(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    uint32 tag = (uint32) (mi->ti.userdata);
    int i;

    for ( i=0; i<fv->sf->charcnt; ++i )
	if ( fv->sf->chars[i]!=NULL && fv->selected[i])
	    SCTagDefault(SCDuplicate(fv->sf->chars[i]),tag);
}

static void FVMenuAATSuffix(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    uint32 tag;
    char *suffix;
    unichar_t *usuffix, *upt, *end;
    int i;
    uint16 flags, sli;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i])
	if ( SCScriptFromUnicode(fv->sf->chars[i])!=0 )
    break;
    usuffix = AskNameTag(_STR_SuffixToTag,NULL,0,0,-1,pst_substitution,fv->sf,fv->sf->chars[i],-1,-1);
    if ( usuffix==NULL )
return;

    upt = usuffix;
    tag  = (*upt++&0xff)<<24;
    tag |= (*upt++&0xff)<<16;
    tag |= (*upt++&0xff)<< 8;
    tag |= (*upt++&0xff)    ;
    if ( *upt==' ' ) ++upt;
    if (( upt[0]=='b' || upt[0]==' ' ) &&
	    ( upt[1]=='l' || upt[1]==' ' ) &&
	    ( upt[2]=='m' || upt[2]==' ' ) &&
	    ( upt[3]=='m' || upt[3]==' ' ) &&
	    upt[4]==' ' ) {
	flags = 0;
	if ( upt[0]=='r' ) flags |= pst_r2l;
	if ( upt[1]=='b' ) flags |= pst_ignorebaseglyphs;
	if ( upt[2]=='l' ) flags |= pst_ignoreligatures;
	if ( upt[3]=='m' ) flags |= pst_ignorecombiningmarks;
	upt += 5;
    }
    sli = u_strtol(upt,&end,10);

    suffix = cu_copy(end);
    free(usuffix);
    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i])
	SCSuffixDefault(SCDuplicate(fv->sf->chars[i]),tag,suffix,flags);
    free(suffix);
}

static void FVMenuShowDependents(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int pos = FVAnyCharSelected(fv);
    if ( pos<0 )
return;
    if ( fv->sf->chars[pos]==NULL || fv->sf->chars[pos]->dependents==NULL )
return;
    SCRefBy(fv->sf->chars[pos]);
}

static int getorigin(void *d,BasePoint *base,int index) {
    /*FontView *fv = (FontView *) d;*/

    base->x = base->y = 0;
    switch ( index ) {
      case 0:		/* Character origin */
	/* all done */
      break;
      case 1:		/* Center of selection */
	/*CVFindCenter(cv,base,!CVAnySel(cv,NULL,NULL,NULL,NULL));*/
      break;
      default:
return( false );
    }
return( true );
}

void TransHints(StemInfo *stem,real mul1, real off1, real mul2, real off2, int round_to_int ) {
    HintInstance *hi;

    for ( ; stem!=NULL; stem=stem->next ) {
	stem->start = stem->start*mul1 + off1;
	stem->width *= mul1;
	if ( round_to_int ) {
	    stem->start = rint(stem->start);
	    stem->width = rint(stem->width);
	}
	for ( hi=stem->where; hi!=NULL; hi=hi->next ) {
	    hi->begin = hi->begin*mul2 + off2;
	    hi->end = hi->end*mul2 + off2;
	    if ( round_to_int ) {
		hi->begin = rint(hi->begin);
		hi->end = rint(hi->end);
	    }
	}
    }
}

void FVTrans(FontView *fv,SplineChar *sc,real transform[6], uint8 *sel,
	enum fvtrans_flags flags) {
    RefChar *refs;
    real t[6];

    SCPreserveState(sc,true);
    if ( transform[0]>0 && transform[3]>0 && transform[1]==0 && transform[2]==0 ) {
	int widthset = sc->widthset;
	SCSynchronizeWidth(sc,sc->width*transform[0]+transform[4],sc->width,fv);
	if ( flags&fvt_dontsetwidth ) sc->widthset = widthset;
    }
    SplinePointListTransform(sc->splines,transform,true);
    for ( refs = sc->refs; refs!=NULL; refs=refs->next ) {
	if ( sel!=NULL && sel[refs->sc->enc] ) {
	    /* if the character referred to is selected then it's going to */
	    /*  be scaled too (or will have been) so we don't want to scale */
	    /*  it twice */
	    t[4] = refs->transform[4]*transform[0] +
			refs->transform[5]*transform[2] +
			/*transform[4]*/0;
	    t[5] = refs->transform[4]*transform[1] +
			refs->transform[5]*transform[3] +
			/*transform[5]*/0;
	    t[0] = refs->transform[4]; t[1] = refs->transform[5];
	    refs->transform[4] = t[4];
	    refs->transform[5] = t[5];
	    /* Now update the splines to match */
	    t[4] -= t[0]; t[5] -= t[1];
	    if ( t[4]!=0 || t[5]!=0 ) {
		t[0] = t[3] = 1; t[1] = t[2] = 0;
		SplinePointListTransform(refs->splines,t,true);
	    }
	} else {
	    SplinePointListTransform(refs->splines,transform,true);
	    t[0] = refs->transform[0]*transform[0] +
			refs->transform[1]*transform[2];
	    t[1] = refs->transform[0]*transform[1] +
			refs->transform[1]*transform[3];
	    t[2] = refs->transform[2]*transform[0] +
			refs->transform[3]*transform[2];
	    t[3] = refs->transform[2]*transform[1] +
			refs->transform[3]*transform[3];
	    t[4] = refs->transform[4]*transform[0] +
			refs->transform[5]*transform[2] +
			transform[4];
	    t[5] = refs->transform[4]*transform[1] +
			refs->transform[5]*transform[3] +
			transform[5];
	    memcpy(refs->transform,t,sizeof(t));
	}
	SplineSetFindBounds(refs->splines,&refs->bb);
    }
    if ( transform[1]==0 && transform[2]==0 ) {
	TransHints(sc->hstem,transform[3],transform[5],transform[0],transform[4],flags&fvt_round_to_int);
	TransHints(sc->vstem,transform[0],transform[4],transform[3],transform[5],flags&fvt_round_to_int);
    }
    if ( flags&fvt_round_to_int )
	SCRound2Int(sc);
    if ( transform[0]==1 && transform[3]==1 && transform[1]==0 &&
	    transform[2]==0 && transform[5]==0 &&
	    transform[4]!=0 && 
	    sc->unicodeenc!=-1 && isalpha(sc->unicodeenc)) {
	SCUndoSetLBearingChange(sc,(int) rint(transform[4]));
	SCSynchronizeLBearing(sc,NULL,transform[4]);
    }
    if ( flags&fvt_dobackground ) {
	ImageList *img;
	SCPreserveBackground(sc);
	SplinePointListTransform(sc->backgroundsplines,transform,true);
	for ( img = sc->backimages; img!=NULL; img=img->next )
	    BackgroundImageTransform(sc, img, transform);
    }
    SCCharChangedUpdate(sc);
}

void FVTransFunc(void *_fv,real transform[6],int otype, BVTFunc *bvts,
	enum fvtrans_flags flags ) {
    FontView *fv = _fv;
    real transx = transform[4], transy=transform[5];
    DBounds bb;
    BasePoint base;
    int i, cnt=0;
    BDFFont *bdf;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_Transforming,_STR_Transforming,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];

	if ( onlycopydisplayed && fv->show!=fv->filled ) {
	    if ( fv->show->chars[i]!=NULL )
		BCTrans(bdf,fv->show->chars[i],bvts,fv);
	} else {
	    if ( otype==1 ) {
		SplineCharFindBounds(sc,&bb);
		base.x = (bb.minx+bb.maxx)/2;
		base.y = (bb.miny+bb.maxy)/2;
		transform[4]=transx+base.x-
		    (transform[0]*base.x+transform[2]*base.y);
		transform[5]=transy+base.y-
		    (transform[1]*base.x+transform[3]*base.y);
	    }
	    FVTrans(fv,sc,transform,fv->selected,flags);
	    if ( !onlycopydisplayed ) {
		for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next ) if ( bdf->chars[i]!=NULL )
		    BCTrans(bdf,bdf->chars[i],bvts,fv);
	    }
	}
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

int SFScaleToEm(SplineFont *sf, int as, int des) {
    double scale;
    real transform[6];
    BVTFunc bvts;
    uint8 *oldselected = sf->fv->selected;
    int i;
    KernPair *kp;
    PST *pst;

    if ( as+des == sf->ascent+sf->descent ) {
	if ( as!=sf->ascent && des!=sf->descent ) {
	    sf->ascent = as; sf->descent = des;
	    sf->changed = true;
	}
return( false );
    }

    scale = (as+des)/(double) (sf->ascent+sf->descent);
    transform[0] = transform[3] = scale;
    transform[1] = transform[2] = transform[4] = transform[5] = 0;
    bvts.func = bvt_none;
    sf->fv->selected = galloc(sf->charcnt);
    memset(sf->fv->selected,1,sf->charcnt);

    sf->ascent = as; sf->descent = des;

    FVTransFunc(sf->fv,transform,0,&bvts,
	    fvt_dobackground|fvt_round_to_int|fvt_dontsetwidth);
    free(sf->fv->selected);
    sf->fv->selected = oldselected;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	for ( kp=sf->chars[i]->kerns; kp!=NULL; kp=kp->next )
	    kp->off = rint(scale*kp->off);
	for ( pst=sf->chars[i]->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_position ) {
		pst->u.pos.xoff = rint(scale*pst->u.pos.xoff);
		pst->u.pos.yoff = rint(scale*pst->u.pos.yoff);
		pst->u.pos.h_adv_off = rint(scale*pst->u.pos.h_adv_off);
		pst->u.pos.v_adv_off = rint(scale*pst->u.pos.v_adv_off);
	    } else if ( pst->type==pst_pair ) {
		pst->u.pair.vr[0].xoff = rint(scale*pst->u.pair.vr[0].xoff);
		pst->u.pair.vr[0].yoff = rint(scale*pst->u.pair.vr[0].yoff);
		pst->u.pair.vr[0].h_adv_off = rint(scale*pst->u.pair.vr[0].h_adv_off);
		pst->u.pair.vr[0].v_adv_off = rint(scale*pst->u.pair.vr[0].v_adv_off);
		pst->u.pair.vr[1].xoff = rint(scale*pst->u.pair.vr[1].xoff);
		pst->u.pair.vr[1].yoff = rint(scale*pst->u.pair.vr[1].yoff);
		pst->u.pair.vr[1].h_adv_off = rint(scale*pst->u.pair.vr[1].h_adv_off);
		pst->u.pair.vr[1].v_adv_off = rint(scale*pst->u.pair.vr[1].v_adv_off);
	    }
	}
    }

    sf->changed = true;
return( true );
}

static void FVMenuBitmaps(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    BitmapDlg(fv,NULL,mi->mid==MID_AvailBitmaps );
}

static void FVMenuTransform(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    TransformDlgCreate(fv,FVTransFunc,getorigin,true);
}

#ifdef PFAEDIT_CONFIG_NONLINEAR
static void FVMenuNLTransform(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    NonLinearDlg(fv,NULL);
}
#endif

static void FVMenuStroke(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVStroke(fv);
}

#ifdef PFAEDIT_CONFIG_TILEPATH
static void FVMenuTilePath(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVTile(fv);
}
#endif

static void FVOverlap(FontView *fv,enum overlap_type ot) {
    int i, cnt=0;

    /* We know it's more likely that we'll find a problem in the overlap code */
    /*  than anywhere else, so let's save the current state against a crash */
    DoAutoSaves();

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_RemovingOverlap,_STR_RemovingOverlap,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	SCPreserveState(sc,false);
	MinimumDistancesFree(sc->md);
	sc->md = NULL;
	sc->splines = SplineSetRemoveOverlap(sc,sc->splines,ot);
	SCCharChangedUpdate(sc);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuOverlap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    /* We know it's more likely that we'll find a problem in the overlap code */
    /*  than anywhere else, so let's save the current state against a crash */
    DoAutoSaves();

    FVOverlap(fv,mi->mid==MID_RmOverlap ? over_remove :
		 mi->mid==MID_Intersection ? over_intersect :
		      over_findinter);
}

static void FVMenuInline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    OutlineDlg(fv,NULL,NULL,true);
}

static void FVMenuOutline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    OutlineDlg(fv,NULL,NULL,false);
}

static void FVMenuShadow(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    ShadowDlg(fv,NULL,NULL,false);
}

static void FVMenuWireframe(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    ShadowDlg(fv,NULL,NULL,true);
}

void _FVSimplify(FontView *fv,struct simplifyinfo *smpl) {
    int i, cnt=0;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_Simplifying,_STR_Simplifying,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	SCPreserveState(sc,false);
	sc->splines = SplineCharSimplify(sc,sc->splines,smpl);
	SCCharChangedUpdate(sc);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVSimplify(FontView *fv,int type) {
    static struct simplifyinfo smpl = { sf_normal,.75,.05,0 };

    smpl.err = (fv->sf->ascent+fv->sf->descent)/1000.;

    if ( type==1 ) {
	if ( !SimplifyDlg(fv->sf,&smpl))
return;
    }
    _FVSimplify(fv,&smpl);
}

static void FVMenuSimplify(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),false );
}

static void FVMenuSimplifyMore(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),true );
}

static void FVMenuCleanup(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),-1 );
}

static void FVAddExtrema(FontView *fv) {
    int i, cnt=0;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_AddingExtrema,_STR_AddingExtrema,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	SCPreserveState(sc,false);
	SplineCharAddExtrema(sc->splines,false);
	SCCharChangedUpdate(sc);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuAddExtrema(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVAddExtrema( (FontView *) GDrawGetUserData(gw) );
}

static void FVMenuCorrectDir(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, cnt=0, changed, refchanged;
    int askedall=-1, asked;
    static int buts[] = { _STR_UnlinkAll, _STR_Unlink, _STR_No, _STR_Cancel, 0 };
    RefChar *ref;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_CorrectingDirection,_STR_CorrectingDirection,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];

	changed = refchanged = false;
	asked = askedall;
	for ( ref=sc->refs; ref!=NULL; ref=ref->next ) {
	    if ( ref->transform[0]*ref->transform[3]<0 ||
		    (ref->transform[0]==0 && ref->transform[1]*ref->transform[2]>0)) {
		if ( asked==-1 ) {
		    asked = GWidgetAskR(_STR_FlippedRef,buts,0,2,_STR_FlippedRefUnlink, sc->name );
		    if ( asked==3 )
return;
		    else if ( asked==2 )
	break;
		    else if ( asked==0 )
			askedall = 0;
		}
		if ( asked==0 || asked==1 ) {
		    if ( !refchanged ) {
			refchanged = true;
			SCPreserveState(sc,false);
		    }
		    SCRefToSplines(sc,ref);
		}
	    }
	}

	if ( !refchanged )
	    SCPreserveState(sc,false);
	sc->splines = SplineSetsCorrect(sc->splines,&changed);
	if ( changed || refchanged )
	    SCCharChangedUpdate(sc);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVRound2Int(FontView *fv) {
    int i, cnt=0;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_Rounding,_STR_Rounding,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SCPreserveState(fv->sf->chars[i],false);
	SCRound2Int( fv->sf->chars[i]);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuRound2Int(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVRound2Int( (FontView *) GDrawGetUserData(gw) );
}

static void FVMenuAutotrace(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVAutoTrace(fv,e!=NULL && (e->u.mouse.state&ksm_shift));
}

void FVBuildAccent(FontView *fv,int onlyaccents) {
    int i, cnt=0;
    SplineChar dummy;
    static int buts[] = { _STR_Yes, _STR_No, 0 };

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_Buildingaccented,_STR_Buildingaccented,_STR_NULL,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	if ( sc==NULL )
	    sc = SCBuildDummy(&dummy,fv->sf,i);
	else if ( screen_display==NULL && sc->unicodeenc == 0x00c5 /* Aring */ &&
		sc->splines!=NULL ) {
	    if ( GWidgetAskR(_STR_Replacearing,buts,0,1,_STR_Areyousurearing)==1 )
    continue;
	}
	if ( SFIsSomethingBuildable(fv->sf,sc,onlyaccents) ) {
	    sc = SFMakeChar(fv->sf,i);
	    SCBuildComposit(fv->sf,sc,!onlycopydisplayed,fv);
	}
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuBuildAccent(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVBuildAccent( (FontView *) GDrawGetUserData(gw), true );
}

static void FVMenuBuildComposite(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVBuildAccent( (FontView *) GDrawGetUserData(gw), false );
}

int ScriptLangMatch(struct script_record *sr,uint32 script,uint32 lang) {
    int i, j;

    if ( script==CHR('*',' ',' ',' ') && lang==CHR('*',' ',' ',' ') )
return(true);	/* Wild card */

    for ( i=0; sr[i].script!=0; ++i ) {
	if ( sr[i].script==script || script==CHR('*',' ',' ',' ') ) {
	    for ( j=0; sr[i].langs[j]!=0 ; ++j )
		if ( sr[i].langs[j]==lang || lang==CHR('*',' ',' ',' ') )
return( true );
	    if ( sr[i].script==script )
return( false );
	}
    }
return( false );
}

static void SCReplaceWith(SplineChar *dest, SplineChar *src) {
    int enc=dest->enc, uenc=dest->unicodeenc, oenc = dest->old_enc;
    Undoes *u[2], *r1;
    SplineSet *back = dest->backgroundsplines;
    ImageList *images = dest->backimages;
    struct splinecharlist *scl = dest->dependents;
    RefChar *refs;

    if ( src==dest )
return;

    SCPreserveState(src,2);
    SCPreserveState(dest,2);
    u[0] = dest->undoes[0]; u[1] = dest->undoes[1]; r1 = dest->redoes[1];

    free(dest->name);
    SplinePointListsFree(dest->splines);
    RefCharsFree(dest->refs);
    StemInfosFree(dest->hstem);
    StemInfosFree(dest->vstem);
    DStemInfosFree(dest->dstem);
    MinimumDistancesFree(dest->md);
    KernPairsFree(dest->kerns);
    AnchorPointsFree(dest->anchor);
    PSTFree(dest->possub);
    free(dest->ttf_instrs);

    *dest = *src;
    dest->backimages = images;
    dest->backgroundsplines = back;
    dest->undoes[0] = u[0]; dest->undoes[1] = u[1]; dest->redoes[0] = NULL; dest->redoes[1] = r1;
    dest->enc = enc; dest->unicodeenc = uenc; dest->old_enc = oenc;
    dest->dependents = scl;
    dest->namechanged = true;

    src->name = copy(".notdef");
    src->splines = NULL;
    src->refs = NULL;
    src->hstem = NULL;
    src->vstem = NULL;
    src->dstem = NULL;
    src->md = NULL;
    src->kerns = NULL;
    src->anchor = NULL;
    src->possub = NULL;
    src->ttf_instrs = NULL;
    src->ttf_instrs_len = 0;
    /* Retain the undoes/redoes */
    /* Retain the dependents */

    /* Fix up dependant info */
    for ( refs = dest->refs; refs!=NULL; refs=refs->next ) {
	for ( scl=refs->sc->dependents; scl!=NULL; scl=scl->next )
	    if ( scl->sc==src )
		scl->sc = dest;
    }
    SCCharChangedUpdate(src);
    SCCharChangedUpdate(dest);
}

void FVApplySubstitution(FontView *fv,uint32 script, uint32 lang, uint32 tag) {
    SplineFont *sf = fv->sf;
    SplineChar *sc, *replacement;
    PST *pst;
    int i;

    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] && (sc=sf->chars[i])!=NULL ) {
	for ( pst = sc->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_substitution && pst->tag==tag &&
		    ScriptLangMatch(sf->script_lang[pst->script_lang_index],script,lang))
	break;
	}
	if ( pst!=NULL ) {
	    replacement = SFGetCharDup(sf,-1,pst->u.subs.variant);
	    if ( replacement!=NULL )
		SCReplaceWith(sc,replacement);
	}
    }
}

#if HANYANG
static void FVMenuModifyComposition(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( fv->sf->rules!=NULL )
	SFModifyComposition(fv->sf);
}

static void FVMenuBuildSyllables(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( fv->sf->rules!=NULL )
	SFBuildSyllables(fv->sf);
}
#endif

static void FVMenuMergeFonts(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVMergeFonts(fv);
}

static void FVMenuInterpFonts(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVInterpolateFonts(fv);
}

void FVScrollToChar(FontView *fv,int i) {

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;

    if ( i!=-1 ) {
	if ( i/fv->colcnt<fv->rowoff || i/fv->colcnt >= fv->rowoff+fv->rowcnt ) {
	    fv->rowoff = i/fv->colcnt;
	    if ( fv->rowcnt>= 3 )
		--fv->rowoff;
	    if ( fv->rowoff+fv->rowcnt>=fv->rowltot )
		fv->rowoff = fv->rowltot-fv->rowcnt;
	    if ( fv->rowoff<0 ) fv->rowoff = 0;
	    GScrollBarSetPos(fv->vsb,fv->rowoff);
	    GDrawRequestExpose(fv->v,NULL,false);
	}
    }
}

static void FVShowInfo(FontView *fv);
void FVChangeChar(FontView *fv,int i) {

    if ( i!=-1 ) {
	FVDeselectAll(fv);
	fv->selected[i] = true;
	fv->end_pos = fv->pressed_pos = i;
	FVToggleCharSelected(fv,i);
	FVScrollToChar(fv,i);
	FVShowInfo(fv);
    }
}

static void FVMenuChangeChar(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *sf = fv->sf;
    int pos = FVAnyCharSelected(fv);
    if ( pos>=0 ) {
	if ( mi->mid==MID_Next )
	    ++pos;
	else if ( mi->mid==MID_Prev )
	    --pos;
	else if ( mi->mid==MID_NextDef ) {
	    for ( ++pos; pos<sf->charcnt && sf->chars[pos]==NULL; ++pos );
	    if ( pos>=sf->charcnt ) {
		if ( sf->encoding_name==em_big5 && FVAnyCharSelected(fv)<0xa140 )
		    pos = 0xa140;
		else if ( sf->encoding_name==em_big5hkscs && FVAnyCharSelected(fv)<0x8140 )
		    pos = 0x8140;
		else if ( sf->encoding_name==em_johab && FVAnyCharSelected(fv)<0x8431 )
		    pos = 0x8431;
		else if ( sf->encoding_name==em_wansung && FVAnyCharSelected(fv)<0xa1a1 )
		    pos = 0xa1a1;
		else if ( sf->encoding_name==em_sjis && FVAnyCharSelected(fv)<0x8100 )
		    pos = 0x8100;
		else if ( sf->encoding_name==em_sjis && FVAnyCharSelected(fv)<0xb000 )
		    pos = 0xe000;
		if ( pos>=sf->charcnt )
return;
	    }
	} else if ( mi->mid==MID_PrevDef ) {
	    for ( --pos; pos>=0 && sf->chars[pos]==NULL; --pos );
	    if ( pos<0 )
return;
	}
    }
    if ( pos<0 ) pos = sf->charcnt-1;
    else if ( pos>= sf->charcnt ) pos = 0;
    if ( pos>=0 && pos<sf->charcnt )
	FVChangeChar(fv,pos);
}

static void FVMenuGotoChar(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int pos = GotoChar(fv->sf);
    FVChangeChar(fv,pos);
}

static void FVMenuLigatures(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFShowLigatures(fv->sf,NULL);
}

static void FVMenuKernPairs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFKernPrepare(fv->sf,false);
    SFShowKernPairs(fv->sf,NULL,NULL);
    SFKernCleanup(fv->sf,false);
}

static void FVMenuAnchorPairs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFShowKernPairs(fv->sf,NULL,mi->ti.userdata);
}

static void FVMenuShowAtt(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    ShowAtt(fv->sf);
}

static void FVMenuCompact(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FontView *fvs;
    int fv_reformat;

    for ( fvs=fv->sf->fv; fvs!=NULL; fvs = fvs->nextsame )
	if ( fvs->fontinfo )
	    FontInfoDestroy(fvs);

    if ( mi->mid==MID_CompactedView )
	fv_reformat = SFCompactFont(fv->sf);
    else
	fv_reformat = SFUncompactFont(fv->sf);
    if ( !fv_reformat )
return;
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs = fvs->nextsame ) {
	free(fvs->selected);
	fvs->selected = gcalloc(fv->sf->charcnt,sizeof(char));
    }
    FontViewReformatAll(fv->sf);
}

static void FVChangeDisplayFont(FontView *fv,BDFFont *bdf) {
    int samesize=0;

    if ( fv->v==NULL )			/* Can happen in scripts */
return;

    if ( fv->show!=bdf ) {
	if ( fv->show!=NULL && fv->cbw == bdf->pixelsize+1 )
	    samesize = true;
	fv->show = bdf;
	if ( bdf->pixelsize<20 ) {
	    if ( bdf->pixelsize<=9 )
		fv->magnify = 3;
	    else
		fv->magnify = 2;
	    samesize = ( fv->show && fv->cbw == (bdf->pixelsize*fv->magnify)+1 );
	} else
	    fv->magnify = 1;
	fv->cbw = (bdf->pixelsize*fv->magnify)+1;
	fv->cbh = (bdf->pixelsize*fv->magnify)+1+FV_LAB_HEIGHT+1;
	if ( samesize ) {
	    GDrawRequestExpose(fv->v,NULL,false);
	} else if ((( bdf->pixelsize<=fv->sf->display_size || bdf->pixelsize<=-fv->sf->display_size ) &&
		 fv->sf->top_enc!=-1 /* Not defaulting */ ) ||
		bdf->pixelsize<=48 ) {
	    GDrawResize(fv->gw,
		    fv->sf->desired_col_cnt*fv->cbw+1+GDrawPointsToPixels(fv->gw,_GScrollBar_Width),
		    fv->sf->desired_row_cnt*fv->cbh+1+fv->mbh+fv->infoh);
	} else if ( bdf->pixelsize<96 ) {
	    GDrawResize(fv->gw,
		    8*fv->cbw+1+GDrawPointsToPixels(fv->gw,_GScrollBar_Width),
		    2*fv->cbh+1+fv->mbh+fv->infoh);
	} else {
	    GDrawResize(fv->gw,
		    4*fv->cbw+1+GDrawPointsToPixels(fv->gw,_GScrollBar_Width),
		    fv->cbh+1+fv->mbh+fv->infoh);
	}
	/* colcnt, rowcnt, etc. will be set by the resize handler */
    }
}

void FVChangeDisplayBitmap(FontView *fv,BDFFont *bdf) {
    FVChangeDisplayFont(fv,bdf);
    fv->sf->display_size = fv->show->pixelsize;
}

struct md_data {
    int done;
    int ish;
    FontView *fv;
};

static int md_e_h(GWindow gw, GEvent *e) {
    if ( e->type==et_close ) {
	struct md_data *d = GDrawGetUserData(gw);
	d->done = true;
    } else if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct md_data *d = GDrawGetUserData(gw);
	static int masks[] = { fvm_baseline, fvm_origin, fvm_advanceat, fvm_advanceto, -1 };
	int i, metrics;
	if ( GGadgetGetCid(e->u.control.g)==10 ) {
	    metrics = 0;
	    for ( i=0; masks[i]!=-1 ; ++i )
		if ( GGadgetIsChecked(GWidgetGetControl(gw,masks[i])))
		    metrics |= masks[i];
	    if ( d->ish )
		default_fv_showhmetrics = d->fv->showhmetrics = metrics;
	    else
		default_fv_showvmetrics = d->fv->showvmetrics = metrics;
	}
	d->done = true;
    } else if ( e->type==et_char ) {
#if 0
	if ( e->u.chr.keysym == GK_F1 || e->u.chr.keysym == GK_Help ) {
	    help("fontinfo.html");
return( true );
	}
#endif
return( false );
    }
return( true );
}

static void FVMenuShowMetrics(GWindow fvgw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(fvgw);
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    struct md_data d;
    GGadgetCreateData gcd[7];
    GTextInfo label[6];
    int metrics = mi->mid==MID_ShowHMetrics ? fv->showhmetrics : fv->showvmetrics;

    d.fv = fv;
    d.done = 0;
    d.ish = mi->mid==MID_ShowHMetrics;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = GStringGetResource(d.ish?_STR_ShowHMetrics:_STR_ShowVMetrics,NULL);
    pos.x = pos.y = 0;
    pos.width =GDrawPointsToPixels(NULL,GGadgetScale(170));
    pos.height = GDrawPointsToPixels(NULL,130);
    gw = GDrawCreateTopWindow(NULL,&pos,md_e_h,&d,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    label[0].text = (unichar_t *) _STR_Baseline;
    label[0].text_in_resource = true;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.pos.x = 8; gcd[0].gd.pos.y = 8; 
    gcd[0].gd.flags = gg_enabled|gg_visible|(metrics&fvm_baseline?gg_cb_on:0);
    gcd[0].gd.cid = fvm_baseline;
    gcd[0].gd.popup_msg = GStringGetResource(_STR_IsVis,NULL);
    gcd[0].creator = GCheckBoxCreate;

    label[1].text = (unichar_t *) _STR_Origin;
    label[1].text_in_resource = true;
    gcd[1].gd.label = &label[1];
    gcd[1].gd.pos.x = 8; gcd[1].gd.pos.y = gcd[0].gd.pos.y+16;
    gcd[1].gd.flags = gg_enabled|gg_visible|(metrics&fvm_origin?gg_cb_on:0);
    gcd[1].gd.cid = fvm_origin;
    gcd[1].creator = GCheckBoxCreate;

    label[2].text = (unichar_t *) _STR_AdvanceWidthAsLine;
    label[2].text_in_resource = true;
    gcd[2].gd.label = &label[2];
    gcd[2].gd.pos.x = 8; gcd[2].gd.pos.y = gcd[1].gd.pos.y+16;
    gcd[2].gd.flags = gg_enabled|gg_visible|(metrics&fvm_advanceat?gg_cb_on:0);
    gcd[2].gd.cid = fvm_advanceat;
    gcd[2].gd.popup_msg = GStringGetResource(_STR_AdvanceLinePopup,NULL);
    gcd[2].creator = GCheckBoxCreate;

    label[3].text = (unichar_t *) _STR_AdvanceWidthAsBar;
    label[3].text_in_resource = true;
    gcd[3].gd.label = &label[3];
    gcd[3].gd.pos.x = 8; gcd[3].gd.pos.y = gcd[2].gd.pos.y+16; 
    gcd[3].gd.flags = gg_enabled|gg_visible|(metrics&fvm_advanceto?gg_cb_on:0);
    gcd[3].gd.cid = fvm_advanceto;
    gcd[3].gd.popup_msg = GStringGetResource(_STR_AdvanceBarPopup,NULL);
    gcd[3].creator = GCheckBoxCreate;

    label[4].text = (unichar_t *) _STR_OK;
    label[4].text_in_resource = true;
    gcd[4].gd.label = &label[4];
    gcd[4].gd.pos.x = 20-3; gcd[4].gd.pos.y = GDrawPixelsToPoints(NULL,pos.height)-35-3;
    gcd[4].gd.pos.width = -1; gcd[4].gd.pos.height = 0;
    gcd[4].gd.flags = gg_visible | gg_enabled | gg_but_default;
    gcd[4].gd.cid = 10;
    gcd[4].creator = GButtonCreate;

    label[5].text = (unichar_t *) _STR_Cancel;
    label[5].text_in_resource = true;
    gcd[5].gd.label = &label[5];
    gcd[5].gd.pos.x = -20; gcd[5].gd.pos.y = gcd[4].gd.pos.y+3;
    gcd[5].gd.pos.width = -1; gcd[5].gd.pos.height = 0;
    gcd[5].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    gcd[5].creator = GButtonCreate;

    GGadgetsCreate(gw,gcd);
    
    GDrawSetVisible(gw,true);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);

    SavePrefs();
    GDrawRequestExpose(fv->v,NULL,false);
}

static void FVMenuSize(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw), *fvs, *fvss;
    int dspsize = fv->filled->pixelsize;
    int changedmodifier = false;

    fv->magnify = 1;
    if ( mi->mid == MID_24 )
	default_fv_font_size = dspsize = 24;
    else if ( mi->mid == MID_36 )
	default_fv_font_size = dspsize = 36;
    else if ( mi->mid == MID_48 )
	default_fv_font_size = dspsize = 48;
    else if ( mi->mid == MID_72 )
	default_fv_font_size = dspsize = 72;
    else if ( mi->mid == MID_96 )
	default_fv_font_size = dspsize = 96;
    else if ( mi->mid == MID_FitToEm ) {
	default_fv_bbsized = fv->bbsized = !fv->bbsized;
	fv->sf->display_bbsized = fv->bbsized;
	changedmodifier = true;
    } else {
	default_fv_antialias = fv->antialias = !fv->antialias;
	fv->sf->display_antialias = fv->antialias;
	changedmodifier = true;
    }
    SavePrefs();
    if ( fv->filled!=fv->show || fv->filled->pixelsize != dspsize || changedmodifier ) {
	BDFFont *new, *old;
	for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	    fvs->touched = false;
	while ( 1 ) {
	    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
		if ( !fvs->touched )
	    break;
	    if ( fvs==NULL )
	break;
	    old = fvs->filled;
	    new = SplineFontPieceMeal(fvs->sf,dspsize,
		(fvs->antialias?pf_antialias:0)|(fvs->bbsized?pf_bbsized:0),
		NULL);
	    for ( fvss=fvs; fvss!=NULL; fvss = fvss->nextsame ) {
		if ( fvss->filled==old ) {
		    fvss->filled = new;
		    fvss->antialias = fvs->antialias;
		    fvss->bbsized = fvs->bbsized;
		    if ( fvss->show==old || fvss==fv )
			FVChangeDisplayFont(fvss,new);
		    fvss->touched = true;
		}
	    }
	    BDFFontFree(old);
	}
	fv->sf->display_size = -dspsize;
	if ( fv->cidmaster!=NULL ) {
	    int i;
	    for ( i=0; i<fv->cidmaster->subfontcnt; ++i )
		fv->cidmaster->subfonts[i]->display_size = -dspsize;
	}
    }
}

static void FVMenuShowBitmap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    BDFFont *bdf = mi->ti.userdata;

    FVChangeDisplayBitmap(fv,bdf);		/* Let's not change any of the others */
}

void FVShowFilled(FontView *fv) {
    FontView *fvs;

    fv->magnify = 1;
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	if ( fvs->show!=fvs->filled )
	    FVChangeDisplayFont(fvs,fvs->filled);
    fv->sf->display_size = -fv->filled->pixelsize;
}

static void ellistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);
    int anybuildable, anytraceable;
    int order2 = fv->sf->order2;

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_CharInfo:
	    mi->ti.disabled = anychars<0 /*|| fv->cidmaster!=NULL*/;
	  break;
	  case MID_ShowDependents:
	    mi->ti.disabled = anychars<0 || fv->sf->chars[anychars]==NULL ||
		    fv->sf->chars[anychars]->dependents == NULL;
	  break;
	  case MID_FindProblems:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_MetaFont: case MID_Effects:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_Transform: case MID_NLTransform:
	    mi->ti.disabled = anychars==-1;
	    /* some Transformations make sense on bitmaps now */
	  break;
	  case MID_AddExtrema:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
	  case MID_Simplify:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || order2;
#if 0
	    free(mi->ti.text);
	    if ( e==NULL || !(e->u.mouse.state&ksm_shift) ) {
		mi->ti.text = u_copy(GStringGetResource(_STR_Simplify,NULL));
		mi->short_mask = ksm_control|ksm_shift;
		mi->invoke = FVMenuSimplify;
	    } else {
		mi->ti.text = u_copy(GStringGetResource(_STR_SimplifyMore,NULL));
		mi->short_mask = (ksm_control|ksm_meta|ksm_shift);
		mi->invoke = FVMenuSimplifyMore;
	    }
#endif
	  break;
	  case MID_RmOverlap:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || order2;
#if 0
	    if ( !mi->ti.disabled ) {
		if ( e==NULL || !(e->u.mouse.state&ksm_shift) )
		    mi->ti.text = u_copy(GStringGetResource(_STR_Rmoverlap,NULL));
		else
		    mi->ti.text = u_copy(GStringGetResource(_STR_FindIntersections,NULL));
	    }
#endif
	  break;
	  case MID_Stroke:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || order2;
	  break;
	  case MID_Round: case MID_Correct:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
#ifdef PFAEDIT_CONFIG_TILEPATH
	  case MID_TilePath:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || ClipBoardToSplineSet()==NULL || order2;
	  break;
#endif
	  case MID_RegenBitmaps:
	    mi->ti.disabled = fv->sf->bitmaps==NULL || fv->sf->onlybitmaps;
	  break;
	  case MID_BuildAccent:
	    anybuildable = false;
	    if ( anychars!=-1 ) {
		int i;
		for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
		    SplineChar *sc, dummy;
		    sc = fv->sf->chars[i];
		    if ( sc==NULL )
			sc = SCBuildDummy(&dummy,fv->sf,i);
		    if ( SFIsSomethingBuildable(fv->sf,sc,false)) {
			anybuildable = true;
		break;
		    }
		}
	    }
	    mi->ti.disabled = !anybuildable;
	  break;
	  case MID_Autotrace:
	    anytraceable = false;
	    if ( FindAutoTraceName()!=NULL && anychars!=-1 ) {
		int i;
		for ( i=0; i<fv->sf->charcnt; ++i )
		    if ( fv->selected[i] && fv->sf->chars[i]!=NULL &&
			    fv->sf->chars[i]->backimages!=NULL ) {
			anytraceable = true;
		break;
		    }
	    }
	    mi->ti.disabled = !anytraceable;
	  break;
	  case MID_MergeFonts:
	    mi->ti.disabled = fv->sf->bitmaps!=NULL && fv->sf->onlybitmaps;
	  break;
	  case MID_InterpolateFonts:
	    mi->ti.disabled = fv->sf->onlybitmaps;
	  break;
	}
    }
}

void FVMetricsCenter(FontView *fv,int docenter) {
    int i;
    DBounds bb;
    real transform[6];

    transform[0] = transform[3] = 1.0;
    transform[1] = transform[2] = transform[5] = 0.0;
    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	SplineCharFindBounds(sc,&bb);
	if ( docenter )
	    transform[4] = (sc->width-(bb.maxx-bb.minx))/2 - bb.minx;
	else
	    transform[4] = (sc->width-(bb.maxx-bb.minx))/3 - bb.minx;
	if ( transform[4]!=0 )
	    FVTrans(fv,sc,transform,NULL,false);
    }
}

static void FVMenuCenter(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVMetricsCenter(fv,mi->mid==MID_Center);
}

static void FVMenuSetWidth(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( mi->mid == MID_SetVWidth && !fv->sf->hasvmetrics )
return;
    FVSetWidth(fv,mi->mid==MID_SetWidth?wt_width:
		  mi->mid==MID_SetLBearing?wt_lbearing:
		  mi->mid==MID_SetRBearing?wt_rbearing:
		  wt_vwidth);
}

static void FVMenuAutoWidth(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVAutoWidth(fv);
}

static void FVMenuAutoKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVAutoKern(fv);
}

static void FVMenuKernByClasses(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    ShowKernClasses(fv->sf,NULL,false);
}

static void FVMenuVKernByClasses(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    ShowKernClasses(fv->sf,NULL,true);
}

static void FVMenuRemoveKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVRemoveKerns(fv);
}

static void FVMenuRemoveVKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVRemoveVKerns(fv);
}

static void FVMenuVKernFromHKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    FVVKernFromHKern(fv);
}

static void FVMenuPrint(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    PrintDlg(fv,NULL,NULL);
}

static void FVMenuDisplay(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    DisplayDlg(fv->sf);
}

static void FVMenuExecute(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    ScriptDlg(fv);
}

static void mtlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Center: case MID_Thirds: case MID_SetWidth: 
	  case MID_SetLBearing: case MID_SetRBearing:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_SetVWidth:
	    mi->ti.disabled = anychars==-1 || !fv->sf->hasvmetrics;
	  break;
	  case MID_VKernByClass:
	  case MID_VKernFromH:
	  case MID_RmVKern:
	    mi->ti.disabled = !fv->sf->hasvmetrics;
	  break;
	}
    }
}

static void FVAutoHint(FontView *fv,int removeOverlap) {
    int i, cnt=0;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_AutoHintingFont,_STR_AutoHintingFont,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	sc->manualhints = false;
	SplineCharAutoHint(sc,removeOverlap);
	SCUpdateAll(sc);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuAutoHint(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    int removeOverlap = e==NULL || !(e->u.mouse.state&ksm_shift);
    FVAutoHint( (FontView *) GDrawGetUserData(gw), removeOverlap );
}

static void FVAutoInstr(FontView *fv) {
    BlueData bd;
    int i, cnt=0;

    QuickBlues(fv->sf,&bd);
    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_AutoInstructingFont,_STR_AutoInstructingFont,0,cnt,1);

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	SCAutoInstr(sc,&bd);
	if ( !GProgressNext())
    break;
    }
    GProgressEndIndicator();
}

static void FVMenuAutoInstr(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVAutoInstr( (FontView *) GDrawGetUserData(gw) );
}

static void FVMenuEditInstrs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int index = FVAnyCharSelected(fv);
    SplineChar *sc;
    if ( index<0 )
return;
    sc = SFMakeChar(fv->sf,index);
    SCEditInstructions(sc);
}

static void FVMenuEditTable(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFEditTable(fv->sf,mi->mid==MID_Editprep?CHR('p','r','e','p'):CHR('f','p','g','m'));
}

static void FVMenuClearInstrs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineChar *sc;
    int i;

    if ( !SFCloseAllInstrs(fv->sf))
return;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( (sc = fv->sf->chars[i])!=NULL ) {
	if ( sc->ttf_instrs_len!=0 ) {
	    free(sc->ttf_instrs);
	    sc->ttf_instrs = NULL;
	    sc->ttf_instrs_len = 0;
	    SCCharChangedUpdate(sc);
	}
    }
}

static void FVClearHints(FontView *fv) {
    int i;

#if 0
    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] )
	++cnt;
    GProgressStartIndicatorR(10,_STR_AutoHintingFont,_STR_AutoHintingFont,0,cnt,1);
#endif

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	sc->manualhints = true;
	StemInfosFree(sc->hstem);
	StemInfosFree(sc->vstem);
	sc->hstem = sc->vstem = NULL;
	sc->hconflicts = sc->vconflicts = false;
	DStemInfosFree(sc->dstem);
	sc->dstem = NULL;
	MinimumDistancesFree(sc->md);
	sc->md = NULL;
	SCClearRounds(sc);
	SCOutOfDateBackground(sc);
	SCUpdateAll(sc);
#if 0
	if ( !GProgressNext())
    break;
#endif
    }
#if 0
    GProgressEndIndicator();
#endif
}

static void FVMenuClearHints(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVClearHints(fv);
}

static void FVMenuClearWidthMD(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, changed;
    MinimumDistance *md, *prev, *next;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->sf->chars[i]!=NULL && fv->selected[i] ) {
	SplineChar *sc = fv->sf->chars[i];
	prev=NULL; changed = false;
	for ( md=sc->md; md!=NULL; md=next ) {
	    next = md->next;
	    if ( md->sp2==NULL ) {
		if ( prev==NULL )
		    sc->md = next;
		else
		    prev->next = next;
		chunkfree(md,sizeof(MinimumDistance));
		changed = true;
	    } else
		prev = md;
	}
	if ( changed ) {
	    sc->manualhints = true;
	    SCOutOfDateBackground(sc);
	    SCUpdateAll(sc);
	}
    }
}

static void FVMenuHistograms(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFHistogram(fv->sf, NULL, FVAnyCharSelected(fv)!=-1?fv->selected:NULL,
			mi->mid==MID_HStemHist ? hist_hstem :
			mi->mid==MID_VStemHist ? hist_vstem :
				hist_blues);
}

void FVSetTitle(FontView *fv) {
    unichar_t *title, *ititle, *temp;
    char *file=NULL;
    int len;

    len = strlen(fv->sf->fontname);
    if ( (file = fv->sf->filename)==NULL )
	file = fv->sf->origname;
    if ( file!=NULL )
	len += 2+strlen(file);
    title = galloc((len+1)*sizeof(unichar_t));
    uc_strcpy(title,fv->sf->fontname);
    if ( file!=NULL ) {
	uc_strcat(title,"  ");
	temp = def2u_copy(GFileNameTail(file));
	u_strcat(title,temp);
	free(temp);
    }
    ititle = uc_copy(fv->sf->fontname);
    GDrawSetWindowTitles(fv->gw,title,ititle);
    free(title);
    free(ititle);
}

static void FVMenuShowSubFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *new = mi->ti.userdata;
    MetricsView *mv, *mvnext;

    for ( mv=fv->metrics; mv!=NULL; mv = mvnext ) {
	/* Don't bother trying to fix up metrics views, just not worth it */
	mvnext = mv->next;
	GDrawDestroyWindow(mv->gw);
    }
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    if ( fv->sf->charcnt!=new->charcnt ) {
	free(fv->selected);
	fv->selected = gcalloc(new->charcnt,sizeof(char));
    }
    fv->sf = new;
    FVSetTitle(fv);
    FontViewReformatOne(fv);
}

static void FVMenuConvert2CID(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster!=NULL )
return;
    MakeCIDMaster(fv->sf,false,NULL,NULL);
}

static void FVMenuConvertByCMap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster!=NULL )
return;
    MakeCIDMaster(fv->sf,true,NULL,NULL);
}

static void FVMenuFlatten(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;
    SplineChar **chars;
    int i,j,max;

    if ( cidmaster==NULL )
return;
    /* This doesn't change the ordering, so no need for special tricks to */
    /*  preserve scrolling location. */
    for ( i=max=0; i<cidmaster->subfontcnt; ++i )
	if ( max<cidmaster->subfonts[i]->charcnt )
	    max = cidmaster->subfonts[i]->charcnt;
    chars = gcalloc(max,sizeof(SplineChar *));
    for ( j=0; j<max; ++j ) {
	for ( i=0; i<cidmaster->subfontcnt; ++i ) {
	    if ( j<cidmaster->subfonts[i]->charcnt && cidmaster->subfonts[i]->chars[j]!=NULL ) {
		chars[j] = cidmaster->subfonts[i]->chars[j];
		cidmaster->subfonts[i]->chars[j] = NULL;
	break;
	    }
	}
    }
    CIDFlatten(cidmaster,chars,max);
}

static void FVMenuFlattenByCMap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster==NULL )
return;
    SFFlattenByCMap(cidmaster,NULL);
}

static void FVInsertInCID(FontView *fv,SplineFont *sf) {
    SplineFont *cidmaster = fv->cidmaster;
    SplineFont **subs;
    int i;

    subs = galloc((cidmaster->subfontcnt+1)*sizeof(SplineFont *));
    for ( i=0; i<cidmaster->subfontcnt && cidmaster->subfonts[i]!=fv->sf; ++i )
	subs[i] = cidmaster->subfonts[i];
    subs[i] = sf;
    for ( ; i<cidmaster->subfontcnt ; ++i )
	subs[i+1] = cidmaster->subfonts[i];
    ++cidmaster->subfontcnt;
    free(cidmaster->subfonts);
    cidmaster->subfonts = subs;
    cidmaster->changed = true;
    sf->cidmaster = cidmaster;

    if ( fv->sf->charcnt!=sf->charcnt ) {
	free(fv->selected);
	fv->selected = gcalloc(sf->charcnt,sizeof(char));
    }
    fv->sf = sf;
    sf->fv = fv;
    FontViewReformatOne(fv);
    FVSetTitle(fv);
}

static void FVMenuInsertFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;
    SplineFont *new;
    struct cidmap *map;
    char *filename;

    if ( cidmaster==NULL || cidmaster->subfontcnt>=255 )	/* Open type allows 1 byte to specify the fdselect */
return;

    filename = GetPostscriptFontName(NULL,false);
    if ( filename==NULL )
return;
    new = LoadSplineFont(filename,0);
    free(filename);
    if ( new==NULL )
return;
    if ( new->fv == fv )		/* Already part of us */
return;
    if ( new->fv != NULL ) {
	if ( new->fv->gw!=NULL )
	    GDrawRaise(new->fv->gw);
	GWidgetErrorR(_STR_CloseFont,_STR_CloseFontForCID,new->origname);
return;
    }

    map = FindCidMap(cidmaster->cidregistry,cidmaster->ordering,cidmaster->supplement,cidmaster);
    SFEncodeToMap(new,map);
    if ( !PSDictHasEntry(new->private,"lenIV"))
	PSDictChangeEntry(new->private,"lenIV","1");		/* It's 4 by default, in CIDs the convention seems to be 1 */
    new->display_antialias = fv->sf->display_antialias;
    new->display_bbsized = fv->sf->display_bbsized;
    new->display_size = fv->sf->display_size;
    FVInsertInCID(fv,new);
}

static void FVMenuInsertBlank(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster, *sf;
    struct cidmap *map;

    if ( cidmaster==NULL || cidmaster->subfontcnt>=255 )	/* Open type allows 1 byte to specify the fdselect */
return;
    map = FindCidMap(cidmaster->cidregistry,cidmaster->ordering,cidmaster->supplement,cidmaster);
    sf = SplineFontBlank(em_none,MaxCID(map));
    sf->cidmaster = cidmaster;
    sf->display_antialias = fv->sf->display_antialias;
    sf->display_bbsized = fv->sf->display_bbsized;
    sf->display_size = fv->sf->display_size;
    sf->private = gcalloc(1,sizeof(struct psdict));
    PSDictChangeEntry(sf->private,"lenIV","1");		/* It's 4 by default, in CIDs the convention seems to be 1 */
    FVInsertInCID(fv,sf);
}

static void FVMenuRemoveFontFromCID(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster, *sf = fv->sf, *replace;
    static int buts[] = { _STR_Remove, _STR_Cancel, 0 };
    int i;
    MetricsView *mv, *mnext;
    FontView *fvs;

    if ( cidmaster==NULL || cidmaster->subfontcnt<=1 )	/* Can't remove last font */
return;
    if ( GWidgetAskR(_STR_RemoveFont,buts,0,1,_STR_CIDRemoveFontCheck,
	    sf->fontname,cidmaster->fontname)==1 )
return;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	CharView *cv, *next;
	for ( cv = sf->chars[i]->views; cv!=NULL; cv = next ) {
	    next = cv->next;
	    GDrawDestroyWindow(cv->gw);
	}
    }
    GDrawProcessPendingEvents(NULL);
    for ( mv=fv->metrics; mv!=NULL; mv = mnext ) {
	mnext = mv->next;
	GDrawDestroyWindow(mv->gw);
    }
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    /* Just in case... */
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);

    for ( i=0; i<cidmaster->subfontcnt; ++i )
	if ( cidmaster->subfonts[i]==sf )
    break;
    replace = i==0?cidmaster->subfonts[1]:cidmaster->subfonts[i-1];
    while ( i<cidmaster->subfontcnt-1 ) {
	cidmaster->subfonts[i] = cidmaster->subfonts[i+1];
	++i;
    }
    --cidmaster->subfontcnt;

    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	if ( fvs->sf==sf ) {
	    fvs->sf = replace;
	    if ( replace->charcnt!=sf->charcnt ) {
		free(fvs->selected);
		fvs->selected = gcalloc(replace->charcnt,sizeof(char));
	    }
	    FVSetTitle(fv);
	}
    }
    FontViewReformatAll(fv->sf);
    SplineFontFree(sf);
}

static void FVMenuCIDFontInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster==NULL )
return;
    FontInfo(cidmaster,-1,false);
}

static void htlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);
    int removeOverlap;

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_AutoHint:
	    mi->ti.disabled = anychars==-1;
	    removeOverlap = e==NULL || !(e->u.mouse.state&ksm_shift);
	    free(mi->ti.text);
	    mi->ti.text = u_copy(GStringGetResource(removeOverlap?_STR_Autohint:_STR_FullAutohint,NULL));
	  break;
	  case MID_AutoInstr: case MID_EditInstructions:
	    mi->ti.disabled = !fv->sf->order2 || anychars==-1;
	  break;
	  case MID_Editfpgm: case MID_Editprep:
	    mi->ti.disabled = !fv->sf->order2 ;
	  break;
	  case MID_ClearHints: case MID_ClearWidthMD: case MID_ClearInstrs:
	    mi->ti.disabled = anychars==-1;
	  break;
	}
    }
}

static void fllistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_OpenOutline:
	    mi->ti.disabled = !anychars;
	  break;
	  case MID_OpenBitmap:
	    mi->ti.disabled = !anychars || fv->sf->bitmaps==NULL;
	  break;
	  case MID_Revert:
	    mi->ti.disabled = fv->sf->origname==NULL;
	  break;
	  case MID_RevertGlyph:
	    mi->ti.disabled = fv->sf->filename==NULL || !anychars;
	  break;
	  case MID_Recent:
	    mi->ti.disabled = !RecentFilesAny();
	  break;
	  case MID_ScriptMenu:
	    mi->ti.disabled = script_menu_names[0]==NULL;
	  break;
	  case MID_Print:
	    mi->ti.disabled = fv->sf->onlybitmaps;
	  break;
	  case MID_Display:
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->sf->bitmaps==NULL;
	  break;
	}
    }
}

#if HANYANG
static void hglistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
        if ( mi->mid==MID_BuildSyllables || mi->mid==MID_ModifyComposition )
	    mi->ti.disabled = fv->sf->rules==NULL;
    }
}

static GMenuItem hglist[] = {
    { { (unichar_t *) _STR_NewComposition, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'N' }, 'N', ksm_shift|ksm_control, NULL, NULL, MenuNewComposition },
    { { (unichar_t *) _STR_ModifyComposition, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuModifyComposition, MID_ModifyComposition },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_BuildSyllables, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuBuildSyllables, MID_BuildSyllables },
    { NULL }
};
#endif

static void balistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
        if ( mi->mid==MID_BuildAccent || mi->mid==MID_BuildComposite ) {
	    int anybuildable = false;
	    int onlyaccents = mi->mid==MID_BuildAccent;
	    int i;
	    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
		SplineChar *sc, dummy;
		sc = fv->sf->chars[i];
		if ( sc==NULL )
		    sc = SCBuildDummy(&dummy,fv->sf,i);
		if ( SFIsSomethingBuildable(fv->sf,sc,onlyaccents)) {
		    anybuildable = true;
	    break;
		}
	    }
	    mi->ti.disabled = !anybuildable;
	}
    }
}

static GMenuItem dummyitem[] = { { (unichar_t *) _STR_New, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'N' }, NULL };
static GMenuItem fllist[] = {
    { { (unichar_t *) _STR_New, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'N' }, 'N', ksm_control, NULL, NULL, MenuNew },
#if HANYANG
    { { (unichar_t *) _STR_Hangul, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', 0, hglist, hglistcheck, NULL, 0 },
#endif
    { { (unichar_t *) _STR_Open, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'O' }, 'O', ksm_control, NULL, NULL, MenuOpen },
    { { (unichar_t *) _STR_Recent, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 't' }, '\0', ksm_control, dummyitem, MenuRecentBuild, NULL, MID_Recent },
    { { (unichar_t *) _STR_Close, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, 'Q', ksm_control|ksm_shift, NULL, NULL, FVMenuClose },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Openoutline, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'u' }, 'H', ksm_control, NULL, NULL, FVMenuOpenOutline, MID_OpenOutline },
    { { (unichar_t *) _STR_Openbitmap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 'J', ksm_control, NULL, NULL, FVMenuOpenBitmap, MID_OpenBitmap },
    { { (unichar_t *) _STR_Openmetrics, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 'K', ksm_control, NULL, NULL, FVMenuOpenMetrics },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Save, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, 'S', ksm_control, NULL, NULL, FVMenuSave },
    { { (unichar_t *) _STR_Saveas, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'a' }, 'S', ksm_control|ksm_shift, NULL, NULL, FVMenuSaveAs },
    { { (unichar_t *) _STR_SaveAll, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, 'S', ksm_control|ksm_meta, NULL, NULL, MenuSaveAll },
    { { (unichar_t *) _STR_Generate, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'G' }, 'G', ksm_control|ksm_shift, NULL, NULL, FVMenuGenerate },
    { { (unichar_t *) _STR_GenerateMac, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 'G', ksm_control|ksm_meta, NULL, NULL, FVMenuGenerateFamily },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Import, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, 'I', ksm_control|ksm_shift, NULL, NULL, FVMenuImport },
    { { (unichar_t *) _STR_Mergekern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 'K', ksm_meta|ksm_control|ksm_shift, NULL, NULL, FVMenuMergeKern },
    { { (unichar_t *) _STR_Revertfile, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, 'R', ksm_control|ksm_shift, NULL, NULL, FVMenuRevert, MID_Revert },
    { { (unichar_t *) _STR_RevertGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, 'R', ksm_control|ksm_meta, NULL, NULL, FVMenuRevertGlyph, MID_RevertGlyph },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Print, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, 'P', ksm_control, NULL, NULL, FVMenuPrint, MID_Print },
    { { (unichar_t *) _STR_Display, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'P', ksm_control|ksm_meta, NULL, NULL, FVMenuDisplay, MID_Display },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ExecuteScript, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'x' }, '.', ksm_control, NULL, NULL, FVMenuExecute },
    { { (unichar_t *) _STR_ScriptMenu, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'r' }, '\0', ksm_control, dummyitem, MenuScriptsBuild, NULL, MID_ScriptMenu },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Prefs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'e' }, '\0', ksm_control, NULL, NULL, MenuPrefs },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Quit, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'Q' }, 'Q', ksm_control, NULL, NULL, FVMenuExit },
    { NULL }
};

static GMenuItem cflist[] = {
    { { (unichar_t *) _STR_Allfonts, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'A' }, '\0', ksm_control, NULL, NULL, FVMenuCopyFrom, MID_AllFonts },
    { { (unichar_t *) _STR_Displayedfont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'D' }, '\0', ksm_control, NULL, NULL, FVMenuCopyFrom, MID_DisplayedFont },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_CharName, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'N' }, '\0', ksm_control, NULL, NULL, FVMenuCopyFrom, MID_CharName },
    { NULL }
};

static GMenuItem sclist[] = {
    { { (unichar_t *)  _STR_Default, &def_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) COLOR_DEFAULT, NULL, 0, 1, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &white_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xffffff, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &red_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xff0000, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &green_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0x00ff00, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &blue_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0x0000ff, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &yellow_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xffff00, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &cyan_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0x00ffff, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { { NULL, &magenta_image, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xff00ff, NULL, 0, 1, 0, 0, 0, 0, 0, 0, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuSelectColor },
    { NULL }
};

static GMenuItem sllist[] = {
    { { (unichar_t *) _STR_SelectAll, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, 'A', ksm_control, NULL, NULL, FVMenuSelectAll },
    { { (unichar_t *) _STR_DeselectAll, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, GK_Escape, 0, NULL, NULL, FVMenuDeselectAll },
    { { (unichar_t *) _STR_SelectColor, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, sclist },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_SelectByATT, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '\0', 0, NULL, NULL, FVMenuSelectByPST },
    { NULL }
};

static GMenuItem edlist[] = {
    { { (unichar_t *) _STR_Undo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 1, 0, 0, 0, 0, 0, 0, 1, 0, 'U' }, 'Z', ksm_control, NULL, NULL, FVMenuUndo, MID_Undo },
    { { (unichar_t *) _STR_Redo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 1, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, 'Y', ksm_control, NULL, NULL, FVMenuRedo, MID_Redo},
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Cut, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 't' }, 'X', ksm_control, NULL, NULL, FVMenuCut, MID_Cut },
    { { (unichar_t *) _STR_Copy, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, 'C', ksm_control, NULL, NULL, FVMenuCopy, MID_Copy },
    { { (unichar_t *) _STR_Copyref, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, 'G', ksm_control, NULL, NULL, FVMenuCopyRef, MID_CopyRef },
    { { (unichar_t *) _STR_Copywidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, 'W', ksm_control, NULL, NULL, FVMenuCopyWidth, MID_CopyWidth },
    { { (unichar_t *) _STR_CopyVWidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, '\0', ksm_control, NULL, NULL, FVMenuCopyWidth, MID_CopyVWidth },
    { { (unichar_t *) _STR_CopyLBearing, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'p' }, '\0', ksm_control, NULL, NULL, FVMenuCopyWidth, MID_CopyLBearing },
    { { (unichar_t *) _STR_CopyRBearing, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'g' }, '\0', ksm_control, NULL, NULL, FVMenuCopyWidth, MID_CopyRBearing },
    { { (unichar_t *) _STR_Paste, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, 'V', ksm_control, NULL, NULL, FVMenuPaste, MID_Paste },
    { { (unichar_t *) _STR_PasteInto, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, 'V', ksm_control|ksm_shift, NULL, NULL, FVMenuPasteInto, MID_PasteInto },
    { { (unichar_t *) _STR_SameGlyphAs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'm' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSameGlyphAs, MID_SameGlyphAs },
    { { (unichar_t *) _STR_Clear, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, 0, 0, NULL, NULL, FVMenuClear, MID_Clear },
    { { (unichar_t *) _STR_ClearBackground, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 0, 0, NULL, NULL, FVMenuClearBackground, MID_ClearBackground },
    { { (unichar_t *) _STR_CopyFgToBg, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 'C', ksm_control|ksm_shift, NULL, NULL, FVMenuCopyFgBg, MID_CopyFgToBg },
    { { (unichar_t *) _STR_Join, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'J' }, 'J', ksm_control|ksm_shift, NULL, NULL, FVMenuJoin, MID_Join },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Select, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, 0, ksm_control, sllist, sllistcheck },
    { { (unichar_t *) _STR_FindReplace, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'i' }, 'F', ksm_control|ksm_meta, NULL, NULL, FVMenuFindRpl },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Unlinkref, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'U' }, 'U', ksm_control, NULL, NULL, FVMenuUnlinkRef, MID_UnlinkRef },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Copyfrom, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', 0, cflist, cflistcheck, NULL, 0 },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_RemoveUndoes, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'e' }, '\0', 0, NULL, NULL, FVMenuRemoveUndoes, MID_RemoveUndoes },
    { NULL }
};

static GMenuItem aatlist[] = {
    { { (unichar_t *) _STR_All, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Ligatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xffffffff, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_StandardLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('l','i','g','a'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_RequiredLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('r','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_HistoricLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('h','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_DiscretionaryLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('d','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_FractionLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('f','r','a','c'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_MacUnicodeDecomposition, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('M','U','C','M'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_R2LAlt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('r','t','l','a'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_VertRotAlt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('v','r','t','2'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_SmallCaps, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('s','m','c','p'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_OldstyleFigures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('o','n','u','m'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Superscript, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('s','u','p','s'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Subscript, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('s','u','b','s'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Swash, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('s','w','s','h'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_PropWidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('p','w','i','d'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_FullWidths, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('f','w','i','d'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_HalfWidths, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('h','w','i','d'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_InitialForms, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('i','n','i','t'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_MedialForms, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('m','e','d','i'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_TerminalForms, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('f','i','n','a'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_IsolatedForms, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('i','s','o','l'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_LeftBounds, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('l','f','b','d'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_RightBounds, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('r','t','b','d'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_SuffixToTag, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAATSuffix },
    { NULL }
};

static GMenuItem smlist[] = {
    { { (unichar_t *) _STR_Simplify, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, 'M', ksm_control|ksm_shift, NULL, NULL, FVMenuSimplify, MID_Simplify },
    { { (unichar_t *) _STR_SimplifyMore, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 'M', ksm_control|ksm_shift|ksm_meta, NULL, NULL, FVMenuSimplifyMore },
    { { (unichar_t *) _STR_CleanupChar, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'n' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCleanup, MID_CleanupChar },
    { NULL }
};

static GMenuItem rmlist[] = {
    { { (unichar_t *) _STR_Rmoverlap, &GIcon_rmoverlap, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'O' }, 'O', ksm_control|ksm_shift, NULL, NULL, FVMenuOverlap, MID_RmOverlap },
    { { (unichar_t *) _STR_Intersect, &GIcon_intersection, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuOverlap, MID_Intersection },
    { { (unichar_t *) _STR_FindIntersections, &GIcon_findinter, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'O' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuOverlap, MID_FindInter },
    { NULL }
};

static GMenuItem eflist[] = {
    { { (unichar_t *) _STR_Inline, &GIcon_inline, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'O' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuInline },
    { { (unichar_t *) _STR_OutlineMn, &GIcon_outline, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuOutline },
    { { (unichar_t *) _STR_Shadow, &GIcon_shadow, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuShadow },
    { { (unichar_t *) _STR_Wireframe, &GIcon_wireframe, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, true, 0, 0, 0, 0, 0, 1, 0, 'W' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuWireframe },
    { NULL }
};

static GMenuItem balist[] = {
    { { (unichar_t *) _STR_Buildaccent, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 'A', ksm_control|ksm_shift, NULL, NULL, FVMenuBuildAccent, MID_BuildAccent },
    { { (unichar_t *) _STR_Buildcomposit, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuBuildComposite, MID_BuildComposite },
    { NULL }
};

static GMenuItem ellist[] = {
    { { (unichar_t *) _STR_Fontinfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 'F', ksm_control|ksm_shift, NULL, NULL, FVMenuFontInfo },
    { { (unichar_t *) _STR_Charinfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, 'I', ksm_control, NULL, NULL, FVMenuCharInfo, MID_CharInfo },
    { { (unichar_t *) _STR_DefaultATT, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'I', ksm_control|ksm_meta, aatlist },
    { { (unichar_t *) _STR_ShowDependents, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'I', ksm_control|ksm_meta, NULL, NULL, FVMenuShowDependents, MID_ShowDependents },
    { { (unichar_t *) _STR_Findprobs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, 'E', ksm_control, NULL, NULL, FVMenuFindProblems, MID_FindProblems },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Bitmapsavail, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, 'B', ksm_control|ksm_shift, NULL, NULL, FVMenuBitmaps, MID_AvailBitmaps },
    { { (unichar_t *) _STR_Regenbitmaps, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 'B', ksm_control, NULL, NULL, FVMenuBitmaps, MID_RegenBitmaps },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Transform, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '\\', ksm_control, NULL, NULL, FVMenuTransform, MID_Transform },
#ifdef PFAEDIT_CONFIG_NONLINEAR
    { { (unichar_t *) _STR_NonLinearTransform, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '|', ksm_shift|ksm_control, NULL, NULL, FVMenuNLTransform, MID_NLTransform },
#endif
    { { (unichar_t *) _STR_Stroke, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'E' }, 'E', ksm_control|ksm_shift, NULL, NULL, FVMenuStroke, MID_Stroke },
#ifdef PFAEDIT_CONFIG_TILEPATH
    { { (unichar_t *) _STR_TilePath, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuTilePath, MID_TilePath },
#endif
    { { (unichar_t *) _STR_Rmoverlap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'O' }, '\0', ksm_control|ksm_shift, rmlist, NULL, NULL, MID_RmOverlap },
    { { (unichar_t *) _STR_Simplify, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_control|ksm_shift, smlist, NULL, NULL, MID_Simplify },
    { { (unichar_t *) _STR_AddExtrema, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'x' }, 'X', ksm_control|ksm_shift, NULL, NULL, FVMenuAddExtrema, MID_AddExtrema },
    { { (unichar_t *) _STR_Round2int, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '_', ksm_control|ksm_shift, NULL, NULL, FVMenuRound2Int, MID_Round },
    { { (unichar_t *) _STR_Effects, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_control|ksm_shift, eflist, NULL, NULL, MID_Effects },
    { { (unichar_t *) _STR_MetaFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, '!', ksm_control|ksm_shift, NULL, NULL, FVMenuMetaFont, MID_MetaFont },
    { { (unichar_t *) _STR_Autotrace, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'r' }, 'T', ksm_control|ksm_shift, NULL, NULL, FVMenuAutotrace, MID_Autotrace },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Correct, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'D', ksm_control|ksm_shift, NULL, NULL, FVMenuCorrectDir, MID_Correct },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Build, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, balist, balistcheck, NULL, MID_BuildAccent },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Mergefonts, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuMergeFonts, MID_MergeFonts },
    { { (unichar_t *) _STR_Interp, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'p' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuInterpFonts, MID_InterpolateFonts },
    { NULL }
};

static GMenuItem dummyall[] = {
    { { (unichar_t *) _STR_All, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 1, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, NULL, NULL, NULL },
    NULL
};

/* Builds up a menu containing all the anchor classes */
static void aplistbuild(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    extern void GMenuItemArrayFree(GMenuItem *mi);

    if ( mi->sub!=dummyall ) {
	GMenuItemArrayFree(mi->sub);
	mi->sub = NULL;
    }

    _aplistbuild(mi,fv->sf,FVMenuAnchorPairs);
}

static GMenuItem cblist[] = {
    { { (unichar_t *) _STR_KernPairs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuKernPairs, MID_KernPairs },
    { { (unichar_t *) _STR_AnchoredPairs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, dummyall, aplistbuild, FVMenuAnchorPairs, MID_AnchorPairs },
    { { (unichar_t *) _STR_Ligatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'L' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuLigatures, MID_Ligatures },
    NULL
};

static void cblistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *sf = fv->sf;
    int i, anyligs=0, anykerns=0;
    PST *pst;

    if ( sf->kerns ) anykerns=true;
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	for ( pst=sf->chars[i]->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_ligature ) {
		anyligs = true;
		if ( anykerns )
    break;
	    }
	}
	if ( sf->chars[i]->kerns!=NULL ) {
	    anykerns = true;
	    if ( anyligs )
    break;
	}
    }

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Ligatures:
	    mi->ti.disabled = !anyligs;
	  break;
	  case MID_KernPairs:
	    mi->ti.disabled = !anykerns;
	  break;
	  case MID_AnchorPairs:
	    mi->ti.disabled = sf->anchor==NULL;
	  break;
	}
    }
}

static GMenuItem vwlist[] = {
    { { (unichar_t *) _STR_NextChar, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'N' }, ']', ksm_control, NULL, NULL, FVMenuChangeChar, MID_Next },
    { { (unichar_t *) _STR_PrevChar, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '[', ksm_control, NULL, NULL, FVMenuChangeChar, MID_Prev },
    { { (unichar_t *) _STR_NextDefChar, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, ']', ksm_control|ksm_meta, NULL, NULL, FVMenuChangeChar, MID_NextDef },
    { { (unichar_t *) _STR_PrevDefChar, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'a' }, '[', ksm_control|ksm_meta, NULL, NULL, FVMenuChangeChar, MID_PrevDef },
    { { (unichar_t *) _STR_Goto, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'G' }, '>', ksm_shift|ksm_control, NULL, NULL, FVMenuGotoChar },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_EncodedView, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'E' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuCompact, MID_EncodedView },
    { { (unichar_t *) _STR_CompactedView, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuCompact, MID_CompactedView },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ShowAtt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuShowAtt },
    { { (unichar_t *) _STR_Combinations, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'b' }, '\0', ksm_shift|ksm_control, cblist, cblistcheck },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ShowHMetrics, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuShowMetrics, MID_ShowHMetrics },
    { { (unichar_t *) _STR_ShowVMetrics, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuShowMetrics, MID_ShowVMetrics },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_24, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '2' }, '2', ksm_control, NULL, NULL, FVMenuSize, MID_24 },
    { { (unichar_t *) _STR_36, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '3' }, '3', ksm_control, NULL, NULL, FVMenuSize, MID_36 },
    { { (unichar_t *) _STR_48, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '4' }, '4', ksm_control, NULL, NULL, FVMenuSize, MID_48 },
    { { (unichar_t *) _STR_72, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '4' }, '7', ksm_control, NULL, NULL, FVMenuSize, MID_72 },
    { { (unichar_t *) _STR_96, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '4' }, '9', ksm_control, NULL, NULL, FVMenuSize, MID_96 },
    { { (unichar_t *) _STR_Antialias, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'A' }, '5', ksm_control, NULL, NULL, FVMenuSize, MID_AntiAlias },
    { { (unichar_t *) _STR_FitToEm, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'F' }, '6', ksm_control, NULL, NULL, FVMenuSize, MID_FitToEm },
    { NULL },			/* Some extra room to show bitmaps */
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }
};

static void vwlistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);
    int i, base;
    BDFFont *bdf;
    unichar_t buffer[50];
    extern void GMenuItemArrayFree(GMenuItem *mi);
    extern GMenuItem *GMenuItemArrayCopy(GMenuItem *mi, uint16 *cnt);
    int pos;
    SplineFont *sf = fv->sf;

    for ( i=0; vwlist[i].ti.text!=(unichar_t *) _STR_FitToEm; ++i );
    base = i+2;
    for ( i=base; vwlist[i].ti.text!=NULL; ++i ) {
	free( vwlist[i].ti.text);
	vwlist[i].ti.text = NULL;
    }

    vwlist[base-1].ti.fg = vwlist[base-1].ti.bg = COLOR_DEFAULT;
    if ( fv->sf->bitmaps==NULL ) {
	vwlist[base-1].ti.line = false;
    } else {
	vwlist[base-1].ti.line = true;
	for ( bdf = fv->sf->bitmaps, i=base;
		i<sizeof(vwlist)/sizeof(vwlist[0])-1 && bdf!=NULL;
		++i, bdf = bdf->next ) {
	    if ( BDFDepth(bdf)==1 )
		u_sprintf( buffer, GStringGetResource(_STR_DPixelBitmap,NULL), bdf->pixelsize );
	    else
		u_sprintf( buffer, GStringGetResource(_STR_DdPixelBitmap,NULL),
			bdf->pixelsize, BDFDepth(bdf) );
	    vwlist[i].ti.text = u_copy(buffer);
	    vwlist[i].ti.checkable = true;
	    vwlist[i].ti.checked = bdf==fv->show;
	    vwlist[i].ti.userdata = bdf;
	    vwlist[i].invoke = FVMenuShowBitmap;
	    vwlist[i].ti.fg = vwlist[i].ti.bg = COLOR_DEFAULT;
	}
    }
    GMenuItemArrayFree(mi->sub);
    mi->sub = GMenuItemArrayCopy(vwlist,NULL);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Next: case MID_Prev:
	    mi->ti.disabled = anychars<0;
	  break;
	  case MID_NextDef:
	    if ( anychars==-1 ) pos = sf->charcnt;
	    else for ( pos = anychars+1; pos<sf->charcnt && sf->chars[pos]==NULL; ++pos );
	    mi->ti.disabled = pos==sf->charcnt;
	  break;
	  case MID_PrevDef:
	    for ( pos = anychars-1; pos>=0 && sf->chars[pos]==NULL; --pos );
	    mi->ti.disabled = pos<0;
	  break;
	  case MID_EncodedView:
	    mi->ti.checked = !fv->sf->compacted;
	  break;
	  case MID_CompactedView:
	    mi->ti.checked = fv->sf->compacted;
	  break;
	  case MID_ShowHMetrics:
	    /*mi->ti.checked = fv->showhmetrics;*/
	  break;
	  case MID_ShowVMetrics:
	    /*mi->ti.checked = fv->showvmetrics;*/
	    mi->ti.disabled = !fv->sf->hasvmetrics;
	  break;
	  case MID_24:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==24);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_36:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==36);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_48:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==48);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_72:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==72);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_96:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==96);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_AntiAlias:
	    mi->ti.checked = (fv->show!=NULL && fv->show->clut!=NULL);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_FitToEm:
	    mi->ti.checked = (fv->show!=NULL && !fv->show->bbsized);
	    mi->ti.disabled = fv->sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	}
    }
}

static GMenuItem histlist[] = {
    { { (unichar_t *) _STR_HStem, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuHistograms, MID_HStemHist },
    { { (unichar_t *) _STR_VStem, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuHistograms, MID_VStemHist },
    { { (unichar_t *) "BlueValues", NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 1, 0, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuHistograms, MID_BlueValuesHist },
    { NULL }
};

static GMenuItem htlist[] = {
    { { (unichar_t *) _STR_Autohint, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, 'H', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoHint, MID_AutoHint },
    { { (unichar_t *) _STR_AutoInstr, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, 'T', ksm_control, NULL, NULL, FVMenuAutoInstr, MID_AutoInstr },
    { { (unichar_t *) _STR_EditInstructions, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, '\0', 0, NULL, NULL, FVMenuEditInstrs, MID_EditInstructions },
    { { (unichar_t *) _STR_Editfpgm, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, FVMenuEditTable, MID_Editfpgm },
    { { (unichar_t *) _STR_Editprep, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, FVMenuEditTable, MID_Editprep },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ClearHints, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuClearHints, MID_ClearHints },
    { { (unichar_t *) _STR_ClearWidthMD, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuClearWidthMD, MID_ClearWidthMD },
    { { (unichar_t *) _STR_ClearInstructions, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuClearInstrs, MID_ClearInstrs },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Histograms, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_shift|ksm_control, histlist },
    { NULL }
};

static GMenuItem mtlist[] = {
    { { (unichar_t *) _STR_Center, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuCenter, MID_Center },
    { { (unichar_t *) _STR_Thirds, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '\0', ksm_control, NULL, NULL, FVMenuCenter, MID_Thirds },
    { { (unichar_t *) _STR_Setwidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, 'L', ksm_control|ksm_shift, NULL, NULL, FVMenuSetWidth, MID_SetWidth },
    { { (unichar_t *) _STR_Setlbearing, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'L' }, 'L', ksm_control, NULL, NULL, FVMenuSetWidth, MID_SetLBearing },
    { { (unichar_t *) _STR_Setrbearing, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, 'R', ksm_control, NULL, NULL, FVMenuSetWidth, MID_SetRBearing },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_SetVWidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSetWidth, MID_SetVWidth },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Autowidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, 'W', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoWidth },
    { { (unichar_t *) _STR_Autokern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, 'K', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoKern },
    { { (unichar_t *) _STR_KernByClasses, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuKernByClasses },
    { { (unichar_t *) _STR_Removeallkern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveKern, MID_RmHKern },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_VKernByClasses, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuVKernByClasses, MID_VKernByClass },
    { { (unichar_t *) _STR_VKernFromHKern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuVKernFromHKern, MID_VKernFromH },
    { { (unichar_t *) _STR_RemoveAllVKern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveVKern, MID_RmVKern },
    { NULL }
};

static GMenuItem cdlist[] = {
    { { (unichar_t *) _STR_Convert2CID, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuConvert2CID, MID_Convert2CID },
    { { (unichar_t *) _STR_ConvertByCMap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuConvertByCMap, MID_ConvertByCMap },
    { { (unichar_t *) _STR_Flatten, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', ksm_control, NULL, NULL, FVMenuFlatten, MID_Flatten },
    { { (unichar_t *) _STR_FlattenByCMap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', ksm_control, NULL, NULL, FVMenuFlattenByCMap, MID_FlattenByCMap },
    { { (unichar_t *) _STR_InsertFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuInsertFont, MID_InsertFont },
    { { (unichar_t *) _STR_InsertBlank, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control, NULL, NULL, FVMenuInsertBlank, MID_InsertBlank },
    { { (unichar_t *) _STR_RemoveFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, '\0', ksm_control, NULL, NULL, FVMenuRemoveFontFromCID, MID_RemoveFromCID },
    { { (unichar_t *) _STR_CIDFontInfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuCIDFontInfo, MID_CIDFontInfo },
    { NULL },				/* Extra room to show sub-font names */
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL }, { NULL },
    { NULL }
};

static void cdlistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, base, j;
    extern void GMenuItemArrayFree(GMenuItem *mi);
    extern GMenuItem *GMenuItemArrayCopy(GMenuItem *mi, uint16 *cnt);
    SplineFont *sub, *cidmaster = fv->cidmaster;

    for ( i=0; cdlist[i].mid!=MID_CIDFontInfo; ++i );
    base = i+2;
    for ( i=base; cdlist[i].ti.text!=NULL; ++i ) {
	free( cdlist[i].ti.text);
	cdlist[i].ti.text = NULL;
    }

    cdlist[base-1].ti.fg = cdlist[base-1].ti.bg = COLOR_DEFAULT;
    if ( cidmaster==NULL ) {
	cdlist[base-1].ti.line = false;
    } else {
	cdlist[base-1].ti.line = true;
	for ( j = 0, i=base; 
		i<sizeof(cdlist)/sizeof(cdlist[0])-1 && j<cidmaster->subfontcnt;
		++i, ++j ) {
	    sub = cidmaster->subfonts[j];
	    cdlist[i].ti.text = uc_copy(sub->fontname);
	    cdlist[i].ti.checkable = true;
	    cdlist[i].ti.checked = sub==fv->sf;
	    cdlist[i].ti.userdata = sub;
	    cdlist[i].invoke = FVMenuShowSubFont;
	    cdlist[i].ti.fg = cdlist[i].ti.bg = COLOR_DEFAULT;
	}
    }
    GMenuItemArrayFree(mi->sub);
    mi->sub = GMenuItemArrayCopy(cdlist,NULL);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Convert2CID: case MID_ConvertByCMap:
	    mi->ti.disabled = cidmaster!=NULL;
	  break;
	  case MID_InsertFont: case MID_InsertBlank:
	    /* OpenType allows at most 255 subfonts (PS allows more, but why go to the effort to make safe font check that? */
	    mi->ti.disabled = cidmaster==NULL || cidmaster->subfontcnt>=255;
	  break;
	  case MID_RemoveFromCID:
	    mi->ti.disabled = cidmaster==NULL || cidmaster->subfontcnt<=1;
	  break;
	  case MID_Flatten: case MID_FlattenByCMap: case MID_CIDFontInfo:
	    mi->ti.disabled = cidmaster==NULL;
	  break;
	}
    }
}

GMenuItem helplist[] = {
    { { (unichar_t *) _STR_Help, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, GK_F1, 0, NULL, NULL, MenuHelp },
    { { (unichar_t *) _STR_Index, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, GK_F1, ksm_control, NULL, NULL, MenuIndex },
    { { (unichar_t *) _STR_About, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, '\0', 0, NULL, NULL, MenuAbout },
    { { (unichar_t *) _STR_LicenseDDD, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, '\0', 0, NULL, NULL, MenuLicense },
    { NULL }
};

GMenuItem fvpopupmenu[] = {
    { { (unichar_t *) _STR_Cut, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 't' }, '\0', ksm_control, NULL, NULL, FVMenuCut, MID_Cut },
    { { (unichar_t *) _STR_Copy, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuCopy, MID_Copy },
    { { (unichar_t *) _STR_Copyref, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, '\0', ksm_control, NULL, NULL, FVMenuCopyRef, MID_CopyRef },
    { { (unichar_t *) _STR_Copywidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, '\0', ksm_control, NULL, NULL, FVMenuCopyWidth, MID_CopyWidth },
    { { (unichar_t *) _STR_Paste, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control, NULL, NULL, FVMenuPaste, MID_Paste },
    { { (unichar_t *) _STR_Clear, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, 0, 0, NULL, NULL, FVMenuClear, MID_Clear },
    { { (unichar_t *) _STR_CopyFgToBg, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCopyFgBg, MID_CopyFgToBg },
    { { (unichar_t *) _STR_Unlinkref, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'U' }, '\0', ksm_control, NULL, NULL, FVMenuUnlinkRef, MID_UnlinkRef },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Charinfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuCharInfo, MID_CharInfo },
    { { (unichar_t *) _STR_Transform, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '\0', ksm_control, NULL, NULL, FVMenuTransform, MID_Transform },
    { { (unichar_t *) _STR_Stroke, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'E' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuStroke, MID_Stroke },
/*    { { (unichar_t *) _STR_Rmoverlap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'O' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuOverlap, MID_RmOverlap },*/
/*    { { (unichar_t *) _STR_Simplify, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSimplify, MID_Simplify },*/
/*    { { (unichar_t *) _STR_AddExtrema, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'x' }, 'X', ksm_control|ksm_shift, NULL, NULL, FVMenuAddExtrema, MID_AddExtrema },*/
    { { (unichar_t *) _STR_Round2int, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRound2Int, MID_Round },
    { { (unichar_t *) _STR_Correct, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCorrectDir, MID_Correct },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Autohint, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoHint, MID_AutoHint },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Center, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuCenter, MID_Center },
    { { (unichar_t *) _STR_Setwidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSetWidth, MID_SetWidth },
    { { (unichar_t *) _STR_SetVWidth, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSetWidth, MID_SetVWidth },
    { NULL }
};

static GMenuItem mblist[] = {
    { { (unichar_t *) _STR_File, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 0, 0, fllist, fllistcheck },
    { { (unichar_t *) _STR_Edit, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'E' }, 0, 0, edlist, edlistcheck },
    { { (unichar_t *) _STR_Element, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, 0, 0, ellist, ellistcheck },
    { { (unichar_t *) _STR_Hints, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'i' }, 0, 0, htlist, htlistcheck },
    { { (unichar_t *) _STR_View, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, 0, 0, vwlist, vwlistcheck },
    { { (unichar_t *) _STR_Metric, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 0, 0, mtlist, mtlistcheck },
    { { (unichar_t *) _STR_CID, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, 0, 0, cdlist, cdlistcheck },
    { { (unichar_t *) _STR_Window, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, 0, 0, NULL, WindowMenuBuild, NULL },
    { { (unichar_t *) _STR_Help, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, 0, 0, helplist, NULL },
    { NULL }
};

void FVRefreshChar(FontView *fv,BDFFont *bdf,int enc) {
    BDFChar *bdfc = bdf->chars[enc];
    int i, j;
    MetricsView *mv;

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;

    for ( fv=fv->sf->fv; fv!=NULL; fv = fv->nextsame ) {
	for ( mv=fv->metrics; mv!=NULL; mv=mv->next )
	    MVRefreshChar(mv,fv->sf->chars[enc]);
	if ( fv->show != bdf )
    continue;
	i = enc / fv->colcnt;
	j = enc - i*fv->colcnt;
	i -= fv->rowoff;
	if ( i>=0 && i<fv->rowcnt ) {
	    struct _GImage base;
	    GImage gi;
	    GClut clut;
	    GRect old, box;

	    if ( bdfc==NULL )
		bdfc = BDFPieceMeal(bdf,enc);

	    memset(&gi,'\0',sizeof(gi));
	    memset(&base,'\0',sizeof(base));
	    if ( bdfc->byte_data ) {
		gi.u.image = &base;
		base.image_type = it_index;
		base.clut = bdf->clut;
		GDrawSetDither(NULL, false);	/* on 8 bit displays we don't want any dithering */
		base.trans = -1;
		/*base.clut->trans_index = 0;*/
	    } else {
		memset(&clut,'\0',sizeof(clut));
		gi.u.image = &base;
		base.image_type = it_mono;
		base.clut = &clut;
		clut.clut_len = 2;
		clut.clut[0] = GDrawGetDefaultBackground(NULL);
	    }

	    base.data = bdfc->bitmap;
	    base.bytes_per_line = bdfc->bytes_per_line;
	    base.width = bdfc->xmax-bdfc->xmin+1;
	    base.height = bdfc->ymax-bdfc->ymin+1;
	    box.x = j*fv->cbw+1; box.width = fv->cbw-1;
	    box.y = i*fv->cbh+FV_LAB_HEIGHT+1; box.height = fv->cbw;
	    GDrawPushClip(fv->v,&box,&old);
	    GDrawFillRect(fv->v,&box,GDrawGetDefaultBackground(NULL));
	    if ( fv->magnify>1 ) {
		GDrawDrawImageMagnified(fv->v,&gi,NULL,
			j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2,
			i*fv->cbh+FV_LAB_HEIGHT+1+fv->magnify*(fv->show->ascent-bdfc->ymax),
			fv->magnify*base.width,fv->magnify*base.height);
	    } else
		GDrawDrawImage(fv->v,&gi,NULL,
			j*fv->cbw+(fv->cbw-1-base.width)/2,
			i*fv->cbh+FV_LAB_HEIGHT+1+fv->show->ascent-bdfc->ymax);
	    GDrawPopClip(fv->v,&old);
	    if ( fv->selected[enc] ) {
		GDrawSetXORBase(fv->v,GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v)));
		GDrawSetXORMode(fv->v);
		GDrawFillRect(fv->v,&box,XOR_COLOR);
		GDrawSetCopyMode(fv->v);
	    }
	    GDrawSetDither(NULL, true);
	}
    }
}

void FVRegenChar(FontView *fv,SplineChar *sc) {
    struct splinecharlist *dlist;
    MetricsView *mv;

    if ( fv->v==NULL )			/* Can happen in scripts */
return;

    sc->changedsincelasthinted = true;
    if ( sc->enc>=fv->filled->charcnt )
	GDrawIError("Character out of bounds in bitmap font %d>=%d", sc->enc, fv->filled->charcnt );
    else
	BDFCharFree(fv->filled->chars[sc->enc]);
    fv->filled->chars[sc->enc] = NULL;
		/* FVRefreshChar does NOT do this for us */
    for ( mv=fv->metrics; mv!=NULL; mv=mv->next )
	MVRegenChar(mv,sc);

    FVRefreshChar(fv,fv->filled,sc->enc);
#if HANYANG
    if ( sc->compositionunit && fv->sf->rules!=NULL )
	Disp_RefreshChar(fv->sf,sc);
#endif

    for ( dlist=sc->dependents; dlist!=NULL; dlist=dlist->next )
	FVRegenChar(fv,dlist->sc);
}

SplineChar *SCBuildDummy(SplineChar *dummy,SplineFont *sf,int i) {
    extern unsigned short unicode_from_adobestd[256];
    static char namebuf[100];
    Encoding *item=NULL;
    int j;

    memset(dummy,'\0',sizeof(*dummy));
    dummy->color = COLOR_DEFAULT;
    dummy->enc = i;
    if ( sf->compacted ) {
	for ( j=i-1; j>=0; --j )
	    if ( j<sf->charcnt && sf->chars[j]!=NULL )
	break;
	if ( j>0 )
	    dummy->old_enc = sf->chars[j]->old_enc+(i-j);
	else
	    dummy->old_enc = i;
    }
    if ( sf->cidmaster!=NULL ) {
	/* CID fonts don't have encodings, instead we must look up the cid */
	if ( sf->cidmaster->loading_cid_map )
	    dummy->unicodeenc = -1;
	else
	    dummy->unicodeenc = CID2NameEnc(FindCidMap(sf->cidmaster->cidregistry,sf->cidmaster->ordering,sf->cidmaster->supplement,sf->cidmaster),
		    i,namebuf,sizeof(namebuf));
    } else if ( sf->encoding_name==em_unicode )
	dummy->unicodeenc = i<65536 ? i : -1;
    else if ( sf->encoding_name==em_unicode4 )
	dummy->unicodeenc = i<=0x7fffffff ? i : -1;
    else if ( sf->encoding_name>=em_unicodeplanes && sf->encoding_name<=em_unicodeplanesmax )
	dummy->unicodeenc = i<65536 ? i+((sf->encoding_name-em_unicodeplanes)<<16) : -1;
    else if ( sf->encoding_name==em_adobestandard )
	dummy->unicodeenc = i>=256?-1:unicode_from_adobestd[i];
    else if ( sf->encoding_name==em_none )
	dummy->unicodeenc = -1;
    else if ( sf->encoding_name>=em_base ) {
	dummy->unicodeenc = -1;
	for ( item=enclist; item!=NULL && item->enc_num!=sf->encoding_name; item=item->next );
	if ( item!=NULL && i>=item->char_cnt ) item = NULL;
	if ( item!=NULL )
	    dummy->unicodeenc = item->unicode[i];
    } else if ( sf->encoding_name==em_big5 ) {
	if ( i<160 )
	    dummy->unicodeenc = i;
	else if ( i>=0xa100 )
	    dummy->unicodeenc = unicode_from_big5[i-0xa100];
	else
	    dummy->unicodeenc = -1;
    } else if ( sf->encoding_name==em_big5hkscs ) {
	if ( i<0x80 )
	    dummy->unicodeenc = i;
	else if ( i>=0x8100 )
	    dummy->unicodeenc = unicode_from_big5hkscs[i-0x8100];
	else
	    dummy->unicodeenc = -1;
    } else if ( sf->encoding_name==em_johab ) {
	if ( i<160 )
	    dummy->unicodeenc = i;
	else if ( i>=0x8400 )
	    dummy->unicodeenc = unicode_from_johab[i-0x8400];
	else
	    dummy->unicodeenc = -1;
    } else if ( sf->encoding_name==em_wansung ) {
	if ( i<160 )
	    dummy->unicodeenc = i;
	else if ( (i&0xff00)>=0xa100 && (i&0xff)>=0xa1 &&
		    (i&0xff00)<0xa100+(94<<8) && (i&0xff)<0xa1+94 ) {
	    int temp = i-0xa1a1;
	    temp = (temp>>8)*94 + (temp&0xff);
	    temp = unicode_from_ksc5601[temp];
	    if ( temp==0 ) temp = -1;
	    dummy->unicodeenc = temp;
	} else
	    dummy->unicodeenc = -1;
    } else if ( sf->encoding_name==em_sjis ) {
	if ( i<0x80 )
	    dummy->unicodeenc = i;
	else if ( i>=0xa1 && i<=0xdf )
	    dummy->unicodeenc = unicode_from_jis201[i];
	else if (( ((i>>8)>=129 && (i>>8)<=159) || ((i>>8)>=224 && (i>>8)<=0xef) ) &&
		 ( (i&0xff)>=64 && (i&0xff)<=252 && (i&0xff)!=127 )) {
	    int ch1 = i>>8, ch2 = i&0xff;
	    int temp;
	    if ( ch1 >= 129 && ch1<= 159 )
		ch1 -= 112;
	    else
		ch1 -= 176;
	    ch1 <<= 1;
	    if ( ch2>=159 )
		ch2-= 126;
	    else if ( ch2>127 ) {
		--ch1;
		ch2 -= 32;
	    } else {
		--ch1;
		ch2 -= 31;
	    }
	    temp = (ch1-0x21)*94+(ch2-0x21);
	    if ( temp>=94*94 )
		temp = -1;
	    else
		temp = unicode_from_jis208[(ch1-0x21)*94+(ch2-0x21)];
	    if ( temp==0 ) temp = -1;
	    dummy->unicodeenc = temp;
	} else
	    dummy->unicodeenc = -1;
    } else if ( sf->encoding_name==em_jis208 && i<96*94 && i%96!=0 && i%96!=95 )
	dummy->unicodeenc = unicode_from_jis208[(i/96)*94+(i%96-1)];
    else if ( sf->encoding_name==em_jis212 && i<96*94 && i%96!=0 && i%96!=95 )
	dummy->unicodeenc = unicode_from_jis212[(i/96)*94+(i%96-1)];
    else if ( sf->encoding_name==em_ksc5601 && i<96*94 && i%96!=0 && i%96!=95 )
	dummy->unicodeenc = unicode_from_ksc5601[(i/96)*94+(i%96-1)];
    else if ( sf->encoding_name==em_gb2312 && i<96*94 && i%96!=0 && i%96!=95 )
	dummy->unicodeenc = unicode_from_gb2312[(i/96)*94+(i%96-1)];
    else if ( sf->encoding_name>=em_jis208 )
	dummy->unicodeenc = -1;
    else
	dummy->unicodeenc = i>=256?-1:unicode_from_alphabets[sf->encoding_name+3][i];
    if ( dummy->unicodeenc==0 && i!=0 )
	dummy->unicodeenc = -1;
    if ( sf->cidmaster!=NULL )
	dummy->name = namebuf;
    else if ( (dummy->unicodeenc>=0 && dummy->unicodeenc<' ') ||
	    (dummy->unicodeenc>=0x7f && dummy->unicodeenc<0xa0) )
	/* standard controls */;
    else if ( dummy->unicodeenc!=-1  ) {
	if ( dummy->unicodeenc<psunicodenames_cnt )
	    dummy->name = (char *) psunicodenames[dummy->unicodeenc];
	if ( dummy->name==NULL ) {
	    if ( dummy->unicodeenc==0x2d )
		dummy->name = "hyphen-minus";
	    else if ( dummy->unicodeenc==0xad )
		dummy->name = "softhyphen";
	    else if ( dummy->unicodeenc==0x00 )
		dummy->name = ".notdef";
	    else if ( dummy->unicodeenc==0xa0 )
		dummy->name = "nonbreakingspace";
	    else {
		if ( dummy->unicodeenc>=0x10000 )
		    sprintf( namebuf, "u%04X", dummy->unicodeenc);
		else
		    sprintf( namebuf, "uni%04X", dummy->unicodeenc);
		dummy->name = namebuf;
	    }
	}
    } else if ( item!=NULL && item->psnames!=NULL )
	dummy->name = item->psnames[i];
    if ( dummy->name==NULL ) {
	if ( dummy->unicodeenc!=-1 || i<256 )
	    dummy->name = ".notdef";
	else {
	    int j;
	    sprintf( namebuf, "NameMe-%d", i);
	    j=0;
	    while ( SFGetChar(sf,-1,namebuf)!=NULL )
		sprintf( namebuf, "NameMe-%d.%d", i, ++j);
	    dummy->name = namebuf;
	}
    }
    dummy->width = dummy->vwidth = sf->ascent+sf->descent;
    if ( dummy->unicodeenc>0 && dummy->unicodeenc<0x10000 &&
	    iscombining(dummy->unicodeenc))
	dummy->width = 0;		/* Mark characters should be 0 width */
    dummy->parent = sf;
    dummy->orig_pos = 0xffff;
return( dummy );
}

SplineChar *SFMakeChar(SplineFont *sf,int i) {
    SplineChar dummy, *sc;
    SplineFont *ssf;
    int j;

    if ( sf->subfontcnt!=0 ) {
	ssf = NULL;
	for ( j=0; j<sf->subfontcnt; ++j )
	    if ( i<sf->subfonts[j]->charcnt ) {
		ssf = sf->subfonts[j];
		if ( ssf->chars[i]!=NULL ) {
return( ssf->chars[i] );
		}
	    }
	sf = ssf;
    }

    if ( (sc = sf->chars[i])==NULL ) {
	SCBuildDummy(&dummy,sf,i);
	sf->chars[i] = sc = SplineCharCreate();
	*sc = dummy;
	sc->name = copy(sc->name);
	SCLigDefault(sc);
	sc->parent = sf;
    }
return( sc );
}

static GImage *GImageCropAndRotate(GImage *unrot) {
    struct _GImage *unbase = unrot->u.image, *rbase;
    int xmin = unbase->width, xmax = -1, ymin = unbase->height, ymax = -1;
    int i,j, ii;
    GImage *rot;

    for ( i=0; i<unbase->height; ++i ) {
	for ( j=0; j<unbase->width; ++j ) {
	    if ( !(unbase->data[i*unbase->bytes_per_line+(j>>3)]&(0x80>>(j&7))) ) {
		if ( j<xmin ) xmin = j;
		if ( j>xmax ) xmax = j;
		if ( i>ymax ) ymax = i;
		if ( i<ymin ) ymin = i;
	    }
	}
    }
    if ( xmax==-1 )
return( NULL );

    rot = GImageCreate(it_mono,ymax-ymin+1,xmax-xmin+1);
    if ( rot==NULL )
return( NULL );
    rbase = rot->u.image;
    memset(rbase->data,-1,rbase->height*rbase->bytes_per_line);
    for ( i=ymin; i<=ymax; ++i ) {
	for ( j=xmin; j<=xmax; ++j ) {
	    if ( !(unbase->data[i*unbase->bytes_per_line+(j>>3)]&(0x80>>(j&7)) )) {
		ii = ymax-i;
		rbase->data[(j-xmin)*rbase->bytes_per_line+(ii>>3)] &= ~(0x80>>(ii&7));
	    }
	}
    }
    rbase->trans = 1;
return( rot );
}

static GImage *UniGetRotatedGlyph(SplineFont *sf, SplineChar *sc,int uni) {
    int cid=-1;
    SplineChar *cidsc, dummy;
    static GWindow pixmap=NULL;
    GRect r;
    unichar_t buf[2];
    GImage *unrot, *rot;

    if ( uni!=-1 )
	/* Do nothing */;
    else if ( sscanf(sc->name,"vertuni%x", &uni)!=1 &&
	    (sscanf( sc->name, "cid-%d", &cid)==1 ||
	     sscanf( sc->name, "vertcid_%d", &cid)==1 ||	/* Obsolete names */
	     sscanf( sc->name, "cid_%d", &cid)==1 )) {
	cidsc = NULL;
	if ( !sf->compacted ) {
	    if ( sf->cidmaster==NULL ) {
		if ( cid>=0 && cid<sf->charcnt )
		    cidsc = sf->chars[cid];
	    } else {
		cidsc = SCBuildDummy(&dummy,sf,cid);
	    }
	} else {
	    int i;
	    for (i=0; i<sf->charcnt && sf->chars[i]->old_enc!=cid; i++);
	    cidsc = SCBuildDummy(&dummy,sf,i);
	}
	if ( cidsc==NULL || cidsc->unicodeenc==-1 )
return( NULL );
	uni = cidsc->unicodeenc;
    }
    if ( uni&0x10000 ) uni -= 0x10000;		/* Bug in some old cidmap files */
    if ( uni<0 || uni>0xffff )
return( NULL );

    if ( pixmap==NULL ) {
	pixmap = GDrawCreateBitmap(NULL,2*FV_LAB_HEIGHT,2*FV_LAB_HEIGHT,NULL);
	if ( pixmap==NULL )
return( NULL );
	GDrawSetFont(pixmap,sf->fv->header);
    }
    r.x = r.y = 0;
    r.width = r.height = 2*FV_LAB_HEIGHT;
    GDrawFillRect(pixmap,&r,1);
    buf[0] = uni; buf[1] = 0;
    GDrawDrawText(pixmap,2,FV_LAB_HEIGHT,buf,1,NULL,0);
    unrot = GDrawCopyScreenToImage(pixmap,&r);
    if ( unrot==NULL )
return( NULL );

    rot = GImageCropAndRotate(unrot);
    GImageDestroy(unrot);
return( rot );
}

static int Use2ByteEnc(FontView *fv,SplineChar *sc, unichar_t *buf,FontMods *mods) {
    int ch1 = sc->enc>>8, ch2 = sc->enc&0xff;

    switch ( fv->sf->encoding_name ) {
      case em_big5: case em_big5hkscs:
	if ( !GDrawFontHasCharset(fv->header,em_big5))
return( false);
	if ( ch1<0xa1 || ch1>0xf9 || ch2<0x40 || ch2>0xfe || sc->enc> 0xf9fe )
return( false );
	mods->has_charset = true; mods->charset = em_big5;
	buf[0] = sc->enc;
	buf[1] = 0;
return( true );
      break;
      case em_sjis:
	if ( !GDrawFontHasCharset(fv->header,em_jis208))
return( false);
	if ( ch1>=129 && ch1<=159 )
	    ch1-=112;
	else if ( ch1>=0xe0 && ch1<=0xef )
	    ch1-=176;
	else
return( false );
	ch1<<=1;
	if ( ch2 == 127 )
return( false );
	else if ( ch2>=159 )
	    ch2-=126;
	else if ( ch2>127 ) {
	    --ch1;
	    ch2 -= 32;
	} else {
	    -- ch1;
	    ch2 -= 31;
	}
	mods->has_charset = true; mods->charset = em_jis208;
	buf[0] = (ch1<<8) | ch2;
	buf[1] = 0;
return( true );
      break;
      case em_wansung:
	if ( !GDrawFontHasCharset(fv->header,em_ksc5601))
return( false);
	if ( ch1<0xa1 || ch1>0xfd || ch2<0xa1 || ch2>0xfe || sc->enc > 0xfdfe )
return( false );
	mods->has_charset = true; mods->charset = em_ksc5601;
	buf[0] = sc->enc-0x8080;
	buf[1] = 0;
return( true );
      break;
      case em_ksc5601: case em_jis208: case em_jis212:
	if ( !GDrawFontHasCharset(fv->header,fv->sf->encoding_name))
return( false);
	ch1 = sc->enc/96; ch2 = (sc->enc%96)-1;
	if ( ch1>0x7d-0x21 || ch2>94 || ch2<0 )
return( false );
	mods->has_charset = true; mods->charset = fv->sf->encoding_name;
	buf[0] = ((ch1+0x21)<<8) + ch2+0x21;
	buf[1] = 0;
return( true );
      break;
      default:
return( false );
    }
}

static void FVExpose(FontView *fv,GWindow pixmap,GEvent *event) {
    int i, j, width;
    int changed;
    GRect old, old2, box;
    GClut clut;
    struct _GImage base;
    GImage gi;
    SplineChar dummy;
    int italic, wasitalic=0;
    GImage *rotated=NULL;
    int em = fv->sf->ascent+fv->sf->descent;
    int yorg = fv->magnify*(fv->show->ascent-fv->sf->vertical_origin*fv->show->pixelsize/em);

    memset(&gi,'\0',sizeof(gi));
    memset(&base,'\0',sizeof(base));
    if ( fv->show->clut!=NULL ) {
	gi.u.image = &base;
	base.image_type = it_index;
	base.clut = fv->show->clut;
	GDrawSetDither(NULL, false);
	base.trans = -1;
	/*base.clut->trans_index = 0;*/
    } else {
	memset(&clut,'\0',sizeof(clut));
	gi.u.image = &base;
	base.image_type = it_mono;
	base.clut = &clut;
	clut.clut_len = 2;
	clut.clut[0] = GDrawGetDefaultBackground(NULL);
    }

    GDrawSetFont(pixmap,fv->header);
    GDrawPushClip(pixmap,&event->u.expose.rect,&old);
    GDrawFillRect(pixmap,NULL,GDrawGetDefaultBackground(NULL));
    for ( i=0; i<=fv->rowcnt; ++i ) {
	GDrawDrawLine(pixmap,0,i*fv->cbh,fv->width,i*fv->cbh,0);
	GDrawDrawLine(pixmap,0,i*fv->cbh+FV_LAB_HEIGHT,fv->width,i*fv->cbh+FV_LAB_HEIGHT,0x808080);
    }
    for ( i=0; i<=fv->colcnt; ++i )
	GDrawDrawLine(pixmap,i*fv->cbw,0,i*fv->cbw,fv->height,0);
    for ( i=event->u.expose.rect.y/fv->cbh; i<=fv->rowcnt &&
	    (event->u.expose.rect.y+event->u.expose.rect.height+fv->cbh-1)/fv->cbh; ++i ) for ( j=0; j<fv->colcnt; ++j ) {
	int index = (i+fv->rowoff)*fv->colcnt+j;
	italic = false;
	if ( fv->mapping!=NULL ) {
	    if ( index>=fv->mapcnt ) index = fv->sf->charcnt;
	    else
		index = fv->mapping[index];
	}
	if ( index < fv->sf->charcnt ) {
	    SplineChar *sc = fv->sf->chars[index];
	    unichar_t buf[2];
	    Color fg = 0;
	    BDFChar *bdfc;
	    FontMods *mods=NULL;
	    static FontMods for_charset;
	    if ( sc==NULL )
		sc = SCBuildDummy(&dummy,fv->sf,index);
	    if ( sc->unicodeenc==0xad )
		buf[0] = '-';
	    else if ( Use2ByteEnc(fv,sc,buf,&for_charset))
		mods = &for_charset;
	    else if ( sc->unicodeenc!=-1 && sc->unicodeenc<65536 )
		buf[0] = sc->unicodeenc;
#if HANYANG
	    else if ( sc->compositionunit ) {
		if ( sc->jamo<19 )
		    buf[0] = 0x1100+sc->jamo;
		else if ( sc->jamo<19+21 )
		    buf[0] = 0x1161 + sc->jamo-19;
		else	/* Leave a hole for the blank char */
		    buf[0] = 0x11a8 + sc->jamo-(19+21+1);
	    }
#endif
	    else {
		char *pt = strchr(sc->name,'.');
		buf[0] = '?';
		fg = 0xff0000;
		if ( pt!=NULL ) {
		    int i, n = pt-sc->name;
		    char *end;
		    if ( n==7 && sc->name[0]=='u' && sc->name[1]=='n' && sc->name[2]=='i' &&
			    (i=strtol(sc->name+3,&end,16), end-sc->name==7))
			buf[0] = i;
		    else if ( n>=5 && n<=7 && sc->name[0]=='u' &&
			    (i=strtol(sc->name+1,&end,16), end-sc->name==n))
			buf[0] = i;
		    else for ( i=0; i<psunicodenames_cnt; ++i )
			if ( psunicodenames[i]!=NULL && strncmp(sc->name,psunicodenames[i],n)==0 &&
				psunicodenames[i][n]=='\0' ) {
			    buf[0] = i;
		    break;
			}
		    if ( strstr(pt,".vert")!=NULL )
			rotated = UniGetRotatedGlyph(fv->sf,sc,buf[0]!='?'?buf[0]:-1);
		    if ( buf[0]!='?' ) {
			fg = 0;
			if ( strstr(pt,".italic")!=NULL )
			    italic = true;
		    }
		} else if ( strncmp(sc->name,"hwuni",5)==0 ) {
		    int uni=-1;
		    sscanf(sc->name,"hwuni%x", &uni );
		    if ( uni!=-1 ) buf[0] = uni;
		} else if ( strncmp(sc->name,"italicuni",9)==0 ) {
		    int uni=-1;
		    sscanf(sc->name,"italicuni%x", &uni );
		    if ( uni!=-1 ) { buf[0] = uni; italic=true; }
		    fg = 0x000000;
		} else if ( strncmp(sc->name,"vertcid_",8)==0 ||
			strncmp(sc->name,"vertuni",7)==0 ) {
		    rotated = UniGetRotatedGlyph(fv->sf,sc,-1);
		}
	    }
	    if ( sc->backgroundsplines!=NULL || sc->backimages!=NULL || sc->color!=COLOR_DEFAULT ) {
		GRect r;
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = FV_LAB_HEIGHT-1;
		GDrawFillRect(pixmap,&r,sc->color!=COLOR_DEFAULT?sc->color:0x808080);
	    }
	    if ( rotated!=NULL ) {
		GRect r;
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = FV_LAB_HEIGHT-1;
		GDrawPushClip(pixmap,&r,&old2);
		GDrawDrawImage(pixmap,rotated,NULL,j*fv->cbw+2,i*fv->cbh+2);
		GDrawPopClip(pixmap,&old2);
		GImageDestroy(rotated);
		rotated = NULL;
	    } else {
		if ( italic!=wasitalic ) GDrawSetFont(pixmap,italic?fv->iheader:fv->header);
		width = GDrawGetTextWidth(pixmap,buf,1,NULL);
		if ( sc->unicodeenc<0x80 || sc->unicodeenc>=0xa0 )
		    GDrawDrawText(pixmap,j*fv->cbw+(fv->cbw-1-width)/2,i*fv->cbh+FV_LAB_HEIGHT-2,buf,1,mods,fg);
		wasitalic = italic;
	    }
	    changed = sc->changed;
	    if ( fv->sf->onlybitmaps )
		changed = fv->show->chars[index]==NULL? false : fv->show->chars[index]->changed;
	    if ( changed ) {
		GRect r;
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = FV_LAB_HEIGHT-1;
		GDrawSetXORBase(pixmap,GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v)));
		GDrawSetXORMode(pixmap);
		GDrawFillRect(pixmap,&r,0x000000);
		GDrawSetCopyMode(pixmap);
	    }

	    if ( fv->show!=NULL && fv->show->piecemeal &&
		    fv->show->chars[index]==NULL && fv->sf->chars[index]!=NULL )
		BDFPieceMeal(fv->show,index);

	    if ( fv->show!=NULL && fv->show->chars[index]==NULL &&
		    SCWorthOutputting(fv->sf->chars[index]) ) {
		/* If we have an outline but no bitmap for this slot */
		box.x = j*fv->cbw+1; box.width = fv->cbw-2;
		box.y = i*fv->cbh+14+2; box.height = box.width+1;
		GDrawDrawRect(pixmap,&box,0xff0000);
		++box.x; ++box.y; box.width -= 2; box.height -= 2;
		GDrawDrawRect(pixmap,&box,0xff0000);
/* When reencoding a font we can find times where index>=show->charcnt */
	    } else if ( fv->show!=NULL && index<fv->show->charcnt &&
		    fv->show->chars[index]!=NULL ) {
		bdfc = fv->show->chars[index];
		base.data = bdfc->bitmap;
		base.bytes_per_line = bdfc->bytes_per_line;
		base.width = bdfc->xmax-bdfc->xmin+1;
		base.height = bdfc->ymax-bdfc->ymin+1;
		box.x = j*fv->cbw; box.width = fv->cbw;
		box.y = i*fv->cbh+14+1; box.height = box.width+1;
		GDrawPushClip(pixmap,&box,&old2);
		if ( !fv->sf->onlybitmaps &&
			sc->splines==NULL && sc->refs==NULL && !sc->widthset &&
			!(bdfc->xmax<=0 && bdfc->xmin==0 && bdfc->ymax<=0 && bdfc->ymax==0) ) {
		    /* If we have a bitmap but no outline character... */
		    GRect b;
		    b.x = box.x+1; b.y = box.y+1; b.width = box.width-2; b.height = box.height-2;
		    GDrawDrawRect(pixmap,&b,0x008000);
		    ++b.x; ++b.y; b.width -= 2; b.height -= 2;
		    GDrawDrawRect(pixmap,&b,0x008000);
		}
		if ( fv->magnify>1 ) {
		    GDrawDrawImageMagnified(pixmap,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2,
			    i*fv->cbh+FV_LAB_HEIGHT+1+fv->magnify*(fv->show->ascent-bdfc->ymax),
			    fv->magnify*base.width,fv->magnify*base.height);
		} else
		    GDrawDrawImage(pixmap,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-base.width)/2,
			    i*fv->cbh+FV_LAB_HEIGHT+1+fv->show->ascent-bdfc->ymax);
		if ( fv->showhmetrics ) {
		    int x1, x0 = j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2- bdfc->xmin*fv->magnify;
		    /* Draw advance width & horizontal origin */
		    if ( fv->showhmetrics&fvm_origin )
			GDrawDrawLine(pixmap,x0,i*fv->cbh+FV_LAB_HEIGHT+yorg-3,x0,
				i*fv->cbh+FV_LAB_HEIGHT+yorg+2,METRICS_ORIGIN);
		    x1 = x0 + bdfc->width;
		    if ( fv->showhmetrics&fvm_advanceat )
			GDrawDrawLine(pixmap,x1,i*fv->cbh+FV_LAB_HEIGHT+1,x1,
				(i+1)*fv->cbh-1,METRICS_ADVANCE);
		    if ( fv->showhmetrics&fvm_advanceto )
			GDrawDrawLine(pixmap,x0,(i+1)*fv->cbh-2,x1,
				(i+1)*fv->cbh-2,METRICS_ADVANCE);
		}
		if ( fv->showvmetrics ) {
		    int x0 = j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2- bdfc->xmin*fv->magnify
			    + fv->magnify*fv->show->pixelsize/2;
		    int y0 = i*fv->cbh+FV_LAB_HEIGHT+yorg;
		    int yvw = y0 + fv->magnify*sc->vwidth*fv->show->pixelsize/em;
		    if ( fv->showvmetrics&fvm_baseline )
			GDrawDrawLine(pixmap,x0,i*fv->cbh+FV_LAB_HEIGHT+1,x0,
				(i+1)*fv->cbh-1,METRICS_BASELINE);
		    if ( fv->showvmetrics&fvm_advanceat )
			GDrawDrawLine(pixmap,j*fv->cbw,yvw,(j+1)*fv->cbw,
				yvw,METRICS_ADVANCE);
		    if ( fv->showvmetrics&fvm_advanceto )
			GDrawDrawLine(pixmap,j*fv->cbw+2,y0,j*fv->cbw+2,
				yvw,METRICS_ADVANCE);
		    if ( fv->showvmetrics&fvm_origin )
			GDrawDrawLine(pixmap,x0-3,i*fv->cbh+FV_LAB_HEIGHT+yorg,x0+2,i*fv->cbh+FV_LAB_HEIGHT+yorg,METRICS_ORIGIN);
		}
		GDrawPopClip(pixmap,&old2);
	    }
	    if ( fv->selected[index] ) {
		box.x = j*fv->cbw+1; box.width = fv->cbw-1;
		box.y = i*fv->cbh+FV_LAB_HEIGHT+1; box.height = fv->cbw;
		GDrawSetXORMode(pixmap);
		GDrawSetXORBase(pixmap,GDrawGetDefaultBackground(NULL));
		GDrawFillRect(pixmap,&box,XOR_COLOR);
		GDrawSetCopyMode(pixmap);
	    }
	}
    }
    if ( fv->showhmetrics&fvm_baseline ) {
	int baseline = (fv->sf->ascent*fv->magnify*fv->show->pixelsize)/em+1;
	for ( i=0; i<=fv->rowcnt; ++i )
	    GDrawDrawLine(pixmap,0,i*fv->cbh+FV_LAB_HEIGHT+baseline,fv->width,i*fv->cbh+FV_LAB_HEIGHT+baseline,METRICS_BASELINE);
    }
    GDrawPopClip(pixmap,&old);
    GDrawSetDither(NULL, true);
}

static char *chosung[] = { "G", "GG", "N", "D", "DD", "L", "M", "B", "BB", "S", "SS", "", "J", "JJ", "C", "K", "T", "P", "H", NULL };
static char *jungsung[] = { "A", "AE", "YA", "YAE", "EO", "E", "YEO", "YE", "O", "WA", "WAE", "OE", "YO", "U", "WEO", "WE", "WI", "YU", "EU", "YI", "I", NULL };
static char *jongsung[] = { "", "G", "GG", "GS", "N", "NJ", "NH", "D", "L", "LG", "LM", "LB", "LS", "LT", "LP", "LH", "M", "B", "BS", "S", "SS", "NG", "J", "C", "K", "T", "P", "H", NULL };

static void FVDrawInfo(FontView *fv,GWindow pixmap,GEvent *event) {
    GRect old, r;
    char buffer[250];
    unichar_t ubuffer[250];
    Color bg = GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(pixmap));
    SplineChar *sc, dummy;
    SplineFont *sf = fv->sf;

    if ( event->u.expose.rect.y+event->u.expose.rect.height<=fv->mbh )
return;

    GDrawSetFont(pixmap,fv->header);
    GDrawPushClip(pixmap,&event->u.expose.rect,&old);

    r.x = 0; r.width = fv->width; r.y = fv->mbh; r.height = fv->infoh;
    GDrawFillRect(pixmap,&r,bg);
    if ( fv->end_pos>=sf->charcnt || fv->pressed_pos>=sf->charcnt )
	fv->end_pos = fv->pressed_pos = -1;	/* Can happen after reencoding */
    if ( fv->end_pos == -1 )
return;

    if ( sf->remap!=NULL ) {
	int localenc = fv->end_pos;
	struct remap *map = sf->remap;
	while ( map->infont!=-1 ) {
	    if ( localenc>=map->infont && localenc<=map->infont+(map->lastenc-map->firstenc) ) {
		localenc += map->firstenc-map->infont;
	break;
	    }
	    ++map;
	}
	sprintf( buffer, "%-5u (0x%04x) ", localenc, localenc );
    } else if ( sf->encoding_name<em_first2byte || sf->encoding_name>=em_base )
	sprintf( buffer, "%-3d (0x%02x) ", fv->end_pos, fv->end_pos );
    else
	sprintf( buffer, "%-5d (0x%04x) ", fv->end_pos, fv->end_pos );
    sc = sf->chars[fv->end_pos];
    if ( sc==NULL )
	sc = SCBuildDummy(&dummy,sf,fv->end_pos);
    if ( sc->unicodeenc!=-1 )
	sprintf( buffer+strlen(buffer), "U+%04X", sc->unicodeenc );
    else
	sprintf( buffer+strlen(buffer), "U+????" );
    sprintf( buffer+strlen(buffer), "  %.*s", sizeof(buffer)-strlen(buffer)-1,
	    sc->name );

    strcat(buffer,"  ");
    uc_strcpy(ubuffer,buffer);
    if ( sc->unicodeenc!=-1 && sc->unicodeenc<0x110000 && _UnicodeNameAnnot!=NULL &&
	    _UnicodeNameAnnot[sc->unicodeenc>>16][(sc->unicodeenc>>8)&0xff][sc->unicodeenc&0xff].name!=NULL ) {
	uc_strncat(ubuffer, _UnicodeNameAnnot[sc->unicodeenc>>16][(sc->unicodeenc>>8)&0xff][sc->unicodeenc&0xff].name, 80);
    } else if ( sc->unicodeenc>=0xAC00 && sc->unicodeenc<=0xD7A3 ) {
	sprintf( buffer, "Hangul Syllable %s%s%s",
		chosung[(sc->unicodeenc-0xAC00)/(21*28)],
		jungsung[(sc->unicodeenc-0xAC00)/28%21],
		jongsung[(sc->unicodeenc-0xAC00)%28] );
	uc_strncat(ubuffer,buffer,80);
    } else if ( sc->unicodeenc!=-1 ) {
	uc_strncat(ubuffer, UnicodeRange(sc->unicodeenc),80);
    }

    GDrawDrawText(pixmap,10,fv->mbh+11,ubuffer,-1,NULL,0xff0000);
    GDrawPopClip(pixmap,&old);
}

static void FVShowInfo(FontView *fv) {
    GRect r;

    if ( fv->v==NULL )			/* Can happen in scripts */
return;

    r.x = 0; r.width = fv->width; r.y = fv->mbh; r.height = fv->infoh;
    GDrawRequestExpose(fv->gw,&r,false);
}

static void FVChar(FontView *fv,GEvent *event) {
    int i,pos, cnt;

#if MyMemory
    if ( event->u.chr.keysym == GK_F2 ) {
	fprintf( stderr, "Malloc debug on\n" );
	__malloc_debug(5);
    } else if ( event->u.chr.keysym == GK_F3 ) {
	fprintf( stderr, "Malloc debug off\n" );
	__malloc_debug(0);
    }
#endif

    if ( event->u.chr.keysym=='s' &&
	    (event->u.chr.state&ksm_control) &&
	    (event->u.chr.state&ksm_meta) )
	MenuSaveAll(NULL,NULL,NULL);
    else if ( event->u.chr.keysym=='q' &&
	    (event->u.chr.state&ksm_control) &&
	    (event->u.chr.state&ksm_meta) )
	MenuExit(NULL,NULL,NULL);
    else if ( event->u.chr.keysym=='I' &&
	    (event->u.chr.state&ksm_shift) &&
	    (event->u.chr.state&ksm_meta) )
	FVMenuCharInfo(fv->gw,NULL,NULL);
    else if (( event->u.chr.keysym=='M' ||event->u.chr.keysym=='m' ) &&
	    (event->u.chr.state&ksm_control) ) {
	if ( (event->u.chr.state&ksm_meta) && (event->u.chr.state&ksm_shift))
	    FVSimplify(fv,1);
	else if ( (event->u.chr.state&ksm_shift))
	    FVSimplify(fv,0);
    } else if ( (event->u.chr.keysym=='[' || event->u.chr.keysym==']') &&
	    (event->u.chr.state&ksm_control) ) {
	/* some people have remapped keyboards so that shift is needed to get [] */
	int pos = FVAnyCharSelected(fv);
	if ( pos>=0 ) {
	    if ( event->u.chr.keysym=='[' )
		--pos;
	    else
		++pos;
	    if ( pos<0 ) pos = fv->sf->charcnt-1;
	    else if ( pos>= fv->sf->charcnt ) pos = 0;
	    if ( pos>=0 && pos<fv->sf->charcnt )
		FVChangeChar(fv,pos);
	}
    } else if ( isdigit(event->u.chr.keysym) && (event->u.chr.state&ksm_control) &&
	    (event->u.chr.state&ksm_meta) ) {
	/* The Script menu isn't always up to date, so we might get one of */
	/*  the shortcuts here */
	int index = event->u.chr.keysym-'1';
	if ( index<0 ) index = 9;
	if ( script_filenames[index]!=NULL )
	    ExecuteScriptFile(fv,script_filenames[index]);
    } else if ( event->u.chr.keysym == GK_Left ||
	    event->u.chr.keysym == GK_Tab ||
	    event->u.chr.keysym == GK_BackTab ||
	    event->u.chr.keysym == GK_Up ||
	    event->u.chr.keysym == GK_Right ||
	    event->u.chr.keysym == GK_Down ||
	    event->u.chr.keysym == GK_KP_Left ||
	    event->u.chr.keysym == GK_KP_Up ||
	    event->u.chr.keysym == GK_KP_Right ||
	    event->u.chr.keysym == GK_KP_Down ||
	    event->u.chr.keysym == GK_Home ||
	    event->u.chr.keysym == GK_KP_Home ||
	    event->u.chr.keysym == GK_End ||
	    event->u.chr.keysym == GK_KP_End ||
	    event->u.chr.keysym == GK_Page_Up ||
	    event->u.chr.keysym == GK_KP_Page_Up ||
	    event->u.chr.keysym == GK_Prior ||
	    event->u.chr.keysym == GK_Page_Down ||
	    event->u.chr.keysym == GK_KP_Page_Down ||
	    event->u.chr.keysym == GK_Next ) {
	int end_pos = fv->end_pos;
	/* We move the currently selected char. If there is none, then pick */
	/*  something on the screen */
	if ( end_pos==-1 )
	    end_pos = (fv->rowoff+fv->rowcnt/2)*fv->colcnt;
	switch ( event->u.chr.keysym ) {
	  case GK_Tab:
	    pos = end_pos;
	    do {
		if ( event->u.chr.state&ksm_shift )
		    --pos;
		else
		    ++pos;
		if ( pos>=fv->sf->charcnt ) pos = 0;
		else if ( pos<0 ) pos = fv->sf->charcnt-1;
	    } while ( pos!=end_pos && !SCWorthOutputting(fv->sf->chars[pos]));
	    if ( pos==end_pos ) ++pos;
	    if ( pos>=fv->sf->charcnt ) pos = 0;
	  break;
#if GK_Tab!=GK_BackTab
	  case GK_BackTab:
	    pos = end_pos;
	    do {
		--pos;
		if ( pos<0 ) pos = fv->sf->charcnt-1;
	    } while ( pos!=end_pos && !SCWorthOutputting(fv->sf->chars[pos]));
	    if ( pos==end_pos ) --pos;
	    if ( pos<0 ) {
		if ( SCWorthOutputting(fv->sf->chars[fv->sf->charcnt-1]))
		    pos = fv->sf->charcnt-1;
		else
		    pos = 0;
	    }
	  break;
#endif
	  case GK_Left: case GK_KP_Left:
	    pos = end_pos-1;
	  break;
	  case GK_Right: case GK_KP_Right:
	    pos = end_pos+1;
	  break;
	  case GK_Up: case GK_KP_Up:
	    pos = end_pos-fv->colcnt;
	  break;
	  case GK_Down: case GK_KP_Down:
	    pos = end_pos+fv->colcnt;
	  break;
	  case GK_End: case GK_KP_End:
	    pos = fv->sf->charcnt;
	  break;
	  case GK_Home: case GK_KP_Home:
	    pos = 0;
	    if ( fv->sf->top_enc!=-1 && fv->sf->top_enc<fv->sf->charcnt )
		pos = fv->sf->top_enc;
	    else {
		pos = SFFindChar(fv->sf,'A',NULL);
		if ( pos==-1 ) pos = 0;
	    }
	  break;
	  case GK_Page_Up: case GK_KP_Page_Up:
#if GK_Prior!=GK_Page_Up
	  case GK_Prior:
#endif
	    pos = (fv->rowoff-fv->rowcnt+1)*fv->colcnt;
	  break;
	  case GK_Page_Down: case GK_KP_Page_Down:
#if GK_Next!=GK_Page_Down
	  case GK_Next:
#endif
	    pos = (fv->rowoff+fv->rowcnt+1)*fv->colcnt;
	  break;
	}
	if ( pos<0 ) pos = 0;
	if ( pos>=fv->sf->charcnt ) pos = fv->sf->charcnt-1;
	if ( event->u.chr.state&ksm_shift && event->u.chr.keysym!=GK_Tab && event->u.chr.keysym!=GK_BackTab ) {
	    FVReselect(fv,pos);
	} else {
	    FVDeselectAll(fv);
	    fv->selected[pos] = true;
	    FVToggleCharSelected(fv,pos);
	    fv->pressed_pos = pos;
	}
	fv->end_pos = pos;
	FVShowInfo(fv);
	FVScrollToChar(fv,pos);
    } else if ( event->u.chr.keysym == GK_Help ) {
	MenuHelp(NULL,NULL,NULL);	/* Menu does F1 */
    } else if ( event->u.chr.keysym == GK_Escape ) {
	FVDeselectAll(fv);
    } else if ( event->u.chr.chars[0]=='\r' || event->u.chr.chars[0]=='\n' ) {
	for ( i=cnt=0; i<fv->sf->charcnt && cnt<10; ++i ) if ( fv->selected[i] ) {
	    SplineChar *sc = SFMakeChar(fv->sf,i);
	    if ( fv->show==fv->filled ) {
		CharViewCreate(sc,fv);
	    } else {
		BDFFont *bdf = fv->show;
		if ( bdf->chars[i]==NULL )
		    bdf->chars[i] = SplineCharRasterize(sc,bdf->pixelsize);
		BitmapViewCreate(bdf->chars[i],bdf,fv);
	    }
	    ++cnt;
	}
    } else if ( event->u.chr.chars[0]<=' ' || event->u.chr.chars[1]!='\0' ) {
	/* Do Nothing */;
    } else {
	if ( fv->sf->encoding_name==em_unicode || fv->sf->encoding_name==em_unicode4 ||
		fv->sf->encoding_name==em_iso8859_1 ) {
	    if ( event->u.chr.chars[0]<fv->sf->charcnt ) {
		i = event->u.chr.chars[0];
		SFMakeChar(fv->sf,i);
	    } else
		i = -1;
	} else {
	    for ( i=fv->sf->charcnt-1; i>=0; --i ) if ( fv->sf->chars[i]!=NULL ) {
		if ( fv->sf->chars[i]->unicodeenc==event->u.chr.chars[0] )
	    break;
	    }
	}
	FVChangeChar(fv,i);
    }
}

static void uc_annot_strncat(unichar_t *to, const char *from, int len) {
    register unichar_t ch;

    to += u_strlen(to);
    while ( (ch = *(unsigned char *) from++) != '\0' && --len>=0 ) {
	if ( from[-2]=='\t' ) {
	    if ( ch=='*' ) ch = 0x2022;
	    else if ( ch=='x' ) ch = 0x2192;
	    else if ( ch==':' ) ch = 0x224d;
	    else if ( ch=='#' ) ch = 0x2245;
	} else if ( ch=='\t' ) {
	    *(to++) = ' ';
	    ch = ' ';
	}
	*(to++) = ch;
    }
    *to = 0;
}

void SCPreparePopup(GWindow gw,SplineChar *sc) {
    static unichar_t space[810];
    char cspace[162];
    int upos=-1;
    int enc = sc->parent->encoding_name;
    int done = false;
    int localenc = sc->enc;
    struct remap *map = sc->parent->remap;

    if ( map!=NULL ) {
	while ( map->infont!=-1 ) {
	    if ( localenc>=map->infont && localenc<=map->infont+(map->lastenc-map->firstenc) ) {
		localenc += map->firstenc-map->infont;
	break;
	    }
	    ++map;
	}
    }

    if ( sc->unicodeenc!=-1 )
	upos = sc->unicodeenc;
#if HANYANG
    else if ( sc->compositionunit ) {
	if ( sc->jamo<19 )
	    upos = 0x1100+sc->jamo;
	else if ( sc->jamo<19+21 )
	    upos = 0x1161 + sc->jamo-19;
	else		/* Leave a hole for the blank char */
	    upos = 0x11a8 + sc->jamo-(19+21+1);
    }
#endif
    else if (( sc->enc<32 || (sc->enc>=127 && sc->enc<160) ) &&
	    (enc = sc->parent->encoding_name)!=em_none &&
	    (enc<=em_zapfding || (enc>=em_big5 && enc<=em_unicode)))
	upos = sc->enc;
    else {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( cspace, "%u 0x%x U+???? \"%.25s\" ", localenc, localenc, sc->name==NULL?"":sc->name );
#else
	snprintf( cspace, sizeof(cspace), "%u 0x%x U+???? \"%.25s\" ", localenc, localenc, sc->name==NULL?"":sc->name );
#endif
	uc_strcpy(space,cspace);
	done = true;
    }
    if ( done )
	/* Do Nothing */;
    else if ( upos<0x110000 && _UnicodeNameAnnot!=NULL &&
	    _UnicodeNameAnnot[upos>>16][(upos>>8)&0xff][upos&0xff].name!=NULL ) {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( cspace, "%u 0x%x U+%04x \"%.25s\" %.100s", localenc, localenc, upos, sc->name==NULL?"":sc->name,
		_UnicodeNameAnnot[upos>>16][(upos>>8)&0xff][upos&0xff].name);
#else
	snprintf( cspace, sizeof(cspace), "%u 0x%x U+%04x \"%.25s\" %.100s", localenc, localenc, upos, sc->name==NULL?"":sc->name,
		_UnicodeNameAnnot[upos>>16][(upos>>8)&0xff][upos&0xff].name);
#endif
	uc_strcpy(space,cspace);
    } else if ( upos>=0xAC00 && upos<=0xD7A3 ) {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( cspace, "%u 0x%x U+%04x \"%.25s\" Hangul Syllable %s%s%s",
		localenc, localenc, upos, sc->name==NULL?"":sc->name,
		chosung[(upos-0xAC00)/(21*28)],
		jungsung[(upos-0xAC00)/28%21],
		jongsung[(upos-0xAC00)%28] );
#else
	snprintf( cspace, sizeof(cspace), "%u 0x%x U+%04x \"%.25s\" Hangul Syllable %s%s%s",
		localenc, localenc, upos, sc->name==NULL?"":sc->name,
		chosung[(upos-0xAC00)/(21*28)],
		jungsung[(upos-0xAC00)/28%21],
		jongsung[(upos-0xAC00)%28] );
#endif
	uc_strcpy(space,cspace);
    } else {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( cspace, "%u 0x%x U+%04x \"%.25s\" %.50s", localenc, localenc, upos, sc->name==NULL?"":sc->name,
	    	UnicodeRange(upos));
#else
	snprintf( cspace, sizeof(cspace), "%u 0x%x U+%04x \"%.25s\" %.50s", localenc, localenc, upos, sc->name==NULL?"":sc->name,
	    	UnicodeRange(upos));
#endif
	uc_strcpy(space,cspace);
    }
    if ( upos>=0 && upos<0x110000 && _UnicodeNameAnnot!=NULL &&
	    _UnicodeNameAnnot[upos>>16][(upos>>8)&0xff][upos&0xff].annot!=NULL ) {
	int left = sizeof(space)/sizeof(space[0]) - u_strlen(space)-1;
	if ( left>4 ) {
	    uc_strcat(space,"\n");
	    uc_annot_strncat(space,_UnicodeNameAnnot[upos>>16][(upos>>8)&0xff][upos&0xff].annot,left-2);
	}
    }
    if ( sc->comment!=NULL ) {
	int left = sizeof(space)/sizeof(space[0]) - u_strlen(space)-1;
	if ( left>4 ) {
	    uc_strcat(space,"\n\n");
	    u_strncat(space,sc->comment,left-2);
	}
    }
    GGadgetPreparePopup(gw,space);
}

static void noop(void *_fv) {
}

static void *ddgencharlist(void *_fv,int *len) {
    int i,j,cnt;
    FontView *fv = (FontView *) _fv;
    SplineFont *sf = fv->sf;
    char *data;

    for ( i=cnt=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && fv->selected[i])
	cnt += strlen(sf->chars[i]->name)+1;
    data = galloc(cnt+1); data[0] = '\0';
    for ( cnt=0, j=1 ; j<=fv->sel_index; ++j ) {
	for ( i=0; i<sf->charcnt; ++i )
	    if ( sf->chars[i]!=NULL && fv->selected[i]==j ) {
		strcpy(data+cnt,sf->chars[i]->name);
		cnt += strlen(sf->chars[i]->name);
		strcpy(data+cnt++," ");
	    }
    }
    if ( cnt>0 )
	data[--cnt] = '\0';
    *len = cnt;
return( data );
}

static void FVMouse(FontView *fv,GEvent *event) {
    int pos = (event->u.mouse.y/fv->cbh + fv->rowoff)*fv->colcnt + event->u.mouse.x/fv->cbw;
    SplineChar *sc, dummy;
    int dopopup = true;

    GGadgetEndPopup();
    if ( event->type==et_mousedown )
	CVPaletteDeactivate();
    if ( pos<0 ) {
	pos = 0;
	dopopup = false;
    } else if ( pos>=fv->sf->charcnt ) {
	pos = fv->sf->charcnt-1;
	dopopup = false;
    }

    sc = fv->sf->chars[pos];
    if ( sc==NULL )
	sc = SCBuildDummy(&dummy,fv->sf,pos);
    if ( event->type == et_mouseup && event->u.mouse.clicks==2 ) {
	if ( sc==&dummy )	/* I got a crash here once. Stack was corrupted, no idea how */
	    sc = SFMakeChar(fv->sf,pos);
	if ( fv->show==fv->filled ) {
	    CharViewCreate(sc,fv);
	} else {
	    BDFFont *bdf = fv->show;
	    if ( bdf->chars[pos]==NULL )
		bdf->chars[pos] = SplineCharRasterize(sc,bdf->pixelsize);
	    BitmapViewCreate(bdf->chars[pos],bdf,fv);
	}
    } else if ( event->type == et_mousemove ) {
	if ( dopopup )
	    SCPreparePopup(fv->v,sc);
    }
    if ( event->type == et_mousedown ) {
	if ( fv->drag_and_drop ) {
	    GDrawSetCursor(fv->v,ct_mypointer);
	    fv->any_dd_events_sent = fv->drag_and_drop = false;
	}
	if ( !(event->u.mouse.state&ksm_shift) && event->u.mouse.clicks<=1 ) {
	    if ( !fv->selected[pos] )
		FVDeselectAll(fv);
	    else if ( event->u.mouse.button!=3 ) {
		fv->drag_and_drop = fv->has_dd_no_cursor = true;
		fv->any_dd_events_sent = false;
		GDrawSetCursor(fv->v,ct_prohibition);
		GDrawGrabSelection(fv->v,sn_drag_and_drop);
		GDrawAddSelectionType(fv->v,sn_drag_and_drop,"STRING",fv,0,sizeof(char),
			ddgencharlist,noop);
	    }
	}
	fv->pressed_pos = fv->end_pos = pos;
	FVShowInfo(fv);
	if ( !fv->drag_and_drop ) {
	    if ( !(event->u.mouse.state&ksm_shift))
		fv->sel_index = 1;
	    else if ( fv->sel_index<255 )
		++fv->sel_index;
	    if ( fv->pressed!=NULL )
		GDrawCancelTimer(fv->pressed);
	    else if ( event->u.mouse.state&ksm_shift ) {
		fv->selected[pos] = fv->selected[pos] ? 0 : fv->sel_index;
		FVToggleCharSelected(fv,pos);
	    } else if ( !fv->selected[pos] ) {
		fv->selected[pos] = fv->sel_index;
		FVToggleCharSelected(fv,pos);
	    }
	    if ( event->u.mouse.button==3 )
		GMenuCreatePopupMenu(fv->v,event, fvpopupmenu);
	    else
		fv->pressed = GDrawRequestTimer(fv->v,200,100,NULL);
	}
    } else if ( fv->drag_and_drop ) {
	if ( event->u.mouse.x>=0 && event->u.mouse.y>=-fv->mbh-fv->infoh-4 &&
		event->u.mouse.x<=fv->width+20 && event->u.mouse.y<fv->height ) {
	    if ( !fv->has_dd_no_cursor ) {
		fv->has_dd_no_cursor = true;
		GDrawSetCursor(fv->v,ct_prohibition);
	    }
	} else {
	    if ( fv->has_dd_no_cursor ) {
		fv->has_dd_no_cursor = false;
		GDrawSetCursor(fv->v,ct_ddcursor);
	    }
	    GDrawPostDragEvent(fv->v,event,event->type==et_mouseup?et_drop:et_drag);
	    fv->any_dd_events_sent = true;
	}
	if ( event->type==et_mouseup ) {
	    fv->drag_and_drop = fv->has_dd_no_cursor = false;
	    GDrawSetCursor(fv->v,ct_mypointer);
	    if ( !fv->any_dd_events_sent )
		FVDeselectAll(fv);
	    fv->any_dd_events_sent = false;
	}
    } else if ( fv->pressed!=NULL ) {
	int showit = pos!=fv->end_pos;
	FVReselect(fv,pos);
	if ( showit )
	    FVShowInfo(fv);
	if ( event->type==et_mouseup ) {
	    GDrawCancelTimer(fv->pressed);
	    fv->pressed = NULL;
	}
    }
    if ( event->type==et_mouseup && dopopup )
	SCPreparePopup(fv->v,sc);
    if ( event->type==et_mouseup )
	SVAttachFV(fv,2);
}

static void FVResize(FontView *fv,GEvent *event) {
    GRect pos,screensize;
    int topchar;

    if ( fv->colcnt!=0 )
	topchar = fv->rowoff*fv->colcnt;
    else if ( fv->sf->encoding_name>=em_jis208 && fv->sf->encoding_name<=em_gb2312 )
	topchar = 1;
    else if ( fv->sf->top_enc!=-1 && fv->sf->top_enc<fv->sf->charcnt )
	topchar = fv->sf->top_enc;
    else {
	/* Position on 'A' if it exists */
	topchar = SFFindChar(fv->sf,'A',NULL);
	if ( topchar==-1 ) topchar = 0;
    }
    if ( (event->u.resize.size.width-
		GDrawPointsToPixels(fv->gw,_GScrollBar_Width)-1)%fv->cbw!=0 ||
	    (event->u.resize.size.height-fv->mbh-fv->infoh-1)%fv->cbh!=0 ) {
	int cc = (event->u.resize.size.width+fv->cbw/2-
		GDrawPointsToPixels(fv->gw,_GScrollBar_Width)-1)/fv->cbw;
	int rc = (event->u.resize.size.height-fv->mbh-fv->infoh-1)/fv->cbh;
	if ( cc<=0 ) cc = 1;
	if ( rc<=0 ) rc = 1;
	GDrawGetSize(GDrawGetRoot(NULL),&screensize);
	if ( cc*fv->cbw+GDrawPointsToPixels(fv->gw,_GScrollBar_Width)+10>screensize.width )
	    --cc;
	if ( rc*fv->cbh+fv->mbh+fv->infoh+20>screensize.height )
	    --rc;
	GDrawResize(fv->gw,
		cc*fv->cbw+1+GDrawPointsToPixels(fv->gw,_GScrollBar_Width),
		rc*fv->cbh+1+fv->mbh+fv->infoh);
	/* somehow KDE loses this event of mine so to get even the vague effect */
	/*  we can't just return */
/*return;*/
    }

    pos.width = GDrawPointsToPixels(fv->gw,_GScrollBar_Width);
    pos.height = event->u.resize.size.height-fv->mbh-fv->infoh;
    pos.x = event->u.resize.size.width-pos.width; pos.y = fv->mbh+fv->infoh;
    GGadgetResize(fv->vsb,pos.width,pos.height);
    GGadgetMove(fv->vsb,pos.x,pos.y);
    pos.width = pos.x; pos.x = 0;
    GDrawResize(fv->v,pos.width,pos.height);

    fv->width = pos.width; fv->height = pos.height;
    fv->colcnt = (fv->width-1)/fv->cbw;
    if ( fv->colcnt<1 ) fv->colcnt = 1;
    fv->rowcnt = (fv->height-1)/fv->cbh;
    if ( fv->rowcnt<1 ) fv->rowcnt = 1;
    fv->rowltot = (fv->sf->charcnt+fv->colcnt-1)/fv->colcnt;

    GScrollBarSetBounds(fv->vsb,0,fv->rowltot,fv->rowcnt);
    fv->rowoff = topchar/fv->colcnt;
    if ( fv->rowoff>=fv->rowltot-fv->rowcnt )
        fv->rowoff = fv->rowltot-fv->rowcnt-1;
    if ( fv->rowoff<0 ) fv->rowoff =0;
    GScrollBarSetPos(fv->vsb,fv->rowoff);
    GDrawRequestExpose(fv->gw,NULL,true);
    GDrawRequestExpose(fv->v,NULL,true);
}

static void FVTimer(FontView *fv,GEvent *event) {

    if ( event->u.timer.timer==fv->pressed ) {
	GEvent e;
	GDrawGetPointerPosition(fv->v,&e);
	if ( e.u.mouse.y<0 || e.u.mouse.y >= fv->height ) {
	    real dy = 0;
	    if ( e.u.mouse.y<0 )
		dy = -1;
	    else if ( e.u.mouse.y>=fv->height )
		dy = 1;
	    if ( fv->rowoff+dy<0 )
		dy = 0;
	    else if ( fv->rowoff+dy+fv->rowcnt > fv->rowltot )
		dy = 0;
	    fv->rowoff += dy;
	    if ( dy!=0 ) {
		GScrollBarSetPos(fv->vsb,fv->rowoff);
		GDrawScroll(fv->v,NULL,0,dy*fv->cbh);
	    }
	}
    } else if ( event->u.timer.timer==fv->resize ) {
	/* It's a delayed resize event (for kde which sends continuous resizes) */
	fv->resize = NULL;
	FVResize(fv,(GEvent *) (event->u.timer.userdata));
    } else if ( event->u.timer.userdata!=NULL ) {
	/* It's a delayed function call */
	void (*func)(FontView *) = (void (*)(FontView *)) (event->u.timer.userdata);
	func(fv);
    }
}

void FVDelay(FontView *fv,void (*func)(FontView *)) {
    GDrawRequestTimer(fv->v,100,0,(void *) func);
}

static void FVScroll(FontView *fv,struct sbevent *sb) {
    int newpos = fv->rowoff;

    switch( sb->type ) {
      case et_sb_top:
        newpos = 0;
      break;
      case et_sb_uppage:
        newpos -= fv->rowcnt;
      break;
      case et_sb_up:
        --newpos;
      break;
      case et_sb_down:
        ++newpos;
      break;
      case et_sb_downpage:
        newpos += fv->rowcnt;
      break;
      case et_sb_bottom:
        newpos = fv->rowltot-fv->rowcnt;
      break;
      case et_sb_thumb:
      case et_sb_thumbrelease:
        newpos = sb->pos;
      break;
    }
    if ( newpos>fv->rowltot-fv->rowcnt )
        newpos = fv->rowltot-fv->rowcnt;
    if ( newpos<0 ) newpos =0;
    if ( newpos!=fv->rowoff ) {
	int diff = newpos-fv->rowoff;
	fv->rowoff = newpos;
	GScrollBarSetPos(fv->vsb,fv->rowoff);
	GDrawScroll(fv->v,NULL,0,diff*fv->cbh);
    }
}

static int v_e_h(GWindow gw, GEvent *event) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    if (( event->type==et_mouseup || event->type==et_mousedown ) &&
	    (event->u.mouse.button==4 || event->u.mouse.button==5) ) {
return( GGadgetDispatchEvent(fv->vsb,event));
    }

    switch ( event->type ) {
      case et_expose:
	GDrawSetLineWidth(gw,0);
	FVExpose(fv,gw,event);
      break;
      case et_char:
	FVChar(fv,event);
      break;
      case et_mousemove: case et_mousedown: case et_mouseup:
	if ( event->type==et_mousedown )
	    GDrawSetGIC(gw,fv->gic,0,20);
	FVMouse(fv,event);
      break;
      case et_timer:
	FVTimer(fv,event);
      break;
      case et_focus:
	if ( event->u.focus.gained_focus ) {
	    GDrawSetGIC(gw,fv->gic,0,20);
#if 0
	    CVPaletteDeactivate();
#endif
	}
      break;
    }
return( true );
}

void FontViewReformatOne(FontView *fv) {
    BDFFont *new;
    FontView *fvs;

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;

    GDrawSetCursor(fv->v,ct_watch);
    fv->rowltot = (fv->sf->charcnt+fv->colcnt-1)/fv->colcnt;
    GScrollBarSetBounds(fv->vsb,0,fv->rowltot,fv->rowcnt);
    if ( fv->rowoff>fv->rowltot-fv->rowcnt ) {
        fv->rowoff = fv->rowltot-fv->rowcnt;
	if ( fv->rowoff<0 ) fv->rowoff =0;
	GScrollBarSetPos(fv->vsb,fv->rowoff);
    }
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	if ( fvs!=fv && fvs->sf==fv->sf )
    break;
    if ( fvs!=NULL )
	new = fvs->filled;
    else
	new = SplineFontPieceMeal(fv->sf,fv->filled->pixelsize,
		(fv->antialias?pf_antialias:0)|(fv->bbsized?pf_bbsized:0),
		NULL);
    BDFFontFree(fv->filled);
    if ( fv->filled == fv->show )
	fv->show = new;
    fv->filled = new;
    GDrawRequestExpose(fv->v,NULL,false);
    GDrawSetCursor(fv->v,ct_pointer);
}

void FontViewReformatAll(SplineFont *sf) {
    BDFFont *new, *old;
    FontView *fvs, *fv;

    if ( sf->fv->v==NULL || sf->fv->colcnt==0 )			/* Can happen in scripts */
return;

    for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	fvs->touched = false;
    while ( 1 ) {
	for ( fv=sf->fv; fv!=NULL && fv->touched; fv=fv->nextsame );
	if ( fv==NULL )
    break;
	old = fv->filled;
				/* In CID fonts fv->sf may not be same as sf */
	new = SplineFontPieceMeal(fv->sf,fv->filled->pixelsize,
		(fv->antialias?pf_antialias:0)|(fv->bbsized?pf_bbsized:0),
		NULL);
	for ( fvs=fv; fvs!=NULL; fvs=fvs->nextsame )
	    if ( fvs->filled == old ) {
		fvs->filled = new;
		if ( fvs->show == old )
		    fvs->show = new;
		fvs->touched = true;
	    }
	BDFFontFree(old);
    }
    for ( fv=sf->fv; fv!=NULL; fv=fv->nextsame ) {
	GDrawSetCursor(fv->v,ct_watch);
	fv->rowltot = (fv->sf->charcnt+fv->colcnt-1)/fv->colcnt;
	GScrollBarSetBounds(fv->vsb,0,fv->rowltot,fv->rowcnt);
	if ( fv->rowoff>fv->rowltot-fv->rowcnt ) {
	    fv->rowoff = fv->rowltot-fv->rowcnt;
	    if ( fv->rowoff<0 ) fv->rowoff =0;
	    GScrollBarSetPos(fv->vsb,fv->rowoff);
	}
	GDrawRequestExpose(fv->v,NULL,false);
	GDrawSetCursor(fv->v,ct_pointer);
    }
}

static int fv_e_h(GWindow gw, GEvent *event) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    if (( event->type==et_mouseup || event->type==et_mousedown ) &&
	    (event->u.mouse.button==4 || event->u.mouse.button==5) ) {
return( GGadgetDispatchEvent(fv->vsb,event));
    }

    switch ( event->type ) {
      case et_selclear:
	ClipboardClear();
      break;
      case et_expose:
	GDrawSetLineWidth(gw,0);
	FVDrawInfo(fv,gw,event);
      break;
      case et_resize:
	/* KDE sends a continuous stream of resize events, and gets very */
	/*  confused if I start resizing the window myself, try to wait for */
	/*  the user to finish before responding to resizes */
	if ( event->u.resize.sized ) {
	    static GEvent temp;
	    if ( fv->resize )
		GDrawCancelTimer(fv->resize);
	    temp = *event;
	    fv->resize = GDrawRequestTimer(fv->v,300,0,(void *) &temp);
	}
      break;
      case et_char:
	FVChar(fv,event);
      break;
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_scrollbarchange:
	    FVScroll(fv,&event->u.control.u.sb);
	  break;
	}
      break;
      case et_close:
	FVMenuClose(gw,NULL,NULL);
      break;
      case et_create:
	fv->next = fv_list;
	fv_list = fv;
      break;
      case et_destroy:
	if ( fv_list==fv )
	    fv_list = fv->next;
	else {
	    FontView *n;
	    for ( n=fv_list; n->next!=fv; n=n->next );
	    n->next = fv->next;
	}
	if ( fv_list!=NULL )		/* Freeing a large font can take forever, and if we're just going to exit there's no real reason to do so... */
	    FontViewFree(fv);
      break;
    }
return( true );
}

static void FontViewOpenKids(FontView *fv) {
    int k, i;
    SplineFont *sf = fv->sf, *_sf;

    if ( sf->cidmaster!=NULL )
	sf = sf->cidmaster;

    k=0;
    do {
	_sf = sf->subfontcnt==0 ? sf : sf->subfonts[k];
	for ( i=0; i<_sf->charcnt; ++i )
	    if ( _sf->chars[i]!=NULL && _sf->chars[i]->wasopen ) {
		_sf->chars[i]->wasopen = false;
		CharViewCreate(_sf->chars[i],fv);
	    }
	++k;
    } while ( k<sf->subfontcnt );
}

FontView *_FontViewCreate(SplineFont *sf) {
    FontView *fv = gcalloc(1,sizeof(FontView));
    int i;
    int ps = sf->display_size<0 ? -sf->display_size :
	     sf->display_size==0 ? default_fv_font_size : sf->display_size;

    fv->nextsame = sf->fv;
    sf->fv = fv;
    if ( sf->subfontcnt==0 )
	fv->sf = sf;
    else {
	fv->cidmaster = sf;
	for ( i=0; i<sf->subfontcnt; ++i )
	    sf->subfonts[i]->fv = fv;
	fv->sf = sf = sf->subfonts[0];
    }
    fv->selected = gcalloc(sf->charcnt,sizeof(char));
    fv->magnify = (ps<=9)? 3 : (ps<20) ? 2 : 1;
    fv->cbw = (ps*fv->magnify)+1;
    fv->cbh = (ps*fv->magnify)+1+FV_LAB_HEIGHT+1;
    fv->antialias = sf->display_antialias;
    fv->bbsized = sf->display_bbsized;
return( fv );
}

FontView *FontViewCreate(SplineFont *sf) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetData gd;
    GGadget *sb;
    FontView *fv = _FontViewCreate(sf);
    GRect gsize;
    FontRequest rq;
    /* sadly, clearlyu is too big for the space I've got */
    static unichar_t monospace[] = { 'c','o','u','r','i','e','r',',','m', 'o', 'n', 'o', 's', 'p', 'a', 'c', 'e',',','c','a','s','l','o','n',',','c','l','e','a','r','l','y','u',',','u','n','i','f','o','n','t',  '\0' };
    static unichar_t *fontnames=NULL;
    static GWindow icon = NULL;
    BDFFont *bdf;
    static int nexty=0;
    GRect size;

    if ( icon==NULL )
#ifdef BIGICONS
	icon = GDrawCreateBitmap(NULL,fontview_width,fontview_height,fontview_bits);
#else
	icon = GDrawCreateBitmap(NULL,fontview2_width,fontview2_height,fontview2_bits);
#endif

    GDrawGetSize(GDrawGetRoot(NULL),&size);

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_icon;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.cursor = ct_pointer;
    wattrs.icon = icon;
    pos.width = sf->desired_col_cnt*fv->cbw+1;
    pos.height = sf->desired_row_cnt*fv->cbh+1;
    pos.x = size.width-pos.width-30; pos.y = nexty;
    nexty += 2*fv->cbh+50;
    if ( nexty+pos.height > size.height )
	nexty = 0;
    fv->gw = gw = GDrawCreateTopWindow(NULL,&pos,fv_e_h,fv,&wattrs);
    FVSetTitle(fv);

    memset(&gd,0,sizeof(gd));
    gd.flags = gg_visible | gg_enabled;
    gd.u.menu = mblist;
    fv->mb = GMenuBarCreate( gw, &gd, NULL);
    GGadgetGetSize(fv->mb,&gsize);
    fv->mbh = gsize.height;
    fv->infoh = 14/*GDrawPointsToPixels(fv->gw,14)*/;
    fv->end_pos = -1;

    gd.pos.y = fv->mbh+fv->infoh; gd.pos.height = pos.height;
    gd.pos.width = GDrawPointsToPixels(gw,_GScrollBar_Width);
    gd.pos.x = pos.width;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_sb_vert;
    fv->vsb = sb = GScrollBarCreate(gw,&gd,fv);

    wattrs.mask = wam_events|wam_cursor;
    pos.x = 0; pos.y = fv->mbh+fv->infoh;
    fv->v = GWidgetCreateSubWindow(gw,&pos,v_e_h,fv,&wattrs);
    GDrawSetVisible(fv->v,true);

    fv->gic = GDrawCreateInputContext(fv->v,gic_root|gic_orlesser);
    GDrawSetGIC(fv->v,fv->gic,0,20);

    if ( fontnames==NULL ) {
	fontnames = uc_copy(GResourceFindString("FontView.FontFamily"));
	if ( fontnames==NULL )
	    fontnames = monospace;
    }
    memset(&rq,0,sizeof(rq));
    rq.family_name = fontnames;
    rq.point_size = -13;
    rq.weight = 400;
    fv->header = GDrawInstanciateFont(GDrawGetDisplayOfWindow(gw),&rq);
    rq.style = fs_italic;
    fv->iheader = GDrawInstanciateFont(GDrawGetDisplayOfWindow(gw),&rq);
    GDrawSetFont(fv->v,fv->header);
    fv->showhmetrics = default_fv_showhmetrics;
    fv->showvmetrics = default_fv_showvmetrics && sf->hasvmetrics;
    if ( fv->nextsame!=NULL ) {
	fv->filled = fv->nextsame->filled;
	bdf = fv->nextsame->show;
    } else {
	bdf = SplineFontPieceMeal(fv->sf,sf->display_size<0?-sf->display_size:default_fv_font_size,
		(fv->antialias?pf_antialias:0)|(fv->bbsized?pf_bbsized:0),
		NULL);
	fv->filled = bdf;
	if ( sf->display_size>0 ) {
	    for ( bdf=sf->bitmaps; bdf!=NULL && bdf->pixelsize!=sf->display_size ;
		    bdf=bdf->next );
	    if ( bdf==NULL )
		bdf = fv->filled;
	}
	if ( sf->onlybitmaps && bdf==fv->filled && sf->bitmaps!=NULL )
	    bdf = sf->bitmaps;
    }
    fv->cbw = -1;
    FVChangeDisplayFont(fv,bdf);

    /*GWidgetHidePalettes();*/
    GDrawSetVisible(gw,true);
    FontViewOpenKids(fv);
return( fv );
}

static SplineFont *SFReadPostscript(char *filename) {
    FontDict *fd;
    SplineFont *sf=NULL;

    GProgressChangeStages(2);
    fd = ReadPSFont(filename);
    GProgressNextStage();
    GProgressChangeLine2R(_STR_InterpretingGlyphs);
    if ( fd!=NULL ) {
	sf = SplineFontFromPSFont(fd);
	PSFontFree(fd);
	if ( sf!=NULL )
	    CheckAfmOfPostscript(sf,filename);
    }
return( sf );
}

/* This does not check currently existing fontviews, and should only be used */
/*  by LoadSplineFont (which does) and by RevertFile (which knows what it's doing) */
SplineFont *ReadSplineFont(char *filename,enum openflags openflags) {
    SplineFont *sf;
    unichar_t ubuf[150];
    char buf[1500];
    int fromsfd = false;
    static struct { char *ext, *decomp, *recomp; } compressors[] = {
	{ "gz", "gunzip", "gzip" },
	{ "bz2", "bunzip2", "bzip2" },
	{ "Z", "gunzip", "compress" },
	NULL
    };
    int i;
    char *pt, *strippedname, *tmpfile=NULL, *paren=NULL, *fullname=filename;
    int len;

    if ( filename==NULL )
return( NULL );

    strippedname = filename;
    pt = strrchr(filename,'/');
    if ( pt==NULL ) pt = filename;
    if ( (paren=strchr(pt,'('))!=NULL && strchr(paren,')')!=NULL ) {
	strippedname = copy(filename);
	strippedname[paren-filename] = '\0';
    }

    pt = strrchr(strippedname,'.');
    i = -1;
    if ( pt!=NULL ) for ( i=0; compressors[i].ext!=NULL; ++i )
	if ( strcmp(compressors[i].ext,pt+1)==0 )
    break;
    if ( i==-1 || compressors[i].ext==NULL ) i=-1;
    else {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( buf, "%s %s", compressors[i].decomp, strippedname );
#else
	snprintf( buf, sizeof(buf), "%s %s", compressors[i].decomp, strippedname );
#endif
	if ( system(buf)==0 ) {
	    *pt='\0';
	} else {
	    /* Assume no write access to file */
	    char *dir = getenv("TMPDIR");
	    if ( dir==NULL ) dir = P_tmpdir;
	    tmpfile = galloc(strlen(dir)+strlen(GFileNameTail(strippedname))+2);
	    strcpy(tmpfile,dir);
	    strcat(tmpfile,"/");
	    strcat(tmpfile,GFileNameTail(strippedname));
	    *strrchr(tmpfile,'.') = '\0';
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	    sprintf( buf, "%s -c %s > %s", compressors[i].decomp, strippedname, tmpfile );
#else
	    snprintf( buf, sizeof(buf), "%s -c %s > %s", compressors[i].decomp, strippedname, tmpfile );
#endif
	    if ( system(buf)==0 ) {
		if ( strippedname!=filename ) free(strippedname);
		strippedname = tmpfile;
	    } else {
		GDrawError("Decompress failed" );
		free(tmpfile);
return( NULL );
	    }
	}
	if ( strippedname!=filename && paren!=NULL ) {
	    fullname = galloc(strlen(strippedname)+strlen(paren)+1);
	    strcpy(fullname,strippedname);
	    strcat(fullname,paren);
	} else
	    fullname = strippedname;
    }

    u_strcpy(ubuf,GStringGetResource(_STR_LoadingFontFrom,NULL));
    len = u_strlen(ubuf);
    uc_strncat(ubuf,GFileNameTail(fullname),100);
    ubuf[100+len] = '\0';
    /* If there are no pfaedit windows, give them something to look at */
    /*  immediately. Otherwise delay a bit */
    GProgressStartIndicator(fv_list==NULL?0:10,GStringGetResource(_STR_Loading,NULL),ubuf,GStringGetResource(_STR_ReadingGlyphs,NULL),0,1);
    GProgressEnableStop(0);
    if ( fv_list==NULL && screen_display!=NULL ) { GDrawSync(NULL); GDrawProcessPendingEvents(NULL); }

    sf = NULL;
    if ( strmatch(fullname+strlen(fullname)-4, ".sfd")==0 ||
	 strmatch(fullname+strlen(fullname)-5, ".sfd~")==0 ) {
	sf = SFDRead(fullname);
	fromsfd = true;
    } else if ( strmatch(fullname+strlen(fullname)-4, ".ttf")==0 ||
		strmatch(fullname+strlen(strippedname)-4, ".ttc")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".otf")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".otb")==0 ) {
	sf = SFReadTTF(fullname,0);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".svg")==0 ) {
	sf = SFReadSVG(fullname,0);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".bdf")==0 ) {
	sf = SFFromBDF(fullname,0,false);
    } else if ( strmatch(fullname+strlen(fullname)-2, "pk")==0 ) {
	sf = SFFromBDF(fullname,1,true);
    } else if ( strmatch(fullname+strlen(fullname)-2, "gf")==0 ) {
	sf = SFFromBDF(fullname,3,true);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".pcf")==0 ) {
	sf = SFFromBDF(fullname,2,false);
    } else if ( strmatch(fullname+strlen(strippedname)-4, ".bin")==0 ||
		strmatch(fullname+strlen(strippedname)-4, ".hqx")==0 ||
		strmatch(fullname+strlen(strippedname)-6, ".dfont")==0 ) {
	sf = SFReadMacBinary(fullname,0);
    } else if ( strmatch(fullname+strlen(strippedname)-4, ".fon")==0 ||
		strmatch(fullname+strlen(strippedname)-4, ".fnt")==0 ) {
	sf = SFReadWinFON(fullname,0);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".pfa")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".pfb")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".pf3")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".cid")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".gsf")==0 ||
		strmatch(fullname+strlen(fullname)-3, ".ps")==0 ) {
	sf = SFReadPostscript(fullname);
    } else if ( strmatch(fullname+strlen(fullname)-3, ".mf")==0 ) {
	sf = SFFromMF(fullname);
    } else if ( strmatch(fullname+strlen(fullname)-3, ".ik")==0 ) {
	sf = SFReadIkarus(fullname);
    } else {
	FILE *foo = fopen(strippedname,"r");
	if ( foo!=NULL ) {
	    /* Try to guess the file type from the first few characters... */
	    int ch1 = getc(foo);
	    int ch2 = getc(foo);
	    int ch3 = getc(foo);
	    int ch4 = getc(foo);
	    int ch5, ch6;
	    fseek(foo, 98, SEEK_SET);
	    ch5 = getc(foo);
	    ch6 = getc(foo);
	    fclose(foo);
	    if (( ch1==0 && ch2==1 && ch3==0 && ch4==0 ) ||
		    (ch1=='O' && ch2=='T' && ch3=='T' && ch4=='O') ||
		    (ch1=='t' && ch2=='r' && ch3=='u' && ch4=='e') ||
		    (ch1=='t' && ch2=='t' && ch3=='c' && ch4=='f') ) {
		sf = SFReadTTF(fullname,0);
	    } else if ( ch1=='%' && ch2=='!' ) {
		sf = SFReadPostscript(fullname);
	    } else if ( ch1=='<' && ch2=='?' && (ch3=='x'||ch3=='X') && (ch4=='m'||ch4=='M') ) {
		sf = SFReadSVG(fullname,0);
#if 0		/* I'm not sure if this is a good test for mf files... */
	    } else if ( ch1=='%' && ch2==' ' ) {
		sf = SFFromMF(fullname);
#endif
	    } else if ( ch1=='S' && ch2=='p' && ch3=='l' && ch4=='i' ) {
		sf = SFDRead(fullname);
		fromsfd = true;
	    } else if ( ch1=='S' && ch2=='T' && ch3=='A' && ch4=='R' ) {
		sf = SFFromBDF(fullname,0,false);
	    } else if ( ch1=='\1' && ch2=='f' && ch3=='c' && ch4=='p' ) {
		sf = SFFromBDF(fullname,2,false);
	    } else if ( ch5=='I' && ch6=='K' && ch3==0 && ch4==55 ) {
		/* Ikarus font type appears at word 50 (byte offset 98) */
		/* Ikarus name section length (at word 2, byte offset 2) was 55 in the 80s at URW */
		sf = SFReadIkarus(fullname);
	    } else /* Too hard to figure out a valid mark for a mac resource file */
		sf = SFReadMacBinary(fullname,0);
	}
    }
    if ( strippedname!=filename && strippedname!=tmpfile )
	free(strippedname);
    if ( fullname!=filename && fullname!=strippedname )
	free(fullname);
    GProgressEndIndicator();

    if ( sf!=NULL ) {
	if ( sf->chosenname!=NULL && strippedname==filename ) {
	    sf->origname = galloc(strlen(filename)+strlen(sf->chosenname)+8);
	    strcpy(sf->origname,filename);
	    strcat(sf->origname,"(");
	    strcat(sf->origname,sf->chosenname);
	    strcat(sf->origname,")");
	} else
	    sf->origname = copy(filename);
	free( sf->chosenname ); sf->chosenname = NULL;
    } else if ( !GFileExists(filename) )
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_NoSuchFontFile,GFileNameTail(filename));
    else if ( !GFileReadable(filename) )
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_FontFileNotReadable,GFileNameTail(filename));
    else
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_CouldntParseFont,GFileNameTail(filename));

    if ( tmpfile!=NULL ) {
	unlink(tmpfile);
	free(tmpfile);
    } else if ( i!=-1 ) {
#if defined( _NO_SNPRINTF ) || defined( __VMS )
	sprintf( buf, "%s %s", compressors[i].recomp, filename );
#else
	snprintf( buf, sizeof(buf), "%s %s", compressors[i].recomp, filename );
#endif
	system(buf);
    }
    if ( (openflags&of_fstypepermitted) && sf!=NULL && (sf->pfminfo.fstype&0xff)==0x0002 ) {
	/* Ok, they have told us from a script they have access to the font */
    } else if ( !fromsfd && sf!=NULL && (sf->pfminfo.fstype&0xff)==0x0002 ) {
	static int buts[] = { _STR_Yes, _STR_No, 0 };
	if ( GWidgetAskR(_STR_RestrictedFont,buts,1,1,_STR_RestrictedRightsFont)==1 ) {
	    SplineFontFree(sf);
return( NULL );
	}
    }
return( sf );
}

static SplineFont *AbsoluteNameCheck(char *filename) {
    char buffer[1025];
    FontView *fv;

    GFileGetAbsoluteName(filename,buffer,sizeof(buffer)); 
    for ( fv=fv_list; fv!=NULL ; fv=fv->next ) {
	if ( fv->sf->filename!=NULL && strcmp(fv->sf->filename,filename)==0 )
return( fv->sf );
	else if ( fv->sf->origname!=NULL && strcmp(fv->sf->origname,filename)==0 )
return( fv->sf );
    }
return( NULL );
}

static char *ToAbsolute(char *filename) {
    char buffer[1025];

    GFileGetAbsoluteName(filename,buffer,sizeof(buffer));
return( copy(buffer));
}

SplineFont *LoadSplineFont(char *filename,enum openflags openflags) {
    FontView *fv;
    SplineFont *sf;
    char *pt, *ept, *tobefreed1=NULL, *tobefreed2=NULL;
    static char *extens[] = { ".sfd", ".pfa", ".pfb", ".ttf", ".otf", ".ps", ".cid", ".bin", ".dfont", ".PFA", ".PFB", ".TTF", ".OTF", ".PS", ".CID", ".BIN", ".DFONT", NULL };
    int i;

    if ( filename==NULL )
return( NULL );

    if (( pt = strrchr(filename,'/'))==NULL ) pt = filename;
    if ( strchr(pt,'.')==NULL ) {
	/* They didn't give an extension. If there's a file with no extension */
	/*  see if it's a valid font file (and if so use the extensionless */
	/*  filename), otherwise guess at an extension */
	/* For some reason Adobe distributes CID keyed fonts (both OTF and */
	/*  postscript) as extensionless files */
	int ok = false;
	FILE *test = fopen(filename,"r");
	if ( test!=NULL ) {
#if 0
	    int ch1 = getc(test);
	    int ch2 = getc(test);
	    int ch3 = getc(test);
	    int ch4 = getc(test);
	    if ( ch1=='%' ) ok = true;
	    else if (( ch1==0 && ch2==1 && ch3==0 && ch4==0 ) ||
		    (ch1=='O' && ch2=='T' && ch3=='T' && ch4=='O') ||
		    (ch1=='t' && ch2=='r' && ch3=='u' && ch4=='e') ||
		    (ch1=='t' && ch2=='t' && ch3=='c' && ch4=='f') ) ok = true;
	    else if ( ch1=='S' && ch2=='p' && ch3=='l' && ch4=='i' ) ok = true;
#endif
	    ok = true;		/* Mac resource files are too hard to check for */
		    /* If file exists, assume good */
	    fclose(test);
	}
	if ( !ok ) {
	    tobefreed1 = galloc(strlen(filename)+8);
	    strcpy(tobefreed1,filename);
	    ept = tobefreed1+strlen(tobefreed1);
	    for ( i=0; extens[i]!=NULL; ++i ) {
		strcpy(ept,extens[i]);
		if ( GFileExists(tobefreed1))
	    break;
	    }
	    if ( extens[i]!=NULL )
		filename = tobefreed1;
	    else {
		free(tobefreed1);
		tobefreed1 = NULL;
	    }
	}
    } else
	tobefreed1 = NULL;

    sf = NULL;
    /* Only one view per font */
    for ( fv=fv_list; fv!=NULL && sf==NULL; fv=fv->next ) {
	if ( fv->sf->filename!=NULL && strcmp(fv->sf->filename,filename)==0 )
	    sf = fv->sf;
	else if ( fv->sf->origname!=NULL && strcmp(fv->sf->origname,filename)==0 )
	    sf = fv->sf;
    }
    if ( sf==NULL && *filename!='/' )
	sf = AbsoluteNameCheck(filename);
    if ( sf==NULL && *filename!='/' )
	filename = tobefreed2 = ToAbsolute(filename);

    if ( sf==NULL )
	sf = ReadSplineFont(filename,openflags);

    free(tobefreed1);
    free(tobefreed2);
return( sf );
}

FontView *ViewPostscriptFont(char *filename) {
    SplineFont *sf = LoadSplineFont(filename,0);
    if ( sf==NULL )
return( NULL );
#if 0
    if ( sf->fv!=NULL ) {
	GDrawSetVisible(sf->fv->gw,true);
	GDrawRaise(sf->fv->gw);
return( sf->fv );
    }
#endif
return( FontViewCreate(sf));	/* Always make a new view now */
}

FontView *FontNew(void) {
return( FontViewCreate(SplineFontNew()));
}

void FontViewFree(FontView *fv) {
    int i;
    FontView *prev;
    FontView *fvs;

    if ( fv->nextsame==NULL && fv->sf->fv==fv ) {
	SplineFontFree(fv->cidmaster?fv->cidmaster:fv->sf);
	BDFFontFree(fv->filled);
    } else {
	for ( fvs=fv->sf->fv, i=0 ; fvs!=NULL; fvs = fvs->nextsame )
	    if ( fvs->filled==fv->filled ) ++i;
	if ( i==1 )
	    BDFFontFree(fv->filled);
	if ( fv->sf->fv==fv ) {
	    if ( fv->cidmaster==NULL )
		fv->sf->fv = fv->nextsame;
	    else {
		fv->cidmaster->fv = fv->nextsame;
		for ( i=0; i<fv->cidmaster->subfontcnt; ++i )
		    fv->cidmaster->subfonts[i]->fv = fv->nextsame;
	    }
	} else {
	    for ( prev = fv->sf->fv; prev->nextsame!=fv; prev=prev->nextsame );
	    prev->nextsame = fv->nextsame;
	}
    }
    DictionaryFree(fv->fontvars);
    free(fv->fontvars);
    free(fv->selected);
    free(fv);
}

void FVFakeMenus(FontView *fv,int cmd) {
    switch ( cmd ) {
      case 0:	/* Cut */
	FVCopy(fv,true);
	FVClear(fv);
      break;
      case 1:
	FVCopy(fv,true);
      break;
      case 2:	/* Copy reference */
	FVCopy(fv,false);
      break;
      case 3:
	FVCopyWidth(fv,ut_width);
      break;
      case 4:
	PasteIntoFV(fv,true);
      break;
      case 5:
	FVClear(fv);
      break;
      case 6:
	FVClearBackground(fv);
      break;
      case 7:
	FVCopyFgtoBg(fv);
      break;
      case 8:
	FVUnlinkRef(fv);
      break;
      case 9:
	PasteIntoFV(fv,false);
      break;
      case 10:
	FVCopyWidth(fv,ut_vwidth);
      break;
      case 11:
	FVCopyWidth(fv,ut_lbearing);
      break;
      case 12:
	FVCopyWidth(fv,ut_rbearing);
      break;
      case 13:
	FVJoin(fv);
      break;
      case 14:
	FVSameGlyphAs(fv);
      break;

      case 100:
	FVOverlap(fv,over_remove);
      break;
      case 101:
	FVSimplify(fv,false);
      break;
      case 102:
	FVAddExtrema(fv);
      break;
      case 103:
	FVRound2Int(fv);
      break;
      case 104:
	FVOverlap(fv,over_intersect);
      break;
      case 105:
	FVOverlap(fv,over_findinter);
      break;

      case 200:
	FVAutoHint(fv,true);
      break;
      case 201:
	FVClearHints(fv);
      break;
      case 202:
	FVAutoInstr(fv);
      break;
    }
}

int FVWinInfo(FontView *fv, int *cc, int *rc) {
    if ( fv==NULL || fv->colcnt==0 || fv->rowcnt==0 ) {
	*cc = 16; *rc = 4;
return( -1 );
    }

    *cc = fv->colcnt;
    *rc = fv->rowcnt;

return( fv->rowoff*fv->colcnt );
}
