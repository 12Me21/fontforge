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
#include <stdlib.h>
#include "utype.h"
#include "gdraw.h"
#include "ggadgetP.h"
#include "gresource.h"
#include "gwidget.h"
#include "ustring.h"

GBox _ggadget_Default_Box = { bt_raised, bs_rect, 2, 2, 0, 0, 
    COLOR_CREATE(0xdd,0xdd,0xdd),		/* border left */
    COLOR_CREATE(0xf7,0xf7,0xf7),		/* border top */
    COLOR_CREATE(0x80,0x80,0x80),		/* border right */
    COLOR_CREATE(0x66,0x66,0x66),		/* border bottom */
    COLOR_DEFAULT,				/* normal background */
    COLOR_DEFAULT,				/* normal foreground */
    COLOR_CREATE(0xd0,0xd0,0xd0),		/* disabled background */
    COLOR_CREATE(0x66,0x66,0x66),		/* disabled foreground */
    COLOR_CREATE(0xff,0xff,0x00),		/* active border */
    COLOR_CREATE(0x90,0x90,0x90)		/* pressed background */
};
GBox _GListMark_Box = { /* Don't initialize here */ 0 };
FontInstance *_ggadget_default_font = NULL;
static FontInstance *popup_font = NULL;
int _GListMarkSize = 12;
static int _GGadget_FirstLine = 6;
static int _GGadget_LeftMargin = 6;
static int _GGadget_LineSkip = 3;
int _GGadget_Skip = 6;
int _GGadget_TextImageSkip = 4;
static int _ggadget_inited=0;
extern void GGadgetInit(void);
static Color popup_foreground=0, popup_background=COLOR_CREATE(0xff,0xff,0xc0);
static int popup_delay=2000, popup_lifetime=10000;

static GWindow popup;
static GTimer *popup_timer, *popup_vanish_timer;
static int popup_visible = false;

static unichar_t courier[] = { 'c', 'o', 'u', 'r', 'i', 'e', 'r',',','m','o','n','o','s','p','a','c','e', '\0' };
static unichar_t helv[] = { 'h', 'e', 'l', 'v', 'e', 't', 'i', 'c', 'a',',','c','a','l','i','b','a','n',  '\0' };

static int match(char **list, char *val) {
    int i;

    for ( i=0; list[i]!=NULL; ++i )
	if ( strmatch(val,list[i])==0 )
return( i );

return( -1 );
}

static void *border_type_cvt(char *val, void *def) {
    static char *types[] = { "none", "box", "raised", "lowered", "engraved",
	    "embossed", "double", NULL };
    int ret = match(types,val);
    if ( ret== -1 )
return( def );
return( (void *) ret );
}

static void *border_shape_cvt(char *val, void *def) {
    static char *shapes[] = { "rect", "roundrect", "elipse", "diamond", NULL };
    int ret = match(shapes,val);
    if ( ret== -1 )
return( def );
return( (void *) ret );
}

/* font name may be something like:
	bold italic extended 12pt courier
	400 10pt small-caps
    family name comes at the end, size must have "pt" after it
*/
static void *font_cvt(char *val, void *def) {
    static char *styles[] = { "normal", "italic", "oblique", "small-caps",
	    "bold", "light", "extended", "condensed", NULL };
    FontRequest rq;
    FontInstance *fi;
    char *pt, *end, ch;
    int ret;

    rq.family_name = helv;
    rq.point_size = 10;
    rq.weight = 400;
    rq.style = 0;
    if ( _ggadget_default_font!=NULL )
	GDrawDecomposeFont(_ggadget_default_font,&rq);

    for ( pt=val; *pt && *pt!='"'; ) {
	for ( end=pt; *end!=' ' && *end!='\0'; ++end );
	ch = *end; *end = '\0';
	ret = match(styles,pt);
	if ( ret==-1 && isdigit(*pt)) {
	    char *e;
	    ret = strtol(pt,&e,10);
	    if ( strmatch(e,"pt")==0 )
		rq.point_size = ret;
	    else if ( *e=='\0' )
		rq.weight = ret;
	    else {
		*end = ch;
    break;
	    }
	} else if ( ret==-1 ) {
	    *end = ch;
    break;
	} else if ( ret==0 )
	    /* Do Nothing */;
	else if ( ret==1 || ret==2 )
	    rq.style |= fs_italic;
	else if ( ret==3 )
	    rq.style |= fs_smallcaps;
	else if ( ret==4 )
	    rq.weight = 700;
	else if ( ret==5 )
	    rq.weight = 300;
	else if ( ret==6 )
	    rq.style |= fs_extended;
	else
	    rq.style |= fs_condensed;
	*end = ch;
	pt = end;
	while ( *pt==' ' ) ++pt;
    }

    if ( *pt!='\0' )
	rq.family_name = uc_copy(pt);
		
    fi = GDrawInstanciateFont(screen_display,&rq);
    if ( rq.family_name!=courier )
	free(rq.family_name );

    if ( fi==NULL )
return( def );
return( (void *) fi );
}

void _GGadgetCopyDefaultBox(GBox *box) {
    *box = _ggadget_Default_Box;
}

FontInstance *_GGadgetInitDefaultBox(char *class,GBox *box, FontInstance *deffont) {
    GResStruct bordertype[] = {
	{ "Box.BorderType", rt_string, NULL, border_type_cvt },
	{ NULL }
    };
    GResStruct boxtypes[] = {
	{ "Box.BorderType", rt_string, NULL, border_type_cvt },
	{ "Box.BorderShape", rt_string, NULL, border_shape_cvt },
	{ "Box.BorderWidth", rt_int, NULL },
	{ "Box.Padding", rt_int, NULL },
	{ "Box.Radius", rt_int, NULL },
	{ "Box.BorderInner", rt_bool, NULL },
	{ "Box.BorderOuter", rt_bool, NULL },
	{ "Box.ActiveInner", rt_bool, NULL },
	{ "Box.DoDepressedBackground", rt_bool, NULL },
	{ "Box.DrawDefault", rt_bool, NULL },
	{ "Box.BorderBrightest", rt_color, NULL },
	{ "Box.BorderBrighter", rt_color, NULL },
	{ "Box.BorderDarkest", rt_color, NULL },
	{ "Box.BorderDarker", rt_color, NULL },
	{ "Box.NormalBackground", rt_color, NULL },
	{ "Box.NormalForeground", rt_color, NULL },
	{ "Box.DisabledBackground", rt_color, NULL },
	{ "Box.DisabledForeground", rt_color, NULL },
	{ "Box.ActiveBorder", rt_color, NULL },
	{ "Box.PressedBackground", rt_color, NULL },
	{ "Box.BorderLeft", rt_color, NULL },
	{ "Box.BorderTop", rt_color, NULL },
	{ "Box.BorderRight", rt_color, NULL },
	{ "Box.BorderBottom", rt_color, NULL },
	{ "Font", rt_string, NULL, font_cvt },
	{ NULL }
    };
    int bt, bs, bw, pad, rr, inner, outer, active, depressed, def;
    FontInstance *fi=deffont;

    if ( !_ggadget_inited )
	GGadgetInit();
    if ( fi==NULL )
	fi = _ggadget_default_font;
    bt = box->border_type;
    bs = box->border_shape;
    bw = box->border_width;
    pad = box->padding;
    rr = box->rr_radius;
    inner = box->flags & box_foreground_border_inner;
    outer = box->flags & box_foreground_border_outer;
    active = box->flags & box_active_border_inner;
    depressed = box->flags & box_do_depressed_background;
    def = box->flags & box_draw_default;

    bordertype[0].val = &bt;
    boxtypes[0].val = &bt;
    boxtypes[1].val = &bs;
    boxtypes[2].val = &bw;
    boxtypes[3].val = &pad;
    boxtypes[4].val = &rr;
    boxtypes[5].val = &inner;
    boxtypes[6].val = &outer;
    boxtypes[7].val = &active;
    boxtypes[8].val = &depressed;
    boxtypes[9].val = &def;
    boxtypes[10].val = &box->border_brightest;
    boxtypes[11].val = &box->border_brighter;
    boxtypes[12].val = &box->border_darkest;
    boxtypes[13].val = &box->border_darker;
    boxtypes[14].val = &box->main_background;
    boxtypes[15].val = &box->main_foreground;
    boxtypes[16].val = &box->disabled_background;
    boxtypes[17].val = &box->disabled_foreground;
    boxtypes[18].val = &box->active_border;
    boxtypes[19].val = &box->depressed_background;
    boxtypes[20].val = &box->border_brightest;
    boxtypes[21].val = &box->border_brighter;
    boxtypes[22].val = &box->border_darkest;
    boxtypes[23].val = &box->border_darker;
    boxtypes[24].val = &fi;

    GResourceFind( bordertype, class);
    /* for a plain box, default to all borders being the same. they must change*/
    /*  explicitly */
    if ( bt==bt_box || bt==bt_double )
	box->border_brightest = box->border_brighter = box->border_darker = box->border_darkest;
    GResourceFind( boxtypes, class);

    box->border_type = bt;
    box->border_shape = bs;
    box->border_width = bw;
    box->padding = pad;
    box->rr_radius = rr;
    box->flags=0;
    if ( inner )
	box->flags |= box_foreground_border_inner;
    if ( outer )
	box->flags |= box_foreground_border_outer;
    if ( active )
	box->flags |= box_active_border_inner;
    if ( depressed )
	box->flags |= box_do_depressed_background;
    if ( def )
	box->flags |= box_draw_default;

    if ( fi==NULL ) {
	FontRequest rq;
	rq.family_name = helv;
	rq.point_size = 10;
	rq.weight = 400;
	rq.style = 0;
	fi = GDrawInstanciateFont(screen_display,&rq);
	if ( fi==NULL )
	    GDrawFatalError("Cannot find a default font for gadgets");
    }
return( fi );
}

void GGadgetInit(void) {
    static GResStruct res[] = {
	{ "Font", rt_string, NULL, font_cvt },
	{ NULL }
    };
    if ( !_ggadget_inited ) {
	_ggadget_inited = true;
	_ggadget_Default_Box.main_background = GDrawGetDefaultBackground(NULL);
	_ggadget_Default_Box.main_foreground = GDrawGetDefaultForeground(NULL);
	_ggadget_default_font = _GGadgetInitDefaultBox("GGadget.",&_ggadget_Default_Box,NULL);
	_GGadgetCopyDefaultBox(&_GListMark_Box);
	_GListMark_Box.border_width = _GListMark_Box.padding = 1;
	_GListMark_Box.flags = 0;
	_GGadgetInitDefaultBox("GListMark.",&_GListMark_Box,NULL);
	_GListMarkSize = GResourceFindInt("GListMark.Width", _GListMarkSize);
	_GGadget_FirstLine = GResourceFindInt("GGadget.FirstLine", _GGadget_FirstLine);
	_GGadget_LeftMargin = GResourceFindInt("GGadget.LeftMargin", _GGadget_LeftMargin);
	_GGadget_LineSkip = GResourceFindInt("GGadget.LineSkip", _GGadget_LineSkip);
	_GGadget_Skip = GResourceFindInt("GGadget.Skip", _GGadget_Skip);
	_GGadget_TextImageSkip = GResourceFindInt("GGadget.TextImageSkip", _GGadget_TextImageSkip);

	popup_foreground = GResourceFindColor("GGadget.Popup.Foreground",popup_foreground);
	popup_background = GResourceFindColor("GGadget.Popup.Background",popup_background);
	popup_delay = GResourceFindInt("GGadget.Popup.Delay",popup_delay);
	popup_lifetime = GResourceFindInt("GGadget.Popup.LifeTime",popup_lifetime);
	res[0].val = &popup_font;
	GResourceFind( res, "GGadget.Popup.");
	if ( popup_font==NULL ) {
	    FontRequest rq;
	    rq.family_name = helv;
	    rq.point_size = -10;
	    rq.weight = 400;
	    rq.style = 0;
	    popup_font = GDrawInstanciateFont(screen_display,&rq);
	    if ( popup_font==NULL )
		popup_font = _ggadget_default_font;
	}
    }
}

void GGadgetEndPopup() {
    if ( popup_visible ) {
	GDrawSetVisible(popup,false);
	popup_visible = false;
    }
    if ( popup_timer!=NULL ) {
	GDrawCancelTimer(popup_timer);
	popup_timer = NULL;
    }
    if ( popup_vanish_timer!=NULL ) {
	GDrawCancelTimer(popup_vanish_timer);
	popup_vanish_timer = NULL;
    }
}

static int GGadgetPopupTest(GEvent *e) {
    unichar_t *msg;
    int lines, temp, width;
    GWindow root = GDrawGetRoot(GDrawGetDisplayOfWindow(popup));
    GRect pos, size;
    unichar_t *pt, *ept;
    int as, ds, ld;
    GEvent where;

    if ( e->type!=et_timer || e->u.timer.timer!=popup_timer || popup==NULL )
return( false );
    popup_timer = NULL;
    pt = msg = (unichar_t *) (e->u.timer.userdata);

    lines = 0; width = 1;
    do {
	temp = -1;
	if (( ept = u_strchr(pt,'\n'))!=NULL )
	    temp = ept-pt;
	temp = GDrawGetTextWidth(popup,pt,temp,NULL);
	if ( temp>width ) width = temp;
	++lines;
	pt = ept+1;
    } while ( ept!=NULL && *pt!='\0' );
    GDrawFontMetrics(popup_font,&as, &ds, &ld);
    pos.width = width+2*GDrawPointsToPixels(popup,2);
    pos.height = lines*(as+ds) + 2*GDrawPointsToPixels(popup,2);
    GDrawGetPointerPosition(root,&where);
    pos.x = where.u.mouse.x+10; pos.y = where.u.mouse.y+10;
    GDrawGetSize(root,&size);
    if ( pos.x + pos.width > size.width )
	pos.x = (pos.x - 20 - pos.width );
    if ( pos.x<0 ) pos.x = 0;
    if ( pos.y + pos.height > size.height )
	pos.y = (pos.y - 20 - pos.height );
    if ( pos.y<0 ) pos.y = 0;
    GDrawMoveResize(popup,pos.x,pos.y,pos.width,pos.height);
    GDrawSetVisible(popup,true);
    GDrawRaise(popup);
    GDrawSetUserData(popup,msg);
    popup_vanish_timer = GDrawRequestTimer(popup,popup_lifetime,0,NULL);
return( true );
}

static int msgpopup_eh(GWindow popup,GEvent *event) {
    if ( event->type == et_expose ) {
	unichar_t *msg, *pt, *ept;
	int x,y, fh, temp;
	int as, ds, ld;

	popup_visible = true;
	pt = msg = (unichar_t *) GDrawGetUserData(popup);
	GDrawFontMetrics(popup_font,&as, &ds, &ld);
	fh = as+ds;
	y = x = GDrawPointsToPixels(popup,2);
	y += as;
	do {
	    temp = -1;
	    if (( ept = u_strchr(pt,'\n'))!=NULL )
		temp = ept-pt;
	    GDrawDrawText(popup,x,y,pt,temp,NULL,popup_foreground);
	    y += fh;
	    pt = ept+1;
	} while ( ept!=NULL && *pt!='\0' );
    } else if ( event->type == et_timer && event->u.timer.timer==popup_timer ) {
	GGadgetPopupTest(event);
    } else if ( event->type == et_mousemove || event->type == et_mouseup ||
	    event->type == et_mousedown || event->type == et_char ||
	    event->type == et_timer || event->type == et_crossing ) {
	GGadgetEndPopup();
    }
return( true );
}

void GGadgetPreparePopup(GWindow base,unichar_t *msg) {
    GGadgetEndPopup();
    if ( msg==NULL )
return;
    if ( popup==NULL ) {
	GWindowAttrs pattrs;
	GRect pos;

	pattrs.mask = wam_events|wam_nodecor|wam_positioned|wam_cursor|wam_backcol|wam_transient;
	pattrs.event_masks = -1;
	pattrs.nodecoration = true;
	pattrs.positioned = true;
	pattrs.cursor = ct_pointer;
	pattrs.background_color = popup_background;
	pattrs.transient = GWidgetGetTopWidget(base);
	pos.x = pos.y = 0; pos.width = pos.height = 1;
	popup = GDrawCreateTopWindow(GDrawGetDisplayOfWindow(base),&pos,
		msgpopup_eh,NULL,&pattrs);
	GDrawSetFont(popup,popup_font);
    }
    popup_timer = GDrawRequestTimer(popup,popup_delay,0,msg);
}

void _ggadget_redraw(GGadget *g) {
    GDrawRequestExpose(g->base, &g->r, false);
}

int _ggadget_noop(GGadget *g, GEvent *event) {
return( false );
}

static void GBoxFigureRect(GWindow gw, GBox *design, GRect *r, int isdef) {
    int scale = GDrawPointsToPixels(gw,1);
    int bp = GDrawPointsToPixels(gw,design->border_width)+
	     GDrawPointsToPixels(gw,design->padding)+
	     ((design->flags & box_foreground_border_outer)?scale:0)+
	     ((design->flags &
		 (box_foreground_border_inner|box_active_border_inner))?scale:0)+
	     (isdef && (design->flags & box_draw_default)?scale+GDrawPointsToPixels(gw,2):0);
    r->width += 2*bp;
    r->height += 2*bp;
}

static void GBoxFigureDiamond(GWindow gw, GBox *design, GRect *r, int isdef) {
    int scale = GDrawPointsToPixels(gw,1);
    int p = GDrawPointsToPixels(gw,design->padding);
    int b = GDrawPointsToPixels(gw,design->border_width)+
	     ((design->flags & box_foreground_border_outer)?scale:0)+
	     ((design->flags &
		 (box_foreground_border_inner|box_active_border_inner))?scale:0)+
	     (isdef && (design->flags & box_draw_default)?scale+GDrawPointsToPixels(gw,2):0);
    int xoff = r->width/2, yoff = r->height/2;

    if ( xoff<2*p ) xoff = 2*p;
    if ( yoff<2*p ) yoff = 2*p;
     r->width += 2*b+xoff;
     r->height += 2*b+yoff;
}

void _ggadgetFigureSize(GWindow gw, GBox *design, GRect *r, int isdef) {
    /* given that we want something with a client rectangle as big as r */
    /*  Then given the box shape, figure out how much of a rectangle we */
    /*  need around it */

    if ( r->width<=0 ) r->width = 1;
    if ( r->height<=0 ) r->height = 1;

    switch ( design->border_shape ) {
      case bs_rect:
	GBoxFigureRect(gw,design,r,isdef);
      break;
      case bs_roundrect:
	GBoxFigureRect(gw,design,r,isdef);
      break;
      case bs_elipse:
	GBoxFigureDiamond(gw,design,r,isdef);
      break;
      case bs_diamond:
	GBoxFigureDiamond(gw,design,r,isdef);
      break;
    }
}

static GGadget *GGadgetFindLastOpenGroup(GGadget *g) {
    for ( g=g->prev; g!=NULL && !g->opengroup; g = g->prev );
return( g );
}

static int GGadgetLMargin(GGadget *sigh, GGadget *lastOpenGroup) {
    if ( lastOpenGroup==NULL )
return( GDrawPointsToPixels(sigh->base,_GGadget_LeftMargin));
    else
return( lastOpenGroup->r.x + GDrawPointsToPixels(lastOpenGroup->base,_GGadget_Skip));
}

GGadget *_GGadget_Create(GGadget *g, struct gwindow *base, GGadgetData *gd,void *data, GBox *def) {
    GGadget *last, *lastopengroup;

    _GWidget_AddGGadget(base,g);
    g->r = gd->pos;
    if ( !(gd->flags&gg_pos_in_pixels) ) {
	g->r.x = GDrawPointsToPixels(base,g->r.x);
	g->r.y = GDrawPointsToPixels(base,g->r.y);
	if ( g->r.width!=-1 )
	    g->r.width = GDrawPointsToPixels(base,g->r.width);
	g->r.height = GDrawPointsToPixels(base,g->r.height);
    }
    last = g->prev;
    lastopengroup = GGadgetFindLastOpenGroup(g);
    if ( g->r.y==0 && !(gd->flags & gg_pos_use0)) {
	if ( last==NULL ) {
	    g->r.y = GDrawPointsToPixels(base,_GGadget_FirstLine);
	    if ( g->r.x==0 )
		g->r.x = GGadgetLMargin(g,lastopengroup);
	} else if ( gd->flags & gg_pos_newline ) {
	    int temp, y = last->r.y, nexty = last->r.y+last->r.height;
	    while ( last!=NULL && last->r.y==y ) {
		temp = last->r.y+last->r.height;
		if ( temp>nexty ) nexty = temp;
		last = last->prev;
	    }
	    g->r.y = nexty + GDrawPointsToPixels(base,_GGadget_LineSkip);
	    if ( g->r.x==0 )
		g->r.x = GGadgetLMargin(g,lastopengroup);
	} else {
	    g->r.y = last->r.y;
	}
    }
    if ( g->r.x == 0 && !(gd->flags & gg_pos_use0)) {
	last = g->prev;
	if ( last==NULL )
	    g->r.x = GGadgetLMargin(g,lastopengroup);
	else if ( gd->flags & gg_pos_under ) {
	    int onthisline=0, onprev=0, i;
	    GGadget *prev, *prevline;
	    /* see if we can find a gadget on the previous line that has the */
	    /*  same number of gadgets between it and the start of the line  */
	    /*  as this one has from the start of its line, then put at the  */
	    /*  same x offset */
	    for ( prev = last; prev!=NULL && prev->r.y==g->r.y; prev = prev->prev )
		++onthisline;
	    prevline = prev;
	    for ( ; prev!=NULL && prev->r.y==prevline->r.y ; prev = prev->prev )
		onprev++;
	    for ( prev = prevline, i=0; prev!=NULL && i<onprev-onthisline; prev = prev->prev);
	    if ( prev==NULL )
		g->r.x = prev->r.x;
	}
	if ( g->r.x==0 )
	    g->r.x = last->r.x + last->r.width + GDrawPointsToPixels(base,_GGadget_Skip);
    }

    g->mnemonic = islower(gd->mnemonic)?toupper(gd->mnemonic):gd->mnemonic;
    g->shortcut = islower(gd->shortcut)?toupper(gd->shortcut):gd->shortcut;
    g->short_mask = gd->short_mask;
    g->cid = gd->cid;
    g->data = data;
    g->popup_msg = u_copy(gd->popup_msg);
    g->handle_controlevent = gd->handle_controlevent;
    if ( gd->box == NULL )
	g->box = def;
    else if ( gd->flags & gg_dontcopybox )
	g->box = gd->box;
    else {
	g->free_box = true;
	g->box = galloc(sizeof(GBox));
	*g->box = *gd->box;
    }
    g->state = !(gd->flags&gg_visible) ? gs_invisible :
		  !(gd->flags&gg_enabled) ? gs_disabled :
		  gs_enabled;
    if ( !(gd->flags&gg_enabled) ) g->was_disabled = true;
return( g );
}

void _ggadget_destroy(GGadget *g) {
    if ( g==NULL )
return;
    _GWidget_RemoveGadget(g);
    if ( g->free_box )
	free( g->box );
    free(g->popup_msg);
    free(g);
}

void _GGadgetCloseGroup(GGadget *g) {
    GGadget *group = GGadgetFindLastOpenGroup(g);
    GGadget *prev;
    int maxx=0, maxy=0, temp;
    int bp = GBoxBorderWidth(g->base,g->box);

    if ( group==NULL )
return;
    for ( prev=g ; prev!=group; prev = prev->prev ) {
	temp = prev->r.x + prev->r.width;
	if ( temp>maxx ) maxx = temp;
	temp = prev->r.y + prev->r.height;
	if ( temp>maxy ) maxy = temp;
    }
    if ( group->prevlabel ) {
	prev = group->prev;
	temp = prev->r.x + prev->r.width;
	if ( temp>maxx ) maxx = temp;
	temp = prev->r.y + prev->r.height/2 ;
	if ( temp>maxy ) maxy = temp;
    }
    maxx += GDrawPointsToPixels(g->base,_GGadget_Skip);
    maxy += GDrawPointsToPixels(g->base,_GGadget_LineSkip);

    if ( group->r.width==0 ) {
	group->r.width = maxx - group->r.x;
	group->inner.width = group->r.width - 2*bp;
    }
    if ( group->r.height==0 ) {
	group->r.height = maxy - group->r.y;
	group->inner.height = group->r.y + group->r.height - bp -
		group->inner.y;
    }
    group->opengroup = false;
}

int GGadgetWithin(GGadget *g, int x, int y) {
    register GRect *r = &g->r;

    if ( x<r->x || y<r->y || x>=r->x+r->width || y>=r->y+r->height )
return( false );

return( true );
}

int GGadgetInnerWithin(GGadget *g, int x, int y) {
    register GRect *r = &g->inner;

    if ( x<r->x || y<r->y || x>=r->x+r->width || y>=r->y+r->height )
return( false );

return( true );
}

void _ggadget_underlineMnemonic(GWindow gw,int32 x,int32 y,unichar_t *label,
	unichar_t mnemonic, Color fg) {
    unichar_t *pt;
    int point = GDrawPointsToPixels(gw,1);
    int width;

    pt = u_strchr(label,mnemonic);
    if ( pt==NULL && isupper(mnemonic))
	pt = u_strchr(label,tolower(mnemonic));
    if ( pt==NULL || mnemonic=='\0' )
return;
    x += GDrawGetBiTextWidth(gw,label,pt-label,-1,NULL);
    width = GDrawGetTextWidth(gw,pt,1,NULL);
    GDrawSetLineWidth(gw,point);
    GDrawDrawLine(gw,x,y+2*point,x+width,y+2*point,fg);
}

void _ggadget_move(GGadget *g, int32 x, int32 y ) {
    g->inner.x = x+(g->inner.x-g->r.x);
    g->inner.y = y+(g->inner.y-g->r.y);
    g->r.x = x;
    g->r.y = y;
}

void _ggadget_resize(GGadget *g, int32 width, int32 height ) {
    g->inner.width = width-(g->r.width-g->inner.width);
    g->inner.height = height-(g->r.height-g->inner.height);
    g->r.width = width;
    g->r.height = height;
}

GRect *_ggadget_getsize(GGadget *g,GRect *rct) {
    *rct = g->r;
return( rct );
}

GRect *_ggadget_getinnersize(GGadget *g,GRect *rct) {
    *rct = g->inner;
return( rct );
}

void _ggadget_setvisible(GGadget *g,int visible) {
    g->state = !visible ? gs_invisible :
		g->was_disabled ? gs_disabled :
		gs_enabled;
    /* Make sure it isn't the focused _ggadget in the container !!!! */
    _ggadget_redraw(g);
}

void _ggadget_setenabled(GGadget *g,int enabled) {
    g->was_disabled = !enabled;
    if ( g->state!=gs_invisible ) {
	g->state = enabled? gs_enabled : gs_disabled;
	_ggadget_redraw(g);
    }
    /* Make sure it isn't the focused _ggadget in the container !!!! */
}

void GGadgetDestroy(GGadget *g) {
    (g->funcs->destroy)(g);
}

void GGadgetRedraw(GGadget *g) {
    (g->funcs->redraw)(g);
}

void GGadgetMove(GGadget *g,int32 x, int32 y ) {
    (g->funcs->move)(g,x,y);
}

void GGadgetResize(GGadget *g,int32 width, int32 height ) {
    (g->funcs->resize)(g,width,height);
}

GRect *GGadgetGetSize(GGadget *g,GRect *rct) {
return( (g->funcs->getsize)(g,rct) );
}

GRect *GGadgetGetInnerSize(GGadget *g,GRect *rct) {
return( (g->funcs->getinnersize)(g,rct) );
}

void GGadgetSetVisible(GGadget *g,int visible) {
    (g->funcs->setvisible)(g,visible);
}

void GGadgetSetEnabled(GGadget *g,int enabled) {
    (g->funcs->setenabled)(g,enabled);
}

void GGadgetSetUserData(GGadget *g,void *data) {
    g->data = data;
}

GWindow GGadgetGetWindow(GGadget *g) {
return( g->base );
}

int GGadgetGetCid(GGadget *g) {
return( g->cid );
}

void *GGadgetGetUserData(GGadget *g) {
return( g->data );
}

int GGadgetEditCmd(GGadget *g,enum editor_commands cmd) {
    if ( g->funcs->handle_editcmd!=NULL )
return(g->funcs->handle_editcmd)(g,cmd);
return( false );
}

void GGadgetSetTitle(GGadget *g,const unichar_t *title) {
    if ( g->funcs->set_title!=NULL )
	(g->funcs->set_title)(g,title);
}

const unichar_t *_GGadgetGetTitle(GGadget *g) {
    if ( g->funcs->_get_title!=NULL )
return( (g->funcs->_get_title)(g) );

return( NULL );
}

unichar_t *GGadgetGetTitle(GGadget *g) {
    if ( g->funcs->get_title!=NULL )
return( (g->funcs->get_title)(g) );
    else if ( g->funcs->_get_title!=NULL )
return( u_copy( (g->funcs->_get_title)(g) ));

return( NULL );
}

void GGadgetSetFont(GGadget *g,GFont *font) {
    if ( g->funcs->set_font!=NULL )
	(g->funcs->set_font)(g,font);
}

GFont *GGadgetGetFont(GGadget *g) {
    if ( g->funcs->get_font!=NULL )
return( (g->funcs->get_font)(g) );

return( NULL );
}

void GGadgetClearList(GGadget *g) {
    if ( g->funcs->clear_list!=NULL )
	(g->funcs->clear_list)(g);
}

void GGadgetSetList(GGadget *g, GTextInfo **ti, int32 copyit) {
    if ( g->funcs->set_list!=NULL )
	(g->funcs->set_list)(g,ti,copyit);
}

GTextInfo **GGadgetGetList(GGadget *g,int32 *len) {
    if ( g->funcs->get_list!=NULL )
return((g->funcs->get_list)(g,len));

return( NULL );
}

GTextInfo *GGadgetGetListItem(GGadget *g,int32 pos) {
    if ( g->funcs->get_list_item!=NULL )
return((g->funcs->get_list_item)(g,pos));

return( NULL );
}

void GGadgetSelectListItem(GGadget *g,int32 pos,int32 sel) {
    if ( g->funcs->select_list_item!=NULL )
	(g->funcs->select_list_item)(g,pos,sel);
}

void GGadgetSelectOneListItem(GGadget *g,int32 pos) {
    if ( g->funcs->select_one_list_item!=NULL )
	(g->funcs->select_one_list_item)(g,pos);
}

int32 GGadgetIsListItemSelected(GGadget *g,int32 pos) {
    if ( g->funcs->is_list_item_selected!=NULL )
return((g->funcs->is_list_item_selected)(g,pos));

return( 0 );
}

int32 GGadgetGetFirstListSelectedItem(GGadget *g) {
    if ( g->funcs->get_first_selection!=NULL )
return((g->funcs->get_first_selection)(g));
return( -1 );
}

void GGadgetScrollListToPos(GGadget *g,int32 pos) {
    if ( g->funcs->scroll_list_to_pos!=NULL )
	(g->funcs->scroll_list_to_pos)(g,pos);
}

void GGadgetScrollListToText(GGadget *g,const unichar_t *lab,int32 sel) {
    if ( g->funcs->scroll_list_to_text!=NULL )
	(g->funcs->scroll_list_to_text)(g,lab,sel);
}

void GGadgetSetListOrderer(GGadget *g,int (*orderer)(const void *, const void *)) {
    if ( g->funcs->set_list_orderer!=NULL )
	(g->funcs->set_list_orderer)(g,orderer);
}

void GGadgetsCreate(GWindow base, GGadgetCreateData *gcd) {
    int i;

    for ( i=0; gcd[i].creator!=NULL; ++i )
	gcd[i].ret = (gcd[i].creator)(base,&gcd[i].gd,gcd[i].data);
}
