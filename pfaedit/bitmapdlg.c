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
#include "gwidget.h"
#include "ustring.h"
#include <math.h>
#include <gkeysym.h>

static int oldusefreetype=1;
int oldsystem=0 /* X */;

typedef struct createbitmapdata {
    unsigned int done: 1;
    FontView *fv;
    SplineFont *sf;
    SplineChar *sc;
    int isavail;
    int which;
    GWindow gw;
} CreateBitmapData;

enum { bd_all, bd_selected, bd_current };
static GTextInfo which[] = {
    { (unichar_t *) _STR_AllChars, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
    { (unichar_t *) _STR_SelChars, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
    { (unichar_t *) _STR_CurChar, NULL, 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1 },
    { NULL }};
static int lastwhich = bd_selected;

static void RemoveBDFWindows(BDFFont *bdf) {
    int i;
    BitmapView *bv, *next;

    for ( i=0; i<bdf->charcnt; ++i ) if ( bdf->chars[i]!=NULL ) {
	for ( bv = bdf->chars[i]->views; bv!=NULL; bv=next ) {
	    next = bv->next;
	    GDrawDestroyWindow(bv->gw);
	}
    }
    if ( screen_display!=NULL ) {
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
	/* Just in case... */
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
    }
    /* We can't free the bdf until all the windows have executed their destroy*/
    /*  routines (which they will when they get the destroy window event) */
    /*  because those routines depend on the bdf existing ... */
}

void SFOrderBitmapList(SplineFont *sf) {
    BDFFont *bdf, *prev, *next;
    BDFFont *bdf2, *prev2;

    for ( prev = NULL, bdf=sf->bitmaps; bdf!=NULL; bdf = bdf->next ) {
	for ( prev2=NULL, bdf2=bdf->next; bdf2!=NULL; bdf2 = bdf2->next ) {
	    if ( bdf->pixelsize>bdf2->pixelsize ||
		    (bdf->pixelsize==bdf2->pixelsize && BDFDepth(bdf)>BDFDepth(bdf2)) ) {
		if ( prev==NULL )
		    sf->bitmaps = bdf2;
		else
		    prev->next = bdf2;
		if ( prev2==NULL ) {
		    bdf->next = bdf2->next;
		    bdf2->next = bdf;
		} else {
		    next = bdf->next;
		    bdf->next = bdf2->next;
		    bdf2->next = next;
		    prev2->next = bdf;
		}
		next = bdf;
		bdf = bdf2;
		bdf2 = next;
	    }
	    prev2 = bdf2;
	}
	prev = bdf;
    }
}

static void SFRemoveUnwantedBitmaps(SplineFont *sf,int32 *sizes) {
    BDFFont *bdf, *prev, *next;
    FontView *fv;
    int i;

    for ( prev = NULL, bdf=sf->bitmaps; bdf!=NULL; bdf = next ) {
	next = bdf->next;
	for ( i=0; sizes[i]!=0 && ((sizes[i]&0xffff)!=bdf->pixelsize || (sizes[i]>>16)!=BDFDepth(bdf)); ++i );
	if ( sizes[i]==0 ) {
	    /* To be deleted */
	    if ( prev==NULL )
		sf->bitmaps = next;
	    else
		prev->next = next;
	    for ( fv=sf->fv; fv!=NULL; fv=fv->nextsame ) {
		if ( fv->show==bdf ) {
		    if ( sf->onlybitmaps )
			FVChangeDisplayBitmap(fv,sf->bitmaps);
		    else
			FVShowFilled(sf->fv);
		}
	    }
	    RemoveBDFWindows(bdf);
	    BDFFontFree(bdf);
	    sf->changed = true;
	} else {
	    sizes[i] = -sizes[i];		/* don't need to create it */
	    prev = bdf;
	}
    }
}

static void SFFigureBitmaps(SplineFont *sf,int32 *sizes,int usefreetype) {
    BDFFont *bdf;
    int i, first;
    void *freetypecontext = NULL;

    SFRemoveUnwantedBitmaps(sf,sizes);

    first = true;
    for ( i=0; sizes[i]!=0 ; ++i ) if ( sizes[i]>0 ) {
	if ( first && autohint_before_rasterize )
	    SplineFontAutoHint(sf);
	if ( first && usefreetype )
	    freetypecontext = FreeTypeFontContext(sf,NULL,true);
	if ( freetypecontext )
	    bdf = SplineFontFreeTypeRasterize(freetypecontext,sizes[i]&0xffff,sizes[i]>>16);
	else
	    bdf = SplineFontAntiAlias(sf,sizes[i]&0xffff,1<<((sizes[i]>>16)/2));
	bdf->next = sf->bitmaps;
	sf->bitmaps = bdf;
	sf->changed = true;
	first = false;
    }
    if ( freetypecontext )
	FreeTypeFreeContext(freetypecontext);

    /* Order the list */
    SFOrderBitmapList(sf);
}

static void FVScaleBitmaps(FontView *fv,int32 *sizes) {
    BDFFont *bdf, *scale;
    int i, cnt=0, warned = false;

    for ( i=0; sizes[i]!=0 ; ++i ) if ( sizes[i]>0 )
	++cnt;
    scale = fv->show;
    if ( scale->clut!=NULL )
	for ( scale = fv->sf->bitmaps; scale->next!=NULL; scale=scale->next );
    GProgressStartIndicatorR(10,_STR_ScalingBitmaps,_STR_ScalingBitmaps,0,cnt,1);

    for ( i=0; sizes[i]!=0 ; ++i ) if ( sizes[i]>0 && (sizes[i]>>16)==1 ) {
	bdf = BitmapFontScaleTo(scale,sizes[i]&0xffff);
	if ( bdf==NULL )
    continue;
	bdf->next = fv->sf->bitmaps;
	fv->sf->bitmaps = bdf;
	fv->sf->changed = true;
	if ( !GProgressNext())
    break;
    } else if ( sizes[i]>0 && (sizes[i]>>16)!=1 && !warned ) {
	GWidgetErrorR(_STR_CantScaleGreymap,_STR_CantScaleGreymap);
	warned = true;
    }
    GProgressEndIndicator();

    /* Order the list */
    SFOrderBitmapList(fv->sf);

    SFRemoveUnwantedBitmaps(fv->sf,sizes);
}

static void ReplaceBDFC(SplineFont *sf,int32 *sizes,int enc,void *freetypecontext) {
    BDFFont *bdf;
    BDFChar *bdfc, temp;
    int i;
    BitmapView *bv;

    if ( enc>=sf->charcnt || sf->chars[enc]==NULL )
return;

    for ( bdf=sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	for ( i=0; sizes[i]!=0 && (bdf->pixelsize!=(sizes[i]&0xffff) || BDFDepth(bdf)!=(sizes[i]>>16)); ++i );
	if ( sizes[i]!=0 ) {
	    if ( freetypecontext )
		bdfc = SplineCharFreeTypeRasterize(freetypecontext,enc,bdf->pixelsize,BDFDepth(bdf));
	    else {
		if ( autohint_before_rasterize && 
			sf->chars[enc]->changedsincelasthinted &&
			!sf->chars[enc]->manualhints )
		    SplineCharAutoHint(sf->chars[enc],true);
		bdfc = SplineCharAntiAlias(sf->chars[enc],bdf->pixelsize,(1<<(BDFDepth(bdf)/2)));
	    }
	    if ( bdf->chars[enc]==NULL )
		bdf->chars[enc] = bdfc;
	    else {
		temp = *(bdf->chars[enc]);
		*bdf->chars[enc] = *bdfc;
		*bdfc = temp;
		bdf->chars[enc]->views = bdfc->views;
		bdfc->views = NULL;
		BDFCharFree(bdfc);
		for ( bv = bdf->chars[enc]->views; bv!=NULL; bv=bv->next ) {
		    GDrawRequestExpose(bv->v,NULL,false);
		    /* Mess with selection?????!!!!! */
		}
	    }
	}
    }
}

static int FVRegenBitmaps(CreateBitmapData *bd,int32 *sizes,int usefreetype) {
    FontView *fv = bd->fv;
    SplineFont *sf = bd->sf, *subsf;
    int i,j;
    BDFFont *bdf;
    void *freetypecontext=NULL;

    for ( i=0; sizes[i]!=0; ++i ) {
	for ( bdf = sf->bitmaps; bdf!=NULL &&
		(bdf->pixelsize!=(sizes[i]&0xffff) || BDFDepth(bdf)!=(sizes[i]>>16));
		bdf=bdf->next );
	if ( bdf==NULL ) {
	    unichar_t temp[100];
	    char buffer[10];
	    u_strcpy(temp,GStringGetResource(_STR_BadRegenSize,NULL));
	    if ( (sizes[i]>>16)==1 )
		sprintf(buffer,"%d", sizes[i]&0xffff);
	    else
		sprintf(buffer,"%d@%d", sizes[i]&0xffff, sizes[i]>>16);
	    uc_strcat(temp,buffer);
	    GWidgetPostNotice(temp,temp);
return( false );
	}
    }
    if ( bd->which==bd_current && bd->sc!=NULL ) {
	if ( usefreetype )
	    freetypecontext = FreeTypeFontContext(bd->sc->parent,bd->sc,false);
	ReplaceBDFC(bd->sc->parent,sizes,bd->sc->enc,freetypecontext);
	if ( freetypecontext )
	    FreeTypeFreeContext(freetypecontext);
    } else {
	if ( sf->subfontcnt!=0 ) {
	    for ( j=0 ; j<sf->subfontcnt; ++j ) {
		subsf = sf->subfonts[j];
		for ( i=0; i<subsf->charcnt; ++i ) {
		    if (( fv->selected[i] || bd->which == bd_all ) &&
			    SCWorthOutputting(subsf->chars[i])) {
			if ( usefreetype && freetypecontext==NULL )
			    freetypecontext = FreeTypeFontContext(subsf,NULL,false);
			ReplaceBDFC(subsf,sizes,i,freetypecontext);
		    }
		}
		if ( freetypecontext )
		    FreeTypeFreeContext(freetypecontext);
		freetypecontext = NULL;
	    }
	} else {
	    for ( i=0; i<sf->charcnt; ++i ) {
		if ( fv->selected[i] || bd->which == bd_all ) {
		    if ( usefreetype && freetypecontext==NULL )
			freetypecontext = FreeTypeFontContext(sf,NULL,false);
		    ReplaceBDFC(sf,sizes,i,freetypecontext);
		}
	    }
	    if ( freetypecontext )
		FreeTypeFreeContext(freetypecontext);
	    freetypecontext = NULL;
	}
    }
    sf->changed = true;
    if ( fv->show!=fv->filled ) {
	for ( i=0; sizes[i]!=0 && ((sizes[i]&0xffff)!=fv->show->pixelsize || (sizes[i]>>16)!=BDFDepth(fv->show)); ++i );
	if ( sizes[i]!=0 && fv->v!=NULL )
	    GDrawRequestExpose(fv->v,NULL,false );
    }
return( true );
}

#define CID_Which	1001
#define CID_Pixel	1002
#define CID_75		1003
#define CID_100		1004
#define CID_75Lab	1005
#define CID_100Lab	1006
#define CID_X		1007
#define CID_Win		1008
#define CID_Mac		1009
#define CID_FreeType	1010

static int GetSystem(GWindow gw) {
    if ( GGadgetIsChecked(GWidgetGetControl(gw,CID_X)) )
return( CID_X );
    if ( GGadgetIsChecked(GWidgetGetControl(gw,CID_Win)) )
return( CID_Win );

return( CID_Mac );
}
    
static int32 *ParseList(GWindow gw, int cid,int *err, int final) {
    const unichar_t *val = _GGadgetGetTitle(GWidgetGetControl(gw,cid)), *pt;
    unichar_t *end, *end2;
    int i;
    real *sizes;
    int32 *ret;
    int system = GetSystem(gw);
    int scale;

    *err = false;
    end2 = NULL;
    for ( i=1, pt = val; (end = u_strchr(pt,',')) || (end2=u_strchr(pt,' ')); ++i ) {
	if ( end!=NULL && end2!=NULL ) {
	    if ( end2<end ) end = end2;
	} else if ( end2!=NULL )
	    end = end2;
	pt = end+1;
	end2 = NULL;
    }
    sizes = galloc((i+1)*sizeof(real));
    ret = galloc((i+1)*sizeof(int32));

    for ( i=0, pt = val; *pt!='\0' ; ) {
	sizes[i]=u_strtod(pt,&end);
	if ( *end=='@' )
	    ret[i] = (u_strtol(end+1,&end,10)<<16);
	else
	    ret[i] = 0x10000;
	if ( sizes[i]>0 ) ++i;
	if ( *end!=' ' && *end!=',' && *end!='\0' ) {
	    free(sizes); free(ret);
	    if ( final )
		ProtestR(_STR_PixelSizes);
	    *err = true;
return( NULL );
	}
	while ( *end==' ' || *end==',' ) ++end;
	pt = end;
    }
    sizes[i] = 0; ret[i] = 0;

    if ( cid==CID_75 ) {
	scale = system==CID_X?75:system==CID_Win?96:72;
	for ( i=0; sizes[i]!=0; ++i )
	    ret[i] |= (int) rint(sizes[i]*scale/72);
    } else if ( cid==CID_100 ) {
	scale = system==CID_X?100:system==CID_Win?120:100;
	for ( i=0; sizes[i]!=0; ++i )
	    ret[i] |= (int) rint(sizes[i]*scale/72);
    } else
	for ( i=0; sizes[i]!=0; ++i )
	    ret[i] |= (int) rint(sizes[i]);
    free(sizes);
return( ret );
}

static void BitmapsDoIt(CreateBitmapData *bd,int32 *sizes,int usefreetype) {

    if ( bd->isavail && bd->sf->onlybitmaps && bd->sf->bitmaps!=NULL ) {
	if ( sizes[0]!=0 )
	    FVScaleBitmaps(bd->fv,sizes);
	else {
	    GWidgetErrorR(_STR_CantDeleteAllBitmaps,_STR_CantDeleteAllBitmaps);
return;
	}
    } else if ( bd->isavail )
	SFFigureBitmaps(bd->sf,sizes,usefreetype);
    else {
	if ( !FVRegenBitmaps(bd,sizes,usefreetype))
	    bd->done = false;
	else
	    lastwhich = bd->which;
    }
    bd->done = true;
}

static int CB_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	int err;
	CreateBitmapData *bd = GDrawGetUserData(GGadgetGetWindow(g));
	int32 *sizes = ParseList(bd->gw, CID_Pixel,&err, true);
	if ( err )
return( true );
	oldusefreetype = GGadgetIsChecked(GWidgetGetControl(bd->gw,CID_FreeType));
	oldsystem = GetSystem(bd->gw)-CID_X;
	if ( !bd->isavail )
	    bd->which = GGadgetGetFirstListSelectedItem(GWidgetGetControl(bd->gw,CID_Which));
	BitmapsDoIt(bd,sizes,oldusefreetype);
	free(sizes);
	SavePrefs();
    }
return( true );
}

static int CB_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	CreateBitmapData *bd = GDrawGetUserData(GGadgetGetWindow(g));
	bd->done = true;
    }
return( true );
}

static unichar_t *GenText(int32 *sizes,real scale) {
    int i;
    char *cret, *pt;
    unichar_t *uret;

    for ( i=0; sizes[i]!=0; ++i );
    pt = cret = galloc(i*10);
    for ( i=0; sizes[i]!=0; ++i ) {
	if ( pt!=cret ) *pt++ = ',';
	sprintf(pt,"%.1f",(sizes[i]&0xffff)*scale );
	pt += strlen(pt);
	if ( pt[-1]=='0' && pt[-2]=='.' ) {
	    pt -= 2;
	    *pt = '\0';
	}
	if ( (sizes[i]>>16)!=1 ) {
	    sprintf(pt,"@%d", sizes[i]>>16 );
	    pt += strlen(pt);
	}
    }
    *pt = '\0';
    uret = uc_copy(cret);
    free(cret);
return( uret );
}

static void _CB_TextChange(CreateBitmapData *bd, GGadget *g) {
    int cid = (int) GGadgetGetCid(g);
    unichar_t *val;
    int err=false;
    int32 *sizes = ParseList(bd->gw,cid,&err,false);
    int ncid;
    int system = GetSystem(bd->gw);
    int scale;

    if ( err )
return;
    for ( ncid=CID_Pixel; ncid<=CID_100; ++ncid ) if ( ncid!=cid ) {
	if ( ncid==CID_Pixel )
	    scale = 72;
	else if ( ncid==CID_75 )
	    scale = system==CID_X?75: system==CID_Win?96 : 72;
	else
	    scale = system==CID_X?100: system==CID_Win?120 : 100;
	val = GenText(sizes,72./scale);
	GGadgetSetTitle(GWidgetGetControl(bd->gw,ncid),val);
	free(val);
    }
    free(sizes);
return;
}

static int CB_TextChange(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_textchanged ) {
	CreateBitmapData *bd = GDrawGetUserData(GGadgetGetWindow(g));
	_CB_TextChange(bd,g);
    }
return( true );
}

static void _CB_SystemChange(CreateBitmapData *bd) {
    int system = GetSystem(bd->gw);
    GGadgetSetTitle(GWidgetGetControl(bd->gw,CID_75Lab),
	    GStringGetResource(system==CID_X?_STR_PointSizes75:
			       system==CID_Win?_STR_PointSizes96:
					       _STR_PointSizes72,NULL));
    GGadgetSetTitle(GWidgetGetControl(bd->gw,CID_100Lab),
	    GStringGetResource(system==CID_Win?_STR_PointSizes120:
					       _STR_PointSizes100,NULL));
    GGadgetSetEnabled(GWidgetGetControl(bd->gw,CID_100Lab),system!=CID_Mac);
    GGadgetSetEnabled(GWidgetGetControl(bd->gw,CID_100),system!=CID_Mac);
    _CB_TextChange(bd,GWidgetGetControl(bd->gw,CID_Pixel));
}

static int CB_SystemChange(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_radiochanged ) {
	CreateBitmapData *bd = GDrawGetUserData(GGadgetGetWindow(g));
	_CB_SystemChange(bd);
    }
return( true );
}

static int bd_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	CreateBitmapData *bd = GDrawGetUserData(gw);
	bd->done = true;
    } else if ( event->type == et_char ) {
	if ( event->u.chr.keysym == GK_F1 || event->u.chr.keysym == GK_Help ) {
	    help("elementmenu.html#Bitmaps");
return( true );
	}
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

void BitmapDlg(FontView *fv,SplineChar *sc, int isavail) {
    GRect pos;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[17];
    GTextInfo label[17];
    CreateBitmapData bd;
    int i,j,y;
    int32 *sizes;
    BDFFont *bdf;

    bd.fv = fv;
    bd.sc = sc;
    bd.sf = fv->cidmaster ? fv->cidmaster : fv->sf;
    bd.isavail = isavail;
    bd.done = false;

    for ( bdf=bd.sf->bitmaps, i=0; bdf!=NULL; bdf=bdf->next, ++i );
    if ( i==0 && isavail )
	i = 2;
    sizes = galloc((i+1)*sizeof(int32));
    for ( bdf=bd.sf->bitmaps, i=0; bdf!=NULL; bdf=bdf->next, ++i )
	sizes[i] = bdf->pixelsize | (BDFDepth(bdf)<<16);
/*
    if ( i==0 && isavail ) {
	sizes[i++] = 12.5;
	sizes[i++] = 17;
    }
*/
    sizes[i] = 0;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_isdlg|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = GStringGetResource(isavail ? _STR_Bitmapsavail : _STR_Regenbitmaps,NULL );
    wattrs.is_dlg = true;
    pos.x = pos.y = 0;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,190));
    pos.height = GDrawPointsToPixels(NULL,252);
    bd.gw = GDrawCreateTopWindow(NULL,&pos,bd_e_h,&bd,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    if ( isavail ) {
	label[0].text = (unichar_t *) _STR_ListPixelSizes;
	label[0].text_in_resource = true;
	gcd[0].gd.label = &label[0];
	gcd[0].gd.pos.x = 5; gcd[0].gd.pos.y = 5; 
	gcd[0].gd.flags = gg_enabled|gg_visible|gg_cb_on;
	gcd[0].creator = GLabelCreate;

	label[1].text = (unichar_t *) _STR_RemovingSize;
	label[1].text_in_resource = true;
	gcd[1].gd.label = &label[1];
	gcd[1].gd.pos.x = 5; gcd[1].gd.pos.y = 5+13;
	gcd[1].gd.flags = gg_enabled|gg_visible;
	gcd[1].creator = GLabelCreate;

	if ( bd.sf->onlybitmaps && bd.sf->bitmaps!=NULL )
	    label[2].text = (unichar_t *) _STR_AddingSizeScale;
	else
	    label[2].text = (unichar_t *) _STR_AddingSize;
	label[2].text_in_resource = true;
	gcd[2].gd.label = &label[2];
	gcd[2].gd.pos.x = 5; gcd[2].gd.pos.y = 5+26;
	gcd[2].gd.flags = gg_enabled|gg_visible;
	gcd[2].creator = GLabelCreate;
	j = 3; y = 5+39+3;
    } else {
	label[0].text = (unichar_t *) _STR_SpecifyRegenSizes;
	label[0].text_in_resource = true;
	gcd[0].gd.label = &label[0];
	gcd[0].gd.pos.x = 5; gcd[0].gd.pos.y = 5; 
	gcd[0].gd.flags = gg_enabled|gg_visible|gg_cb_on;
	gcd[0].creator = GLabelCreate;

	if ( lastwhich==bd_current && sc==NULL )
	    lastwhich = bd_selected;
	gcd[1].gd.label = &which[lastwhich];
	gcd[1].gd.u.list = which;
	gcd[1].gd.pos.x = 5; gcd[1].gd.pos.y = 5+13;
	gcd[1].gd.flags = gg_enabled|gg_visible;
	gcd[1].gd.cid = CID_Which;
	gcd[1].creator = GListButtonCreate;
	which[bd_current].disabled = sc==NULL;
	which[lastwhich].selected = true;

	j=2; y = 5+13+28;
    }

    label[j].text = (unichar_t *) _STR_XSizes;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 10; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_X;
    gcd[j].gd.handle_controlevent = CB_SystemChange;
    gcd[j++].creator = GRadioCreate;

    label[j].text = (unichar_t *) _STR_WinSizes;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 50; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_Win;
    gcd[j].gd.handle_controlevent = CB_SystemChange;
    gcd[j++].creator = GRadioCreate;

    label[j].text = (unichar_t *) _STR_MacSizes;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 90; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_Mac;
    gcd[j].gd.handle_controlevent = CB_SystemChange;
    gcd[j++].creator = GRadioCreate;
    y += 26;
    gcd[j-3+oldsystem].gd.flags |= gg_cb_on;

    label[j].text = (unichar_t *) _STR_PointSizes75;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 5; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_75Lab;
    gcd[j++].creator = GLabelCreate;
    y += 13;

    label[j].text = GenText(sizes,oldsystem==0?72./75.:oldsystem==1?72/96.:1.);
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 15; gcd[j].gd.pos.y = y;
    gcd[j].gd.pos.width = 170;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_75;
    gcd[j].gd.handle_controlevent = CB_TextChange;
    gcd[j++].creator = GTextFieldCreate;
    y += 26;

    label[j].text = (unichar_t *) _STR_PointSizes100;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 5; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_100Lab;
    gcd[j++].creator = GLabelCreate;
    y += 13;

    label[j].text = GenText(sizes,oldsystem==1?72./120.:72/100.);
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 15; gcd[j].gd.pos.y = y;
    gcd[j].gd.pos.width = 170;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    if ( oldsystem==2 )
	gcd[j].gd.flags = gg_visible;
    gcd[j].gd.cid = CID_100;
    gcd[j].gd.handle_controlevent = CB_TextChange;
    gcd[j++].creator = GTextFieldCreate;
    y += 26;

    label[j].text = (unichar_t *) _STR_PixelSizes;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 5; gcd[j].gd.pos.y = y;
    gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j++].creator = GLabelCreate;
    y += 13;

    label[j].text = GenText(sizes,1.);
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 15; gcd[j].gd.pos.y = y;
    gcd[j].gd.pos.width = 170;
    gcd[j].gd.flags = gg_enabled|gg_visible|gg_cb_on;
    gcd[j].gd.cid = CID_Pixel;
    gcd[j].gd.handle_controlevent = CB_TextChange;
    gcd[j++].creator = GTextFieldCreate;
    y += 26;

    label[j].text = (unichar_t *) _STR_UseFreeType;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.pos.x = 10; gcd[j].gd.pos.y = y;
    if ( !hasFreeType() || bd.sf->onlybitmaps)
	gcd[j].gd.flags = gg_visible;
    else if ( oldusefreetype )
	gcd[j].gd.flags = gg_enabled|gg_visible|gg_cb_on;
    else
	gcd[j].gd.flags = gg_enabled|gg_visible;
    gcd[j].gd.cid = CID_FreeType;
    gcd[j].gd.handle_controlevent = CB_SystemChange;
    gcd[j++].creator = GCheckBoxCreate;
    y += 26;

    gcd[j].gd.pos.x = 2; gcd[j].gd.pos.y = 2;
    gcd[j].gd.pos.width = pos.width-4;
    gcd[j].gd.pos.height = pos.height-4;
    gcd[j].gd.flags = gg_enabled|gg_visible|gg_pos_in_pixels;
    gcd[j++].creator = GGroupCreate;

    gcd[j].gd.pos.x = 20-3; gcd[j].gd.pos.y = 252-32-3;
    gcd[j].gd.pos.width = -1; gcd[j].gd.pos.height = 0;
    gcd[j].gd.flags = gg_visible | gg_enabled | gg_but_default;
    label[j].text = (unichar_t *) _STR_OK;
    label[j].text_in_resource = true;
    gcd[j].gd.mnemonic = 'O';
    gcd[j].gd.label = &label[j];
    gcd[j].gd.handle_controlevent = CB_OK;
    gcd[j++].creator = GButtonCreate;

    gcd[j].gd.pos.x = -20; gcd[j].gd.pos.y = 252-32;
    gcd[j].gd.pos.width = -1; gcd[j].gd.pos.height = 0;
    gcd[j].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[j].text = (unichar_t *) _STR_Cancel;
    label[j].text_in_resource = true;
    gcd[j].gd.label = &label[j];
    gcd[j].gd.mnemonic = 'C';
    gcd[j].gd.handle_controlevent = CB_Cancel;
    gcd[j++].creator = GButtonCreate;

    GGadgetsCreate(bd.gw,gcd);
    which[lastwhich].selected = false;
    _CB_SystemChange(&bd);

    GWidgetHidePalettes();
    GDrawSetVisible(bd.gw,true);
    while ( !bd.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(bd.gw);
}

int BitmapControl(FontView *fv,int32 *sizes,int isavail) {
    CreateBitmapData bd;

    memset(&bd,0,sizeof(bd));
    bd.fv = fv;
    bd.sf = fv->sf;
    bd.isavail = isavail;
    bd.which = bd_selected;
    BitmapsDoIt(&bd,sizes,hasFreeType());
return( bd.done );
}
