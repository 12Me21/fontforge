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
#include <gwidget.h>
#include <ustring.h>
#include <math.h>
#include <gkeysym.h>

struct problems {
    FontView *fv;
    CharView *cv;
    SplineChar *sc;
    SplineChar *msc;
    unsigned int openpaths: 1;
    unsigned int pointstooclose: 1;
    /*unsigned int missingextrema: 1;*/
    unsigned int xnearval: 1;
    unsigned int ynearval: 1;
    unsigned int ynearstd: 1;		/* baseline, xheight, cap, ascent, descent, etc. */
    unsigned int linenearstd: 1;	/* horizontal, vertical, italicangle */
    unsigned int cpnearstd: 1;		/* control points near: horizontal, vertical, italicangle */
    unsigned int cpodd: 1;		/* control points beyond points on spline */
    unsigned int hintwithnopt: 1;
    unsigned int ptnearhint: 1;
    unsigned int hintwidthnearval: 1;
    unsigned int direction: 1;
    unsigned int flippedrefs: 1;
    unsigned int cidmultiple: 1;
    unsigned int cidblank: 1;
    unsigned int bitmaps: 1;
    unsigned int advancewidth: 1;
    unsigned int vadvancewidth: 1;
    unsigned int stem3: 1;
    unsigned int showexactstem3: 1;
    unsigned int irrelevantcontrolpoints: 1;
    unsigned int badsubs: 1;
    unsigned int missingglyph: 1;
    unsigned int missinglookuptag: 1;
    unsigned int toomanypoints: 1;
    unsigned int explain: 1;
    unsigned int done: 1;
    unsigned int doneexplain: 1;
    unsigned int finish: 1;
    unsigned int ignorethis: 1;
    real near, xval, yval, widthval;
    int explaining;
    real found, expected;
    real xheight, caph, ascent, descent;
    real irrelevantfactor;
    int advancewidthval, vadvancewidthval;
    int pointsmax;
    GWindow explainw;
    GGadget *explaintext, *explainvals, *ignoregadg;
    SplineChar *lastcharopened;
    CharView *cvopened;
    char *badsubsname;
    uint32 badsubstag;
    int rpl_cnt, rpl_max;
    struct mgrpl {
	char *search;
	char *rpl;		/* a rpl of "" means delete (NULL means not found) */
    } *mg;
    struct mlrpl {
	uint32 search;
	uint32 rpl;
    } *mlt;
};

static int openpaths=1, pointstooclose=1/*, missing=0*/, doxnear=0, doynear=0;
static int doynearstd=1, linestd=1, cpstd=1, cpodd=1, hintnopt=0, ptnearhint=0;
static int hintwidth=0, direction=0, flippedrefs=1, bitmaps=0;
static int cidblank=0, cidmultiple=1, advancewidth=0, vadvancewidth=0;
static int irrelevantcp=1, missingglyph=0, missinglookuptag=0;
static int badsubs=1, toomanypoints=1, pointsmax = 1500;
static int stem3=0, showexactstem3=0;
static real near=3, xval=0, yval=0, widthval=50, advancewidthval=0, vadvancewidthval=0;
static real irrelevantfactor = .005;

#define CID_Stop		2001
#define CID_Next		2002
#define CID_Fix			2003
#define CID_ClearAll		2004
#define CID_SetAll		2005

#define CID_OpenPaths		1001
#define CID_PointsTooClose	1002
/*#define CID_MissingExtrema	1003*/
#define CID_XNear		1004
#define CID_YNear		1005
#define CID_YNearStd		1006
#define CID_HintNoPt		1007
#define CID_PtNearHint		1008
#define CID_HintWidthNear	1009
#define CID_HintWidth		1010
#define CID_Near		1011
#define CID_XNearVal		1012
#define CID_YNearVal		1013
#define CID_LineStd		1014
#define CID_Direction		1015
#define CID_CpStd		1016
#define CID_CpOdd		1017
#define CID_CIDMultiple		1018
#define CID_CIDBlank		1019
#define CID_FlippedRefs		1020
#define CID_Bitmaps		1021
#define CID_AdvanceWidth	1022
#define CID_AdvanceWidthVal	1023
#define CID_VAdvanceWidth	1024
#define CID_VAdvanceWidthVal	1025
#define CID_Stem3		1026
#define CID_ShowExactStem3	1027
#define CID_IrrelevantCP	1028
#define CID_IrrelevantFactor	1029
#define CID_BadSubs		1030
#define CID_MissingGlyph	1031
#define CID_MissingLookupTag	1032
#define CID_TooManyPoints	1033
#define CID_PointsMax		1034


static void FixIt(struct problems *p) {
    SplinePointList *spl;
    SplinePoint *sp;
    /*StemInfo *h;*/
    RefChar *r;
    int changed;

#if 0	/* The ultimate cause (the thing we need to fix) for these two errors */
	/* is that the stem is wrong, it's too hard to fix that here, so best */
	/* not to attempt to fix the proximal cause */
    if ( p->explaining==_STR_ProbHintHWidth ) {
	for ( h=p->sc->hstem; h!=NULL && !h->active; h=h->next );
	if ( h!=NULL ) {
	    h->width = p->widthval;
	    SCOutOfDateBackground(p->sc);
	    SCUpdateAll(p->sc);
	} else
	    GDrawIError("Could not find hint");
return;
    }
    if ( p->explaining==_STR_ProbHintVWidth ) {
	for ( h=p->sc->vstem; h!=NULL && !h->active; h=h->next );
	if ( h!=NULL ) {
	    h->width = p->widthval;
	    SCOutOfDateBackground(p->sc);
	    SCUpdateAll(p->sc);
	} else
	    GDrawIError("Could not find hint");
return;
    }
#endif
    if ( p->explaining==_STR_ProbFlippedRef ) {
	for ( r=p->sc->refs; r!=NULL && !r->selected; r = r->next );
	if ( r!=NULL ) {
	    SCPreserveState(p->sc,false);
	    SCRefToSplines(p->sc,r);
	    changed = false;
	    p->sc->splines = SplineSetsCorrect(p->sc->splines,&changed);
	    if ( changed )
		SCCharChangedUpdate(p->sc);
	} else
	    GDrawIError("Could not find referenc");
return;
    } else if ( p->explaining==_STR_ProbBadWidth ) {
	SCSynchronizeWidth(p->sc,p->advancewidthval,p->sc->width,NULL);
return;
    } else if ( p->explaining==_STR_ProbBadVWidth ) {
	p->sc->vwidth=p->vadvancewidthval;
return;
    }

    for ( spl=p->sc->splines; spl!=NULL; spl=spl->next ) {
	for ( sp = spl->first; ; ) {
	    if ( sp->selected )
	break;
	    if ( sp->next==NULL )
	break;
	    sp = sp->next->to;
	    if ( sp==spl->first )
	break;
	}
	if ( sp->selected )
    break;
    }
    if ( sp==NULL ) {
	GDrawIError("Nothing selected");
return;
    }

/* I do not handle:
    _STR_ProbOpenPath
    _STR_ProbPointsTooClose
    _STR_ProbLineItal, _STR_ProbAboveItal, _STR_ProbBelowItal, _STR_ProbRightItal, _STR_ProbLeftItal
    _STR_ProbAboveOdd, _STR_ProbBelowOdd, _STR_ProbRightOdd, _STR_ProbLeftOdd
    _STR_ProbHintControl
    _STR_ProbHint3*
*/

    SCPreserveState(p->sc,false);
    if ( p->explaining==_STR_ProbXNear || p->explaining==_STR_ProbPtNearVHint) {
	sp->prevcp.x += p->expected-sp->me.x;
	sp->nextcp.x += p->expected-sp->me.x;
	sp->me.x = p->expected;
    } else if ( p->explaining==_STR_ProbYNear || p->explaining==_STR_ProbPtNearHHint ||
	    p->explaining==_STR_ProbYBase || p->explaining==_STR_ProbYXHeight ||
	    p->explaining==_STR_ProbYCapHeight || p->explaining==_STR_ProbYAs ||
	    p->explaining==_STR_ProbYDs ) {
	sp->prevcp.y += p->expected-sp->me.y;
	sp->nextcp.y += p->expected-sp->me.y;
	sp->me.y = p->expected;
    } else if ( p->explaining==_STR_ProbLineHor ) {
	if ( sp->me.y!=p->found ) {
	    sp=sp->next->to;
	    if ( !sp->selected || sp->me.y!=p->found ) {
		GDrawIError("Couldn't find line");
return;
	    }
	}
	sp->prevcp.y += p->expected-sp->me.y;
	sp->nextcp.y += p->expected-sp->me.y;
	sp->me.y = p->expected;
    } else if ( p->explaining==_STR_ProbAboveHor || p->explaining==_STR_ProbBelowHor ||
	    p->explaining==_STR_ProbRightHor || p->explaining==_STR_ProbLeftHor ) {
	BasePoint *tofix, *other;
	if ( sp->nextcp.y==p->found ) {
	    tofix = &sp->nextcp;
	    other = &sp->prevcp;
	} else {
	    tofix = &sp->prevcp;
	    other = &sp->nextcp;
	}
	if ( tofix->y!=p->found ) {
	    GDrawIError("Couldn't find control point");
return;
	}
	tofix->y = p->expected;
	if ( sp->pointtype==pt_curve )
	    other->y = p->expected;
	else
	    sp->pointtype = pt_corner;
    } else if ( p->explaining==_STR_ProbLineVert ) {
	if ( sp->me.x!=p->found ) {
	    sp=sp->next->to;
	    if ( !sp->selected || sp->me.x!=p->found ) {
		GDrawIError("Couldn't find line");
return;
	    }
	}
	sp->prevcp.x += p->expected-sp->me.x;
	sp->nextcp.x += p->expected-sp->me.x;
	sp->me.x = p->expected;
    } else if ( p->explaining==_STR_ProbAboveVert || p->explaining==_STR_ProbBelowVert ||
	    p->explaining==_STR_ProbRightVert || p->explaining==_STR_ProbLeftVert ) {
	BasePoint *tofix, *other;
	if ( sp->nextcp.x==p->found ) {
	    tofix = &sp->nextcp;
	    other = &sp->prevcp;
	} else {
	    tofix = &sp->prevcp;
	    other = &sp->nextcp;
	}
	if ( tofix->x!=p->found ) {
	    GDrawIError("Couldn't find control point");
return;
	}
	tofix->x = p->expected;
	if ( sp->pointtype==pt_curve )
	    other->x = p->expected;
	else
	    sp->pointtype = pt_corner;
    } else if ( p->explaining==_STR_ProbExpectedCounter || p->explaining==_STR_ProbExpectedClockwise ) {
	SplineSetReverse(spl);
    } else if ( p->explaining==_STR_ProbIrrelCP ) {
	if ( sp->next!=NULL ) {
	    double len = sqrt((sp->me.x-sp->next->to->me.x)*(sp->me.x-sp->next->to->me.x) +
		    (sp->me.y-sp->next->to->me.y)*(sp->me.y-sp->next->to->me.y));
	    double cplen = sqrt((sp->me.x-sp->nextcp.x)*(sp->me.x-sp->nextcp.x) +
		    (sp->me.y-sp->nextcp.y)*(sp->me.y-sp->nextcp.y));
	    if ( cplen!=0 && cplen<p->irrelevantfactor*len ) {
		sp->nextcp = sp->me;
		sp->nonextcp = true;
	    }
	}
	if ( sp->prev!=NULL ) {
	    double len = sqrt((sp->me.x-sp->prev->from->me.x)*(sp->me.x-sp->prev->from->me.x) +
		    (sp->me.y-sp->prev->from->me.y)*(sp->me.y-sp->prev->from->me.y));
	    double cplen = sqrt((sp->me.x-sp->prevcp.x)*(sp->me.x-sp->prevcp.x) +
		    (sp->me.y-sp->prevcp.y)*(sp->me.y-sp->prevcp.y));
	    if ( cplen!=0 && cplen<p->irrelevantfactor*len ) {
		sp->prevcp = sp->me;
		sp->noprevcp = true;
	    }
	}
    } else
	GDrawIError("Did not fix: %d", p->explaining );
    if ( sp->next!=NULL )
	SplineRefigure(sp->next);
    if ( sp->prev!=NULL )
	SplineRefigure(sp->prev);
    SCCharChangedUpdate(p->sc);
}

static int explain_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	struct problems *p = GDrawGetUserData(gw);
	p->doneexplain = true;
    } else if ( event->type==et_controlevent &&
	    event->u.control.subtype == et_buttonactivate ) {
	struct problems *p = GDrawGetUserData(gw);
	if ( GGadgetGetCid(event->u.control.g)==CID_Stop )
	    p->finish = true;
	else if ( GGadgetGetCid(event->u.control.g)==CID_Fix )
	    FixIt(p);
	p->doneexplain = true;
    } else if ( event->type==et_controlevent &&
	    event->u.control.subtype == et_radiochanged ) {
	struct problems *p = GDrawGetUserData(gw);
	p->ignorethis = GGadgetIsChecked(event->u.control.g);
    } else if ( event->type==et_char ) {
	if ( event->u.chr.keysym == GK_F1 || event->u.chr.keysym == GK_Help ) {
	    help("problems.html");
return( true );
	}
return( false );
    }
return( true );
}

static void ExplainIt(struct problems *p, SplineChar *sc, int explain,
	real found, real expected ) {
    GRect pos;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[9];
    GTextInfo label[9];
    unichar_t ubuf[100]; char buf[20];
    SplinePointList *spl; Spline *spline, *first;
    int fixable;

    if ( !p->explain || p->finish )
return;
    if ( p->explainw==NULL ) {
	memset(&wattrs,0,sizeof(wattrs));
	wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor;
	wattrs.event_masks = ~(1<<et_charup);
	wattrs.undercursor = 1;
	wattrs.cursor = ct_pointer;
	wattrs.window_title = GStringGetResource(_STR_ProbExplain,NULL);
	pos.x = pos.y = 0;
	pos.width = GGadgetScale(GDrawPointsToPixels(NULL,400));
	pos.height = GDrawPointsToPixels(NULL,86);
	p->explainw = GDrawCreateTopWindow(NULL,&pos,explain_e_h,p,&wattrs);

	memset(&label,0,sizeof(label));
	memset(&gcd,0,sizeof(gcd));

	label[0].text = (unichar_t *) explain;
	label[0].text_in_resource = true;
	gcd[0].gd.label = &label[0];
	gcd[0].gd.pos.x = 6; gcd[0].gd.pos.y = 6; gcd[0].gd.pos.width = 400-12;
	gcd[0].gd.flags = gg_visible | gg_enabled;
	gcd[0].creator = GLabelCreate;

	label[4].text = (unichar_t *) "";
	label[4].text_is_1byte = true;
	gcd[4].gd.label = &label[4];
	gcd[4].gd.pos.x = 6; gcd[4].gd.pos.y = gcd[0].gd.pos.y+12; gcd[4].gd.pos.width = 400-12;
	gcd[4].gd.flags = gg_visible | gg_enabled;
	gcd[4].creator = GLabelCreate;

	label[5].text = (unichar_t *) _STR_IgnoreProblemFuture;
	label[5].text_in_resource = true;
	gcd[5].gd.label = &label[5];
	gcd[5].gd.pos.x = 6; gcd[5].gd.pos.y = gcd[4].gd.pos.y+12;
	gcd[5].gd.flags = gg_visible | gg_enabled;
	gcd[5].creator = GCheckBoxCreate;

	gcd[1].gd.pos.x = 15-3; gcd[1].gd.pos.y = gcd[5].gd.pos.y+20;
	gcd[1].gd.pos.width = -1; gcd[1].gd.pos.height = 0;
	gcd[1].gd.flags = gg_visible | gg_enabled | gg_but_default;
	label[1].text = (unichar_t *) _STR_Next;
	label[1].text_in_resource = true;
	gcd[1].gd.mnemonic = 'N';
	gcd[1].gd.label = &label[1];
	gcd[1].gd.cid = CID_Next;
	gcd[1].creator = GButtonCreate;

	gcd[2].gd.pos.x = -15; gcd[2].gd.pos.y = gcd[1].gd.pos.y+3;
	gcd[2].gd.pos.width = -1; gcd[2].gd.pos.height = 0;
	gcd[2].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
	label[2].text = (unichar_t *) _STR_Stop;
	label[2].text_in_resource = true;
	gcd[2].gd.label = &label[2];
	gcd[2].gd.mnemonic = 'S';
	gcd[2].gd.cid = CID_Stop;
	gcd[2].creator = GButtonCreate;

	gcd[3].gd.pos.x = 2; gcd[3].gd.pos.y = 2;
	gcd[3].gd.pos.width = pos.width-4; gcd[3].gd.pos.height = pos.height-2;
	gcd[3].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
	gcd[3].creator = GGroupCreate;

	gcd[6].gd.pos.x = 200-30; gcd[6].gd.pos.y = gcd[2].gd.pos.y;
	gcd[6].gd.pos.width = -1; gcd[6].gd.pos.height = 0;
	gcd[6].gd.flags = /*gg_visible |*/ gg_enabled;
	label[6].text = (unichar_t *) _STR_Fix;
	label[6].text_in_resource = true;
	gcd[6].gd.mnemonic = 'F';
	gcd[6].gd.label = &label[6];
	gcd[6].gd.cid = CID_Fix;
	gcd[6].creator = GButtonCreate;

	GGadgetsCreate(p->explainw,gcd);
	p->explaintext = gcd[0].ret;
	p->explainvals = gcd[4].ret;
	p->ignoregadg = gcd[5].ret;
    } else
	GGadgetSetTitle(p->explaintext,GStringGetResource(explain,NULL));
    p->explaining = explain;
    fixable = explain==/*_STR_ProbHintHWidth || explain==_STR_ProbHintVWidth ||*/
	    explain==_STR_ProbFlippedRef ||
	    explain==_STR_ProbXNear || explain==_STR_ProbPtNearVHint ||
	    explain==_STR_ProbYNear || explain==_STR_ProbPtNearHHint ||
	    explain==_STR_ProbIrrelCP ||
	    explain==_STR_ProbYBase || explain==_STR_ProbYXHeight ||
	    explain==_STR_ProbYCapHeight || explain==_STR_ProbYAs ||
	    explain==_STR_ProbYDs ||
	    explain==_STR_ProbLineHor || explain==_STR_ProbLineVert ||
	    explain==_STR_ProbAboveHor || explain==_STR_ProbBelowHor ||
	    explain==_STR_ProbRightHor || explain==_STR_ProbLeftHor ||
	    explain==_STR_ProbAboveVert || explain==_STR_ProbBelowVert ||
	    explain==_STR_ProbRightVert || explain==_STR_ProbLeftVert ||
	    explain==_STR_ProbExpectedCounter || explain==_STR_ProbExpectedClockwise ||
	    explain==_STR_ProbBadWidth ||
	    explain==_STR_ProbBadVWidth;
    GGadgetSetVisible(GWidgetGetControl(p->explainw,CID_Fix),fixable);

    if ( explain==_STR_ProbBadSubs ) {
	u_snprintf(ubuf,sizeof(ubuf)/sizeof(ubuf[0]),
		GStringGetResource(_STR_ProbBadSubs2,NULL), p->badsubsname,
		(p->badsubstag>>24),(p->badsubstag>>16)&0xff,(p->badsubstag>>8)&0xff,p->badsubstag&0xff);
    } else if ( found==expected )
	ubuf[0]='\0';
    else {
	u_strcpy(ubuf,GStringGetResource(_STR_Found,NULL));
	sprintf(buf,"%.4g", found );
	uc_strcat(ubuf,buf);
	u_strcat(ubuf,GStringGetResource(_STR_Expected,NULL));
	sprintf(buf,"%.4g", expected );
	uc_strcat(ubuf,buf);
    }
    p->found = found; p->expected = expected;
    GGadgetSetTitle(p->explainvals,ubuf);
    GGadgetSetChecked(p->ignoregadg,false);

    p->doneexplain = false;
    p->ignorethis = false;

    if ( sc!=p->lastcharopened || sc->views==NULL ) {
	if ( p->cvopened!=NULL && CVValid(p->fv->sf,p->lastcharopened,p->cvopened) )
	    GDrawDestroyWindow(p->cvopened->gw);
	p->cvopened = NULL;
	if ( sc->views!=NULL )
	    GDrawRaise(sc->views->gw);
	else
	    p->cvopened = CharViewCreate(sc,p->fv);
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
	GDrawProcessPendingEvents(NULL);
	p->lastcharopened = sc;
    }
    if ( explain==_STR_ProbBadSubs ) {
	SCCharInfo(sc);
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
	GDrawProcessPendingEvents(NULL);
    }
	
    SCUpdateAll(sc);		/* We almost certainly just selected something */

    GDrawSetVisible(p->explainw,true);
    GDrawRaise(p->explainw);

    while ( !p->doneexplain )
	GDrawProcessOneEvent(NULL);
    /*GDrawSetVisible(p->explainw,false);*/		/* KDE gets unhappy about this and refuses to show the window later. I don't know why */

    if ( p->cv!=NULL ) {
	CVClearSel(p->cv);
    } else {
	for ( spl = p->sc->splines; spl!=NULL; spl = spl->next ) {
	    spl->first->selected = false;
	    first = NULL;
	    for ( spline = spl->first->next; spline!=NULL && spline!=first; spline=spline->to->next ) {
		spline->to->selected = false;
		if ( first==NULL ) first = spline;
	    }
	}
    }
}

static void _ExplainIt(struct problems *p, int enc, int explain,
	real found, real expected ) {
    ExplainIt(p,p->sc=SFMakeChar(p->fv->sf,enc),explain,found,expected);
}

/* if they deleted a point or a splineset while we were explaining then we */
/*  need to do some fix-ups. This routine detects a deletion and lets us know */
/*  that more processing is needed */
static int missing(struct problems *p,SplineSet *test, SplinePoint *sp) {
    SplinePointList *spl, *check;
    SplinePoint *tsp;

    if ( !p->explain )
return( false );

    if ( p->cv!=NULL )
	spl = *p->cv->heads[p->cv->drawmode];
    else
	spl = p->sc->splines;
    for ( check = spl; check!=test && check!=NULL; check = check->next );
    if ( check==NULL )
return( true );		/* Deleted splineset */

    if ( sp!=NULL ) {
	for ( tsp=test->first; tsp!=sp ; ) {
	    if ( tsp->next==NULL )
return( true );
	    tsp = tsp->next->to;
	    if ( tsp==test->first )
return( true );
	}
    }
return( false );
}

static int missingspline(struct problems *p,SplineSet *test, Spline *spline) {
    SplinePointList *spl, *check;
    Spline *t, *first=NULL;

    if ( !p->explain )
return( false );

    if ( p->cv!=NULL )
	spl = *p->cv->heads[p->cv->drawmode];
    else
	spl = p->sc->splines;
    for ( check = spl; check!=test && check!=NULL; check = check->next );
    if ( check==NULL )
return( true );		/* Deleted splineset */

    for ( t=test->first->next; t!=NULL && t!=first && t!=spline; t = t->to->next )
	if ( first==NULL ) first = t;
return( t!=spline );
}

static int missinghint(StemInfo *base, StemInfo *findme) {

    while ( base!=NULL && base!=findme )
	base = base->next;
return( base==NULL );
}

static int HVITest(struct problems *p,BasePoint *to, BasePoint *from,
	Spline *spline, int hasia, real ia) {
    real yoff, xoff, angle;
    int ishor=false, isvert=false, isital=false;
    int isto;
    int type;
    BasePoint *base, *other;
    static int hmsgs[5] = { _STR_ProbLineHor, _STR_ProbAboveHor, _STR_ProbBelowHor, _STR_ProbRightHor, _STR_ProbLeftHor };
    static int vmsgs[5] = { _STR_ProbLineVert, _STR_ProbAboveVert, _STR_ProbBelowVert, _STR_ProbRightVert, _STR_ProbLeftVert };
    static int imsgs[5] = { _STR_ProbLineItal, _STR_ProbAboveItal, _STR_ProbBelowItal, _STR_ProbRightItal, _STR_ProbLeftItal };

    yoff = to->y-from->y;
    xoff = to->x-from->x;
    angle = atan2(yoff,xoff);
    if ( angle<0 )
	angle += 3.1415926535897932;
    if ( angle<.1 || angle>3.1415926535897932-.1 ) {
	if ( yoff!=0 )
	    ishor = true;
    } else if ( angle>1.5707963-.1 && angle<1.5707963+.1 ) {
	if ( xoff!=0 )
	    isvert = true;
    } else if ( hasia && angle>ia-.1 && angle<ia+.1 ) {
	if ( angle<ia-.03 || angle>ia+.03 )
	    isital = true;
    }
    if ( ishor || isvert || isital ) {
	isto = false;
	if ( &spline->from->me==from || &spline->from->me==to )
	    spline->from->selected = true;
	if ( &spline->to->me==from || &spline->to->me==to )
	    spline->to->selected = isto = true;
	if ( from==&spline->from->me || from == &spline->to->me ) {
	    base = from; other = to;
	} else {
	    base = to; other = from;
	}
	if ( &spline->from->me==from && &spline->to->me==to ) {
	    type = 0;	/* Line */
	    if ( (ishor && xoff<0) || (isvert && yoff<0)) {
		base = from;
		other = to;
	    } else {
		base = to;
		other = from;
	    }
	} else if ( abs(yoff)>abs(xoff) )
	    type = ((yoff>0) ^ isto)?1:2;
	else
	    type = ((xoff>0) ^ isto)?3:4;
	if ( ishor )
	    ExplainIt(p,p->sc,hmsgs[type], other->y,base->y);
	else if ( isvert )
	    ExplainIt(p,p->sc,vmsgs[type], other->x,base->x);
	else
	    ExplainIt(p,p->sc,imsgs[type],0,0);
return( true );
    }
return( false );
}

/* Is the control point outside of the spline segment when projected onto the */
/*  vector between the end points of the spline segment? */
static int OddCPCheck(BasePoint *cp,BasePoint *base,BasePoint *v,
	SplinePoint *sp, struct problems *p) {
    real len = (cp->x-base->x)*v->x+ (cp->y-base->y)*v->y;
    real xoff, yoff;
    int msg=0;

    if ( len<0 || len>1 || (len==0 && &sp->me!=base) || (len==1 && &sp->me==base)) {
	xoff = cp->x-sp->me.x; yoff = cp->y-sp->me.y;
	if ( fabs(yoff)>fabs(xoff) )
	    msg = yoff>0?_STR_ProbAboveOdd:_STR_ProbBelowOdd;
	else
	    msg = xoff>0?_STR_ProbRightOdd:_STR_ProbLeftOdd;
	sp->selected = true;
	ExplainIt(p,p->sc,msg, 0,0);
return( true );
    }
return( false );
}

static int Hint3Check(struct problems *p,StemInfo *h) {
    StemInfo *h2, *h3;

    /* Must be three hints to be interesting */
    if ( h==NULL || (h2=h->next)==NULL || (h3=h2->next)==NULL )
return(false);
    if ( h3->next!=NULL ) {
	StemInfo *bad, *goods[3];
	if ( h3->next->next!=NULL )	/* Don't try to find a subset with 5 */
return(false);
	if ( h->width==h2->width || h->width==h3->width ) {
	    goods[0] = h;
	    if ( h->width==h2->width ) {
		goods[1] = h2;
		if ( h->width==h3->width && h->width!=h3->next->width ) {
		    goods[2] = h3;
		    bad = h3->next;
		} else if ( h->width!=h3->width && h->width==h3->next->width ) {
		    goods[2] = h3->next;
		    bad = h3;
		} else
return(false);
	    } else if ( h->width==h3->width && h->width==h3->next->width ) {
		goods[1] = h3;
		goods[2] = h3->next;
		bad = h2;
	    } else
return(false);
	} else if ( h2->width == h3->width && h2->width==h3->next->width ) {
	    bad = h;
	    goods[0] = h2; goods[1] = h3; goods[2] = h3->next;
	} else
return(false);
	if ( goods[2]->start-goods[1]->start == goods[1]->start-goods[0]->start ) {
	    bad->active = true;
	    ExplainIt(p,p->sc,_STR_ProbHint3Four,0,0);
	    if ( !missinghint(p->sc->hstem,bad) || !missinghint(p->sc->vstem,bad))
		bad->active = false;
	    if ( p->ignorethis )
		p->stem3 = false;
return( true );
	}
return(false);
    }

    if ( h->width==h2->width && h->width==h3->width &&
	    h2->start-h->start == h3->start-h2->start ) {
	if ( p->showexactstem3 ) {
	    ExplainIt(p,p->sc,_STR_NoProbHint3,0,0);
	    if ( p->ignorethis )
		p->showexactstem3 = false;
	}
return( false );		/* It IS a stem3, so don't complain */
    }

    if ( h->width==h2->width && h->width==h3->width ) {
	if ( h2->start-h->start+p->near > h3->start-h2->start &&
		h2->start-h->start-p->near < h3->start-h2->start ) {
	    ExplainIt(p,p->sc,_STR_ProbHint3Spacing,0,0);
	    if ( p->ignorethis )
		p->stem3 = false;
return( true );
	}
return( false );
    }

    if ( (h2->start-h->start+p->near > h3->start-h2->start &&
	     h2->start-h->start-p->near < h3->start-h2->start ) ||
	    (h2->start-h->start-h->width+p->near > h3->start-h2->start-h2->width &&
	     h2->start-h->start-h->width-p->near < h3->start-h2->start-h2->width )) {
	if ( h->width==h2->width ) {
	    if ( h->width+p->near > h3->width && h->width-p->near < h3->width ) {
		h3->active = true;
		ExplainIt(p,p->sc,_STR_ProbHint3Width,0,0);
		if ( !missinghint(p->sc->hstem,h3) || !missinghint(p->sc->vstem,h3))
		    h3->active = false;
		if ( p->ignorethis )
		    p->stem3 = false;
return( true );
	    } else
return( false );
	}
	if ( h->width==h3->width ) {
	    if ( h->width+p->near > h2->width && h->width-p->near < h2->width ) {
		h2->active = true;
		ExplainIt(p,p->sc,_STR_ProbHint3Width,0,0);
		if ( !missinghint(p->sc->hstem,h2) || !missinghint(p->sc->vstem,h2))
		    h2->active = false;
		if ( p->ignorethis )
		    p->stem3 = false;
return( true );
	    } else
return( false );
	}
	if ( h2->width==h3->width ) {
	    if ( h2->width+p->near > h->width && h2->width-p->near < h->width ) {
		h->active = true;
		ExplainIt(p,p->sc,_STR_ProbHint3Width,0,0);
		if ( !missinghint(p->sc->hstem,h) || !missinghint(p->sc->vstem,h))
		    h->active = false;
		if ( p->ignorethis )
		    p->stem3 = false;
return( true );
	    } else
return( false );
	}
    }
return( false );
}

static int SCProblems(CharView *cv,SplineChar *sc,struct problems *p) {
    SplineSet *spl, *test;
    Spline *spline, *first;
    SplinePoint *sp, *nsp;
    int needsupdate=false, changed=false;
    StemInfo *h;

  restart:
    if ( cv!=NULL ) {
	needsupdate = CVClearSel(cv);
	spl = *cv->heads[cv->drawmode];
	sc = cv->sc;
    } else {
	for ( spl = sc->splines; spl!=NULL; spl = spl->next ) {
	    if ( spl->first->selected ) { needsupdate = true; spl->first->selected = false; }
	    first = NULL;
	    for ( spline = spl->first->next; spline!=NULL && spline!=first; spline=spline->to->next ) {
		if ( spline->to->selected )
		    { needsupdate = true; spline->to->selected = false; }
		if ( first==NULL ) first = spline;
	    }
	}
	spl = sc->splines;
    }
    p->sc = sc;
    if (( p->ptnearhint || p->hintwidthnearval || p->hintwithnopt ) &&
	    sc->changedsincelasthinted && !sc->manualhints )
	SplineCharAutoHint(sc,true);

    if ( p->openpaths ) {
	for ( test=spl; test!=NULL && !p->finish; test=test->next ) {
	    /* I'm also including in "open paths" the special case of a */
	    /*  singleton point with connects to itself */
	    if ( test->first!=NULL && ( test->first->prev==NULL ||
		    ( test->first->prev == test->first->next &&
			test->first->noprevcp && test->first->nonextcp))) {
		changed = true;
		test->first->selected = test->last->selected = true;
		ExplainIt(p,sc,_STR_ProbOpenPath,0,0);
		if ( p->ignorethis ) {
		    p->openpaths = false;
	break;
		}
		if ( missing(p,test,NULL))
      goto restart;
	    }
	}
    }

    if ( p->pointstooclose && !p->finish ) {
	for ( test=spl; test!=NULL && !p->finish && p->pointstooclose; test=test->next ) {
	    sp = test->first;
	    do {
		if ( sp->next==NULL )
	    break;
		nsp = sp->next->to;
		if ( (nsp->me.x-sp->me.x)*(nsp->me.x-sp->me.x) + (nsp->me.y-sp->me.y)*(nsp->me.y-sp->me.y) < 2*2 ) {
		    changed = true;
		    sp->selected = nsp->selected = true;
		    ExplainIt(p,sc,_STR_ProbPointsTooClose,0,0);
		    if ( p->ignorethis ) {
			p->pointstooclose = false;
	    break;
		    }
		    if ( missing(p,test,nsp))
  goto restart;
		}
		sp = nsp;
	    } while ( sp!=test->first && !p->finish );
	    if ( !p->pointstooclose )
	break;
	}
    }

    if ( p->xnearval && !p->finish ) {
	for ( test=spl; test!=NULL && !p->finish && p->xnearval; test=test->next ) {
	    sp = test->first;
	    do {
		if ( sp->me.x-p->xval<p->near && p->xval-sp->me.x<p->near &&
			sp->me.x!=p->xval ) {
		    changed = true;
		    sp->selected = true;
		    ExplainIt(p,sc,_STR_ProbXNear,sp->me.x,p->xval);
		    if ( p->ignorethis ) {
			p->xnearval = false;
	    break;
		    }
		    if ( missing(p,test,sp))
  goto restart;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
	    } while ( sp!=test->first && !p->finish );
	    if ( !p->xnearval )
	break;
	}
    }

    if ( p->ynearval && !p->finish ) {
	for ( test=spl; test!=NULL && !p->finish && p->ynearval; test=test->next ) {
	    sp = test->first;
	    do {
		if ( sp->me.y-p->yval<p->near && p->yval-sp->me.y<p->near &&
			sp->me.y != p->yval ) {
		    changed = true;
		    sp->selected = true;
		    ExplainIt(p,sc,_STR_ProbYNear,sp->me.y,p->yval);
		    if ( p->ignorethis ) {
			p->ynearval = false;
	    break;
		    }
		    if ( missing(p,test,sp))
  goto restart;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
	    } while ( sp!=test->first && !p->finish );
	    if ( !p->ynearval )
	break;
	}
    }

    if ( p->ynearstd && !p->finish ) {
	real expected;
	int msg;
	for ( test=spl; test!=NULL && !p->finish && p->ynearstd; test=test->next ) {
	    sp = test->first;
	    do {
		if (( sp->me.y-p->xheight<p->near && p->xheight-sp->me.y<p->near && sp->me.y!=p->xheight ) ||
			( sp->me.y-p->caph<p->near && p->caph-sp->me.y<p->near && sp->me.y!=p->caph && sp->me.y!=p->ascent ) ||
			( sp->me.y-p->ascent<p->near && p->ascent-sp->me.y<p->near && sp->me.y!=p->caph && sp->me.y!=p->ascent ) ||
			( sp->me.y-p->descent<p->near && p->descent-sp->me.y<p->near && sp->me.y!=p->descent ) ||
			( sp->me.y<p->near && -sp->me.y<p->near && sp->me.y!=0 ) ) {
		    changed = true;
		    sp->selected = true;
		    if ( sp->me.y<p->near && -sp->me.y<p->near ) {
			msg = _STR_ProbYBase;
			expected = 0;
		    } else if ( sp->me.y-p->xheight<p->near && p->xheight-sp->me.y<p->near ) {
			msg = _STR_ProbYXHeight;
			expected = p->xheight;
		    } else if ( sp->me.y-p->caph<p->near && p->caph-sp->me.y<p->near ) {
			msg = _STR_ProbYCapHeight;
			expected = p->caph;
		    } else if ( sp->me.y-p->ascent<p->near && p->ascent-sp->me.y<p->near ) {
			msg = _STR_ProbYAs;
			expected = p->ascent;
		    } else {
			msg = _STR_ProbYDs;
			expected = p->descent;
		    }
		    ExplainIt(p,sc,msg,sp->me.y,expected);
		    if ( p->ignorethis ) {
			p->ynearstd = false;
	    break;
		    }
		    if ( missing(p,test,sp))
  goto restart;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
	    } while ( sp!=test->first && !p->finish );
	    if ( !p->ynearstd )
	break;
	}
    }

    if ( p->linenearstd && !p->finish ) {
	real ia = (90-p->fv->sf->italicangle)*(3.1415926535897932/180);
	int hasia = p->fv->sf->italicangle!=0;
	for ( test=spl; test!=NULL && !p->finish && p->linenearstd; test = test->next ) {
	    first = NULL;
	    for ( spline = test->first->next; spline!=NULL && spline!=first && !p->finish; spline=spline->to->next ) {
		if ( spline->knownlinear ) {
		    if ( HVITest(p,&spline->to->me,&spline->from->me,spline,
			    hasia, ia)) {
			changed = true;
			if ( p->ignorethis ) {
			    p->linenearstd = false;
	    break;
			}
			if ( missingspline(p,test,spline))
  goto restart;
		    }
		}
		if ( first==NULL ) first = spline;
	    }
	    if ( !p->linenearstd )
	break;
	}
    }

    if ( p->cpnearstd && !p->finish ) {
	real ia = (90-p->fv->sf->italicangle)*(3.1415926535897932/180);
	int hasia = p->fv->sf->italicangle!=0;
	for ( test=spl; test!=NULL && !p->finish && p->linenearstd; test = test->next ) {
	    first = NULL;
	    for ( spline = test->first->next; spline!=NULL && spline!=first && !p->finish; spline=spline->to->next ) {
		if ( !spline->knownlinear ) {
		    if ( !spline->from->nonextcp &&
			    HVITest(p,&spline->from->nextcp,&spline->from->me,spline,
				hasia, ia)) {
			changed = true;
			if ( p->ignorethis ) {
			    p->cpnearstd = false;
	    break;
			}
			if ( missingspline(p,test,spline))
  goto restart;
		    }
		    if ( !spline->to->noprevcp &&
			    HVITest(p,&spline->to->me,&spline->to->prevcp,spline,
				hasia, ia)) {
			changed = true;
			if ( p->ignorethis ) {
			    p->cpnearstd = false;
	    break;
			}
			if ( missingspline(p,test,spline))
  goto restart;
		    }
		}
		if ( first==NULL ) first = spline;
	    }
	    if ( !p->cpnearstd )
	break;
	}
    }

    if ( p->cpodd && !p->finish ) {
	for ( test=spl; test!=NULL && !p->finish && p->linenearstd; test = test->next ) {
	    first = NULL;
	    for ( spline = test->first->next; spline!=NULL && spline!=first && !p->finish; spline=spline->to->next ) {
		if ( !spline->knownlinear ) {
		    BasePoint v; real len;
		    v.x = spline->to->me.x-spline->from->me.x;
		    v.y = spline->to->me.y-spline->from->me.y;
		    len = /*sqrt*/(v.x*v.x+v.y*v.y);
		    v.x /= len; v.y /= len;
		    if ( !spline->from->nonextcp &&
			    OddCPCheck(&spline->from->nextcp,&spline->from->me,&v,
			     spline->from,p)) {
			changed = true;
			if ( p->ignorethis ) {
			    p->cpodd = false;
	    break;
			}
			if ( missingspline(p,test,spline))
  goto restart;
		    }
		    if ( !spline->to->noprevcp &&
			    OddCPCheck(&spline->to->prevcp,&spline->from->me,&v,
			     spline->to,p)) {
			changed = true;
			if ( p->ignorethis ) {
			    p->cpodd = false;
	    break;
			}
			if ( missingspline(p,test,spline))
  goto restart;
		    }
		}
		if ( first==NULL ) first = spline;
	    }
	    if ( !p->cpodd )
	break;
	}
    }

    if ( p->irrelevantcontrolpoints && !p->finish ) {
	for ( test=spl; test!=NULL && !p->finish && p->irrelevantcontrolpoints; test = test->next ) {
	    for ( sp=test->first; !p->finish && p->irrelevantcontrolpoints; ) {
		int either = false;
		if ( sp->prev!=NULL ) {
		    double len = sqrt((sp->me.x-sp->prev->from->me.x)*(sp->me.x-sp->prev->from->me.x) +
			    (sp->me.y-sp->prev->from->me.y)*(sp->me.y-sp->prev->from->me.y));
		    double cplen = sqrt((sp->me.x-sp->prevcp.x)*(sp->me.x-sp->prevcp.x) +
			    (sp->me.y-sp->prevcp.y)*(sp->me.y-sp->prevcp.y));
		    if ( cplen!=0 && cplen<p->irrelevantfactor*len )
			either = true;
		}
		if ( sp->next!=NULL ) {
		    double len = sqrt((sp->me.x-sp->next->to->me.x)*(sp->me.x-sp->next->to->me.x) +
			    (sp->me.y-sp->next->to->me.y)*(sp->me.y-sp->next->to->me.y));
		    double cplen = sqrt((sp->me.x-sp->nextcp.x)*(sp->me.x-sp->nextcp.x) +
			    (sp->me.y-sp->nextcp.y)*(sp->me.y-sp->nextcp.y));
		    if ( cplen!=0 && cplen<p->irrelevantfactor*len )
			either = true;
		}
		if ( either ) {
		    sp->selected = true;
		    ExplainIt(p,sc,_STR_ProbIrrelCP,0,0);
		    if ( p->ignorethis ) {
			p->irrelevantcontrolpoints = false;
	    break;
		    }
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
		if ( sp==test->first )
	    break;
	    }
	}
    }

    if ( p->hintwithnopt && !p->finish ) {
	int anys, anye;
      restarthhint:
	for ( h=sc->hstem; h!=NULL ; h=h->next ) {
	    anys = anye = false;
	    for ( test=spl; test!=NULL && !p->finish && (!anys || !anye); test=test->next ) {
		sp = test->first;
		do {
		    if (sp->me.y==h->start )
			anys = true;
		    if (sp->me.y==h->start+h->width )
			anye = true;
		    if ( sp->next==NULL )
		break;
		    sp = sp->next->to;
		} while ( sp!=test->first && !p->finish );
	    }
	    if ( h->ghost && ( anys || anye ))
		/* Ghost hints only define one edge */;
	    else if ( !anys || !anye ) {
		h->active = true;
		changed = true;
		ExplainIt(p,sc,_STR_ProbHintControl,0,0);
		if ( !missinghint(sc->hstem,h))
		    h->active = false;
		if ( p->ignorethis ) {
		    p->hintwithnopt = false;
	break;
		}
		if ( missinghint(sc->hstem,h))
      goto restarthhint;
	    }
	}
      restartvhint:
	for ( h=sc->vstem; h!=NULL && p->hintwithnopt && !p->finish; h=h->next ) {
	    anys = anye = false;
	    for ( test=spl; test!=NULL && !p->finish && (!anys || !anye); test=test->next ) {
		sp = test->first;
		do {
		    if (sp->me.x==h->start )
			anys = true;
		    if (sp->me.x==h->start+h->width )
			anye = true;
		    if ( sp->next==NULL )
		break;
		    sp = sp->next->to;
		} while ( sp!=test->first && !p->finish );
	    }
	    if ( !anys || !anye ) {
		h->active = true;
		changed = true;
		ExplainIt(p,sc,_STR_ProbHintControl,0,0);
		if ( p->ignorethis ) {
		    p->hintwithnopt = false;
	break;
		}
		if ( missinghint(sc->vstem,h))
      goto restartvhint;
		h->active = false;
	    }
	}
    }

    if ( p->ptnearhint && !p->finish ) {
	real found, expected;
	for ( test=spl; test!=NULL && !p->finish && p->ptnearhint; test=test->next ) {
	    sp = test->first;
	    do {
		int hs = false, vs = false;
		for ( h=sc->hstem; h!=NULL; h=h->next ) {
		    if (( sp->me.y-h->start<p->near && h->start-sp->me.y<p->near &&
				sp->me.y!=h->start ) ||
			    ( sp->me.y-h->start+h->width<p->near && h->start+h->width-sp->me.y<p->near &&
				sp->me.y!=h->start+h->width )) {
			found = sp->me.y;
			if ( sp->me.y-h->start<p->near && h->start-sp->me.y<p->near )
			    expected = h->start;
			else
			    expected = h->start+h->width;
			h->active = true;
			hs = true;
		break;
		    }
		}
		if ( !hs ) {
		    for ( h=sc->vstem; h!=NULL; h=h->next ) {
			if (( sp->me.x-h->start<p->near && h->start-sp->me.x<p->near &&
				    sp->me.x!=h->start ) ||
				( sp->me.x-h->start+h->width<p->near && h->start+h->width-sp->me.x<p->near &&
				    sp->me.x!=h->start+h->width )) {
			    found = sp->me.x;
			    if ( sp->me.x-h->start<p->near && h->start-sp->me.x<p->near )
				expected = h->start;
			    else
				expected = h->start+h->width;
			    h->active = true;
			    vs = true;
		    break;
			}
		    }
		}
		if ( hs || vs ) {
		    changed = true;
		    sp->selected = true;
		    ExplainIt(p,sc,hs?_STR_ProbPtNearHHint:_STR_ProbPtNearVHint,found,expected);
		    if ( p->ignorethis ) {
			p->ptnearhint = false;
	    break;
		    }
		    if ( missing(p,test,sp))
  goto restart;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
	    } while ( sp!=test->first && !p->finish );
	    if ( !p->ptnearhint )
	break;
	}
    }

    if ( p->hintwidthnearval && !p->finish ) {
	StemInfo *hs = NULL, *vs = NULL;
	for ( h=sc->hstem; h!=NULL; h=h->next ) {
	    if ( h->width-p->widthval<p->near && p->widthval-h->width<p->near &&
		    h->width!=p->widthval ) {
		h->active = true;
		hs = h;
	break;
	    }
	}
	for ( h=sc->vstem; h!=NULL; h=h->next ) {
	    if ( h->width-p->widthval<p->near && p->widthval-h->width<p->near &&
		    h->width!=p->widthval ) {
		h->active = true;
		vs = h;
	break;
	    }
	}
	if ( hs || vs ) {
	    changed = true;
	    ExplainIt(p,sc,hs?_STR_ProbHintHWidth:_STR_ProbHintVWidth,
		    hs?hs->width:vs->width,p->widthval);
	    if ( hs!=NULL && !missinghint(sc->hstem,hs)) hs->active = false;
	    if ( vs!=NULL && !missinghint(sc->vstem,vs)) vs->active = false;
	    if ( p->ignorethis )
		p->hintwidthnearval = false;
	    else if ( (hs!=NULL && missinghint(sc->hstem,hs)) &&
		    ( vs!=NULL && missinghint(sc->vstem,vs)))
      goto restart;
	}
    }

    if ( p->stem3 && !p->finish )
	changed |= Hint3Check(p,sc->hstem);
    if ( p->stem3 && !p->finish )
	changed |= Hint3Check(p,sc->vstem);

    if ( p->direction && !p->finish ) {
	SplineSet **base, *ret;
	int lastscan= -1;
	if ( cv!=NULL )
	    base = cv->heads[cv->drawmode];
	else
	    base = &sc->splines;
	while ( !p->finish && (ret=SplineSetsDetectDir(base,&lastscan))!=NULL ) {
	    sp = ret->first;
	    changed = true;
	    while ( 1 ) {
		sp->selected = true;
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
		if ( sp==ret->first )
	    break;
	    }
	    if ( SplinePointListIsClockwise(ret))
		ExplainIt(p,sc,_STR_ProbExpectedCounter,0,0);
	    else
		ExplainIt(p,sc,_STR_ProbExpectedClockwise,0,0);
	    if ( p->ignorethis ) {
		p->direction = false;
	break;
	    }
	}
    }

    if ( p->flippedrefs && !p->finish && ( cv==NULL || cv->drawmode==dm_fore )) {
	RefChar *ref;
	for ( ref = sc->refs; ref!=NULL ; ref = ref->next )
	    ref->selected = false;
	for ( ref = sc->refs; !p->finish && ref!=NULL ; ref = ref->next ) {
	    if ( ref->transform[0]*ref->transform[3]<0 ||
		    (ref->transform[0]==0 && ref->transform[1]*ref->transform[2]>0)) {
		changed = true;
		ref->selected = true;
		ExplainIt(p,sc,_STR_ProbFlippedRef,0,0);
		ref->selected = false;
		if ( p->ignorethis ) {
		    p->flippedrefs = false;
	break;
		}
	    }
	}
    }

    if ( p->toomanypoints && !p->finish ) {
	int cnt=0;
	for ( spl = sc->splines; spl!=NULL; spl = spl->next ) {
	    for ( sp = spl->first; ; ) {
		++cnt;
		if ( sp->prev!=NULL && !sp->prev->knownlinear ) {
		    if ( sp->prev->order2 )
			++cnt;
		    else
			cnt += 2;
		}
		if ( sp->next==NULL )
	    break;
		sp = sp->next->to;
		if ( sp==spl->first )
	    break;
	    }
	}
	if ( cnt>=p->pointsmax ) {
	    changed = true;
	    ExplainIt(p,sc,_STR_ProbTooManyPoints,cnt,p->pointsmax);
	    if ( p->ignorethis )
		p->toomanypoints = false;
	}
    }

    if ( p->bitmaps && !p->finish && SCWorthOutputting(sc)) {
	BDFFont *bdf;

	for ( bdf=sc->parent->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	    if ( sc->enc>=bdf->charcnt || bdf->chars[sc->enc]==NULL ) {
		changed = true;
		ExplainIt(p,sc,_STR_ProbMissingBitmap,0,0);
		if ( p->ignorethis )
		    p->bitmaps = false;
	break;
	    }
	}
    }

    if ( p->advancewidth && !p->finish && SCWorthOutputting(sc)) {
	if ( sc->width!=p->advancewidthval ) {
	    changed = true;
	    ExplainIt(p,sc,_STR_ProbBadWidth,sc->width,p->advancewidthval);
	    if ( p->ignorethis )
		p->advancewidth = false;
	}
    }

    if ( p->vadvancewidth && !p->finish && SCWorthOutputting(sc)) {
	if ( sc->vwidth!=p->vadvancewidthval ) {
	    changed = true;
	    ExplainIt(p,sc,_STR_ProbBadVWidth,sc->vwidth,p->vadvancewidthval);
	    if ( p->ignorethis )
		p->vadvancewidth = false;
	}
    }

    if ( p->badsubs && !p->finish ) {
	PST *pst;
	char *pt, *end; int ch;
	for ( pst = sc->possub ; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_substitution || pst->type==pst_alternate ||
		    pst->type==pst_multiple || pst->type==pst_ligature ) {
		for ( pt=pst->u.subs.variant; *pt!='\0' ; pt=end ) {
		    end = strchr(pt,' ');
		    if ( end==NULL ) end=pt+strlen(pt);
		    ch = *end;
		    *end = '\0';
		    if ( !SCWorthOutputting(SFGetCharDup(sc->parent,-1,pt)) ) {
			changed = true;
			p->badsubsname = copy(pt);
			*end = ch;
			p->badsubstag = pst->tag;
			ExplainIt(p,sc,_STR_ProbBadSubs,0,0);
			free(p->badsubsname);
			if ( p->ignorethis )
			    p->badsubs = false;
		    } else
			*end = ch;
		    while ( *end==' ' ) ++end;
		    if ( !p->badsubs )
		break;
		}
		if ( !p->badsubs )
	    break;
	    }
	}
    }


    if ( needsupdate || changed )
	SCUpdateAll(sc);
return( changed );
}

static int CIDCheck(struct problems *p,int cid) {
    int found = false;

    if ( (p->cidmultiple || p->cidblank) && !p->finish ) {
	SplineFont *csf = p->fv->cidmaster;
	int i, cnt;
	for ( i=cnt=0; i<csf->subfontcnt; ++i )
	    if ( cid<csf->subfonts[i]->charcnt &&
		    SCWorthOutputting(csf->subfonts[i]->chars[cid]) )
		++cnt;
	if ( cnt>1 && p->cidmultiple ) {
	    _ExplainIt(p,cid,_STR_ProbCIDMult,cnt,1);
	    if ( p->ignorethis )
		p->cidmultiple = false;
	    found = true;
	} else if ( cnt==0 && p->cidblank ) {
	    _ExplainIt(p,cid,_STR_ProbCIDBlank,0,0);
	    if ( p->ignorethis )
		p->cidblank = false;
	    found = true;
	}
    }
return( found );
}

static char *missinglookup(struct problems *p,char *str) {
    int i;

    for ( i=0; i<p->rpl_cnt; ++i )
	if ( strcmp(str,p->mg[i].search)==0 )
return( p->mg[i].rpl );

return( NULL );
}

static uint32 missinglookup_tag(struct problems *p,uint32 tag) {
    int i;

    for ( i=0; i<p->rpl_cnt; ++i )
	if ( p->mlt[i].search==tag )
return( p->mlt[i].rpl );

return( 0 );
}

static void mgreplace(char **base, char *str,char *end, char *new, SplineChar *sc, PST *pst) {
    PST *p, *ps;

    if ( new==NULL || *new=='\0' ) {
	if ( *base==str && *end=='\0' && sc!=NULL ) {
	    /* We deleted the last name from the pst, it is meaningless, remove it */
	    if ( sc->possub==pst )
		sc->possub = pst->next;
	    else {
		for ( p = sc->possub, ps=p->next; ps!=NULL && ps!=pst; p=ps, ps=ps->next );
		if ( ps!=NULL )
		    p->next = pst->next;
	    }
	    pst->next = NULL;
	    PSTFree(pst);
	} else if ( *end=='\0' )
	    *str = '\0';
	else
	    strcpy(str,end+1);	/* Skip the space */
    } else {
	char *res = galloc(strlen(*base)+strlen(new)-(end-str)+1);
	strncpy(res,*base,str-*base);
	strcpy(res+(str-*base),new);
	strcat(res,end);
	free(*base);
	*base = res;
    }
}

static void ClearMissingState(struct problems *p) {
    int i;

    if ( p->mg!=NULL ) {
	for ( i=0; i<p->rpl_cnt; ++i ) {
	    free(p->mg[i].search);
	    free(p->mg[i].rpl);
	}
	free(p->mg);
    } else
	free(p->mlt);
    p->mlt = NULL;
    p->mg = NULL;
    p->rpl_cnt = p->rpl_max = 0;
}
	    
enum missingglyph_type { mg_pst, mg_fpst, mg_kern, mg_vkern, mg_lookups };
struct mgask_data {
    GWindow gw;
    uint8 done, skipped, islookup;
    uint32 tag;
    char **_str, *start, *end;
    SplineChar *sc;
    PST *pst;
    struct problems *p;
};

static void mark_tagto_replace(struct problems *p,uint32 tag, uint32 rpl) {

    if ( p->rpl_cnt >= p->rpl_max ) {
	if ( p->rpl_max == 0 )
	    p->mlt = galloc((p->rpl_max = 30)*sizeof(struct mlrpl));
	else
	    p->mlt = grealloc(p->mlt,(p->rpl_max += 30)*sizeof(struct mlrpl));
    }
    p->mlt[p->rpl_cnt].search = tag;
    p->mlt[p->rpl_cnt++].rpl = rpl;
}

static void mark_to_replace(struct problems *p,struct mgask_data *d, char *rpl) {
    int ch;

    if ( p->rpl_cnt >= p->rpl_max ) {
	if ( p->rpl_max == 0 )
	    p->mg = galloc((p->rpl_max = 30)*sizeof(struct mgrpl));
	else
	    p->mg = grealloc(p->mg,(p->rpl_max += 30)*sizeof(struct mgrpl));
    }
    ch = *d->end; *d->end = '\0';
    p->mg[p->rpl_cnt].search = copy( d->start );
    p->mg[p->rpl_cnt++].rpl = copy( rpl );
    *d->end = ch;
}

#define CID_Always	1001
#define CID_RplText	1002
#define CID_Ignore	1003
#define CID_Rpl		1004
#define CID_Skip	1005
#define CID_Delete	1006

static int MGA_RplChange(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_textchanged ) {
	struct mgask_data *d = GDrawGetUserData(GGadgetGetWindow(g));
	const unichar_t *rpl = _GGadgetGetTitle(g);
	GGadgetSetEnabled(GWidgetGetControl(d->gw,CID_Rpl),*rpl!=0);
    }
return( true );
}

static int MGA_Rpl(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct mgask_data *d = GDrawGetUserData(GGadgetGetWindow(g));
	const unichar_t *_rpl = _GGadgetGetTitle(GWidgetGetControl(d->gw,CID_RplText));
	if ( d->islookup ) {
	    if ( u_strlen(_rpl)!=4 ) {
		GWidgetErrorR(_STR_TagMustBe4,_STR_TagMustBe4);
return( true);
	    }
	    mark_tagto_replace(d->p,d->tag,((_rpl[0]&0xff)<<24)|((_rpl[1]&0xff)<<16)|((_rpl[2]&0xff)<<8)|((_rpl[3]&0xff)));
	} else {
	    char *rpl = cu_copy(_rpl);
	    if ( GGadgetIsChecked(GWidgetGetControl(d->gw,CID_Always)))
		mark_to_replace(d->p,d,rpl);
	    mgreplace(d->_str,d->start,d->end,rpl,d->sc,d->pst);
	    free(rpl);
	}
	d->done = true;
    }
return( true );
}

static int MGA_Delete(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct mgask_data *d = GDrawGetUserData(GGadgetGetWindow(g));
	if ( d->islookup )
	    mark_tagto_replace(d->p,d->tag,-1);
	else {
	    if ( GGadgetIsChecked(GWidgetGetControl(d->gw,CID_Always)))
		mark_to_replace(d->p,d,"");
	    mgreplace(d->_str,d->start,d->end,"",d->sc,d->pst);
	}
	d->done = true;
    }
return( true );
}

static int MGA_Skip(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct mgask_data *d = GDrawGetUserData(GGadgetGetWindow(g));
	d->done = d->skipped = true;
    }
return( true );
}

static int mgask_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	struct mgask_data *d = GDrawGetUserData(gw);
	d->done = d->skipped = true;
    } else if ( event->type==et_char ) {
	if ( event->u.chr.keysym == GK_F1 || event->u.chr.keysym == GK_Help ) {
	    help("problems.html");
return( true );
	}
return( false );
    }
return( true );
}

static int mgAsk(struct problems *p,char **_str,char *str, char *end,uint32 tag,
	SplineChar *sc,enum missingglyph_type which,void *data) {
    unichar_t buffer[200], tbuf[6];
    static char *pstnames[] = { "", "position", "pair", "substitution",
	"alternate subs", "multiple subs", "ligature", NULL };
    static char *fpstnames[] = { "Contextual position", "Contextual substitution",
	"Chaining position", "Chaining substitution", "Reverse chaining subs", NULL };
    PST *pst = data;
    FPST *fpst = data;
    char end_ch;
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[12];
    GTextInfo label[12];
    struct mgask_data d;
    int blen = GIntGetResource(_NUM_Buttonsize), ptwidth;
    int k, rplpos;

    if ( which != mg_lookups ) {
	end_ch = *end; *end = '\0';
    }

    if ( which == mg_pst )
	u_snprintf(buffer,sizeof(buffer)/sizeof(buffer[0]),
		GStringGetResource(_STR_GlyphPSTTag,NULL),
		sc->name, pstnames[pst->type],
		pst->tag>>24, (pst->tag>>16)&0xff, (pst->tag>>8)&0xff, pst->tag&0xff);
    else if ( which == mg_fpst || which==mg_lookups )
	u_snprintf(buffer,sizeof(buffer)/sizeof(buffer[0]),
		GStringGetResource(_STR_FPSTKernTag,NULL),
		fpstnames[fpst->type-pst_contextpos],
		fpst->tag>>24, (fpst->tag>>16)&0xff, (fpst->tag>>8)&0xff, fpst->tag&0xff);
    else
	u_snprintf(buffer,sizeof(buffer)/sizeof(buffer[0]),
		GStringGetResource(_STR_FPSTKernTag,NULL),
		which==mg_kern ? "Kerning Class": "Vertical Kerning Class",
		which==mg_kern ? 'k' : 'v', which==mg_kern ? 'e' : 'k', 'r', 'n' );

    memset(&d,'\0',sizeof(d));
    d._str = _str;
    d.start = str;
    d.end = end;
    d.sc = sc;
    d.pst = which==mg_pst ? data : NULL;
    d.p = p;
    d.islookup = which==mg_lookups;
    d.tag = tag;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_centered|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.centered = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = GStringGetResource(_STR_MissingGlyph,NULL);
    pos.x = pos.y = 0;
    ptwidth = 3*blen+GGadgetScale(80);
    pos.width =GDrawPointsToPixels(NULL,ptwidth);
    pos.height = GDrawPointsToPixels(NULL,180);
    d.gw = gw = GDrawCreateTopWindow(NULL,&pos,mgask_e_h,&d,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    k=0;
    label[k].text = buffer;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = 6;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k++].creator = GLabelCreate;

    label[k].text = (unichar_t *) (which==mg_lookups ?
	    _STR_RefersToMissingLookup :
	    _STR_RefersToMissingGlyph);
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+13;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k++].creator = GLabelCreate;

    if ( which!=mg_lookups ) {
	label[k].text = (unichar_t *) str;
	label[k].text_is_1byte = true;
    } else {
	label[k].text = tbuf;
	tbuf[0] = tag>>24; tbuf[1] = (tag>>16)&0xff;
	tbuf[2] = (tag>>8)&0xff; tbuf[3] = tag&0xff;
	tbuf[4] = 0;
    }
    gcd[k].gd.label = &label[k];
    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+13;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k++].creator = GLabelCreate;

    label[k].text = (unichar_t *) _STR_ReplaceWith;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.pos.x = 5; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+16;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k++].creator = GLabelCreate;

    rplpos = k;
    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+13; gcd[k].gd.pos.width = ptwidth-20;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k].gd.cid = CID_RplText;
    gcd[k].gd.handle_controlevent = MGA_RplChange;
    if ( which==mg_lookups ) {
	gcd[k++].creator = GListFieldCreate;
    } else
	gcd[k++].creator = GTextFieldCreate;

    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+30;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    label[k].text = (unichar_t *) _STR_Always;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.cid = CID_Always;
    gcd[k++].creator = GCheckBoxCreate;
    if ( which==mg_lookups )
	gcd[k-1].gd.flags = gg_enabled | gg_cb_on;

    label[k].text = (unichar_t *) _STR_IgnoreProblemFuture;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.pos.x = 10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+20;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    gcd[k].gd.cid = CID_Ignore;
    gcd[k++].creator = GCheckBoxCreate;

    gcd[k].gd.pos.x = 10-3; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+30 -3;
    gcd[k].gd.pos.width = -1;
    gcd[k].gd.flags = gg_visible | gg_but_default;
    label[k].text = (unichar_t *) _STR_Replace;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.handle_controlevent = MGA_Rpl;
    gcd[k].gd.cid = CID_Rpl;
    gcd[k++].creator = GButtonCreate;

    gcd[k].gd.pos.x = 10+blen+(ptwidth-3*blen-GGadgetScale(20))/2;
    gcd[k].gd.pos.y = gcd[k-1].gd.pos.y+3;
    gcd[k].gd.pos.width = -1;
    gcd[k].gd.flags = gg_visible | gg_enabled;
    label[k].text = (unichar_t *) _STR_Remove;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.handle_controlevent = MGA_Delete;
    gcd[k].gd.cid = CID_Delete;
    gcd[k++].creator = GButtonCreate;

    gcd[k].gd.pos.x = -10; gcd[k].gd.pos.y = gcd[k-1].gd.pos.y;
    gcd[k].gd.pos.width = -1;
    gcd[k].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[k].text = (unichar_t *) _STR_Skip;
    label[k].text_in_resource = true;
    gcd[k].gd.label = &label[k];
    gcd[k].gd.handle_controlevent = MGA_Skip;
    gcd[k].gd.cid = CID_Skip;
    gcd[k++].creator = GButtonCreate;

    gcd[k].gd.pos.x = 2; gcd[k].gd.pos.y = 2;
    gcd[k].gd.pos.width = pos.width-4; gcd[k].gd.pos.height = pos.height-4;
    gcd[k].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
    gcd[k++].creator = GGroupCreate;

    GGadgetsCreate(gw,gcd);
    if ( which!=mg_lookups )
	*end = end_ch;
    else {
	int searchtype;
	SplineFont *sf = p->fv!=NULL ? p->fv->sf : p->cv!=NULL ? p->cv->sc->parent : p->msc->parent;
	if ( fpst->type==pst_contextpos || fpst->type==pst_chainpos )
	    searchtype = fpst_max;		/* Search for positioning tables */
	else
	    searchtype = fpst_max+1;	/* Search for substitution tables */
	GGadgetSetList(gcd[rplpos].ret, SFGenTagListFromType(&sf->gentags,searchtype),false);
    }
    GDrawSetVisible(gw,true);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    if ( GGadgetIsChecked(GWidgetGetControl(gw,CID_Ignore)))
	p->missingglyph = false;
    GDrawDestroyWindow(gw);
return( !d.skipped );
}

static int StrMissingGlyph(struct problems *p,char **_str,SplineChar *sc,int which, void *data) {
    char *end, ch, *str = *_str, *new;
    int off;
    int found = false;
    SplineFont *sf = p->fv!=NULL ? p->fv->sf : p->cv!=NULL ? p->cv->sc->parent : p->msc->parent;
    SplineChar *ssc;
    int changed;

    if ( str==NULL )
return( false );

    while ( *str ) {
	if ( p->finish || !p->missingglyph )
    break;
	while ( *str==' ' ) ++str;
	for ( end=str; *end!='\0' && *end!=' '; ++end );
	ch = *end; *end='\0';
	ssc = SFGetChar(sf,-1,str);
	*end = ch;
	if ( ssc==NULL ) {
	    off = str-*_str;
	    if ( (new = missinglookup(p,str))!=NULL ) {
		mgreplace(_str, str,end, new, sc, which==mg_pst ? data : NULL);
		changed = true;
	    } else {
		if ( mgAsk(p,_str,str,end,0,sc,which,data))
		    changed = true;
		found = true;
	    }
	    if ( changed ) {
		PST *test;
		if ( which==mg_pst ) {
		    for ( test = sc->possub; test!=NULL && test!=data; test=test->next );
		    if ( test==NULL )		/* Entire pst was removed */
return( true );
		}
		end = *_str+off;
	    }
	}
	str = end;
    }
return( found );
}
	
static int SCMissingGlyph(struct problems *p,SplineChar *sc) {
    PST *pst, *next;
    int found = false;

    if ( !p->missingglyph || p->finish || sc==NULL )
return( false );

    for ( pst=sc->possub; pst!=NULL; pst=next ) {
	next = pst->next;
	switch ( pst->type ) {
	  case pst_pair:
	    found |= StrMissingGlyph(p,&pst->u.pair.paired,sc,mg_pst,pst);
	  break;
	  case pst_substitution:
	  case pst_alternate:
	  case pst_multiple:
	  case pst_ligature:
	    found |= StrMissingGlyph(p,&pst->u.subs.variant,sc,mg_pst,pst);
	  break;
	}
    }
return( found );
}

static int KCMissingGlyph(struct problems *p,KernClass *kc,int isv) {
    int i;
    int found = false;
    int which = isv ? mg_vkern : mg_kern;

    for ( i=1; i<kc->first_cnt; ++i )
	found |= StrMissingGlyph(p,&kc->firsts[i],NULL,which,kc);
    for ( i=1; i<kc->second_cnt; ++i )
	found |= StrMissingGlyph(p,&kc->seconds[i],NULL,which,kc);
return( found );
}

static int FPSTMissingGlyph(struct problems *p,FPST *fpst) {
    int i,j;
    int found = false;

    switch ( fpst->format ) {
      case pst_glyphs:
	for ( i=0; i<fpst->rule_cnt; ++i )
	    for ( j=0; j<3; ++j )
		found |= StrMissingGlyph(p,&(&fpst->rules[i].u.glyph.names)[j],
			NULL,mg_fpst,fpst);
      break;
      case pst_class:
	for ( i=1; i<3; ++i )
	    for ( j=0; j<(&fpst->nccnt)[i]; ++j )
		found |= StrMissingGlyph(p,&(&fpst->nclass)[i][j],NULL,mg_fpst,fpst);
      break;
      case pst_reversecoverage:
	found |= StrMissingGlyph(p,&fpst->rules[0].u.rcoverage.replacements,NULL,mg_fpst,fpst);
	/* fall through */;
      case pst_coverage:
	for ( i=1; i<3; ++i )
	    for ( j=0; j<(&fpst->rules[0].u.coverage.ncnt)[i]; ++j )
		found |= StrMissingGlyph(p,&(&fpst->rules[0].u.coverage.ncovers)[i][j],NULL,mg_fpst,fpst);
      break;
    }
return( found );
}

static int FPSTMissingLookups(struct problems *p,FPST *fpst) {
    int i, j;
    int ispos = (fpst->type==pst_contextpos || fpst->type==pst_chainpos);
    SplineFont *sf = p->fv!=NULL ? p->fv->sf : p->cv!=NULL ? p->cv->sc->parent : p->msc->parent;
    int found = false;
    uint32 new;

    if ( fpst->type==pst_reversesub )		/* No other lookups needed */
return( false );

    for ( i=0; i<fpst->rule_cnt ; ++i ) {
	struct fpst_rule *r = &fpst->rules[i];
	for ( j=0; j<r->lookup_cnt; ++j ) {
	    if ( !SFHasNestedLookupWithTag(sf,r->lookups[j].lookup_tag,ispos) ) {
		found = true;
		if ( (new = missinglookup_tag(p,r->lookups[j].lookup_tag))==0 ) {
		    if ( !mgAsk(p,NULL,NULL,NULL,r->lookups[j].lookup_tag,NULL,mg_lookups,fpst))
	continue;
		}
		if ( (new = missinglookup_tag(p,r->lookups[j].lookup_tag))!=0 ) {
		    if ( new==(uint32) -1 ) {
			if ( j!=r->lookup_cnt-1 )
			    memmove(&r->lookups[j],&r->lookups[j+1],r->lookup_cnt-j-1);
			--r->lookup_cnt;
		    } else
			r->lookups[j].lookup_tag = new;
		    --j;
		}
	    }
	}
    }
return( found );
}

static int CheckForATT(struct problems *p) {
    int found = false;
    int i,k;
    FPST *fpst;
    KernClass *kc;
    SplineFont *_sf, *sf;

    _sf = p->fv->sf;
    if ( _sf->cidmaster ) _sf = _sf->cidmaster;

    if ( p->missingglyph && !p->finish ) {
	if ( p->cv!=NULL )
	    found = SCMissingGlyph(p,p->cv->sc);
	else if ( p->msc!=NULL )
	    found = SCMissingGlyph(p,p->msc);
	else {
	    k=0;
	    do {
		if ( _sf->subfonts==NULL ) sf = _sf;
		else sf = _sf->subfonts[k++];
		for ( i=0; i<sf->charcnt && !p->finish; ++i ) if ( sf->chars[i]!=NULL )
		    found |= SCMissingGlyph(p,sf->chars[i]);
	    } while ( k<_sf->subfontcnt && !p->finish );
	    for ( kc=_sf->kerns; kc!=NULL && !p->finish; kc=kc->next )
		found |= KCMissingGlyph(p,kc,false);
	    for ( kc=_sf->vkerns; kc!=NULL && !p->finish; kc=kc->next )
		found |= KCMissingGlyph(p,kc,true);
	    for ( fpst=_sf->possub; fpst!=NULL && !p->finish && p->missingglyph; fpst=fpst->next )
		found |= FPSTMissingGlyph(p,fpst);
	}
	ClearMissingState(p);
    }

    if ( p->missinglookuptag && !p->finish ) {
	for ( fpst=_sf->possub; fpst!=NULL && !p->finish && p->missinglookuptag; fpst=fpst->next )
	    found |= FPSTMissingLookups(p,fpst);
	ClearMissingState(p);
    }
return( found );
}

static void DoProbs(struct problems *p) {
    int i, ret=false;
    SplineChar *sc;
    BDFFont *bdf;

    if ( p->missingglyph || p->missinglookuptag )
	ret = CheckForATT(p);
    if ( p->cv!=NULL ) {
	ret |= SCProblems(p->cv,NULL,p);
	ret |= CIDCheck(p,p->cv->sc->enc);
    } else if ( p->msc!=NULL ) {
	ret |= SCProblems(NULL,p->msc,p);
	ret |= CIDCheck(p,p->msc->enc);
    } else {
	for ( i=0; i<p->fv->sf->charcnt && !p->finish; ++i )
	    if ( p->fv->selected[i] ) {
		if ( (sc = p->fv->sf->chars[i])!=NULL ) {
		    if ( SCProblems(NULL,sc,p)) {
			if ( sc!=p->lastcharopened ) {
			    if ( sc->views!=NULL )
				GDrawRaise(sc->views->gw);
			    else
				CharViewCreate(sc,p->fv);
			    p->lastcharopened = sc;
			}
			ret = true;
		    }
		}
		if ( !p->finish && p->bitmaps && !SCWorthOutputting(sc)) {
		    for ( bdf=p->fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
			if ( i<bdf->charcnt && bdf->chars[i]!=NULL ) {
			    sc = SFMakeChar(p->fv->sf,i);
			    ExplainIt(p,sc,_STR_ProbMissingOutline,0,0);
			    ret = true;
			}
		}
		ret |= CIDCheck(p,i);
	    }
    }
    if ( !ret )
	GDrawError( "No problems found");
}

static void FigureStandardHeights(struct problems *p) {
    BlueData bd;

    QuickBlues(p->fv->sf,&bd);
    p->xheight = bd.xheight;
    p->caph = bd.caph;
    p->ascent = bd.ascent;
    p->descent = bd.descent;
}

static int Prob_DoAll(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct problems *p = GDrawGetUserData(GGadgetGetWindow(g));
	int set = GGadgetGetCid(g)==CID_SetAll;
	GWindow gw = GGadgetGetWindow(g);
	static int cbs[] = { CID_OpenPaths, CID_PointsTooClose, CID_XNear,
	    CID_YNear, CID_YNearStd, CID_HintNoPt, CID_PtNearHint,
	    CID_HintWidthNear, CID_LineStd, CID_Direction, CID_CpStd,
	    CID_CpOdd, CID_FlippedRefs, CID_Bitmaps, CID_AdvanceWidth,
	    CID_MissingGlyph, CID_MissingLookupTag,
	    CID_Stem3, CID_IrrelevantCP, CID_TooManyPoints,
	    0 };
	int i;
	if ( p->fv->cidmaster!=NULL ) {
	    GGadgetSetChecked(GWidgetGetControl(gw,CID_CIDMultiple),set);
	    GGadgetSetChecked(GWidgetGetControl(gw,CID_CIDBlank),set);
	}
	if ( p->fv->sf->hasvmetrics )
	    GGadgetSetChecked(GWidgetGetControl(gw,CID_VAdvanceWidth),set);
	for ( i=0; cbs[i]!=0; ++i )
	    GGadgetSetChecked(GWidgetGetControl(gw,cbs[i]),set);
    }
return( true );
}

static int Prob_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	GWindow gw = GGadgetGetWindow(g);
	struct problems *p = GDrawGetUserData(gw);
	int errs = false;

	openpaths = p->openpaths = GGadgetIsChecked(GWidgetGetControl(gw,CID_OpenPaths));
	pointstooclose = p->pointstooclose = GGadgetIsChecked(GWidgetGetControl(gw,CID_PointsTooClose));
	/*missing = p->missingextrema = GGadgetIsChecked(GWidgetGetControl(gw,CID_MissingExtrema))*/;
	doxnear = p->xnearval = GGadgetIsChecked(GWidgetGetControl(gw,CID_XNear));
	doynear = p->ynearval = GGadgetIsChecked(GWidgetGetControl(gw,CID_YNear));
	doynearstd = p->ynearstd = GGadgetIsChecked(GWidgetGetControl(gw,CID_YNearStd));
	linestd = p->linenearstd = GGadgetIsChecked(GWidgetGetControl(gw,CID_LineStd));
	cpstd = p->cpnearstd = GGadgetIsChecked(GWidgetGetControl(gw,CID_CpStd));
	cpodd = p->cpodd = GGadgetIsChecked(GWidgetGetControl(gw,CID_CpOdd));
	hintnopt = p->hintwithnopt = GGadgetIsChecked(GWidgetGetControl(gw,CID_HintNoPt));
	ptnearhint = p->ptnearhint = GGadgetIsChecked(GWidgetGetControl(gw,CID_PtNearHint));
	hintwidth = p->hintwidthnearval = GGadgetIsChecked(GWidgetGetControl(gw,CID_HintWidthNear));
	direction = p->direction = GGadgetIsChecked(GWidgetGetControl(gw,CID_Direction));
	flippedrefs = p->flippedrefs = GGadgetIsChecked(GWidgetGetControl(gw,CID_FlippedRefs));
	bitmaps = p->bitmaps = GGadgetIsChecked(GWidgetGetControl(gw,CID_Bitmaps));
	advancewidth = p->advancewidth = GGadgetIsChecked(GWidgetGetControl(gw,CID_AdvanceWidth));
	irrelevantcp = p->irrelevantcontrolpoints = GGadgetIsChecked(GWidgetGetControl(gw,CID_IrrelevantCP));
	badsubs = p->badsubs = GGadgetIsChecked(GWidgetGetControl(gw,CID_BadSubs));
	missingglyph = p->missingglyph = GGadgetIsChecked(GWidgetGetControl(gw,CID_MissingGlyph));
	missinglookuptag = p->missinglookuptag = GGadgetIsChecked(GWidgetGetControl(gw,CID_MissingLookupTag));
	toomanypoints = p->toomanypoints = GGadgetIsChecked(GWidgetGetControl(gw,CID_TooManyPoints));
	stem3 = p->stem3 = GGadgetIsChecked(GWidgetGetControl(gw,CID_Stem3));
	if ( stem3 )
	    showexactstem3 = p->showexactstem3 = GGadgetIsChecked(GWidgetGetControl(gw,CID_ShowExactStem3));
	if ( p->fv->cidmaster!=NULL ) {
	    cidmultiple = p->cidmultiple = GGadgetIsChecked(GWidgetGetControl(gw,CID_CIDMultiple));
	    cidblank = p->cidblank = GGadgetIsChecked(GWidgetGetControl(gw,CID_CIDBlank));
	}
	if ( p->fv->sf->hasvmetrics ) {
	    vadvancewidth = p->vadvancewidth = GGadgetIsChecked(GWidgetGetControl(gw,CID_VAdvanceWidth));
	} else
	    p->vadvancewidth = false;
	p->explain = true;
	if ( doxnear )
	    p->xval = xval = GetRealR(gw,CID_XNearVal,_STR_XNear,&errs);
	if ( doynear )
	    p->yval = yval = GetRealR(gw,CID_YNearVal,_STR_YNear,&errs);
	if ( hintwidth )
	    widthval = p->widthval = GetRealR(gw,CID_HintWidth,_STR_HintWidth,&errs);
	if ( p->advancewidth )
	    advancewidthval = p->advancewidthval = GetIntR(gw,CID_AdvanceWidthVal,_STR_HintWidth,&errs);
	if ( p->vadvancewidth )
	    vadvancewidthval = p->vadvancewidthval = GetIntR(gw,CID_VAdvanceWidthVal,_STR_HintWidth,&errs);
	if ( toomanypoints )
	    p->pointsmax = pointsmax = GetIntR(gw,CID_PointsMax,_STR_MorePointsThan,&errs);
	if ( irrelevantcp )
	    p->irrelevantfactor = irrelevantfactor = GetRealR(gw,CID_IrrelevantFactor,_STR_IrrelevantFactor,&errs)/100.0;
	near = p->near = GetRealR(gw,CID_Near,_STR_Near,&errs);
	if ( errs )
return( true );
	if ( doynearstd )
	    FigureStandardHeights(p);
	GDrawSetVisible(gw,false);
	if ( openpaths || pointstooclose /*|| missing*/ || doxnear || doynear ||
		doynearstd || linestd || hintnopt || ptnearhint || hintwidth ||
		direction || p->cidmultiple || p->cidblank || p->flippedrefs ||
		p->bitmaps || p->advancewidth || p->vadvancewidth || p->stem3 ||
		p->irrelevantcontrolpoints || p->badsubs || p->missingglyph ||
		p->missinglookuptag || p->toomanypoints ) {
	    DoProbs(p);
	}
	p->done = true;
    }
return( true );
}

static int Prob_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct problems *p = GDrawGetUserData(GGadgetGetWindow(g));
	p->done = true;
    }
return( true );
}

static int Prob_TextChanged(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_textchanged ) {
	GGadgetSetChecked(GWidgetGetControl(GGadgetGetWindow(g),(int) GGadgetGetUserData(g)),true);
    }
return( true );
}

static int Prob_EnableExact(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_radiochanged ) {
	GGadgetSetEnabled(GWidgetGetControl(GGadgetGetWindow(g),CID_ShowExactStem3),
		GGadgetIsChecked(g));
    }
return( true );
}

static int e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	struct problems *p = GDrawGetUserData(gw);
	p->done = true;
    }
return( event->type!=et_char );
}

void FindProblems(FontView *fv,CharView *cv, SplineChar *sc) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData pgcd[13], pagcd[7], hgcd[7], rgcd[7], cgcd[5], mgcd[10], agcd[5];
    GTextInfo plabel[13], palabel[7], hlabel[7], rlabel[7], clabel[5], mlabel[10], alabel[5];
    GTabInfo aspects[9];
    struct problems p;
    char xnbuf[20], ynbuf[20], widthbuf[20], nearbuf[20], awidthbuf[20],
	    vawidthbuf[20], irrel[20], pmax[20];
    int i;
    SplineFont *sf;
    /*static GBox smallbox = { bt_raised, bs_rect, 2, 1, 0, 0, 0,0,0,0, COLOR_DEFAULT,COLOR_DEFAULT };*/

    memset(&p,0,sizeof(p));
    if ( fv==NULL ) fv = cv->fv;
    p.fv = fv; p.cv=cv; p.msc = sc;
    if ( cv!=NULL )
	p.lastcharopened = cv->sc;

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = GStringGetResource(_STR_Findprobs,NULL);
    pos.x = pos.y = 0;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,218));
    pos.height = GDrawPointsToPixels(NULL,294);
    gw = GDrawCreateTopWindow(NULL,&pos,e_h,&p,&wattrs);

    memset(&plabel,0,sizeof(plabel));
    memset(&pgcd,0,sizeof(pgcd));

    plabel[0].text = (unichar_t *) _STR_Points2Close;
    plabel[0].text_in_resource = true;
    pgcd[0].gd.label = &plabel[0];
    pgcd[0].gd.mnemonic = 't';
    pgcd[0].gd.pos.x = 3; pgcd[0].gd.pos.y = 5; 
    pgcd[0].gd.flags = gg_visible | gg_enabled;
    if ( pointstooclose ) pgcd[0].gd.flags |= gg_cb_on;
    pgcd[0].gd.popup_msg = GStringGetResource(_STR_Points2ClosePopup,NULL);
    pgcd[0].gd.cid = CID_PointsTooClose;
    pgcd[0].creator = GCheckBoxCreate;

    plabel[1].text = (unichar_t *) _STR_XNear;
    plabel[1].text_in_resource = true;
    pgcd[1].gd.label = &plabel[1];
    pgcd[1].gd.mnemonic = 'X';
    pgcd[1].gd.pos.x = 3; pgcd[1].gd.pos.y = pgcd[0].gd.pos.y+19; 
    pgcd[1].gd.flags = gg_visible | gg_enabled;
    if ( doxnear ) pgcd[1].gd.flags |= gg_cb_on;
    pgcd[1].gd.popup_msg = GStringGetResource(_STR_XNearPopup,NULL);
    pgcd[1].gd.cid = CID_XNear;
    pgcd[1].creator = GCheckBoxCreate;

    sprintf(xnbuf,"%g",xval);
    plabel[2].text = (unichar_t *) xnbuf;
    plabel[2].text_is_1byte = true;
    pgcd[2].gd.label = &plabel[2];
    pgcd[2].gd.pos.x = 60; pgcd[2].gd.pos.y = pgcd[1].gd.pos.y-1; pgcd[2].gd.pos.width = 40;
    pgcd[2].gd.flags = gg_visible | gg_enabled;
    pgcd[2].gd.cid = CID_XNearVal;
    pgcd[2].gd.handle_controlevent = Prob_TextChanged;
    pgcd[2].data = (void *) CID_XNear;
    pgcd[2].creator = GTextFieldCreate;

    plabel[3].text = (unichar_t *) _STR_YNear;
    plabel[3].text_in_resource = true;
    pgcd[3].gd.label = &plabel[3];
    pgcd[3].gd.mnemonic = 'Y';
    pgcd[3].gd.pos.x = 3; pgcd[3].gd.pos.y = pgcd[1].gd.pos.y+26; 
    pgcd[3].gd.flags = gg_visible | gg_enabled;
    if ( doynear ) pgcd[3].gd.flags |= gg_cb_on;
    pgcd[3].gd.popup_msg = GStringGetResource(_STR_YNearPopup,NULL);
    pgcd[3].gd.cid = CID_YNear;
    pgcd[3].creator = GCheckBoxCreate;

    sprintf(ynbuf,"%g",yval);
    plabel[4].text = (unichar_t *) ynbuf;
    plabel[4].text_is_1byte = true;
    pgcd[4].gd.label = &plabel[4];
    pgcd[4].gd.pos.x = 60; pgcd[4].gd.pos.y = pgcd[3].gd.pos.y-1; pgcd[4].gd.pos.width = 40;
    pgcd[4].gd.flags = gg_visible | gg_enabled;
    pgcd[4].gd.cid = CID_YNearVal;
    pgcd[4].gd.handle_controlevent = Prob_TextChanged;
    pgcd[4].data = (void *) CID_YNear;
    pgcd[4].creator = GTextFieldCreate;

    plabel[5].text = (unichar_t *) _STR_YNearStd;
    plabel[5].text_in_resource = true;
    pgcd[5].gd.label = &plabel[5];
    pgcd[5].gd.mnemonic = 'S';
    pgcd[5].gd.pos.x = 3; pgcd[5].gd.pos.y = pgcd[3].gd.pos.y+20; 
    pgcd[5].gd.flags = gg_visible | gg_enabled;
    if ( doynearstd ) pgcd[5].gd.flags |= gg_cb_on;
    pgcd[5].gd.popup_msg = GStringGetResource(_STR_YNearStdPopup,NULL);
    pgcd[5].gd.cid = CID_YNearStd;
    pgcd[5].creator = GCheckBoxCreate;

    plabel[6].text = (unichar_t *) (fv->sf->italicangle==0?_STR_CpStd:_STR_CpStd2);
    plabel[6].text_in_resource = true;
    pgcd[6].gd.label = &plabel[6];
    pgcd[6].gd.mnemonic = 'C';
    pgcd[6].gd.pos.x = 3; pgcd[6].gd.pos.y = pgcd[5].gd.pos.y+15; 
    pgcd[6].gd.flags = gg_visible | gg_enabled;
    if ( cpstd ) pgcd[6].gd.flags |= gg_cb_on;
    pgcd[6].gd.popup_msg = GStringGetResource(_STR_CpStdPopup,NULL);
    pgcd[6].gd.cid = CID_CpStd;
    pgcd[6].creator = GCheckBoxCreate;

    plabel[7].text = (unichar_t *) _STR_CpOdd;
    plabel[7].text_in_resource = true;
    pgcd[7].gd.label = &plabel[7];
    pgcd[7].gd.mnemonic = 'b';
    pgcd[7].gd.pos.x = 3; pgcd[7].gd.pos.y = pgcd[6].gd.pos.y+15; 
    pgcd[7].gd.flags = gg_visible | gg_enabled;
    if ( cpodd ) pgcd[7].gd.flags |= gg_cb_on;
    pgcd[7].gd.popup_msg = GStringGetResource(_STR_CpOddPopup,NULL);
    pgcd[7].gd.cid = CID_CpOdd;
    pgcd[7].creator = GCheckBoxCreate;

    plabel[8].text = (unichar_t *) _STR_IrrelevantCP;
    plabel[8].text_in_resource = true;
    pgcd[8].gd.label = &plabel[8];
    pgcd[8].gd.pos.x = 3; pgcd[8].gd.pos.y = pgcd[7].gd.pos.y+15; 
    pgcd[8].gd.flags = gg_visible | gg_enabled;
    if ( irrelevantcp ) pgcd[8].gd.flags |= gg_cb_on;
    pgcd[8].gd.popup_msg = GStringGetResource(_STR_IrrelevantCPPopup,NULL);
    pgcd[8].gd.cid = CID_IrrelevantCP;
    pgcd[8].creator = GCheckBoxCreate;

    plabel[9].text = (unichar_t *) _STR_IrrelevantFactor;
    plabel[9].text_in_resource = true;
    pgcd[9].gd.label = &plabel[9];
    pgcd[9].gd.pos.x = 20; pgcd[9].gd.pos.y = pgcd[8].gd.pos.y+20; 
    pgcd[9].gd.flags = gg_visible | gg_enabled;
    pgcd[9].gd.popup_msg = GStringGetResource(_STR_IrrelevantFactorPopup,NULL);
    pgcd[9].creator = GLabelCreate;

    sprintf( irrel, "%g", irrelevantfactor*100 );
    plabel[10].text = (unichar_t *) irrel;
    plabel[10].text_is_1byte = true;
    pgcd[10].gd.label = &plabel[10];
    pgcd[10].gd.pos.x = 105; pgcd[10].gd.pos.y = pgcd[9].gd.pos.y-3;
    pgcd[10].gd.pos.width = 50; 
    pgcd[10].gd.flags = gg_visible | gg_enabled;
    pgcd[10].gd.popup_msg = GStringGetResource(_STR_IrrelevantFactorPopup,NULL);
    pgcd[10].gd.cid = CID_IrrelevantFactor;
    pgcd[10].creator = GTextFieldCreate;

    plabel[11].text = (unichar_t *) "%";
    plabel[11].text_is_1byte = true;
    pgcd[11].gd.label = &plabel[11];
    pgcd[11].gd.pos.x = 163; pgcd[11].gd.pos.y = pgcd[9].gd.pos.y; 
    pgcd[11].gd.flags = gg_visible | gg_enabled;
    pgcd[11].gd.popup_msg = GStringGetResource(_STR_IrrelevantFactorPopup,NULL);
    pgcd[11].creator = GLabelCreate;

/* ************************************************************************** */

    memset(&palabel,0,sizeof(palabel));
    memset(&pagcd,0,sizeof(pagcd));

    palabel[0].text = (unichar_t *) _STR_OpenPaths;
    palabel[0].text_in_resource = true;
    pagcd[0].gd.label = &palabel[0];
    pagcd[0].gd.mnemonic = 'P';
    pagcd[0].gd.pos.x = 3; pagcd[0].gd.pos.y = 6; 
    pagcd[0].gd.flags = gg_visible | gg_enabled;
    if ( openpaths ) pagcd[0].gd.flags |= gg_cb_on;
    pagcd[0].gd.popup_msg = GStringGetResource(_STR_OpenPathsPopup,NULL);
    pagcd[0].gd.cid = CID_OpenPaths;
    pagcd[0].creator = GCheckBoxCreate;

    palabel[1].text = (unichar_t *) (fv->sf->italicangle==0?_STR_LineStd:_STR_LineStd2);
    palabel[1].text_in_resource = true;
    pagcd[1].gd.label = &palabel[1];
    pagcd[1].gd.mnemonic = 'E';
    pagcd[1].gd.pos.x = 3; pagcd[1].gd.pos.y = pagcd[0].gd.pos.y+17; 
    pagcd[1].gd.flags = gg_visible | gg_enabled;
    if ( linestd ) pagcd[1].gd.flags |= gg_cb_on;
    pagcd[1].gd.popup_msg = GStringGetResource(_STR_LineStdPopup,NULL);
    pagcd[1].gd.cid = CID_LineStd;
    pagcd[1].creator = GCheckBoxCreate;

    palabel[2].text = (unichar_t *) _STR_CheckDirection;
    palabel[2].text_in_resource = true;
    pagcd[2].gd.label = &palabel[2];
    pagcd[2].gd.mnemonic = 'S';
    pagcd[2].gd.pos.x = 3; pagcd[2].gd.pos.y = pagcd[1].gd.pos.y+17; 
    pagcd[2].gd.flags = gg_visible | gg_enabled;
    if ( direction ) pagcd[2].gd.flags |= gg_cb_on;
    pagcd[2].gd.popup_msg = GStringGetResource(_STR_CheckDirectionPopup,NULL);
    pagcd[2].gd.cid = CID_Direction;
    pagcd[2].creator = GCheckBoxCreate;

    palabel[3].text = (unichar_t *) _STR_CheckFlippedRefs;
    palabel[3].text_in_resource = true;
    pagcd[3].gd.label = &palabel[3];
    pagcd[3].gd.mnemonic = 'r';
    pagcd[3].gd.pos.x = 3; pagcd[3].gd.pos.y = pagcd[2].gd.pos.y+17; 
    pagcd[3].gd.flags = gg_visible | gg_enabled;
    if ( flippedrefs ) pagcd[3].gd.flags |= gg_cb_on;
    pagcd[3].gd.popup_msg = GStringGetResource(_STR_CheckFlippedRefsPopup,NULL);
    pagcd[3].gd.cid = CID_FlippedRefs;
    pagcd[3].creator = GCheckBoxCreate;

    palabel[4].text = (unichar_t *) _STR_MorePointsThan;
    palabel[4].text_in_resource = true;
    pagcd[4].gd.label = &palabel[4];
    pagcd[4].gd.mnemonic = 'r';
    pagcd[4].gd.pos.x = 3; pagcd[4].gd.pos.y = pagcd[3].gd.pos.y+21; 
    pagcd[4].gd.flags = gg_visible | gg_enabled;
    if ( toomanypoints ) pagcd[4].gd.flags |= gg_cb_on;
    pagcd[4].gd.popup_msg = GStringGetResource(_STR_MorePointsThanPopup,NULL);
    pagcd[4].gd.cid = CID_TooManyPoints;
    pagcd[4].creator = GCheckBoxCreate;

    sprintf( pmax, "%d", pointsmax );
    palabel[5].text = (unichar_t *) pmax;
    palabel[5].text_is_1byte = true;
    pagcd[5].gd.label = &palabel[5];
    pagcd[5].gd.pos.x = 105; pagcd[5].gd.pos.y = pagcd[4].gd.pos.y-3;
    pagcd[5].gd.pos.width = 50; 
    pagcd[5].gd.flags = gg_visible | gg_enabled;
    pagcd[5].gd.popup_msg = GStringGetResource(_STR_MorePointsThanPopup,NULL);
    pagcd[5].gd.cid = CID_PointsMax;
    pagcd[5].creator = GTextFieldCreate;

/* ************************************************************************** */

    memset(&hlabel,0,sizeof(hlabel));
    memset(&hgcd,0,sizeof(hgcd));

    hlabel[0].text = (unichar_t *) _STR_HintNoPt;
    hlabel[0].text_in_resource = true;
    hgcd[0].gd.label = &hlabel[0];
    hgcd[0].gd.mnemonic = 'H';
    hgcd[0].gd.pos.x = 3; hgcd[0].gd.pos.y = 5; 
    hgcd[0].gd.flags = gg_visible | gg_enabled;
    if ( hintnopt ) hgcd[0].gd.flags |= gg_cb_on;
    hgcd[0].gd.popup_msg = GStringGetResource(_STR_HintNoPtPopup,NULL);
    hgcd[0].gd.cid = CID_HintNoPt;
    hgcd[0].creator = GCheckBoxCreate;

    hlabel[1].text = (unichar_t *) _STR_PtNearHint;
    hlabel[1].text_in_resource = true;
    hgcd[1].gd.label = &hlabel[1];
    hgcd[1].gd.mnemonic = 'H';
    hgcd[1].gd.pos.x = 3; hgcd[1].gd.pos.y = hgcd[0].gd.pos.y+17; 
    hgcd[1].gd.flags = gg_visible | gg_enabled;
    if ( ptnearhint ) hgcd[1].gd.flags |= gg_cb_on;
    hgcd[1].gd.popup_msg = GStringGetResource(_STR_PtNearHintPopup,NULL);
    hgcd[1].gd.cid = CID_PtNearHint;
    hgcd[1].creator = GCheckBoxCreate;

    hlabel[2].text = (unichar_t *) _STR_HintWidth;
    hlabel[2].text_in_resource = true;
    hgcd[2].gd.label = &hlabel[2];
    hgcd[2].gd.mnemonic = 'W';
    hgcd[2].gd.pos.x = 3; hgcd[2].gd.pos.y = hgcd[1].gd.pos.y+21;
    hgcd[2].gd.flags = gg_visible | gg_enabled;
    if ( hintwidth ) hgcd[2].gd.flags |= gg_cb_on;
    hgcd[2].gd.popup_msg = GStringGetResource(_STR_HintWidthPopup,NULL);
    hgcd[2].gd.cid = CID_HintWidthNear;
    hgcd[2].creator = GCheckBoxCreate;

    sprintf(widthbuf,"%g",widthval);
    hlabel[3].text = (unichar_t *) widthbuf;
    hlabel[3].text_is_1byte = true;
    hgcd[3].gd.label = &hlabel[3];
    hgcd[3].gd.pos.x = 100+5; hgcd[3].gd.pos.y = hgcd[2].gd.pos.y-1; hgcd[3].gd.pos.width = 40;
    hgcd[3].gd.flags = gg_visible | gg_enabled;
    hgcd[3].gd.cid = CID_HintWidth;
    hgcd[3].gd.handle_controlevent = Prob_TextChanged;
    hgcd[3].data = (void *) CID_HintWidthNear;
    hgcd[3].creator = GTextFieldCreate;

    hlabel[4].text = (unichar_t *) _STR_Hint3;
    hlabel[4].text_in_resource = true;
    hgcd[4].gd.label = &hlabel[4];
    hgcd[4].gd.mnemonic = '3';
    hgcd[4].gd.pos.x = 3; hgcd[4].gd.pos.y = hgcd[3].gd.pos.y+19;
    hgcd[4].gd.flags = gg_visible | gg_enabled;
    if ( stem3 ) hgcd[4].gd.flags |= gg_cb_on;
    hgcd[4].gd.popup_msg = GStringGetResource(_STR_Hint3Popup,NULL);
    hgcd[4].gd.cid = CID_Stem3;
    hgcd[4].gd.handle_controlevent = Prob_EnableExact;
    hgcd[4].creator = GCheckBoxCreate;

    hlabel[5].text = (unichar_t *) _STR_ShowExactHint3;
    hlabel[5].text_in_resource = true;
    hgcd[5].gd.label = &hlabel[5];
    hgcd[5].gd.mnemonic = 'S';
    hgcd[5].gd.pos.x = hgcd[4].gd.pos.x+5; hgcd[5].gd.pos.y = hgcd[4].gd.pos.y+17;
    hgcd[5].gd.flags = gg_visible;
    if ( showexactstem3 ) hgcd[5].gd.flags |= gg_cb_on;
    if ( stem3 ) hgcd[5].gd.flags |= gg_enabled;
    hgcd[5].gd.popup_msg = GStringGetResource(_STR_ShowExactHint3Popup,NULL);
    hgcd[5].gd.cid = CID_ShowExactStem3;
    hgcd[5].creator = GCheckBoxCreate;

/* ************************************************************************** */

    memset(&rlabel,0,sizeof(rlabel));
    memset(&rgcd,0,sizeof(rgcd));

    rlabel[0].text = (unichar_t *) _STR_CheckBitmaps;
    rlabel[0].text_in_resource = true;
    rgcd[0].gd.label = &rlabel[0];
    rgcd[0].gd.mnemonic = 'r';
    rgcd[0].gd.pos.x = 3; rgcd[0].gd.pos.y = 6; 
    rgcd[0].gd.flags = gg_visible | gg_enabled;
    if ( bitmaps ) rgcd[0].gd.flags |= gg_cb_on;
    rgcd[0].gd.popup_msg = GStringGetResource(_STR_CheckBitmapsPopup,NULL);
    rgcd[0].gd.cid = CID_Bitmaps;
    rgcd[0].creator = GCheckBoxCreate;

    rlabel[1].text = (unichar_t *) _STR_AdvanceWidth;
    rlabel[1].text_in_resource = true;
    rgcd[1].gd.label = &rlabel[1];
    rgcd[1].gd.mnemonic = 'W';
    rgcd[1].gd.pos.x = 3; rgcd[1].gd.pos.y = rgcd[0].gd.pos.y+21;
    rgcd[1].gd.flags = gg_visible | gg_enabled;
    if ( advancewidth ) rgcd[1].gd.flags |= gg_cb_on;
    rgcd[1].gd.popup_msg = GStringGetResource(_STR_AdvanceWidthPopup,NULL);
    rgcd[1].gd.cid = CID_AdvanceWidth;
    rgcd[1].creator = GCheckBoxCreate;

    sf = p.fv->sf;

    if ( advancewidthval==0 && sf->chars[' ']!=NULL )
	advancewidthval = sf->chars[' ']->width;
    sprintf(awidthbuf,"%g",advancewidthval);
    rlabel[2].text = (unichar_t *) awidthbuf;
    rlabel[2].text_is_1byte = true;
    rgcd[2].gd.label = &rlabel[2];
    rgcd[2].gd.pos.x = 100+15; rgcd[2].gd.pos.y = rgcd[1].gd.pos.y-1; rgcd[2].gd.pos.width = 40;
    rgcd[2].gd.flags = gg_visible | gg_enabled;
    rgcd[2].gd.cid = CID_AdvanceWidthVal;
    rgcd[2].gd.handle_controlevent = Prob_TextChanged;
    rgcd[2].data = (void *) CID_AdvanceWidth;
    rgcd[2].creator = GTextFieldCreate;

    rlabel[3].text = (unichar_t *) _STR_AdvanceVWidth;
    rlabel[3].text_in_resource = true;
    rgcd[3].gd.label = &rlabel[3];
    rgcd[3].gd.mnemonic = 'W';
    rgcd[3].gd.pos.x = 3; rgcd[3].gd.pos.y = rgcd[2].gd.pos.y+24;
    rgcd[3].gd.flags = gg_visible | gg_enabled;
    if ( !sf->hasvmetrics ) rgcd[3].gd.flags = gg_visible;
    else if ( vadvancewidth ) rgcd[3].gd.flags |= gg_cb_on;
    rgcd[3].gd.popup_msg = GStringGetResource(_STR_AdvanceVWidthPopup,NULL);
    rgcd[3].gd.cid = CID_VAdvanceWidth;
    rgcd[3].creator = GCheckBoxCreate;

    if ( vadvancewidth==0 ) vadvancewidth = sf->ascent+sf->descent;
    sprintf(vawidthbuf,"%g",vadvancewidthval);
    rlabel[4].text = (unichar_t *) vawidthbuf;
    rlabel[4].text_is_1byte = true;
    rgcd[4].gd.label = &rlabel[4];
    rgcd[4].gd.pos.x = 100+15; rgcd[4].gd.pos.y = rgcd[3].gd.pos.y-1; rgcd[4].gd.pos.width = 40;
    rgcd[4].gd.flags = gg_visible | gg_enabled;
    if ( !sf->hasvmetrics ) rgcd[4].gd.flags = gg_visible;
    rgcd[4].gd.cid = CID_VAdvanceWidthVal;
    rgcd[4].gd.handle_controlevent = Prob_TextChanged;
    rgcd[4].data = (void *) CID_VAdvanceWidth;
    rgcd[4].creator = GTextFieldCreate;

    rlabel[5].text = (unichar_t *) _STR_SubsToEmptyChar;
    rlabel[5].text_in_resource = true;
    rgcd[5].gd.label = &rlabel[5];
    rgcd[5].gd.pos.x = 3; rgcd[5].gd.pos.y = rgcd[4].gd.pos.y+24; 
    rgcd[5].gd.flags = gg_visible | gg_enabled;
    if ( badsubs ) rgcd[5].gd.flags |= gg_cb_on;
    rgcd[5].gd.popup_msg = GStringGetResource(_STR_SubsToEmptyCharPopup,NULL);
    rgcd[5].gd.cid = CID_BadSubs;
    rgcd[5].creator = GCheckBoxCreate;

/* ************************************************************************** */

    memset(&clabel,0,sizeof(clabel));
    memset(&cgcd,0,sizeof(cgcd));

    clabel[0].text = (unichar_t *) _STR_CIDMultiple;
    clabel[0].text_in_resource = true;
    cgcd[0].gd.label = &clabel[0];
    cgcd[0].gd.mnemonic = 'S';
    cgcd[0].gd.pos.x = 3; cgcd[0].gd.pos.y = 6;
    cgcd[0].gd.flags = gg_visible | gg_enabled;
    if ( cidmultiple ) cgcd[0].gd.flags |= gg_cb_on;
    cgcd[0].gd.popup_msg = GStringGetResource(_STR_CIDMultiplePopup,NULL);
    cgcd[0].gd.cid = CID_CIDMultiple;
    cgcd[0].creator = GCheckBoxCreate;

    clabel[1].text = (unichar_t *) _STR_CIDBlank;
    clabel[1].text_in_resource = true;
    cgcd[1].gd.label = &clabel[1];
    cgcd[1].gd.mnemonic = 'S';
    cgcd[1].gd.pos.x = 3; cgcd[1].gd.pos.y = cgcd[0].gd.pos.y+17; 
    cgcd[1].gd.flags = gg_visible | gg_enabled;
    if ( cidblank ) cgcd[1].gd.flags |= gg_cb_on;
    cgcd[1].gd.popup_msg = GStringGetResource(_STR_CIDBlankPopup,NULL);
    cgcd[1].gd.cid = CID_CIDBlank;
    cgcd[1].creator = GCheckBoxCreate;

/* ************************************************************************** */

    memset(&alabel,0,sizeof(alabel));
    memset(&agcd,0,sizeof(agcd));

    alabel[0].text = (unichar_t *) _STR_MissingGlyph;
    alabel[0].text_in_resource = true;
    agcd[0].gd.label = &alabel[0];
    agcd[0].gd.pos.x = 3; agcd[0].gd.pos.y = 6;
    agcd[0].gd.flags = gg_visible | gg_enabled;
    if ( missingglyph ) agcd[0].gd.flags |= gg_cb_on;
    agcd[0].gd.popup_msg = GStringGetResource(_STR_MissingGlyphPopup,NULL);
    agcd[0].gd.cid = CID_MissingGlyph;
    agcd[0].creator = GCheckBoxCreate;

    alabel[1].text = (unichar_t *) _STR_MissingLookupTag;
    alabel[1].text_in_resource = true;
    agcd[1].gd.label = &alabel[1];
    agcd[1].gd.pos.x = 3; agcd[1].gd.pos.y = agcd[0].gd.pos.y+17; 
    agcd[1].gd.flags = gg_visible | gg_enabled;
    if ( missinglookuptag ) agcd[1].gd.flags |= gg_cb_on;
    agcd[1].gd.popup_msg = GStringGetResource(_STR_MissingLookupTagPopup,NULL);
    agcd[1].gd.cid = CID_MissingLookupTag;
    agcd[1].creator = GCheckBoxCreate;

/* ************************************************************************** */

    memset(&mlabel,0,sizeof(mlabel));
    memset(&mgcd,0,sizeof(mgcd));
    memset(aspects,0,sizeof(aspects));
    i = 0;

    aspects[i].text = (unichar_t *) _STR_PointsNoC;
    aspects[i].selected = true;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = pgcd;

    aspects[i].text = (unichar_t *) _STR_Paths;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = pagcd;

    aspects[i].text = (unichar_t *) _STR_Hints;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = hgcd;

    aspects[i].text = (unichar_t *) _STR_ATT;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = agcd;

    aspects[i].text = (unichar_t *) _STR_CID;
    aspects[i].disabled = fv->cidmaster==NULL;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = cgcd;

    aspects[i].text = (unichar_t *) _STR_Random;
    aspects[i].text_in_resource = true;
    aspects[i++].gcd = rgcd;

    mgcd[0].gd.pos.x = 4; mgcd[0].gd.pos.y = 6;
    mgcd[0].gd.pos.width = 210;
    mgcd[0].gd.pos.height = 190;
    mgcd[0].gd.u.tabs = aspects;
    mgcd[0].gd.flags = gg_visible | gg_enabled;
    mgcd[0].creator = GTabSetCreate;

    mgcd[1].gd.pos.x = 15; mgcd[1].gd.pos.y = 190+10;
    mgcd[1].gd.flags = gg_visible | gg_enabled | gg_dontcopybox;
    mlabel[1].text = (unichar_t *) _STR_ClearAll;
    mlabel[1].text_in_resource = true;
    mgcd[1].gd.label = &mlabel[1];
    /*mgcd[1].gd.box = &smallbox;*/
    mgcd[1].gd.handle_controlevent = Prob_DoAll;
    mgcd[2].gd.cid = CID_ClearAll;
    mgcd[1].creator = GButtonCreate;

    mgcd[2].gd.pos.x = mgcd[1].gd.pos.x+1.25*GIntGetResource(_NUM_Buttonsize);
    mgcd[2].gd.pos.y = mgcd[1].gd.pos.y;
    mgcd[2].gd.flags = gg_visible | gg_enabled | gg_dontcopybox;
    mlabel[2].text = (unichar_t *) _STR_SetAll;
    mlabel[2].text_in_resource = true;
    mgcd[2].gd.label = &mlabel[2];
    /*mgcd[2].gd.box = &smallbox;*/
    mgcd[2].gd.handle_controlevent = Prob_DoAll;
    mgcd[2].gd.cid = CID_SetAll;
    mgcd[2].creator = GButtonCreate;

    mgcd[3].gd.pos.x = 6; mgcd[3].gd.pos.y = mgcd[1].gd.pos.y+27;
    mgcd[3].gd.pos.width = 218-12;
    mgcd[3].gd.flags = gg_visible | gg_enabled;
    mgcd[3].creator = GLineCreate;

    mlabel[4].text = (unichar_t *) _STR_PointsNear;
    mlabel[4].text_in_resource = true;
    mgcd[4].gd.label = &mlabel[4];
    mgcd[4].gd.mnemonic = 'N';
    mgcd[4].gd.pos.x = 6; mgcd[4].gd.pos.y = mgcd[3].gd.pos.y+6+6;
    mgcd[4].gd.flags = gg_visible | gg_enabled;
    mgcd[4].creator = GLabelCreate;

    sprintf(nearbuf,"%g",near);
    mlabel[5].text = (unichar_t *) nearbuf;
    mlabel[5].text_is_1byte = true;
    mgcd[5].gd.label = &mlabel[5];
    mgcd[5].gd.pos.x = 130; mgcd[5].gd.pos.y = mgcd[4].gd.pos.y-6; mgcd[5].gd.pos.width = 40;
    mgcd[5].gd.flags = gg_visible | gg_enabled;
    mgcd[5].gd.cid = CID_Near;
    mgcd[5].creator = GTextFieldCreate;

    mgcd[6].gd.pos.x = 15-3; mgcd[6].gd.pos.y = mgcd[5].gd.pos.y+26;
    mgcd[6].gd.pos.width = -1; mgcd[6].gd.pos.height = 0;
    mgcd[6].gd.flags = gg_visible | gg_enabled | gg_but_default;
    mlabel[6].text = (unichar_t *) _STR_OK;
    mlabel[6].text_in_resource = true;
    mgcd[6].gd.mnemonic = 'O';
    mgcd[6].gd.label = &mlabel[6];
    mgcd[6].gd.handle_controlevent = Prob_OK;
    mgcd[6].creator = GButtonCreate;

    mgcd[7].gd.pos.x = -15; mgcd[7].gd.pos.y = mgcd[6].gd.pos.y+3;
    mgcd[7].gd.pos.width = -1; mgcd[7].gd.pos.height = 0;
    mgcd[7].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    mlabel[7].text = (unichar_t *) _STR_Cancel;
    mlabel[7].text_in_resource = true;
    mgcd[7].gd.label = &mlabel[7];
    mgcd[7].gd.mnemonic = 'C';
    mgcd[7].gd.handle_controlevent = Prob_Cancel;
    mgcd[7].creator = GButtonCreate;

    mgcd[8].gd.pos.x = 2; mgcd[8].gd.pos.y = 2;
    mgcd[8].gd.pos.width = pos.width-4; mgcd[8].gd.pos.height = pos.height-2;
    mgcd[8].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
    mgcd[8].creator = GGroupCreate;

    GGadgetsCreate(gw,mgcd);

    GDrawSetVisible(gw,true);
    while ( !p.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    if ( p.explainw!=NULL )
	GDrawDestroyWindow(p.explainw);
}
