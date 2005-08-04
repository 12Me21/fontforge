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
#include "groups.h"
#include "psfont.h"
#ifndef FONTFORGE_CONFIG_GTK
#include <ustring.h>
#include <gkeysym.h>
#include <utype.h>
#include <gfile.h>
#include <chardata.h>
#include <gresource.h>
#endif
#include <math.h>
#include <unistd.h>

int onlycopydisplayed = 0;
int copymetadata = 0;

#define XOR_COLOR	0x505050
#define	FV_LAB_HEIGHT	15

#ifdef FONTFORGE_CONFIG_GDRAW
/* GTK window icons are set by glade */
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
#endif

enum glyphlable { gl_glyph, gl_name, gl_unicode, gl_encoding };
int default_fv_font_size = 24, default_fv_antialias=true,
	default_fv_bbsized=true,
	default_fv_showhmetrics=false, default_fv_showvmetrics=false,
	default_fv_glyphlabel = gl_glyph;
#define METRICS_BASELINE 0x0000c0
#define METRICS_ORIGIN	 0xc00000
#define METRICS_ADVANCE	 0x008000
FontView *fv_list=NULL;

#if defined(FONTFORGE_CONFIG_GTK)
# define FV_From_MI(menuitem)	\
	(FontView *) g_object_get_data( \
		G_OBJECT( gtk_widget_get_toplevel( GTK_WIDGET( menuitem ))),\
		"data" )
#endif

void FVToggleCharChanged(SplineChar *sc) {
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    int i, j;
    int pos;
    FontView *fv;

    for ( fv = sc->parent->fv; fv!=NULL; fv=fv->nextsame ) {
	if ( fv->sf!=sc->parent )		/* Can happen in CID fonts if char's parent is not currently active */
    continue;
	if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
    continue;
	for ( pos=0; pos<fv->map->enccount; ++pos ) if ( fv->map->map[pos]==sc->orig_pos ) {
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
 /* Normally we should be checking against fv->rowcnt (rather than <=rowcnt) */
 /*  but every now and then the WM forces us to use a window size which doesn't */
 /*  fit our expectations (maximized view) and we must be prepared for half */
 /*  lines */
	    if ( i>=0 && i<=fv->rowcnt ) {
# ifdef FONTFORGE_CONFIG_GDRAW
		GRect r;
		Color bg;
		if ( sc->color!=COLOR_DEFAULT )
		    bg = sc->color;
		else if ( sc->layers[ly_back].splines!=NULL || sc->layers[ly_back].images!=NULL )
		    bg = 0x808080;
		else
		    bg = GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v));
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = FV_LAB_HEIGHT-1;
		GDrawSetXORBase(fv->v,bg);
		GDrawSetXORMode(fv->v);
		GDrawFillRect(fv->v,&r,0x000000);
		GDrawSetCopyMode(fv->v);
# elif defined(FONTFORGE_CONFIG_GTK)
		GdkGC *gc = fv->v->style->fg_gc[fv->v->state];
		GdkGCValues values;
		struct GdkColor bg;
		gdk_gc_get_values(gc,&values);
		bg.pixel = -1;
		if ( sc->color!=COLOR_DEFAULT ) {
		    bg.red   = ((sc->color>>16)&0xff)<<8;
		    bg.green = ((sc->color>>8 )&0xff)<<8;
		    bg.blue  = ((sc->color    )&0xff)<<8;
		} else if ( sc->layers[ly_back].splines!=NULL || sc->layers[ly_back].images!=NULL )
		    bg.red = bg.green = bg.blue = 0x8000;
		else
		    bg = values.background;
		/* Bug!!! This only works on RealColor */
		bg.red ^= values.foreground.red;
		bg.green ^= values.foreground.green;
		bg.blue ^= values.foreground.blue;
		/* End bug */
		gdk_gc_set_function(gc,GDK_XOR);
		gdk_gc_set_foreground(gc, &bg);
		gdk_draw_rectangle(fv->v->window, gc, TRUE,
			j*fv->cbw+1, i*fv->cbh+1,  fv->cbw-1, fv->lab_height);
		gdk_gc_set_values(gc,&values,
			GDK_GC_FOREGROUND | GDK_GC_FUNCTION );
# endif
	    }
	}
    }
#endif
}

void FVMarkHintsOutOfDate(SplineChar *sc) {
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    int i, j;
    int pos;
    FontView *fv;

    if ( sc->parent->onlybitmaps || sc->parent->multilayer || sc->parent->strokedfont || sc->parent->order2 )
return;
    for ( fv = sc->parent->fv; fv!=NULL; fv=fv->nextsame ) {
	if ( fv->sf!=sc->parent )		/* Can happen in CID fonts if char's parent is not currently active */
    continue;
	if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
    continue;
	for ( pos=0; pos<fv->map->enccount; ++pos ) if ( fv->map->map[pos]==sc->orig_pos ) {
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
 /* Normally we should be checking against fv->rowcnt (rather than <=rowcnt) */
 /*  but every now and then the WM forces us to use a window size which doesn't */
 /*  fit our expectations (maximized view) and we must be prepared for half */
 /*  lines */
	    if ( i>=0 && i<=fv->rowcnt ) {
# ifdef FONTFORGE_CONFIG_GDRAW
		GRect r;
		Color hintcol = 0x0000ff;
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = fv->lab_height-1;
		GDrawDrawLine(fv->v,r.x,r.y,r.x,r.y+r.height-1,hintcol);
		GDrawDrawLine(fv->v,r.x+1,r.y,r.x+1,r.y+r.height-1,hintcol);
		GDrawDrawLine(fv->v,r.x+r.width-1,r.y,r.x+r.width-1,r.y+r.height-1,hintcol);
		GDrawDrawLine(fv->v,r.x+r.width-2,r.y,r.x+r.width-2,r.y+r.height-1,hintcol);
# elif defined(FONTFORGE_CONFIG_GTK)
		GdkGC *gc = fv->v->style->fg_gc[fv->v->state];
		GdkGCValues values;
# endif
	    }
	}
    }
#endif
}

static void FVToggleCharSelected(FontView *fv,int enc) {
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    int i, j;

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;

    i = enc / fv->colcnt;
    j = enc - i*fv->colcnt;
    i -= fv->rowoff;
 /* Normally we should be checking against fv->rowcnt (rather than <=rowcnt) */
 /*  but every now and then the WM forces us to use a window size which doesn't */
 /*  fit our expectations (maximized view) and we must be prepared for half */
 /*  lines */
    if ( i>=0 && i<=fv->rowcnt ) {
# ifdef FONTFORGE_CONFIG_GDRAW
	GRect r;
	r.x = j*fv->cbw+1; r.width = fv->cbw-1;
	r.y = i*fv->cbh+fv->lab_height+1; r.height = fv->cbw;
	GDrawSetXORBase(fv->v,GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v)));
	GDrawSetXORMode(fv->v);
	GDrawFillRect(fv->v,&r,XOR_COLOR);
	GDrawSetCopyMode(fv->v);
# elif defined(FONTFORGE_CONFIG_GTK)
	GdkGC *gc = fv->v->style->fg_gc[fv->v->state];
	GdkGCValues values;
	gdk_gc_get_values(gc,&values);
	gdk_gc_set_function(gc,GDK_XOR);
	gdk_gc_set_foreground(gc, &values.background);
	gdk_draw_rectangle(fv->v->window, gc, TRUE,
		j*fv->cbw+1, i*fv->cbh+fv->lab_height+1,  fv->cbw-1, fv->cbw);
	gdk_gc_set_values(gc,&values,
		GDK_GC_FOREGROUND | GDK_GC_FUNCTION );
# endif
    }
#endif
}

void SFUntickAll(SplineFont *sf) {
    int i;

    for ( i=0; i<sf->glyphcnt; ++i ) if ( sf->glyphs[i]!=NULL )
	sf->glyphs[i]->ticked = false;
}

void FVDeselectAll(FontView *fv) {
    int i;

    for ( i=0; i<fv->map->enccount; ++i ) {
	if ( fv->selected[i] ) {
	    fv->selected[i] = false;
	    FVToggleCharSelected(fv,i);
	}
    }
    fv->sel_index = 0;
}

static void FVInvertSelection(FontView *fv) {
    int i;

    for ( i=0; i<fv->map->enccount; ++i ) {
	fv->selected[i] = !fv->selected[i];
	FVToggleCharSelected(fv,i);
    }
    fv->sel_index = 1;
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void FVSelectAll(FontView *fv) {
    int i;

    for ( i=0; i<fv->map->enccount; ++i ) {
	if ( !fv->selected[i] ) {
	    fv->selected[i] = true;
	    FVToggleCharSelected(fv,i);
	}
    }
    fv->sel_index = 1;
}

static void FVSelectColor(FontView *fv, uint32 col, int door) {
    int i, any=0;
    uint32 sccol;
    SplineChar **glyphs = fv->sf->glyphs;

    for ( i=0; i<fv->map->enccount; ++i ) {
	int gid = fv->map->map[i];
	sccol =  ( gid==-1 || glyphs[gid]==NULL ) ? COLOR_DEFAULT : glyphs[gid]->color;
	if ( (door && !fv->selected[i] && sccol==col) ||
		(!door && fv->selected[i]!=(sccol==col)) ) {
	    fv->selected[i] = !fv->selected[i];
	    if ( fv->selected[i] ) any = true;
	    FVToggleCharSelected(fv,i);
	}
    }
    fv->sel_index = any;
}

static void FVReselect(FontView *fv, int newpos) {
    int i, start, end;

    if ( newpos<0 ) newpos = 0;
    else if ( newpos>=fv->map->enccount ) newpos = fv->map->enccount-1;

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
# ifdef FONTFORGE_CONFIG_GDRAW
    if ( newpos>=0 && newpos<fv->map->enccount && (i = fv->map->map[newpos])!=-1 &&
	    fv->sf->glyphs[i]!=NULL &&
	    fv->sf->glyphs[i]->unicodeenc>=0 && fv->sf->glyphs[i]->unicodeenc<0x10000 )
	GInsCharSetChar(fv->sf->glyphs[i]->unicodeenc);
# endif
}
#endif

static void _SplineFontSetUnChanged(SplineFont *sf) {
    int i;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    int was = sf->changed;
    FontView *fvs;
#endif
    BDFFont *bdf;

    sf->changed = false;
    SFClearAutoSave(sf);
    for ( i=0; i<sf->glyphcnt; ++i ) if ( sf->glyphs[i]!=NULL )
	if ( sf->glyphs[i]->changed ) {
	    sf->glyphs[i]->changed = false;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
	    SCRefreshTitles(sf->glyphs[i]);
#endif
	}
    for ( bdf=sf->bitmaps; bdf!=NULL; bdf=bdf->next )
	for ( i=0; i<bdf->glyphcnt; ++i ) if ( bdf->glyphs[i]!=NULL )
	    bdf->glyphs[i]->changed = false;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    if ( was && sf->fv!=NULL && sf->fv->v!=NULL )
# ifdef FONTFORGE_CONFIG_GDRAW
	GDrawRequestExpose(sf->fv->v,NULL,false);
# elif defined(FONTFORGE_CONFIG_GTK)
	gtk_widget_queue_draw(sf->fv->v);
# endif
    if ( was )
	for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->next )
	    FVSetTitle(fvs);
#endif
    for ( i=0; i<sf->subfontcnt; ++i )
	_SplineFontSetUnChanged(sf->subfonts[i]);
}

void SplineFontSetUnChanged(SplineFont *sf) {
    int i;

    if ( sf->cidmaster!=NULL ) sf = sf->cidmaster;
    if ( sf->mm!=NULL ) sf = sf->mm->normal;
    _SplineFontSetUnChanged(sf);
    if ( sf->mm!=NULL )
	for ( i=0; i<sf->mm->instance_count; ++i )
	    _SplineFontSetUnChanged(sf->mm->instances[i]);
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void FVFlattenAllBitmapSelections(FontView *fv) {
    BDFFont *bdf;
    int i;

    for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	for ( i=0; i<bdf->glyphcnt; ++i )
	    if ( bdf->glyphs[i]!=NULL && bdf->glyphs[i]->selection!=NULL )
		BCFlattenFloat(bdf->glyphs[i]);
    }
}

static int AskChanged(SplineFont *sf) {
    int ret;
#if defined(FONTFORGE_CONFIG_GDRAW)
    static int buts[] = { _STR_Save, _STR_Dontsave, _STR_Cancel, 0 };
#elif defined(FONTFORGE_CONFIG_GTK)
    char *buts[4];
#endif
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
#if defined(FONTFORGE_CONFIG_GDRAW)
    ret = GWidgetAskR( _STR_Fontchange,buts,0,2,_STR_FontChangedMsg,fontname,filename);
#elif defined(FONTFORGE_CONFIG_GTK)
    buts[0] = GTK_STOCK_SAVE;
    buts[1] = _("_Don't Save");
    buts[2] = GTK_STOCK_CANCEL;
    buts[3] = NULL;
    ret = gwwv_ask( _("Font changed"),buts,0,2,_("Font %1$.40s in file %2$.40s has been changed.\nDo you want to save it?"),fontname,filename);
#endif
return( ret );
}

static int RevertAskChanged(char *fontname,char *filename) {
    int ret;
#if defined(FONTFORGE_CONFIG_GDRAW)
    static int buts[] = { _STR_Revert, _STR_Cancel, 0 };
#elif defined(FONTFORGE_CONFIG_GTK)
    char *buts[3];
#endif

    if ( filename==NULL ) filename = "untitled.sfd";
    filename = GFileNameTail(filename);
#if defined(FONTFORGE_CONFIG_GDRAW)
    ret = GWidgetAskR( _STR_Fontchange,buts,0,1,_STR_FontChangedRevertMsg,fontname,filename);
#elif defined(FONTFORGE_CONFIG_GTK)
    buts[0] = GTK_STOCK_REVERT;
    buts[1] = GTK_STOCK_CANCEL;
    buts[2] = NULL;
    ret = gwwv_ask( _("Font changed"),buts,0,1,_("Font %1$.40s in file %2$.40s has been changed.\nReverting the file will lose those changes.\nIs that what you want?"),fontname,filename);
#endif
return( ret==0 );
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
int _FVMenuGenerate(FontView *fv,int family) {
    FVFlattenAllBitmapSelections(fv);
return( SFGenerateFont(fv->sf,family,fv->map) );
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuGenerate(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuGenerate(fv,false);
}

static void FVMenuGenerateFamily(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuGenerate(fv,true);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Generate(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuGenerate(fv,false);
}

void FontViewMenu_GenerateFamily(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuGenerate(fv,true);
}
# endif

int _FVMenuSaveAs(FontView *fv) {
    unichar_t *temp;
    unichar_t *ret;
    char *filename;
    int ok;

    if ( fv->cidmaster!=NULL && fv->cidmaster->filename!=NULL )
	temp=def2u_copy(fv->cidmaster->filename);
    else if ( fv->sf->mm!=NULL && fv->sf->mm->normal->filename!=NULL )
	temp=def2u_copy(fv->sf->mm->normal->filename);
    else if ( fv->sf->filename!=NULL )
	temp=def2u_copy(fv->sf->filename);
    else {
	SplineFont *sf = fv->cidmaster?fv->cidmaster:
		fv->sf->mm!=NULL?fv->sf->mm->normal:fv->sf;
	temp = galloc(sizeof(unichar_t)*(strlen(sf->fontname)+10));
	uc_strcpy(temp,sf->fontname);
	if ( fv->cidmaster!=NULL )
	    uc_strcat(temp,"CID");
	else if ( sf->mm==NULL )
	    ;
	else if ( sf->mm->apple )
	    uc_strcat(temp,"Var");
	else
	    uc_strcat(temp,"MM");
	uc_strcat(temp,".sfd");
    }
#if defined(FONTFORGE_CONFIG_GDRAW)
    ret = GWidgetSaveAsFile(GStringGetResource(_STR_Saveas,NULL),temp,NULL,NULL,NULL);
#elif defined(FONTFORGE_CONFIG_GTK)
    ret = gwwv_save_filename(_("Save as..."),temp);
#endif
    free(temp);
    if ( ret==NULL )
return( 0 );
    filename = u2def_copy(ret);
    free(ret);
    FVFlattenAllBitmapSelections(fv);
    ok = SFDWrite(filename,fv->sf,fv->map);
    if ( ok ) {
	SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf->mm!=NULL?fv->sf->mm->normal:fv->sf;
	free(sf->filename);
	sf->filename = filename;
	free(sf->origname);
	sf->origname = copy(filename);
	sf->new = false;
	if ( sf->mm!=NULL ) {
	    int i;
	    for ( i=0; i<sf->mm->instance_count; ++i ) {
		free(sf->mm->instances[i]->filename);
		sf->mm->instances[i]->filename = filename;
		free(sf->mm->instances[i]->origname);
		sf->mm->instances[i]->origname = copy(filename);
		sf->mm->instances[i]->new = false;
	    }
	}
	SplineFontSetUnChanged(sf);
	FVSetTitle(fv);
    } else
	free(filename);
return( ok );
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSaveAs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    _FVMenuSaveAs(fv);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SaveAs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuSaveAs(fv);
}
# endif

int _FVMenuSave(FontView *fv) {
    int ret = 0;
    SplineFont *sf = fv->cidmaster?fv->cidmaster:
		    fv->sf->mm!=NULL?fv->sf->mm->normal:
			    fv->sf;

#if 0		/* This seems inconsistant with normal behavior. Removed 6 Feb '04 */
    if ( sf->filename==NULL && sf->origname!=NULL &&
	    sf->onlybitmaps && sf->bitmaps!=NULL && sf->bitmaps->next==NULL ) {
	/* If it's a single bitmap font then just save back to the bdf file */
	FVFlattenAllBitmapSelections(fv);
	ret = BDFFontDump(sf->origname,sf->bitmaps,EncodingName(sf->encoding_name),sf->bitmaps->res);
	if ( ret )
	    SplineFontSetUnChanged(sf);
    } else
#endif
    if ( sf->filename==NULL )
	ret = _FVMenuSaveAs(fv);
    else {
	FVFlattenAllBitmapSelections(fv);
	if ( !SFDWriteBak(sf,fv->map) )
#if defined(FONTFORGE_CONFIG_GDRAW)
	    GWidgetErrorR(_STR_SaveFailed,_STR_SaveFailed);
#elif defined(FONTFORGE_CONFIG_GTK)
	    gwwv_post_error(_("Save Failed"),_("Save Failed"));
#endif
	else {
	    SplineFontSetUnChanged(sf);
	    ret = true;
	}
    }
return( ret );
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSave(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    _FVMenuSave(fv);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Save(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuSave(fv);
}
# endif
#endif

void _FVCloseWindows(FontView *fv) {
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    int i, j;
    BDFFont *bdf;
    MetricsView *mv, *mnext;
    SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf->mm!=NULL?fv->sf->mm->normal : fv->sf;

    if ( fv->nextsame==NULL && fv->sf->fv==fv && fv->sf->kcld!=NULL )
	KCLD_End(fv->sf->kcld);
    if ( fv->nextsame==NULL && fv->sf->fv==fv && fv->sf->vkcld!=NULL )
	KCLD_End(fv->sf->vkcld);

    for ( i=0; i<sf->glyphcnt; ++i ) if ( sf->glyphs[i]!=NULL ) {
	CharView *cv, *next;
	for ( cv = sf->glyphs[i]->views; cv!=NULL; cv = next ) {
	    next = cv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
	    GDrawDestroyWindow(cv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
	    gtk_widget_destroy(cv->gw);
# endif
	}
	if ( sf->glyphs[i]->charinfo )
	    CharInfoDestroy(sf->glyphs[i]->charinfo);
    }
    if ( sf->mm!=NULL ) {
	MMSet *mm = sf->mm;
	for ( j=0; j<mm->instance_count; ++j ) {
	    SplineFont *sf = mm->instances[j];
	    for ( i=0; i<sf->glyphcnt; ++i ) if ( sf->glyphs[i]!=NULL ) {
		CharView *cv, *next;
		for ( cv = sf->glyphs[i]->views; cv!=NULL; cv = next ) {
		    next = cv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
		    GDrawDestroyWindow(cv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
		    gtk_widget_destroy(cv->gw);
# endif
		}
		if ( sf->glyphs[i]->charinfo )
		    CharInfoDestroy(sf->glyphs[i]->charinfo);
	    }
	}
    } else if ( sf->subfontcnt!=0 ) {
	for ( j=0; j<sf->subfontcnt; ++j ) {
	    for ( i=0; i<sf->subfonts[j]->glyphcnt; ++i ) if ( sf->subfonts[j]->glyphs[i]!=NULL ) {
		CharView *cv, *next;
		for ( cv = sf->subfonts[j]->glyphs[i]->views; cv!=NULL; cv = next ) {
		    next = cv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
		    GDrawDestroyWindow(cv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
		    gtk_widget_destroy(cv->gw);
# endif
		if ( sf->subfonts[j]->glyphs[i]->charinfo )
		    CharInfoDestroy(sf->subfonts[j]->glyphs[i]->charinfo);
		}
	    }
	}
    }
    for ( bdf = sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	for ( i=0; i<bdf->glyphcnt; ++i ) if ( bdf->glyphs[i]!=NULL ) {
	    BitmapView *bv, *next;
	    for ( bv = bdf->glyphs[i]->views; bv!=NULL; bv = next ) {
		next = bv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
		GDrawDestroyWindow(bv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
		gtk_widget_destroy(bv->gw);
# endif
	    }
	}
    }
    for ( mv=fv->metrics; mv!=NULL; mv = mnext ) {
	mnext = mv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
	GDrawDestroyWindow(mv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
	gtk_widget_destroy(mv->gw);
# endif
    }
    if ( fv->fontinfo!=NULL )
	FontInfoDestroy(fv);
    SVDetachFV(fv);
#endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static int SFAnyChanged(SplineFont *sf) {
    if ( sf->mm!=NULL ) {
	MMSet *mm = sf->mm;
	int i;
	if ( mm->changed )
return( true );
	for ( i=0; i<mm->instance_count; ++i )
	    if ( sf->mm->instances[i]->changed )
return( true );
	/* Changes to the blended font aren't real (for adobe fonts) */
	if ( mm->apple && mm->normal->changed )
return( true );

return( false );
    } else
return( sf->changed );
}

static int _FVMenuClose(FontView *fv) {
    int i;
    SplineFont *sf = fv->cidmaster?fv->cidmaster:fv->sf;

    if ( !SFCloseAllInstrs(fv->sf) )
return( false );

    if ( fv->nextsame!=NULL || fv->sf->fv!=fv ) {
	/* There's another view, can close this one with no problems */
    } else if ( SFAnyChanged(sf) ) {
	i = AskChanged(fv->sf);
	if ( i==2 )	/* Cancel */
return( false );
	if ( i==0 && !_FVMenuSave(fv))		/* Save */
return(false);
	else
	    SFClearAutoSave(sf);		/* if they didn't save it, remove change record */
    }
    _FVCloseWindows(fv);
    if ( sf->filename!=NULL )
	RecentFilesRemember(sf->filename);
#ifdef FONTFORGE_CONFIG_GDRAW
    GDrawDestroyWindow(fv->gw);
#elif defined(FONTFORGE_CONFIG_GTK)
    gtk_widget_destroy(fv->gw);
#endif
return( true );
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuNew(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontNew();
}

static void FVMenuClose(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    DelayEvent((void (*)(void *)) _FVMenuClose, fv);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void Menu_New(GtkMenuItem *menuitem, gpointer user_data) {
    FontNew();
}

void FontViewMenu_Close(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuClose(fv);
}

gboolean FontView_RequestClose(GtkWidget *widget, GdkEvent *event,
	gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _FVMenuClose(fv);
}
# endif

static void FVReattachCVs(SplineFont *old,SplineFont *new) {
    int i, j, pos;
    CharView *cv, *cvnext;
    SplineFont *sub;

    for ( i=0; i<old->glyphcnt; ++i ) {
	if ( old->glyphs[i]!=NULL && old->glyphs[i]->views!=NULL ) {
	    if ( new->subfontcnt==0 ) {
		pos = SFFindExistingSlot(new,old->glyphs[i]->unicodeenc,old->glyphs[i]->name);
		sub = new;
	    } else {
		pos = -1;
		for ( j=0; j<new->subfontcnt && pos==-1 ; ++j ) {
		    sub = new->subfonts[j];
		    pos = SFFindExistingSlot(sub,old->glyphs[i]->unicodeenc,old->glyphs[i]->name);
		}
	    }
	    if ( pos==-1 ) {
		for ( cv=old->glyphs[i]->views; cv!=NULL; cv = cvnext ) {
		    cvnext = cv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
		    GDrawDestroyWindow(cv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
		    gtk_widget_destroy(cv->gw);
# endif
		}
	    } else {
		for ( cv=old->glyphs[i]->views; cv!=NULL; cv = cvnext ) {
		    cvnext = cv->next;
		    CVChangeSC(cv,sub->glyphs[pos]);
		    cv->layerheads[dm_grid] = &new->grid;
		}
	    }
# ifdef FONTFORGE_CONFIG_GDRAW
	    GDrawProcessPendingEvents(NULL);		/* Don't want to many destroy_notify events clogging up the queue */
# endif
	}
    }
}

void FVRevert(FontView *fv) {
    SplineFont *temp, *old = fv->cidmaster?fv->cidmaster:fv->sf;
    BDFFont *tbdf, *fbdf;
    BDFChar *bc;
    BitmapView *bv, *bvnext;
    MetricsView *mv, *mvnext;
    int i;
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
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    FVReattachCVs(old,temp);
    for ( i=0; i<old->subfontcnt; ++i )
	FVReattachCVs(old->subfonts[i],temp);
    for ( fbdf=old->bitmaps; fbdf!=NULL; fbdf=fbdf->next ) {
	for ( tbdf=temp->bitmaps; tbdf!=NULL; tbdf=tbdf->next )
	    if ( tbdf->pixelsize==fbdf->pixelsize )
	break;
	for ( i=0; i<fv->sf->glyphcnt; ++i ) {
	    if ( i<fbdf->glyphcnt && fbdf->glyphs[i]!=NULL && fbdf->glyphs[i]->views!=NULL ) {
		int pos = SFFindExistingSlot(temp,fv->sf->glyphs[i]->unicodeenc,fv->sf->glyphs[i]->name);
		bc = pos==-1 || tbdf==NULL?NULL:tbdf->glyphs[pos];
		if ( bc==NULL ) {
		    for ( bv=fbdf->glyphs[i]->views; bv!=NULL; bv = bvnext ) {
			bvnext = bv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
			GDrawDestroyWindow(bv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
			gtk_widget_destroy(bv->gw);
# endif
		    }
		} else {
		    for ( bv=fbdf->glyphs[i]->views; bv!=NULL; bv = bvnext ) {
			bvnext = bv->next;
			BVChangeBC(bv,bc,true);
		    }
		}
# ifdef FONTFORGE_CONFIG_GDRAW
		GDrawProcessPendingEvents(NULL);		/* Don't want to many destroy_notify events clogging up the queue */
# endif
	    }
	}
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
# endif
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	for ( mv=fvs->metrics; mv!=NULL; mv = mvnext ) {
	    /* Don't bother trying to fix up metrics views, just not worth it */
	    mvnext = mv->next;
# ifdef FONTFORGE_CONFIG_GDRAW
	    GDrawDestroyWindow(mv->gw);
# elif defined(FONTFORGE_CONFIG_GTK)
	    gtk_widget_destroy(mv->gw);
# endif
	}
	if ( fvs->fontinfo )
	    FontInfoDestroy(fvs);
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
# endif
#endif
    if ( temp->map->enccount>fv->map->enccount ) {
	fv->selected = grealloc(fv->selected,temp->map->enccount);
	memset(fv->selected+fv->map->enccount,0,temp->map->enccount-fv->map->enccount);
    }
    EncMapFree(fv->map);
    fv->map = temp->map;
    SFClearAutoSave(old);
    temp->fv = fv->sf->fv;
    for ( fvs=fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	fvs->sf = temp;
    FontViewReformatAll(fv->sf);
    SplineFontFree(old);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRevert(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Revert(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVRevert(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRevertGlyph(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RevertGlyph(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i, gid;
    int nc_state = -1;
    SplineFont *sf = fv->sf;
    SplineChar *sc, *tsc;
    SplineChar temp;
#ifdef FONTFORGE_CONFIG_TYPE3
    Undoes **undoes;
    int layer, lc;
#endif
    EncMap *map = fv->map;
    CharView *cvs;

    for ( i=0; i<map->enccount; ++i ) if ( fv->selected[i] && (gid=map->map[i])!=-1 && sf->glyphs[gid]!=NULL ) {
	tsc = sf->glyphs[gid];
	if ( tsc->namechanged ) {
	    if ( nc_state==-1 ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
		GWidgetErrorR(_STR_NameChanged,_STR_NameChangedGlyph,tsc->name);
#elif defined(FONTFORGE_CONFIG_GTK)
		gwwv_post_error(_("Glyph Name Changed"),_("The the name of character %.40s has changed. This is what I use to find the glyph in the file, so I cannot revert this character.\n(You will not be warned for subsequent characters)"),tsc->name);
#endif
		nc_state = 0;
	    }
	} else {
	    sc = SFDReadOneChar(sf,tsc->name);
	    if ( sc==NULL ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
		GWidgetErrorR(_STR_CantFindGlyph,_STR_CantRevertGlyph,tsc->name);
#elif defined(FONTFORGE_CONFIG_GTK)
		gwwv_post_error(_("Can't Find Glyph"),_("The glyph, %.80s, can't be found in the sfd file"),tsc->name);
#endif
		tsc->namechanged = true;
	    } else {
		SCPreserveState(tsc,true);
		SCPreserveBackground(tsc);
		temp = *tsc;
		tsc->dependents = NULL;
#ifdef FONTFORGE_CONFIG_TYPE3
		lc = tsc->layer_cnt;
		undoes = galloc(lc*sizeof(Undoes *));
		for ( layer=0; layer<lc; ++layer ) {
		    undoes[layer] = tsc->layers[layer].undoes;
		    tsc->layers[layer].undoes = NULL;
		}
#else
		tsc->layers[ly_back].undoes = tsc->layers[ly_fore].undoes = NULL;
#endif
		SplineCharFreeContents(tsc);
		*tsc = *sc;
		chunkfree(sc,sizeof(SplineChar));
		tsc->parent = sf;
		tsc->dependents = temp.dependents;
		tsc->views = temp.views;
#ifdef FONTFORGE_CONFIG_TYPE3
		for ( layer = 0; layer<lc && layer<tsc->layer_cnt; ++layer )
		    tsc->layers[layer].undoes = undoes[layer];
		for ( ; layer<lc; ++layer )
		    UndoesFree(undoes[layer]);
		free(undoes);
#else
		tsc->layers[ly_fore].undoes = temp.layers[ly_fore].undoes;
		tsc->layers[ly_back].undoes = temp.layers[ly_back].undoes;
#endif
		/* tsc->changed = temp.changed; */
		/* tsc->orig_pos = temp.orig_pos; */
		for ( cvs=tsc->views; cvs!=NULL; cvs=cvs->next ) {
		    cvs->layerheads[dm_back] = &tsc->layers[ly_back];
		    cvs->layerheads[dm_fore] = &tsc->layers[ly_fore];
		}
		RevertedGlyphReferenceFixup(tsc, sf);
		_SCCharChangedUpdate(tsc,false);
	    }
	}
    }
}

# if defined(FONTFORGE_CONFIG_GDRAW)
void MenuPrefs(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuPrefs(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    DoPrefs();
}

# if defined(FONTFORGE_CONFIG_GDRAW)
void MenuSaveAll(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuSaveAll(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    FontView *fv;

    for ( fv = fv_list; fv!=NULL; fv = fv->next ) {
	if ( SFAnyChanged(fv->sf) && !_FVMenuSave(fv))
return;
    }
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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

# ifdef FONTFORGE_CONFIG_GTK
static void MenuExit(GtkMenuItem *menuitem, gpointer user_data) {
    _MenuExit(NULL);
}

char *GetPostscriptFontName(char *dir, int mult) {
    /* Some people use pf3 as an extension for postscript type3 fonts */
    /* Any of these extensions */
    static char wild[] = "*.{"
	   "pfa,"
	   "pfb,"
	   "pt3,"
	   "t42,"
	   "sfd,"
	   "ttf,"
	   "bdf,"
	   "otf,"
	   "otb,"
	   "cff,"
	   "cef,"
#ifndef _NO_LIBXML
	   "svg,"
#endif
	   "pf3,"
	   "ttc,"
	   "gsf,"
	   "cid,"
	   "bin,"
	   "hqx,"
	   "dfont,"
	   "mf,"
	   "ik,"
	   "fon,"
	   "fnt"
	   "}"
/* With any of these methods of compression */
	     "{.gz,.Z,.bz2,}";

return( FVOpenFont(_("Open Postscript Font"), dir,wild,mult));
}

void MergeKernInfo(SplineFont *sf,EncMap *map) {
#ifndef __Mac
    static char wild = "*.{afm,tfm,pfm,bin,hqx,dfont}";
    static char wild2[] = "*.{afm,amfm,tfm,pfm,bin,hqx,dfont}";
#else
    static char wild[] = "*";	/* Mac resource files generally don't have extensions */
    static char wild2[] = "*";
#endif
    char *ret = gwwv_open_filename(_("Merge Kern Info"),
	    sf->mm!=NULL?wild2:wild);

    if ( ret==NULL )		/* Cancelled */
return;

    if ( !LoadKerningDataFromMetricsFile(sf,ret,map))
	gwwv_post_error( _("Failed to load kern data from %s"), ret);
    free(ret);
}
# elif defined(FONTFORGE_CONFIG_GDRAW)
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
					       'p','t','3',',',
					       't','4','2',',',
			                       's','f','d',',',
			                       't','t','f',',',
			                       'b','d','f',',',
			                       'o','t','f',',',
			                       'o','t','b',',',
			                       'c','f','f',',',
			                       'c','e','f',',',
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
			                       'p','d','b',',',
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

void MergeKernInfo(SplineFont *sf,EncMap *map) {
#ifndef __Mac
    static unichar_t wild[] = { '*', '.', '{','a','f','m',',','t','f','m',',','p','f','m',',','b','i','n',',','h','q','x',',','d','f','o','n','t','}',  '\0' };
    static unichar_t wild2[] = { '*', '.', '{','a','f','m',',','a','m','f','m',',','t','f','m',',','p','f','m',',','b','i','n',',','h','q','x',',','d','f','o','n','t','}',  '\0' };
#else
    static unichar_t wild[] = { '*', 0 };	/* Mac resource files generally don't have extensions */
    static unichar_t wild2[] = { '*', 0 };
#endif
    unichar_t *ret = GWidgetOpenFile(GStringGetResource(_STR_MergeKernInfo,NULL),
	    NULL,sf->mm!=NULL?wild2:wild,NULL,NULL);
    char *temp = u2def_copy(ret);

    if ( temp==NULL )		/* Cancelled */
return;

    if ( !LoadKerningDataFromMetricsFile(sf,temp,map))
#if defined(FONTFORGE_CONFIG_GTK)
	gwwv_post_error(_("Load of Kerning Metrics Failed"),_("Failed to load kern data from %s"), temp);
#else
	GWidgetErrorR(_STR_KerningLoadFailed,_STR_KerningLoadFailedLong, temp);
#endif
    free(ret); free(temp);
}
# endif
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuMergeKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_MergeKern(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MergeKernInfo(fv->sf,fv->map);
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuOpen(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuOpen(GtkMenuItem *menuitem, gpointer user_data) {
# endif
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
	    free(full);
	} while ( fpt!=NULL );
	free(temp);
	for ( fvtest=0, test=fv_list; test!=NULL; ++fvtest, test=test->next );
    } while ( fvtest==fvcnt );	/* did the load fail for some reason? try again */
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuHelp(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuHelp(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    help("overview.html");
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuIndex(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuIndex(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    help("IndexFS.html");
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuLicense(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuLicense(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    help("license.html");
}

# ifdef FONTFORGE_CONFIG_GDRAW
void MenuAbout(GWindow base,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void MenuAbout(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    ShowAboutScreen();
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuImport(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Import(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int empty = fv->sf->onlybitmaps && fv->sf->bitmaps==NULL;
    BDFFont *bdf;
    FVImport(fv);
    if ( empty && fv->sf->bitmaps!=NULL ) {
	for ( bdf= fv->sf->bitmaps; bdf->next!=NULL; bdf = bdf->next );
	FVChangeDisplayBitmap(fv,bdf);
    }
}

static int FVSelCount(FontView *fv) {
    int i, cnt=0;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] ) ++cnt;
    if ( cnt>10 ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
    static int buts[] = { _STR_OK, _STR_Cancel, 0 };
	if ( GWidgetAskR(_STR_Manywin,buts,0,1,_STR_Toomany)==1 )
#elif defined(FONTFORGE_CONFIG_GTK)
	static char *buts[] = { GTK_STOCK_OK, GTK_STOCK_CANCEL, NULL };
	if ( gwwv_ask(_("Many Windows"),buts,0,1,_("This involves opening more than 10 windows.\nIs that really what you want?"))==1 )
#endif
return( false );
    }
return( true );
}
	
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuOpenOutline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_OpenOutline(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i;
    SplineChar *sc;

    if ( !FVSelCount(fv))
return;
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] ) {
	    sc = FVMakeChar(fv,i);
	    CharViewCreate(sc,fv,i);
	}
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuOpenBitmap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_OpenBitmap(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i;
    SplineChar *sc;

    if ( fv->cidmaster==NULL ? (fv->sf->bitmaps==NULL) : (fv->cidmaster->bitmaps==NULL) )
return;
    if ( !FVSelCount(fv))
return;
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] ) {
	    sc = FVMakeChar(fv,i);
	    if ( sc!=NULL )
		BitmapViewCreatePick(i,fv);
	}
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuOpenMetrics(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_OpenMetrics(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MetricsViewCreate(fv,NULL,fv->filled==fv->show?NULL:fv->show);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuPrint(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Print(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    PrintDlg(fv,NULL,NULL);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuDisplay(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Display(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    DisplayDlg(fv->sf);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuExecute(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Execute(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    ScriptDlg(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuFontInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_FontInfo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FontMenuFontInfo(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuFindProblems(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_FindProblems(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FindProblems(fv,NULL,NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuMetaFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_MetaFont(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MetaFont(fv,NULL,NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
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
#define MID_Ligatures	2020
#define MID_KernPairs	2021
#define MID_AnchorPairs	2022
#define MID_FitToEm	2023
#define MID_DisplaySubs	2024
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
#define MID_ShowDependentRefs	2222
#define MID_AddExtrema	2224
#define MID_CleanupGlyph	2225
#define MID_TilePath	2226
#define MID_BuildComposite	2227
#define MID_NLTransform	2228
#define MID_Intersection	2229
#define MID_FindInter	2230
#define MID_Effects	2231
#define MID_CopyFeatureToFont	2232
#define MID_SimplifyMore	2233
#define MID_ShowDependentSubs	2234
#define MID_DefaultATT	2235
#define MID_POV		2236
#define MID_BuildDuplicates	2237
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
#define MID_Editcvt	2512
#define MID_HintSubsPt	2513
#define MID_AutoCounter	2514
#define MID_DontAutoHint	2515
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
#define MID_RplRef	2131
#define MID_PasteAfter	2132
#define MID_CopyFeatures	2133
#define MID_Convert2CID	2800
#define MID_Flatten	2801
#define MID_InsertFont	2802
#define MID_InsertBlank	2803
#define MID_CIDFontInfo	2804
#define MID_RemoveFromCID 2805
#define MID_ConvertByCMap	2806
#define MID_FlattenByCMap	2807
#define MID_ChangeSupplement	2808
#define MID_Reencode		2830
#define MID_ForceReencode	2831
#define MID_AddUnencoded	2832
#define MID_RemoveUnused	2833
#define MID_DetachGlyphs	2834
#define MID_DetachAndRemoveGlyphs	2835
#define MID_LoadEncoding	2836
#define MID_MakeFromFont	2837
#define MID_RemoveEncoding	2838
#define MID_DisplayByGroups	2839
#define MID_Compact	2840
#define MID_CreateMM	2900
#define MID_MMInfo	2901
#define MID_MMValid	2902
#define MID_ChangeMMBlend	2903
#define MID_BlendToNew	2904
#define MID_ModifyComposition	20902
#define MID_BuildSyllables	20903
# endif
#endif

/* returns -1 if nothing selected, if exactly one char return it, -2 if more than one */
static int FVAnyCharSelected(FontView *fv) {
    int i, val=-1;

    for ( i=0; i<fv->map->enccount; ++i ) {
	if ( fv->selected[i]) {
	    if ( val==-1 )
		val = i;
	    else
return( -2 );
	}
    }
return( val );
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static int FVAllSelected(FontView *fv) {
    int i, any = false;
    /* Is everything real selected? */

    for ( i=0; i<fv->sf->glyphcnt; ++i ) if ( SCWorthOutputting(fv->sf->glyphs[i])) {
	if ( !fv->selected[fv->map->backmap[i]] )
return( false );
	any = true;
    }
return( any );
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCopyFrom(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    /*FontView *fv = (FontView *) GDrawGetUserData(gw);*/

    if ( mi->mid==MID_CharName )
	copymetadata = !copymetadata;
    else
	onlycopydisplayed = (mi->mid==MID_DisplayedFont);
    SavePrefs();
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyFromAll(GtkMenuItem *menuitem, gpointer user_data) {

    onlycopydisplayed = false;
    SavePrefs();
}

void FontViewMenu_CopyFromDisplayed(GtkMenuItem *menuitem, gpointer user_data) {

    onlycopydisplayed = true;
    SavePrefs();
}

void FontViewMenu_CopyFromMetadata(GtkMenuItem *menuitem, gpointer user_data) {

    copymetadata = !copymetadata;
    SavePrefs();
}
# endif

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCopy(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Copy(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopy(fv,true);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCopyRef(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyRef(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopy(fv,false);
}

# ifdef FONTFORGE_CONFIG_GDRAW
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
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyWidth(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopyWidth(fv,ut_width);
}

void FontViewMenu_CopyVWidth(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( !fv->sf->hasvmetrics )
return;
    FVCopyWidth(fv,ut_vwidth);
}

void FontViewMenu_CopyLBearing(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopyWidth(fv,ut_lbearing);
}

void FontViewMenu_CopyRBearing(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    FVCopyWidth(fv,ut_rbearing);
}
# endif

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCopyFeatures(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyFeatures(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int ch = FVAnyCharSelected(fv);

    if ( ch<0 || fv->sf->glyphs[ch]==NULL )
return;
    SCCopyFeatures(fv->sf->glyphs[ch]);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuPaste(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Paste(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    if ( FVAnyCharSelected(fv)==-1 )
return;
    PasteIntoFV(fv,false,NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuPasteInto(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_PasteInto(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    if ( FVAnyCharSelected(fv)==-1 )
return;
    PasteIntoFV(fv,true,NULL);
}

#ifdef FONTFORGE_CONFIG_PASTEAFTER
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuPasteAfter(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_PasteAfter(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int pos = FVAnyCharSelected(fv);
    if ( pos<0 )
return;
    PasteIntoFV(fv,2,NULL);
}
#elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_PasteAfter(GtkMenuItem *menuitem, gpointer user_data) {
}
#endif
#endif

static void LinkEncToGid(FontView *fv,int enc, int gid) {
    EncMap *map = fv->map;
    int old_gid;
    int flags = -1;
    int j;

    if ( map->map[enc]!=-1 && map->map[enc]!=gid ) {
	SplineFont *sf = fv->sf;
	old_gid = map->map[enc];
	for ( j=0; j<map->enccount; ++j )
	    if ( j!=enc && map->map[j]==old_gid )
	break;
	/* If the glyph is used elsewhere in the encoding then reusing this */
	/* slot causes no problems */
	if ( j==map->enccount ) {
	    /* However if this is the only use and the glyph is interesting */
	    /*  then add it to the unencoded area. If it is uninteresting we*/
	    /*  can just get rid of it */
	    if ( SCWorthOutputting(sf->glyphs[old_gid]) )
		SFAddEncodingSlot(sf,old_gid);
	    else
		SFRemoveGlyph(sf,sf->glyphs[old_gid],&flags);
	}
    }
    map->map[enc] = gid;
}

static void FVSameGlyphAs(FontView *fv) {
    SplineFont *sf = fv->sf;
    RefChar *base = CopyContainsRef(sf);
    int i;
    EncMap *map = fv->map;

    if ( base==NULL || fv->cidmaster!=NULL )
return;
    for ( i=0; i<map->enccount; ++i ) if ( fv->selected[i] ) {
	LinkEncToGid(fv,i,base->orig_pos);
    }
    GDrawRequestExpose(fv->v,NULL,false);
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSameGlyphAs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SameGlyphAs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVSameGlyphAs(fv);
}
#endif

static void FVCopyFgtoBg(FontView *fv) {
    int i, gid;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL &&
		fv->sf->glyphs[gid]->layers[1].splines!=NULL )
	    SCCopyFgToBg(fv->sf->glyphs[gid],true);
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCopyFgBg(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyFgBg(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVCopyFgtoBg( fv );
}
#endif

void BCClearAll(BDFChar *bc) {
    if ( bc==NULL )
return;
    BCPreserveState(bc);
    BCFlattenFloat(bc);
    memset(bc->bitmap,'\0',bc->bytes_per_line*(bc->ymax-bc->ymin+1));
    BCCompressBitmap(bc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    BCCharChangedUpdate(bc);
#endif
}

void SCClearContents(SplineChar *sc) {
    RefChar *refs, *next;
    int layer;

    if ( sc==NULL )
return;
    sc->widthset = false;
    sc->width = sc->parent->ascent+sc->parent->descent;
    for ( layer = ly_fore; layer<sc->layer_cnt; ++layer ) {
	SplinePointListsFree(sc->layers[layer].splines);
	sc->layers[layer].splines = NULL;
	for ( refs=sc->layers[layer].refs; refs!=NULL; refs = next ) {
	    next = refs->next;
	    SCRemoveDependent(sc,refs);
	}
	sc->layers[layer].refs = NULL;
    }
    AnchorPointsFree(sc->anchor);
    sc->anchor = NULL;
    StemInfosFree(sc->hstem); sc->hstem = NULL;
    StemInfosFree(sc->vstem); sc->vstem = NULL;
    DStemInfosFree(sc->dstem); sc->dstem = NULL;
    MinimumDistancesFree(sc->md); sc->md = NULL;
    free(sc->ttf_instrs);
    sc->ttf_instrs = NULL;
    sc->ttf_instrs_len = 0;
    SCOutOfDateBackground(sc);
}

void SCClearAll(SplineChar *sc) {

    if ( sc==NULL )
return;
    if ( sc->layers[1].splines==NULL && sc->layers[ly_fore].refs==NULL && !sc->widthset &&
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
    SCClearContents(sc);
    SCCharChangedUpdate(sc);
}

void SCClearBackground(SplineChar *sc) {

    if ( sc==NULL )
return;
    if ( sc->layers[0].splines==NULL && sc->layers[ly_back].images==NULL )
return;
    SCPreserveBackground(sc);
    SplinePointListsFree(sc->layers[0].splines);
    sc->layers[0].splines = NULL;
    ImageListsFree(sc->layers[ly_back].images);
    sc->layers[ly_back].images = NULL;
    SCOutOfDateBackground(sc);
    SCCharChangedUpdate(sc);
}

static int UnselectedDependents(FontView *fv, SplineChar *sc) {
    struct splinecharlist *dep;

    for ( dep=sc->dependents; dep!=NULL; dep=dep->next ) {
	if ( !fv->selected[fv->map->backmap[dep->sc->orig_pos]] )
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
	if ( fv==NULL || !fv->selected[fv->map->backmap[dep->sc->orig_pos]]) {
	    SplineChar *dsc = dep->sc;
	    RefChar *rf, *rnext;
	    /* May be more than one reference to us, colon has two refs to period */
	    /*  but only one dlist entry */
	    for ( rf = dsc->layers[ly_fore].refs; rf!=NULL; rf=rnext ) {
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
#if defined(FONTFORGE_CONFIG_GDRAW)
    static int buts[] = { _STR_Yes, _STR_YesToAll, _STR_UnlinkAll, _STR_NoToAll, _STR_No, 0 };
#elif defined(FONTFORGE_CONFIG_GTK)
    char *buts[6];
#endif
    int yes, unsel, gid;
    /* refstate==0 => ask, refstate==1 => clearall, refstate==-1 => skip all */

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 ) {
	if ( !onlycopydisplayed || fv->filled==fv->show ) {
	    /* If we are messing with the outline character, check for dependencies */
	    if ( refstate<=0 && fv->sf->glyphs[gid]!=NULL &&
		    fv->sf->glyphs[gid]->dependents!=NULL ) {
		unsel = UnselectedDependents(fv,fv->sf->glyphs[gid]);
		if ( refstate==-2 && unsel ) {
		    UnlinkThisReference(fv,fv->sf->glyphs[gid]);
		} else if ( unsel ) {
		    if ( refstate<0 )
    continue;
#if defined(FONTFORGE_CONFIG_GDRAW)
		    yes = GWidgetAskCenteredR(_STR_BadReference,buts,2,4,_STR_ClearDependent,fv->sf->glyphs[gid]->name);
#elif defined(FONTFORGE_CONFIG_GTK)
		    buts[0] = GTK_STOCK_YES;
		    buts[1] = _("Yes to All");
		    buts[2] = _("Unlink All");
		    buts[3] = _("No to All");
		    buts[4] = GTK_STOCK_NO;
		    buts[5] = NULL;
		    yes = gwwv_ask(_("Bad Reference"),buts,2,4,_("You are attempting to clear %.30s which is refered to by\nanother character. Are you sure you want to clear it?"),fv->sf->glyphs[gid]->name);
#endif
		    if ( yes==1 )
			refstate = 1;
		    else if ( yes==2 ) {
			UnlinkThisReference(fv,fv->sf->glyphs[gid]);
			refstate = -2;
		    } else if ( yes==3 )
			refstate = -1;
		    if ( yes>=3 )
    continue;
		}
	    }
	}

	if ( onlycopydisplayed && fv->filled==fv->show ) {
	    SCClearAll(fv->sf->glyphs[gid]);
	} else if ( onlycopydisplayed ) {
	    BCClearAll(fv->show->glyphs[gid]);
	} else {
	    SCClearAll(fv->sf->glyphs[gid]);
	    for ( bdf=fv->sf->bitmaps; bdf!=NULL; bdf = bdf->next )
		BCClearAll(bdf->glyphs[gid]);
	}
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuClear(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Clear(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVClear( fv );
}
#endif

static void FVClearBackground(FontView *fv) {
    SplineFont *sf = fv->sf;
    int i, gid;

    if ( onlycopydisplayed && fv->filled!=fv->show )
return;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && sf->glyphs[gid]!=NULL ) {
	SCClearBackground(sf->glyphs[gid]);
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuClearBackground(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ClearBackground(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVClearBackground( fv );
}
#endif

static void FVJoin(FontView *fv) {
    SplineFont *sf = fv->sf;
    int i,changed,gid;
    extern float joinsnap;

    if ( onlycopydisplayed && fv->filled!=fv->show )
return;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && sf->glyphs[gid]!=NULL ) {
	SCPreserveState(sf->glyphs[gid],false);
	sf->glyphs[gid]->layers[ly_fore].splines = SplineSetJoin(sf->glyphs[gid]->layers[ly_fore].splines,true,joinsnap,&changed);
	if ( changed )
	    SCCharChangedUpdate(sf->glyphs[gid]);
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuJoin(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Join(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVJoin( fv );
}
#endif

static void FVUnlinkRef(FontView *fv) {
    int i,layer, gid;
    SplineChar *sc;
    RefChar *rf, *next;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && (sc = fv->sf->glyphs[gid])!=NULL &&
	     sc->layers[ly_fore].refs!=NULL ) {
	SCPreserveState(sc,false);
	for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
	    for ( rf=sc->layers[ly_fore].refs; rf!=NULL ; rf=next ) {
		next = rf->next;
		SCRefToSplines(sc,rf);
	    }
	}
	SCCharChangedUpdate(sc);
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuUnlinkRef(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_UnlinkRef(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVUnlinkRef( fv );
}
#endif

void SFRemoveUndoes(SplineFont *sf,uint8 *selected, EncMap *map) {
    SplineFont *main = sf->cidmaster? sf->cidmaster : sf, *ssf;
    int i,k, max, layer, gid;
    SplineChar *sc;
    BDFFont *bdf;

    if ( selected!=NULL || main->subfontcnt==0 )
	max = sf->glyphcnt;
    else {
	max = 0;
	for ( k=0; k<main->subfontcnt; ++k )
	    if ( main->subfonts[k]->glyphcnt>max ) max = main->subfonts[k]->glyphcnt;
    }
    for ( i=0; ; ++i ) {
	if ( selected==NULL || main->subfontcnt!=0 ) {
	    if ( i>=max )
    break;
	    gid = i;
	} else {
	    if ( i>=map->enccount )
    break;
	    if ( !selected[i])
    continue;
	    gid = map->map[i];
	    if ( gid==-1 )
    continue;
	}
	for ( bdf=main->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	    if ( bdf->glyphs[gid]!=NULL ) {
		UndoesFree(bdf->glyphs[gid]->undoes); bdf->glyphs[gid]->undoes = NULL;
		UndoesFree(bdf->glyphs[gid]->redoes); bdf->glyphs[gid]->redoes = NULL;
	    }
	}
	k = 0;
	do {
	    ssf = main->subfontcnt==0? main: main->subfonts[k];
	    if ( gid<ssf->glyphcnt && ssf->glyphs[gid]!=NULL ) {
		sc = ssf->glyphs[gid];
		for ( layer = 0; layer<sc->layer_cnt; ++layer ) {
		    UndoesFree(sc->layers[layer].undoes); sc->layers[layer].undoes = NULL;
		    UndoesFree(sc->layers[layer].redoes); sc->layers[layer].redoes = NULL;
		}
	    }
	    ++k;
	} while ( k<main->subfontcnt );
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuRemoveUndoes(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveUndoes(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFRemoveUndoes(fv->sf,fv->selected,fv->map);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuUndo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Undo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i,j,layer, gid;
    MMSet *mm = fv->sf->mm;
    int was_blended = mm!=NULL && mm->normal==fv->sf;

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL && !fv->sf->glyphs[gid]->ticked) {
	    SplineChar *sc = fv->sf->glyphs[gid];
	    for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
		if ( sc->layers[layer].undoes!=NULL ) {
		    SCDoUndo(sc,layer);
		    if ( was_blended ) {
			for ( j=0; j<mm->instance_count; ++j )
			    SCDoUndo(mm->instances[j]->glyphs[gid],layer);
		    }
		}
	    }
	    sc->ticked = true;
	}
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuRedo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Redo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i,j,layer, gid;
    MMSet *mm = fv->sf->mm;
    int was_blended = mm!=NULL && mm->normal==fv->sf;

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL && !fv->sf->glyphs[gid]->ticked) {
	    SplineChar *sc = fv->sf->glyphs[gid];
	    for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
		if ( sc->layers[layer].redoes!=NULL ) {
		    SCDoRedo(sc,layer);
		    if ( was_blended ) {
			for ( j=0; j<mm->instance_count; ++j )
			    SCDoRedo(mm->instances[j]->glyphs[gid],layer);
		    }
		}
	    }
	    sc->ticked = true;
	}
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCut(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Cut(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVCopy(fv,true);
    FVClear(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSelectAll(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SelectAll(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVSelectAll(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuInvertSelection(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_InvertSelection(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVInvertSelection(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuDeselectAll(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_DeselectAll(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVDeselectAll(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSelectChanged(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SelectChanged(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i, gid;
    EncMap *map = fv->map;
    SplineFont *sf = fv->sf;

    for ( i=0; i< map->enccount; ++i )
	fv->selected[i] = ( (gid=map->map[i])!=-1 && sf->glyphs[gid]!=NULL && sf->glyphs[gid]->changed );
    GDrawRequestExpose(fv->v,NULL,false);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSelectHintingNeeded(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SelectHintingNeeded(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i, gid;
    EncMap *map = fv->map;
    SplineFont *sf = fv->sf;

    for ( i=0; i< map->enccount; ++i )
	fv->selected[i] = ( (gid=map->map[i])!=-1 && sf->glyphs[gid]!=NULL && sf->glyphs[gid]->changedsincelasthinted );
    GDrawRequestExpose(fv->v,NULL,false);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSelectColor(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVSelectColor(fv,(uint32) (mi->ti.userdata),(e->u.chr.state&ksm_shift)?1:0);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SelectDefault(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,COLOR_DEFAULT,false);
}

void FontViewMenu_SelectWhite(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0xffffff,false);
}

void FontViewMenu_SelectRed(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0xff0000,false);
}

void FontViewMenu_SelectGreen(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0x00ff00,false);
}

void FontViewMenu_SelectBlue(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0x0000ff,false);
}

void FontViewMenu_SelectYellow(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0xffff00,false);
}

void FontViewMenu_SelectCyan(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0x00ffff,false);
}

void FontViewMenu_SelectBlue(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0x0000ff,false);
}

void FontViewMenu_SelectMagenta(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FVSelectColor(fv,0xff00ff,false);
}

void FontViewMenu_SelectColorPicker(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    GtkWidget *color_picker = gtk_color_selection_dialog_new(_("Select Color"));
    static GdkColor last_col;

    gtk_color_selection_set_current_color(
	    GTK_COLOR_SELECTION_DIALOG( color_picker )->colorsel,
	    &last_col );
    if (gtk_dialog_run( GTK_DIALOG( color_picker )) == GTK_RESPONSE_ACCEPT) {
	gtk_color_selection_get_current_color(
		GTK_COLOR_SELECTION_DIALOG( color_picker )->colorsel,
		&last_col );
	FVSelectColor(fv,((last_col.red>>8)<<16) |
			((last_col.green>>8)<<8) |
			 (last_col.blue>>8),
		false);
    }
    gtk_widget_destroy( color_picker );
}
# endif

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSelectByPST(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_SelectByPST(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVSelectByPST(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuFindRpl(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_FindRpl(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    SVCreate(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuReplaceWithRef(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ReplaceWithRef(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVReplaceOutlineWithReference(fv,.01);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuCharInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CharInfo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int pos = FVAnyCharSelected(fv);
    if ( pos<0 )
return;
    if ( fv->cidmaster!=NULL &&
	    (fv->map->map[pos]==-1 || fv->sf->glyphs[fv->map->map[pos]]==NULL ))
return;
    SCCharInfo(SFMakeChar(fv->sf,fv->map,pos),fv->map,pos);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAAT(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    int i,gid;
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    uint32 tag = (uint32) (mi->ti.userdata);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AAT(GtkMenuItem *menuitem, gpointer user_data) {
    int i, gid;
    FontView *fv = FV_From_MI(menuitem);
    guint32 tag;
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);

    if ( strcmp(name,"all1")==0 )
	tag = 0;
    else if ( strcmp(name,"ligatures1")==0 )
	tag = 0xffffffff;
    else if ( strcmp(name,"standard_ligatures1")==0 )
	tag = CHR('l','i','g','a');
    else if ( strcmp(name,"required_ligatures1")==0 )
	tag = CHR('r','l','i','g');
    else if ( strcmp(name,"historic_ligatures1")==0 )
	tag = CHR('h','l','i','g');
    else if ( strcmp(name,"discretionary_ligatures1")==0 )
	tag = CHR('d','l','i','g');
    else if ( strcmp(name,"diagonal_fractions1")==0 )
	tag = CHR('f','r','a','c');
    else if ( strcmp(name,"unicode_decomposition1")==0 )
	tag = ((27<<16)|1);
    else if ( strcmp(name,"right_to_left_alternates1")==0 )
	tag = CHR('r','t','l','a');
    else if ( strcmp(name,"vertical_rotation_alternates1")==0 )
	tag = CHR('v','r','t','2');
    else if ( strcmp(name,"lowercase_to_smallcaps1")==0 )
	tag = CHR('s','m','c','p');
    else if ( strcmp(name,"uppercase_to_small_caps1")==0 )
	tag = CHR('c','2','s','c');
    else if ( strcmp(name,"oldstyle_figures1")==0 )
	tag = CHR('o','n','u','m');
    else if ( strcmp(name,"superscript1")==0 )
	tag = CHR('s','u','p','s');
    else if ( strcmp(name,"subscript1")==0 )
	tag = CHR('s','u','b','s');
    else if ( strcmp(name,"swash1")==0 )
	tag = CHR('s','w','s','h');
    else if ( strcmp(name,"proportional_width1")==0 )
	tag = CHR('p','w','i','d');
    else if ( strcmp(name,"full_width1")==0 )
	tag = CHR('f','w','i','d');
    else if ( strcmp(name,"half_width1")==0 )
	tag = CHR('h','w','i','d');
    else if ( strcmp(name,"initial_forms1")==0 )
	tag = CHR('i','n','i','t');
    else if ( strcmp(name,"medial_forms1")==0 )
	tag = CHR('m','e','d','i');
    else if ( strcmp(name,"terminal_forms1")==0 )
	tag = CHR('f','i','n','a');
    else if ( strcmp(name,"isolated_forms1")==0 )
	tag = CHR('i','s','o','l');
    else if ( strcmp(name,"left_bounds1")==0 )
	tag = CHR('l','f','b','d');
    else if ( strcmp(name,"right_bounds1")==0 )
	tag = CHR('r','t','b','d');
# endif

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL )
	    SCTagDefault(fv->sf->glyphs[gid],tag);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAATSuffix(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AATSuffix(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    uint32 tag;
    char *suffix;
    unichar_t *usuffix, *upt, *end;
    int i, gid;
    uint16 flags, sli;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 && fv->sf->glyphs[gid]!=NULL )
	if ( SCScriptFromUnicode(fv->sf->glyphs[gid])!=DEFAULT_SCRIPT )
    break;
    usuffix = AskNameTag(_STR_SuffixToTag,NULL,0,0,-1,pst_substitution,fv->sf,fv->sf->glyphs[gid],-1,-1);
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
    while ( *end==' ' ) ++end;

    if ( *end=='.' ) ++end;
    suffix = cu_copy(end);
    free(usuffix);
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL )
	    SCSuffixDefault(fv->sf->glyphs[gid],tag,suffix,flags,sli);
    free(suffix);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuCopyFeatureToFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CopyFeatureToFont(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFCopyFeatureToFontDlg(fv->sf);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRemoveAllFeatures(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    if ( !SFRemoveThisFeatureTag(fv->sf,0xffffffff,SLI_UNKNOWN,-1))
	GWidgetErrorR(_STR_NoFeaturesRemoved,_STR_NoFeaturesRemoved);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveAllFeatures(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    if ( !SFRemoveThisFeatureTag(fv->sf,0xffffffff,SLI_UNKNOWN,-1))
	gwwv_post_error(_("No Features Removed"),_("No Features Removed"));
}
# endif

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRemoveFeatures(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveFeatures(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFRemoveFeatureDlg(fv->sf);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRemoveUnusedNested(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( !SFRemoveUnusedNestedFeatures(fv->sf))
	GWidgetErrorR(_STR_NoFeaturesRemoved,_STR_NoFeaturesRemoved);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_UnusedNested(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( !SFRemoveUnusedNestedFeatures(fv->sf))
	gwwv_post_error(_("No Features Removed"),_("No Features Removed"));
}
# endif

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRetagFeature(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RetagFeature(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFRetagFeatureDlg(fv->sf);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuShowDependentRefs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowDependentRefs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int pos = FVAnyCharSelected(fv);
    SplineChar *sc;

    if ( pos<0 || fv->map->map[pos]==-1 )
return;
    sc = fv->sf->glyphs[fv->map->map[pos]];
    if ( sc==NULL || sc->dependents==NULL )
return;
    SCRefBy(sc);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuShowDependentSubs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowDependentSubs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int pos = FVAnyCharSelected(fv);
    SplineChar *sc;

    if ( pos<0 || fv->map->map[pos]==-1 )
return;
    sc = fv->sf->glyphs[fv->map->map[pos]];
    if ( sc==NULL )
return;
    SCSubBy(sc);
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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
#endif

void TransHints(StemInfo *stem,real mul1, real off1, real mul2, real off2, int round_to_int ) {
    HintInstance *hi;

    for ( ; stem!=NULL; stem=stem->next ) {
	stem->start = stem->start*mul1 + off1;
	stem->width *= mul1;
	if ( round_to_int ) {
	    stem->start = rint(stem->start);
	    stem->width = rint(stem->width);
	}
	if ( mul1<0 ) {
	    stem->start += stem->width;
	    stem->width = -stem->width;
	}
	for ( hi=stem->where; hi!=NULL; hi=hi->next ) {
	    hi->begin = hi->begin*mul2 + off2;
	    hi->end = hi->end*mul2 + off2;
	    if ( round_to_int ) {
		hi->begin = rint(hi->begin);
		hi->end = rint(hi->end);
	    }
	    if ( mul2<0 ) {
		double temp = hi->begin;
		hi->begin = hi->end;
		hi->end = temp;
	    }
	}
    }
}

void VrTrans(struct vr *vr,real transform[6]) {
    /* I'm interested in scaling and skewing. I think translation should */
    /*  not affect these guys (they are offsets, so offsets should be */
    /*  unchanged by translation */
    double x,y;

    x = vr->xoff; y=vr->yoff;
    vr->xoff = rint(transform[0]*x + transform[1]*y);
    vr->yoff = rint(transform[2]*x + transform[3]*y);
    x = vr->h_adv_off; y=vr->v_adv_off;
    vr->h_adv_off = rint(transform[0]*x + transform[1]*y);
    vr->v_adv_off = rint(transform[2]*x + transform[3]*y);
}

static void KCTrans(KernClass *kc,double scale) {
    /* Again these are offsets, so I don't apply translation */
    int i;

    for ( i=kc->first_cnt*kc->second_cnt-1; i>=0; --i )
	kc->offsets[i] = rint(scale*kc->offsets[i]);
}

void FVTrans(FontView *fv,SplineChar *sc,real transform[6], uint8 *sel,
	enum fvtrans_flags flags) {
    RefChar *refs;
    real t[6];
    AnchorPoint *ap;
    int i,j;
    KernPair *kp;
    PST *pst;

    if ( sc->blended ) {
	int j;
	MMSet *mm = sc->parent->mm;
	for ( j=0; j<mm->instance_count; ++j )
	    FVTrans(fv,mm->instances[j]->glyphs[sc->orig_pos],transform,sel,flags);
    }

    SCPreserveState(sc,true);
    if ( !(flags&fvt_dontmovewidth) )
	if ( transform[0]>0 && transform[3]>0 && transform[1]==0 && transform[2]==0 ) {
	    int widthset = sc->widthset;
	    SCSynchronizeWidth(sc,sc->width*transform[0]+transform[4],sc->width,fv);
	    if ( !(flags&fvt_dontsetwidth) ) sc->widthset = widthset;
	    sc->vwidth = sc->vwidth*transform[3]+transform[5];
	}
    if ( flags & fvt_scalepstpos ) {
	for ( kp=sc->kerns; kp!=NULL; kp=kp->next )
	    kp->off = rint(kp->off*transform[0]);
	for ( kp=sc->vkerns; kp!=NULL; kp=kp->next )
	    kp->off = rint(kp->off*transform[3]);
	for ( pst = sc->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type == pst_position )
		VrTrans(&pst->u.pos,transform);
	    else if ( pst->type==pst_pair ) {
		VrTrans(&pst->u.pair.vr[0],transform);
		VrTrans(&pst->u.pair.vr[1],transform);
	    }
	}
    }
    for ( ap=sc->anchor; ap!=NULL; ap=ap->next )
	ApTransform(ap,transform);
    for ( i=ly_fore; i<sc->layer_cnt; ++i ) {
	SplinePointListTransform(sc->layers[i].splines,transform,true);
	for ( refs = sc->layers[i].refs; refs!=NULL; refs=refs->next ) {
	    if ( sel!=NULL && sel[fv->map->backmap[refs->sc->orig_pos]] ) {
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
		    for ( j=0; j<refs->layer_cnt; ++j )
			SplinePointListTransform(refs->layers[j].splines,t,true);
		}
	    } else {
		for ( j=0; j<refs->layer_cnt; ++j )
		    SplinePointListTransform(refs->layers[j].splines,transform,true);
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
	    RefCharFindBounds(refs);
	}
    }
    if ( transform[1]==0 && transform[2]==0 ) {
	TransHints(sc->hstem,transform[3],transform[5],transform[0],transform[4],flags&fvt_round_to_int);
	TransHints(sc->vstem,transform[0],transform[4],transform[3],transform[5],flags&fvt_round_to_int);
    }
    if ( flags&fvt_round_to_int )
	SCRound2Int(sc,1.0);
    if ( transform[0]==1 && transform[3]==1 && transform[1]==0 &&
	    transform[2]==0 && transform[5]==0 &&
	    transform[4]!=0 && 
	    sc->unicodeenc!=-1 && isalpha(sc->unicodeenc)) {
	SCUndoSetLBearingChange(sc,(int) rint(transform[4]));
	SCSynchronizeLBearing(sc,transform[4]);
    }
    if ( flags&fvt_dobackground ) {
	ImageList *img;
	SCPreserveBackground(sc);
	SplinePointListTransform(sc->layers[ly_back].splines,transform,true);
	for ( img = sc->layers[ly_back].images; img!=NULL; img=img->next )
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
    int i, cnt=0, gid;
    BDFFont *bdf;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;

    if ( cnt>10 || cnt>fv->sf->glyphcnt/2 ) {
	if ( transform[3]!=1.0 ) {
	    fv->sf->pfminfo.os2_typoascent = 0;		/* Value won't be valid after a masive scale */
	    if ( transform[2]==0 )
		fv->sf->pfminfo.linegap *= transform[3];
	}
	if ( transform[1]==0 && transform[0]!=1.0 )
	    fv->sf->pfminfo.vlinegap *= transform[0];
    }

# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_Transforming,_STR_Transforming,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Transforming..."),_("Transforming..."),0,cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 &&
	    SCWorthOutputting(fv->sf->glyphs[gid]) &&
	    !fv->sf->glyphs[gid]->ticked ) {
	SplineChar *sc = fv->sf->glyphs[gid];

	if ( onlycopydisplayed && fv->show!=fv->filled ) {
	    if ( fv->show->glyphs[gid]!=NULL )
		BCTrans(bdf,fv->show->glyphs[gid],bvts,fv);
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
		for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
		    if ( gid<bdf->glyphcnt && bdf->glyphs[gid]!=NULL )
			BCTrans(bdf,bdf->glyphs[gid],bvts,fv);
	    }
	}
	sc->ticked = true;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif

    if ( flags&fvt_scalekernclasses ) {
	KernClass *kc;
	SplineFont *sf = fv->cidmaster!=NULL ? fv->cidmaster : fv->sf;
	for ( kc=sf->kerns; kc!=NULL; kc=kc->next )
	    KCTrans(kc,transform[0]);
	for ( kc=sf->vkerns; kc!=NULL; kc=kc->next )
	    KCTrans(kc,transform[3]);
    }
}

int SFScaleToEm(SplineFont *sf, int as, int des) {
    double scale;
    real transform[6];
    BVTFunc bvts;
    uint8 *oldselected = sf->fv->selected;

    scale = (as+des)/(double) (sf->ascent+sf->descent);
    sf->pfminfo.hhead_ascent *= scale;
    sf->pfminfo.hhead_descent *= scale;
    sf->pfminfo.linegap *= scale;
    sf->pfminfo.vlinegap *= scale;
    sf->pfminfo.os2_winascent *= scale;
    sf->pfminfo.os2_windescent *= scale;
    if ( sf->pfminfo.os2_typoascent!=0 ) {
	sf->pfminfo.os2_typoascent = as;
	sf->pfminfo.os2_typodescent = -des;
    }
    sf->pfminfo.os2_typolinegap *= scale;

    sf->pfminfo.os2_subxsize *= scale;
    sf->pfminfo.os2_subysize *= scale;
    sf->pfminfo.os2_subxoff *= scale;
    sf->pfminfo.os2_subyoff *= scale;
    sf->pfminfo.os2_supxsize *= scale;
    sf->pfminfo.os2_supysize *= scale;
    sf->pfminfo.os2_supxoff *= scale;
    sf->pfminfo.os2_supyoff *= scale;
    sf->pfminfo.os2_strikeysize *= scale;
    sf->pfminfo.os2_strikeypos *= scale;
    sf->upos *= scale;
    sf->uwidth *= scale;

    if ( as+des == sf->ascent+sf->descent ) {
	if ( as!=sf->ascent && des!=sf->descent ) {
	    sf->ascent = as; sf->descent = des;
	    sf->changed = true;
	}
return( false );
    }

    transform[0] = transform[3] = scale;
    transform[1] = transform[2] = transform[4] = transform[5] = 0;
    bvts.func = bvt_none;
    sf->fv->selected = galloc(sf->fv->map->enccount);
    memset(sf->fv->selected,1,sf->fv->map->enccount);

    sf->ascent = as; sf->descent = des;

    FVTransFunc(sf->fv,transform,0,&bvts,
	    fvt_dobackground|fvt_round_to_int|fvt_dontsetwidth|fvt_scalekernclasses|fvt_scalepstpos);
    free(sf->fv->selected);
    sf->fv->selected = oldselected;

    if ( !sf->changed ) {
	sf->changed = true;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
	FVSetTitle(sf->fv);
#endif
    }
	
return( true );
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuTransform(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Transform(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int flags=0x3;
    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( FVAllSelected(fv))
	flags = 0x7;
    TransformDlgCreate(fv,FVTransFunc,getorigin,flags,cvt_none);
}

#if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuPOV(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    struct pov_data pov_data;
    if ( FVAnyCharSelected(fv)==-1 || fv->sf->onlybitmaps )
return;
    if ( PointOfViewDlg(&pov_data,fv->sf,false)==-1 )
return;
    FVPointOfView(fv,&pov_data);
}

static void FVMenuNLTransform(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    if ( FVAnyCharSelected(fv)==-1 )
return;
    NonLinearDlg(fv,NULL);
}
#elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_PointOfView(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    struct pov_data pov_data;
    if ( FVAnyCharSelected(fv)==-1 || fv->sf->onlybitmaps )
return;
    if ( PointOfViewDlg(&pov_data,fv->sf,false)==-1 )
return;
    FVPointOfView(fv,&pov_data);
}

void FontViewMenu_NonLinearTransform(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    if ( FVAnyCharSelected(fv)==-1 || fv->sf->onlybitmaps )
return;
    NonLinearDlg(fv,NULL);
}
#endif

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuBitmaps(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    BitmapDlg(fv,NULL,mi->mid==MID_AvailBitmaps );
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_BitmapsAvail(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    BitmapDlg(fv,NULL,true );
}

void FontViewMenu_RegenBitmaps(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    BitmapDlg(fv,NULL,false );
}
# endif

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuStroke(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Stroke(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVStroke(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
#  ifdef FONTFORGE_CONFIG_TILEPATH
static void FVMenuTilePath(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FVTile(fv);
}
#  endif
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_TilePath(GtkMenuItem *menuitem, gpointer user_data) {
#  ifdef FONTFORGE_CONFIG_TILEPATH
    FontView *fv = FV_From_MI(menuitem);
    FVTile(fv);
#  endif
}
# endif
#endif

static void FVOverlap(FontView *fv,enum overlap_type ot) {
    int i, cnt=0, layer, gid;
    SplineChar *sc;

    /* We know it's more likely that we'll find a problem in the overlap code */
    /*  than anywhere else, so let's save the current state against a crash */
    DoAutoSaves();

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;

# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_RemovingOverlap,_STR_RemovingOverlap,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Removing overlaps..."),_("Removing overlaps..."),0,cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 &&
	    SCWorthOutputting((sc=fv->sf->glyphs[gid])) &&
	    !sc->ticked ) {
	sc->ticked = true;
	if ( !SCRoundToCluster(sc,-2,false,.03,.12))
	    SCPreserveState(sc,false);
	MinimumDistancesFree(sc->md);
	sc->md = NULL;
	for ( layer = ly_fore; layer<sc->layer_cnt; ++layer )
	    sc->layers[layer].splines = SplineSetRemoveOverlap(sc,sc->layers[layer].splines,ot);
	SCCharChangedUpdate(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuOverlap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    /* We know it's more likely that we'll find a problem in the overlap code */
    /*  than anywhere else, so let's save the current state against a crash */
    DoAutoSaves();

    FVOverlap(fv,mi->mid==MID_RmOverlap ? over_remove :
		 mi->mid==MID_Intersection ? over_intersect :
		      over_findinter);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveOverlap(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    /* We know it's more likely that we'll find a problem in the overlap code */
    /*  than anywhere else, so let's save the current state against a crash */
    DoAutoSaves();

    FVOverlap(fv,over_remove);
}

void FontViewMenu_Intersect(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    DoAutoSaves();
    FVOverlap(fv,over_intersect);
}

void FontViewMenu_FindInter(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    DoAutoSaves();
    FVOverlap(fv,over_findinter);
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuInline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Inline(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    OutlineDlg(fv,NULL,NULL,true);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuOutline(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Outline(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    OutlineDlg(fv,NULL,NULL,false);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuShadow(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Shadow(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    ShadowDlg(fv,NULL,NULL,false);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuWireframe(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Wireframe(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    ShadowDlg(fv,NULL,NULL,true);
}
#endif

void _FVSimplify(FontView *fv,struct simplifyinfo *smpl) {
    int i, cnt=0, layer, gid;
    SplineChar *sc;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_Simplifying,_STR_Simplifying,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Simplifying..."),_("Simplifying..."),0,cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i )
	if ( (gid=fv->map->map[i])!=-1 && SCWorthOutputting((sc=fv->sf->glyphs[gid])) &&
		fv->selected[i] && !sc->ticked ) {
	    sc->ticked = true;
	    SCPreserveState(sc,false);
	    for ( layer = ly_fore; layer<sc->layer_cnt; ++layer )
		sc->layers[layer].splines = SplineCharSimplify(sc,sc->layers[layer].splines,smpl);
	    SCCharChangedUpdate(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	    if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	    if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
	}
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void FVSimplify(FontView *fv,int type) {
    static struct simplifyinfo smpls[] = {
	    { sf_normal },
	    { sf_normal,.75,.05,0,-1 },
	    { sf_normal,.75,.05,0,-1 }};
    struct simplifyinfo *smpl = &smpls[type+1];

    if ( smpl->linelenmax==-1 || (type==0 && !smpl->set_as_default)) {
	smpl->err = (fv->sf->ascent+fv->sf->descent)/1000.;
	smpl->linelenmax = (fv->sf->ascent+fv->sf->descent)/100.;
    }

    if ( type==1 ) {
	if ( !SimplifyDlg(fv->sf,smpl))
return;
	if ( smpl->set_as_default )
	    smpls[1] = *smpl;
    }
    _FVSimplify(fv,smpl);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuSimplify(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),false );
}

static void FVMenuSimplifyMore(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),true );
}

static void FVMenuCleanup(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVSimplify( (FontView *) GDrawGetUserData(gw),-1 );
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Simplify(GtkMenuItem *menuitem, gpointer user_data) {
    FVSimplify( (FontView *) FV_From_MI(menuitem), false);
}

void FontViewMenu_SimplifyMore(GtkMenuItem *menuitem, gpointer user_data) {
    FVSimplify( (FontView *) FV_From_MI(menuitem), true);
}

void FontViewMenu_Cleanup(GtkMenuItem *menuitem, gpointer user_data) {
    FVSimplify( (FontView *) FV_From_MI(menuitem), -1);
}
#endif
#endif

static void FVAddExtrema(FontView *fv) {
    int i, cnt=0, layer, gid;
    SplineChar *sc;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_AddingExtrema,_STR_AddingExtrema,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Adding points at Extrema..."),_("Adding points at Extrema..."),0,cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 &&
	    SCWorthOutputting((sc = fv->sf->glyphs[gid])) &&
	    !sc->ticked) {
	sc->ticked = true;
	SCPreserveState(sc,false);
	for ( layer = ly_fore; layer<sc->layer_cnt; ++layer )
	    SplineCharAddExtrema(sc->layers[layer].splines,ae_only_good,sc->parent);
	SCCharChangedUpdate(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAddExtrema(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVAddExtrema( (FontView *) GDrawGetUserData(gw) );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AddExtrema(GtkMenuItem *menuitem, gpointer user_data) {
    FVAddExtrema( (FontView *) FV_From_MI(menuitem) );
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuCorrectDir(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CorrectDir(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i, cnt=0, changed, refchanged, preserved, layer, gid;
    int askedall=-1, asked;
    RefChar *ref, *next;
    SplineChar *sc;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_CorrectingDirection,_STR_CorrectingDirection,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Correcting Direction..."),_("Correcting Direction..."),0,cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting((sc=fv->sf->glyphs[gid])) &&
	    !sc->ticked ) {
	sc->ticked = true;
	changed = refchanged = preserved = false;
	asked = askedall;
	for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
	    for ( ref=sc->layers[layer].refs; ref!=NULL; ref=next ) {
		next = ref->next;
		if ( ref->transform[0]*ref->transform[3]<0 ||
			(ref->transform[0]==0 && ref->transform[1]*ref->transform[2]>0)) {
		    if ( asked==-1 ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
			static int buts[] = { _STR_UnlinkAll, _STR_Unlink, _STR_No, _STR_Cancel, 0 };
			asked = GWidgetAskR(_STR_FlippedRef,buts,0,2,_STR_FlippedRefUnlink, sc->name );
#elif defined(FONTFORGE_CONFIG_GTK)
			char *buts[5];
			buts[0] = _("Unlink All");
			buts[1] = _("Unlink");
			buts[2] = GTK_STOCK_CANCEL;
			buts[3] = NULL;
			asked = gwwv_ask(_("Flipped Reference"),buts,0,2,_("%.50s contains a flipped reference. This cannot be corrected as is. Would you like me to unlink it and then correct it?"), sc->name );
#endif
			if ( asked==3 )
return;
			else if ( asked==2 )
	    break;
			else if ( asked==0 )
			    askedall = 0;
		    }
		    if ( asked==0 || asked==1 ) {
			if ( !preserved ) {
			    preserved = refchanged = true;
			    SCPreserveState(sc,false);
			}
			SCRefToSplines(sc,ref);
		    }
		}
	    }

	    if ( !preserved && sc->layers[layer].splines!=NULL ) {
		SCPreserveState(sc,false);
		preserved = true;
	    }
	    sc->layers[layer].splines = SplineSetsCorrect(sc->layers[layer].splines,&changed);
	}
	if ( changed || refchanged )
	    SCCharChangedUpdate(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}
#endif

static void FVRound2Int(FontView *fv,real factor) {
    int i, cnt=0, gid;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_Rounding,_STR_Rounding,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Rounding to integer..."),_("Rounding to integer..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];

	SCPreserveState(sc,false);
	SCRound2Int( sc,factor);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRound2Int(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVRound2Int( (FontView *) GDrawGetUserData(gw), 1.0 );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Round2Int(GtkMenuItem *menuitem, gpointer user_data) {
    FVRound2Int( (FontView *) FV_From_MI(menuitem), 1.0);
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRound2Hundredths(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVRound2Int( (FontView *) GDrawGetUserData(gw),100.0 );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Round2Hundredths(GtkMenuItem *menuitem, gpointer user_data) {
    FVRound2Int( (FontView *) FV_From_MI(menuitem),100.0);
# endif
}

static void FVCluster(FontView *fv) {
    int i, cnt=0, gid;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_Rounding,_STR_Rounding,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Rounding to integer..."),_("Rounding to integer..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SCRoundToCluster(fv->sf->glyphs[gid],-2,false,.1,.5);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuCluster(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVCluster( (FontView *) GDrawGetUserData(gw));
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Cluster(GtkMenuItem *menuitem, gpointer user_data) {
    FVCluster( (FontView *) FV_From_MI(menuitem));
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAutotrace(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Autotrace(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVAutoTrace(fv,e!=NULL && (e->u.mouse.state&ksm_shift));
}
#endif

void FVBuildAccent(FontView *fv,int onlyaccents) {
    int i, cnt=0, gid;
    SplineChar dummy;
    SplineChar *sc;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_Buildingaccented,_STR_Buildingaccented,_STR_NULL,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Building accented letters"),_("Building accented letters"),_(),cnt,1);
# endif

    SFUntickAll(fv->sf);
    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
	gid = fv->map->map[i];
	sc = NULL;
	if ( gid!=-1 ) {
	    sc = fv->sf->glyphs[gid];
	    if ( sc!=NULL && sc->ticked )
    continue;
	}
	if ( sc==NULL )
	    sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
	else if ( !no_windowing_ui && sc->unicodeenc == 0x00c5 /* Aring */ &&
		sc->layers[ly_fore].splines!=NULL ) {
#if defined(FONTFORGE_CONFIG_GDRAW)
	    static int buts[] = { _STR_Yes, _STR_No, 0 };
	    if ( GWidgetAskR(_STR_Replacearing,buts,0,1,_STR_Areyousurearing)==1 )
#elif defined(FONTFORGE_CONFIG_GTK)
    static char *buts[] = { GTK_STOCK_YES, GTK_STOCK_NO, NULL };
	    if ( gwwv_ask(_("Replace Å"),buts,0,1,_("Are you sure you want to replace Å?\nThe ring will not join to the A."))==1 )
#endif
    continue;
	}
	if ( SFIsSomethingBuildable(fv->sf,sc,onlyaccents) ) {
	    sc = SFMakeChar(fv->sf,fv->map,i);
	    sc->ticked = true;
	    SCBuildComposit(fv->sf,sc,!onlycopydisplayed,fv);
	}
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuBuildAccent(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVBuildAccent( (FontView *) GDrawGetUserData(gw), true );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Transform(GtkMenuItem *menuitem, gpointer user_data) {
    FVBuildAccent( (FontView *) FV_From_MI(menuitem), true );
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuBuildComposite(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVBuildAccent( (FontView *) GDrawGetUserData(gw), false );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Transform(GtkMenuItem *menuitem, gpointer user_data) {
    FVBuildAccent( (FontView *) FV_From_MI(menuitem), false );
# endif
}
#endif

static int SFIsDuplicatable(SplineFont *sf, SplineChar *sc) {
    extern const int cns14pua[], amspua[];
    const int *pua = sf->uni_interp==ui_trad_chinese ? cns14pua : sf->uni_interp==ui_ams ? amspua : NULL;
    int baseuni = 0;
    const unichar_t *pt;

    if ( pua!=NULL && sc->unicodeenc>=0xe000 && sc->unicodeenc<=0xf8ff )
	baseuni = pua[sc->unicodeenc-0xe000];
    if ( baseuni==0 && ( pt = SFGetAlternate(sf,sc->unicodeenc,sc,false))!=NULL &&
	    pt[0]!='\0' && pt[1]=='\0' )
	baseuni = pt[0];
    if ( baseuni!=0 && SFGetChar(sf,baseuni,NULL)!=NULL )
return( true );

return( false );
}

void FVBuildDuplicate(FontView *fv) {
    extern const int cns14pua[], amspua[];
    const int *pua = fv->sf->uni_interp==ui_trad_chinese ? cns14pua : fv->sf->uni_interp==ui_ams ? amspua : NULL;
    int i, cnt=0, gid;
    SplineChar dummy;
    const unichar_t *pt;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] )
	++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_BuildingDuplicates,_STR_BuildingDuplicates,_STR_NULL,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Building duplicate encodings"),_("Building duplicate encodings"),NULL,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
	SplineChar *sc;
	int baseuni = 0;
	if ( (gid = fv->map->map[i])==-1 || (sc = fv->sf->glyphs[gid])==NULL )
	    sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
	if ( pua!=NULL && sc->unicodeenc>=0xe000 && sc->unicodeenc<=0xf8ff )
	    baseuni = pua[sc->unicodeenc-0xe000];
	if ( baseuni==0 && ( pt = SFGetAlternate(fv->sf,sc->unicodeenc,sc,false))!=NULL &&
		pt[0]!='\0' && pt[1]=='\0' )
	    baseuni = pt[0];
	
	if ( baseuni!=0 && (gid = SFFindExistingSlot(fv->sf,baseuni,NULL))!=-1 )
	    LinkEncToGid(fv,i,gid);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuBuildDuplicate(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FVBuildDuplicate( (FontView *) GDrawGetUserData(gw));
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_BuildDuplicatem(GtkMenuItem *menuitem, gpointer user_data) {
    FVBuildDuplicate( (FontView *) FV_From_MI(menuitem));
# endif
}
#endif

#ifdef KOREAN
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuShowGroup(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    ShowGroup( ((FontView *) GDrawGetUserData(gw))->sf );
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowGroup(GtkMenuItem *menuitem, gpointer user_data) {
    ShowGroup( ((FontView *) FV_From_MI(menuitem))->sf );
# endif
}
#endif
#endif

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

static void SCFreeMostContents(SplineChar *sc) {
    int i;

    free(sc->name);
    for ( i=0; i<sc->layer_cnt; ++i ) {
	SplinePointListsFree(sc->layers[i].splines); sc->layers[i].splines = NULL;
	RefCharsFree(sc->layers[i].refs); sc->layers[i].refs = NULL;
	ImageListsFree(sc->layers[i].images); sc->layers[i].images = NULL;
	/* image garbage collection????!!!! */
	/* Keep undoes */
    }
    StemInfosFree(sc->hstem); sc->hstem = NULL;
    StemInfosFree(sc->vstem); sc->vstem = NULL;
    DStemInfosFree(sc->dstem); sc->dstem = NULL;
    MinimumDistancesFree(sc->md); sc->md = NULL;
    KernPairsFree(sc->kerns); sc->kerns = NULL;
    KernPairsFree(sc->vkerns); sc->vkerns = NULL;
    AnchorPointsFree(sc->anchor); sc->anchor = NULL;
    SplineCharListsFree(sc->dependents); sc->dependents = NULL;
    PSTFree(sc->possub); sc->possub = NULL;
    free(sc->ttf_instrs); sc->ttf_instrs = NULL;
    free(sc->countermasks); sc->countermasks = NULL;
#ifdef FONTFORGE_CONFIG_TYPE3
    /*free(sc->layers);*/	/* don't free this, leave it empty */
#endif
}

static void SCReplaceWith(SplineChar *dest, SplineChar *src) {
    int opos=dest->orig_pos, uenc=dest->unicodeenc;
    Undoes *u[2], *r1;
    struct splinecharlist *scl = dest->dependents;
    RefChar *refs;
    int layer;
#ifdef FONTFORGE_CONFIG_TYPE3
    int lc;
    Layer *layers;
#else
    SplineSet *back = dest->layers[ly_back].splines;
    ImageList *images = dest->layers[ly_back].images;
#endif

    if ( src==dest )
return;

    SCPreserveState(dest,2);
    u[0] = dest->layers[ly_fore].undoes; u[1] = dest->layers[ly_back].undoes; r1 = dest->layers[ly_back].redoes;

    free(dest->name);
    for ( layer = ly_fore; layer<dest->layer_cnt; ++layer ) {
	SplinePointListsFree(dest->layers[layer].splines);
	RefCharsFree(dest->layers[layer].refs);
#ifdef FONTFORGE_CONFIG_TYPE3
	ImageListsFree(dest->layers[layer].images);
#endif
    }
    StemInfosFree(dest->hstem);
    StemInfosFree(dest->vstem);
    DStemInfosFree(dest->dstem);
    MinimumDistancesFree(dest->md);
    KernPairsFree(dest->kerns);
    KernPairsFree(dest->vkerns);
    AnchorPointsFree(dest->anchor);
    PSTFree(dest->possub);
    free(dest->ttf_instrs);

#ifdef FONTFORGE_CONFIG_TYPE3
    layers = dest->layers;
    lc = dest->layer_cnt;
    *dest = *src;
    dest->layers = galloc(dest->layer_cnt*sizeof(Layer));
    memcpy(&dest->layers[ly_back],&layers[ly_back],sizeof(Layer));
    for ( layer=ly_fore; layer<dest->layer_cnt; ++layer ) {
	memcpy(&dest->layers[layer],&src->layers[layer],sizeof(Layer));
	dest->layers[layer].undoes = dest->layers[layer].redoes = NULL;
	src->layers[layer].splines = NULL;
	src->layers[layer].images = NULL;
	src->layers[layer].refs = NULL;
    }
    for ( layer=ly_fore; layer<dest->layer_cnt && layer<lc; ++layer )
	dest->layers[layer].undoes = layers[layer].undoes;
    for ( ; layer<lc; ++layer )
	UndoesFree(layers[layer].undoes);
    free(layers);
#else
    *dest = *src;
    dest->layers[ly_back].images = images;
    dest->layers[ly_back].splines = back;
    dest->layers[ly_fore].undoes = u[0]; dest->layers[ly_back].undoes = u[1]; dest->layers[ly_fore].redoes = NULL; dest->layers[ly_back].redoes = r1;

    src->layers[ly_fore].splines = NULL;
    src->layers[ly_fore].refs = NULL;
#endif
    dest->orig_pos = opos; dest->unicodeenc = uenc;
    dest->dependents = scl;
    dest->namechanged = true;

    src->name = NULL;
    src->unicodeenc = -1;
    src->hstem = NULL;
    src->vstem = NULL;
    src->dstem = NULL;
    src->md = NULL;
    src->kerns = NULL;
    src->vkerns = NULL;
    src->anchor = NULL;
    src->possub = NULL;
    src->ttf_instrs = NULL;
    src->ttf_instrs_len = 0;
    SplineCharFree(src);

    /* Fix up dependant info */
    for ( layer=ly_fore; layer<dest->layer_cnt; ++layer )
	for ( refs = dest->layers[layer].refs; refs!=NULL; refs=refs->next ) {
	    for ( scl=refs->sc->dependents; scl!=NULL; scl=scl->next )
		if ( scl->sc==src )
		    scl->sc = dest;
	}
    SCCharChangedUpdate(dest);
}

void FVApplySubstitution(FontView *fv,uint32 script, uint32 lang, uint32 tag) {
    SplineFont *sf = fv->sf, *sf_sl=sf;
    SplineChar *sc, *replacement;
    PST *pst;
    EncMap *map = fv->map;
    int i, gid;
    SplineChar **replacements;
    uint8 *removes;
    char namebuf[40];

    if ( sf_sl->cidmaster!=NULL ) sf_sl = sf_sl->cidmaster;
    else if ( sf_sl->mm!=NULL ) sf_sl = sf_sl->mm->normal;

    /* I used to do replaces and removes as I went along, and then Werner */
    /*  gave me a font were "a" was replaced by "b" replaced by "a" */
    replacements = gcalloc(sf->glyphcnt,sizeof(SplineChar *));
    removes = gcalloc(sf->glyphcnt,sizeof(uint8));

    for ( i=0; i<map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid=map->map[i])!=-1 && (sc=sf->glyphs[gid])!=NULL ) {
	for ( pst = sc->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_substitution && pst->tag==tag &&
		    ScriptLangMatch(sf_sl->script_lang[pst->script_lang_index],script,lang))
	break;
	}
	if ( pst!=NULL ) {
	    replacement = SFGetChar(sf,-1,pst->u.subs.variant);
	    if ( replacement!=NULL ) {
		replacements[gid] = SplineCharCopy( replacement,sf);
		removes[replacement->orig_pos] = true;
	    }
	}
    }

    for ( gid=0; gid<sf->glyphcnt; ++gid ) if ( (sc = sf->glyphs[gid])!=NULL ) {
	if ( replacements[gid]!=NULL ) {
	    SCReplaceWith(sc,replacements[gid]);
	} else if ( removes[gid] ) {
	    /* This is deliberatly in the else. We don't want to remove a glyph*/
	    /*  we are about to replace */
	    SCPreserveState(sc,2);
	    SCFreeMostContents(sc);
	    sprintf(namebuf,"NameMe-%d", sc->orig_pos);
	    sc->name = copy(namebuf);
	    sc->namechanged = true;
	    sc->unicodeenc = -1;
	    SCCharChangedUpdate(sc);
	    /* Retain the undoes/redoes */
	    /* Retain the dependents */
	}
    }

    free(removes);
    free(replacements);
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

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuMergeFonts(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_MergeFonts(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVMergeFonts(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuInterpFonts(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Interpolate(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVInterpolateFonts(fv);
}

static void FVShowInfo(FontView *fv);
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

void FVChangeChar(FontView *fv,int i) {

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    if ( i!=-1 ) {
	FVDeselectAll(fv);
	fv->selected[i] = true;
	fv->sel_index = 1;
	fv->end_pos = fv->pressed_pos = i;
	FVToggleCharSelected(fv,i);
	FVScrollToChar(fv,i);
	FVShowInfo(fv);
    }
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuChangeChar(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
#  define _CC_ISNext (mi->mid==MID_Next)
#  define _CC_ISPrev (mi->mid==MID_Prev)
#  define _CC_ISNextDef (mi->mid==MID_NextDef)
#  define _CC_ISPrevDef (mi->mid==MID_PrevDef)
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ChangeChar(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);
#  define _CC_ISNext (strcmp(name,"next_char1")==0)
#  define _CC_ISPrev (strcmp(name,"prev_char1")==0)
#  define _CC_ISNextDef (strcmp(name,"next_defined_char1")==0)
#  define _CC_ISPrevDef (strcmp(name,"prev_defined_char1")==0)
# endif
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    int pos = FVAnyCharSelected(fv);
    if ( pos>=0 ) {
	if ( _CC_ISNext )
	    ++pos;
	else if ( _CC_ISPrev )
	    --pos;
	else if ( _CC_ISNextDef ) {
	    for ( ++pos; pos<map->enccount &&
		    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
		    ++pos );
	    if ( pos>=map->enccount ) {
		int selpos = FVAnyCharSelected(fv);
		char *iconv_name = map->enc->iconv_name ? map->enc->iconv_name :
			map->enc->enc_name;
		if ( strstr(iconv_name,"2022")!=NULL && selpos<0x2121 )
		    pos = 0x2121;
		else if ( strstr(iconv_name,"EUC")!=NULL && selpos<0xa1a1 )
		    pos = 0xa1a1;
		else if ( map->enc->is_tradchinese ) {
		    if ( strstrmatch(map->enc->enc_name,"HK")!=NULL &&
			    selpos<0x8140 )
			pos = 0x8140;
		    else
			pos = 0xa140;
		} else if ( map->enc->is_japanese ) {
		    if ( strstrmatch(iconv_name,"SJIS")!=NULL ||
			    (strstrmatch(iconv_name,"JIS")!=NULL && strstrmatch(iconv_name,"SHIFT")!=NULL )) {
			if ( selpos<0x8100 )
			    pos = 0x8100;
			else if ( selpos<0xb000 )
			    pos = 0xb000;
		    }
		} else if ( map->enc->is_korean ) {
		    if ( strstrmatch(iconv_name,"JOHAB")!=NULL ) {
			if ( selpos<0x8431 )
			    pos = 0x8431;
		    } else {	/* Wansung, EUC-KR */
			if ( selpos<0xa1a1 )
			    pos = 0xa1a1;
		    }
		} else if ( map->enc->is_simplechinese ) {
		    if ( strmatch(iconv_name,"EUC-CN")==0 && selpos<0xa1a1 )
			pos = 0xa1a1;
		}
		if ( pos>=map->enccount )
return;
	    }
	} else if ( _CC_ISPrevDef ) {
	    for ( --pos; pos>=0 &&
		    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
		    --pos );
	    if ( pos<0 )
return;
	}
    }
    if ( pos<0 ) pos = map->enccount-1;
    else if ( pos>= map->enccount ) pos = 0;
    if ( pos>=0 && pos<map->enccount )
	FVChangeChar(fv,pos);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuGotoChar(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Goto(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int pos = GotoChar(fv->sf,fv->map);
    FVChangeChar(fv,pos);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuLigatures(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Ligatures(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFShowLigatures(fv->sf,NULL);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuKernPairs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_KernPairs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFKernPrepare(fv->sf,false);
    SFShowKernPairs(fv->sf,NULL,NULL);
    SFKernCleanup(fv->sf,false);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAnchorPairs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AnchorPairs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SFShowKernPairs(fv->sf,NULL,mi->ti.userdata);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuShowAtt(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowAtt(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    ShowAtt(fv->sf);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuDisplaySubs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_DisplaySubstitutions(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    if ( fv->cur_feat_tag!=0 ) {
	fv->cur_feat_tag = 0;
	fv->cur_sli = 0;
    } else {
	unichar_t *newname, *components=NULL;
	int macfeature;
	uint16 flags, sli;
	uint32 tag;
	newname = AskNameTag(_STR_DisplaySubstitutions,NULL,0,0,
		-1, pst_substitution,fv->sf,NULL,-2,-1);
	if ( newname==NULL )
return;
	DecomposeClassName(newname,&components,&tag,&macfeature,
		&flags, &sli, NULL,NULL,fv->sf);
	fv->cur_feat_tag = tag;
	fv->cur_sli = sli;
    }
    GDrawRequestExpose(fv->v,NULL,false);
}

static void FVChangeDisplayFont(FontView *fv,BDFFont *bdf) {
    int samesize=0;
    int rcnt, ccnt;
    int oldr, oldc;
    int first_time = fv->show==NULL;

    if ( fv->v==NULL )			/* Can happen in scripts */
return;

    if ( fv->show!=bdf ) {
	if ( !first_time && fv->cbw == bdf->pixelsize+1 )
	    samesize = true;
	oldc = fv->cbw*fv->colcnt;
	oldr = fv->cbh*fv->rowcnt;

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
	fv->cbh = (bdf->pixelsize*fv->magnify)+1+fv->lab_height+1;
	fv->resize_expected = !samesize;
	ccnt = fv->sf->desired_col_cnt;
	rcnt = fv->sf->desired_row_cnt;
	if ((( bdf->pixelsize<=fv->sf->display_size || bdf->pixelsize<=-fv->sf->display_size ) &&
		 fv->sf->top_enc!=-1 /* Not defaulting */ ) ||
		bdf->pixelsize<=48 ) {
	    /* use the desired sizes */
	} else {
	    if ( bdf->pixelsize>48 ) {
		ccnt = 8;
		rcnt = 2;
	    } else if ( bdf->pixelsize>=96 ) {
		ccnt = 4;
		rcnt = 1;
	    }
	    if ( !first_time ) {
		if ( ccnt < oldc/fv->cbw )
		    ccnt = oldc/fv->cbw;
		if ( rcnt < oldr/fv->cbh )
		    rcnt = oldr/fv->cbh;
	    }
	}
#if defined(FONTFORGE_CONFIG_GTK)
	if ( samesize ) {
	    gtk_widget_queue_draw(fv->v);
	} else {
	    gtk_widget_set_size_request(fv->v,ccnt*fv->cbw+1,rcnt*fv->cbh+1);
	    /* If we try to make the window smaller, we must force a resize */
	    /*  of the whole system, else the size will remain the same */
	    /* Also when we get a resize event we must set the widget size */
	    /*  to be minimal so that the user can resize */
	    if (( ccnt*fv->cbw < oldc || rcnt*fv->cbh < oldr ) && !first_time )
		gtk_window_resize( GTK_WINDOW(widget), 1,1 );
	}
#elif defined(FONTFORGE_CONFIG_GDRAW)
	if ( samesize ) {
	    GDrawRequestExpose(fv->v,NULL,false);
	} else {
	    GDrawResize(fv->gw,
		    ccnt*fv->cbw+1+GDrawPointsToPixels(fv->gw,_GScrollBar_Width),
		    rcnt*fv->cbh+1+fv->mbh+fv->infoh);
	}
#endif
    }
}

# if defined(FONTFORGE_CONFIG_GDRAW)
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
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowMetrics(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dlg = create_FontViewShowMetrics();
    int isv = strcmp(gtk_widget_get_name(menuitem),"show_v_metrics1")==0;
    int metrics = !isv ? fv->showhmetrics : fv->showvmetrics;

    if ( isv )
	gtk_window_set_title(GTK_WINDOW(dlg), _("Show Vertical Metrics"));
    gtk_toggle_button_set_active(lookup_widget(dlg,"baseline"),
	    (metrics&fvm_baseline)?true:false);
    gtk_toggle_button_set_active(lookup_widget(dlg,"origin"),
	    (metrics&fvm_origin)?true:false);
    gtk_toggle_button_set_active(lookup_widget(dlg,"AWBar"),
	    (metrics&fvm_advanceto)?true:false);
    gtk_toggle_button_set_active(lookup_widget(dlg,"AWLine"),
	    (metrics&fvm_advanceat)?true:false);
    if ( gtk_dialog_run( GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT ) {
	metrics = 0;
	if ( gtk_toggle_button_get_active(lookup_widget(dlg,"baseline")) )
	    metrics |= fvm_baseline;
	if ( gtk_toggle_button_get_active(lookup_widget(dlg,"origin")) )
	    metrics |= fvm_origin;
	if ( gtk_toggle_button_get_active(lookup_widget(dlg,"AWBar")) )
	    metrics |= fvm_advanceto;
	if ( gtk_toggle_button_get_active(lookup_widget(dlg,"AWLine")) )
	    metrics |= fvm_advanceat;
	if ( isv )
	    fv->showvmetrics = metrics;
	else
	    fv->showhmetrics = metrics;
    }
    gtk_widget_destroy( dlg );
}
#endif

void FVChangeDisplayBitmap(FontView *fv,BDFFont *bdf) {
    FVChangeDisplayFont(fv,bdf);
    fv->sf->display_size = fv->show->pixelsize;
}

# if defined(FONTFORGE_CONFIG_GDRAW)
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
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_PixelSize(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int dspsize = fv->filled->pixelsize;
    int changedmodifier = false;
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);

    fv->magnify = 1;
    if ( strstr(name,"24")!=NULL )
	default_fv_font_size = dspsize = 24;
    else if ( strstr(name,"36")!=NULL )
	default_fv_font_size = dspsize = 36;
    else if ( strstr(name,"48")!=NULL )
	default_fv_font_size = dspsize = 48;
    else if ( strstr(name,"72")!=NULL )
	default_fv_font_size = dspsize = 72;
    else if ( strstr(name,"96")!=NULL )
	default_fv_font_size = dspsize = 96;
    else if ( strcmp(name,"fit_to_em1")==0 ) {
# endif
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

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuGlyphLabel(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    default_fv_glyphlabel = fv->glyphlabel = mi->mid;

    GDrawRequestExpose(fv->v,NULL,false);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_GlyphLabel(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);

    fv->magnify = 1;
    if ( strstr(name,"glyph_image")!=NULL )
	default_fv_glyphlabel = fv->glyphlabel = gl_glyph;
    else if ( strstr(name,"glyph_name")!=NULL )
	default_fv_glyphlabel = fv->glyphlabel = gl_name;
    else if ( strstr(name,"unicode")!=NULL )
	default_fv_glyphlabel = fv->glyphlabel = gl_unicode;
    else if ( strstr(name,"encoding")!=NULL )
	default_fv_glyphlabel = fv->glyphlabel = gl_encoding;

    gtk_widget_queue_draw(fv->v);
# endif

    SavePrefs();
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuShowBitmap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    BDFFont *bdf = mi->ti.userdata;
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ShowBitmap(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    BDFFont *bdf = user_data;
# endif

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
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */

void FVMetricsCenter(FontView *fv,int docenter) {
    int i, gid;
    SplineChar *sc;
    DBounds bb;
    IBounds ib;
    real transform[6];
    BVTFunc bvts[2];
    BDFFont *bdf;

    transform[0] = transform[3] = 1.0;
    transform[1] = transform[2] = transform[5] = 0.0;
    bvts[1].func = bvt_none;
    bvts[0].func = bvt_transmove; bvts[0].y = 0;
    if ( !fv->sf->onlybitmaps ) {
	for ( i=0; i<fv->map->enccount; ++i ) {
	    if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 &&
		    (sc = fv->sf->glyphs[gid])!=NULL ) {
		SplineCharFindBounds(sc,&bb);
		if ( docenter )
		    transform[4] = (sc->width-(bb.maxx-bb.minx))/2 - bb.minx;
		else
		    transform[4] = (sc->width-(bb.maxx-bb.minx))/3 - bb.minx;
		if ( transform[4]!=0 ) {
		    FVTrans(fv,sc,transform,NULL,fvt_dontmovewidth);
		    bvts[0].x = transform[4];
		    for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
			if ( gid<bdf->glyphcnt && bdf->glyphs[gid]!=NULL )
			    BCTrans(bdf,bdf->glyphs[gid],bvts,fv);
		}
	    }
	}
    } else {
	double scale = (fv->sf->ascent+fv->sf->descent)/(double) (fv->show->pixelsize);
	for ( i=0; i<fv->map->enccount; ++i ) {
	    if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 &&
		    fv->sf->glyphs[gid]!=NULL ) {
		BDFChar *bc = fv->show->glyphs[gid];
		if ( bc==NULL ) bc = BDFMakeChar(fv->show,fv->map,i);
		BDFCharFindBounds(bc,&ib);
		if ( docenter )
		    transform[4] = scale * ((bc->width-(ib.maxx-ib.minx))/2 - ib.minx);
		else
		    transform[4] = scale * ((bc->width-(ib.maxx-ib.minx))/3 - ib.minx);
		if ( transform[4]!=0 ) {
		    FVTrans(fv,fv->sf->glyphs[gid],transform,NULL,fvt_dontmovewidth);
		    bvts[0].x = transform[4];
		    for ( bdf = fv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
			if ( gid<bdf->glyphcnt && bdf->glyphs[gid]!=NULL )
			    BCTrans(bdf,bdf->glyphs[gid],bvts,fv);
		}
	    }
	}
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCenter(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Center(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVMetricsCenter(fv,mi->mid==MID_Center);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuSetWidth(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( mi->mid == MID_SetVWidth && !fv->sf->hasvmetrics )
return;
    FVSetWidth(fv,mi->mid==MID_SetWidth   ?wt_width:
		  mi->mid==MID_SetLBearing?wt_lbearing:
		  mi->mid==MID_SetRBearing?wt_rbearing:
		  wt_vwidth);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Setwidth(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);

    if ( FVAnyCharSelected(fv)==-1 )
return;
    if ( strcmp(name,"set_vertical_advance1")==0 && !fv->sf->hasvmetrics )
return;
    FVSetWidth(fv,strcmp(name,"set_width1")==0   ?wt_width:
		  strcmp(name,"set_lbearing1")==0?wt_lbearing:
		  strcmp(name,"set_rbearing1")==0?wt_rbearing:
		  wt_vwidth);
# endif
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAutoWidth(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoWidth(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVAutoWidth(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuAutoKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoKern(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVAutoKern(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuKernByClasses(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_KernClasses(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    ShowKernClasses(fv->sf,NULL,false);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuVKernByClasses(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_VKernClasses(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    ShowKernClasses(fv->sf,NULL,true);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRemoveKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveKP(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVRemoveKerns(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuRemoveVKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveVKP(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVRemoveVKerns(fv);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuKPCloseup(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_KernPairCloseup(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] )
    break;
    KernPairD(fv->sf,i==fv->map->enccount?NULL:
		    fv->map->map[i]==-1?NULL:
		    fv->sf->glyphs[fv->map->map[i]],NULL,false);
}

# if defined(FONTFORGE_CONFIG_GDRAW)
static void FVMenuVKernFromHKern(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_VKernFromH(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

    FVVKernFromHKern(fv);
}
#endif

static void FVAutoHint(FontView *fv) {
    int i, cnt=0, gid;
    BlueData *bd = NULL, _bd;

    if ( fv->sf->mm==NULL ) {
	QuickBlues(fv->sf,&_bd);
	bd = &_bd;
    }

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_AutoHintingFont,_STR_AutoHintingFont,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Auto Hinting Font..."),_("Auto Hinting Font..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	sc->manualhints = false;
	/* Hint undoes are done in _SplineCharAutoHint */
	SplineCharAutoHint(sc,bd);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
    GDrawRequestExpose(fv->v,NULL,false);	/* Clear any changedsincelasthinted marks */
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuAutoHint(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoHint(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVAutoHint( fv );
}
#endif

static void FVAutoHintSubs(FontView *fv) {
    int i, cnt=0, gid;

    if ( fv->sf->mm!=NULL && fv->sf->mm->apple )
return;
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_FindingSubstitutionPts,_STR_FindingSubstitutionPts,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Finding Substitution Points..."),_("Finding Substitution Points..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	SCFigureHintMasks(sc);
	SCUpdateAll(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuAutoHintSubs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoHintSubs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVAutoHintSubs( fv );
}
#endif

static void FVAutoCounter(FontView *fv) {
    int i, cnt=0, gid;

    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_FindingCounterMasks,_STR_FindingCounterMasks,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Finding Counter Masks..."),_("Finding Counter Masks..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	SCFigureCounterMasks(sc);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
    break;
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuAutoCounter(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoCounter(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVAutoCounter( fv );
}
#endif

static void FVDontAutoHint(FontView *fv) {
    int i, gid;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	sc->manualhints = true;
    }
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuDontAutoHint(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_DontAutoHint(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVDontAutoHint( fv );
}
#endif

static void FVAutoInstr(FontView *fv) {
    BlueData bd;
    int i, cnt=0, gid;

    QuickBlues(fv->sf,&bd);
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		SCWorthOutputting(fv->sf->glyphs[gid]) )
	    ++cnt;
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressStartIndicatorR(10,_STR_AutoInstructingFont,_STR_AutoInstructingFont,0,cnt,1);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_start_indicator(10,_("Auto Instructing Font..."),_("Auto Instructing Font..."),0,cnt,1);
# endif

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	SCAutoInstr(sc,&bd);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
	if ( !GProgressNext())
# elif defined(FONTFORGE_CONFIG_GTK)
	if ( !gwwv_progress_next())
# endif
#endif		/* FONTFORGE_CONFIG_NO_WINDOWING_UI */
    break;
    }
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif
}

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuAutoInstr(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_AutoInstr(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVAutoInstr( fv );
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuEditInstrs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_EditInstrs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int index = FVAnyCharSelected(fv);
    SplineChar *sc;
    if ( index<0 )
return;
    sc = SFMakeChar(fv->sf,fv->map,index);
    SCEditInstructions(sc);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuEditTable(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFEditTable(fv->sf,
	    mi->mid==MID_Editprep?CHR('p','r','e','p'):
	    mi->mid==MID_Editfpgm?CHR('f','p','g','m'):
				  CHR('c','v','t',' '));
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_EditTable(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);
    SFEditTable(fv->sf,
	    strcmp(name,"edit_prep1")==0?CHR('p','r','e','p'):
	    strcmp(name,"edit_fpgm1")==0?CHR('f','p','g','m'):
				  CHR('c','v','t',' '));
}
# endif

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuClearInstrs(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ClearInstrs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineChar *sc;
    int i, gid;

    if ( !SFCloseAllInstrs(fv->sf))
return;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting((sc = fv->sf->glyphs[gid])) ) {
	if ( sc->ttf_instrs_len!=0 ) {
	    free(sc->ttf_instrs);
	    sc->ttf_instrs = NULL;
	    sc->ttf_instrs_len = 0;
	    SCCharChangedUpdate(sc);
	}
    }
}
#endif

static void FVClearHints(FontView *fv) {
    int i, gid;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
	sc->manualhints = true;
	SCPreserveHints(sc);
	SCClearHintMasks(sc,true);
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

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuClearHints(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ClearHints(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    FVClearHints(fv);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuClearWidthMD(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ClearWidthMD(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    int i, changed, gid;
    MinimumDistance *md, *prev, *next;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] &&
	    (gid = fv->map->map[i])!=-1 && SCWorthOutputting(fv->sf->glyphs[gid]) ) {
	SplineChar *sc = fv->sf->glyphs[gid];
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

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuHistograms(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SFHistogram(fv->sf, NULL, FVAnyCharSelected(fv)!=-1?fv->selected:NULL,
			mi->mid==MID_HStemHist ? hist_hstem :
			mi->mid==MID_VStemHist ? hist_vstem :
				hist_blues);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Histograms(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    G_CONST_RETURN gchar *name = gtk_widget_get_name(menuitem);
    SFHistogram(fv->sf, NULL, FVAnyCharSelected(fv)!=-1?fv->selected:NULL,
			strcmp(name,"hstem1")==0 ? hist_hstem :
			strcmp(name,"vstem1")==0 ? hist_vstem :
				hist_blues);
# endif

void FVSetTitle(FontView *fv) {
# ifdef FONTFORGE_CONFIG_GDRAW
    unichar_t *title, *ititle, *temp;
    char *file=NULL;
    char *enc;
    int len;

    if ( fv->gw==NULL )		/* In scripting */
return;

    enc = SFEncodingName(fv->sf,fv->map);
    len = strlen(fv->sf->fontname)+1 + strlen(enc)+4;
    if ( fv->cidmaster!=NULL ) {
	if ( (file = fv->cidmaster->filename)==NULL )
	    file = fv->cidmaster->origname;
    } else {
	if ( (file = fv->sf->filename)==NULL )
	    file = fv->sf->origname;
    }
    if ( file!=NULL )
	len += 2+strlen(file);
    title = galloc((len+1)*sizeof(unichar_t));
    uc_strcpy(title,fv->sf->fontname);
    if ( fv->sf->changed )
	uc_strcat(title,"*");
    if ( file!=NULL ) {
	uc_strcat(title,"  ");
	temp = def2u_copy(GFileNameTail(file));
	u_strcat(title,temp);
	free(temp);
    }
    uc_strcat(title, " (" );
    uc_strcat(title,enc);
    uc_strcat(title, ")" );
    free(enc);

    ititle = uc_copy(fv->sf->fontname);
    GDrawSetWindowTitles(fv->gw,title,ititle);
    free(title);
    free(ititle);
# elif defined(FONTFORGE_CONFIG_GTK)
    unichar_t *title, *ititle, *temp;
    char *file=NULL;
    char *enc;
    int len;

    if ( fv->gw==NULL )		/* In scripting */
return;

    enc = SFEncodingName(fv->sf,fv->map);
    len = strlen(fv->sf->fontname)+1 + strlen(enc)+4;
    if ( fv->cidmaster!=NULL ) {
	if ( (file = fv->cidmaster->filename)==NULL )
	    file = fv->cidmaster->origname;
    } else {
	if ( (file = fv->sf->filename)==NULL )
	    file = fv->sf->origname;
    }
    if ( file!=NULL )
	len += 2+strlen(file);
    title = galloc((len+1));
    strcpy(title,fv->sf->fontname);
    if ( fv->sf->changed )
	strcat(title,"*");
    if ( file!=NULL ) {
	strcat(title,"  ");
	strcat(title,GFileNameTail(file));
    }
    strcat(title, " (" );
    strcat(title,enc);
    strcat(title, ")" );
    free(enc);

    gdk_window_set_icon_name( GTK_WIDGET(fv->gw)->window, fv->sf->fontname );
    gtk_window_set_title( GTK_WINDOW(fv->gw), title );
    free(title);
# endif
}

static void CIDSetEncMap(FontView *fv, SplineFont *new ) {
    int gcnt = new->glyphcnt;

    if ( gcnt!=fv->sf->glyphcnt ) {
	int i;
	if ( fv->map->encmax<gcnt ) {
	    fv->map->map = grealloc(fv->map->map,gcnt*sizeof(int));
	    fv->map->backmap = grealloc(fv->map->backmap,gcnt*sizeof(int));
	    fv->map->backmax = fv->map->encmax = gcnt;
	}
	for ( i=0; i<gcnt; ++i )
	    fv->map->map[i] = fv->map->backmap[i] = i;
	if ( gcnt<fv->map->enccount )
	    memset(fv->selected+gcnt,0,fv->map->enccount-gcnt);
	else {
	    free(fv->selected);
	    fv->selected = gcalloc(gcnt,sizeof(char));
	}
	fv->map->enccount = gcnt;
    }
    fv->sf = new;
    new->fv = fv;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    FVSetTitle(fv);
    FontViewReformatOne(fv);
#endif
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuShowSubFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
static void FontViewMenu_ShowSubFont(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *new = mi->ti.userdata;
    MetricsView *mv, *mvnext;

    for ( mv=fv->metrics; mv!=NULL; mv = mvnext ) {
	/* Don't bother trying to fix up metrics views, just not worth it */
	mvnext = mv->next;
	GDrawDestroyWindow(mv->gw);
    }
    GDrawSync(NULL);
    GDrawProcessPendingEvents(NULL);
    CIDSetEncMap(fv,new);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuConvert2CID(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Convert2CID(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster!=NULL )
return;
    MakeCIDMaster(fv->sf,fv->map,false,NULL,NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuConvertByCMap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ConvertByCMap(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster!=NULL )
return;
    MakeCIDMaster(fv->sf,fv->map,true,NULL,NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuFlatten(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Flatten(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster==NULL )
return;
    SFFlatten(cidmaster);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuFlattenByCMap(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_FlattenByCMap(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster==NULL )
return;
    SFFlattenByCMap(cidmaster,NULL);
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static void FVInsertInCID(FontView *fv,SplineFont *sf) {
    SplineFont *cidmaster = fv->cidmaster;
    SplineFont **subs;
    int i;

    subs = galloc((cidmaster->subfontcnt+1)*sizeof(SplineFont *));
    for ( i=0; i<cidmaster->subfontcnt && cidmaster->subfonts[i]!=fv->sf; ++i )
	subs[i] = cidmaster->subfonts[i];
    subs[i] = sf;
    if ( sf->uni_interp == ui_none || sf->uni_interp == ui_unset )
	sf->uni_interp = cidmaster->uni_interp;
    for ( ; i<cidmaster->subfontcnt ; ++i )
	subs[i+1] = cidmaster->subfonts[i];
    ++cidmaster->subfontcnt;
    free(cidmaster->subfonts);
    cidmaster->subfonts = subs;
    cidmaster->changed = true;
    sf->cidmaster = cidmaster;

    CIDSetEncMap(fv,sf);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuInsertFont(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_InsertFont(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
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
#if defined(FONTFORGE_CONFIG_GDRAW)
	GWidgetErrorR(_STR_CloseFont,_STR_CloseFontForCID,new->origname);
#elif defined(FONTFORGE_CONFIG_GTK)
	gwwv_post_error(_("Please close font"),_("Please close %s before inserting it into a CID font"),new->origname);
#endif
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

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuInsertBlank(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_InsertBlank(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster, *sf;
    struct cidmap *map;

    if ( cidmaster==NULL || cidmaster->subfontcnt>=255 )	/* Open type allows 1 byte to specify the fdselect */
return;
    map = FindCidMap(cidmaster->cidregistry,cidmaster->ordering,cidmaster->supplement,cidmaster);
    sf = SplineFontBlank(MaxCID(map));
    sf->glyphcnt = sf->glyphmax;
    sf->cidmaster = cidmaster;
    sf->display_antialias = fv->sf->display_antialias;
    sf->display_bbsized = fv->sf->display_bbsized;
    sf->display_size = fv->sf->display_size;
    sf->private = gcalloc(1,sizeof(struct psdict));
    PSDictChangeEntry(sf->private,"lenIV","1");		/* It's 4 by default, in CIDs the convention seems to be 1 */
    FVInsertInCID(fv,sf);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuRemoveFontFromCID(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    static int buts[] = { _STR_Remove, _STR_Cancel, 0 };
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_RemoveFontFromCID(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    static char *buts[] = { GTK_STOCK_REMOVE, GTK_STOCK_CANCEL, NULL };
# endif
    SplineFont *cidmaster = fv->cidmaster, *sf = fv->sf, *replace;
    int i;
    MetricsView *mv, *mnext;
    FontView *fvs;

    if ( cidmaster==NULL || cidmaster->subfontcnt<=1 )	/* Can't remove last font */
return;
#if defined(FONTFORGE_CONFIG_GDRAW)
    if ( GWidgetAskR(_STR_RemoveFont,buts,0,1,_STR_CIDRemoveFontCheck,
#elif defined(FONTFORGE_CONFIG_GTK)
    if ( gwwv_ask(_("Remove Font"),buts,0,1,_("Are you sure you wish to remove sub-font %1$.40s from the CID font %2$.40s"),
#endif
	    sf->fontname,cidmaster->fontname)==1 )
return;

    for ( i=0; i<sf->glyphcnt; ++i ) if ( sf->glyphs[i]!=NULL ) {
	CharView *cv, *next;
	for ( cv = sf->glyphs[i]->views; cv!=NULL; cv = next ) {
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
	if ( fvs->sf==sf )
	    FVInsertInCID(fvs,replace);
    }
    FontViewReformatAll(fv->sf);
    SplineFontFree(sf);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCIDFontInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CIDFontInfo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;

    if ( cidmaster==NULL )
return;
    FontInfo(cidmaster,-1,false);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuChangeSupplement(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ChangeSupplement(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    SplineFont *cidmaster = fv->cidmaster;
    struct cidmap *cidmap;
    char buffer[20];
    unichar_t ubuffer[20];
    unichar_t *ret, *end;
    int supple;

    if ( cidmaster==NULL )
return;
    sprintf(buffer,"%d",cidmaster->supplement);
    uc_strcpy(ubuffer,buffer);
    ret = GWidgetAskStringR(_STR_ChangeSupplement,ubuffer,_STR_ChangeSupplementOfFont,
	    cidmaster->cidregistry,cidmaster->ordering);
    if ( ret==NULL )
return;
    supple = u_strtol(ret,&end,10);
    if ( *end!='\0' || supple<=0 ) {
	free(ret);
	GWidgetErrorR( _STR_BadNumber,_STR_BadNumber );
return;
    }
    free(ret);
    if ( supple!=cidmaster->supplement ) {
	    /* this will make noises if it can't find an appropriate cidmap */
	cidmap = FindCidMap(cidmaster->cidregistry,cidmaster->ordering,supple,cidmaster);
	cidmaster->supplement = supple;
	FVSetTitle(fv);
    }
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuReencode(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    Encoding *enc = NULL;
    EncMap *map;

    enc = FindOrMakeEncoding(mi->ti.userdata);
    if ( enc==&custom )
	fv->map->enc = &custom;
    else {
	map = EncMapFromEncoding(fv->sf,enc);
	fv->selected = grealloc(fv->selected,map->enccount);
	memset(fv->selected,0,map->enccount);
	EncMapFree(fv->map);
	fv->map = map;
    }
    FVSetTitle(fv);
    FontViewReformatOne(fv);
}

static void FVMenuForceEncode(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    Encoding *enc = NULL;
    int oldcnt = fv->map->enccount;

    enc = FindOrMakeEncoding(mi->ti.userdata);
    SFForceEncoding(fv->sf,fv->map,enc);
    if ( oldcnt < fv->map->enccount ) {
	fv->selected = grealloc(fv->selected,fv->map->enccount);
	memset(fv->selected+oldcnt,0,fv->map->enccount-oldcnt);
    }
    FVSetTitle(fv);
    FontViewReformatOne(fv);
}

static void FVMenuDisplayByGroups(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    DisplayGroups(fv);
}

static void FVMenuDefineGroups(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    DefineGroups(fv);
}
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Reencode(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
void FontViewMenu_ForceEncode(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuMMValid(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_MMValid(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MMSet *mm = fv->sf->mm;

    if ( mm==NULL )
return;
    MMValid(mm,true);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuCreateMM(GWindow gw,struct gmenuitem *mi,GEvent *e) {
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_CreateMM(GtkMenuItem *menuitem, gpointer user_data) {
# endif
    MMWizard(NULL);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuMMInfo(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_MMInfo(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MMSet *mm = fv->sf->mm;

    if ( mm==NULL )
return;
    MMWizard(mm);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuChangeMMBlend(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ChangeDefWeights(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MMSet *mm = fv->sf->mm;

    if ( mm==NULL || mm->apple )
return;
    MMChangeBlend(mm,fv,false);
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuBlendToNew(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_Blend(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
# endif
    MMSet *mm = fv->sf->mm;

    if ( mm==NULL )
return;
    MMChangeBlend(mm,fv,true);
}
#endif

# ifdef FONTFORGE_CONFIG_GDRAW
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

static void htlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);
    int multilayer = fv->sf->multilayer;

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_AutoHint:
	    mi->ti.disabled = anychars==-1 || multilayer;
	  break;
	  case MID_HintSubsPt:
	    mi->ti.disabled = fv->sf->order2 || anychars==-1 || multilayer;
	    if ( fv->sf->mm!=NULL && fv->sf->mm->apple )
		mi->ti.disabled = true;
	  break;
	  case MID_AutoCounter: case MID_DontAutoHint:
	    mi->ti.disabled = fv->sf->order2 || anychars==-1 || multilayer;
	  break;
	  case MID_AutoInstr: case MID_EditInstructions:
	    mi->ti.disabled = !fv->sf->order2 || anychars==-1 || multilayer;
	  break;
	  case MID_Editfpgm: case MID_Editprep: case MID_Editcvt:
	    mi->ti.disabled = !fv->sf->order2 || multilayer;
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
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_OpenBitmap:
	    mi->ti.disabled = anychars==-1 || fv->sf->bitmaps==NULL;
	  break;
	  case MID_Revert:
	    mi->ti.disabled = fv->sf->origname==NULL;
	  break;
	  case MID_RevertGlyph:
	    mi->ti.disabled = fv->sf->origname==NULL || anychars==-1;
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
	    mi->ti.disabled = (fv->sf->onlybitmaps && fv->sf->bitmaps==NULL) ||
			fv->sf->multilayer;
	  break;
	}
    }
}

static void edlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int pos = FVAnyCharSelected(fv), i, gid;
    int not_pasteable = pos==-1 ||
		    (!CopyContainsSomething() &&
#ifndef _NO_LIBPNG
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/png") &&
#endif
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/bmp") &&
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/eps"));
    RefChar *base = CopyContainsRef(fv->sf);
    int base_enc = base!=NULL ? fv->map->backmap[base->orig_pos] : -1;


    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Paste: case MID_PasteInto:
	    mi->ti.disabled = not_pasteable;
	  break;
#ifdef FONTFORGE_CONFIG_PASTEAFTER
	  case MID_PasteAfter:
	    mi->ti.disabled = not_pasteable || pos<0;
	  break;
#endif
	  case MID_SameGlyphAs:
	    mi->ti.disabled = not_pasteable || base==NULL || fv->cidmaster!=NULL ||
		    base_enc==-1 ||
		    fv->selected[base_enc];	/* Can't be self-referential */
	  break;
	  case MID_CopyFeatures:
	    mi->ti.disabled = pos<0 || fv->map->map[pos]==-1 ||
		    !SCAnyFeatures(fv->sf->glyphs[fv->map->map[pos]]);
	  break;
	  case MID_Join:
	  case MID_Cut: case MID_Copy: case MID_Clear:
	  case MID_CopyWidth: case MID_CopyLBearing: case MID_CopyRBearing:
	  case MID_CopyRef: case MID_UnlinkRef:
	  case MID_RemoveUndoes: case MID_CopyFgToBg:
	  case MID_RplRef:
	    mi->ti.disabled = pos==-1;
	  break;
	  case MID_CopyVWidth: 
	    mi->ti.disabled = pos==-1 || !fv->sf->hasvmetrics;
	  break;
	  case MID_ClearBackground:
	    mi->ti.disabled = true;
	    if ( pos!=-1 && !( onlycopydisplayed && fv->filled!=fv->show )) {
		for ( i=0; i<fv->map->enccount; ++i )
		    if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
			    fv->sf->glyphs[gid]!=NULL )
			if ( fv->sf->glyphs[gid]->layers[ly_back].images!=NULL ||
				fv->sf->glyphs[gid]->layers[ly_back].splines!=NULL ) {
			    mi->ti.disabled = false;
		break;
			}
	    }
	  break;
	  case MID_Undo:
	    for ( i=0; i<fv->map->enccount; ++i )
		if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
			fv->sf->glyphs[gid]!=NULL )
		    if ( fv->sf->glyphs[gid]->layers[ly_fore].undoes!=NULL )
	    break;
	    mi->ti.disabled = i==fv->map->enccount;
	  break;
	  case MID_Redo:
	    for ( i=0; i<fv->map->enccount; ++i )
		if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
			fv->sf->glyphs[gid]!=NULL )
		    if ( fv->sf->glyphs[gid]->layers[ly_fore].redoes!=NULL )
	    break;
	    mi->ti.disabled = i==fv->map->enccount;
	  break;
	}
    }
}

static void trlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Transform:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_NLTransform: case MID_POV:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
	}
    }
}

static void ellistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv), gid;
    int anybuildable, anytraceable;
    int order2 = fv->sf->order2;

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_CharInfo:
	    mi->ti.disabled = anychars<0 || (gid = fv->map->map[anychars])==-1 ||
		    (fv->cidmaster!=NULL && fv->sf->glyphs[gid]==NULL);
	  break;
	  case MID_FindProblems:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_Transform:
	    mi->ti.disabled = anychars==-1;
	    /* some Transformations make sense on bitmaps now */
	  break;
	  case MID_AddExtrema:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
	  case MID_Simplify:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
	  case MID_MetaFont: case MID_Stroke:
	  case MID_RmOverlap: case MID_Effects:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || order2;
	  break;
	  case MID_Round: case MID_Correct:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps;
	  break;
#ifdef FONTFORGE_CONFIG_TILEPATH
	  case MID_TilePath:
	    mi->ti.disabled = anychars==-1 || fv->sf->onlybitmaps || ClipBoardToSplineSet()==NULL || order2;
	  break;
#endif
	  case MID_AvailBitmaps:
	    mi->ti.disabled = fv->sf->mm!=NULL;
	  break;
	  case MID_RegenBitmaps:
	    mi->ti.disabled = fv->sf->bitmaps==NULL || fv->sf->onlybitmaps ||
		    fv->sf->mm!=NULL;
	  break;
	  case MID_BuildAccent:
	    anybuildable = false;
	    if ( anychars!=-1 ) {
		int i;
		for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
		    SplineChar *sc=NULL, dummy;
		    gid = fv->map->map[i];
		    if ( gid!=-1 )
			sc = fv->sf->glyphs[gid];
		    if ( sc==NULL )
			sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
		    if ( SFIsSomethingBuildable(fv->sf,sc,false) ||
			    SFIsDuplicatable(fv->sf,sc)) {
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
		for ( i=0; i<fv->map->enccount; ++i )
		    if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
			    fv->sf->glyphs[gid]!=NULL &&
			    fv->sf->glyphs[gid]->layers[ly_back].images!=NULL ) {
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
	    int i, gid;
	    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
		SplineChar *sc=NULL, dummy;
		if ( (gid=fv->map->map[i])!=-1 )
		    sc = fv->sf->glyphs[gid];
		if ( sc==NULL )
		    sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
		if ( SFIsSomethingBuildable(fv->sf,sc,onlyaccents)) {
		    anybuildable = true;
	    break;
		}
	    }
	    mi->ti.disabled = !anybuildable;
        } else if ( mi->mid==MID_BuildDuplicates ) {
	    int anybuildable = false;
	    int i, gid;
	    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
		SplineChar *sc=NULL, dummy;
		if ( (gid=fv->map->map[i])!=-1 )
		    sc = fv->sf->glyphs[gid];
		if ( sc==NULL )
		    sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
		if ( SFIsDuplicatable(fv->sf,sc)) {
		    anybuildable = true;
	    break;
		}
	    }
	    mi->ti.disabled = !anybuildable;
	}
    }
}

static void delistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int gid = fv->map->map[gid];

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_ShowDependentRefs:
	    mi->ti.disabled = gid<0 || fv->sf->glyphs[gid]==NULL ||
		    fv->sf->glyphs[gid]->dependents == NULL;
	  break;
	  case MID_ShowDependentSubs:
	    mi->ti.disabled = gid<0 || fv->sf->glyphs[gid]==NULL ||
		    !SCUsedBySubs(fv->sf->glyphs[gid]);
	  break;
	}
    }
}

static void aatlistcheck(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    FontView *ofv;
    int anychars = FVAnyCharSelected(fv);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_DefaultATT:
	    mi->ti.disabled = anychars==-1;
	  break;
	  case MID_CopyFeatureToFont:
	    for ( ofv=fv_list; ofv!=NULL && ofv->sf==fv->sf; ofv = ofv->next );
	    mi->ti.disabled = ofv==NULL;
	  break;
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
    { { (unichar_t *) _STR_GlyphMetadata, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'N' }, '\0', ksm_control, NULL, NULL, FVMenuCopyFrom, MID_CharName },
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
    { { (unichar_t *) _STR_SelectInvert, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, GK_Escape, ksm_control, NULL, NULL, FVMenuInvertSelection },
    { { (unichar_t *) _STR_DeselectAll, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, GK_Escape, 0, NULL, NULL, FVMenuDeselectAll },
    { { (unichar_t *) _STR_SelectColor, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, sclist },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_SelectChangedGlyphs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, NULL,NULL, FVMenuSelectChanged },
    { { (unichar_t *) _STR_SelectHintingNeeded, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, NULL,NULL, FVMenuSelectHintingNeeded },
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
    { { (unichar_t *) _STR_CopyFeatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control, NULL, NULL, FVMenuCopyFeatures, MID_CopyFeatures },
    { { (unichar_t *) _STR_Paste, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, 'V', ksm_control, NULL, NULL, FVMenuPaste, MID_Paste },
    { { (unichar_t *) _STR_PasteInto, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, 'V', ksm_control|ksm_shift, NULL, NULL, FVMenuPasteInto, MID_PasteInto },
#ifdef FONTFORGE_CONFIG_PASTEAFTER
    { { (unichar_t *) _STR_PasteAfter, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, 'V', ksm_control|ksm_meta|ksm_shift, NULL, NULL, FVMenuPasteAfter, MID_PasteAfter },
#endif
    { { (unichar_t *) _STR_SameGlyphAs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'm' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuSameGlyphAs, MID_SameGlyphAs },
    { { (unichar_t *) _STR_Clear, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, 0, 0, NULL, NULL, FVMenuClear, MID_Clear },
    { { (unichar_t *) _STR_ClearBackground, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 0, 0, NULL, NULL, FVMenuClearBackground, MID_ClearBackground },
    { { (unichar_t *) _STR_CopyFgToBg, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 'C', ksm_control|ksm_shift, NULL, NULL, FVMenuCopyFgBg, MID_CopyFgToBg },
    { { (unichar_t *) _STR_Join, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'J' }, 'J', ksm_control|ksm_shift, NULL, NULL, FVMenuJoin, MID_Join },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Select, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, 0, ksm_control, sllist, sllistcheck },
    { { (unichar_t *) _STR_FindReplace, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'i' }, 'F', ksm_control|ksm_meta, NULL, NULL, FVMenuFindRpl },
    { { (unichar_t *) _STR_ReplaceOutlineWithReference, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'i' }, 'F', ksm_control|ksm_meta|ksm_shift, NULL, NULL, FVMenuReplaceWithRef, MID_RplRef },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Unlinkref, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'U' }, 'U', ksm_control, NULL, NULL, FVMenuUnlinkRef, MID_UnlinkRef },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Copyfrom, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', 0, cflist, cflistcheck, NULL, 0 },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_RemoveUndoes, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'e' }, '\0', 0, NULL, NULL, FVMenuRemoveUndoes, MID_RemoveUndoes },
    { NULL }
};

static GMenuItem aat2list[] = {
    { { (unichar_t *) _STR_All, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Ligatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) 0xffffffff, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_StandardLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('l','i','g','a'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_RequiredLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('r','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_HistoricLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('h','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_DiscretionaryLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('d','l','i','g'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_FractionLig, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('f','r','a','c'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_MacUnicodeDecomposition, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) ((27<<16)|1), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_R2LAlt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('r','t','l','a'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_VertRotAlt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('v','r','t','2'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_SmallCaps, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('s','m','c','p'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
    { { (unichar_t *) _STR_Cap2SmallCaps, NULL, COLOR_DEFAULT, COLOR_DEFAULT, (void *) CHR('c','2','s','c'), NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAAT },
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
    { { (unichar_t *) _STR_SimplifyMore, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 'M', ksm_control|ksm_shift|ksm_meta, NULL, NULL, FVMenuSimplifyMore, MID_SimplifyMore },
    { { (unichar_t *) _STR_CleanupGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'n' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCleanup, MID_CleanupGlyph },
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
    { { (unichar_t *) _STR_BuildDuplicates, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuBuildDuplicate, MID_BuildDuplicates },
#ifdef KOREAN
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ShowGrp, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuShowGroup },
#endif
    { NULL }
};

static GMenuItem aatlist[] = {
    { { (unichar_t *) _STR_CopyFeatureToFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCopyFeatureToFont, MID_CopyFeatureToFont },
    { { (unichar_t *) _STR_DefaultATT, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'I', ksm_control|ksm_meta, aat2list, NULL, NULL, MID_DefaultATT },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_RemoveAllFeatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveAllFeatures },
    { { (unichar_t *) _STR_RemoveFeature, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveFeatures },
    { { (unichar_t *) _STR_RemoveUnusedNested, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveUnusedNested },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_RetagFeature, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRetagFeature },
    { NULL }
};

static GMenuItem delist[] = {
    { { (unichar_t *) _STR_ReferencesDDD, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'u' }, 'I', ksm_control|ksm_meta, NULL, NULL, FVMenuShowDependentRefs, MID_ShowDependentRefs },
    { { (unichar_t *) _STR_SubstitutionsDDD, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuShowDependentSubs, MID_ShowDependentSubs },
    { NULL }
};

static GMenuItem trlist[] = {
    { { (unichar_t *) _STR_Transform, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '\\', ksm_control, NULL, NULL, FVMenuTransform, MID_Transform },
    { { (unichar_t *) _STR_PoVProj, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '<', ksm_shift|ksm_control, NULL, NULL, FVMenuPOV, MID_POV },
    { { (unichar_t *) _STR_NonLinearTransform, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, '|', ksm_shift|ksm_control, NULL, NULL, FVMenuNLTransform, MID_NLTransform },
    { NULL }
};

static GMenuItem rndlist[] = {
    { { (unichar_t *) _STR_Round2int, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '_', ksm_control|ksm_shift, NULL, NULL, FVMenuRound2Int, MID_Round },
    { { (unichar_t *) _STR_Round2Hundredths, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRound2Hundredths, 0 },
    { { (unichar_t *) _STR_Cluster, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuCluster },
    { NULL }
};

static GMenuItem ellist[] = {
    { { (unichar_t *) _STR_Fontinfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, 'F', ksm_control|ksm_shift, NULL, NULL, FVMenuFontInfo },
    { { (unichar_t *) _STR_GlyphInfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, 'I', ksm_control, NULL, NULL, FVMenuCharInfo, MID_CharInfo },
    { { (unichar_t *) _STR_TypoFeatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, '\0', ksm_control|ksm_meta, aatlist, aatlistcheck },
    { { (unichar_t *) _STR_ShowDependents, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, 'I', ksm_control|ksm_meta, delist, delistcheck },
    { { (unichar_t *) _STR_Findprobs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, 'E', ksm_control, NULL, NULL, FVMenuFindProblems, MID_FindProblems },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Bitmapsavail, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'A' }, 'B', ksm_control|ksm_shift, NULL, NULL, FVMenuBitmaps, MID_AvailBitmaps },
    { { (unichar_t *) _STR_Regenbitmaps, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 'B', ksm_control, NULL, NULL, FVMenuBitmaps, MID_RegenBitmaps },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Transformations, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'T' }, 0, ksm_control, trlist, trlistcheck, NULL, MID_Transform },
    { { (unichar_t *) _STR_Stroke, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'E' }, 'E', ksm_control|ksm_shift, NULL, NULL, FVMenuStroke, MID_Stroke },
#ifdef FONTFORGE_CONFIG_TILEPATH
    { { (unichar_t *) _STR_TilePath, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuTilePath, MID_TilePath },
#endif
    { { (unichar_t *) _STR_Overlap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'O' }, '\0', ksm_control|ksm_shift, rmlist, NULL, NULL, MID_RmOverlap },
    { { (unichar_t *) _STR_Simplify, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_control|ksm_shift, smlist, NULL, NULL, MID_Simplify },
    { { (unichar_t *) _STR_AddExtrema, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'x' }, 'X', ksm_control|ksm_shift, NULL, NULL, FVMenuAddExtrema, MID_AddExtrema },
    { { (unichar_t *) _STR_Round_Menu, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control|ksm_shift, rndlist, NULL, NULL, MID_Round },
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
    { { (unichar_t *) _STR_AnchoredPairs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, dummyall, aplistbuild, NULL, MID_AnchorPairs },
    { { (unichar_t *) _STR_Ligatures, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'L' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuLigatures, MID_Ligatures },
    NULL
};

static void cblistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *sf = fv->sf;
    int i, anyligs=0, anykerns=0, gid;
    PST *pst;

    if ( sf->kerns ) anykerns=true;
    for ( i=0; i<fv->map->enccount; ++i ) if ( (gid = fv->map->map[i])!=-1 && sf->glyphs[gid]!=NULL ) {
	for ( pst=sf->glyphs[gid]->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_ligature ) {
		anyligs = true;
		if ( anykerns )
    break;
	    }
	}
	if ( sf->glyphs[gid]->kerns!=NULL ) {
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


static GMenuItem gllist[] = {
    { { (unichar_t *) _STR_GlyphImage, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuGlyphLabel, gl_glyph },
    { { (unichar_t *) _STR_GlyphName, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuGlyphLabel, gl_name },
    { { (unichar_t *) _STR_GlyphUnicode, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'L' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuGlyphLabel, gl_unicode },
    { { (unichar_t *) _STR_GlyphEncodingHex, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'L' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuGlyphLabel, gl_encoding },
    NULL
};

static void gllistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	mi->ti.checked = fv->glyphlabel == mi->mid;
    }
}

static GMenuItem emptymenu[] = {
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { NULL }
};

static void FVEncodingMenuBuild(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    extern void GMenuItemArrayFree(GMenuItem *mi);

    if ( mi->sub!=NULL ) {
	GMenuItemArrayFree(mi->sub);
	mi->sub = NULL;
    }
    mi->sub = GetEncodingMenu(FVMenuReencode,fv->map->enc);
}

static void FVForceEncodingMenuBuild(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    extern void GMenuItemArrayFree(GMenuItem *mi);

    if ( mi->sub!=NULL ) {
	GMenuItemArrayFree(mi->sub);
	mi->sub = NULL;
    }
    mi->sub = GetEncodingMenu(FVMenuForceEncode,fv->map->enc);
}

static void FVMenuAddUnencoded(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    unichar_t *ret, *end;
    static unichar_t def[] = { '1', 0 };
    int cnt, i;
    EncMap *map = fv->map;

    /* Add unused unencoded slots in the map */
    ret = GWidgetAskStringR(_STR_AddEncodingSlots,def,fv->cidmaster?_STR_HowManyCIDsToAdd:_STR_HowManySlotsToAdd);
    if ( ret==NULL )
return;
    cnt = u_strtol(ret,&end,10);
    if ( *end!='\0' || cnt<=0 ) {
	free(ret);
	GWidgetErrorR( _STR_BadNumber,_STR_BadNumber );
return;
    }
    free(ret);
    if ( fv->cidmaster ) {
	SplineFont *sf = fv->sf;
	FontView *fvs;
	if ( sf->glyphcnt+cnt<sf->glyphmax )
	    sf->glyphs = grealloc(sf->glyphs,(sf->glyphmax = sf->glyphcnt+cnt+10)*sizeof(SplineChar *));
	memset(sf->glyphs+sf->glyphcnt,0,cnt*sizeof(SplineChar *));
	for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	    EncMap *map = fvs->map;
	    if ( map->enccount+cnt>=map->encmax )
		map->map = grealloc(map->map,(map->encmax += cnt+10)*sizeof(int));
	    if ( sf->glyphcnt+cnt<map->backmax )
		map->backmap = grealloc(map->map,(map->backmax += cnt+10)*sizeof(int));
	    for ( i=map->enccount; i<map->enccount+cnt; ++i )
		map->map[i] = map->backmap[i] = i;
	    fv->selected = grealloc(fv->selected,(map->enccount+cnt));
	    memset(fv->selected+map->enccount,0,cnt);
	    map->enccount += cnt;
	    if ( fv->filled!=NULL ) {
		if ( fv->filled->glyphmax<sf->glyphmax )
		    fv->filled->glyphs = grealloc(fv->filled->glyphs,(sf->glyphmax = sf->glyphcnt+cnt+10)*sizeof(BDFChar *));
		memset(fv->filled->glyphs+fv->filled->glyphcnt,0,cnt*sizeof(BDFChar *));
		fv->filled->glyphcnt = fv->filled->glyphmax = sf->glyphcnt+cnt;
	    }
	}
	sf->glyphcnt += cnt;
	FontViewReformatAll(fv->sf);
    } else {
	if ( map->enccount+cnt>=map->encmax )
	    map->map = grealloc(map->map,(map->encmax += cnt+10)*sizeof(int));
	for ( i=map->enccount; i<map->enccount+cnt; ++i )
	    map->map[i] = -1;
	fv->selected = grealloc(fv->selected,(map->enccount+cnt));
	memset(fv->selected+map->enccount,0,cnt);
	map->enccount += cnt;
	FontViewReformatOne(fv);
    }
}

static void FVMenuRemoveUnused(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    int oldcount = map->enccount;
    int gid, i;
    int flags = -1;

    for ( i=map->enccount-1; i>=0 && ((gid=map->map[i])==-1 || !SCWorthOutputting(sf->glyphs[gid]));
	    --i ) {
	if ( gid!=-1 )
	    SFRemoveGlyph(sf,sf->glyphs[gid],&flags);
	map->enccount = i;
    }
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    /* We reduced the encoding, so don't really need to reallocate the selection */
    /*  array. It's just bigger than it needs to be. */
    if ( oldcount!=map->enccount )
	FontViewReformatOne(fv);
#endif
}

static void FVMenuCompact(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    EncMap *map = fv->map;
    int oldcount = map->enccount;

    CompactEncMap(map,fv->sf);
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    /* We reduced the encoding, so don't really need to reallocate the selection */
    /*  array. It's just bigger than it needs to be. */
    if ( oldcount!=map->enccount )
	FontViewReformatOne(fv);
#endif
}

static void FVMenuDetachGlyphs(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, j, gid;
    EncMap *map = fv->map;
    int altered = false;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    FontView *fvs;
#endif

    for ( i=0; i<map->enccount; ++i ) if ( fv->selected[i] && (gid=map->map[i])!=-1 ) {
	altered = true;
	map->map[i] = -1;
	if ( map->backmap[gid]==i ) {
	    for ( j=map->enccount-1; j>=0 && map->map[j]!=gid; --j );
	    map->backmap[gid] = j;
	}
    }
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    if ( altered )
	for ( fvs = fv->sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	    GDrawRequestExpose(fvs->v,NULL,false);
#endif
}

# ifdef FONTFORGE_CONFIG_GDRAW
static void FVMenuDetachAndRemoveGlyphs(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    static int buts[] = { _STR_Remove, _STR_Cancel, 0 };
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_DetachAndRemoveGlyphs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    static char *buts[] = { GTK_STOCK_REMOVE, GTK_STOCK_CANCEL, NULL };
# endif
    int i, j, gid;
    EncMap *map = fv->map;
    SplineFont *sf = fv->sf;
    int flags = -1;
    int changed = false, altered = false;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    FontView *fvs;
#endif

#if defined(FONTFORGE_CONFIG_GDRAW)
    if ( GWidgetAskR(_STR_DetachAndRemoveGlyphs,buts,0,1,_STR_AreYouSureRemoveGlyphs)==1 )
#elif defined(FONTFORGE_CONFIG_GTK)
    if ( gwwv_ask(_("Remove Glyphs"),buts,0,1,_("Are you sure you wish to remove these glyphs? This operation cannot be undone.")==1 )
#endif
return;

    for ( i=0; i<map->enccount; ++i ) if ( fv->selected[i] && (gid=map->map[i])!=-1 ) {
	altered = true;
	map->map[i] = -1;
	if ( map->backmap[gid]==i ) {
	    for ( j=map->enccount-1; j>=0 && map->map[j]!=gid; --j );
	    map->backmap[gid] = j;
	    if ( j==-1 ) {
		SFRemoveGlyph(sf,sf->glyphs[gid],&flags);
		changed = true;
	    }
	}
    }
    if ( changed && !fv->sf->changed ) {
	fv->sf->changed = true;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
	for ( fvs = sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	    FVSetTitle(fvs);
#endif
    }
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
    if ( altered )
	for ( fvs = sf->fv; fvs!=NULL; fvs=fvs->nextsame )
	    GDrawRequestExpose(fvs->v,NULL,false);
#endif
}

static void FVMenuAddEncodingName(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    unichar_t *ret;
    char *cret;
    Encoding *enc;

    /* Search the iconv database for the named encoding */
    ret = GWidgetAskStringR(_STR_AddEncodingName,NULL,_STR_GiveMeIconvEncodingName);
    if ( ret==NULL )
return;
    cret = cu_copy(ret);
    enc = FindOrMakeEncoding(cret);
    if ( enc==NULL )
	GWidgetErrorR(_STR_InvalidEncoding,_STR_InvalidEncoding);
    free(ret);
    free(cret);
}

static void FVMenuLoadEncoding(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    LoadEncodingFile();
}

static void FVMenuMakeFromFont(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    (void) MakeEncoding(fv->sf,fv->map);
}

static void FVMenuRemoveEncoding(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    RemoveEncoding();
}

static GMenuItem enlist[] = {
    { { (unichar_t *) _STR_Reencode, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'E' }, '\0', ksm_shift|ksm_control, emptymenu, FVEncodingMenuBuild, NULL, MID_Reencode },
    { { (unichar_t *) _STR_Compact, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuCompact, MID_Compact },
    { { (unichar_t *) _STR_ForceEncoding, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, emptymenu, FVForceEncodingMenuBuild, NULL, MID_ForceReencode },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_AddEncodingSlots, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuAddUnencoded, MID_AddUnencoded },
    { { (unichar_t *) _STR_RemoveUnusedSlots, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuRemoveUnused, MID_RemoveUnused },
    { { (unichar_t *) _STR_DetachGlyphs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuDetachGlyphs, MID_DetachGlyphs },
    { { (unichar_t *) _STR_DetachAndRemoveGlyphs, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuDetachAndRemoveGlyphs, MID_DetachAndRemoveGlyphs },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_AddEncodingName, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuAddEncodingName },
    { { (unichar_t *) _STR_LoadEncoding, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuLoadEncoding, MID_LoadEncoding },
    { { (unichar_t *) _STR_Makefromfont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuMakeFromFont, MID_MakeFromFont },
    { { (unichar_t *) _STR_RemoveEncoding, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuRemoveEncoding, MID_RemoveEncoding },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_DisplayByGroups, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuDisplayByGroups, MID_DisplayByGroups },
    { { (unichar_t *) _STR_DefineGroups, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuDefineGroups },
    { NULL }
};

static void enlistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, gid;
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    int anyglyphs = false;

    for ( i=map->enccount-1; i>=0 ; --i )
	if ( fv->selected[i] && (gid=map->map[i])!=-1 )
	    anyglyphs = true;

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_Reencode: case MID_ForceReencode:
	    mi->ti.disabled = fv->cidmaster!=NULL;
	  break;
	  case MID_DetachGlyphs: case MID_DetachAndRemoveGlyphs:
	    mi->ti.disabled = !anyglyphs;
	  break;
	  case MID_RemoveUnused:
	    gid = map->map[map->enccount-1];
	    mi->ti.disabled = gid!=-1 && SCWorthOutputting(sf->glyphs[gid]);
	  break;
	  case MID_MakeFromFont:
	    mi->ti.disabled = fv->cidmaster!=NULL || map->enccount>1024;
	  break;
	  case MID_RemoveEncoding:
	  break;
	  case MID_DisplayByGroups:
	    mi->ti.disabled = fv->cidmaster!=NULL || group_root==NULL;
	  break;
	}
    }
}

static GMenuItem vwlist[] = {
    { { (unichar_t *) _STR_NextGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'N' }, ']', ksm_control, NULL, NULL, FVMenuChangeChar, MID_Next },
    { { (unichar_t *) _STR_PrevGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '[', ksm_control, NULL, NULL, FVMenuChangeChar, MID_Prev },
    { { (unichar_t *) _STR_NextDefGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'D' }, ']', ksm_control|ksm_meta, NULL, NULL, FVMenuChangeChar, MID_NextDef },
    { { (unichar_t *) _STR_PrevDefGlyph, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'a' }, '[', ksm_control|ksm_meta, NULL, NULL, FVMenuChangeChar, MID_PrevDef },
    { { (unichar_t *) _STR_Goto, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'G' }, '>', ksm_shift|ksm_control, NULL, NULL, FVMenuGotoChar },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ShowAtt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'S' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuShowAtt },
    { { (unichar_t *) _STR_DisplaySubstitutions, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 1, 0, 0, 0, 0, 1, 0, 'u' }, '\0', ksm_shift|ksm_control, NULL, NULL, FVMenuDisplaySubs, MID_DisplaySubs },
    { { (unichar_t *) _STR_Combinations, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'b' }, '\0', ksm_shift|ksm_control, cblist, cblistcheck },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_LabelGlyphBy, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'b' }, '\0', ksm_shift|ksm_control, gllist, gllistcheck },
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
    EncMap *map = fv->map;
    int anyglyphs = false;

    for ( i=sf->glyphcnt-1; i>=0 ; --i ) if ( SCWorthOutputting(sf->glyphs[i])) {
	anyglyphs = true;
    break;
    }

    for ( i=0; vwlist[i].ti.text!=(unichar_t *) _STR_FitToEm; ++i );
    base = i+2;
    for ( i=base; vwlist[i].ti.text!=NULL; ++i ) {
	free( vwlist[i].ti.text);
	vwlist[i].ti.text = NULL;
    }

    vwlist[base-1].ti.fg = vwlist[base-1].ti.bg = COLOR_DEFAULT;
    if ( sf->bitmaps==NULL ) {
	vwlist[base-1].ti.line = false;
    } else {
	vwlist[base-1].ti.line = true;
	for ( bdf = sf->bitmaps, i=base;
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
	    pos = anychars+1;
	    if ( anychars<0 ) pos = map->enccount;
	    for ( ; pos<map->enccount &&
		    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
		    ++pos );
	    mi->ti.disabled = pos==map->enccount;
	  break;
	  case MID_PrevDef:
	    for ( pos = anychars-1; pos>=0 &&
		    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
		    --pos );
	    mi->ti.disabled = pos<0;
	  break;
	  case MID_DisplaySubs:
	    mi->ti.checked = fv->cur_feat_tag!=0;
	  break;
	  case MID_ShowHMetrics:
	    /*mi->ti.checked = fv->showhmetrics;*/
	  break;
	  case MID_ShowVMetrics:
	    /*mi->ti.checked = fv->showvmetrics;*/
	    mi->ti.disabled = !sf->hasvmetrics;
	  break;
	  case MID_24:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==24);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_36:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==36);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_48:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==48);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_72:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==72);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_96:
	    mi->ti.checked = (fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==96);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_AntiAlias:
	    mi->ti.checked = (fv->show!=NULL && fv->show->clut!=NULL);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
	  break;
	  case MID_FitToEm:
	    mi->ti.checked = (fv->show!=NULL && !fv->show->bbsized);
	    mi->ti.disabled = sf->onlybitmaps && fv->show!=fv->filled;
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
    { { (unichar_t *) _STR_HintSubsPts, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoHintSubs, MID_HintSubsPt },
    { { (unichar_t *) _STR_AutoCounterHint, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuAutoCounter, MID_AutoCounter },
    { { (unichar_t *) _STR_DontAutoHint, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuDontAutoHint, MID_DontAutoHint },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_AutoInstr, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, 'T', ksm_control, NULL, NULL, FVMenuAutoInstr, MID_AutoInstr },
    { { (unichar_t *) _STR_EditInstructions, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'l' }, '\0', 0, NULL, NULL, FVMenuEditInstrs, MID_EditInstructions },
    { { (unichar_t *) _STR_Editfpgm, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, FVMenuEditTable, MID_Editfpgm },
    { { (unichar_t *) _STR_Editprep, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, FVMenuEditTable, MID_Editprep },
    { { (unichar_t *) _STR_Editcvt, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, '\0', 0, NULL, NULL, FVMenuEditTable, MID_Editcvt },
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
    { { (unichar_t *) _STR_KernPairCloseup, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuKPCloseup },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_VKernByClasses, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'K' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuVKernByClasses, MID_VKernByClass },
    { { (unichar_t *) _STR_VKernFromHKern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuVKernFromHKern, MID_VKernFromH },
    { { (unichar_t *) _STR_RemoveAllVKern, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'P' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuRemoveVKern, MID_RmVKern },
    { NULL }
};

static GMenuItem cdlist[] = {
    { { (unichar_t *) _STR_Convert2CID, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuConvert2CID, MID_Convert2CID },
    { { (unichar_t *) _STR_ConvertByCMap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, '\0', ksm_control, NULL, NULL, FVMenuConvertByCMap, MID_ConvertByCMap },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_Flatten, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', ksm_control, NULL, NULL, FVMenuFlatten, MID_Flatten },
    { { (unichar_t *) _STR_FlattenByCMap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'F' }, '\0', ksm_control, NULL, NULL, FVMenuFlattenByCMap, MID_FlattenByCMap },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_InsertFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'o' }, '\0', ksm_control|ksm_shift, NULL, NULL, FVMenuInsertFont, MID_InsertFont },
    { { (unichar_t *) _STR_InsertBlank, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, '\0', ksm_control, NULL, NULL, FVMenuInsertBlank, MID_InsertBlank },
    { { (unichar_t *) _STR_RemoveFont, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'R' }, '\0', ksm_control, NULL, NULL, FVMenuRemoveFontFromCID, MID_RemoveFromCID },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { { (unichar_t *) _STR_ChangeSupplement, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuChangeSupplement, MID_ChangeSupplement },
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
	    mi->ti.disabled = cidmaster!=NULL || fv->sf->mm!=NULL;
	  break;
	  case MID_InsertFont: case MID_InsertBlank:
	    /* OpenType allows at most 255 subfonts (PS allows more, but why go to the effort to make safe font check that? */
	    mi->ti.disabled = cidmaster==NULL || cidmaster->subfontcnt>=255;
	  break;
	  case MID_RemoveFromCID:
	    mi->ti.disabled = cidmaster==NULL || cidmaster->subfontcnt<=1;
	  break;
	  case MID_Flatten: case MID_FlattenByCMap: case MID_CIDFontInfo:
	  case MID_ChangeSupplement:
	    mi->ti.disabled = cidmaster==NULL;
	  break;
	}
    }
}

static GMenuItem mmlist[] = {
    { { (unichar_t *) _STR_CreateMM, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuCreateMM, MID_CreateMM },
    { { (unichar_t *) _STR_MMValid, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuMMValid, MID_MMValid },
    { { (unichar_t *) _STR_MMInfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuMMInfo, MID_MMInfo },
    { { (unichar_t *) _STR_MMBlendToNew, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuBlendToNew, MID_BlendToNew },
    { { (unichar_t *) _STR_ChangeMMBlend, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuChangeMMBlend, MID_ChangeMMBlend },
    { NULL },				/* Extra room to show sub-font names */
};

static void mmlistcheck(GWindow gw,struct gmenuitem *mi, GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int i, base, j;
    extern void GMenuItemArrayFree(GMenuItem *mi);
    extern GMenuItem *GMenuItemArrayCopy(GMenuItem *mi, uint16 *cnt);
    MMSet *mm = fv->sf->mm;
    SplineFont *sub;
    GMenuItem *mml;

    for ( i=0; mmlist[i].mid!=MID_ChangeMMBlend; ++i );
    base = i+2;
    if ( mm==NULL )
	mml = mmlist;
    else {
	mml = gcalloc(base+mm->instance_count+2,sizeof(GMenuItem));
	memcpy(mml,mmlist,sizeof(mmlist));
	mml[base-1].ti.fg = mml[base-1].ti.bg = COLOR_DEFAULT;
	mml[base-1].ti.line = true;
	for ( j = 0, i=base; j<mm->instance_count+1; ++i, ++j ) {
	    if ( j==0 )
		sub = mm->normal;
	    else
		sub = mm->instances[j-1];
	    mml[i].ti.text = uc_copy(sub->fontname);
	    mml[i].ti.checkable = true;
	    mml[i].ti.checked = sub==fv->sf;
	    mml[i].ti.userdata = sub;
	    mml[i].invoke = FVMenuShowSubFont;
	    mml[i].ti.fg = mml[i].ti.bg = COLOR_DEFAULT;
	}
    }
    GMenuItemArrayFree(mi->sub);
    mi->sub = GMenuItemArrayCopy(mml,NULL);
    if ( mml!=mmlist ) {
	for ( i=base; mml[i].ti.text!=NULL; ++i )
	    free( mml[i].ti.text);
	free(mml);
    }

    for ( mi = mi->sub; mi->ti.text!=NULL || mi->ti.line ; ++mi ) {
	switch ( mi->mid ) {
	  case MID_CreateMM:
	    mi->ti.disabled = false;
	  break;
	  case MID_MMInfo: case MID_MMValid: case MID_BlendToNew:
	    mi->ti.disabled = mm==NULL;
	  break;
	  case MID_ChangeMMBlend:
	    mi->ti.disabled = mm==NULL || mm->apple;
	  break;
	}
    }
}

static GMenuItem wnmenu[] = {
    { { (unichar_t *) _STR_NewOutline, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'u' }, 'H', ksm_control, NULL, NULL, FVMenuOpenOutline, MID_OpenOutline },
    { { (unichar_t *) _STR_NewBitmap, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'B' }, 'J', ksm_control, NULL, NULL, FVMenuOpenBitmap, MID_OpenBitmap },
    { { (unichar_t *) _STR_NewMetrics, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 'K', ksm_control, NULL, NULL, FVMenuOpenMetrics },
    { { NULL, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 1, 0, 0, }},
    { NULL }
};

static void FVWindowMenuBuild(GWindow gw,struct gmenuitem *mi,GEvent *e) {
    FontView *fv = (FontView *) GDrawGetUserData(gw);
    int anychars = FVAnyCharSelected(fv);
    struct gmenuitem *wmi;

    for ( wmi = wnmenu; wmi->ti.text!=NULL || wmi->ti.line ; ++wmi ) {
	switch ( wmi->mid ) {
	  case MID_OpenOutline:
	    wmi->ti.disabled = anychars==-1;
	  break;
	  case MID_OpenBitmap:
	    wmi->ti.disabled = anychars==-1 || fv->sf->bitmaps==NULL;
	  break;
	}
    }
    WindowMenuBuild(gw,mi,e,wnmenu);
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
    { { (unichar_t *) _STR_GlyphInfo, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'I' }, '\0', ksm_control, NULL, NULL, FVMenuCharInfo, MID_CharInfo },
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
    { { (unichar_t *) _STR_EncodingNoC, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, 0, 0, enlist, enlistcheck },
    { { (unichar_t *) _STR_View, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'V' }, 0, 0, vwlist, vwlistcheck },
    { { (unichar_t *) _STR_Metric, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'M' }, 0, 0, mtlist, mtlistcheck },
    { { (unichar_t *) _STR_CID, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'C' }, 0, 0, cdlist, cdlistcheck },
    { { (unichar_t *) _STR_MM, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, '\0' }, 0, 0, mmlist, mmlistcheck },
    { { (unichar_t *) _STR_Window, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'W' }, 0, 0, wnmenu, FVWindowMenuBuild, NULL },
    { { (unichar_t *) _STR_Help, NULL, COLOR_DEFAULT, COLOR_DEFAULT, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 'H' }, 0, 0, helplist, NULL },
    { NULL }
};
# elif defined(FONTFORGE_CONFIG_GTK)
void FontViewMenu_ActivateCopyFrom(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *w;

    w = lookup_widget( menuitem, "all_fonts1" );
    gtk_widget_set_sensitive(w,!onlycopydisplayed);

    w = lookup_widget( menuitem, "displayed_fonts1" );
    gtk_widget_set_sensitive(w,onlycopydisplayed);

    w = lookup_widget( menuitem, "char_metadata1" );
    gtk_widget_set_sensitive(w,copymetadata);
}

struct fv_any {
    FontView *fv;
    int anychars;
    void (*callback)(GtkWidget *w,gpointer user_data);
};

static void activate_file_items(GtkWidget *w, gpointer user_data) {
    struct fv_any *fv_any = (struct fv_any *) user_data;
    G_CONST_RETURN gchar *name = gtk_widget_get_name( GTK_WIDGET(w));

    if ( strcmp(name,"open_outline_window1")==0 )
	gtk_widget_set_sensitive(w,fv_any->anychars!=-1);
    else if ( strcmp(name,"open_bitmap_window1")==0 )
	gtk_widget_set_sensitive(w,fv_any->anychars!=-1 && fv_any->fv->sf->bitmaps!=NULL);
    else if ( strcmp(name,"revert_file1")==0 )
	gtk_widget_set_sensitive(w,fv_any->fv->sf->origname!=NULL);
    else if ( strcmp(name,"revert_glyph1")==0 )
	gtk_widget_set_sensitive(w,fv_any->anychars!=-1 && fv_any->fv->sf->origname!=NULL);
    else if ( strcmp(name,"recent1")==0 )
	gtk_widget_set_sensitive(w,RecentFilesAny());
    else if ( strcmp(name,"script_menu1")==0 )
	gtk_widget_set_sensitive(w,script_menu_names[0]!=NULL);
    else if ( strcmp(name,"print2")==0 )
	gtk_widget_set_sensitive(w,!fv_any->fv->onlybitmaps);
    else if ( strcmp(name,"display1")==0 )
	gtk_widget_set_sensitive(w,!((fv->sf->onlybitmaps && fv->sf->bitmaps==NULL) ||
			fv->sf->multilayer));
}

void FontViewMenu_ActivateFile(GtkMenuItem *menuitem, gpointer user_data) {
    struct fv_any data;

    data.fv = FV_From_MI(menuitem);
    data.anychars = FVAnyCharSelected(data.fv);

    gtk_container_foreach( GTK_CONTAINER( gtk_menu_item_get_submenu(menuitem )),
	    activate_file_items, &data );
}

void FontViewMenu_ActivateEdit(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int pos = FVAnyCharSelected(fv), i;
    /* I can only determine the contents of the clipboard asyncronously */
    /*  hence I can't check to see if it contains something that is pastable */
    /*  So all I can do with paste is check that something is selected */
#if 0
    int not_pasteable = pos==-1 ||
		    (!CopyContainsSomething() &&
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/png") &&
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/bmp") &&
		    !GDrawSelectionHasType(fv->gw,sn_clipboard,"image/eps"));
#endif
    RefChar *base = CopyContainsRef(fv->sf);
    GtkWidget *w;
    int gid;
    static char *poslist[] = { "cut2", "copy2", "copy_reference1", "copy_width1",
	    "copy_lbearing1", "copy_rbearing1", "paste2", "paste_into1",
	    "paste_after1", "same_glyph_as1", "clear2", "clear_background1",
	    "copy_fg_to_bg1", "join1", "replace_with_reference1",
	    "unlink_reference1", "remove_undoes1", NULL };

    w = lookup_widget( menuitem, "undo2" );
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL ) {
	    if ( fv->sf->glyphs[gid]->layers[ly_fore].undoes!=NULL )
    break;
    gtk_widget_set_sensitive(w,i!=fv->map->enccount);

    w = lookup_widget( menuitem, "redo2" );
    for ( i=0; i<fv->map->enccount; ++i )
	if ( fv->selected[i] && (gid = fv->map->map[i])!=-1 &&
		fv->sf->glyphs[gid]!=NULL ) {
	    if ( fv->sf->glyphs[gid]->layers[ly_fore].redoes!=NULL )
    break;
    gtk_widget_set_sensitive(w,i!=fv->map->enccount);

    for ( i=0; poslist[i]!=NULL; ++i ) {
	w = lookup_widget( menuitem, poslist[i] );
	if ( w!=NULL )
	    gtk_widget_set_sensitive(w,pos!=-1);
    }

    w = lookup_widget( menuitem, "copy_vwidth1" );
    if ( w!=NULL )
	gtk_widget_set_sensitive(w,pos!=-1 && fv->sf->hasvmetrics);

    w = lookup_widget( menuitem, "copy_glyph_features1" );
    if ( w!=NULL )
	gtk_widget_set_sensitive(w,pos!=-1 && (gid = fv->map->map[pos])!=-1 &&
		SCAnyFeatures(fv->sf->glyphs[gid]);

#  ifndef FONTFORGE_CONFIG_PASTEAFTER
    w = lookup_widget( menuitem, "paste_after1" );
    gtk_widget_hide(w);
#  endif
}

void FontViewMenu_ActivateDependents(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anychars = FVAnyCharSelected(fv);
    int anygid = anychars<0 ? -1 : fv->map->map[anychars];

    gtk_widget_set_sensitive(lookup_widget( menuitem, "references1" ),
	    anygid>=0 &&
		    fv->sf->glyphs[anygid]!=NULL &&
		    fv->sf->glyphs[anygid]->dependents != NULL);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "substitutions1" ),
	    anygid>=0 &&
		    fv->sf->glyphs[anygid]!=NULL &&
		    SCUsedBySubs(fv->sf->glyphs[anygid]));
}

void FontViewMenu_ActivateAAT(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    FontView *ofv;
    int anychars = FVAnyCharSelected(fv);
    int anygid = anychars<0 ? -1 : fv->map->map[anychars];

    gtk_widget_set_sensitive(lookup_widget( menuitem, "default_att1" ),
	    anygid>=0 );
    for ( ofv=fv_list; ofv!=NULL && ofv->sf==fv->sf; ofv = ofv->next );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "copy_features_to_font1" ),
	    ofv!=NULL );
}

void FontViewMenu_ActivateBuild(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anybuildable = false;
    int anybuildableaccents = false;
    int i, gid;

    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 ) {
	SplineChar *sc, dummy;
	sc = fv->sf->glyphs[gid];
	if ( sc==NULL )
	    sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
	if ( SFIsSomethingBuildable(fv->sf,sc,true)) {
	    anybuildableaccents = anybuildable = true;
    break;
	else if ( SFIsSomethingBuildable(fv->sf,sc,false))
	    anybuildable = true;
    }

    gtk_widget_set_sensitive(lookup_widget( menuitem, "build_accented_char1" ),
	    anybuildableaccents );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "build_composite_char1" ),
	    anybuildable );
}

void FontViewMenu_ActivateElement(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anychars = FVAnyCharSelected(fv);
    int anygid = anychars<0 ? -1 : fv->map->map[anychars];
    int anybuildable, anytraceable;
    int order2 = fv->sf->order2;

    gtk_widget_set_sensitive(lookup_widget( menuitem, "char_info1" ),
	    anygid>=0 &&
		    (fv->cidmaster==NULL || fv->sf->glyphs[anygid]!=NULL));

    gtk_widget_set_sensitive(lookup_widget( menuitem, "find_problems1" ),
	    anygid!=-1);

    gtk_widget_set_sensitive(lookup_widget( menuitem, "transform1" ),
	    anygid!=-1);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "non_linear_transform1" ),
	    anygid!=-1);

    gtk_widget_set_sensitive(lookup_widget( menuitem, "add_extrema1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "round_to_int1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "correct_direction1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "simplify3" ),
	    anygid!=-1 && !fv->sf->onlybitmaps );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "meta_font1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps && !order2 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "expand_stroke1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps && !order2 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "overlap1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps && !order2 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "effects1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps && !order2 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "tilepath1" ),
	    anygid!=-1 && !fv->sf->onlybitmaps && !order2 );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "bitmaps_available1" ),
	    fv->mm==NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "regenerate_bitmaps1" ),
	    fv->mm==NULL && fv->sf->bitmaps!=NULL && !fv->sf->onlybitmaps );

    anybuildable = false;
    if ( anygid!=-1 ) {
	int i, gid;
	for ( i=0; i<fv->map->enccount; ++i ) if ( fv->selected[i] ) {
	    SplineChar *sc=NULL, dummy;
	    if ( (gid=fv->map->map[i])!=-1 )
		sc = fv->sf->glyphs[gid];
	    if ( sc==NULL )
		sc = SCBuildDummy(&dummy,fv->sf,fv->map,i);
	    if ( SFIsSomethingBuildable(fv->sf,sc,false)) {
		anybuildable = true;
	break;
	    }
	}
    }
    gtk_widget_set_sensitive(lookup_widget( menuitem, "build1" ),
	    anybuildable );

    anytraceable = false;
    if ( FindAutoTraceName()!=NULL && anychars!=-1 ) {
	int i;
	for ( i=0; i<fv->map->enccount; ++i )
	    if ( fv->selected[i] && (gid=fv->map->map[i])!=-1 &&
		    fv->sf->glyphs[gid]!=NULL &&
		    fv->sf->glyphs[gid]->layers[ly_back].images!=NULL ) {
		anytraceable = true;
	break;
	    }
    }
    gtk_widget_set_sensitive(lookup_widget( menuitem, "autotrace1" ),
	    anytraceable );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "interpolate_fonts1" ),
	    !fv->sf->onlybitmaps );

#ifndef FONTFORGE_CONFIG_NONLINEAR
    w = lookup_widget( menuitem, "non_linear_transform1" );
    gtk_widget_hide(w);
#endif
#ifndef FONTFORGE_CONFIG_TILEPATH
    w = lookup_widget( menuitem, "tilepath1" );
    gtk_widget_hide(w);
#endif
}

void FontViewMenu_ActivateHints(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anychars = FVAnyCharSelected(fv);
    int anygid = anychars<0 ? -1 : fv->map->map[anychars];
    int multilayer = fv->sf->multilayer;

    gtk_widget_set_sensitive(lookup_widget( menuitem, "autohint1" ),
	    anygid!=-1 && !multilayer );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "hint_subsitution_pts1" ),
	    !fv->sf->order2 && anygid!=-1 && !multilayer );
    if ( fv->sf->mm!=NULL && fv->sf->mm->apple )
	gtk_widget_set_sensitive(lookup_widget( menuitem, "hint_subsitution_pts1" ),
		false);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "auto_counter_hint1" ),
	    !fv->sf->order2 && anygid!=-1 && !multilayer );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "dont_autohint1" ),
	    !fv->sf->order2 && anygid!=-1 && !multilayer );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "autoinstr1" ),
	    fv->sf->order2 && anygid!=-1 && !multilayer );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "edit_instructions1" ),
	    fv->sf->order2 && anygid>=0 && !multilayer );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "edit_fpgm1" ),
	    fv->sf->order2 && !multilayer );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "edit_prep1" ),
	    fv->sf->order2 && !multilayer );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "edit_cvt_1" ),
	    fv->sf->order2 && !multilayer );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "clear_hints1" ),
	    anygid!=-1 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "clear_width_md1" ),
	    anygid!=-1 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "clear_instructions1" ),
	    fv->sf->order2 && anygid!=-1 );
}

/* Builds up a menu containing all the anchor classes */
void FontViewMenu_ActivateAnchoredPairs(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);

    _aplistbuild(menuitem,fv->sf,FVMenuAnchorPairs);
}

void FontViewMenu_ActivateCombinations(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    SplineFont *sf = fv->sf;
    int i, gid, anyligs=0, anykerns=0;
    PST *pst;

    if ( sf->kerns ) anykerns=true;
    for ( i=0; i<map->enccount; ++i ) if ( (gid=fv->map->map[i]!=-1 && sf->glyphs[gid]!=NULL ) {
	for ( pst=sf->glyphs[gid]->possub; pst!=NULL; pst=pst->next ) {
	    if ( pst->type==pst_ligature ) {
		anyligs = true;
		if ( anykerns )
    break;
	    }
	}
	if ( sf->glyphs[gid]->kerns!=NULL ) {
	    anykerns = true;
	    if ( anyligs )
    break;
	}
    }

    gtk_widget_set_sensitive(lookup_widget( menuitem, "ligatures2" ),
	    anyligs);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "kern_pairs1" ),
	    anykerns);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "anchored_pairs1" ),
	    sf->anchor!=NULL);
}

void FontViewMenu_ActivateView(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anychars = FVAnyCharSelected(fv);
    int i,gid;
    BDFFont *bdf;
    int pos;
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    GtkWidget *menu = gtk_menu_item_get_submenu(menuitem);
    GList *kids, *next;
    static int sizes[] = { 24, 36, 48, 72, 96, 0 };

    /* First remove anything we might have added previously */
    for ( kids = GTK_MENU_SHELL(menu)->children; kids!=NULL; kids = next ) {
	GtkWidget *w = kids->data;
	next = kids->next;
	if ( strcmp(gtk_widget_get_name(w),"temporary")==0 )
	    gtk_container_remove(GTK_CONTAINER(menu),w);
    }

    /* Then add any pre-built bitmap sizes */
    if ( sf->bitmaps!=NULL ) {
	GtkWidget *w;

	w = gtk_separator_menu_item_new();
	gtk_widget_show(w);
	gtk_widget_set_name(w,"temporary");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);

	for ( bdf = sf->bitmaps; bdf!=NULL; bdf=bdf->next ) {
	    if ( BDFDepth(bdf)==1 )
		sprintf( buffer, _("%d pixel bitmap"), bdf->pixelsize );
	    else
		sprintf( buffer, _("%d@%d pixel bitmap"),
			bdf->pixelsize, BDFDepth(bdf) );
	    w = gtk_check_menu_item_new_with_label( buffer );
	    gtk_widget_show(w);
	    gtk_widget_set_name(w,"temporary");
	    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(w), bdf==fv->show );
	    g_signal_connect ( G_OBJECT( w ), "activate",
			    G_CALLBACK (FontViewMenu_ShowBitmap),
			    bdf);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);
	}
    }

    gtk_widget_set_sensitive(lookup_widget( menuitem, "next_char1" ),
	    anychars>=0 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "prev_char1" ),
	    anychars>=0 );
    if ( anychars<0 ) pos = map->enccount;
    else for ( pos = anychars+1; pos<map->enccount &&
	    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
	    ++pos );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "prev_char1" ),
	    pos!=map->enccount );
    for ( pos=anychars-1; pos>=0 &&
	    (map->map[pos]==-1 || !SCWorthOutputting(sf->glyphs[map->map[pos]]));
	    --pos );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "prev_char1" ),
	    pos>=0 );
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		lookup_widget( menuitem, "encoded_view1" )),
	    !fv->sf->compacted);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		lookup_widget( menuitem, "compacted_view1" )),
	    fv->sf->compacted);
    gtk_widget_set_sensitive(lookup_widget( menuitem, "show_v_metrics1" ),
	    fv->sf->hasvmetrics );

    for ( i=0; sizes[i]!=NULL; ++i ) {
	char buffer[80];
	sprintf( buffer, "%d_pixel_outline1", sizes[i]);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		    lookup_widget( menuitem, buffer )),
		(fv->show!=NULL && fv->show==fv->filled && fv->show->pixelsize==sizes[i]));
	gtk_widget_set_sensitive(lookup_widget( menuitem, buffer ),
		!fv->sf->onlybitmaps || fv->show==fv->filled );
    }

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		lookup_widget( menuitem, "anti_alias1" )),
	    (fv->show!=NULL && fv->show->clut!=NULL));
    gtk_widget_set_sensitive(lookup_widget( menuitem, "anti_alias1" ),
	    !fv->sf->onlybitmaps || fv->show==fv->filled );
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		lookup_widget( menuitem, "fit_to_em1" )),
	    (fv->show!=NULL && !fv->show->bbsized));
    gtk_widget_set_sensitive(lookup_widget( menuitem, "fit_to_em1" ),
	    !fv->sf->onlybitmaps || fv->show==fv->filled );
}

void FontViewMenu_ActivateMetrics(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int anychars = FVAnyCharSelected(fv);
    char *argnames[] = { "center_in_width1", "thirds_in_width1", "set_width1",
	    "set_lbearing1", "set_rbearing1", "set_vertical_advance1", NULL };
    char *vnames[] = { "vkern_by_classes1", "vkern_from_hkern1",
	    "remove_all_vkern_pairs1", NULL };

    for ( i=0; argnames[i]!=NULL; ++i )
	gtk_widget_set_sensitive(lookup_widget( menuitem, argnames[i] ),
		anychars!=-1 );
    if ( !fv->sf->hasvmetrics )
	gtk_widget_set_sensitive(lookup_widget( menuitem, "set_vertical_advance1" ),
		false );
    for ( i=0; vnames[i]!=NULL; ++i )
	gtk_widget_set_sensitive(lookup_widget( menuitem, vnames[i] ),
		fv->sf->hasvmetrics );
}

void FontViewMenu_ActivateCID(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int i, j;
    SplineFont *sub, *cidmaster = fv->cidmaster;
    GtkWidget *menu = gtk_menu_item_get_submenu(menuitem);
    GList *kids, *next;

    /* First remove anything we might have added previously */
    for ( kids = GTK_MENU_SHELL(menu)->children; kids!=NULL; kids = next ) {
	GtkWidget *w = kids->data;
	next = kids->next;
	if ( strcmp(gtk_widget_get_name(w),"temporary")==0 )
	    gtk_container_remove(GTK_CONTAINER(menu),w);
    }

    /* Then add any sub-fonts */
    if ( cidmaster!=NULL ) {
	GtkWidget *w;

	w = gtk_separator_menu_item_new();
	gtk_widget_show(w);
	gtk_widget_set_name(w,"temporary");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);

	for ( j = 0; j<cidmaster->subfontcnt; ++j ) {
	    sub = cidmaster->subfonts[j];
	    w = gtk_check_menu_item_new_with_label( sub->fontname );
	    gtk_widget_show(w);
	    gtk_widget_set_name(w,"temporary");
	    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(w), sub==fv->sf );
	    g_signal_connect ( G_OBJECT( w ), "activate",
			    G_CALLBACK (FontViewMenu_ShowSubFont),
			    sub);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);
	}
    }

    gtk_widget_set_sensitive(lookup_widget( menuitem, "convert_to_cid1" ),
	    cidmaster==NULL && fv->sf->mm==NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "convert_by_cmap1" ),
	    cidmaster==NULL && fv->sf->mm==NULL );

	    /* OpenType allows at most 255 subfonts (PS allows more, but why go to the effort to make safe font check that? */
    gtk_widget_set_sensitive(lookup_widget( menuitem, "insert_font1" ),
	    cidmaster!=NULL && cidmaster->subfontcnt<255 );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "insert_empty_font1" ),
	    cidmaster!=NULL && cidmaster->subfontcnt<255 );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "remove_font1" ),
	    cidmaster!=NULL && cidmaster->subfontcnt>1 );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "flatten1" ),
	    cidmaster!=NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "flatten_by_cmap1" ),
	    cidmaster!=NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "cid_font_info1" ),
	    cidmaster!=NULL );
}

void FontViewMenu_ActivateMM(GtkMenuItem *menuitem, gpointer user_data) {
    FontView *fv = FV_From_MI(menuitem);
    int i, base, j;
    MMSet *mm = fv->sf->mm;
    SplineFont *sub;
    GMenuItem *mml;
    GtkWidget *menu = gtk_menu_item_get_submenu(menuitem);
    GList *kids, *next;

    /* First remove anything we might have added previously */
    for ( kids = GTK_MENU_SHELL(menu)->children; kids!=NULL; kids = next ) {
	GtkWidget *w = kids->data;
	next = kids->next;
	if ( strcmp(gtk_widget_get_name(w),"temporary")==0 )
	    gtk_container_remove(GTK_CONTAINER(menu),w);
    }

    /* Then add any sub-fonts */
    if ( mm!=NULL ) {
	GtkWidget *w;

	w = gtk_separator_menu_item_new();
	gtk_widget_show(w);
	gtk_widget_set_name(w,"temporary");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);

	for ( j = 0; j<mm->instance_count+1; ++j ) {
	    if ( j==0 )
		sub = mm->normal;
	    else
		sub = mm->instances[j-1];
	    w = gtk_check_menu_item_new_with_label( sub->fontname );
	    gtk_widget_show(w);
	    gtk_widget_set_name(w,"temporary");
	    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(w), sub==fv->sf );
	    g_signal_connect ( G_OBJECT( w ), "activate",
			    G_CALLBACK (FontViewMenu_ShowSubFont),
			    sub);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu),w);
	}
    }

    gtk_widget_set_sensitive(lookup_widget( menuitem, "mm_validity_check1" ),
	    mm!=NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "mm_info1" ),
	    mm!=NULL );
    gtk_widget_set_sensitive(lookup_widget( menuitem, "blend_to_new_font1" ),
	    mm!=NULL );

    gtk_widget_set_sensitive(lookup_widget( menuitem, "mm_change_def_weights1" ),
	    mm!=NULL && !mm->apple );
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
static int FeatureTrans(FontView *fv, int enc) {
    SplineChar *sc;
    PST *pst;
    char *pt;

    if ( fv->cur_feat_tag==0 )
return( enc );
    if ( enc<0 || enc>=fv->map->enccount || fv->map->map[enc]==-1 )
return( -1 );
    sc = fv->sf->glyphs[fv->map->map[enc]];
    if ( sc==NULL )
return( -1 );
    for ( pst = sc->possub; pst!=NULL; pst=pst->next ) {
	if (( pst->type == pst_substitution || pst->type == pst_alternate ) &&
		pst->tag == fv->cur_feat_tag && pst->script_lang_index == fv->cur_sli )
    break;
    }
    if ( pst==NULL )
return( -1 );
    pt = strchr(pst->u.subs.variant,' ');
    if ( pt!=NULL )
	*pt = '\0';
return( SFFindExistingSlot(fv->sf, -1, pst->u.subs.variant ));
}

void FVRefreshChar(FontView *fv,int gid) {
    BDFChar *bdfc;
    int i, j, enc;
    MetricsView *mv;

    if ( fv->v==NULL || fv->colcnt==0 )	/* Can happen in scripts */
return;
    if ( fv->cur_feat_tag!=0 && strchr(fv->sf->glyphs[gid]->name,'.')!=NULL ) {
	char *temp = copy(fv->sf->glyphs[gid]->name);
	SplineChar *sc2;
	*strchr(temp,'.') = '\0';
	sc2 = SFGetChar(fv->sf,-1,temp);
	if ( sc2!=NULL && sc2->orig_pos!=gid )
	    gid = sc2->orig_pos;
	free(temp);
    }

    for ( fv=fv->sf->fv; fv!=NULL; fv = fv->nextsame ) {
	for ( mv=fv->metrics; mv!=NULL; mv=mv->next )
	    MVRefreshChar(mv,fv->sf->glyphs[gid]);
	bdfc = fv->show->glyphs[gid];
	/* A glyph may be encoded in several places, all need updating */
	for ( enc = 0; enc<fv->map->enccount; ++enc ) if ( fv->map->map[enc]==gid ) {
	    i = enc / fv->colcnt;
	    j = enc - i*fv->colcnt;
	    i -= fv->rowoff;
	    if ( i>=0 && i<fv->rowcnt ) {
		struct _GImage base;
		GImage gi;
		GClut clut;
		GRect old, box;

		if ( bdfc==NULL )
		    bdfc = BDFPieceMeal(fv->show,gid);

		memset(&gi,'\0',sizeof(gi));
		memset(&base,'\0',sizeof(base));
		if ( bdfc->byte_data ) {
		    gi.u.image = &base;
		    base.image_type = it_index;
		    base.clut = fv->show->clut;
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
		box.y = i*fv->cbh+fv->lab_height+1; box.height = fv->cbw;
		GDrawPushClip(fv->v,&box,&old);
		GDrawFillRect(fv->v,&box,GDrawGetDefaultBackground(NULL));
		if ( fv->magnify>1 ) {
		    GDrawDrawImageMagnified(fv->v,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2,
			    i*fv->cbh+fv->lab_height+1+fv->magnify*(fv->show->ascent-bdfc->ymax),
			    fv->magnify*base.width,fv->magnify*base.height);
		} else
		    GDrawDrawImage(fv->v,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-base.width)/2,
			    i*fv->cbh+fv->lab_height+1+fv->show->ascent-bdfc->ymax);
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
}

void FVRegenChar(FontView *fv,SplineChar *sc) {
    struct splinecharlist *dlist;
    MetricsView *mv;

    if ( fv->v==NULL )			/* Can happen in scripts */
return;

    sc->changedsincelasthinted = true;
    if ( sc->orig_pos>=fv->filled->glyphcnt )
	IError("Character out of bounds in bitmap font %d>=%d", sc->orig_pos, fv->filled->glyphcnt );
    else
	BDFCharFree(fv->filled->glyphs[sc->orig_pos]);
    fv->filled->glyphs[sc->orig_pos] = NULL;
		/* FVRefreshChar does NOT do this for us */
    for ( mv=fv->metrics; mv!=NULL; mv=mv->next )
	MVRegenChar(mv,sc);

    FVRefreshChar(fv,sc->orig_pos);
#if HANYANG
    if ( sc->compositionunit && fv->sf->rules!=NULL )
	Disp_RefreshChar(fv->sf,sc);
#endif

    for ( dlist=sc->dependents; dlist!=NULL; dlist=dlist->next )
	FVRegenChar(fv,dlist->sc);
}
#endif

int32 UniFromEnc(int enc, Encoding *encname) {
    char from[20];
    unsigned short to[20];
    ICONV_CONST char *fpt;
    char *tpt;
    size_t fromlen, tolen;

    if ( encname->is_custom || encname->is_original )
return( -1 );
    if ( enc>=encname->char_cnt )
return( -1 );
    if ( encname->is_unicodebmp || encname->is_unicodefull )
return( enc );
    if ( encname->unicode!=NULL )
return( encname->unicode[enc] );
    else if ( encname->tounicode ) {
	/* To my surprise, on RH9, doing a reset on conversion of CP1258->UCS2 */
	/* causes subsequent calls to return garbage */
	if ( encname->iso_2022_escape_len ) {
	    tolen = sizeof(to); fromlen = 0;
	    iconv(encname->tounicode,NULL,&fromlen,NULL,&tolen);	/* Reset state */
	}
	fpt = from; tpt = (char *) to; tolen = sizeof(to);
	if ( encname->has_1byte && enc<256 ) {
	    *(char *) fpt = enc;
	    fromlen = 1;
	} else if ( encname->has_2byte ) {
	    if ( encname->iso_2022_escape_len )
		strncpy(from,encname->iso_2022_escape,encname->iso_2022_escape_len );
	    fromlen = encname->iso_2022_escape_len;
	    from[fromlen++] = enc>>8;
	    from[fromlen++] = enc&0xff;
	}
	if ( iconv(encname->tounicode,&fpt,&fromlen,&tpt,&tolen)==(size_t) -1 )
return( -1 );
	if ( tpt-(char *) to == 0 ) {
	    /* This strange call appears to be what we need to make CP1258->UCS2 */
	    /*  work.  It's supposed to reset the state and give us the shift */
	    /*  out. As there is no state, and no shift out I have no idea why*/
	    /*  this works, but it does. */
	    if ( iconv(encname->tounicode,NULL,&fromlen,&tpt,&tolen)==(size_t) -1 )
return( -1 );
	}
	if ( tpt-(char *) to == 2 )
return( to[0] );
	else if ( tpt-(char *) to == 4 && to[0]>=0xd800 && to[0]<0xdc00 && to[1]>=0xdc00 )
return( ((to[0]-0xd800)<<10) + (to[1]-0xdc00) + 0x10000 );
    }
return( -1 );
}

int32 EncFromUni(int32 uni, Encoding *enc) {
    short from[20];
    unsigned char to[20];
    ICONV_CONST char *fpt;
    char *tpt;
    size_t fromlen, tolen;
    int i;

    if ( enc->is_custom || enc->is_original || enc->is_compact )
return( -1 );
    if ( enc->is_unicodebmp || enc->is_unicodefull )
return( uni<enc->char_cnt ? uni : -1 );

    if ( enc->unicode!=NULL ) {
	for ( i=0; i<enc->char_cnt; ++i ) {
	    if ( enc->unicode[i]==uni )
return( i );
	}
return( -1 );
    } else if ( enc->fromunicode!=NULL ) {
	/* I don't see how there can be any state to reset in this direction */
	/*  So I don't reset it */
	if ( uni<0x10000 ) {
	    from[0] = uni;
	    fromlen = 2;
	} else {
	    uni -= 0x10000;
	    from[0] = 0xd800 + (uni>>10);
	    from[1] = 0xdc00 + (uni&0x3ff);
	    fromlen = 4;
	}
	fpt = (char *) from; tpt = (char *) to; tolen = sizeof(to);
	iconv(enc->fromunicode,NULL,NULL,NULL,NULL);	/* reset shift in/out, etc. */
	if ( iconv(enc->fromunicode,&fpt,&fromlen,&tpt,&tolen)==(size_t) -1 )
return( -1 );
	if ( tpt-(char *) to == 1 )
return( to[0] );
	if ( enc->iso_2022_escape_len!=0 ) {
	    if ( tpt-(char *) to == enc->iso_2022_escape_len+2 &&
		    strncmp((char *) to,enc->iso_2022_escape,enc->iso_2022_escape_len)==0 )
return( (to[enc->iso_2022_escape_len]<<8) | to[enc->iso_2022_escape_len+1] );
	} else {
	    if ( tpt-(char *) to == 2 )
return( (to[0]<<8) | to[1] );
	}
    }
return( -1 );
}

char *StdGlyphName(char *buffer, int uni,enum uni_interp interp) {
    char *name = NULL;
    int j;

    if ( (uni>=0 && uni<' ') ||
	    (uni>=0x7f && uni<0xa0) )
	/* standard controls */;
    else if ( uni!=-1  ) {
	if ( uni>=0xe000 && uni<=0xf8ff &&
		(interp==ui_trad_chinese || interp==ui_ams)) {
	    extern const int cns14pua[], amspua[];
	    const int *pua = interp==ui_trad_chinese ? cns14pua : amspua;
	    if ( pua[uni-0xe000]!=0 )
		uni = pua[uni-0xe000];
	}
	/* Adobe's standard names are wrong for: */
	/* 0x2206 is named Delta, 0x394 should be */
	/* 0x2126 is named Omega, 0x3A9 should be */
	/* 0x00b5 is named mu, 0x3BC should be */
	/* 0x0162 is named Tcommaaccent, 0x21A should be */
	/* 0x0163 is named tcommaaccent, 0x21B should be */
	/* 0xf6be is named dotlessj, 0x237 should be */
	if ( uni==0x2206 || uni==0x2126 || uni==0x00b5 || uni==0x0162 ||
		uni==0x0163 || uni==0xf6be )
	    /* Don't use the standard names */;
	else if ( uni<psunicodenames_cnt )
	    name = (char *) psunicodenames[uni];
	if ( name==NULL &&
		(interp==ui_adobe || interp==ui_ams) &&
		((uni>=0xe000 && uni<=0xf7ff ) ||
		 (uni>=0xfb00 && uni<=0xfb06 ))) {
	    int provenance = interp==ui_adobe ? 1 : 2;
	    /* If we are using Adobe's interpretation of the private use */
	    /*  area (which means small caps, etc. Then look for those */
	    /*  names (also include the names for ligatures) */
	    for ( j=0; psaltuninames[j].name!=NULL; ++j ) {
		if ( psaltuninames[j].unicode == uni &&
			psaltuninames[j].provenance == provenance ) {
		    name = psaltuninames[j].name;
	    break;
		}
	    }
	}
	if ( name==NULL ) {
	    if ( uni==0x2d )
		name = "hyphen-minus";
	    else if ( uni==0xad )
		name = "softhyphen";
	    else if ( uni==0x00 )
		name = ".notdef";
	    else if ( uni==0xa0 )
		name = "nonbreakingspace";
	    else if ( uni==0x03bc && interp==ui_greek )
		name = "mu.greek";
	    else if ( uni==0x0394 && interp==ui_greek )
		name = "Delta.greek";
	    else if ( uni==0x03a9 && interp==ui_greek )
		name = "Omega.greek";
	    else {
		if ( uni>=0x10000 )
		    sprintf( buffer, "u%04X", uni);
		else
		    sprintf( buffer, "uni%04X", uni);
		name = buffer;
	    }
	}
    }
return( name );
}

SplineChar *SCBuildDummy(SplineChar *dummy,SplineFont *sf,EncMap *map,int i) {
    static char namebuf[100];
#ifdef FONTFORGE_CONFIG_TYPE3
    static Layer layers[2];
#endif

    memset(dummy,'\0',sizeof(*dummy));
    dummy->color = COLOR_DEFAULT;
    dummy->layer_cnt = 2;
#ifdef FONTFORGE_CONFIG_TYPE3
    dummy->layers = layers;
#endif
    if ( sf->cidmaster!=NULL ) {
	/* CID fonts don't have encodings, instead we must look up the cid */
	if ( sf->cidmaster->loading_cid_map )
	    dummy->unicodeenc = -1;
	else
	    dummy->unicodeenc = CID2NameUni(FindCidMap(sf->cidmaster->cidregistry,sf->cidmaster->ordering,sf->cidmaster->supplement,sf->cidmaster),
		    i,namebuf,sizeof(namebuf));
    } else
	dummy->unicodeenc = UniFromEnc(i,map->enc);

    if ( sf->cidmaster!=NULL )
	dummy->name = namebuf;
    else if ( map->enc->psnames!=NULL && i<map->enc->char_cnt &&
	    map->enc->psnames[i]!=NULL )
	dummy->name = map->enc->psnames[i];
    else
	dummy->name = StdGlyphName(namebuf,dummy->unicodeenc,sf->uni_interp);
    if ( dummy->name==NULL ) {
	/*if ( dummy->unicodeenc!=-1 || i<256 )
	    dummy->name = ".notdef";
	else*/ {
	    int j;
	    sprintf( namebuf, "NameMe-%d", i);
	    j=0;
	    while ( SFFindExistingSlot(sf,-1,namebuf)!=-1 )
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
    dummy->tex_height = *dummy->name;		/* Debugging */
return( dummy );
}

static SplineChar *_SFMakeChar(SplineFont *sf,EncMap *map,int enc) {
    SplineChar dummy, *sc;
    SplineFont *ssf;
    int j, real_uni, gid;
#ifdef FONTFORGE_CONFIG_TYPE3
    Layer *l;
#endif
    extern const int cns14pua[], amspua[];

    gid = map->map[enc];
    if ( sf->subfontcnt!=0 ) {
	ssf = NULL;
	for ( j=0; j<sf->subfontcnt; ++j )
	    if ( gid<sf->subfonts[j]->glyphcnt ) {
		ssf = sf->subfonts[j];
		if ( ssf->glyphs[gid]!=NULL ) {
return( ssf->glyphs[gid] );
		}
	    }
	sf = ssf;
    }

    if ( gid==-1 || (sc = sf->glyphs[gid])==NULL ) {
	if (( map->enc->is_unicodebmp || map->enc->is_unicodefull ) &&
		( enc>=0xe000 && enc<=0xf8ff ) &&
		( sf->uni_interp==ui_ams || sf->uni_interp==ui_trad_chinese ) &&
		( real_uni = (sf->uni_interp==ui_ams ? amspua : cns14pua)[enc-0xe000])!=0 ) {
	    if ( real_uni<map->enccount ) {
		SplineChar *sc;
		/* if necessary, create the real unicode code point */
		/*  and then make us be a duplicate of it */
		sc = _SFMakeChar(sf,map,real_uni);
		map->map[enc] = gid = sc->orig_pos;
		SCCharChangedUpdate(sc);
return( sc );
	    }
	}

	SCBuildDummy(&dummy,sf,map,enc);
	if ((sc = SFGetChar(sf,dummy.unicodeenc,dummy.name))!=NULL )
return( sc );
	sc = SplineCharCreate();
#ifdef FONTFORGE_CONFIG_TYPE3
	l = sc->layers;
	*sc = dummy;
	sc->layers = l;		/* It's empty, no need to copy dummy's layers */
	if ( sf->strokedfont ) {
	    l[ly_fore].dostroke = true;
	    l[ly_fore].dofill = false;
	}
#else
	*sc = dummy;
#endif
	sc->name = copy(sc->name);
	SCLigDefault(sc);
	SFAddGlyphAndEncode(sf,sc,map,enc);
    }
return( sc );
}

SplineChar *SFMakeChar(SplineFont *sf,EncMap *map, int enc) {
    int gid;

    if ( enc==-1 )
return( NULL );
    gid = map->map[enc];
    if ( sf->mm!=NULL && (gid==-1 || sf->glyphs[gid]==NULL) ) {
	int j;
	_SFMakeChar(sf->mm->normal,map,enc);
	for ( j=0; j<sf->mm->instance_count; ++j )
	    _SFMakeChar(sf->mm->instances[j],map,enc);
    }
return( _SFMakeChar(sf,map,enc));
}

#if !defined(FONTFORGE_CONFIG_NO_WINDOWING_UI)
SplineChar *FVMakeChar(FontView *fv,int enc) {
    SplineFont *sf = fv->sf;
    SplineChar *base_sc = SFMakeChar(sf,fv->map,enc), *feat_sc = NULL;
    int feat_i = FeatureTrans(fv,enc);

    if ( fv->cur_feat_tag==0 )
return( base_sc );

    if ( feat_i==-1 ) {
	int uni = -1;
	if ( base_sc->unicodeenc>=0x600 && base_sc->unicodeenc<=0x6ff &&
		(fv->cur_feat_tag == CHR('i','n','i','t') ||
		 fv->cur_feat_tag == CHR('m','e','d','i') ||
		 fv->cur_feat_tag == CHR('f','i','n','a') ||
		 fv->cur_feat_tag == CHR('i','s','o','l')) ) {
	    uni = fv->cur_feat_tag == CHR('i','n','i','t') ? ArabicForms[base_sc->unicodeenc-0x600].initial  :
		  fv->cur_feat_tag == CHR('m','e','d','i') ? ArabicForms[base_sc->unicodeenc-0x600].medial   :
		  fv->cur_feat_tag == CHR('f','i','n','a') ? ArabicForms[base_sc->unicodeenc-0x600].final    :
		  fv->cur_feat_tag == CHR('i','s','o','l') ? ArabicForms[base_sc->unicodeenc-0x600].isolated :
		  -1;
	    feat_sc = SFGetChar(sf,uni,NULL);
	    if ( feat_sc!=NULL )
return( feat_sc );
	}
	feat_sc = SplineCharCreate();
	feat_sc->parent = sf;
	feat_sc->unicodeenc = uni;
	if ( uni!=-1 ) {
	    feat_sc->name = galloc(8);
	    feat_sc->unicodeenc = uni;
	    sprintf( feat_sc->name,"uni%04X", uni );
	} else if ( fv->cur_feat_tag>=CHR(' ',' ',' ',' ')) {
	    /* OpenType feature tag */
	    feat_sc->name = galloc(strlen(base_sc->name)+6);
	    sprintf( feat_sc->name,"%s.%c%c%c%c", base_sc->name,
		    fv->cur_feat_tag>>24,
		    (fv->cur_feat_tag>>16)&0xff,
		    (fv->cur_feat_tag>>8)&0xff,
		    (fv->cur_feat_tag)&0xff );
	} else {
	    /* mac feature/setting */
	    feat_sc->name = galloc(strlen(base_sc->name)+14);
	    sprintf( feat_sc->name,"%s.m%d_%d", base_sc->name,
		    fv->cur_feat_tag>>16,
		    (fv->cur_feat_tag)&0xffff );
	}
	SFAddGlyphAndEncode(sf,feat_sc,fv->map,enc);
	base_sc->possub = AddSubs(base_sc->possub,fv->cur_feat_tag,feat_sc->name,
		0/* No flags */,fv->cur_sli,base_sc);
return( feat_sc );
    } else
return( base_sc );
}
#else
SplineChar *FVMakeChar(FontView *fv,int enc) {
    SplineFont *sf = fv->sf;
return( SFMakeChar(sf,fv->map,enc) );
}
#endif

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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

static GImage *UniGetRotatedGlyph(FontView *fv, SplineChar *sc,int uni) {
    SplineFont *sf = fv->sf;
    int cid=-1;
    static GWindow pixmap=NULL;
    GRect r;
    unichar_t buf[2];
    GImage *unrot, *rot;
    SplineFont *cm = sf->cidmaster;

    if ( uni!=-1 )
	/* Do nothing */;
    else if ( sscanf(sc->name,"vertuni%x", (unsigned *) &uni)==1 )
	/* All done */;
    else if ( cm!=NULL &&
	    ((cid=CIDFromName(sc->name,cm))!=-1 ||
	     sscanf( sc->name, "cid-%d", &cid)==1 ||		/* Obsolete names */
	     sscanf( sc->name, "vertcid_%d", &cid)==1 ||
	     sscanf( sc->name, "cid_%d", &cid)==1 )) {
	uni = CID2Uni(FindCidMap(cm->cidregistry,cm->ordering,cm->supplement,cm),
		cid);
    }
    if ( uni&0x10000 ) uni -= 0x10000;		/* Bug in some old cidmap files */
    if ( uni<0 || uni>0xffff )
return( NULL );

    if ( pixmap==NULL ) {
	pixmap = GDrawCreateBitmap(NULL,2*fv->lab_height,2*fv->lab_height,NULL);
	if ( pixmap==NULL )
return( NULL );
	GDrawSetFont(pixmap,sf->fv->fontset[0]);
    }
    r.x = r.y = 0;
    r.width = r.height = 2*fv->lab_height;
    GDrawFillRect(pixmap,&r,1);
    buf[0] = uni; buf[1] = 0;
    GDrawDrawText(pixmap,2,fv->lab_height,buf,1,NULL,0);
    unrot = GDrawCopyScreenToImage(pixmap,&r);
    if ( unrot==NULL )
return( NULL );

    rot = GImageCropAndRotate(unrot);
    GImageDestroy(unrot);
return( rot );
}

#if 0
static int Use2ByteEnc(FontView *fv,SplineChar *sc, unichar_t *buf,FontMods *mods) {
    int ch1 = sc->enc>>8, ch2 = sc->enc&0xff, newch;
    Encoding *enc = fv->map->enc;
    unsigned short *subtable;

 retry:
    switch ( enc ) {
      case em_big5: case em_big5hkscs:
	if ( !GDrawFontHasCharset(fv->fontset[0],em_big5))
return( false);
	if ( ch1<0xa1 || ch1>0xf9 || ch2<0x40 || ch2>0xfe || sc->enc> 0xf9fe )
return( false );
	mods->has_charset = true; mods->charset = em_big5;
	buf[0] = sc->enc;
	buf[1] = 0;
return( true );
      break;
      case em_sjis:
	if ( !GDrawFontHasCharset(fv->fontset[0],em_jis208))
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
	if ( !GDrawFontHasCharset(fv->fontset[0],em_ksc5601))
return( false);
	if ( ch1<0xa1 || ch1>0xfd || ch2<0xa1 || ch2>0xfe || sc->enc > 0xfdfe )
return( false );
	mods->has_charset = true; mods->charset = em_ksc5601;
	buf[0] = sc->enc-0x8080;
	buf[1] = 0;
return( true );
      break;
      case em_jisgb:
	if ( !GDrawFontHasCharset(fv->fontset[0],em_gb2312))
return( false);
	if ( ch1<0xa1 || ch1>0xfd || ch2<0xa1 || ch2>0xfe || sc->enc > 0xfdfe )
return( false );
	mods->has_charset = true; mods->charset = em_gb2312;
	buf[0] = sc->enc-0x8080;
	buf[1] = 0;
return( true );
      break;
      case em_ksc5601: case em_jis208: case em_jis212: case em_gb2312:
	if ( !GDrawFontHasCharset(fv->fontset[0],enc))
return( false);
	if ( ch1<0x21 || ch1>0x7e || ch2<0x21 || ch2>0x7e )
return( false );
	mods->has_charset = true; mods->charset = enc;
	buf[0] = (ch1<<8)|ch2;
	buf[1] = 0;
return( true );
      break;
      default:
    /* If possible, look at the unicode font using the appropriate glyphs */
    /*  for the CJ language for which the font was designed */
	ch1 = sc->unicodeenc>>8, ch2 = sc->unicodeenc&0xff;
	switch ( fv->sf->uni_interp ) {
	  case ui_japanese:
	    if ( ch1>=jis_from_unicode.first && ch1<=jis_from_unicode.last &&
		    (subtable = jis_from_unicode.table[ch1-jis_from_unicode.first])!=NULL &&
		    (newch = subtable[ch2])!=0 ) {
		if ( newch&0x8000 ) {
		    if ( GDrawFontHasCharset(fv->fontset[0],em_jis212)) {
			enc = em_jis212;
			newch &= ~0x8000;
			ch1 = newch>>8; ch2 = newch&0xff;
		    } else
return( false );
		} else {
		    if ( GDrawFontHasCharset(fv->fontset[0],em_jis208)) {
			enc = em_jis208;
			ch1 = newch>>8; ch2 = newch&0xff;
		    } else
return( false );
		}
	    } else
return( false );
	  break;
	  case ui_korean:
	    /* Don't know what to do about korean hanga chars */
	    /* No ambiguity for hangul */
return( false );
	  break;
	  case ui_trad_chinese:
	    if ( ch1>=big5hkscs_from_unicode.first && ch1<=big5hkscs_from_unicode.last &&
		    (subtable = big5hkscs_from_unicode.table[ch1-big5hkscs_from_unicode.first])!=NULL &&
		    (newch = subtable[ch2])!=0 &&
		    GDrawFontHasCharset(fv->fontset[0],em_big5)) {
		enc = em_big5hkscs;
		ch1 = newch>>8; ch2 = newch&0xff;
	    } else
return( false );
	  break;
	  case ui_simp_chinese:
	    if ( ch1>=gb2312_from_unicode.first && ch1<=gb2312_from_unicode.last &&
		    (subtable = gb2312_from_unicode.table[ch1-gb2312_from_unicode.first])!=NULL &&
		    (newch = subtable[ch2])!=0 &&
		    GDrawFontHasCharset(fv->fontset[0],em_gb2312)) {
		enc = em_gb2312;
		ch1 = newch>>8; ch2 = newch&0xff;
	    } else
return( false );
	  break;
	  default:
return( false );
	}
 goto retry;
    }
}
#endif

/* Mathmatical Alphanumeric Symbols in the 1d400-1d7ff range are styled */
/*  variants on latin, greek, and digits				*/
#define _uni_bold	0x1
#define _uni_italic	0x2
#define _uni_script	(1<<2)
#define _uni_fraktur	(2<<2)
#define _uni_doublestruck	(3<<2)
#define _uni_sans	(4<<2)
#define _uni_mono	(5<<2)
#define _uni_fontmax	(6<<2)
#define _uni_latin	0
#define _uni_greek	1
#define _uni_digit	2

static int latinmap[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '\0'
};
static int greekmap[] = {
    0x391, 0x392, 0x393, 0x394, 0x395, 0x396, 0x397, 0x398, 0x399, 0x39a,
    0x39b, 0x39c, 0x39d, 0x39e, 0x39f, 0x3a0, 0x3a1, 0x3f4, 0x3a3, 0x3a4,
    0x3a5, 0x3a6, 0x3a7, 0x3a8, 0x3a9, 0x2207,
    0x3b1, 0x3b2, 0x3b3, 0x3b4, 0x3b5, 0x3b6, 0x3b7, 0x3b8, 0x3b9, 0x3ba,
    0x3bb, 0x3bc, 0x3bd, 0x3be, 0x3bf, 0x3c0, 0x3c1, 0x3c2, 0x3c3, 0x3c4,
    0x3c5, 0x3c6, 0x3c7, 0x3c8, 0x3c9,
    0x2202, 0x3f5, 0x3d1, 0x3f0, 0x3d5, 0x3f1, 0x3d6,
    0
};
static int digitmap[] = { '0', '1', '2', '3', '4', '5', '6','7','8','9', '\0' };
static int *maps[] = { latinmap, greekmap, digitmap };

static struct { int start, last; int styles; int charset; } mathmap[] = {
    { 0x1d400, 0x1d433, _uni_bold,		_uni_latin },
    { 0x1d434, 0x1d467, _uni_italic,		_uni_latin },
    { 0x1d468, 0x1d49b, _uni_bold|_uni_italic,	_uni_latin },
    { 0x1d49c, 0x1d4cf, _uni_script,		_uni_latin },
    { 0x1d4d0, 0x1d503, _uni_script|_uni_bold,	_uni_latin },
    { 0x1d504, 0x1d537, _uni_fraktur,		_uni_latin },
    { 0x1d538, 0x1d56b, _uni_doublestruck,	_uni_latin },
    { 0x1d56c, 0x1d59f, _uni_fraktur|_uni_bold,	_uni_latin },
    { 0x1d5a0, 0x1d5d3, _uni_sans,		_uni_latin },
    { 0x1d5d4, 0x1d607, _uni_sans|_uni_bold,	_uni_latin },
    { 0x1d608, 0x1d63b, _uni_sans|_uni_italic,	_uni_latin },
    { 0x1d63c, 0x1d66f, _uni_sans|_uni_bold|_uni_italic,	_uni_latin },
    { 0x1d670, 0x1d6a3, _uni_mono,		_uni_latin },
    { 0x1d6a8, 0x1d6e1, _uni_bold,		_uni_greek },
    { 0x1d6e2, 0x1d71b, _uni_italic,		_uni_greek },
    { 0x1d71c, 0x1d755, _uni_bold|_uni_italic,	_uni_greek },
    { 0x1d756, 0x1d78f, _uni_sans|_uni_bold,	_uni_greek },
    { 0x1d790, 0x1d7c9, _uni_sans|_uni_bold|_uni_italic,	_uni_greek },
    { 0x1d7ce, 0x1d7d7, _uni_bold,		_uni_digit },
    { 0x1d7d8, 0x1d7e1, _uni_doublestruck,	_uni_digit },
    { 0x1d7e2, 0x1d7eb, _uni_sans,		_uni_digit },
    { 0x1d7ec, 0x1d7f5, _uni_sans|_uni_bold,	_uni_digit },
    { 0x1d7f6, 0x1d7ff, _uni_mono,		_uni_digit },
    { 0, 0 }
};
    
static GFont *FVCheckFont(FontView *fv,int type) {
    FontRequest rq;
    int family = type>>2;
    char *fontnames;
    unichar_t *ufontnames;

    static char *resourcenames[] = { "FontView.SerifFamily", "FontView.ScriptFamily",
	    "FontView.FrakturFamily", "FontView.DoubleStruckFamily",
	    "FontView.SansFamily", "FontView.MonoFamily", NULL };
    static char *defaultfontnames[] = {
	    "times,serif,caslon,clearlyu,unifont",
	    "script,formalscript,clearlyu,unifont",
	    "fraktur,clearlyu,unifont",
	    "doublestruck,clearlyu,unifont",
	    "helvetica,caliban,sansserif,sans,clearlyu,unifont",
	    "courier,monospace,caslon,clearlyu,unifont",
	    NULL
	};

    if ( fv->fontset[type]==NULL ) {
	fontnames = GResourceFindString(resourcenames[family]);
	if ( fontnames==NULL )
	    fontnames = defaultfontnames[family];
	ufontnames = uc_copy(fontnames);

	memset(&rq,0,sizeof(rq));
	rq.family_name = ufontnames;
	rq.point_size = -13;
	rq.weight = (type&_uni_bold) ? 700:400;
	rq.style = (type&_uni_italic) ? fs_italic : 0;
	fv->fontset[type] = GDrawInstanciateFont(GDrawGetDisplayOfWindow(fv->v),&rq);
    }
return( fv->fontset[type] );
}

extern unichar_t adobes_pua_alts[0x200][3];

static void do_Adobe_Pua(unichar_t *buf,int sob,int uni) {
    int i, j;

    for ( i=j=0; j<sob-1 && i<3; ++i ) {
	int ch = adobes_pua_alts[uni-0xf600][i];
	if ( ch==0 )
    break;
	if ( ch>=0xf600 && ch<=0xf7ff && adobes_pua_alts[ch-0xf600]!=0 ) {
	    do_Adobe_Pua(buf+j,sob-j,ch);
	    while ( buf[j]!=0 ) ++j;
	} else
	    buf[j++] = ch;
    }
    buf[j] = 0;
}

static void FVExpose(FontView *fv,GWindow pixmap,GEvent *event) {
    int i, j, width, gid;
    int changed;
    GRect old, old2, box, r;
    GClut clut;
    struct _GImage base;
    GImage gi;
    SplineChar dummy;
    int styles, laststyles=0;
    GImage *rotated=NULL;
    int em = fv->sf->ascent+fv->sf->descent;
    int yorg = fv->magnify*(fv->show->ascent-fv->sf->vertical_origin*fv->show->pixelsize/em);
    Color bg;

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

    GDrawSetFont(pixmap,fv->fontset[0]);
    GDrawSetLineWidth(pixmap,0);
    GDrawPushClip(pixmap,&event->u.expose.rect,&old);
    GDrawFillRect(pixmap,NULL,GDrawGetDefaultBackground(NULL));
    for ( i=0; i<=fv->rowcnt; ++i ) {
	GDrawDrawLine(pixmap,0,i*fv->cbh,fv->width,i*fv->cbh,0);
	GDrawDrawLine(pixmap,0,i*fv->cbh+fv->lab_height,fv->width,i*fv->cbh+fv->lab_height,0x808080);
    }
    for ( i=0; i<=fv->colcnt; ++i )
	GDrawDrawLine(pixmap,i*fv->cbw,0,i*fv->cbw,fv->height,0);
    for ( i=event->u.expose.rect.y/fv->cbh; i<=fv->rowcnt && 
	    (event->u.expose.rect.y+event->u.expose.rect.height+fv->cbh-1)/fv->cbh; ++i ) for ( j=0; j<fv->colcnt; ++j ) {
	int index = (i+fv->rowoff)*fv->colcnt+j;
	int feat_index;
	styles = 0;
	if ( fv->mapping!=NULL ) {
	    if ( index>=fv->mapcnt ) index = fv->map->enccount;
	    else
		index = fv->mapping[index];
	}
	if ( index < fv->map->enccount && index!=-1 ) {
	    SplineChar *sc = (gid=fv->map->map[index])!=-1 ? fv->sf->glyphs[gid]: NULL;
	    unichar_t buf[60]; char cbuf[8];
	    Color fg;
	    FontMods *mods=NULL;
	    extern const int amspua[];
	    int uni;
	    struct cidmap *cidmap = NULL;

	    if ( fv->cidmaster!=NULL )
		cidmap = FindCidMap(fv->cidmaster->cidregistry,fv->cidmaster->ordering,fv->cidmaster->supplement,fv->cidmaster);

	    if ( ( fv->map->enc==&custom && index<256 ) ||
		 ( fv->map->enc!=&custom && index<fv->map->enc->char_cnt ) ||
		 ( cidmap!=NULL && index<MaxCID(cidmap) ))
		fg = 0x000000;
	    else
		fg = 0x505050;
	    if ( sc==NULL )
		sc = SCBuildDummy(&dummy,fv->sf,fv->map,index);
	    uni = sc->unicodeenc;
	    buf[0] = buf[1] = 0;
	    if ( fv->sf->uni_interp==ui_ams && uni>=0xe000 && uni<=0xf8ff &&
		    amspua[uni-0xe000]!=0 )
		uni = amspua[uni-0xe000];
	    switch ( fv->glyphlabel ) {
	      case gl_name:
		uc_strncpy(buf,sc->name,sizeof(buf)/sizeof(buf[0]));
		styles = _uni_sans;
	      break;
	      case gl_unicode:
		if ( sc->unicodeenc!=-1 ) {
		    sprintf(cbuf,"%04x",sc->unicodeenc);
		    uc_strcpy(buf,cbuf);
		} else
		    uc_strcpy(buf,"?");
		styles = _uni_sans;
	      break;
	      case gl_encoding:
		if ( fv->map->enc->only_1byte ||
			(fv->map->enc->has_1byte && index<256))
		    sprintf(cbuf,"%02x",index);
		else
		    sprintf(cbuf,"%04x",index);
		uc_strcpy(buf,cbuf);
		styles = _uni_sans;
	      break;
	      case gl_glyph:
		if ( uni==0xad )
		    buf[0] = '-';
#if 0
		else if ( Use2ByteEnc(fv,sc,buf,&for_charset))
		    mods = &for_charset;
#endif
		else if ( fv->sf->uni_interp==ui_adobe && uni>=0xf600 && uni<=0xf7ff &&
			adobes_pua_alts[uni-0xf600]!=0 )
		    do_Adobe_Pua(buf,sizeof(buf),uni);
		else if ( uni!=-1 && uni<65536 )
		    buf[0] = uni;
		else if ( uni>=0x1d400 && uni<=0x1d7ff ) {
		    int i;
		    for ( i=0; mathmap[i].start!=0; ++i ) {
			if ( uni<=mathmap[i].last ) {
			    buf[0] = maps[mathmap[i].charset][uni-mathmap[i].start];
			    styles = mathmap[i].styles;
		    break;
			}
		    }
		} else if ( uni>=0xe0020 && uni<=0xe0007e ) {
		    buf[0] = uni-0xe0000;	/* A map of Ascii for language names */
#if HANYANG
		} else if ( sc->compositionunit ) {
		    if ( sc->jamo<19 )
			buf[0] = 0x1100+sc->jamo;
		    else if ( sc->jamo<19+21 )
			buf[0] = 0x1161 + sc->jamo-19;
		    else	/* Leave a hole for the blank char */
			buf[0] = 0x11a8 + sc->jamo-(19+21+1);
#endif
		} else {
		    char *pt = strchr(sc->name,'.');
		    buf[0] = '?';
		    fg = 0xff0000;
		    if ( pt!=NULL ) {
			int i, n = pt-sc->name;
			char *end;
			SplineFont *cm = fv->sf->cidmaster;
			if ( n==7 && sc->name[0]=='u' && sc->name[1]=='n' && sc->name[2]=='i' &&
				(i=strtol(sc->name+3,&end,16), end-sc->name==7))
			    buf[0] = i;
			else if ( n>=5 && n<=7 && sc->name[0]=='u' &&
				(i=strtol(sc->name+1,&end,16), end-sc->name==n))
			    buf[0] = i;
			else if ( cm!=NULL && (i=CIDFromName(sc->name,cm))!=-1 ) {
			    int uni;
			    uni = CID2Uni(FindCidMap(cm->cidregistry,cm->ordering,cm->supplement,cm),
				    i);
			    if ( uni!=-1 )
				buf[0] = uni;
			} else for ( i=0; i<psunicodenames_cnt; ++i )
			    if ( psunicodenames[i]!=NULL && strncmp(sc->name,psunicodenames[i],n)==0 &&
				    psunicodenames[i][n]=='\0' ) {
				buf[0] = i;
			break;
			    }
			if ( strstr(pt,".vert")!=NULL )
			    rotated = UniGetRotatedGlyph(fv,sc,buf[0]!='?'?buf[0]:-1);
			if ( buf[0]!='?' ) {
			    fg = 0;
			    if ( strstr(pt,".italic")!=NULL )
				styles = _uni_italic|_uni_mono;
			}
		    } else if ( strncmp(sc->name,"hwuni",5)==0 ) {
			int uni=-1;
			sscanf(sc->name,"hwuni%x", (unsigned *) &uni );
			if ( uni!=-1 ) buf[0] = uni;
		    } else if ( strncmp(sc->name,"italicuni",9)==0 ) {
			int uni=-1;
			sscanf(sc->name,"italicuni%x", (unsigned *) &uni );
			if ( uni!=-1 ) { buf[0] = uni; styles=_uni_italic|_uni_mono; }
			fg = 0x000000;
		    } else if ( strncmp(sc->name,"vertcid_",8)==0 ||
			    strncmp(sc->name,"vertuni",7)==0 ) {
			rotated = UniGetRotatedGlyph(fv,sc,-1);
		    }
		}
	      break;
	    }
	    bg = COLOR_DEFAULT;
	    r.x = j*fv->cbw+1; r.width = fv->cbw-1;
	    r.y = i*fv->cbh+1; r.height = fv->lab_height-1;
	    if ( sc->layers[ly_back].splines!=NULL || sc->layers[ly_back].images!=NULL ||
		    sc->color!=COLOR_DEFAULT ) {
		bg = sc->color!=COLOR_DEFAULT?sc->color:0x808080;
		GDrawFillRect(pixmap,&r,bg);
	    }
	    if ( sc->changedsincelasthinted && !sc->changed && !fv->sf->order2 ) {
		Color hintcol = 0x0000ff;
		GDrawDrawLine(pixmap,r.x,r.y,r.x,r.y+r.height-1,hintcol);
		GDrawDrawLine(pixmap,r.x+1,r.y,r.x+1,r.y+r.height-1,hintcol);
		GDrawDrawLine(pixmap,r.x+r.width-1,r.y,r.x+r.width-1,r.y+r.height-1,hintcol);
		GDrawDrawLine(pixmap,r.x+r.width-2,r.y,r.x+r.width-2,r.y+r.height-1,hintcol);
	    }
	    if ( rotated!=NULL ) {
		GDrawPushClip(pixmap,&r,&old2);
		GDrawDrawImage(pixmap,rotated,NULL,j*fv->cbw+2,i*fv->cbh+2);
		GDrawPopClip(pixmap,&old2);
		GImageDestroy(rotated);
		rotated = NULL;
	    } else {
		if ( styles!=laststyles ) GDrawSetFont(pixmap,FVCheckFont(fv,styles));
		width = GDrawGetTextWidth(pixmap,buf,-1,mods);
		if ( width >= fv->cbw-1 ) {
		    GDrawPushClip(pixmap,&r,&old2);
		    width = fv->cbw-1;
		}
		if ( sc->unicodeenc<0x80 || sc->unicodeenc>=0xa0 )
		    GDrawDrawText(pixmap,j*fv->cbw+(fv->cbw-1-width)/2,i*fv->cbh+fv->lab_height-2,buf,-1,mods,fg);
		if ( width >= fv->cbw-1 )
		    GDrawPopClip(pixmap,&old2);
		laststyles = styles;
	    }
	    changed = sc->changed;
	    if ( fv->sf->onlybitmaps )
		changed = gid==-1 || fv->show->glyphs[gid]==NULL? false : fv->show->glyphs[gid]->changed;
	    if ( changed ) {
		GRect r;
		r.x = j*fv->cbw+1; r.width = fv->cbw-1;
		r.y = i*fv->cbh+1; r.height = fv->lab_height-1;
		if ( bg == COLOR_DEFAULT )
		    bg = GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(fv->v));
		GDrawSetXORBase(pixmap,bg);
		GDrawSetXORMode(pixmap);
		GDrawFillRect(pixmap,&r,0x000000);
		GDrawSetCopyMode(pixmap);
	    }
	}

	feat_index = FeatureTrans(fv,index);
	if ( feat_index < fv->map->enccount && feat_index!=-1 ) {
	    SplineChar *sc = (gid=fv->map->map[feat_index])!=-1 ? fv->sf->glyphs[gid]: NULL;
	    BDFChar *bdfc;

	    if ( sc==NULL )
		sc = SCBuildDummy(&dummy,fv->sf,fv->map,feat_index);

	    if ( fv->show!=NULL && fv->show->piecemeal &&
		    gid!=-1 && fv->show->glyphs[gid]==NULL &&
		    fv->sf->glyphs[gid]!=NULL )
		BDFPieceMeal(fv->show,gid);

	    if ( gid==-1 || !SCWorthOutputting(fv->sf->glyphs[gid]) ) {
		int x = j*fv->cbw+1, xend = x+fv->cbw-2;
		int y = i*fv->cbh+14+2, yend = y+fv->cbw-1;
		GDrawDrawLine(pixmap,x,y,xend,yend,0xd08080);
		GDrawDrawLine(pixmap,x,yend,xend,y,0xd08080);
	    }
	    if ( fv->show!=NULL && gid!=-1 &&
		    gid < fv->show->glyphcnt &&
		    fv->show->glyphs[gid]==NULL &&
		    SCWorthOutputting(fv->sf->glyphs[gid]) ) {
		/* If we have an outline but no bitmap for this slot */
		box.x = j*fv->cbw+1; box.width = fv->cbw-2;
		box.y = i*fv->cbh+14+2; box.height = box.width+1;
		GDrawDrawRect(pixmap,&box,0xff0000);
		++box.x; ++box.y; box.width -= 2; box.height -= 2;
		GDrawDrawRect(pixmap,&box,0xff0000);
/* When reencoding a font we can find times where index>=show->charcnt */
	    } else if ( fv->show!=NULL && gid<fv->show->glyphcnt && gid!=-1 &&
		    fv->show->glyphs[gid]!=NULL ) {
		bdfc = fv->show->glyphs[gid];
		base.data = bdfc->bitmap;
		base.bytes_per_line = bdfc->bytes_per_line;
		base.width = bdfc->xmax-bdfc->xmin+1;
		base.height = bdfc->ymax-bdfc->ymin+1;
		box.x = j*fv->cbw; box.width = fv->cbw;
		box.y = i*fv->cbh+14+1; box.height = box.width+1;
		GDrawPushClip(pixmap,&box,&old2);
		if ( !fv->sf->onlybitmaps && fv->show!=fv->filled &&
			sc->layers[ly_fore].splines==NULL && sc->layers[ly_fore].refs==NULL &&
			!sc->widthset &&
			!(bdfc->xmax<=0 && bdfc->xmin==0 && bdfc->ymax<=0 && bdfc->ymax==0) ) {
		    /* If we have a bitmap but no outline character... */
		    GRect b;
		    b.x = box.x+1; b.y = box.y+1; b.width = box.width-2; b.height = box.height-2;
		    GDrawDrawRect(pixmap,&b,0x008000);
		    ++b.x; ++b.y; b.width -= 2; b.height -= 2;
		    GDrawDrawRect(pixmap,&b,0x008000);
		}
		/* I assume that the bitmap image matches the bounding*/
		/*  box. In some bitmap fonts the bitmap has white space on the*/
		/*  right. This can throw off the centering algorithem */
		if ( fv->magnify>1 ) {
		    GDrawDrawImageMagnified(pixmap,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2,
			    i*fv->cbh+fv->lab_height+1+fv->magnify*(fv->show->ascent-bdfc->ymax),
			    fv->magnify*base.width,fv->magnify*base.height);
		} else
		    GDrawDrawImage(pixmap,&gi,NULL,
			    j*fv->cbw+(fv->cbw-1-base.width)/2,
			    i*fv->cbh+fv->lab_height+1+fv->show->ascent-bdfc->ymax);
		if ( fv->showhmetrics ) {
		    int x1, x0 = j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2- bdfc->xmin*fv->magnify;
		    /* Draw advance width & horizontal origin */
		    if ( fv->showhmetrics&fvm_origin )
			GDrawDrawLine(pixmap,x0,i*fv->cbh+fv->lab_height+yorg-3,x0,
				i*fv->cbh+fv->lab_height+yorg+2,METRICS_ORIGIN);
		    x1 = x0 + bdfc->width;
		    if ( fv->showhmetrics&fvm_advanceat )
			GDrawDrawLine(pixmap,x1,i*fv->cbh+fv->lab_height+1,x1,
				(i+1)*fv->cbh-1,METRICS_ADVANCE);
		    if ( fv->showhmetrics&fvm_advanceto )
			GDrawDrawLine(pixmap,x0,(i+1)*fv->cbh-2,x1,
				(i+1)*fv->cbh-2,METRICS_ADVANCE);
		}
		if ( fv->showvmetrics ) {
		    int x0 = j*fv->cbw+(fv->cbw-1-fv->magnify*base.width)/2- bdfc->xmin*fv->magnify
			    + fv->magnify*fv->show->pixelsize/2;
		    int y0 = i*fv->cbh+fv->lab_height+yorg;
		    int yvw = y0 + fv->magnify*sc->vwidth*fv->show->pixelsize/em;
		    if ( fv->showvmetrics&fvm_baseline )
			GDrawDrawLine(pixmap,x0,i*fv->cbh+fv->lab_height+1,x0,
				(i+1)*fv->cbh-1,METRICS_BASELINE);
		    if ( fv->showvmetrics&fvm_advanceat )
			GDrawDrawLine(pixmap,j*fv->cbw,yvw,(j+1)*fv->cbw,
				yvw,METRICS_ADVANCE);
		    if ( fv->showvmetrics&fvm_advanceto )
			GDrawDrawLine(pixmap,j*fv->cbw+2,y0,j*fv->cbw+2,
				yvw,METRICS_ADVANCE);
		    if ( fv->showvmetrics&fvm_origin )
			GDrawDrawLine(pixmap,x0-3,i*fv->cbh+fv->lab_height+yorg,x0+2,i*fv->cbh+fv->lab_height+yorg,METRICS_ORIGIN);
		}
		GDrawPopClip(pixmap,&old2);
	    }
	    if ( fv->selected[index] ) {
		box.x = j*fv->cbw+1; box.width = fv->cbw-1;
		box.y = i*fv->cbh+fv->lab_height+1; box.height = fv->cbw;
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
	    GDrawDrawLine(pixmap,0,i*fv->cbh+fv->lab_height+baseline,fv->width,i*fv->cbh+fv->lab_height+baseline,METRICS_BASELINE);
    }
    GDrawPopClip(pixmap,&old);
    GDrawSetDither(NULL, true);
}

static char *chosung[] = { "G", "GG", "N", "D", "DD", "L", "M", "B", "BB", "S", "SS", "", "J", "JJ", "C", "K", "T", "P", "H", NULL };
static char *jungsung[] = { "A", "AE", "YA", "YAE", "EO", "E", "YEO", "YE", "O", "WA", "WAE", "OE", "YO", "U", "WEO", "WE", "WI", "YU", "EU", "YI", "I", NULL };
static char *jongsung[] = { "", "G", "GG", "GS", "N", "NJ", "NH", "D", "L", "LG", "LM", "LB", "LS", "LT", "LP", "LH", "M", "B", "BS", "S", "SS", "NG", "J", "C", "K", "T", "P", "H", NULL };

static void FVDrawInfo(FontView *fv,GWindow pixmap,GEvent *event) {
    GRect old, r;
    char buffer[250], *pt;
    unichar_t ubuffer[250];
    Color bg = GDrawGetDefaultBackground(GDrawGetDisplayOfWindow(pixmap));
    SplineChar *sc, dummy;
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    int gid;
    int uni;
    Color fg = 0xff0000;
    int ulen, tlen;

    if ( event->u.expose.rect.y+event->u.expose.rect.height<=fv->mbh )
return;

    GDrawSetFont(pixmap,fv->fontset[0]);
    GDrawPushClip(pixmap,&event->u.expose.rect,&old);

    r.x = 0; r.width = fv->width; r.y = fv->mbh; r.height = fv->infoh;
    GDrawFillRect(pixmap,&r,bg);
    if ( fv->end_pos>=map->enccount || fv->pressed_pos>=map->enccount ||
	    fv->end_pos<0 || fv->pressed_pos<0 )
	fv->end_pos = fv->pressed_pos = -1;	/* Can happen after reencoding */
    if ( fv->end_pos == -1 )
return;

    if ( map->remap!=NULL ) {
	int localenc = fv->end_pos;
	struct remap *remap = map->remap;
	while ( remap->infont!=-1 ) {
	    if ( localenc>=remap->infont && localenc<=remap->infont+(remap->lastenc-remap->firstenc) ) {
		localenc += remap->firstenc-remap->infont;
	break;
	    }
	    ++remap;
	}
	sprintf( buffer, "%-5u (0x%04x) ", localenc, localenc );
    } else if ( map->enc->only_1byte ||
	    (map->enc->has_1byte && fv->end_pos<256))
	sprintf( buffer, "%-3d (0x%02x) ", fv->end_pos, fv->end_pos );
    else
	sprintf( buffer, "%-5d (0x%04x) ", fv->end_pos, fv->end_pos );
    sc = (gid=fv->map->map[fv->end_pos])!=-1 ? sf->glyphs[gid] : NULL;
    if ( sc==NULL )
	sc = SCBuildDummy(&dummy,sf,fv->map,fv->end_pos);
    if ( sc->unicodeenc!=-1 )
	sprintf( buffer+strlen(buffer), "U+%04X", sc->unicodeenc );
    else
	sprintf( buffer+strlen(buffer), "U+????" );
    sprintf( buffer+strlen(buffer), "  %.*s", (int) (sizeof(buffer)-strlen(buffer)-1),
	    sc->name );

    strcat(buffer,"  ");
    uc_strcpy(ubuffer,buffer);
    ulen = u_strlen(ubuffer);

    uni = sc->unicodeenc;
    if ( uni==-1 && (pt=strchr(sc->name,'.'))!=NULL && pt-sc->name<30 ) {
	strncpy(buffer,sc->name,pt-sc->name);
	buffer[(pt-sc->name)] = '\0';
	uni = UniFromName(buffer,fv->sf->uni_interp,map->enc);
	if ( uni!=-1 ) {
	    sprintf( buffer, "U+%04X ", uni );
	    uc_strcat(ubuffer,buffer);
	}
	fg = 0x707070;
    }
    if ( uni!=-1 && uni<0x110000 && _UnicodeNameAnnot!=NULL &&
	    _UnicodeNameAnnot[uni>>16][(uni>>8)&0xff][uni&0xff].name!=NULL ) {
	uc_strncat(ubuffer, _UnicodeNameAnnot[uni>>16][(uni>>8)&0xff][uni&0xff].name, 80);
    } else if ( uni>=0xAC00 && uni<=0xD7A3 ) {
	sprintf( buffer, "Hangul Syllable %s%s%s",
		chosung[(uni-0xAC00)/(21*28)],
		jungsung[(uni-0xAC00)/28%21],
		jongsung[(uni-0xAC00)%28] );
	uc_strncat(ubuffer,buffer,80);
    } else if ( uni!=-1 ) {
	uc_strncat(ubuffer, UnicodeRange(uni),80);
    }

    tlen = GDrawDrawText(pixmap,10,fv->mbh+11,ubuffer,ulen,NULL,0xff0000);
    GDrawDrawText(pixmap,10+tlen,fv->mbh+11,ubuffer+ulen,-1,NULL,fg);
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
    int i,pos, cnt, gid;

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
	    if ( pos<0 ) pos = fv->map->enccount-1;
	    else if ( pos>= fv->map->enccount ) pos = 0;
	    if ( pos>=0 && pos<fv->map->enccount )
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
		if ( pos>=fv->map->enccount ) pos = 0;
		else if ( pos<0 ) pos = fv->map->enccount-1;
	    } while ( pos!=end_pos &&
		    ((gid=fv->map->map[pos])==-1 || !SCWorthOutputting(fv->sf->glyphs[gid])));
	    if ( pos==end_pos ) ++pos;
	    if ( pos>=fv->map->enccount ) pos = 0;
	  break;
#if GK_Tab!=GK_BackTab
	  case GK_BackTab:
	    pos = end_pos;
	    do {
		--pos;
		if ( pos<0 ) pos = fv->map->enccount-1;
	    } while ( pos!=end_pos &&
		    ((gid=fv->map->map[pos])==-1 || !SCWorthOutputting(fv->sf->glyphs[gid])));
	    if ( pos==end_pos ) --pos;
	    if ( pos<0 ) pos = 0;
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
	    pos = fv->map->enccount;
	  break;
	  case GK_Home: case GK_KP_Home:
	    pos = 0;
	    if ( fv->sf->top_enc!=-1 && fv->sf->top_enc<fv->map->enccount )
		pos = fv->sf->top_enc;
	    else {
		pos = SFFindSlot(fv->sf,fv->map,'A',NULL);
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
	if ( pos>=fv->map->enccount ) pos = fv->map->enccount-1;
	if ( event->u.chr.state&ksm_shift && event->u.chr.keysym!=GK_Tab && event->u.chr.keysym!=GK_BackTab ) {
	    FVReselect(fv,pos);
	} else {
	    FVDeselectAll(fv);
	    fv->selected[pos] = true;
	    FVToggleCharSelected(fv,pos);
	    fv->pressed_pos = pos;
	    fv->sel_index = 1;
	}
	fv->end_pos = pos;
	FVShowInfo(fv);
	FVScrollToChar(fv,pos);
    } else if ( event->u.chr.keysym == GK_Help ) {
	MenuHelp(NULL,NULL,NULL);	/* Menu does F1 */
    } else if ( event->u.chr.keysym == GK_Escape ) {
	FVDeselectAll(fv);
    } else if ( event->u.chr.chars[0]=='\r' || event->u.chr.chars[0]=='\n' ) {
	for ( i=cnt=0; i<fv->map->enccount && cnt<10; ++i ) if ( fv->selected[i] ) {
	    SplineChar *sc = SFMakeChar(fv->sf,fv->map,i);
	    if ( fv->show==fv->filled ) {
		CharViewCreate(sc,fv,i);
	    } else {
		BDFFont *bdf = fv->show;
		BitmapViewCreate(BDFMakeGID(bdf,sc->orig_pos),bdf,fv,i);
	    }
	    ++cnt;
	}
    } else if ( event->u.chr.chars[0]<=' ' || event->u.chr.chars[1]!='\0' ) {
	/* Do Nothing */;
    } else {
	SplineFont *sf = fv->sf;
	for ( i=0; i<sf->glyphcnt; ++i ) {
	    if ( sf->glyphs[i]!=NULL )
		if ( sf->glyphs[i]->unicodeenc==event->u.chr.chars[0] )
	break;
	}
	if ( i==sf->glyphcnt ) {
	    for ( i=0; i<fv->map->enccount; ++i ) if ( fv->map->map[i]==-1 ) {
		SplineChar dummy;
		SCBuildDummy(&dummy,sf,fv->map,i);
		if ( dummy.unicodeenc==event->u.chr.chars[0] )
	    break;
	    }
	} else
	    i = fv->map->backmap[i];
	if ( i!=fv->map->enccount && i!=-1 )
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

void SCPreparePopup(GWindow gw,SplineChar *sc,struct remap *remap, int localenc) {
    static unichar_t space[810];
    char cspace[162];
    int upos=-1;
    int done = false;

    if ( remap!=NULL ) {
	while ( remap->infont!=-1 ) {
	    if ( localenc>=remap->infont && localenc<=remap->infont+(remap->lastenc-remap->firstenc) ) {
		localenc += remap->firstenc-remap->infont;
	break;
	    }
	    ++remap;
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
    int i,j,cnt, gid;
    FontView *fv = (FontView *) _fv;
    SplineFont *sf = fv->sf;
    EncMap *map = fv->map;
    char *data;

    for ( i=cnt=0; i<map->enccount; ++i ) if ( fv->selected[i] && (gid=map->map[i])!=-1 && sf->glyphs[gid]!=NULL )
	cnt += strlen(sf->glyphs[gid]->name)+1;
    data = galloc(cnt+1); data[0] = '\0';
    for ( cnt=0, j=1 ; j<=fv->sel_index; ++j ) {
	for ( i=cnt=0; i<map->enccount; ++i )
	    if ( fv->selected[i] && (gid=map->map[i])!=-1 && sf->glyphs[gid]!=NULL ) {
		strcpy(data+cnt,sf->glyphs[gid]->name);
		cnt += strlen(sf->glyphs[gid]->name);
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
    int gid;
    int realpos = pos;
    SplineChar *sc, dummy;
    int dopopup = true;

    if ( event->type==et_mousedown )
	CVPaletteDeactivate();
    if ( pos<0 ) {
	pos = 0;
	dopopup = false;
    } else if ( pos>=fv->map->enccount ) {
	pos = fv->map->enccount-1;
	if ( pos<0 )		/* No glyph slots in font */
return;
	dopopup = false;
    }

    sc = (gid=fv->map->map[pos])!=-1 ? fv->sf->glyphs[gid] : NULL;
    if ( sc==NULL )
	sc = SCBuildDummy(&dummy,fv->sf,fv->map,pos);
    if ( event->type == et_mouseup && event->u.mouse.clicks==2 ) {
	if ( fv->cur_feat_tag!=0 ) {
	    sc = FVMakeChar(fv,pos);
	    pos = fv->map->backmap[sc->orig_pos];
	}
	if ( sc==&dummy ) {
	    sc = SFMakeChar(fv->sf,fv->map,pos);
	    gid = fv->map->map[pos];
	}
	if ( fv->show==fv->filled ) {
	    CharViewCreate(sc,fv,pos);
	} else {
	    BDFFont *bdf = fv->show;
	    BitmapViewCreate(BDFMakeGID(bdf,gid),bdf,fv,pos);
	}
    } else if ( event->type == et_mousemove ) {
	if ( dopopup )
	    SCPreparePopup(fv->v,sc,fv->map->remap,pos);
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
	int showit = realpos!=fv->end_pos;
	FVReselect(fv,realpos);
	if ( showit )
	    FVShowInfo(fv);
	if ( event->type==et_mouseup ) {
	    GDrawCancelTimer(fv->pressed);
	    fv->pressed = NULL;
	}
    }
    if ( event->type==et_mouseup && dopopup )
	SCPreparePopup(fv->v,sc,fv->map->remap,pos);
    if ( event->type==et_mouseup )
	SVAttachFV(fv,2);
}

#if defined(FONTFORGE_CONFIG_GTK)
gboolean FontView_ViewportResize(GtkWidget *widget, GdkEventConfigure *event,
	gpointer user_data) {
    extern int default_fv_row_count, default_fv_col_count;
    int cc, rc;
    FontView *fv = (FontView *) g_object_get_data( widget, "data" );

    cc = (event->width-1)/fv->cbw; rc = (event->height-1)/fv->cbh;
    if ( cc==0 ) cc = 1; if ( rc==0 ) rc = 1;

    if ( event->width==fv->width || event->height==fv->height )
	/* Ok, the window manager didn't change the size. */
    else if ( event->width!=cc*fv->cbw+1 || event->height!=rc*fv->cbh+1 ) {
	gtk_widget_set_size_request( widget, cc*fv->cbw+1, rc*fv->cbh+1);
return TRUE;
    }
    gtk_widget_set_size_request( lookup_widget(widget,"view"), 1, 1 );
    fv->width = width; fv->height = height;

    default_fv_row_count = rc;
    default_fv_col_count = cc;
    SavePrefs();

return TRUE;
}
#elif defined(FONTFORGE_CONFIG_GDRAW)
static void FVResize(FontView *fv,GEvent *event) {
    extern int default_fv_row_count, default_fv_col_count;
    GRect pos,screensize;
    int topchar;

    if ( fv->colcnt!=0 )
	topchar = fv->rowoff*fv->colcnt;
    else if ( fv->sf->top_enc!=-1 && fv->sf->top_enc<fv->map->enccount )
	topchar = fv->sf->top_enc;
    else {
	/* Position on 'A' if it exists */
	topchar = SFFindSlot(fv->sf,fv->map,'A',NULL);
	if ( topchar==-1 ) topchar = 0;
    }
    if ( !event->u.resize.sized )
	/* WM isn't responding to my resize requests, so no point in trying */;
    else if ( (event->u.resize.size.width-
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
    fv->rowltot = (fv->map->enccount+fv->colcnt-1)/fv->colcnt;

    GScrollBarSetBounds(fv->vsb,0,fv->rowltot,fv->rowcnt);
    fv->rowoff = topchar/fv->colcnt;
    if ( fv->rowoff>=fv->rowltot-fv->rowcnt )
        fv->rowoff = fv->rowltot-fv->rowcnt-1;
    if ( fv->rowoff<0 ) fv->rowoff =0;
    GScrollBarSetPos(fv->vsb,fv->rowoff);
    GDrawRequestExpose(fv->gw,NULL,true);
    GDrawRequestExpose(fv->v,NULL,true);

    default_fv_row_count = fv->rowcnt;
    default_fv_col_count = fv->colcnt;
    SavePrefs();
}
#endif

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

    GGadgetPopupExternalEvent(event);
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
    fv->rowltot = (fv->map->enccount+fv->colcnt-1)/fv->colcnt;
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
	fv->rowltot = (fv->map->enccount+fv->colcnt-1)/fv->colcnt;
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
	if ( event->u.resize.sized || fv->resize_expected ) {
	    static GEvent temp;
	    if ( fv->resize )
		GDrawCancelTimer(fv->resize);
	    temp = *event;
	    fv->resize = GDrawRequestTimer(fv->v,300,0,(void *) &temp);
	    fv->resize_expected = false;
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
	for ( i=0; i<_sf->glyphcnt; ++i )
	    if ( _sf->glyphs[i]!=NULL && _sf->glyphs[i]->wasopen ) {
		_sf->glyphs[i]->wasopen = false;
		CharViewCreate(_sf->glyphs[i],fv,-1);
	    }
	++k;
    } while ( k<sf->subfontcnt );
}
#endif

FontView *_FontViewCreate(SplineFont *sf) {
    FontView *fv = gcalloc(1,sizeof(FontView));
    int i;
    int ps = sf->display_size<0 ? -sf->display_size :
	     sf->display_size==0 ? default_fv_font_size : sf->display_size;

    if ( ps>200 ) ps = 128;

    fv->nextsame = sf->fv;
    sf->fv = fv;
    if ( sf->mm!=NULL ) {
	sf->mm->normal->fv = fv;
	for ( i = 0; i<sf->mm->instance_count; ++i )
	    sf->mm->instances[i]->fv = fv;
    }
    if ( sf->subfontcnt==0 ) {
	fv->sf = sf;
	fv->map = fv->nextsame==NULL ? sf->map : EncMapCopy(fv->nextsame->map);
    } else {
	fv->cidmaster = sf;
	for ( i=0; i<sf->subfontcnt; ++i )
	    sf->subfonts[i]->fv = fv;
	fv->sf = sf = sf->subfonts[0];
	if ( fv->nextsame==NULL ) EncMapFree(sf->map);
	fv->map = EncMap1to1(sf->glyphcnt);
    }
    fv->selected = gcalloc(fv->map->enccount,sizeof(char));
    fv->magnify = (ps<=9)? 3 : (ps<20) ? 2 : 1;
    fv->cbw = (ps*fv->magnify)+1;
    fv->cbh = (ps*fv->magnify)+1+fv->lab_height+1;
    fv->antialias = sf->display_antialias;
    fv->bbsized = sf->display_bbsized;
    fv->glyphlabel = default_fv_glyphlabel;

    fv->end_pos = -1;
return( fv );
}

FontView *FontViewCreate(SplineFont *sf) {
    FontView *fv = _FontViewCreate(sf);
#if defined(FONTFORGE_CONFIG_GTK)
    GtkWidget *status;
    PangoContext *context;
    PangoFont *font;
    PangoFontMetrics *fm;
    int as, ds;
    BDFFont *bdf;

    fv->gw = create_FontView();
    g_object_set_data (G_OBJECT (fv->gw), "data", fv );
    fv->v = lookup_widget( fv->gw,"view" );
    status = lookup_widget( fv->gw,"status" );
    context = gtk_widget_get_pango_context( status );
    font = pango_context_load_font( context, pango_context_get_font_description(context));
    fm = pango_font_get_metrics(font,NULL);
    as = pango_font_metrics_get_ascent(fm);
    ds = pango_font_metrics_get_descent(fm);
    fv->infoh = (as+ds)/1024;
    gtk_widget_set_size_request(status,0,fv->infoh);

    context = gtk_widget_get_pango_context( fv->v );
    font = pango_context_load_font( context, pango_context_get_font_description(context));
    fm = pango_font_get_metrics(font,NULL);
    as = pango_font_metrics_get_ascent(fm);
    ds = pango_font_metrics_get_descent(fm);
    fv->lab_height = (as+ds)/1024;

    g_object_set_data( fv->gw, "data", fv );
    g_object_set_data( fv->v , "data", fv );

    {
	GdkGC *gc = fv->v->style->fg_gc[fv->v->state];
	GdkGCValues values;
	gdk_gc_get_values(gc,&values);
	default_background = ((values.background.red  &0xff00)<<8) |
			     ((values.background.green&0xff00)   ) |
			     ((values.background.blue &0xff00)>>8);
    }
#elif defined(FONTFORGE_CONFIG_GDRAW)
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetData gd;
    GGadget *sb;
    GRect gsize;
    FontRequest rq;
    /* sadly, clearlyu is too big for the space I've got */
    static unichar_t monospace[] = { 'c','o','u','r','i','e','r',',','m', 'o', 'n', 'o', 's', 'p', 'a', 'c', 'e',',','c','a','s','l','o','n',',','c','l','e','a','r','l','y','u',',','u','n','i','f','o','n','t',  '\0' };
    static unichar_t *fontnames=NULL;
    static GWindow icon = NULL;
    static int nexty=0;
    GRect size;
    BDFFont *bdf;

    if ( icon==NULL ) {
#ifdef BIGICONS
	icon = GDrawCreateBitmap(NULL,fontview_width,fontview_height,fontview_bits);
#else
	icon = GDrawCreateBitmap(NULL,fontview2_width,fontview2_height,fontview2_bits);
#endif
    }

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
    fv->lab_height = FV_LAB_HEIGHT;

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
    fv->fontset = gcalloc(_uni_fontmax,sizeof(GFont *));
    memset(&rq,0,sizeof(rq));
    rq.family_name = fontnames;
    rq.point_size = -13;
    rq.weight = 400;
    fv->fontset[0] = GDrawInstanciateFont(GDrawGetDisplayOfWindow(gw),&rq);
    GDrawSetFont(fv->v,fv->fontset[0]);
#endif
    fv->showhmetrics = default_fv_showhmetrics;
    fv->showvmetrics = default_fv_showvmetrics && sf->hasvmetrics;
#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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
#endif

#if defined(FONTFORGE_CONFIG_GTK)
    gtk_widget_show(fv->gw);
#elif defined(FONTFORGE_CONFIG_GDRAW)
    /*GWidgetHidePalettes();*/
    GDrawSetVisible(gw,true);
    FontViewOpenKids(fv);
#endif
return( fv );
}

static SplineFont *SFReadPostscript(char *filename) {
    FontDict *fd=NULL;
    SplineFont *sf=NULL;

# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressChangeStages(2);
    fd = ReadPSFont(filename);
    GProgressNextStage();
    GProgressChangeLine2R(_STR_InterpretingGlyphs);
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_change_stages(2);
    fd = ReadPSFont(filename);
    gwwv_progress_next_stage();
    gwwv_progress_change_line2(_("Interpreting Glyphs"));
# else
    fd = ReadPSFont(filename);
# endif
    if ( fd!=NULL ) {
	sf = SplineFontFromPSFont(fd);
	PSFontFree(fd);
	if ( sf!=NULL )
	    CheckAfmOfPostscript(sf,filename,sf->map);
    }
return( sf );
}

/* This does not check currently existing fontviews, and should only be used */
/*  by LoadSplineFont (which does) and by RevertFile (which knows what it's doing) */
SplineFont *ReadSplineFont(char *filename,enum openflags openflags) {
    SplineFont *sf;
# ifdef FONTFORGE_CONFIG_GDRAW
    unichar_t ubuf[150], *temp;
#elif defined(FONTFORGE_CONFIG_GTK)
    char buf[400];
#endif
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
    FILE *foo;

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
#if defined(FONTFORGE_CONFIG_GTK)
		gwwv_post_error(_("Decompress Failed"),_("Decompress Failed"));
#else
		GWidgetErrorR(_STR_DecompressFailed,_STR_DecompressFailed);
#endif
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

    /* If there are no pfaedit windows, give them something to look at */
    /*  immediately. Otherwise delay a bit */
#if defined(FONTFORGE_CONFIG_GDRAW)
    u_strcpy(ubuf,GStringGetResource(_STR_LoadingFontFrom,NULL));
    len = u_strlen(ubuf);
    u_strncat(ubuf,temp = def2u_copy(GFileNameTail(fullname)),100);
    free(temp);
    ubuf[100+len] = '\0';
    GProgressStartIndicator(fv_list==NULL?0:10,GStringGetResource(_STR_Loading,NULL),ubuf,GStringGetResource(_STR_ReadingGlyphs,NULL),0,1);
    GProgressEnableStop(0);
    if ( fv_list==NULL && !no_windowing_ui ) { GDrawSync(NULL); GDrawProcessPendingEvents(NULL); }
#elif defined(FONTFORGE_CONFIG_GTK)
    strcpy(ubuf,_("Loading font from "));
    len = u_strlen(buf);
    strncat(buf,GFileNameTail(fullname),100);
    buf[100+len] = '\0';
    gwwv_progress_start_indicator(fv_list==NULL?0:10,_("Loading..."),buf,_("Reading Glyphs"),NULL),0,1);
    gwwv_progress_enable_stop(0);
    /* Do something here to force an expose !!!! */
#else
    len = 0;
#endif

    sf = NULL;
    foo = fopen(strippedname,"rb");
    if ( foo!=NULL ) {
	/* Try to guess the file type from the first few characters... */
	int ch1 = getc(foo);
	int ch2 = getc(foo);
	int ch3 = getc(foo);
	int ch4 = getc(foo);
	int ch5 = getc(foo);
	int ch6 = getc(foo);
	int ch7 = getc(foo);
	int ch9, ch10;
	fseek(foo, 98, SEEK_SET);
	ch9 = getc(foo);
	ch10 = getc(foo);
	fclose(foo);
	if (( ch1==0 && ch2==1 && ch3==0 && ch4==0 ) ||
		(ch1=='O' && ch2=='T' && ch3=='T' && ch4=='O') ||
		(ch1=='t' && ch2=='r' && ch3=='u' && ch4=='e') ||
		(ch1=='t' && ch2=='t' && ch3=='c' && ch4=='f') ) {
	    sf = SFReadTTF(fullname,0);
	} else if (( ch1=='%' && ch2=='!' ) ||
		    ( ch1==0x80 && ch2=='\01' ) ) {	/* PFB header */
	    sf = SFReadPostscript(fullname);
	} else if ( ch1==1 && ch2==0 && ch3==4 ) {
	    sf = CFFParse(fullname);
	} else if ( ch1=='<' && ch2=='?' && (ch3=='x'||ch3=='X') && (ch4=='m'||ch4=='M') ) {
	    sf = SFReadSVG(fullname,0);
	} else if ( ch1==0xef && ch2==0xbb && ch3==0xbf &&
		ch4=='<' && ch5=='?' && (ch6=='x'||ch6=='X') && (ch7=='m'||ch7=='M') ) {
	    /* UTF-8 SVG with initial byte ordering mark */
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
	} else if ( ch9=='I' && ch10=='K' && ch3==0 && ch4==55 ) {
	    /* Ikarus font type appears at word 50 (byte offset 98) */
	    /* Ikarus name section length (at word 2, byte offset 2) was 55 in the 80s at URW */
	    sf = SFReadIkarus(fullname);
	} /* Too hard to figure out a valid mark for a mac resource file */
    }
    if ( sf!=NULL )
	/* good */;
    else if ( strmatch(fullname+strlen(fullname)-4, ".sfd")==0 ||
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
    } else if ( strmatch(fullname+strlen(strippedname)-4, ".pdb")==0 ) {
	sf = SFReadPalmPdb(fullname,0);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".pfa")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".pfb")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".pf3")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".cid")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".gsf")==0 ||
		strmatch(fullname+strlen(fullname)-4, ".pt3")==0 ||
		strmatch(fullname+strlen(fullname)-3, ".ps")==0 ) {
	sf = SFReadPostscript(fullname);
    } else if ( strmatch(fullname+strlen(fullname)-4, ".cff")==0 ) {
	sf = CFFParse(fullname);
    } else if ( strmatch(fullname+strlen(fullname)-3, ".mf")==0 ) {
	sf = SFFromMF(fullname);
    } else if ( strmatch(fullname+strlen(fullname)-3, ".ik")==0 ) {
	sf = SFReadIkarus(fullname);
    } else {
	sf = SFReadMacBinary(fullname,0);
    }
    if ( strippedname!=filename && strippedname!=tmpfile )
	free(strippedname);
    if ( fullname!=filename && fullname!=strippedname )
	free(fullname);
# ifdef FONTFORGE_CONFIG_GDRAW
    GProgressEndIndicator();
# elif defined(FONTFORGE_CONFIG_GTK)
    gwwv_progress_end_indicator();
# endif

    if ( sf!=NULL ) {
	SplineFont *norm = sf->mm!=NULL ? sf->mm->normal : sf;
	if ( sf->chosenname!=NULL && strippedname==filename ) {
	    norm->origname = galloc(strlen(filename)+strlen(sf->chosenname)+8);
	    strcpy(norm->origname,filename);
	    strcat(norm->origname,"(");
	    strcat(norm->origname,sf->chosenname);
	    strcat(norm->origname,")");
	} else
	    norm->origname = copy(filename);
	free( sf->chosenname ); sf->chosenname = NULL;
	if ( sf->mm!=NULL ) {
	    int j;
	    for ( j=0; j<sf->mm->instance_count; ++j ) {
		free(sf->mm->instances[j]->origname);
		sf->mm->instances[j]->origname = copy(norm->origname);
	    }
	}
    } else if ( !GFileExists(filename) )
#if defined(FONTFORGE_CONFIG_GTK)
	gwwv_post_error(_("Couldn't open font"),_("The requested file, %.100s, does not exist"),GFileNameTail(filename));
#else
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_NoSuchFontFile,GFileNameTail(filename));
#endif
    else if ( !GFileReadable(filename) )
#if defined(FONTFORGE_CONFIG_GTK)
	gwwv_post_error(_("Couldn't open font"),_("You do not have permission to read %.100s"),GFileNameTail(filename));
#else
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_FontFileNotReadable,GFileNameTail(filename));
#endif
    else
#if defined(FONTFORGE_CONFIG_GTK)
	gwwv_post_error(_("Couldn't open font"),_("%.100s is not in a known format (or is so badly corrupted as to be unreadable)"),GFileNameTail(filename));
#else
	GWidgetErrorR(_STR_CouldntOpenFontTitle,_STR_CouldntParseFont,GFileNameTail(filename));
#endif

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
#if defined(FONTFORGE_CONFIG_GDRAW)
	static int buts[] = { _STR_Yes, _STR_No, 0 };
	if ( GWidgetAskR(_STR_RestrictedFont,buts,1,1,_STR_RestrictedRightsFont)==1 ) {
#elif defined(FONTFORGE_CONFIG_GTK)
	static char *buts[] = { GTK_STOCK_YES, GTK_STOCK_NO, NULL };
	if ( gwwv_ask(_("Restricted Font"),buts,1,1,_("This font is marked with an FSType of 2 (Restricted\nLicense). That means it is not editable without the\npermission of the legal owner.\n\nDo you have such permission?"))==1 ) {
#else
	if ( true ) {		/* In a script, if they didn't explicitly say so, fail */
#endif
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
	if ( fv->sf->filename!=NULL && strcmp(fv->sf->filename,buffer)==0 )
return( fv->sf );
	else if ( fv->sf->origname!=NULL && strcmp(fv->sf->origname,buffer)==0 )
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
	FILE *test = fopen(filename,"rb");
	if ( test!=NULL ) {
#if 0
	    int ch1 = getc(test);
	    int ch2 = getc(test);
	    int ch3 = getc(test);
	    int ch4 = getc(test);
	    if ( ch1=='%' ) ok = true;
	    else if (( ch1==0 && ch2==1 && ch3==0 && ch4==0 ) ||
		    (  ch1==0 && ch2==2 && ch3==0 && ch4==0 ) ||
		    /* Windows 3.1 Chinese version used this version for some arphic fonts */
		    /* See discussion on freetype list, july 2004 */
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

#ifndef FONTFORGE_CONFIG_NO_WINDOWING_UI
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
#endif

void FontViewFree(FontView *fv) {
    int i;
    FontView *prev;
    FontView *fvs;

    if ( fv->sf == NULL )	/* Happens when usurping a font to put it into an MM */
	BDFFontFree(fv->filled);
    else if ( fv->nextsame==NULL && fv->sf->fv==fv ) {
	EncMapFree(fv->map);
	SplineFontFree(fv->cidmaster?fv->cidmaster:fv->sf);
	BDFFontFree(fv->filled);
    } else {
	EncMapFree(fv->map);
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
    free(fv->fontset);
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
	PasteIntoFV(fv,false,NULL);
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
	PasteIntoFV(fv,true,NULL);
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
#if 0		/* Not used any more */
      case 101:
	FVSimplify(fv,false);
      break;
#endif
      case 102:
	FVAddExtrema(fv);
      break;
      case 103:
	FVRound2Int(fv,1.0);
      break;
      case 104:
	FVOverlap(fv,over_intersect);
      break;
      case 105:
	FVOverlap(fv,over_findinter);
      break;

      case 200:
	FVAutoHint(fv);
      break;
      case 201:
	FVClearHints(fv);
      break;
      case 202:
	FVAutoInstr(fv);
      break;
      case 203:
	FVAutoHintSubs(fv);
      break;
      case 204:
	FVAutoCounter(fv);
      break;
      case 205:
	FVDontAutoHint(fv);
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
