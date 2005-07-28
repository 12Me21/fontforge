/* Copyright (C) 2000-2005 by George Williams */
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
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
#include <ustring.h>
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

typedef struct createwidthdata {
    unsigned int done: 1;
    void *_fv;
    void (*doit)(struct createwidthdata *);
    GWindow gw;
    real setto;
    real scale;
    real increment;
    enum settype { st_set, st_scale, st_incr } type;
    enum widthtype wtype;
} CreateWidthData;

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
#define CID_Set		1001
#define CID_Incr	1002
#define CID_Scale	1003
#define CID_SetVal	1011
#define CID_IncrVal	1012
#define CID_ScaleVal	1013

static int rb1[] = { _STR_SetWidthTo, _STR_SetLBearingTo, _STR_SetRBearingTo, _STR_SetVWidthTo };
static int rb2[] = { _STR_IncrWidthBy, _STR_IncrLBearingBy, _STR_IncrRBearingBy, _STR_IncrVWidthBy };
static int rb3[] = { _STR_ScaleWidthBy, _STR_ScaleLBearingBy, _STR_ScaleRBearingBy, _STR_ScaleVWidthBy };

static int CW_OK(GGadget *g, GEvent *e) {

    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	int err;
	CreateWidthData *wd = GDrawGetUserData(GGadgetGetWindow(g));
	if ( GGadgetIsChecked(GWidgetGetControl(wd->gw,CID_Set)) ) {
	    wd->type = st_set;
	    wd->setto = GetRealR(wd->gw,CID_SetVal,rb1[wd->wtype],&err);
	    if ( wd->setto<0 && wd->wtype==wt_width ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
		static int yesno[] = { _STR_Yes, _STR_No, 0 };
		if ( GWidgetAskR(_STR_NegativeWidth, yesno, 0, 1, _STR_NegativeWidthCheck )==1 )
#elif defined(FONTFORGE_CONFIG_GTK)
		char *yesno[3];
		yesno[0] = GTK_STOCK_YES;
		yesno[1] = GTK_STOCK_NO;
		yesno[2] = NULL;
		if ( gwwv_ask(_("Negative Width"), yesno, 0, 1, _("Negative character widths are not allowed in TrueType\nDo you really want a negative width?") )==1 )
#endif
return( true );
	    }
	} else if ( GGadgetIsChecked(GWidgetGetControl(wd->gw,CID_Incr)) ) {
	    wd->type = st_incr;
	    wd->increment = GetRealR(wd->gw,CID_IncrVal,rb2[wd->wtype],&err);
	} else {
	    wd->type = st_scale;
	    wd->scale = GetRealR(wd->gw,CID_ScaleVal,rb2[wd->wtype],&err);
	}
	(wd->doit)(wd);
    }
return( true );
}

static int CW_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	CreateWidthData *wd = GDrawGetUserData(GGadgetGetWindow(g));
	wd->done = true;
    }
return( true );
}

static int CW_FocusChange(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_textfocuschanged ) {
	CreateWidthData *wd = GDrawGetUserData(GGadgetGetWindow(g));
	int cid = (int) GGadgetGetUserData(g);
	GGadgetSetChecked(GWidgetGetControl(wd->gw,cid),true);
    }
return( true );
}

static int CW_RadioChange(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_radiochanged ) {
	CreateWidthData *wd = GDrawGetUserData(GGadgetGetWindow(g));
	int cid = (int) GGadgetGetUserData(g);
	GWidgetIndicateFocusGadget(GWidgetGetControl(wd->gw,cid));
	GTextFieldSelect(GWidgetGetControl(wd->gw,cid),0,-1);
    }
return( true );
}

static int cwd_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	CreateWidthData *wd = GDrawGetUserData(gw);
	wd->done = true;
    } else if ( event->type == et_char ) {
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

static void FVCreateWidth(void *_fv,void (*doit)(CreateWidthData *),
	enum widthtype wtype, char *def) {
    GRect pos;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[11];
    GTextInfo label[11];
    static CreateWidthData cwd;
    static GWindow winds[3];
    static int title[] = { _STR_Setwidth, _STR_Setlbearing, _STR_Setrbearing, _STR_SetVWidth };

    cwd.done = false;
    cwd._fv = _fv;
    cwd.wtype = wtype;
    cwd.doit = doit;
    cwd.gw = winds[wtype];

    if ( cwd.gw==NULL ) {
	memset(&wattrs,0,sizeof(wattrs));
	wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_isdlg|wam_restrict;
	wattrs.event_masks = ~(1<<et_charup);
	wattrs.restrict_input_to_me = 1;
	wattrs.undercursor = 1;
	wattrs.cursor = ct_pointer;
	wattrs.window_title = GStringGetResource(title[wtype],NULL);
	wattrs.is_dlg = true;
	pos.x = pos.y = 0;
	pos.width = GGadgetScale(GDrawPointsToPixels(NULL,210));
	pos.height = GDrawPointsToPixels(NULL,120);
	cwd.gw = winds[wtype] = GDrawCreateTopWindow(NULL,&pos,cwd_e_h,&cwd,&wattrs);

	memset(&label,0,sizeof(label));
	memset(&gcd,0,sizeof(gcd));

	label[0].text = (unichar_t *) rb1[wtype];
	label[0].text_in_resource = true;
	gcd[0].gd.label = &label[0];
	gcd[0].gd.pos.x = 5; gcd[0].gd.pos.y = 5; 
	gcd[0].gd.flags = gg_enabled|gg_visible|gg_cb_on;
	gcd[0].gd.cid = CID_Set;
	gcd[0].gd.handle_controlevent = CW_RadioChange;
	gcd[0].data = (void *) CID_SetVal;
	gcd[0].creator = GRadioCreate;

	label[1].text = (unichar_t *) rb2[wtype];
	label[1].text_in_resource = true;
	gcd[1].gd.label = &label[1];
	gcd[1].gd.pos.x = 5; gcd[1].gd.pos.y = 32; 
	gcd[1].gd.flags = gg_enabled|gg_visible;
	gcd[1].gd.cid = CID_Incr;
	gcd[1].gd.handle_controlevent = CW_RadioChange;
	gcd[1].data = (void *) CID_IncrVal;
	gcd[1].creator = GRadioCreate;

	label[2].text = (unichar_t *) rb3[wtype];
	label[2].text_in_resource = true;
	gcd[2].gd.label = &label[2];
	gcd[2].gd.pos.x = 5; gcd[2].gd.pos.y = 59; 
	gcd[2].gd.flags = gg_enabled|gg_visible;
	gcd[2].gd.cid = CID_Scale;
	gcd[2].gd.handle_controlevent = CW_RadioChange;
	gcd[2].data = (void *) CID_ScaleVal;
	gcd[2].creator = GRadioCreate;

	label[3].text = (unichar_t *) def;
	label[3].text_is_1byte = true;
	gcd[3].gd.label = &label[3];
	gcd[3].gd.pos.x = 131; gcd[3].gd.pos.y = 5;  gcd[3].gd.pos.width = 60;
	gcd[3].gd.flags = gg_enabled|gg_visible;
	gcd[3].gd.cid = CID_SetVal;
	gcd[3].gd.handle_controlevent = CW_FocusChange;
	gcd[3].data = (void *) CID_Set;
	gcd[3].creator = GTextFieldCreate;

	label[4].text = (unichar_t *) "0";
	label[4].text_is_1byte = true;
	gcd[4].gd.label = &label[4];
	gcd[4].gd.pos.x = 131; gcd[4].gd.pos.y = 32;  gcd[4].gd.pos.width = 60;
	gcd[4].gd.flags = gg_enabled|gg_visible;
	gcd[4].gd.cid = CID_IncrVal;
	gcd[4].gd.handle_controlevent = CW_FocusChange;
	gcd[4].data = (void *) CID_Incr;
	gcd[4].creator = GTextFieldCreate;

	label[5].text = (unichar_t *) "100";
	label[5].text_is_1byte = true;
	gcd[5].gd.label = &label[5];
	gcd[5].gd.pos.x = 131; gcd[5].gd.pos.y = 59;  gcd[5].gd.pos.width = 60;
	gcd[5].gd.flags = gg_enabled|gg_visible;
	gcd[5].gd.cid = CID_ScaleVal;
	gcd[5].gd.handle_controlevent = CW_FocusChange;
	gcd[5].data = (void *) CID_Scale;
	gcd[5].creator = GTextFieldCreate;

	gcd[6].gd.pos.x = 20-3; gcd[6].gd.pos.y = 120-32-3;
	gcd[6].gd.pos.width = -1; gcd[6].gd.pos.height = 0;
	gcd[6].gd.flags = gg_visible | gg_enabled | gg_but_default;
	label[6].text = (unichar_t *) _STR_OK;
	label[6].text_in_resource = true;
	gcd[6].gd.mnemonic = 'O';
	gcd[6].gd.label = &label[6];
	gcd[6].gd.handle_controlevent = CW_OK;
	gcd[6].creator = GButtonCreate;

	gcd[7].gd.pos.x = -20; gcd[7].gd.pos.y = 120-32;
	gcd[7].gd.pos.width = -1; gcd[7].gd.pos.height = 0;
	gcd[7].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
	label[7].text = (unichar_t *) _STR_Cancel;
	label[7].text_in_resource = true;
	gcd[7].gd.label = &label[7];
	gcd[7].gd.mnemonic = 'C';
	gcd[7].gd.handle_controlevent = CW_Cancel;
	gcd[7].creator = GButtonCreate;

	gcd[8].gd.pos.x = 2; gcd[8].gd.pos.y = 2;
	gcd[8].gd.pos.width = pos.width-4;
	gcd[8].gd.pos.height = pos.height-4;
	gcd[8].gd.flags = gg_enabled|gg_visible|gg_pos_in_pixels;
	gcd[8].creator = GGroupCreate;

	label[9].text = (unichar_t *) "%";
	label[9].text_is_1byte = true;
	gcd[9].gd.label = &label[9];
	gcd[9].gd.pos.x = 194; gcd[9].gd.pos.y = 59+6;
	gcd[9].gd.flags = gg_enabled|gg_visible;
	gcd[9].creator = GLabelCreate;

	GGadgetsCreate(cwd.gw,gcd);
	GWidgetIndicateFocusGadget(GWidgetGetControl(cwd.gw,CID_SetVal));
	GTextFieldSelect(GWidgetGetControl(cwd.gw,CID_SetVal),0,-1);
    } else {
	unichar_t *temp = uc_copy(def);
	GGadgetSetTitle(GWidgetGetControl(cwd.gw,CID_SetVal),temp);
	free( temp );
    }

    GWidgetHidePalettes();
    GDrawSetVisible(cwd.gw,true);
    while ( !cwd.done )
	GDrawProcessOneEvent(NULL);
    GDrawSetVisible(cwd.gw,false);
}
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

static void DoChar(SplineChar *sc,CreateWidthData *wd, FontView *fv,
	BDFChar *bc) {
    real transform[6];
    DBounds bb;
    IBounds ib;
    int width=0;
    BVTFunc bvts[2];
    BDFFont *bdf;

    if ( wd->wtype == wt_width ) {
	if ( wd->type==st_set )
	    width = wd->setto;
	else if ( wd->type == st_incr )
	    width = sc->width + wd->increment;
	else
	    width = sc->width * wd->scale/100;
	sc->widthset = true;
	if ( width!=sc->width ) {
	    SCPreserveWidth(sc);
	    SCSynchronizeWidth(sc,width,sc->width,fv);
	}
    } else if ( wd->wtype == wt_lbearing ) {
	transform[0] = transform[3] = 1.0;
	transform[1] = transform[2] = transform[5] = 0;
	bvts[1].func = bvt_none;
	bvts[0].func = bvt_transmove; bvts[0].y = 0;
	if ( bc==NULL ) {
	    SplineCharFindBounds(sc,&bb);
	    if ( wd->type==st_set )
		transform[4] = wd->setto-bb.minx;
	    else if ( wd->type == st_incr )
		transform[4] = wd->increment;
	    else
		transform[4] = bb.minx*wd->scale/100 - bb.minx;
	} else {
	    double scale = (fv->sf->ascent+fv->sf->descent)/(double) (fv->show->pixelsize);
	    BDFCharFindBounds(bc,&ib);
	    if ( wd->type==st_set )
		transform[4] = wd->setto-ib.minx*scale;
	    else if ( wd->type == st_incr )
		transform[4] = wd->increment;
	    else
		transform[4] = scale*ib.minx*wd->scale/100 - ib.minx;
	}
	if ( transform[4]!=0 ) {
	    FVTrans(fv,sc,transform,NULL,fvt_dontmovewidth);
	    bvts[0].x = transform[4];
	    for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next ) if ( bdf->glyphs[sc->orig_pos]!=NULL )
		BCTrans(bdf,bdf->glyphs[sc->orig_pos],bvts,fv);
	}
return;
    } else if ( wd->wtype == wt_rbearing ) {
	if ( bc==NULL ) {
	    SplineCharFindBounds(sc,&bb);
	    if ( wd->type==st_set )
		width = bb.maxx + wd->setto;
	    else if ( wd->type == st_incr )
		width = sc->width+wd->increment;
	    else
		width = (sc->width-bb.maxx) * wd->scale/100 + bb.maxx;
	} else {
	    double scale = (fv->sf->ascent+fv->sf->descent)/(double) (fv->show->pixelsize);
	    BDFCharFindBounds(bc,&ib);
	    ++ib.maxx;
	    if ( wd->type==st_set )
		width = rint(ib.maxx*scale + wd->setto);
	    else if ( wd->type == st_incr )
		width = rint(sc->width+wd->increment);
	    else
		width = rint(scale * (bc->width-ib.maxx) * wd->scale/100 + ib.maxx*scale);
	}
	if ( width!=sc->width ) {
	    SCPreserveWidth(sc);
	    SCSynchronizeWidth(sc,width,sc->width,fv);
	}
    } else {
	if ( wd->type==st_set )
	    width = wd->setto;
	else if ( wd->type == st_incr )
	    width = sc->vwidth + wd->increment;
	else
	    width = sc->vwidth * wd->scale/100;
	if ( width!=sc->vwidth ) {
	    SCPreserveVWidth(sc);
	    sc->vwidth = width;
	}
    }
    SCCharChangedUpdate(sc);
}

static void FVDoit(CreateWidthData *wd) {
    FontView *fv = (FontView *) (wd->_fv);
    int i;
    BDFChar *bc;

    if ( fv->sf->onlybitmaps && fv->show!=NULL && fv->sf->bitmaps!=NULL ) {
	double scale = (fv->sf->ascent+fv->sf->descent)/(double) fv->show->pixelsize;
	wd->setto *= scale;
	wd->increment *= scale;
    }
    bc = NULL;
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
	SplineChar *sc;

	sc = SFMakeChar(fv->sf,fv->map,i);
	if ( fv->sf->onlybitmaps && fv->sf->bitmaps!=NULL ) {
	    if ( fv->show!=NULL )
		bc = BDFMakeChar(fv->show,fv->map,i);
	    else {
		BDFFont *bdf;
		for ( bdf=fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
		    bc = BDFMakeChar(bdf,fv->map,i);
	    }
	}
	DoChar(sc,wd,fv,bc);
    }
    wd->done = true;
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void CVDoit(CreateWidthData *wd) {
    CharView *cv = (CharView *) (wd->_fv);

    DoChar(cv->sc,wd,cv->fv,NULL);
    wd->done = true;
}

static void BCDefWidthVal(char *buf,BDFChar *bc, FontView *fv, enum widthtype wtype) {
    IBounds bb;

    if ( wtype==wt_width )
	sprintf( buf, "%d", bc->width );
    else if ( wtype==wt_vwidth )
	sprintf( buf, "%d", fv->show->pixelsize );
    else {
	BDFCharFindBounds(bc,&bb);
	if ( wtype==wt_lbearing )
	    sprintf( buf, "%d", bb.minx );
	else
	    sprintf( buf, "%d", bc->width-bb.maxx-1 );
    }
}

static void SCDefWidthVal(char *buf,SplineChar *sc, enum widthtype wtype) {
    DBounds bb;

    if ( wtype==wt_width )
	sprintf( buf, "%d", sc->width );
    else if ( wtype==wt_vwidth )
	sprintf( buf, "%d", sc->vwidth );
    else {
	SplineCharFindBounds(sc,&bb);
	if ( wtype==wt_lbearing )
	    sprintf( buf, "%.4g", bb.minx );
	else
	    sprintf( buf, "%.4g", sc->width-bb.maxx );
    }
}

void FVSetWidth(FontView *fv,enum widthtype wtype) {
    char buffer[12];
    int em = fv->sf->ascent + fv->sf->descent;
    int i, gid;

    if ( !fv->sf->onlybitmaps || fv->sf->bitmaps==NULL ) {
	sprintf(buffer,"%d",wtype==wt_width?6*em/10:wtype==wt_vwidth?em: em/10 );
	for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 && fv->sf->glyphs[gid]!=NULL ) {
	    SCDefWidthVal(buffer,fv->sf->glyphs[gid],wtype);
	break;
	}
    } else {
	int size = fv->show->pixelsize;
	sprintf(buffer,"%d",wtype==wt_width?6*size/10:wtype==wt_vwidth?size: size/10 );
	for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 && fv->show->glyphs[gid]!=NULL ) {
	    BCDefWidthVal(buffer,fv->show->glyphs[gid],fv,wtype);
	break;
	}
    }
    FVCreateWidth(fv,FVDoit,wtype,buffer);
}

void CVSetWidth(CharView *cv,enum widthtype wtype) {
    char buf[10];

    SCDefWidthVal(buf,cv->sc,wtype);
    FVCreateWidth(cv,CVDoit,wtype,buf);
}
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

void FVSetWidthScript(FontView *fv,enum widthtype wtype,int val,int incr) {
    CreateWidthData wd;

    memset(&wd,0,sizeof(wd));
    wd._fv = fv;
    wd.doit = FVDoit;
    wd.setto = wd.increment = wd.scale = val;
    wd.type = incr==0 ? st_set : incr==2 ? st_scale : st_incr;
    wd.wtype = wtype;
    FVDoit(&wd);
}
