/* Copyright (C) 2003,2004 by George Williams */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.

 * The name of the author may not be used to endorse or promote products
 * dercved from this software without specific prior written permission.

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
#include <gkeysym.h>
#include <ustring.h>

#ifndef FREETYPE_HAS_DEBUGGER
void CVDebugReInit(CharView *cv,int restart_debug,int debug_fpgm) {
}

void CVDebugFree(DebugView *dv) {
}

int DVChar(DebugView *dv, GEvent *event) {
return( false );
}
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include "ttinterp.h"

static int DVBpCheck(struct instrinfo *ii, int ip) {
    DebugView *dv = (ii->userdata);
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);

    if ( exc==NULL )
return( false );

return( DebuggerBpCheck( dv->dc,exc->curRange,ip));
}

static void DVToggleBp(struct instrinfo *ii, int ip) {
    DebugView *dv = (ii->userdata);
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);

    if ( exc==NULL )
return;
    DebuggerToggleBp(dv->dc,exc->curRange,ip);
}

static void DVRegExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    char buffer[100];
    unichar_t ubuffer[100];
    int y;

    GDrawFillRect(pixmap,&event->u.expose.rect,GDrawGetDefaultBackground(screen_display));
    if ( exc==NULL )
return;
    GDrawSetFont(pixmap,dv->ii.gfont);
    y = 3+dv->ii.as;

    sprintf( buffer, " rp0: %d", exc->GS.rp0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, " rp1: %d", exc->GS.rp1 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, " rp2: %d", exc->GS.rp2 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "loop: %ld", exc->GS.loop ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    y+=2;

    sprintf( buffer, " zp0: %s", exc->GS.gep0?"Normal":"Twilight" ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, " zp1: %s", exc->GS.gep1?"Normal":"Twilight" ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, " zp2: %s", exc->GS.gep2?"Normal":"Twilight" ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    y+=2;

    sprintf( buffer, "MinDist: %.2f", exc->GS.minimum_distance/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "CvtCutin: %.2f", exc->GS.control_value_cutin/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "SingWidCut: %.2f", exc->GS.single_width_cutin/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "SingWidVal: %.2f", exc->GS.single_width_value/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    y+=2;

    sprintf( buffer, "freeVec: %g,%g", (((int)exc->GS.freeVector.x<<16)>>(16+14)) + ((exc->GS.freeVector.x&0x3fff)/16384.0),
	    (((int)exc->GS.freeVector.y<<16)>>(16+14)) + ((exc->GS.freeVector.y&0x3fff)/16384.0) ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "projVec: %g,%g", (((int)exc->GS.projVector.x<<16)>>(16+14)) + ((exc->GS.projVector.x&0x3fff)/16384.0),
	    (((int)exc->GS.projVector.y<<16)>>(16+14)) + ((exc->GS.projVector.y&0x3fff)/16384.0) ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "dualVec: %g,%g", (((int)exc->GS.dualVector.x<<16)>>(16+14)) + ((exc->GS.dualVector.x&0x3fff)/16384.0),
	    (((int)exc->GS.dualVector.y<<16)>>(16+14)) + ((exc->GS.dualVector.y&0x3fff)/16384.0) ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    y+=2;

    sprintf( buffer, "AutoFlip: %s", exc->GS.auto_flip?"True": "False" ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "DeltaBase: %d", exc->GS.delta_base ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "DeltaShift: %d", exc->GS.delta_shift ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "RndState: %s",
	    exc->GS.round_state==TT_Round_To_Half_Grid? "To Half Grid" :
	    exc->GS.round_state==TT_Round_To_Grid? "To Grid" :
	    exc->GS.round_state==TT_Round_To_Double_Grid? "To Double Grid" :
	    exc->GS.round_state==TT_Round_Down_To_Grid? "Down To Grid" :
	    exc->GS.round_state==TT_Round_Up_To_Grid? "Up To Grid" :
	    exc->GS.round_state==TT_Round_Off? "Off" :
	    exc->GS.round_state==TT_Round_Super? "Super" :
	    exc->GS.round_state==TT_Round_Super_45? "Super45" :
		"Unknown" );
    uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "SRndPeriod: %.2f", exc->period/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "SRndPhase: %.2f", exc->phase/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "SRndThreshold: %.2f", exc->threshold/64.0 ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "InstrControl: %d", exc->GS.instruct_control ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "ScanControl: %s", exc->GS.scan_control?"True": "False" ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;
    sprintf( buffer, "ScanType: %d", exc->GS.scan_type ); uc_strcpy(ubuffer,buffer);
    GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0); y += dv->ii.fh;

    /* Instruction control, scan control, scan type, phase, threshold for super rounding */
}

static void DVStackExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    char buffer[100];
    unichar_t ubuffer[100];
    int i, y;

    GDrawFillRect(pixmap,&event->u.expose.rect,GDrawGetDefaultBackground(screen_display));
    if ( exc==NULL )
return;
    GDrawSetFont(pixmap,dv->ii.gfont);
    y = 3+dv->ii.as;
    for ( i=exc->top-1; i>=0; --i ) {
	sprintf(buffer, "%3d: %3ld (%.2f)", i, exc->stack[i], exc->stack[i]/64.0 );
	uc_strcpy(ubuffer,buffer);
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
	if ( y>event->u.expose.rect.y+event->u.expose.rect.height )
    break;
	y += dv->ii.fh;
    }
    if ( exc->top==0 ) {
	uc_strcpy(ubuffer,"<empty>");
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
    }
}

static void DVStorageExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    char buffer[100];
    unichar_t ubuffer[100];
    int i, y;

    GDrawFillRect(pixmap,&event->u.expose.rect,GDrawGetDefaultBackground(screen_display));
    if ( exc==NULL )
return;
    GDrawSetFont(pixmap,dv->ii.gfont);
    y = 3+dv->ii.as;
    for ( i=0; i<exc->storeSize; ++i ) {
	sprintf(buffer, "%3d: %3ld (%.2f)", i, exc->storage[i], exc->storage[i]/64.0 );
	uc_strcpy(ubuffer,buffer);
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
	if ( y>event->u.expose.rect.y+event->u.expose.rect.height )
    break;
	y += dv->ii.fh;
    }
    if ( exc->storeSize==0 ) {
	uc_strcpy(ubuffer,"<empty>");
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
    }
}

static void DVCvtExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    char buffer[100];
    unichar_t ubuffer[100];
    int i, y;

    GDrawFillRect(pixmap,&event->u.expose.rect,GDrawGetDefaultBackground(screen_display));
    if ( exc==NULL )
return;
    GDrawSetFont(pixmap,dv->ii.gfont);
    y = 3+dv->ii.as;
    for ( i=0; dv->cvt_offtop+i<exc->cvtSize; ++i ) {
	sprintf(buffer, "%3d: %3ld (%.2f)", dv->cvt_offtop+i,
		exc->cvt[dv->cvt_offtop+i], exc->cvt[dv->cvt_offtop+i]/64.0 );
	uc_strcpy(ubuffer,buffer);
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
	if ( y>event->u.expose.rect.y+event->u.expose.rect.height )
    break;
	y += dv->ii.fh;
    }
    if ( exc->cvtSize==0 ) {
	uc_strcpy(ubuffer,"<empty>");
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
    }
}

#define CID_Twilight	1001
#define CID_Normal	1002
#define CID_Grid	1003
#define CID_EmUnit	1004
#define CID_Current	1005
#define CID_Original	1006
static int show_twilight = true, show_grid=true, show_current=true;

static void DVPointsVExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    char buffer[100];
    unichar_t ubuffer[100];
    int i, y;
    FT_Vector *pts;
    int n, n_watch;
    char watched;
    TT_GlyphZoneRec *r;
    uint8 *watches;

    GDrawFillRect(pixmap,&event->u.expose.rect,GDrawGetDefaultBackground(screen_display));
    if ( exc==NULL )
return;

    show_twilight = GGadgetIsChecked(GWidgetGetControl(dv->points,CID_Twilight));
    show_grid = GGadgetIsChecked(GWidgetGetControl(dv->points,CID_Grid));
    show_current = GGadgetIsChecked(GWidgetGetControl(dv->points,CID_Current));
    r = show_twilight ? &exc->twilight : &exc->pts;
    n = r->n_points;
    pts = show_current ? r->cur : r->org;

    watches = DebuggerGetWatches(dv->dc,&n_watch);

    GDrawSetFont(pixmap,dv->ii.gfont);
    y = 3+dv->ii.as;
    for ( i=0; i<n; ++i ) {
	watched = i<n_watch && !show_twilight && watches!=NULL && watches[i] ? 'W' : ' ';
	if ( show_grid )
	    sprintf(buffer, "%3d: %c%c%c %.2f,%.2f", i,
		    r->tags[i]&FT_Curve_Tag_Touch_X?'H':' ', r->tags[i]&FT_Curve_Tag_Touch_Y?'V':' ', watched,
		    pts[i].x/64.0, pts[i].y/64.0 );
	else
	    sprintf(buffer, "%3d: %c%c%c %g,%g", i,
		    r->tags[i]&FT_Curve_Tag_Touch_X?'H':' ', r->tags[i]&FT_Curve_Tag_Touch_Y?'V':' ', watched,
		    pts[i].x*dv->scale, pts[i].y*dv->scale );
	uc_strcpy(ubuffer,buffer);
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
	if ( y>event->u.expose.rect.y+event->u.expose.rect.height )
    break;
	y += dv->ii.fh;
    }
    if ( n==0 ) {
	uc_strcpy(ubuffer,"<none>");
	GDrawDrawText(pixmap,3,y,ubuffer,-1,NULL,0);
    }
}

static void DVPointsExpose(GWindow pixmap,DebugView *dv,GEvent *event) {
    GDrawSetLineWidth(pixmap,0);
    GDrawDrawLine(pixmap,event->u.expose.rect.x,dv->pts_head-1,event->u.expose.rect.x+event->u.expose.rect.width,dv->pts_head-1,0x000000);
}

static SplineSet *SplineSetsFromPoints(TT_GlyphZoneRec *pts, real scale) {
    int i=0, c, last_off, start;
    SplineSet *head=NULL, *last=NULL, *cur;
    SplinePoint *sp;
    /* very similar to parsettf.c: ttfbuildcontours */

    for ( c=0; c<pts->n_contours; ++c ) {
	if ( pts->contours[c]<i )	/* Sigh. Yes there are fonts with bad endpt info */
    continue;
	cur = chunkalloc(sizeof(SplineSet));
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
	last_off = false;
	start = i;
	while ( i<=pts->contours[c] ) {
	    if ( pts->tags[i]&FT_Curve_Tag_On ) {
		sp = chunkalloc(sizeof(SplinePoint));
		sp->me.x = sp->nextcp.x = sp->prevcp.x = pts->cur[i].x*scale;
		sp->me.y = sp->nextcp.y = sp->prevcp.y = pts->cur[i].y*scale;
		sp->nonextcp = sp->noprevcp = true;
		sp->ttfindex = i;
		if ( last_off && cur->last!=NULL ) {
		    cur->last->nextcp.x = sp->prevcp.x = pts->cur[i-1].x*scale;
		    cur->last->nextcp.y = sp->prevcp.y = pts->cur[i-1].y*scale;
		}
		last_off = false;
	    } else if ( last_off ) {
		sp = chunkalloc(sizeof(SplinePoint));
		sp->me.x = (pts->cur[i].x+pts->cur[i-1].x)/2 * scale;
		sp->me.y = (pts->cur[i].y+pts->cur[i-1].y)/2 * scale;
		sp->nextcp = sp->prevcp = sp->me;
		sp->nonextcp = true;
		sp->ttfindex = 0xffff;
		if ( last_off && cur->last!=NULL ) {
		    cur->last->nextcp.x = sp->prevcp.x = pts->cur[i-1].x * scale;
		    cur->last->nextcp.y = sp->prevcp.y = pts->cur[i-1].y * scale;
		}
		/* last_off continues to be true */
	    } else {
		last_off = true;
		sp = NULL;
	    }
	    if ( sp!=NULL ) {
		if ( cur->first==NULL )
		    cur->first = sp;
		else
		    SplineMake2(cur->last,sp);
		cur->last = sp;
	    }
	    ++i;
	}
	if ( start==i-1 ) {
	    /* MS chinese fonts have contours consisting of a single off curve*/
	    /*  point. What on earth do they think that means? */
	    sp = chunkalloc(sizeof(SplinePoint));
	    sp->me.x = pts->cur[start].x * scale;
	    sp->me.y = pts->cur[start].y * scale;
	    sp->nextcp = sp->prevcp = sp->me;
	    sp->nonextcp = sp->noprevcp = true;
	    sp->ttfindex = i-1;
	    cur->first = cur->last = sp;
	} else if ( !(pts->tags[start]&FT_Curve_Tag_On) && !(pts->tags[i-1]&FT_Curve_Tag_On) ) {
	    sp = chunkalloc(sizeof(SplinePoint));
	    sp->me.x = (pts->cur[start].x+pts->cur[i-1].x)/2 * scale;
	    sp->me.y = (pts->cur[start].y+pts->cur[i-1].y)/2 * scale;
	    sp->nextcp = sp->prevcp = sp->me;
	    sp->nonextcp = true;
	    sp->ttfindex = 0xffff;
	    cur->last->nextcp.x = sp->prevcp.x = pts->cur[i-1].x * scale;
	    cur->last->nextcp.y = sp->prevcp.y = pts->cur[i-1].y * scale;
	    SplineMake2(cur->last,sp);
	    cur->last = sp;
	    cur->last->nextcp.x = cur->first->prevcp.x = pts->cur[start].x * scale;
	    cur->last->nextcp.y = cur->first->prevcp.y = pts->cur[start].y * scale;
	} else if ( !(pts->tags[i-1]&FT_Curve_Tag_On)) {
	    cur->last->nextcp.x = cur->first->prevcp.x = pts->cur[i-1].x * scale;
	    cur->last->nextcp.y = cur->first->prevcp.y = pts->cur[i-1].y * scale;
	} else if ( !(pts->tags[start]&FT_Curve_Tag_On) ) {
	    cur->last->nextcp.x = cur->first->prevcp.x = pts->cur[start].x * scale;
	    cur->last->nextcp.y = cur->first->prevcp.y = pts->cur[start].y * scale;
	}
	if ( cur->last!=cur->first ) {
	    SplineMake2(cur->last,cur->first);
	    cur->last = cur->first;
	}
    }
return( head );
}

static void DVFigureNewState(DebugView *dv,TT_ExecContext exc) {
    int range = exc==NULL ? cr_none : exc->curRange;

    /* Code to look for proper function/idef rather than the full fpgm table */
    if ( exc==NULL ) {
	dv->id.instrs = NULL;
	dv->id.instr_cnt = 0;
	IIReinit(&dv->ii,-1);
    } else if ( dv->id.instrs!=(uint8 *) exc->code ) {
	dv->id.instrs =(uint8 *) exc->code;
	dv->id.instr_cnt = exc->codeSize;
	IIReinit(&dv->ii,exc->IP);
    } else
	IIScrollTo(&dv->ii,exc->IP,true);

    if ( dv->cv!=NULL && dv->cv->coderange!=range ) {
	dv->cv->coderange = range;
	CVInfoDraw(dv->cv,dv->cv->gw);
    }
    

    if ( exc!=NULL ) {
	SplinePointListsFree(dv->cv->gridfit);
	dv->cv->gridfit = SplineSetsFromPoints(&exc->pts,dv->scale);
	if ( exc->pts.n_points!=0 )
	    dv->cv->ft_gridfitwidth = exc->pts.cur[exc->pts.n_points-1].x * dv->scale;
	else
	    dv->cv->ft_gridfitwidth = 0;
	GDrawRequestExpose(dv->cv->v,NULL,false);
	if ( dv->regs!=NULL )
	    GDrawRequestExpose(dv->regs,NULL,false);
	if ( dv->stack!=NULL )
	    GDrawRequestExpose(dv->stack,NULL,false);
	if ( dv->storage!=NULL )
	    GDrawRequestExpose(dv->storage,NULL,false);
	if ( dv->cvt!=NULL )
	    GDrawRequestExpose(dv->cvt,NULL,false);
    }
}

static void DVGoFigure(DebugView *dv,enum debug_gotype go) {
    DebuggerGo(dv->dc,go);
    DVFigureNewState(dv,DebuggerGetEContext(dv->dc));
}

static int DV_Run(GGadget *g, GEvent *e) {
    DebugView *dv;

    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	dv = GDrawGetUserData(GGadgetGetWindow(g));
	DVGoFigure(dv,(enum debug_gotype) GGadgetGetCid(g));
    }
return( true );
}

static int DV_WatchPnt(GGadget *g, GEvent *e) {
    DebugView *dv;
    int pnum=0, n, any=0;
    SplineSet *ss;
    SplinePoint *sp;
    uint8 *watches;

    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	dv = GDrawGetUserData(GGadgetGetWindow(g));
	if ( dv->cv->sc->refs!=NULL ) {
	    GWidgetErrorR(_STR_NoWatchPoints,_STR_NoWatchPointsWithRefs);
return( true );
	}

	DebuggerGetWatches(dv->dc,&n);
	watches = gcalloc(n,sizeof(uint8));

	for ( ss = dv->cv->sc->layers[ly_fore].splines; ss!=NULL; ss=ss->next ) {
	    for ( sp=ss->first; ; ) {
		sp->watched = false;
		if ( sp->ttfindex == 0xffff )
		    /* Ignore it */;
		else if ( sp->selected && sp->ttfindex<n) {
		    watches[pnum=sp->ttfindex] = true;
		    any = true;
		    sp->watched = true;
		}
		if ( !sp->nonextcp ) {
		    ++pnum;
		    if ( sp==dv->cv->p.sp && dv->cv->p.nextcp && pnum<n )
			watches[pnum] = true;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
		if ( sp==ss->first )
	    break;
	    }
	}
	if ( !any ) {
	    free(watches);
	    watches = NULL;
	}
	DebuggerSetWatches(dv->dc,n,watches);
	GDrawRequestExpose(dv->cv->v,NULL,false);
	
    }
return( true );
}

#define MID_Registers	1001
#define MID_Stack	1002
#define MID_Storage	1003
#define MID_Points	1004
#define MID_Cvt		1005

static void DVCreateRegs(DebugView *dv);
static void DVCreateStack(DebugView *dv);
static void DVCreateStore(DebugView *dv);
static void DVCreatePoints(DebugView *dv);
static void DVCreateCvt(DebugView *dv);

static void DVMenuCreate(GWindow v, GMenuItem *mi,GEvent *e) {
    DebugView *dv = (DebugView *) GDrawGetUserData(v);

    if ( mi->mid==MID_Registers ) {
	if ( dv->regs!=NULL ) GDrawDestroyWindow(dv->regs);
	else DVCreateRegs(dv);
    } else if ( mi->mid==MID_Stack ) {
	if ( dv->stack!=NULL ) GDrawDestroyWindow(dv->stack);
	else DVCreateStack(dv);
    } else if ( mi->mid==MID_Storage ) {
	if ( dv->storage!=NULL ) GDrawDestroyWindow(dv->storage);
	else DVCreateStore(dv);
    } else if ( mi->mid==MID_Points ) {
	if ( dv->points!=NULL ) GDrawDestroyWindow(dv->points);
	else DVCreatePoints(dv);
    } else if ( mi->mid==MID_Cvt ) {
	if ( dv->cvt!=NULL ) GDrawDestroyWindow(dv->cvt);
	else DVCreateCvt(dv);
    }
}

static GMenuItem popupwindowlist[] = {
    { { (unichar_t *) _STR_Registers, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, DVMenuCreate, MID_Registers },
    { { (unichar_t *) _STR_Stack, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, DVMenuCreate, MID_Stack },
    { { (unichar_t *) _STR_Storage, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, DVMenuCreate, MID_Storage },
    { { (unichar_t *) _STR_Points, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, DVMenuCreate, MID_Points },
    { { (unichar_t *) _STR_Cvt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, DVMenuCreate, MID_Cvt },
    { NULL }
};

static int DV_WindowMenu(GGadget *g, GEvent *e) {
    DebugView *dv;
    GEvent fake;
    GRect pos;

    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonpress ) {
	dv = GDrawGetUserData(GGadgetGetWindow(g));
	popupwindowlist[0].ti.checked = dv->regs!=NULL;
	popupwindowlist[1].ti.checked = dv->stack!=NULL;
	popupwindowlist[2].ti.checked = dv->storage!=NULL;
	popupwindowlist[3].ti.checked = dv->points!=NULL;
	GGadgetGetSize(g,&pos);
	memset(&fake,0,sizeof(fake));
	fake.type = et_mousedown;
	fake.w = dv->dv;
	fake.u.mouse.x = pos.x;
	fake.u.mouse.y = pos.y+pos.height;
	GMenuCreatePopupMenu(dv->dv,&fake, popupwindowlist);
    }
return( true );
}

static int DV_Exit(GGadget *g, GEvent *e) {
    DebugView *dv;

    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	dv = GDrawGetUserData(GGadgetGetWindow(g));
	CVDebugFree(dv);
    }
return( true );
}

static void DVExpose(GWindow pixmap, DebugView *dv,GEvent *event) {
    GDrawSetLineWidth(pixmap,0);
    GDrawDrawLine(pixmap, 0, dv->toph-1, dv->dwidth, dv->toph-1, 0x000000 );
}

int DVChar(DebugView *dv, GEvent *event) {

    if ( event->u.chr.state&(ksm_control|ksm_meta) || dv==NULL )
return( false );
    /* Can't redo F1, handled by menu */
    if ( event->u.chr.keysym == GK_Help )
return( false );
    if ( event->u.chr.keysym >= GK_F1 && event->u.chr.keysym<=GK_F10 )
return( false );

    switch ( event->u.chr.keysym ) {
      case 's': case 'S':		/* Step */
	DVGoFigure(dv,dgt_step);
      break;
      case 'n': case 'N':		/* Next */
	DVGoFigure(dv,dgt_next);
      break;
      case 'f': case 'F':		/* finish */
	DVGoFigure(dv,dgt_stepout);
      break;
      case 'c': case 'C':		/* Continue */
	DVGoFigure(dv,dgt_continue);
      break;
      case 'k': case 'K': case 'q': case 'Q':	/* Kill (debugger) */
	CVDebugFree(dv);
      break;
      case 'r': case 'R':		/* run/restart (debugger) */
	CVDebugReInit(dv->cv,true,DebuggingFpgm(dv->dc));
      break;
    }
return( true );
}

static int DV_HandleChar(struct instrinfo *ii, GEvent *event) {
    DebugView *dv = (ii->userdata);
return( DVChar(dv,event));
}

static int dvreg_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);

    switch ( event->type ) {
      case et_expose:
	DVRegExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
      case et_close:
	GDrawDestroyWindow(dv->regs);
      break;
      case et_destroy:
	dv->regs = NULL;
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static int dvstack_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);

    switch ( event->type ) {
      case et_expose:
	DVStackExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
#if 0
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_scrollbarchange:
	    stack_scroll(dv,&event->u.control.u.sb);
	  break;
	}
      break;
#endif
      case et_close:
	GDrawDestroyWindow(dv->stack);
      break;
      case et_destroy:
	dv->stack = NULL;
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static int dvstore_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);

    switch ( event->type ) {
      case et_expose:
	DVStorageExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
#if 0
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_scrollbarchange:
	    stack_scroll(dv,&event->u.control.u.sb);
	  break;
	}
      break;
#endif
      case et_close:
	GDrawDestroyWindow(dv->storage);
      break;
      case et_destroy:
	dv->storage = NULL;
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static int dvpointsv_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);

    switch ( event->type ) {
      case et_expose:
	DVPointsVExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static int dvpoints_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);
    GRect r;

    switch ( event->type ) {
      case et_expose:
	DVPointsExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_radiochanged:
	    GDrawRequestExpose(dv->points_v,NULL,false);
	  break;
	}
      break;
      case et_resize:
	GDrawGetSize(gw,&r);
	GDrawResize(dv->points_v,r.width,r.height-dv->pts_head);
      break;
      case et_close:
	GDrawDestroyWindow(dv->points);
      break;
      case et_destroy:
	dv->points = NULL;
	dv->points_v = NULL;
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static void DVCvt_SetScrollBar(DebugView *dv) {
    GRect size;
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);
    int lh = exc==NULL ? 0 : exc->cvtSize;

    GDrawGetSize(dv->cvt,&size);
    GScrollBarSetBounds(dv->cvtsb,0,lh,size.height/dv->ii.fh);
    if ( dv->cvt_offtop + size.height/dv->ii.fh > lh ) {
	int lpos = lh-size.height/dv->ii.fh;
	if ( lpos<0 ) lpos = 0;
	dv->cvt_offtop = lpos;
    }
    GScrollBarSetPos(dv->cvtsb,dv->cvt_offtop);
}


static void dvcvt_scroll(DebugView *dv,struct sbevent *sb) {
    int newpos = dv->cvt_offtop;
    GRect size;
    TT_ExecContext exc = DebuggerGetEContext(dv->dc);

    GDrawGetSize(dv->cvt,&size);
    switch( sb->type ) {
      case et_sb_top:
        newpos = 0;
      break;
      case et_sb_uppage:
        newpos -= size.height/dv->ii.fh;
      break;
      case et_sb_up:
        --newpos;
      break;
      case et_sb_down:
        ++newpos;
      break;
      case et_sb_downpage:
        newpos += size.height/dv->ii.fh;
      break;
      case et_sb_bottom:
        newpos = exc->cvtSize-size.height/dv->ii.fh;
      break;
      case et_sb_thumb:
      case et_sb_thumbrelease:
        newpos = sb->pos;
      break;
    }
    if ( newpos>exc->cvtSize-size.height/dv->ii.fh )
        newpos = exc->cvtSize-size.height/dv->ii.fh;
    if ( newpos<0 ) newpos =0;
    if ( newpos!=dv->cvt_offtop ) {
	int diff = newpos-dv->cvt_offtop;
	dv->cvt_offtop = newpos;
	GScrollBarSetPos(dv->cvtsb,dv->cvt_offtop);
	GDrawScroll(dv->cvt,NULL,0,diff*dv->ii.fh);
    }
}

static int dvcvt_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);
    GRect r,g;

    switch ( event->type ) {
      case et_expose:
	DVCvtExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_scrollbarchange:
	    dvcvt_scroll(dv,&event->u.control.u.sb);
	  break;
	}
      break;
      case et_resize:
	GDrawGetSize(gw,&r);
	GGadgetGetSize(dv->cvtsb,&g);
	GGadgetMove(dv->cvtsb,r.width-g.width,0);
	GGadgetResize(dv->cvtsb,g.width,r.height);
	DVCvt_SetScrollBar(dv);
	GDrawRequestExpose(dv->cvt,NULL,false);
      break;
      case et_close:
	GDrawDestroyWindow(dv->cvt);
      break;
      case et_destroy:
	dv->cvt = NULL;
      break;
      case et_mouseup: case et_mousedown:
      case et_mousemove:
	GGadgetEndPopup();
      break;
    }
return( true );
}

static void DVCreateRegs(DebugView *dv) {
    GWindowAttrs wattrs;
    GRect pos;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    wattrs.event_masks = -1;
    wattrs.cursor = ct_mypointer;
    wattrs.window_title = GStringGetResource(_STR_TTRegisters,NULL);
    pos.x = 664; pos.y = 1;
    pos.width = 133; pos.height = 269;
    dv->regs = GDrawCreateTopWindow(NULL,&pos,dvreg_e_h,dv,&wattrs);
    GDrawSetVisible(dv->regs,true);
}

static void DVCreateStack(DebugView *dv) {
    GWindowAttrs wattrs;
    GRect pos;
    /*extern int _GScrollBar_Width;*/

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    wattrs.event_masks = -1;
    wattrs.cursor = ct_mypointer;
    wattrs.window_title = GStringGetResource(_STR_TTStack,NULL);
    pos.x = 664; pos.y = 302;
    pos.width = 133; pos.height = 269;
    dv->stack = GDrawCreateTopWindow(NULL,&pos,dvstack_e_h,dv,&wattrs);
    GDrawSetVisible(dv->stack,true);
}

static void DVCreateStore(DebugView *dv) {
    GWindowAttrs wattrs;
    GRect pos;
    /*extern int _GScrollBar_Width;*/

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    wattrs.event_masks = -1;
    wattrs.cursor = ct_mypointer;
    wattrs.window_title = GStringGetResource(_STR_TTStorage,NULL);
    pos.x = 664; pos.y = 602;
    pos.width = 133; pos.height = 100;
    dv->storage = GDrawCreateTopWindow(NULL,&pos,dvstore_e_h,dv,&wattrs);
    GDrawSetVisible(dv->storage,true);
}

static void DVCreatePoints(DebugView *dv) {
    GWindowAttrs wattrs;
    GRect pos;
    /*extern int _GScrollBar_Width;*/
    GGadgetCreateData gcd[8];
    GTextInfo label[8];

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    wattrs.event_masks = -1;
    wattrs.cursor = ct_mypointer;
    wattrs.window_title = GStringGetResource(_STR_TTPoints,NULL);
    pos.x = 664; pos.y = 732;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,125)); pos.height = 269;
    dv->points = GDrawCreateTopWindow(NULL,&pos,dvpoints_e_h,dv,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    label[0].text = (unichar_t *) _STR_Twilight;
    label[0].text_in_resource = true;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.pos.x = 5; gcd[0].gd.pos.y = 3;
    gcd[0].gd.flags = gg_visible | gg_enabled | (show_twilight ? gg_cb_on : 0 );
    gcd[0].gd.cid = CID_Twilight;
    gcd[0].creator = GRadioCreate;

    label[1].text = (unichar_t *) _STR_Normal;
    label[1].text_in_resource = true;
    gcd[1].gd.label = &label[1];
    gcd[1].gd.pos.x = 60; gcd[1].gd.pos.y = gcd[0].gd.pos.y;
    gcd[1].gd.flags = gg_visible | gg_enabled | (!show_twilight ? gg_cb_on : 0 );
    gcd[1].gd.cid = CID_Normal;
    gcd[1].creator = GRadioCreate;

    label[2].text = (unichar_t *) _STR_Current;
    label[2].text_in_resource = true;
    gcd[2].gd.label = &label[2];
    gcd[2].gd.pos.x = 5; gcd[2].gd.pos.y = gcd[0].gd.pos.y+16;
    gcd[2].gd.flags = gg_visible | gg_enabled | gg_rad_startnew | (show_current ? gg_cb_on : 0 );
    gcd[2].gd.cid = CID_Current;
    gcd[2].creator = GRadioCreate;

    label[3].text = (unichar_t *) _STR_Original;
    label[3].text_in_resource = true;
    gcd[3].gd.label = &label[3];
    gcd[3].gd.pos.x = gcd[1].gd.pos.x; gcd[3].gd.pos.y = gcd[2].gd.pos.y;
    gcd[3].gd.flags = gg_visible | gg_enabled | (!show_current ? gg_cb_on : 0 );
    gcd[3].gd.cid = CID_Original;
    gcd[3].creator = GRadioCreate;

    label[4].text = (unichar_t *) _STR_GridUnit;
    label[4].text_in_resource = true;
    gcd[4].gd.label = &label[4];
    gcd[4].gd.pos.x = 5; gcd[4].gd.pos.y = gcd[2].gd.pos.y+16;
    gcd[4].gd.flags = gg_visible | gg_enabled | gg_rad_startnew | (show_current ? gg_cb_on : 0 );
    gcd[4].gd.cid = CID_Grid;
    gcd[4].creator = GRadioCreate;

    label[5].text = (unichar_t *) _STR_EmUnit;
    label[5].text_in_resource = true;
    gcd[5].gd.label = &label[5];
    gcd[5].gd.pos.x = gcd[1].gd.pos.x; gcd[5].gd.pos.y = gcd[4].gd.pos.y;
    gcd[5].gd.flags = gg_visible | gg_enabled | (!show_current ? gg_cb_on : 0 );
    gcd[5].gd.cid = CID_EmUnit;
    gcd[5].creator = GRadioCreate;

    GGadgetsCreate(dv->points,gcd);

    dv->pts_head = GDrawPointsToPixels(NULL,GGadgetScale(5+3*16+3));
    pos.x = 0;
    pos.y = dv->pts_head;
    pos.height -= dv->pts_head;
    dv->points_v = GWidgetCreateSubWindow(dv->points,&pos,dvpointsv_e_h,dv,&wattrs);
    GDrawSetVisible(dv->points_v,true);
    GDrawSetVisible(dv->points,true);
}

static void DVCreateCvt(DebugView *dv) {
    GWindowAttrs wattrs;
    GRect pos;
    /*extern int _GScrollBar_Width;*/
    GGadgetData gd;
    extern int _GScrollBar_Width;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    wattrs.event_masks = -1;
    wattrs.cursor = ct_mypointer;
    wattrs.window_title = GStringGetResource(_STR_Cvt,NULL);
    pos.x = 664; pos.y = 732;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,125)); pos.height = 169;
    dv->cvt = GDrawCreateTopWindow(NULL,&pos,dvcvt_e_h,dv,&wattrs);

    memset(&gd,0,sizeof(gd));

    gd.pos.y = 0; gd.pos.height = pos.height;
    gd.pos.width = GDrawPointsToPixels(dv->cvt,_GScrollBar_Width);
    gd.pos.x = pos.width-gd.pos.width;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_sb_vert;
    dv->cvtsb = GScrollBarCreate(dv->cvt,&gd,dv);

    DVCvt_SetScrollBar(dv);

    GDrawSetVisible(dv->cvt,true);
}

static int dv_e_h(GWindow gw, GEvent *event) {
    DebugView *dv = (DebugView *) GDrawGetUserData(gw);

    switch ( event->type ) {
      case et_expose:
	DVExpose(gw,dv,event);
      break;
      case et_char:
return( DVChar(dv,event));
      break;
      case et_charup:
      break;
      case et_controlevent:
	switch ( event->u.control.subtype ) {
	  case et_scrollbarchange:
	    instr_scroll(&dv->ii,&event->u.control.u.sb);
	  break;
	}
      break;
      case et_resize:
	if ( event->u.resize.sized )
	    /*DVResize(dv,event)*/;
      break;
      case et_destroy:
	dv->dv = NULL;
	free(dv->id.bts);
      break;
      case et_mouseup: case et_mousedown:
	GGadgetEndPopup();
      break;
      case et_mousemove:
      break;
    }
return( true );
}

void CVDebugFree(DebugView *dv) {
    if ( dv!=NULL ) {
	CharView *cv = dv->cv;
	SplineSet *ss;
	SplinePoint *sp;

	cv->show_ft_results = false;
	DebuggerTerminate(dv->dc);
	cv->dv = NULL;
	if ( dv->points!=NULL )
	    GDrawDestroyWindow(dv->points);
	if ( dv->cvt!=NULL )
	    GDrawDestroyWindow(dv->cvt);
	if ( dv->regs!=NULL )
	    GDrawDestroyWindow(dv->regs);
	if ( dv->stack!=NULL )
	    GDrawDestroyWindow(dv->stack);
	if ( dv->storage!=NULL )
	    GDrawDestroyWindow(dv->storage);
	if ( dv->cvt!=NULL )
	    GDrawDestroyWindow(dv->cvt);
	if ( dv->dv!=NULL ) {
	    GDrawDestroyWindow(dv->dv);
	    CVResize(cv);
	    GDrawRequestExpose(cv->v,NULL,false);
	}
	free(dv);

	for ( ss = dv->cv->sc->layers[ly_fore].splines; ss!=NULL; ss=ss->next ) {
	    for ( sp=ss->first; ; ) {
		sp->watched = false;
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
		if ( sp==ss->first )
	    break;
	    }
	}
	GDrawRequestExpose(cv->v,NULL,false);

	if ( cv->coderange!=cr_none ) {
	    cv->coderange = cr_none;
	    CVInfoDraw(cv,cv->gw);
	}
    }
}

void CVDebugReInit(CharView *cv,int restart_debug,int dbg_fpgm) {
    DebugView *dv = cv->dv;
    GWindowAttrs wattrs;
    GRect pos;
    TT_ExecContext exc;
    FontRequest rq;
    static unichar_t monospace[] = { 'c','o','u','r','i','e','r',',','m', 'o', 'n', 'o', 's', 'p', 'a', 'c', 'e',',','c','a','s','l','o','n',',','u','n','i','f','o','n','t', '\0' };
    int as,ds,ld;
    GGadgetCreateData gcd[9];
    GTextInfo label[9];
    extern int _GScrollBar_Width;
    double scale;

    if ( restart_debug )
	scale = (cv->sc->parent->ascent+cv->sc->parent->descent)/(cv->ft_pointsize*cv->ft_dpi/72.0) / (1<<6);
    if ( !restart_debug ) {
	CVDebugFree(dv);
    } else if ( dv==NULL ) {
	int sbsize = GDrawPointsToPixels(cv->gw,_GScrollBar_Width);
	cv->show_ft_results = false;
	cv->dv = dv = gcalloc(1,sizeof(DebugView));
	dv->dwidth = 260;
	dv->scale = scale;
	dv->cv = cv;
	dv->dc = DebuggerCreate(cv->sc,cv->ft_pointsize,cv->ft_dpi,dbg_fpgm);
	if ( dv->dc==NULL ) {
	    free(dv);
	    cv->dv = NULL;
return;
	}
	memset(&wattrs,0,sizeof(wattrs));
	wattrs.mask = wam_events|wam_cursor|wam_bordwidth|wam_bordcol;
	wattrs.event_masks = -1;
	wattrs.border_width = 0;
	wattrs.border_color = 0x000000;
	wattrs.cursor = ct_mypointer;
	pos.x = 0; pos.y = 0;
	pos.width = dv->dwidth; pos.height = cv->height;
	dv->dv = GWidgetCreateSubWindow(cv->gw,&pos,dv_e_h,dv,&wattrs);
	GDrawSetVisible(dv->dv,true);

	dv->toph = 36;
	memset(&gcd,0,sizeof(gcd));
	memset(&label,0,sizeof(label));
	gcd[0].gd.pos.y = dv->toph; gcd[0].gd.pos.height = pos.height-dv->toph;
	gcd[0].gd.pos.width = GDrawPointsToPixels(cv->gw,_GScrollBar_Width);
	gcd[0].gd.pos.x = pos.width-gcd[0].gd.pos.width;
	gcd[0].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_sb_vert;
	gcd[0].creator = GScrollBarCreate;

	gcd[1].gd.pos.y = 2; gcd[1].gd.pos.x = 2;
	gcd[1].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	gcd[1].gd.cid = dgt_step;
	gcd[1].gd.label = &label[1];
	label[1].image = &GIcon_stepinto;
	gcd[1].gd.handle_controlevent = DV_Run;
	gcd[1].gd.popup_msg = GStringGetResource(_STR_StepPopup,NULL);
	gcd[1].creator = GButtonCreate;

	gcd[2].gd.pos.y = 2; gcd[2].gd.pos.x = 38;
	gcd[2].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	gcd[2].gd.cid = dgt_next;
	gcd[2].gd.label = &label[2];
	label[2].image = &GIcon_stepover;
	gcd[2].gd.handle_controlevent = DV_Run;
	gcd[2].gd.popup_msg = GStringGetResource(_STR_NextPopup,NULL);
	gcd[2].creator = GButtonCreate;

	gcd[3].gd.pos.y = 2; gcd[3].gd.pos.x = 74;
	gcd[3].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	gcd[3].gd.cid = dgt_stepout;
	gcd[3].gd.label = &label[3];
	label[3].image = &GIcon_stepout;
	gcd[3].gd.handle_controlevent = DV_Run;
	gcd[3].gd.popup_msg = GStringGetResource(_STR_StepOutOfPopup,NULL);
	gcd[3].creator = GButtonCreate;

	gcd[4].gd.pos.y = 2; gcd[4].gd.pos.x = 110;
	gcd[4].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	gcd[4].gd.cid = dgt_continue;
	gcd[4].gd.label = &label[4];
	label[4].image = &GIcon_continue;
	gcd[4].gd.handle_controlevent = DV_Run;
	gcd[4].gd.popup_msg = GStringGetResource(_STR_ContinuePopup,NULL);
	gcd[4].creator = GButtonCreate;

	gcd[5].gd.pos.y = 2; gcd[5].gd.pos.x = 146;
	gcd[5].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	/*gcd[5].gd.cid = dgt_continue;*/
	gcd[5].gd.label = &label[5];
	label[5].image = &GIcon_watchpnt;
	gcd[5].gd.handle_controlevent = DV_WatchPnt;
	gcd[5].gd.popup_msg = GStringGetResource(_STR_WatchPointPopup,NULL);
	gcd[5].creator = GButtonCreate;

	gcd[6].gd.pos.y = 2; gcd[6].gd.pos.x = 182;
	gcd[6].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	/*gcd[6].gd.cid = dgt_continue;*/
	gcd[6].gd.label = &label[6];
	label[6].image = &GIcon_menudelta;
	gcd[6].gd.handle_controlevent = DV_WindowMenu;
	gcd[6].gd.popup_msg = GStringGetResource(_STR_Window,NULL);
	gcd[6].creator = GButtonCreate;

	gcd[7].gd.pos.y = 2; gcd[7].gd.pos.x = 218;
	gcd[7].gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
	/*gcd[7].gd.cid = dgt_continue;*/
	gcd[7].gd.label = &label[7];
	label[7].image = &GIcon_exit;
	gcd[7].gd.handle_controlevent = DV_Exit;
	gcd[7].gd.popup_msg = GStringGetResource(_STR_ExitDebugger,NULL);
	gcd[7].creator = GButtonCreate;

	GGadgetsCreate(dv->dv,gcd);

	dv->ii.vsb = gcd[0].ret;
	dv->ii.sbw = gcd[0].gd.pos.width;
	dv->ii.vheight = gcd[0].gd.pos.height; dv->ii.vwidth = pos.width-sbsize;
	dv->ii.showaddr = true;
	dv->ii.userdata = dv;
	dv->ii.selection_callback = DVToggleBp;
	dv->ii.bpcheck = DVBpCheck;
	dv->ii.handle_char = DV_HandleChar;

	pos.y = dv->toph;
	pos.width -= sbsize; pos.height -= pos.y;
	dv->ii.v = GWidgetCreateSubWindow(dv->dv,&pos,ii_v_e_h,&dv->ii,&wattrs);
	dv->ii.instrdata = &dv->id;

	memset(&rq,0,sizeof(rq));
	rq.family_name = monospace;
	rq.point_size = -12;
	rq.weight = 400;
	dv->ii.gfont = GDrawInstanciateFont(GDrawGetDisplayOfWindow(cv->gw),&rq);
	GDrawSetFont(dv->ii.v,dv->ii.gfont);
	GDrawFontMetrics(dv->ii.gfont,&as,&ds,&ld);
	dv->ii.as = as+1;
	dv->ii.fh = dv->ii.as+ds;
	dv->ii.isel_pos = -1;

	if (( exc = DebuggerGetEContext(dv->dc))!=NULL )
	    DVFigureNewState(dv,exc);
	GDrawSetVisible(dv->ii.v,true);
	GDrawSetVisible(dv->dv,true);
	CVResize(cv);
	GDrawRequestExpose(cv->v,NULL,false);
	DVCreateRegs(dv);
	DVCreateStack(dv);
    } else {
	dv->scale = scale;
	DebuggerReset(dv->dc,cv->ft_pointsize,cv->ft_dpi,dbg_fpgm);
	if (( exc = DebuggerGetEContext(dv->dc))!=NULL )
	    DVFigureNewState(dv,exc);
    }
}
#endif
