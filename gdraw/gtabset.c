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
#include "gdraw.h"
#include "gresource.h"
#include "ggadgetP.h"
#include "ustring.h"
#include "gkeysym.h"

static GBox gtabset_box = { /* Don't initialize here */ 0 };
static FontInstance *gtabset_font = NULL;
static int gtabset_inited = false;

static void GTabSetInit() {

    GGadgetInit();
    _GGadgetCopyDefaultBox(&gtabset_box);
    gtabset_box.border_width = 1; gtabset_box.border_shape = bs_rect;
    gtabset_box.flags = 0;
    gtabset_font = _GGadgetInitDefaultBox("GTabSet.",&gtabset_box,NULL);
    gtabset_inited = true;
}

static void GTabSetChanged(GTabSet *gts,int oldsel) {
    GEvent e;

    e.type = et_controlevent;
    e.w = gts->g.base;
    e.u.control.subtype = et_radiochanged;
    e.u.control.g = &gts->g;
    if ( gts->g.handle_controlevent != NULL )
	(gts->g.handle_controlevent)(&gts->g,&e);
    else
	GDrawPostEvent(&e);
}

static int DrawLeftArrowTab(GWindow pixmap, GTabSet *gts, int x, int y ) {
    Color fg = gts->g.box->main_foreground;
    GPoint pts[5];
    int retx = x + gts->arrow_width, cnt;

    if ( fg==COLOR_DEFAULT ) fg = GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(pixmap));
    GBoxDrawTabOutline(pixmap,&gts->g,x,y,gts->arrow_width,gts->rowh,false);
    gts->haslarrow = true;
	y += (gts->rowh-gts->arrow_size)/2;
	x += (gts->arrow_width-gts->arrow_size/2)/2;
	cnt = 4;
	pts[0].y = (y+(gts->arrow_size-1)/2); 	pts[0].x = x;
	pts[1].y = y; 				pts[1].x = x + (gts->arrow_size-1)/2;
	pts[2].y = y+gts->arrow_size-1; 	pts[2].x = pts[1].x;
	pts[3] = pts[0];
	if ( !(gts->arrow_size&1 )) {
	    ++pts[3].y;
	    pts[4] = pts[0];
	    cnt = 5;
	}
	GDrawFillPoly(pixmap,pts,cnt,fg);
return( retx );
}

static int DrawRightArrowTab(GWindow pixmap, GTabSet *gts, int x, int y ) {
    Color fg = gts->g.box->main_foreground;
    GPoint pts[5];
    int retx = x + gts->arrow_width, cnt;

    if ( fg==COLOR_DEFAULT ) fg = GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(pixmap));
    GBoxDrawTabOutline(pixmap,&gts->g,x,y,gts->arrow_width,gts->rowh,false);
    gts->hasrarrow = true;
	y += (gts->rowh-gts->arrow_size)/2;
	x += (gts->arrow_width-gts->arrow_size/2)/2;
	cnt = 4;
	pts[0].y = (y+(gts->arrow_size-1)/2); 	pts[0].x = x + (gts->arrow_size-1)/2;
	pts[1].y = y; 				pts[1].x = x;
	pts[2].y = y+gts->arrow_size-1; 	pts[2].x = pts[1].x;
	pts[3] = pts[0];
	if ( !(gts->arrow_size&1 )) {
	    ++pts[3].y;
	    pts[4] = pts[0];
	    cnt = 5;
	}
	GDrawFillPoly(pixmap,pts,cnt,fg);
return( retx );
}

static int DrawTab(GWindow pixmap, GTabSet *gts, int i, int x, int y ) {
    Color fg = gts->tabs[i].disabled?gts->g.box->disabled_foreground:gts->g.box->main_foreground;

    if ( fg==COLOR_DEFAULT ) fg = GDrawGetDefaultForeground(GDrawGetDisplayOfWindow(pixmap));
    GBoxDrawTabOutline(pixmap,&gts->g,x,y,gts->tabs[i].width,gts->rowh,i==gts->sel);
    GDrawDrawBiText(pixmap,x+(gts->tabs[i].width-gts->tabs[i].tw)/2,y+gts->rowh-gts->ds,
	    gts->tabs[i].name,-1,NULL,fg);
    gts->tabs[i].x = x;
    x += gts->tabs[i].width;
return( x );
}

static int gtabset_expose(GWindow pixmap, GGadget *g, GEvent *event) {
    GTabSet *gts = (GTabSet *) g;
    int x,y,i,rd, dsel;
    GRect old1, bounds;
    int yoff = (gts->rcnt==1?GBoxBorderWidth(pixmap,g->box):0);

    if ( g->state == gs_invisible )
return( false );

    GDrawPushClip(pixmap,&g->r,&old1);

    GBoxDrawBackground(pixmap,&g->r,g->box,
	    g->state==gs_enabled? gs_pressedactive: g->state,false);
    bounds = g->r; bounds.y += gts->rcnt*gts->rowh+yoff; bounds.height -= gts->rcnt*gts->rowh-yoff;
    GBoxDrawBorder(pixmap,&bounds,g->box,g->state,false);
    GDrawSetFont(pixmap,gts->font);

    gts->haslarrow = gts->hasrarrow = false;
    if ( gts->scrolled ) {
	x = g->r.x + GBoxBorderWidth(pixmap,gts->g.box);
	y = g->r.y+yoff;
	dsel = 0;
	if ( gts->toff!=0 )
	    x = DrawLeftArrowTab(pixmap,gts,x,y);
	for ( i=gts->toff;
		(i==gts->tabcnt-1 && x+gts->tabs[i].width < g->r.width) ||
		(i<gts->tabcnt-1 && x+gts->tabs[i].width < g->r.width-gts->arrow_width ) ;
		++i ) {
	    if ( i!=gts->sel )
		x = DrawTab(pixmap,gts,i,x,y);
	    else {
		gts->tabs[i].x = x;
		x += gts->tabs[i].width;
		dsel = 1;
	    }
	}
	if ( i!=gts->tabcnt ) {
	    int p = gts->g.inner.x+gts->g.inner.width - gts->arrow_width;
	    if ( p>x ) x=p;
	    x = DrawRightArrowTab(pixmap,gts,x,y);
	    gts->tabs[i].x = 0x7fff;
	}
	/* This one draws on top of the others, must come last */
	if ( dsel )
	    DrawTab(pixmap,gts,gts->sel,gts->tabs[gts->sel].x, g->r.y + (gts->rcnt-1) * gts->rowh + yoff );
    } else {
	/* r is real row, rd is drawn pos */
	/* rd is 0 at the top of the ggadget */
	/* r is 0 when it contains tabs[0], (the index in the rowstarts array) */
	for ( rd = 0; rd<gts->rcnt; ++rd ) {
	    int r = (gts->rcnt-1-rd+gts->active_row)%gts->rcnt;
	    y = g->r.y + rd * gts->rowh + yoff;
	    x = g->r.x + (gts->rcnt-1-rd) * gts->offset_per_row + GBoxBorderWidth(pixmap,gts->g.box);
	    for ( i = gts->rowstarts[r]; i<gts->rowstarts[r+1]; ++i )
		if ( i==gts->sel ) {
		    gts->tabs[i].x = x;
		    x += gts->tabs[i].width;
		} else
		    x = DrawTab(pixmap,gts,i,x,y);
	}
	/* This one draws on top of the others, must come last */
	DrawTab(pixmap,gts,gts->sel,gts->tabs[gts->sel].x, g->r.y + (gts->rcnt-1) * gts->rowh + yoff );
    }
    GDrawPopClip(pixmap,&old1);
return( true );
}

static int GTabSetRCnt(GTabSet *gts, int totwidth) {
    int i, off, r, width;
    int bp = GBoxBorderWidth(gts->g.base,gts->g.box) + GDrawPointsToPixels(gts->g.base,5);

    width = totwidth;
    for ( i = off = r = 0; i<gts->tabcnt; ++i ) {
	if ( off!=0 && width-(gts->tabs[i].tw+2*bp)< 0 ) {
	    off = 0; ++r;
	    width = totwidth;
	}
	width -= gts->tabs[i].width;
	gts->tabs[i].x = off;
	off ++;
    }
return( r+1 );
}

static int GTabSetGetLineWidth(GTabSet *gts,int r) {
    int i, width = 0;

    for ( i=gts->rowstarts[r]; i<gts->rowstarts[r+1]; ++i )
	width += gts->tabs[i].width;
return( width );
}

static void GTabSetDistributePixels(GTabSet *gts,int r, int widthdiff) {
    int diff, off, i;

    diff = widthdiff/(gts->rowstarts[r+1]-gts->rowstarts[r]);
    off = widthdiff-diff*(gts->rowstarts[r+1]-gts->rowstarts[r]);
    for ( i=gts->rowstarts[r]; i<gts->rowstarts[r+1]; ++i ) {
	gts->tabs[i].width += diff;
	if ( off ) {
	    ++gts->tabs[i].width;
	    --off;
	}
    }
}

/* We have rearranged the rows of tabs. We want to make sure that each row */
/*  row is at least as wide as the row above it */
static void GTabSetFigureWidths(GTabSet *gts) {
    int bp = GBoxBorderWidth(gts->g.base,gts->g.box) + GDrawPointsToPixels(gts->g.base,5);
    int i, rd;
    int oldwidth=0, width;

    /* set all row widths to default values */
    for ( i=0; i<gts->tabcnt; ++i ) {
	gts->tabs[i].width = gts->tabs[i].tw + 2*bp;
    }
    /* r is real row, rd is drawn pos */
    /* rd is 0 at the top of the ggadget */
    /* r is 0 when it contains tabs[0], (the index in the rowstarts array) */
    if ( ( gts->filllines && gts->rcnt>1 ) || (gts->fill1line && gts->rcnt==1) ) {
	for ( rd = 0; rd<gts->rcnt; ++rd ) {
	    int r = (rd+gts->rcnt-1-gts->active_row)%gts->rcnt;
	    int totwidth = gts->g.r.width-2*GBoxBorderWidth(gts->g.base,gts->g.box) -
		    (gts->rcnt-1-rd)*gts->offset_per_row;
	    width = GTabSetGetLineWidth(gts,r);
	    GTabSetDistributePixels(gts,r,totwidth-width);
	}
    } else {
	for ( rd = 0; rd<gts->rcnt; ++rd ) {		/* r is real row, rd is drawn pos */
	    int r = (rd+gts->rcnt-1-gts->active_row)%gts->rcnt;
	    width = GTabSetGetLineWidth(gts,r) + (gts->rcnt-1-rd)*gts->offset_per_row;
	    if ( rd==0 )
		oldwidth = width;
	    else if ( oldwidth>width )
		GTabSetDistributePixels(gts,r,oldwidth-width);
	    else
		oldwidth = width;
	}
    }
}

/* Something happened which would change how many things fit on a line */
/*  (initialization, change of font, addition or removal of tab, etc.) */
/* Figure out how many rows we need and then how best to divide the tabs */
/*  between those rows, and then the widths of each tab */
static void GTabSetRemetric(GTabSet *gts) {
    int bbp = GBoxBorderWidth(gts->g.base,gts->g.box);
    int bp = bbp + GDrawPointsToPixels(gts->g.base,5);
    int r, r2, width, i;
    int as, ds, ld;

    GDrawSetFont(gts->g.base,gts->font);
    GDrawFontMetrics(gts->font,&as,&ds,&ld);
    gts->rowh = as+ds + bbp+GDrawPointsToPixels(gts->g.base,3);
    gts->ds = ds + bbp+GDrawPointsToPixels(gts->g.base,1);
    gts->arrow_size = as+ds;
    gts->arrow_width = gts->arrow_size + 2*GBoxBorderWidth(gts->g.base,gts->g.box);

    for ( i=0; i<gts->tabcnt; ++i ) {
	gts->tabs[i].tw = GDrawGetTextWidth(gts->g.base,gts->tabs[i].name,-1,NULL);
	gts->tabs[i].width = gts->tabs[i].tw + 2*bp;
    }

    if ( gts->scrolled ) {
	free(gts->rowstarts);
	gts->rowstarts = malloc(2*sizeof(16));
	gts->rowstarts[0] = 0; gts->rowstarts[1] = gts->tabcnt;
	gts->rcnt = 1;
    } else {
	width = gts->g.r.width-2*GBoxBorderWidth(gts->g.base,gts->g.box);
	r = GTabSetRCnt(gts,width);
	if ( gts->offset_per_row!=0 && r>1 )
	    while ( (r2 = GTabSetRCnt(gts,width-(r-1)*gts->offset_per_row))!=r )
		r = r2;
	free(gts->rowstarts);
	gts->rowstarts = galloc((r+1)*sizeof(int16));
	gts->rcnt = r;
	gts->rowstarts[r] = gts->tabcnt;
	for ( i=r=0; i<gts->tabcnt; ++i ) {
	    if ( gts->tabs[i].x==0 ) 
		gts->rowstarts[r++] = i;
	}
	/* if there is a single tab on the last line and there are more on */
	/*  the previous line, then things look nicer if we move one of the */
	/*  tabs from the previous line onto the last line */
	/*  Providing it fits, of course */
	if ( gts->rowstarts[r]-gts->rowstarts[r-1]==1 && r>1 &&
		gts->rowstarts[r-1]-gts->rowstarts[r-2]>1 &&
		gts->tabs[i-1].width+gts->tabs[i-2].width < width-(r-1)*gts->offset_per_row )
	    --gts->rowstarts[r-1];

	GTabSetFigureWidths(gts);
    }
}

static void GTabSetChangeSel(GTabSet *gts, int sel) {
    int i, width;
    int oldsel = gts->sel;

    if ( sel==-2 )		/* left arrow */
	--gts->toff;
    else if ( sel==-3 )
	++gts->toff;
    else if ( sel<0 || sel>=gts->tabcnt || gts->tabs[sel].disabled )
return;
    else {
	for ( i=0; i<gts->rcnt && sel>=gts->rowstarts[i+1]; ++i );
	if ( gts->active_row != i ) {
	    gts->active_row = i;
	    if ( gts->rcnt>1 && (!gts->filllines || gts->offset_per_row!=0))
		GTabSetFigureWidths(gts);
	}
	gts->sel = sel;
	if ( sel<gts->toff )
	    gts->toff = sel;
	else if ( gts->scrolled ) {
	    for ( i=gts->toff; i<sel && gts->tabs[i].x!=0x7fff; ++i );
	    if ( gts->tabs[i].x==0x7fff ) {
		width = gts->g.r.width-2*gts->arrow_width;	/* it will have a left arrow */
		if ( sel!=gts->tabcnt )
		    width -= gts->arrow_width;		/* it might have a right arrow */
		for ( i=sel; i>=0 && width-gts->tabs[i].width>=0; --i )
		    width -= gts->tabs[i].width;
		if ( ++i>sel ) i = sel;
		gts->toff = i;
	    }
	}
	GTabSetChanged(gts,oldsel);
	if ( gts->tabs[oldsel].w!=NULL )
	    GDrawSetVisible(gts->tabs[oldsel].w,false);
	if ( gts->tabs[gts->sel].w!=NULL )
	    GDrawSetVisible(gts->tabs[gts->sel].w,true);
    }
    _ggadget_redraw(&gts->g);
}

static int gtabset_mouse(GGadget *g, GEvent *event) {
    GTabSet *gts = (GTabSet *) g;

    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active && g->state!=gs_focused ))
return( false );

    if ( event->type == et_crossing ) {
return( true );
    } else if ( event->type == et_mousemove ) {
return( true );
    } else {
	int i, sel= -1, l;
	if ( event->u.mouse.y < gts->g.r.y || event->u.mouse.y >= gts->g.inner.y )
	    sel = -1;
	else if ( gts->scrolled ) {
	    if ( gts->haslarrow && event->u.mouse.x<gts->tabs[gts->toff].x )
		sel = -2;	/* left arrow */
	    else {
		for ( i=gts->toff;
			i<gts->tabcnt && event->u.mouse.x>=gts->tabs[i].x+gts->tabs[i].width;
			++i );
		if ( gts->hasrarrow && gts->tabs[i].x==0x7fff &&
			event->u.mouse.x>=gts->tabs[i-1].x+gts->tabs[i-1].width )
		    sel = -3;
		else
		    sel = i;
	    }
	} else {
	    l = (event->u.mouse.y-gts->g.r.y)/gts->rowh;	/* screen row */
	    if ( l>=gts->rcnt ) l = gts->rcnt-1;		/* can happen on single line tabsets (there's extra space then) */
	    l = (l+gts->rcnt-1-gts->active_row)%gts->rcnt;	/* internal row number */
	    if ( event->u.mouse.x<gts->tabs[gts->rowstarts[l]].x )
		sel = -1;
	    else if ( event->u.mouse.x>=gts->tabs[gts->rowstarts[l+1]-1].x+gts->tabs[gts->rowstarts[l+1]-1].width )
		sel = -1;
	    else {
		for ( i=gts->rowstarts[l]; i<gts->rowstarts[l+1] &&
			event->u.mouse.x>=gts->tabs[i].x+gts->tabs[i].width; ++i );
		sel = i;
	    }
	}
	if ( event->type==et_mousedown ) {
	    gts->pressed = true;
	    gts->pressed_sel = sel;
	} else {
	    if ( gts->pressed && gts->pressed_sel == sel )
		GTabSetChangeSel(gts,sel);
	    gts->pressed = false;
	    gts->pressed_sel = -1;
	}
    }
return( true );
}

static int gtabset_key(GGadget *g, GEvent *event) {
    GTabSet *gts = (GTabSet *) g;
    int i;

    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active && g->state!=gs_focused ))
return(false);
    if ( event->type == et_charup )
return( true );

    if (event->u.chr.keysym == GK_Left || event->u.chr.keysym == GK_KP_Left ) {
	for ( i = gts->sel-1; i>0 && gts->tabs[i].disabled; --i );
	GTabSetChangeSel(gts,i);
return( true );
    } else if (event->u.chr.keysym == GK_Right || event->u.chr.keysym == GK_KP_Right ) {
	for ( i = gts->sel+1; i<gts->tabcnt-1 && gts->tabs[i].disabled; ++i );
	GTabSetChangeSel(gts,i);
return( true );
    }
return( false );
}

static int gtabset_focus(GGadget *g, GEvent *event) {

    if ( !g->takes_input || (g->state!=gs_enabled && g->state!=gs_active ))
return(false);

return( true );
}

static void gtabset_destroy(GGadget *g) {
    GTabSet *gts = (GTabSet *) g;
    int i;

    if ( gts==NULL )
return;
    free(gts->rowstarts);
    for ( i=0; i<gts->tabcnt; ++i ) {
	free(gts->tabs[i].name);
/* This has already been done */
/*	if ( gts->tabs[i].w!=NULL ) */
/*	    GDrawDestroyWindow(gts->tabs[i].w); */
    }
    free(gts->tabs);
    _ggadget_destroy(g);
}

static void GTabSetSetFont(GGadget *g,FontInstance *new) {
    GTabSet *gts = (GTabSet *) g;
    gts->font = new;
    GTabSetRemetric(gts);
}

static FontInstance *GTabSetGetFont(GGadget *g) {
    GTabSet *gts = (GTabSet *) g;
return( gts->font );
}

static void _gtabset_redraw(GGadget *g) {
    GTabSet *gts = (GTabSet *) g;
    int i;

    GDrawRequestExpose(g->base, &g->r, false);
    i = gts->sel;
    if ( gts->tabs[i].w!=NULL )
	GDrawRequestExpose(gts->tabs[i].w, NULL, false);
}

static void _gtabset_move(GGadget *g, int32 x, int32 y ) {
    GTabSet *gts = (GTabSet *) g;
    int i;
    int32 nx = x+g->inner.x-g->r.x, ny = y+g->inner.y-g->r.y;

    for ( i=0; i<gts->tabcnt; ++i ) if ( gts->tabs[i].w!=NULL )
	GDrawMove(gts->tabs[i].w,nx,ny);
    _ggadget_move(g,x,y);
}

static void _gtabset_resize(GGadget *g, int32 width, int32 height ) {
    GTabSet *gts = (GTabSet *) g;
    int i;

    _ggadget_resize(g,width,height);
    for ( i=0; i<gts->tabcnt; ++i ) if ( gts->tabs[i].w!=NULL )
	GDrawResize(gts->tabs[i].w,g->inner.width,g->inner.height);
}

static void _gtabset_setvisible(GGadget *g,int visible) {
    GTabSet *gts = (GTabSet *) g;
    int i;

    _ggadget_setvisible(g,visible);
    i = gts->sel;
    if ( gts->tabs[i].w!=NULL )
	GDrawSetVisible(gts->tabs[i].w, visible);
}

struct gfuncs gtabset_funcs = {
    0,
    sizeof(struct gfuncs),

    gtabset_expose,
    gtabset_mouse,
    gtabset_key,
    NULL,
    gtabset_focus,
    NULL,
    NULL,

    _gtabset_redraw,
    _gtabset_move,
    _gtabset_resize,
    _gtabset_setvisible,
    _ggadget_setenabled,		/* Doesn't work right */
    _ggadget_getsize,
    _ggadget_getinnersize,

    gtabset_destroy,

    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    GTabSetSetFont,
    GTabSetGetFont
};

static int sendtoparent_eh(GWindow gw, GEvent *event) {
    if ( event->type==et_controlevent ) {
	event->w = GDrawGetParentWindow(gw);
	GDrawPostEvent(event);
    } else if ( event->type==et_char )
return( false );

return( true );
}

GGadget *GTabSetCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GTabSet *gts = gcalloc(1,sizeof(GTabSet));
    int i, bp;
    GRect r;
    GWindowAttrs childattrs;

    memset(&childattrs,0,sizeof(childattrs));
    childattrs.mask = wam_events;
    childattrs.event_masks = -1;

    if ( !gtabset_inited )
	GTabSetInit();
    gts->g.funcs = &gtabset_funcs;
    _GGadget_Create(&gts->g,base,gd,data,&gtabset_box);
    gts->font = gtabset_font;

    gts->g.takes_input = true; gts->g.takes_keyboard = true; gts->g.focusable = true;

    GDrawGetSize(base,&r);
    if ( gd->pos.x <= 0 )
	gts->g.r.x = GDrawPointsToPixels(base,2);
    if ( gd->pos.y <= 0 )
	gts->g.r.y = GDrawPointsToPixels(base,2);
    if ( gd->pos.width<=0 )
	gts->g.r.width = r.width - gts->g.r.x - GDrawPointsToPixels(base,2);
    if ( gd->pos.height<=0 )
	gts->g.r.height = r.height - gts->g.r.y - GDrawPointsToPixels(base,26);

    for ( i=0; gd->u.tabs[i].text!=NULL; ++i );
    gts->tabcnt = i;
    gts->tabs = galloc(i*sizeof(struct tabs));
    for ( i=0; gd->u.tabs[i].text!=NULL; ++i ) {
	if ( gd->u.tabs[i].text_in_resource )
	    gts->tabs[i].name = u_copy(GStringGetResource((int) (gd->u.tabs[i].text),NULL));
	else if ( gd->u.tabs[i].text_is_1byte )
	    gts->tabs[i].name = uc_copy((char *) (gd->u.tabs[i].text));
	else
	    gts->tabs[i].name = u_copy(gd->u.tabs[i].text);
	gts->tabs[i].disabled = gd->u.tabs[i].disabled;
	if ( gd->u.tabs[i].selected && !gts->tabs[i].disabled )
	    gts->sel = i;
    }
    if ( gd->flags&gg_tabset_scroll ) gts->scrolled = true;
    if ( gd->flags&gg_tabset_filllines ) gts->filllines = true;
    if ( gd->flags&gg_tabset_fill1line ) gts->fill1line = true;
    gts->offset_per_row = GDrawPointsToPixels(base,2);
    GTabSetRemetric(gts);

    bp = GBoxBorderWidth(base,gts->g.box);
    gts->g.inner = gts->g.r;
    gts->g.inner.x += bp; gts->g.inner.width -= 2*bp;
    gts->g.inner.y += gts->rcnt*gts->rowh; gts->g.inner.height -= bp+gts->rcnt*gts->rowh;
    if ( gts->rcnt==1 ) {
	gts->g.inner.y += bp; gts->g.inner.height -= bp;
    }

    for ( i=0; gd->u.tabs[i].text!=NULL; ++i ) if ( gd->u.tabs[i].gcd!=NULL ) {
	gts->tabs[i].w = GDrawCreateSubWindow(base,&gts->g.inner,sendtoparent_eh,GDrawGetUserData(base),&childattrs);
	GGadgetsCreate(gts->tabs[i].w,gd->u.tabs[i].gcd);
	if ( gts->sel==i )
	    GDrawSetVisible(gts->tabs[i].w,true);
    }

    if ( gd->flags & gg_group_end )
	_GGadgetCloseGroup(&gts->g);

return( &gts->g );
}

int GTabSetGetSel(GGadget *g) {
    GTabSet *gts = (GTabSet *) g;
return( gts->sel );
}
