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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ustring.h"
#include "gdraw.h"
#include "gwidget.h"
#include "ggadget.h"
#include "ggadgetP.h"

struct dlg_info {
    int done;
    int ret;
};

static int d_e_h(GWindow gw, GEvent *event) {
    struct dlg_info *d = GDrawGetUserData(gw);

    if ( event->type==et_close ) {
	d->done = true;
	/* d->ret is initialized to cancel */
    } else if ( event->type==et_controlevent &&
	    event->u.control.subtype == et_buttonactivate ) {
	d->done = true;
	d->ret = GGadgetGetCid(event->u.control.g);
    } else if ( event->type == et_char ) {
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

static int w_e_h(GWindow gw, GEvent *event) {

    if ( event->type==et_close ||
	    (event->type==et_controlevent &&
	     event->u.control.subtype == et_buttonactivate ) ||
	    event->type==et_timer )
	GDrawDestroyWindow(gw);
    else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    } else if ( event->type == et_char ) {
return( false );
    }
return( true );
}

#define GLINE_MAX	10

static int FindLineBreaks(const unichar_t *question, GTextInfo linebreaks[GLINE_MAX+1]) {
    int lb, i;
    const unichar_t *pt, *last;

    /* Find good places to break the question into bits */
    last = pt = question;
    linebreaks[0].text = (unichar_t *) question;
    lb=0;
    while ( *pt!='\0' && lb<GLINE_MAX-1 ) {
	last = pt;
	/* break lines around 60 characters (or if there are no spaces at 90 chars) */
	while ( pt-linebreaks[lb].text<60 ||
		(pt-linebreaks[lb].text<90 && last==linebreaks[lb].text)) {
	    if ( *pt=='\n' || *pt=='\0' ) {
		last = pt;
	break;
	    }
	    if ( *pt==' ' )
		last = pt;
	    ++pt;
	}
	if ( last==linebreaks[lb].text )
	    last = pt;
	if ( *last==' ' || *last=='\n' ) ++last;
	linebreaks[++lb].text = (unichar_t *) last;
	pt = last;
    }
    if ( *pt!='\0' )
	linebreaks[++lb].text = (unichar_t *) pt+u_strlen(pt);
    for ( i=0; i<lb; ++i ) {
	int temp = linebreaks[i+1].text - linebreaks[i].text;
	if ( linebreaks[i+1].text[-1]==' ' || linebreaks[i+1].text[-1]=='\n' )
	    --temp;
	linebreaks[i].text = u_copyn(linebreaks[i].text,temp);
    }

    if ( question[u_strlen(question)-1]=='\n' )
	--lb;
return( lb );
}

static GWindow DlgCreate(const unichar_t *title,const unichar_t *question,va_list ap,
	const unichar_t **answers, const unichar_t *mn, int def, int cancel,
	struct dlg_info *d, int add_text, int restrict_input, int center) {
    GTextInfo qlabels[GLINE_MAX+1], *blabels;
    GGadgetCreateData *gcd;
    int lb, bcnt=0;
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    extern FontInstance *_ggadget_default_font;
    int as, ds, ld, fh;
    int w, maxw, bw, bspace;
    int i, y;
    unichar_t ubuf[300];

    u_vsnprintf(ubuf,sizeof(ubuf)/sizeof(ubuf[0]),question,ap);
    memset(qlabels,'\0',sizeof(qlabels));
    lb = FindLineBreaks(ubuf,qlabels);
    for ( bcnt=0; answers[bcnt]!=NULL; ++bcnt);
    blabels = gcalloc(bcnt+1,sizeof(GTextInfo));
    for ( bcnt=0; answers[bcnt]!=NULL; ++bcnt)
	blabels[bcnt].text = (unichar_t *) answers[bcnt];

    memset(&wattrs,0,sizeof(wattrs));
    /* If we have many questions in quick succession the dlg will jump around*/
    /*  as it tracks the cursor (which moves to the buttons). That's not good*/
    /*  So I don't do undercursor here */
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    if ( restrict_input )
	wattrs.mask |= wam_restrict;
    else 
	wattrs.mask |= wam_notrestricted;
    if ( center )
	wattrs.mask |= wam_centered;
    else
	wattrs.mask |= wam_undercursor;
    wattrs.not_restricted = true;
    wattrs.restrict_input_to_me = 1;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.undercursor = 1;
    wattrs.centered = 2;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = (unichar_t *) title;
    pos.x = pos.y = 0;
    pos.width = 200; pos.height = 60;		/* We'll figure size later */
		/* but if we get the size too small, the cursor isn't in dlg */
    gw = GDrawCreateTopWindow(NULL,&pos,restrict_input?d_e_h:w_e_h,d,&wattrs);

    GGadgetInit();
    GDrawSetFont(gw,_ggadget_default_font);
    GDrawFontMetrics(_ggadget_default_font,&as,&ds,&ld);
    fh = as+ds;
    maxw = 0;
    for ( i=0; i<lb; ++i ) {
	w = GDrawGetTextWidth(gw,qlabels[i].text,-1,NULL);
	if ( w>maxw ) maxw = w;
    }
    bw = 0;
    for ( i=0; i<bcnt; ++i ) {
	w = GDrawGetTextWidth(gw,answers[i],-1,NULL);
	if ( w>bw ) bw = w;
    }
    bw += GDrawPointsToPixels(gw,12);
    bspace = GDrawPointsToPixels(gw,6);
    if ( (bw+bspace) * bcnt > maxw )
	maxw = (bw+bspace)*bcnt;
    if ( bcnt!=1 )
	bspace = (maxw-bcnt*bw)/(bcnt-1);
    maxw += GDrawPointsToPixels(gw,16);

    gcd = gcalloc(lb+bcnt+2,sizeof(GGadgetCreateData));
    if ( lb==1 ) {
	gcd[0].gd.pos.width = GDrawGetTextWidth(gw,qlabels[0].text,-1,NULL);
	gcd[0].gd.pos.x = (maxw-gcd[0].gd.pos.width)/2;
	gcd[0].gd.pos.y = GDrawPointsToPixels(gw,6);
	gcd[0].gd.pos.height = fh;
	gcd[0].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	gcd[0].gd.label = &qlabels[0];
	gcd[0].creator = GLabelCreate;
    } else for ( i=0; i<lb; ++i ) {
	gcd[i].gd.pos.x = GDrawPointsToPixels(gw,8);
	gcd[i].gd.pos.y = GDrawPointsToPixels(gw,6)+i*fh;
	gcd[i].gd.pos.width = GDrawGetTextWidth(gw,qlabels[i].text,-1,NULL);
	gcd[i].gd.pos.height = fh;
	gcd[i].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	gcd[i].gd.label = &qlabels[i];
	gcd[i].creator = GLabelCreate;
    }
    y = GDrawPointsToPixels(gw,12)+lb*fh;
    if ( add_text ) {
	gcd[bcnt+lb].gd.pos.x = GDrawPointsToPixels(gw,8);
	gcd[bcnt+lb].gd.pos.y = y;
	gcd[bcnt+lb].gd.pos.width = maxw-2*GDrawPointsToPixels(gw,6);
	gcd[bcnt+lb].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	gcd[bcnt+lb].gd.cid = bcnt;
	gcd[bcnt+lb].creator = GTextFieldCreate;
	y += fh + GDrawPointsToPixels(gw,6) + GDrawPointsToPixels(gw,10);
    }
    y += GDrawPointsToPixels(gw,2);
    for ( i=0; i<bcnt; ++i ) {
	gcd[i+lb].gd.pos.x = GDrawPointsToPixels(gw,8) + i*(bw+bspace);
	gcd[i+lb].gd.pos.y = y;
	gcd[i+lb].gd.pos.width = bw;
	gcd[i+lb].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	if ( i==def ) {
	    gcd[i+lb].gd.flags |= gg_but_default;
	    gcd[i+lb].gd.pos.x -= GDrawPointsToPixels(gw,3);
	    gcd[i+lb].gd.pos.y -= GDrawPointsToPixels(gw,3);
	    gcd[i+lb].gd.pos.width += 2*GDrawPointsToPixels(gw,3);
	}
	if ( i==cancel )
	    gcd[i+lb].gd.flags |= gg_but_cancel;
	gcd[i+lb].gd.cid = i;
	gcd[i+lb].gd.label = &blabels[i];
	if ( mn!=NULL ) {
	    gcd[i+lb].gd.mnemonic = mn[i];
	    if ( mn[i]=='\0' ) mn = NULL;
	}
	gcd[i+lb].creator = GButtonCreate;
    }
    if ( bcnt==1 )
	gcd[lb].gd.pos.x = (maxw-bw)/2;
    GGadgetsCreate(gw,gcd);
    pos.width = maxw;
    pos.height = (lb+1)*fh + GDrawPointsToPixels(gw,34);
    if ( add_text )
	pos.height += fh + GDrawPointsToPixels(gw,16);
    GDrawResize(gw,pos.width,pos.height);
    GWidgetHidePalettes();
    GDrawSetVisible(gw,true);
    if ( d!=NULL ) {
	memset(d,'\0',sizeof(d));
	d->ret = cancel;
    }
    free(blabels);
    free(gcd);
    for ( i=0; i<lb; ++i )
	free(qlabels[i].text);
return( gw );
}

int GWidgetAsk(const unichar_t *title,
	const unichar_t **answers, const unichar_t *mn, int def, int cancel,
	const unichar_t *question,...) {
    struct dlg_info d;
    GWindow gw;
    va_list ap;

    va_start(ap,question);
    gw = DlgCreate(title,question,ap,answers,mn,def,cancel,&d,false,true,false);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(d.ret);
}

int GWidgetAskCentered(const unichar_t *title,
	const unichar_t ** answers, const unichar_t *mn, int def, int cancel,
	const unichar_t *question, ... ) {
    struct dlg_info d;
    GWindow gw;
    va_list ap;

    va_start(ap,question);
    gw = DlgCreate(title,question,ap,answers,mn,def,cancel,&d,false,true,true);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(d.ret);
}

int GWidgetAskR(int title, int *answers, int def, int cancel,int question,...) {
    const unichar_t **ans;
    unichar_t *mn;
    struct dlg_info d;
    int i;
    va_list ap;
    GWindow gw;

    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i );
    ans = gcalloc(i+1,sizeof(unichar_t *));
    mn = gcalloc(i,sizeof(unichar_t));
    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i )
	ans[i] = GStringGetResource(answers[i],&mn[i]);
    va_start(ap,question);
    gw = DlgCreate(GStringGetResource(title,NULL),GStringGetResource(question,NULL),ap,
	    ans,mn,def,cancel,&d,false,true,false);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    free(mn); free(ans);
return(d.ret);
}

int GWidgetAskR_(int title, int *answers, int def, int cancel,const unichar_t *question, ...) {
    const unichar_t **ans;
    unichar_t *mn;
    struct dlg_info d;
    int i;
    GWindow gw;
    va_list ap;

    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i );
    ans = gcalloc(i+1,sizeof(unichar_t *));
    mn = gcalloc(i,sizeof(unichar_t));
    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i )
	ans[i] = GStringGetResource(answers[i],&mn[i]);
    va_start(ap,question);
    gw = DlgCreate(GStringGetResource(title,NULL),question,ap,
	    ans,mn,def,cancel,&d,false,true,false);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    free(mn); free(ans);
return(d.ret);
}

int GWidgetAskCenteredR_(int title, int *answers, int def, int cancel,const unichar_t *question,...) {
    const unichar_t **ans;
    unichar_t *mn;
    struct dlg_info d;
    int i;
    GWindow gw;
    va_list ap;

    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i );
    ans = gcalloc(i+1,sizeof(unichar_t *));
    mn = gcalloc(i,sizeof(unichar_t));
    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i )
	ans[i] = GStringGetResource(answers[i],&mn[i]);

    va_start(ap,question);
    gw = DlgCreate(GStringGetResource(title,NULL),question,ap,ans,mn,def,cancel,&d,false,true,true);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(d.ret);
}

int GWidgetAskCenteredR(int title, int *answers, int def, int cancel,int question,...) {
    const unichar_t **ans;
    unichar_t *mn;
    struct dlg_info d;
    int i;
    GWindow gw;
    va_list ap;

    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i );
    ans = gcalloc(i+1,sizeof(unichar_t *));
    mn = gcalloc(i,sizeof(unichar_t));
    for ( i=0; answers[i]!=0 && answers[i]!=0x80000000; ++i )
	ans[i] = GStringGetResource(answers[i],&mn[i]);

    va_start(ap,question);
    gw = DlgCreate(GStringGetResource(title,NULL),GStringGetResource(question,NULL),ap,
	    ans,mn,def,cancel,&d,false,true,true);
    va_end(ap);

    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(d.ret);
}

unichar_t *GWidgetAskString(const unichar_t *title,const unichar_t *def,
	const unichar_t *question,...) {
    struct dlg_info d;
    GWindow gw;
    unichar_t *ret = NULL;
    const unichar_t *ocb[3]; unichar_t ocmn[2];
    va_list ap;

    ocb[2]=NULL;
    ocb[0] = GStringGetResource( _STR_OK, &ocmn[0]);
    ocb[1] = GStringGetResource( _STR_Cancel, &ocmn[1]);
    va_start(ap,question);
    gw = DlgCreate(title,question,ap,ocb,ocmn,0,1,&d,true,true,false);
    va_end(ap);
    if ( def!=NULL && *def!='\0' )
	GGadgetSetTitle(GWidgetGetControl(gw,2),def);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    if ( d.ret==0 )
	ret = u_copy(GGadgetGetTitle(GWidgetGetControl(gw,2)));
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(ret);
}

unichar_t *GWidgetAskStringR(int title,const unichar_t *def,int question,...) {
    struct dlg_info d;
    GWindow gw;
    unichar_t *ret = NULL;
    const unichar_t *ocb[3]; unichar_t ocmn[2];
    va_list ap;

    ocb[2]=NULL;
    ocb[0] = GStringGetResource( _STR_OK, &ocmn[0]);
    ocb[1] = GStringGetResource( _STR_Cancel, &ocmn[1]);
    va_start(ap,question);
    gw = DlgCreate(GStringGetResource( title,NULL),GStringGetResource( question,NULL),ap,
	    ocb,ocmn,0,1,&d,true,true,false);
    va_end(ap);
    if ( def!=NULL && *def!='\0' )
	GGadgetSetTitle(GWidgetGetControl(gw,2),def);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    if ( d.ret==0 )
	ret = u_copy(GGadgetGetTitle(GWidgetGetControl(gw,2)));
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(ret);
}

void GWidgetPostNotice(const unichar_t *title,const unichar_t *statement,...) {
    GWindow gw;
    const unichar_t *ob[2]; unichar_t omn[1];
    va_list ap;

    ob[1]=NULL;
    ob[0] = GStringGetResource( _STR_OK, &omn[0]);
    va_start(ap,statement);
    gw = DlgCreate(title,statement,ap,ob,omn,0,0,NULL,false,false,true);
    va_end(ap);
    GDrawRequestTimer(gw,40*1000,0,NULL);
    /* Continue merrily on our way. Window will destroy itself in 40 secs */
    /*  or when user kills it. We can ignore it */
}

void GWidgetPostNoticeR(int title,int statement,...) {
    GWindow gw;
    const unichar_t *oc[2]; unichar_t omn[1];
    va_list ap;

    oc[1]=NULL;
    oc[0] = GStringGetResource( _STR_OK, &omn[0]);
    va_start(ap,statement);
    gw = DlgCreate(GStringGetResource(title,NULL),GStringGetResource(statement,NULL),ap,
	    oc,omn,0,0,NULL,false,false,true);
    va_end(ap);
    GDrawRequestTimer(gw,40*1000,0,NULL);
    /* Continue merrily on our way. Window will destroy itself in 40 secs */
    /*  or when user kills it. We can ignore it */
}

void GWidgetError(const unichar_t *title,const unichar_t *statement, ...) {
    struct dlg_info d;
    GWindow gw;
    const unichar_t *ob[2]; unichar_t omn[1];
    va_list ap;

    ob[1]=NULL;
    ob[0] = GStringGetResource( _STR_OK, &omn[0]);
    va_start(ap,statement);
    gw = DlgCreate(title,statement,ap,ob,omn,0,0,&d,false,true,true);
    va_end(ap);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
}

void GWidgetErrorR(int title,int statement, ...) {
    struct dlg_info d;
    GWindow gw;
    const unichar_t *oc[2]; unichar_t omn[1];
    va_list ap;

    oc[1]=NULL;
    oc[0] = GStringGetResource( _STR_OK, &omn[0]);
    va_start(ap,statement);
    gw = DlgCreate(GStringGetResource(title,NULL),GStringGetResource(statement,NULL),ap,
	    oc,omn,0,0,&d,false,true,true);
    va_end(ap);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
}

/* ************************************************************************** */

#define CID_Cancel	0
#define CID_OK		1
#define CID_List	2

static int c_e_h(GWindow gw, GEvent *event) {
    struct dlg_info *d = GDrawGetUserData(gw);

    if ( event->type==et_close ) {
	d->done = true;
	d->ret = -1;
    } else if ( event->type==et_controlevent &&
	    (event->u.control.subtype == et_buttonactivate ||
	     event->u.control.subtype == et_listdoubleclick )) {
	d->done = true;
	if ( GGadgetGetCid(event->u.control.g)==CID_Cancel )
	    d->ret = -1;
	else
	    d->ret = GGadgetGetFirstListSelectedItem(GWidgetGetControl(gw,CID_List));
    } else if ( event->type == et_char ) {
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

static GWindow ChoiceDlgCreate(struct dlg_info *d,const unichar_t *title,
	const unichar_t *question, va_list ap,
	const unichar_t **choices, int cnt, int def,
	int restrict_input, int center) {
    GTextInfo qlabels[GLINE_MAX+1], *llabels, blabel[2];
    GGadgetCreateData *gcd;
    int lb;
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    extern FontInstance *_ggadget_default_font;
    int as, ds, ld, fh;
    int w, maxw;
    int i, y;
    unichar_t ubuf[300];

    u_vsnprintf(ubuf,sizeof(ubuf)/sizeof(ubuf[0]),question,ap);
    memset(qlabels,'\0',sizeof(qlabels));
    lb = FindLineBreaks(ubuf,qlabels);
    llabels = gcalloc(cnt+1,sizeof(GTextInfo));
    for ( i=0; i<cnt; ++i) {
	llabels[i].text = (unichar_t *) choices[i];
	llabels[i].selected = (i==def);
    }

    memset(&wattrs,0,sizeof(wattrs));
    /* If we have many questions in quick succession the dlg will jump around*/
    /*  as it tracks the cursor (which moves to the buttons). That's not good*/
    /*  So I don't do undercursor here */
    wattrs.mask = wam_events|wam_cursor|wam_wtitle;
    if ( restrict_input )
	wattrs.mask |= wam_restrict;
    else 
	wattrs.mask |= wam_notrestricted;
    if ( center )
	wattrs.mask |= wam_centered;
    else
	wattrs.mask |= wam_undercursor;
    wattrs.not_restricted = true;
    wattrs.restrict_input_to_me = 1;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.undercursor = 1;
    wattrs.centered = 2;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = (unichar_t *) title;
    pos.x = pos.y = 0;
    pos.width = 200; pos.height = 60;		/* We'll figure size later */
		/* but if we get the size too small, the cursor isn't in dlg */
    gw = GDrawCreateTopWindow(NULL,&pos,c_e_h,d,&wattrs);

    GDrawSetFont(gw,_ggadget_default_font);
    GDrawFontMetrics(_ggadget_default_font,&as,&ds,&ld);
    fh = as+ds;
    maxw = 200;
    for ( i=0; i<lb; ++i ) {
	w = GDrawGetTextWidth(gw,qlabels[i].text,-1,NULL);
	if ( w>maxw ) maxw = w;
    }
    maxw += GDrawPointsToPixels(gw,16);

    gcd = gcalloc(lb+1+2+2,sizeof(GGadgetCreateData));
    if ( lb==1 ) {
	gcd[0].gd.pos.width = GDrawGetTextWidth(gw,qlabels[0].text,-1,NULL);
	gcd[0].gd.pos.x = (maxw-gcd[0].gd.pos.width)/2;
	gcd[0].gd.pos.y = GDrawPointsToPixels(gw,6);
	gcd[0].gd.pos.height = fh;
	gcd[0].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	gcd[0].gd.label = &qlabels[0];
	gcd[0].creator = GLabelCreate;
    } else for ( i=0; i<lb; ++i ) {
	gcd[i].gd.pos.x = GDrawPointsToPixels(gw,8);
	gcd[i].gd.pos.y = GDrawPointsToPixels(gw,6)+i*fh;
	gcd[i].gd.pos.width = GDrawGetTextWidth(gw,qlabels[i].text,-1,NULL);
	gcd[i].gd.pos.height = fh;
	gcd[i].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels;
	gcd[i].gd.label = &qlabels[i];
	gcd[i].creator = GLabelCreate;
    }

    y = GDrawPointsToPixels(gw,12)+lb*fh;
    gcd[i].gd.pos.x = GDrawPointsToPixels(gw,8); gcd[i].gd.pos.y = y;
    gcd[i].gd.pos.width = maxw - 2*GDrawPointsToPixels(gw,8);
    gcd[i].gd.pos.height = (cnt<4?4:cnt<8?cnt:8)*fh + 2*GDrawPointsToPixels(gw,6);
    gcd[i].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels | gg_list_exactlyone;
    gcd[i].gd.u.list = llabels;
    gcd[i].gd.cid = CID_List;
    gcd[i++].creator = GListCreate;
    y += gcd[i-1].gd.pos.height + GDrawPointsToPixels(gw,10);

    memset(blabel,'\0',sizeof(blabel));
    gcd[i].gd.pos.x = GDrawPointsToPixels(gw,15)-3; gcd[i].gd.pos.y = y-3;
    gcd[i].gd.pos.width = -1;
    gcd[i].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels |gg_but_default;
    gcd[i].gd.label = &blabel[0];
    blabel[0].text = (unichar_t *) _STR_OK;
    blabel[0].text_in_resource = true;
    gcd[i].gd.cid = CID_OK;
    gcd[i++].creator = GButtonCreate;

    gcd[i].gd.pos.x = maxw-GDrawPointsToPixels(gw,15)-
	    GDrawPointsToPixels(gw,GIntGetResource(_NUM_Buttonsize));
    gcd[i].gd.pos.y = y;
    gcd[i].gd.pos.width = -1;
    gcd[i].gd.flags = gg_visible | gg_enabled | gg_pos_in_pixels |gg_but_cancel;
    gcd[i].gd.label = &blabel[1];
    blabel[1].text = (unichar_t *) _STR_Cancel;
    blabel[1].text_in_resource = true;
    gcd[i].gd.cid = CID_Cancel;
    gcd[i++].creator = GButtonCreate;

    GGadgetsCreate(gw,gcd);
    pos.width = maxw;
    pos.height = y + GDrawPointsToPixels(gw,34);
    GDrawResize(gw,pos.width,pos.height);
    GWidgetHidePalettes();
    GDrawSetVisible(gw,true);
    memset(d,'\0',sizeof(d));
    d->ret = -1;
    free(llabels);
    free(gcd);
    for ( i=0; i<lb; ++i )
	free(qlabels[i].text);
return( gw );
}

int GWidgetChoicesR(int title, const unichar_t **choices,int cnt, int def,int question,...) {
    struct dlg_info d;
    GWindow gw;
    va_list ap;

    va_start(ap,question);
    gw = ChoiceDlgCreate(&d,GStringGetResource( title,NULL),GStringGetResource( question,NULL),ap,
	    choices,cnt,def,true,false);
    va_end(ap);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
return(d.ret);
}
