/* Copyright (C) 2000-2004 by George Williams */
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
#include <ustring.h>
#include <utype.h>

extern char *coord_sep;

/* ********************************* Undoes ********************************* */

int maxundoes = 12;		/* -1 is infinite */

static uint8 *bmpcopy(uint8 *bitmap,int bytes_per_line, int lines) {
    uint8 *ret = galloc(bytes_per_line*lines);
    memcpy(ret,bitmap,bytes_per_line*lines);
return( ret );
}

static RefChar *RefCharsCopyState(SplineChar *sc,int layer) {
    RefChar *head=NULL, *last=NULL, *new, *crefs;

    if ( layer<0 || sc->layers[layer].refs==NULL )
return( NULL );
    for ( crefs = sc->layers[layer].refs; crefs!=NULL; crefs=crefs->next ) {
	new = RefCharCreate();
	*new = *crefs;
	new->layers[0].splines = NULL;
	new->next = NULL;
	if ( last==NULL )
	    head = last = new;
	else {
	    last->next = new;
	    last = new;
	}
    }
return( head );
}

static MinimumDistance *MDsCopyState(SplineChar *sc,SplineSet *rpl) {
    MinimumDistance *head=NULL, *last, *md, *cur;

    if ( sc->md==NULL )
return( NULL );

    for ( md = sc->md; md!=NULL; md=md->next ) {
	cur = chunkalloc(sizeof(MinimumDistance));
	*cur = *md;
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
    cur->next = NULL;

    MDReplace(head,sc->layers[ly_fore].splines,rpl);
return( head );
}

static int RefCharsMatch(RefChar *urefs,RefChar *crefs) {
    /* I assume they are in the same order */
    while ( urefs!=NULL && crefs!=NULL ) {
	if ( urefs->sc!=crefs->sc ||
		urefs->transform[0]!=crefs->transform[0] ||
		urefs->transform[1]!=crefs->transform[1] ||
		urefs->transform[2]!=crefs->transform[2] ||
		urefs->transform[3]!=crefs->transform[3] ||
		urefs->transform[4]!=crefs->transform[4] ||
		urefs->transform[5]!=crefs->transform[5] )
return( false );
	urefs = urefs->next;
	crefs = crefs->next;
    }
    if ( urefs==NULL && crefs==NULL )
return( true );

return( false );
}

static int ImagesMatch(ImageList *uimgs,ImageList *cimgs) {
    /* I assume they are in the same order */
    while ( uimgs!=NULL && cimgs!=NULL ) {
	if ( uimgs->image!=cimgs->image ||
		uimgs->xoff!=cimgs->xoff ||
		uimgs->yoff!=cimgs->yoff ||
		uimgs->xscale!=cimgs->xscale ||
		uimgs->yscale!=cimgs->yscale  )
return( false );
	uimgs = uimgs->next;
	cimgs = cimgs->next;
    }
    if ( uimgs==NULL && cimgs==NULL )
return( true );

return( false );
}

static RefChar *RefCharInList(RefChar *search,RefChar *list) {
    while ( list!=NULL ) {
	if ( search->sc==list->sc &&
		search->transform[0]==list->transform[0] &&
		search->transform[1]==list->transform[1] &&
		search->transform[2]==list->transform[2] &&
		search->transform[3]==list->transform[3] &&
		search->transform[4]==list->transform[4] &&
		search->transform[5]==list->transform[5] )
return( list );
	list = list->next;
    }
return( NULL );
}

static ImageList *ImageInList(ImageList *search,ImageList *list) {
    while ( list!=NULL ) {
	if ( search->image!=list->image ||
		search->xoff!=list->xoff ||
		search->yoff!=list->yoff ||
		search->xscale!=list->xscale ||
		search->yscale!=list->yscale  )
return( list );
	list = list->next;
    }
return( NULL );
}

static void FixupRefChars(SplineChar *sc,RefChar *urefs,int layer) {
    RefChar *crefs = sc->layers[layer].refs, *cend, *cprev, *unext, *cnext;

    cprev = NULL;
    while ( crefs!=NULL && urefs!=NULL ) {
	if ( urefs->sc==crefs->sc &&
		urefs->transform[0]==crefs->transform[0] &&
		urefs->transform[1]==crefs->transform[1] &&
		urefs->transform[2]==crefs->transform[2] &&
		urefs->transform[3]==crefs->transform[3] &&
		urefs->transform[4]==crefs->transform[4] &&
		urefs->transform[5]==crefs->transform[5] ) {
	    unext = urefs->next;
	    crefs->selected = urefs->selected;
	    RefCharFree(urefs);
	    urefs = unext;
	    cprev = crefs;
	    crefs = crefs->next;
	} else if ( (cend = RefCharInList(urefs,crefs->next))!=NULL ) {
	    /* if the undo refchar matches something further down the char's */
	    /*  ref list, then than means we need to delete everything on the */
	    /*  char's list between the two */
	    while ( crefs!=cend ) {
		cnext = crefs->next;
		SCRemoveDependent(sc,crefs);
		crefs = cnext;
	    }
	} else { /* urefs isn't on the list. Add it here */
	    unext = urefs->next;
	    urefs->next = crefs;
	    if ( cprev==NULL )
		sc->layers[layer].refs = urefs;
	    else
		cprev->next = urefs;
	    cprev = urefs;
	    SCReinstanciateRefChar(sc,urefs);
	    SCMakeDependent(sc,urefs->sc);
	    urefs = unext;
	}
    }
    if ( crefs!=NULL ) {
	while ( crefs!=NULL ) {
	    cnext = crefs->next;
	    SCRemoveDependent(sc,crefs);
	    crefs = cnext;
	}
    } else if ( urefs!=NULL ) {
	if ( cprev==NULL )
	    sc->layers[layer].refs = urefs;
	else
	    cprev->next = urefs;
	while ( urefs!=NULL ) {
	    SCReinstanciateRefChar(sc,urefs);
	    SCMakeDependent(sc,urefs->sc);
	    urefs = urefs->next;
	}
    }
}

static void FixupImages(SplineChar *sc,ImageList *uimgs,int layer) {
    ImageList *cimgs = sc->layers[layer].images, *cend, *cprev, *unext, *cnext;

    cprev = NULL;
    while ( cimgs!=NULL && uimgs!=NULL ) {
	if ( uimgs->image==cimgs->image &&
		uimgs->xoff==cimgs->xoff &&
		uimgs->yoff==cimgs->yoff &&
		uimgs->xscale==cimgs->xscale &&
		uimgs->yscale==cimgs->yscale  ) {
	    unext = uimgs->next;
	    cimgs->selected = uimgs->selected;
	    free(uimgs);
	    uimgs = unext;
	    cprev = cimgs;
	    cimgs = cimgs->next;
	} else if ( (cend = ImageInList(uimgs,cimgs->next))!=NULL ) {
	    /* if the undo image matches something further down the char's */
	    /*  img list, then than means we need to delete everything on the */
	    /*  char's list between the two */
	    if ( cprev==NULL )
		sc->layers[layer].images = cend;
	    else
		cprev->next = cend;
	    while ( cimgs!=cend ) {
		cnext = cimgs->next;
		free(cimgs);
		cimgs = cnext;
	    }
	} else { /* uimgs isn't on the list. Add it here */
	    unext = uimgs->next;
	    uimgs->next = cimgs;
	    if ( cprev==NULL )
		sc->layers[layer].images = uimgs;
	    else
		cprev->next = uimgs;
	    cprev = uimgs;
	    uimgs = unext;
	}
    }
    if ( cimgs!=NULL ) {
	ImageListsFree(cimgs);
	if ( cprev==NULL )
	    sc->layers[layer].images = NULL;
	else
	    cprev->next = NULL;
    } else if ( uimgs!=NULL ) {
	if ( cprev==NULL )
	    sc->layers[layer].images = uimgs;
	else
	    cprev->next = uimgs;
    }
}

static void UHintListFree(void *hints) {
    StemInfo *h, *t, *p;

    if ( hints==NULL )
return;
    if ( ((StemInfo *) hints)->hinttype==ht_d )
	DStemInfosFree(hints);
    else {
	h = t = hints;
	p = NULL;
	while ( t!=NULL && t->hinttype!=ht_d ) {
	    p = t;
	    t = t->next;
	}
	p->next = NULL;
	StemInfosFree(h);
	DStemInfosFree((DStemInfo *) t);
    }
}

static void *UHintCopy(SplineChar *sc,int docopy) {
    StemInfo *h = sc->hstem, *v = sc->vstem, *last=NULL;
    DStemInfo *d = sc->dstem;
    void *ret = NULL;

    if ( docopy ) {
	h = StemInfoCopy(h);
	v = StemInfoCopy(v);
	d = DStemInfoCopy(d);
    } else {
	sc->hstem = NULL;
	sc->vstem = NULL;
	sc->dstem = NULL;
	sc->hconflicts = sc->vconflicts = false;
    }
    ret = h;
    if ( h!=NULL ) {
	h->hinttype = ht_h;
	for ( last=h; last->next!=NULL; last=last->next ) last->next->hinttype = ht_unspecified;
	last->next = v;
    } else
	ret = v;
    if ( v!=NULL ) {
	v->hinttype = ht_v;
	for ( last=v; last->next!=NULL; last=last->next ) last->next->hinttype = ht_unspecified;
    }
    if ( last!=NULL )
	last->next = (StemInfo *) d;
    else
	ret = d;
    if ( d!=NULL ) {
	d->hinttype = ht_d;
	for ( d=d->next; d!=NULL; d=d->next ) d->hinttype = ht_unspecified;
    }
return(ret);
}

static void ExtractHints(SplineChar *sc,void *hints,int docopy) {
    StemInfo *h = NULL, *v = NULL, *p;
    DStemInfo *d = NULL;
    StemInfo *pv = NULL, *pd = NULL;

    p = NULL;
    while ( hints!=NULL ) {
	if ( ((StemInfo *) hints)->hinttype == ht_h )
	    h = hints;
	else if ( ((StemInfo *) hints)->hinttype == ht_v ) {
	    v = hints;
	    pv = p;
	} else if ( ((StemInfo *) hints)->hinttype == ht_d ) {
	    d = hints;
	    pd = p;
    break;
	}
	p = hints;
	hints = ((StemInfo *) hints)->next;
    }

    if ( pv!=NULL ) pv->next = NULL;
    if ( pd!=NULL ) pd->next = NULL;
    if ( docopy ) {
	h = StemInfoCopy(h);
	if ( pv!=NULL ) pv->next = v;
	v = StemInfoCopy(v);
	if ( pd!=NULL ) pd->next = (StemInfo *) d;
	d = DStemInfoCopy(d);
    }

    StemInfosFree(sc->hstem);
    StemInfosFree(sc->vstem);
    DStemInfosFree(sc->dstem);
    sc->hstem = h;
    sc->vstem = v;
    sc->dstem = d;
    sc->hconflicts = StemInfoAnyOverlaps(h);
    sc->vconflicts = StemInfoAnyOverlaps(v);
}

void UndoesFree(Undoes *undo) {
    Undoes *unext;
    int i;

    while ( undo!=NULL ) {
	unext = undo->next;
	switch ( undo->undotype ) {
	  case ut_noop:
	  case ut_width: case ut_vwidth: case ut_lbearing: case ut_rbearing:
	    /* Nothing else to free */;
	  break;
	  case ut_state: case ut_tstate: case ut_statehint: case ut_statename:
	    SplinePointListsFree(undo->u.state.splines);
	    RefCharsFree(undo->u.state.refs);
	    MinimumDistancesFree(undo->u.state.md);
#ifdef FONTFORGE_CONFIG_TYPE3
	    UHintListFree(undo->u.state.u.hints);
	    ImageListsFree(undo->u.state.u.images);
#else
	    if ( undo->undotype==ut_statehint || undo->undotype==ut_statename )
		UHintListFree(undo->u.state.u.hints);
	    else
		ImageListsFree(undo->u.state.u.images);
#endif
	    if ( undo->undotype==ut_statename ) {
		free( undo->u.state.charname );
		free( undo->u.state.comment );
		PSTFree( undo->u.state.possub );
	    }
	    AnchorPointsFree(undo->u.state.anchor);
	  break;
	  case ut_bitmap:
	    free(undo->u.bmpstate.bitmap);
	    BDFFloatFree(undo->u.bmpstate.selection);
	  break;
	  case ut_multiple: case ut_layers:
	    UndoesFree( undo->u.multiple.mult );
	  break;
	  case ut_composit:
	    UndoesFree(undo->u.composit.state);
	    UndoesFree(undo->u.composit.bitmaps);
	  break;
	  case ut_possub:
	    for ( i=0; undo->u.possub.data[i]!=NULL; ++i )
		free(undo->u.possub.data[i]);
	    free(undo->u.possub.data);
	  break;
	  default:
	    GDrawIError( "Unknown undo type in UndoesFree: %d", undo->undotype );
	  break;
	}
	chunkfree(undo,sizeof(Undoes));
	undo = unext;
    }
}

static Undoes *AddUndo(Undoes *undo,Undoes **uhead,Undoes **rhead) {
    int ucnt;
    Undoes *u, *prev;

    UndoesFree(*rhead);
    *rhead = NULL;
    if ( maxundoes==0 ) maxundoes = 1;		/* Must be at least one or snap to breaks */
    if ( maxundoes>0 ) {
	ucnt = 0;
	prev = NULL;
	for ( u= *uhead; u!=NULL; u=u->next ) {
	    if ( ++ucnt>=maxundoes )
	break;
	    prev = u;
	}
	if ( u!=NULL ) {
	    UndoesFree(u);
	    if ( prev!=NULL )
		prev->next = NULL;
	    else
		*uhead = NULL;
	}
    }
    undo->next = *uhead;
    *uhead = undo;
return( undo );
}

static Undoes *CVAddUndo(CharView *cv,Undoes *undo) {
return( AddUndo(undo,&cv->layerheads[cv->drawmode]->undoes,
	&cv->layerheads[cv->drawmode]->redoes));
}

int CVLayer(CharView *cv) {
    if ( cv->drawmode==dm_grid )
return( ly_grid );

return( cv->layerheads[cv->drawmode]-cv->sc->layers );
}

Undoes *CVPreserveState(CharView *cv) {
    Undoes *undo;
    int layer = CVLayer(cv);

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_state;
    undo->was_modified = cv->sc->changed;
    undo->was_order2 = cv->sc->parent->order2;
    undo->u.state.width = cv->sc->width;
    undo->u.state.vwidth = cv->sc->vwidth;
    undo->u.state.splines = SplinePointListCopy(cv->layerheads[cv->drawmode]->splines);
    undo->u.state.refs = RefCharsCopyState(cv->sc,layer);
    if ( layer==ly_fore ) {
	undo->u.state.md = MDsCopyState(cv->sc,undo->u.state.splines);
	undo->u.state.anchor = AnchorPointsCopy(cv->sc->anchor);
    }
    undo->u.state.u.images = ImageListCopy(cv->layerheads[cv->drawmode]->images);
#ifdef FONTFORGE_CONFIG_TYPE3
    undo->u.state.fill_brush = cv->layerheads[cv->drawmode]->fill_brush;
    undo->u.state.stroke_pen = cv->layerheads[cv->drawmode]->stroke_pen;
    undo->u.state.dofill = cv->layerheads[cv->drawmode]->dofill;
    undo->u.state.dostroke = cv->layerheads[cv->drawmode]->dostroke;
    undo->u.state.fillfirst = cv->layerheads[cv->drawmode]->fillfirst;
#endif
return( CVAddUndo(cv,undo));
}

Undoes *SCPreserveLayer(SplineChar *sc,int layer, int dohints) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_state;
    undo->was_modified = sc->changed;
    undo->was_order2 = sc->parent->order2;
    undo->u.state.width = sc->width;
    undo->u.state.vwidth = sc->vwidth;
    undo->u.state.splines = SplinePointListCopy(sc->layers[layer].splines);
    undo->u.state.refs = RefCharsCopyState(sc,layer);
    if ( layer==ly_fore ) {
	undo->u.state.md = MDsCopyState(sc,undo->u.state.splines);
	undo->u.state.anchor = AnchorPointsCopy(sc->anchor);
    }
    undo->u.state.u.images = ImageListCopy(sc->layers[layer].images);
    if ( dohints ) {
	undo->undotype = ut_statehint;
	undo->u.state.u.hints = UHintCopy(sc,true);
	if ( dohints==2 ) {
	    undo->undotype = ut_statename;
	    undo->u.state.unicodeenc = sc->unicodeenc;
	    undo->u.state.charname = copy(sc->name);
	    undo->u.state.comment = u_copy(sc->comment);
	    undo->u.state.possub = PSTCopy(sc->possub,sc,sc->parent);
	}
    }
#ifdef FONTFORGE_CONFIG_TYPE3
    undo->u.state.fill_brush = sc->layers[layer].fill_brush;
    undo->u.state.stroke_pen = sc->layers[layer].stroke_pen;
    undo->u.state.dofill = sc->layers[layer].dofill;
    undo->u.state.dostroke = sc->layers[layer].dostroke;
    undo->u.state.fillfirst = sc->layers[layer].fillfirst;
#endif
    undo->u.state.copied_from = sc->parent;
return( AddUndo(undo,&sc->layers[layer].undoes,&sc->layers[layer].redoes));
}

Undoes *SCPreserveState(SplineChar *sc,int dohints) {
#ifdef FONTFORGE_CONFIG_TYPE3
    int i;

    for ( i=ly_fore+1; i<sc->layer_cnt; ++i )
	SCPreserveLayer(sc,i,false);
#endif
return( SCPreserveLayer(sc,ly_fore,dohints));
}

Undoes *SCPreserveBackground(SplineChar *sc) {
return( SCPreserveLayer(sc,ly_back,false));
}

void SCUndoSetLBearingChange(SplineChar *sc,int lbc) {
    Undoes *undo = sc->layers[ly_fore].undoes;

    if ( undo==NULL || undo->undotype != ut_state )
return;
    undo->u.state.lbearingchange = lbc;
}

Undoes *CVPreserveTState(CharView *cv) {
    Undoes *undo;
    int anyrefs;
    RefChar *refs, *urefs;
    int was0 = false, j;

    cv->p.transany = CVAnySel(cv,NULL,&anyrefs,NULL,NULL);
    cv->p.transanyrefs = anyrefs;

    if ( maxundoes==0 ) {
	was0 = true;
	maxundoes = 1;
    }

    undo = CVPreserveState(cv);
    if ( !cv->p.transany || cv->p.transanyrefs ) {
	for ( refs = cv->layerheads[cv->drawmode]->refs, urefs=undo->u.state.refs; urefs!=NULL; refs=refs->next, urefs=urefs->next )
	    if ( !cv->p.transany || refs->selected )
		for ( j=0; j<urefs->layer_cnt; ++j )
		    urefs->layers[j].splines = SplinePointListCopy(refs->layers[j].splines);
    }
    undo->undotype = ut_tstate;

    if ( was0 )
	maxundoes = 0;

return( undo );
}

Undoes *CVPreserveWidth(CharView *cv,int width) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_width;
    undo->was_modified = cv->sc->changed;
    undo->was_order2 = cv->sc->parent->order2;
    undo->u.width = width;
return( CVAddUndo(cv,undo));
}

Undoes *CVPreserveVWidth(CharView *cv,int vwidth) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_vwidth;
    undo->was_modified = cv->sc->changed;
    undo->was_order2 = cv->sc->parent->order2;
    undo->u.width = vwidth;
return( CVAddUndo(cv,undo));
}

Undoes *SCPreserveWidth(SplineChar *sc) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_width;
    undo->was_modified = sc->changed;
    undo->was_order2 = sc->parent->order2;
    undo->u.state.width = sc->width;
return( AddUndo(undo,&sc->layers[ly_fore].undoes,&sc->layers[ly_fore].redoes));
}

Undoes *SCPreserveVWidth(SplineChar *sc) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_vwidth;
    undo->was_modified = sc->changed;
    undo->was_order2 = sc->parent->order2;
    undo->u.state.width = sc->vwidth;
return( AddUndo(undo,&sc->layers[ly_fore].undoes,&sc->layers[ly_fore].redoes));
}

Undoes *BCPreserveState(BDFChar *bc) {
    Undoes *undo;

    if ( screen_display==NULL || maxundoes==0 )		/* No use for undoes in scripting */
return(NULL);

    undo = chunkalloc(sizeof(Undoes));

    undo->undotype = ut_bitmap;
    undo->u.bmpstate.width = bc->width;
    undo->u.bmpstate.xmin = bc->xmin;
    undo->u.bmpstate.xmax = bc->xmax;
    undo->u.bmpstate.ymin = bc->ymin;
    undo->u.bmpstate.ymax = bc->ymax;
    undo->u.bmpstate.bytes_per_line = bc->bytes_per_line;
    undo->u.bmpstate.bitmap = bmpcopy(bc->bitmap,bc->bytes_per_line,
	    bc->ymax-bc->ymin+1);
    undo->u.bmpstate.selection = BDFFloatCopy(bc->selection);
return( AddUndo(undo,&bc->undoes,&bc->redoes));
}

static void SCUndoAct(SplineChar *sc,int layer, Undoes *undo) {

    switch ( undo->undotype ) {
      case ut_noop:
      break;
      case ut_width: {
	int width = sc->width;
	if ( sc->width!=undo->u.width )
	    SCSynchronizeWidth(sc,undo->u.width,width,NULL);
	undo->u.width = width;
      } break;
      case ut_vwidth: {
	int vwidth = sc->vwidth;
	sc->vwidth = undo->u.width;
	undo->u.width = vwidth;
      } break;
      case ut_state: case ut_tstate: case ut_statehint: case ut_statename: {
	Layer *head = layer==ly_grid ? &sc->parent->grid : &sc->layers[layer];
	SplinePointList *spl = head->splines;

	if ( layer==ly_fore ) {
	    int width = sc->width;
	    int vwidth = sc->vwidth;
	    if ( sc->width!=undo->u.state.width )
		SCSynchronizeWidth(sc,undo->u.state.width,width,NULL);
	    sc->vwidth = undo->u.state.vwidth;
	    undo->u.state.width = width;
	    undo->u.state.vwidth = vwidth;
	}
	head->splines = undo->u.state.splines;
	if ( layer==ly_fore ) {
	    MinimumDistance *md = sc->md;
	    AnchorPoint *ap = sc->anchor;
	    sc->md = undo->u.state.md;
	    undo->u.state.md = md;
	    sc->anchor = undo->u.state.anchor;
	    undo->u.state.anchor = ap;
	}
	if ( layer>=ly_fore && !RefCharsMatch(undo->u.state.refs,sc->layers[layer].refs)) {
	    RefChar *refs = RefCharsCopyState(sc,layer);
	    FixupRefChars(sc,undo->u.state.refs,layer);
	    undo->u.state.refs = refs;
	} else if ( layer==ly_fore && undo->undotype==ut_statehint ) {
	    void *hints = UHintCopy(sc,false);
	    ExtractHints(sc,undo->u.state.u.hints,false);
	    undo->u.state.u.hints = hints;
	}
	if ( undo->undotype!=ut_statehint &&
		!ImagesMatch(undo->u.state.u.images,sc->layers[layer].images)) {
	    ImageList *images = ImageListCopy(sc->layers[layer].images);
	    FixupImages(sc,undo->u.state.u.images,layer);
	    undo->u.state.u.images = images;
	    SCOutOfDateBackground(sc);
	}
	undo->u.state.splines = spl;
	if ( undo->u.state.lbearingchange ) {
	    undo->u.state.lbearingchange = -undo->u.state.lbearingchange;
	    SCSynchronizeLBearing(sc,NULL,undo->u.state.lbearingchange);
	}
	if ( layer==ly_fore && undo->undotype==ut_statename ) {
	    char *temp = sc->name;
	    int uni = sc->unicodeenc;
	    PST *possub = sc->possub;
	    unichar_t *comment = sc->comment;
	    sc->name = undo->u.state.charname;
	    undo->u.state.charname = temp;
	    sc->unicodeenc = undo->u.state.unicodeenc;
	    undo->u.state.unicodeenc = uni;
	    sc->possub = undo->u.state.possub;
	    undo->u.state.possub = possub;
	    sc->comment = undo->u.state.comment;
	    undo->u.state.comment = comment;
	}
      } break;
      default:
	GDrawIError( "Unknown undo type in SCUndoAct: %d", undo->undotype );
      break;
    }
}

void CVDoUndo(CharView *cv) {
    Undoes *undo = cv->layerheads[cv->drawmode]->undoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    cv->layerheads[cv->drawmode]->undoes = undo->next;
    undo->next = NULL;
    SCUndoAct(cv->sc,CVLayer(cv),undo);
    undo->next = cv->layerheads[cv->drawmode]->redoes;
    cv->layerheads[cv->drawmode]->redoes = undo;
    _CVCharChangedUpdate(cv,undo->was_modified);
    cv->lastselpt = NULL;
return;
}

void CVDoRedo(CharView *cv) {
    Undoes *undo = cv->layerheads[cv->drawmode]->redoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    cv->layerheads[cv->drawmode]->redoes = undo->next;
    undo->next = NULL;
    SCUndoAct(cv->sc,CVLayer(cv),undo);
    undo->next = cv->layerheads[cv->drawmode]->undoes;
    cv->layerheads[cv->drawmode]->undoes = undo;
    CVCharChangedUpdate(cv);
    cv->lastselpt = NULL;
return;
}

void SCDoUndo(SplineChar *sc,int layer) {
    Undoes *undo = sc->layers[layer].undoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    sc->layers[layer].undoes = undo->next;
    undo->next = NULL;
    SCUndoAct(sc,layer,undo);
    undo->next = sc->layers[layer].redoes;
    sc->layers[layer].redoes = undo;
    _SCCharChangedUpdate(sc,undo->was_modified);
return;
}

void SCDoRedo(SplineChar *sc, int layer) {
    Undoes *undo = sc->layers[layer].redoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    sc->layers[layer].redoes = undo->next;
    undo->next = NULL;
    SCUndoAct(sc,layer,undo);
    undo->next = sc->layers[layer].undoes;
    sc->layers[layer].undoes = undo;
    SCCharChangedUpdate(sc);
return;
}

/* Used when doing incremental transformations. If I just keep doing increments*/
/*  then rounding errors will mount. Instead I go back to the original state */
/*  each time */
void CVRestoreTOriginalState(CharView *cv) {
    Undoes *undo = cv->layerheads[cv->drawmode]->undoes;
    RefChar *ref, *uref;
    ImageList *img, *uimg;
    int j;

    SplinePointListSet(cv->layerheads[cv->drawmode]->splines,undo->u.state.splines);
    if ( cv->drawmode==dm_fore && (!cv->p.anysel || cv->p.transanyrefs)) {
	for ( ref=cv->layerheads[cv->drawmode]->refs, uref=undo->u.state.refs; uref!=NULL; ref=ref->next, uref=uref->next )
	    for ( j=0; j<uref->layer_cnt; ++j )
		if ( uref->layers[j].splines!=NULL ) {
		    SplinePointListSet(ref->layers[j].splines,uref->layers[j].splines);
		    memcpy(&ref->transform,&uref->transform,sizeof(ref->transform));
		}
    }
    for ( img=cv->layerheads[cv->drawmode]->images, uimg=undo->u.state.u.images; uimg!=NULL;
	    img = img->next, uimg = uimg->next ) {
	img->xoff = uimg->xoff;
	img->yoff = uimg->yoff;
	img->xscale = uimg->xscale;
	img->yscale = uimg->yscale;
    }
}

void CVUndoCleanup(CharView *cv) {
    Undoes * undo = cv->layerheads[cv->drawmode]->undoes;
    RefChar *uref;

    if ( cv->drawmode==dm_fore && (!cv->p.anysel || cv->p.transanyrefs)) {
	for ( uref=undo->u.state.refs; uref!=NULL; uref=uref->next ) {
	    SplinePointListFree(uref->layers[0].splines);
	    uref->layers[0].splines = NULL;
	}
    }
    undo->undotype = ut_state;
}

void CVRemoveTopUndo(CharView *cv) {
    Undoes * undo = cv->layerheads[cv->drawmode]->undoes;

    cv->layerheads[cv->drawmode]->undoes = undo->next;
    undo->next = NULL;
    UndoesFree(undo);
}

static void BCUndoAct(BDFChar *bc,Undoes *undo) {

    switch ( undo->undotype ) {
      case ut_bitmap: {
	uint8 *b;
	int temp;
	BDFFloat *sel;
	temp = bc->width; bc->width = undo->u.bmpstate.width; undo->u.bmpstate.width = temp;
	temp = bc->xmin; bc->xmin = undo->u.bmpstate.xmin; undo->u.bmpstate.xmin = temp;
	temp = bc->xmax; bc->xmax = undo->u.bmpstate.xmax; undo->u.bmpstate.xmax = temp;
	temp = bc->ymin; bc->ymin = undo->u.bmpstate.ymin; undo->u.bmpstate.ymin = temp;
	temp = bc->ymax; bc->ymax = undo->u.bmpstate.ymax; undo->u.bmpstate.ymax = temp;
	temp = bc->bytes_per_line; bc->bytes_per_line = undo->u.bmpstate.bytes_per_line; undo->u.bmpstate.bytes_per_line = temp;
	b = bc->bitmap; bc->bitmap = undo->u.bmpstate.bitmap; undo->u.bmpstate.bitmap = b;
	sel = bc->selection; bc->selection = undo->u.bmpstate.selection; undo->u.bmpstate.selection = sel;
      } break;
      default:
	GDrawIError( "Unknown undo type in BCUndoAct: %d", undo->undotype );
      break;
    }
}

void BCDoUndo(BDFChar *bc,FontView *fv) {
    Undoes *undo = bc->undoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    bc->undoes = undo->next;
    undo->next = NULL;
    BCUndoAct(bc,undo);
    undo->next = bc->redoes;
    bc->redoes = undo;
    BCCharChangedUpdate(bc);
return;
}

void BCDoRedo(BDFChar *bc,FontView *fv) {
    Undoes *undo = bc->redoes;

    if ( undo==NULL )		/* Shouldn't happen */
return;
    bc->redoes = undo->next;
    undo->next = NULL;
    BCUndoAct(bc,undo);
    undo->next = bc->undoes;
    bc->undoes = undo;
    BCCharChangedUpdate(bc);
return;
}

/* **************************** Cut, Copy & Paste *************************** */

static Undoes copybuffer;

static void CopyBufferFree(void) {

    switch( copybuffer.undotype ) {
      case ut_state: case ut_statehint:
	SplinePointListsFree(copybuffer.u.state.splines);
	RefCharsFree(copybuffer.u.state.refs);
	AnchorPointsFree(copybuffer.u.state.anchor);
	if ( copybuffer.undotype==ut_statehint )
	    UHintListFree(copybuffer.u.state.u.hints);
	else
	    ImageListsFree(copybuffer.u.state.u.images);
      break;
      case ut_bitmapsel:
	BDFFloatFree(copybuffer.u.bmpstate.selection);
      break;
      case ut_multiple: case ut_layers:
	UndoesFree( copybuffer.u.multiple.mult );
      break;
      case ut_composit:
	UndoesFree( copybuffer.u.composit.state );
	UndoesFree( copybuffer.u.composit.bitmaps );
      break;
    }
    memset(&copybuffer,'\0',sizeof(copybuffer));
    copybuffer.undotype = ut_none;
}

static void CopyBufferFreeGrab(void) {
    CopyBufferFree();
    if ( fv_list!=NULL && screen_display!=NULL )
	GDrawGrabSelection(fv_list->gw,sn_clipboard);	/* Grab the selection to one of my windows, doesn't matter which, aren't going to export it, but just want to clear things out so no one else thinks they have the selection */
}

static void noop(void *_copybuffer) {
}

static void *copybufferPt2str(void *_copybuffer,int32 *len) {
    Undoes *cur = &copybuffer;
    SplinePoint *sp;
    char buffer[100];

    while ( cur ) {
	switch ( cur->undotype ) {
	  case ut_multiple:
	    cur = cur->u.multiple.mult;
	  break;
	  case ut_composit:
	    cur = cur->u.composit.state;
	  break;
	  case ut_state: case ut_statehint: case ut_statename:
    goto out;
	  default:
	    cur = NULL;
	  break;
	}
    }
    out:
    if ( cur==NULL || fv_list==NULL ||
	    cur->u.state.splines==NULL || cur->u.state.refs!=NULL ||
	    cur->u.state.splines->next!=NULL ||
	    cur->u.state.splines->first->next!=NULL ) {
	*len=0;
return( copy(""));
    }

    sp = cur->u.state.splines->first;
    sprintf(buffer,"(%g%s%g)", sp->me.x, coord_sep, sp->me.y );
    *len = strlen(buffer);
return( copy(buffer));
}

static void *copybufferName2str(void *_copybuffer,int32 *len) {
    Undoes *cur = &copybuffer;

    while ( cur ) {
	switch ( cur->undotype ) {
	  case ut_multiple:
	    cur = cur->u.multiple.mult;
	  break;
	  case ut_composit:
	    cur = cur->u.composit.state;
	  break;
	  case ut_statename:
    goto out;
	  default:
	    cur = NULL;
	  break;
	}
    }
    out:
    if ( cur==NULL || fv_list==NULL || cur->u.state.charname==NULL ) {
	*len=0;
return( copy(""));
    }
    *len = strlen(cur->u.state.charname);
return( copy( cur->u.state.charname ));
}

static void *copybufferPosSub2str(void *_copybuffer,int32 *len) {
    Undoes *cur = &copybuffer;
    char *pt, *data;
    int lcnt, size;

    while ( cur ) {
	switch ( cur->undotype ) {
	  case ut_multiple:
	    cur = cur->u.multiple.mult;
	  break;
	  case ut_composit:
	    cur = cur->u.composit.state;
	  break;
	  case ut_possub:
    goto out;
	  default:
	    cur = NULL;
	  break;
	}
    }
    out:
    if ( cur==NULL || fv_list==NULL || cur->u.possub.data==NULL ) {
	*len=0;
return( copy(""));
    }
    for ( lcnt=size=0; cur->u.possub.data[lcnt]!=NULL; ++lcnt )
	size += strlen(cur->u.possub.data[lcnt])+1;
    data = pt = galloc(size+1);
    for ( lcnt=0; cur->u.possub.data[lcnt]!=NULL; ++lcnt ) {
	strcpy(pt,cur->u.possub.data[lcnt]);
	pt += strlen(cur->u.possub.data[lcnt]);
	*pt++='\n';
    }
    if ( lcnt!=0 )
	pt[-1] = '\0';
    *pt = '\0';

    *len = strlen(data);
return( data );
}

static void *copybuffer2eps(void *_copybuffer,int32 *len) {
    Undoes *cur = &copybuffer;
    SplineChar dummy;
    FILE *eps;
    char *ret;
    int old_order2;

    while ( cur ) {
	switch ( cur->undotype ) {
	  case ut_multiple:
	    cur = cur->u.multiple.mult;
	  break;
	  case ut_composit:
	    cur = cur->u.composit.state;
	  break;
	  case ut_state: case ut_statehint:
    goto out;
	  default:
	    cur = NULL;
	  break;
	}
    }
    out:
    if ( cur==NULL || fv_list==NULL ) {
	*len=0;
return( copy(""));
    }

    memset(&dummy,0,sizeof(dummy));
    dummy.name = "dummy";
    dummy.parent = fv_list->sf;		/* might not be the actual parent */
    dummy.layers[ly_fore].splines = cur->u.state.splines;
    dummy.layers[ly_fore].refs = cur->u.state.refs;

    eps = tmpfile();
    if ( eps==NULL ) {
	*len=0;
return( copy(""));
    }

    old_order2 = dummy.parent->order2;
    dummy.parent->order2 = cur->was_order2;
    _ExportEPS(eps,&dummy);
    dummy.parent->order2 = old_order2;

    fseek(eps,0,SEEK_END);
    *len = ftell(eps);
    ret = galloc(*len);
    rewind(eps);
    fread(ret,1,*len,eps);
    fclose(eps);
return( ret );
}

static void XClipCheckEps(void) {
    Undoes *cur = &copybuffer;

    if ( fv_list==NULL )
return;
    if ( screen_display==NULL )
return;

    while ( cur ) {
	switch ( cur->undotype ) {
	  case ut_multiple:
	    cur = cur->u.multiple.mult;
	  break;
	  case ut_composit:
	    cur = cur->u.composit.state;
	  break;
	  case ut_state: case ut_statehint: case ut_statename:
	    GDrawAddSelectionType(fv_list->gw,sn_clipboard,"image/eps",&copybuffer,0,sizeof(char),
		    copybuffer2eps,noop);
	    /* If the selection is one point, then export the coordinates as a string */
	    if ( cur->u.state.splines!=NULL && cur->u.state.refs==NULL &&
		    cur->u.state.splines->next==NULL &&
		    cur->u.state.splines->first->next==NULL )
		GDrawAddSelectionType(fv_list->gw,sn_clipboard,"STRING",&copybuffer,0,sizeof(char),
			copybufferPt2str,noop);
	    else if ( cur->undotype==ut_statename )
		GDrawAddSelectionType(fv_list->gw,sn_clipboard,"STRING",&copybuffer,0,sizeof(char),
			copybufferName2str,noop);
	    cur = NULL;
	  break;
	  case ut_possub:
	    GDrawAddSelectionType(fv_list->gw,sn_clipboard,"STRING",&copybuffer,0,sizeof(char),
		    copybufferPosSub2str,noop);
	    cur = NULL;
	  break;
	  default:
	    cur = NULL;
	  break;
	}
    }
}

void ClipboardClear(void) {
    CopyBufferFree();
}

enum undotype CopyUndoType(void) {
    Undoes *paster;

    paster = &copybuffer;
    while ( paster->undotype==ut_composit || paster->undotype==ut_multiple ) {
	if ( paster->undotype==ut_multiple )
	    paster = paster->u.multiple.mult;
	else if ( paster->u.composit.state==NULL )
return( ut_none );
	else
	    paster = paster->u.composit.state;
    }
return( paster->undotype );
}

int CopyContainsSomething(void) {
    Undoes *cur = &copybuffer;
    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    if ( cur->undotype==ut_composit )
return( cur->u.composit.state!=NULL );

return( cur->undotype==ut_state || cur->undotype==ut_tstate ||
	cur->undotype==ut_statehint || cur->undotype==ut_statename ||
	cur->undotype==ut_width || cur->undotype==ut_vwidth ||
	cur->undotype==ut_lbearing || cur->undotype==ut_rbearing ||
	cur->undotype==ut_noop );
}

int CopyContainsBitmap(void) {
    Undoes *cur = &copybuffer;
    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    if ( cur->undotype==ut_composit )
return( cur->u.composit.bitmaps!=NULL );

return( cur->undotype==ut_bitmapsel || cur->undotype==ut_noop );
}

RefChar *CopyContainsRef(SplineFont *sf) {
    Undoes *cur = &copybuffer;
    if ( cur->undotype==ut_multiple ) {
	cur = cur->u.multiple.mult;
	if ( cur->next!=NULL )
return( NULL );
    }
    if ( cur->undotype==ut_composit )
	cur = cur->u.composit.state;
    if ( cur==NULL || (cur->undotype!=ut_state && cur->undotype!=ut_tstate &&
	    cur->undotype!=ut_statehint && cur->undotype!=ut_statename ))
return( NULL );
    if ( cur->u.state.splines!=NULL || cur->u.state.refs==NULL ||
	    cur->u.state.refs->next != NULL )
return( NULL );
    if ( sf!=cur->u.state.copied_from )
return( NULL );

return( cur->u.state.refs );
}

char **CopyGetPosSubData(enum possub_type *type) {
    Undoes *cur = &copybuffer;

    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    if ( cur->undotype!=ut_possub )
return( NULL );
    *type = cur->u.possub.pst;
return( cur->u.possub.data );
}

int getAdobeEnc(char *name) {
    extern char *AdobeStandardEncoding[256];
    int i;

    for ( i=0; i<256; ++i )
	if ( strcmp(name,AdobeStandardEncoding[i])==0 )
    break;
    if ( i==256 ) i = -1;
return( i );
}

void CopyReference(SplineChar *sc) {
    RefChar *ref;

    CopyBufferFreeGrab();

    copybuffer.undotype = ut_state;
    copybuffer.was_order2 = sc->parent->order2;
    copybuffer.u.state.width = sc->width;
    copybuffer.u.state.vwidth = sc->vwidth;
    copybuffer.u.state.refs = ref = RefCharCreate();
    copybuffer.u.state.copied_from = sc->parent;
    ref->unicode_enc = sc->unicodeenc;
    ref->local_enc = sc->enc;
    ref->adobe_enc = getAdobeEnc(sc->name);
    ref->transform[0] = ref->transform[3] = 1.0;

    XClipCheckEps();
}

void CopySelected(CharView *cv) {

    CopyBufferFreeGrab();

    copybuffer.undotype = ut_state;
    copybuffer.was_order2 = cv->sc->parent->order2;
    copybuffer.u.state.width = cv->sc->width;
    copybuffer.u.state.vwidth = cv->sc->vwidth;
    copybuffer.u.state.splines = SplinePointListCopySelected(cv->layerheads[cv->drawmode]->splines);
    if ( cv->drawmode==dm_fore ) {
	RefChar *refs, *new;
	for ( refs = cv->layerheads[cv->drawmode]->refs; refs!=NULL; refs = refs->next ) if ( refs->selected ) {
	    new = RefCharCreate();
	    *new = *refs;
	    new->layers[0].splines = NULL;
	    new->local_enc = new->sc->enc;
	    new->sc = NULL;
	    new->next = copybuffer.u.state.refs;
	    copybuffer.u.state.refs = new;
	}
	if ( cv->showanchor ) {
	    AnchorPoint *ap, *new;
	    for ( ap=cv->sc->anchor; ap!=NULL; ap=ap->next ) if ( ap->selected ) {
		new = chunkalloc(sizeof(AnchorPoint));
		*new = *ap;
		new->next = copybuffer.u.state.anchor;
		copybuffer.u.state.anchor = new;
	    }
	}
    }
#ifndef FONTFORGE_CONFIG_TYPE3
    if ( cv->drawmode==dm_back )
#endif
    {
	ImageList *imgs, *new;
	for ( imgs = cv->layerheads[cv->drawmode]->images; imgs!=NULL; imgs = imgs->next ) if ( imgs->selected ) {
	    new = chunkalloc(sizeof(ImageList));
	    *new = *imgs;
	    new->next = copybuffer.u.state.u.images;
	    copybuffer.u.state.u.images = new;
	}
    }
#ifdef FONTFORGE_CONFIG_TYPE3
    if ( cv->drawmode==dm_fore || cv->drawmode==dm_back ) {
	copybuffer.u.state.fill_brush = cv->layerheads[cv->drawmode]->fill_brush;
	copybuffer.u.state.stroke_pen = cv->layerheads[cv->drawmode]->stroke_pen;
	copybuffer.u.state.dofill = cv->layerheads[cv->drawmode]->dofill;
	copybuffer.u.state.dostroke = cv->layerheads[cv->drawmode]->dostroke;
	copybuffer.u.state.fillfirst = cv->layerheads[cv->drawmode]->fillfirst;
    }
#endif
    copybuffer.u.state.copied_from = cv->sc->parent;

    XClipCheckEps();
}

void CVCopyGridFit(CharView *cv) {
    SplineChar *sc = cv->sc;

    if ( cv->gridfit==NULL )
return;

    CopyBufferFreeGrab();

    copybuffer.undotype = ut_state;
    copybuffer.was_order2 = sc->parent->order2;
    copybuffer.u.state.width = cv->ft_gridfitwidth;
    copybuffer.u.state.vwidth = sc->vwidth;
    copybuffer.u.state.splines = SplinePointListCopy(cv->gridfit);
    copybuffer.u.state.copied_from = cv->sc->parent;

    XClipCheckEps();
}

#ifdef FONTFORGE_CONFIG_TYPE3
static Undoes *SCCopyAllLayer(SplineChar *sc,int full,int layer) {
#else
static Undoes *SCCopyAll(SplineChar *sc,int full) {
    const int layer = ly_fore;
#endif
    Undoes *cur;
    RefChar *ref;
    extern int copymetadata;

    cur = chunkalloc(sizeof(Undoes));
    if ( sc==NULL ) {
	cur->undotype = ut_noop;
    } else {
	cur->was_order2 = sc->parent->order2;
	cur->u.state.width = sc->width;
	cur->u.state.vwidth = sc->vwidth;
	if ( full ) {
	    cur->undotype = copymetadata ? ut_statename : ut_statehint;
	    cur->u.state.splines = SplinePointListCopy(sc->layers[layer].splines);
	    cur->u.state.refs = RefCharsCopyState(sc,layer);
	    cur->u.state.anchor = AnchorPointsCopy(sc->anchor);
	    cur->u.state.u.hints = UHintCopy(sc,true);
	    cur->u.state.unicodeenc = sc->unicodeenc;
	    if ( copymetadata && layer==ly_fore ) {
		cur->u.state.charname = copy(sc->name);
		cur->u.state.comment = u_copy(sc->comment);
		cur->u.state.possub = PSTCopy(sc->possub,sc,sc->parent);
	    } else {
		cur->u.state.charname = NULL;
		cur->u.state.comment = NULL;
		cur->u.state.possub = NULL;
	    }
#ifdef FONTFORGE_CONFIG_TYPE3
	    cur->u.state.u.images = ImageListCopy(sc->layers[layer].images);
	    cur->u.state.fill_brush = sc->layers[layer].fill_brush;
	    cur->u.state.stroke_pen = sc->layers[layer].stroke_pen;
	    cur->u.state.dofill = sc->layers[layer].dofill;
	    cur->u.state.dostroke = sc->layers[layer].dostroke;
	    cur->u.state.fillfirst = sc->layers[layer].fillfirst;
#endif
	} else {		/* Or just make a reference */
	    sc = SCDuplicate(sc);
	    cur->undotype = ut_state;
	    cur->u.state.refs = ref = RefCharCreate();
	    ref->unicode_enc = sc->unicodeenc;
	    ref->local_enc = sc->enc;
	    ref->adobe_enc = getAdobeEnc(sc->name);
	    ref->transform[0] = ref->transform[3] = 1.0;
	}
	cur->u.state.copied_from = sc->parent;
    }
return( cur );
}

#ifdef FONTFORGE_CONFIG_TYPE3
static Undoes *SCCopyAll(SplineChar *sc,int full) {
    int layer;
    Undoes *ret, *cur, *last=NULL;

    ret = chunkalloc(sizeof(Undoes));
    if ( sc==NULL ) {
	ret->undotype = ut_noop;
    } else if ( !full || !sc->parent->multilayer ) {	/* Make a reference */
	chunkfree(ret,sizeof(Undoes));
	ret = SCCopyAllLayer(sc,full,ly_fore );
    } else {
	ret->undotype = ut_layers;
	for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
	    cur = SCCopyAllLayer(sc,true,layer);
	    if ( ret->u.multiple.mult==NULL )
		ret->u.multiple.mult = cur;
	    else
		last->next = cur;
	    last = cur;
	    full = false;
	}
    }
return( ret );
}
#endif

void SCCopyWidth(SplineChar *sc,enum undotype ut) {
    DBounds bb;

    CopyBufferFreeGrab();

    copybuffer.undotype = ut;
    switch ( ut ) {
      case ut_width:
	copybuffer.u.width = sc->width;
      break;
      case ut_vwidth:
	copybuffer.u.width = sc->width;
      break;
      case ut_lbearing:
	SplineCharFindBounds(sc,&bb);
	copybuffer.u.lbearing = bb.minx;
      break;
      case ut_rbearing:
	SplineCharFindBounds(sc,&bb);
	copybuffer.u.rbearing = sc->width-bb.maxx;
      break;
    }
}

void CopyWidth(CharView *cv,enum undotype ut) {
    SCCopyWidth(cv->sc,ut);
}

static SplineChar *FindCharacter(SplineFont *sf,RefChar *rf) {
    extern char *AdobeStandardEncoding[256];
    int i;

    if ( rf->local_enc<sf->charcnt && sf->chars[rf->local_enc]!=NULL &&
	    sf->chars[rf->local_enc]->unicodeenc == rf->unicode_enc )
return( sf->chars[rf->local_enc] );

    if ( rf->unicode_enc!=-1 ) {
	for ( i=0; i<sf->charcnt; ++i )
	    if ( sf->chars[i]!=NULL && sf->chars[i]->unicodeenc == rf->unicode_enc )
return( sf->chars[i] );
    }
    if ( rf->adobe_enc!=-1 ) {
	for ( i=0; i<sf->charcnt; ++i )
	    if ( sf->chars[i]!=NULL &&
		    strcmp(sf->chars[i]->name,AdobeStandardEncoding[rf->adobe_enc])==0 )
return( sf->chars[i] );
    }
    if ( rf->local_enc<sf->charcnt )
return( sf->chars[rf->local_enc] );

return( NULL );
}

int SCDependsOnSC(SplineChar *parent, SplineChar *child) {
    RefChar *ref;

    if ( parent==child )
return( true );
    for ( ref=parent->layers[ly_fore].refs; ref!=NULL; ref=ref->next ) {
	if ( SCDependsOnSC(ref->sc,child))
return( true );
    }
return( false );
}

static void PasteNonExistantRefCheck(SplineChar *sc,Undoes *paster,RefChar *ref,
	int *refstate) {
    FontView *fv;
    SplineChar *rsc=NULL;
    SplineSet *new, *spl;
    int yes = 3;

    for ( fv = fv_list; fv!=NULL && fv->sf!=paster->u.state.copied_from; fv=fv->next );
    if ( fv!=NULL ) {
	rsc = FindCharacter(fv->sf,ref);
	if ( rsc==NULL )
	    fv = NULL;
    }
    if ( fv==NULL ) {
	if ( !(*refstate&0x4) ) {
	    static int buts[] = { _STR_DontWarnAgain, _STR_OK, 0 };
	    char buf[10]; const char *name;
	    if ( ref->unicode_enc==-1 )
		name = "<Unknown>";
	    else if ( ref->unicode_enc<psunicodenames_cnt && psunicodenames[ref->unicode_enc]!=NULL )
		name = psunicodenames[ref->unicode_enc];
	    else {
		if ( ref->unicode_enc<0x10000 )
		    sprintf( buf, "uni%04X", ref->unicode_enc );
		else
		    sprintf( buf, "u%04X", ref->unicode_enc );
		name = buf;
	    }
	    yes = GWidgetAskCenteredR(_STR_BadReference,buts,1,1,_STR_FontNoRefNoOrig,name,sc->name);
	    if ( yes==0 )
		*refstate |= 0x4;
	}
    } else {
	if ( !(*refstate&0x3) ) {
	    static int buts[] = { _STR_Yes, _STR_YesToAll, _STR_NoToAll, _STR_No, 0 };
	    GProgressPauseTimer();
	    yes = GWidgetAskCenteredR(_STR_BadReference,buts,0,3,_STR_FontNoRef,rsc->name,sc->name);
	    GProgressResumeTimer();
	    if ( yes==1 )
		*refstate |= 1;
	    else if ( yes==2 )
		*refstate |= 2;
	}
	if ( (*refstate&1) || yes<=1 ) {
	    new = SplinePointListTransform(SplinePointListCopy(rsc->layers[ly_fore].splines),ref->transform,true);
	    SplinePointListSelect(new,true);
	    if ( new!=NULL ) {
		for ( spl = new; spl->next!=NULL; spl = spl->next );
		spl->next = sc->layers[ly_fore].splines;
		sc->layers[ly_fore].splines = new;
	    }
	}
    }
}

static void SCCheckXClipboard(GWindow awindow,SplineChar *sc,int layer,int doclear) {
    int type, len;
    char *paste;
    FILE *temp;
    GImage *image;

    if ( screen_display==NULL )
return;
    type = 0;
#ifndef _NO_LIBPNG
    if ( GDrawSelectionHasType(awindow,sn_clipboard,"image/png") )
	type = 1;
    else
#endif
    if ( GDrawSelectionHasType(awindow,sn_clipboard,"image/bmp") )
	type = 2;
    else if ( GDrawSelectionHasType(awindow,sn_clipboard,"image/eps") )
	type = 3;

    if ( type==0 )
return;

    paste = GDrawRequestSelection(awindow,sn_clipboard,type==1?"image/png":
		type==2?"image/bmp":"image/eps",&len);
    if ( paste==NULL )
return;

    temp = tmpfile();
    if ( temp!=NULL ) {
	fwrite(paste,1,len,temp);
	rewind(temp);
	if ( type==3 ) {
	    SCImportPSFile(sc,layer,temp,doclear);
	} else {
#ifndef _NO_LIBPNG
	    if ( type==1 )
		image = GImageRead_Png(temp);
	    else
#endif
		image = GImageRead_Bmp(temp);
	    SCAddScaleImage(sc,image,doclear,layer);
	}
	fclose(temp);
    }
    free(paste);
}

/* When we paste a composit character from one font to another the references */
/*  refer to the glyphs in the new font. So the width should refer to the */
/*  width of the glyphs in the new font as well */
static int PasteGuessCorrectWidth(SplineFont *sf,Undoes *paster,int *vwidth) {
    RefChar *ref, *base=NULL;

    if ( paster->u.state.copied_from==NULL ||
	    paster->u.state.vwidth == paster->u.state.copied_from->ascent+paster->u.state.copied_from->descent )
	*vwidth = sf->ascent+sf->descent;
    for ( ref=paster->u.state.refs; ref!=NULL; ref=ref->next ) {
	if ( ref->unicode_enc!=-1 && ref->unicode_enc<0x10000 &&
		isalpha(ref->unicode_enc) && !iscombining(ref->unicode_enc) &&
		ref->transform[0]==1 && ref->transform[3]==1 &&
		ref->transform[1]==0 && ref->transform[2]==0 &&
		ref->transform[4]==0 && ref->transform[5]==0 ) {
	    if ( base!=NULL ) {
		base= NULL;
    break;
	    }
	    base = ref;
	}
    }
    if ( base!=NULL ) {
	SplineChar *sc = FindCharacter(sf,base);
	*vwidth = sc->vwidth;
return( sc->width );
    }
    *vwidth = paster->u.state.vwidth;
return( paster->u.state.width );
}

static double PasteFigureScale(SplineFont *newsf,SplineFont *oldsf) {
    FontView *fv;

    if ( newsf==oldsf )
return( 1.0 );
    for ( fv = fv_list; fv!=NULL && fv->sf!=oldsf; fv=fv->next );
    if ( fv==NULL )		/* Font we copied from has been closed */
return( 1.0 );

return( (newsf->ascent+newsf->descent) / (double) (oldsf->ascent+oldsf->descent) );
}

static int anchor_lost_warning = false;

static void APMerge(SplineChar *sc,AnchorPoint *anchor) {
    AnchorPoint *ap, *prev, *next, *test;
    AnchorClass *ac;

    if ( anchor==NULL )
return;
    anchor = AnchorPointsCopy(anchor);
    /* If we pasted from one font to another, the anchor class list will be */
    /*  different. */
    for ( ac = sc->parent->anchor; ac!=NULL && ac!=anchor->anchor; ac=ac->next );
    if ( ac==NULL ) {		/* Into a different font. See if we can find a class with same name in new font */
	prev = NULL;
	for ( ap = anchor; ap!=NULL; ap=next ) {
	    next = ap->next;
	    for ( ac = sc->parent->anchor; ac!=NULL && u_strcmp(ac->name,ap->anchor->name)!=0; ac = ac->next );
	    if ( ac!=NULL ) {
		ap->anchor = ac;
		prev = ap;
	    } else {
		if ( prev==NULL )
		    anchor = next;
		else
		    prev->next = next;
		chunkfree(ap,sizeof(AnchorPoint));
		anchor_lost_warning = true;
	    }
	}
	if ( anchor_lost_warning )
	    GWidgetErrorR(_STR_AnchorLost,_STR_AnchorLostPaste);
	if ( anchor==NULL )
return;
    }
    if ( sc->anchor==NULL ) {
	sc->anchor = anchor;
return;
    }

    prev = NULL;
    for ( ap=anchor; ap!=NULL; ap=next ) {
	next = ap->next;
	for ( test=sc->anchor; test!=NULL; test=test->next )
	    if ( test->anchor==ap->anchor ) {
		if (( test->type==at_centry && ap->type==at_cexit) ||
			(test->type==at_cexit && ap->type==at_centry))
		    /* It's ok */;
		else if ( test->type!=at_baselig || ap->type!=at_baselig ||
			test->lig_index==ap->lig_index )
	break;
	    }
	if ( test!=NULL ) {
	    GWidgetErrorR(_STR_DupAnchor,_STR_DupAnchorIn,test->anchor->name,sc->name);
	    if ( prev==NULL )
		anchor = next;
	    else
		prev->next = next;
	    chunkfree(ap,sizeof(AnchorPoint));
	} else
	    prev = ap;
    }
    if ( prev!=NULL ) {
	prev->next = sc->anchor;
	sc->anchor = anchor;
    }
}

/* when pasting from the fontview we do a clear first */
#ifdef FONTFORGE_CONFIG_TYPE3
static void _PasteToSC(SplineChar *sc,Undoes *paster,FontView *fv,int doclear,
	int layer) {
#else
static void PasteToSC(SplineChar *sc,Undoes *paster,FontView *fv,int doclear) {
    const int layer = ly_fore;
#endif
    DBounds bb;
    real transform[6];
    int width, vwidth;
    FontView *fvs;

    switch ( paster->undotype ) {
      case ut_noop:
      break;
      case ut_state: case ut_statehint: case ut_statename:
	if ( paster->u.state.splines!=NULL || paster->u.state.refs!=NULL )
	    sc->parent->onlybitmaps = false;
	SCPreserveState(sc,paster->undotype==ut_statehint);
	width = paster->u.state.width;
	vwidth = paster->u.state.vwidth;
	if ( doclear && paster->u.state.copied_from!=sc->parent &&
		paster->u.state.splines==NULL &&
		paster->u.state.refs!=NULL )
	    width = PasteGuessCorrectWidth(sc->parent,paster,&vwidth);
	if ( !sc->parent->onlybitmaps )
	    SCSynchronizeWidth(sc,width,sc->width,fv);
	sc->vwidth = vwidth;
	if ( doclear ) {
	    SplinePointListsFree(sc->layers[layer].splines);
	    sc->layers[layer].splines = NULL;
	    ImageListsFree(sc->layers[layer].images);
	    sc->layers[layer].images = NULL;
	    SCRemoveLayerDependents(sc,layer);
	    AnchorPointsFree(sc->anchor);
	    sc->anchor = NULL;
	}
#ifdef FONTFORGE_CONFIG_TYPE3
	if ( layer>=ly_fore && sc->layers[layer].splines==NULL &&
		sc->layers[layer].refs==NULL && sc->layers[layer].images==NULL &&
		sc->parent->multilayer ) {
	    /* pasting into an empty layer sets the fill/stroke */
	    sc->layers[layer].fill_brush = paster->u.state.fill_brush;
	    sc->layers[layer].stroke_pen = paster->u.state.stroke_pen;
	    sc->layers[layer].dofill = paster->u.state.dofill;
	    sc->layers[layer].dostroke = paster->u.state.dostroke;
	    sc->layers[layer].fillfirst = paster->u.state.fillfirst;
	}
#endif
	if ( paster->u.state.splines!=NULL ) {
	    SplinePointList *temp = SplinePointListCopy(paster->u.state.splines);
	    if ( paster->was_order2 != sc->parent->order2 )
		temp = SplineSetsConvertOrder(temp,sc->parent->order2);
	    if ( sc->layers[layer].splines!=NULL ) {
		SplinePointList *e = sc->layers[layer].splines;
		while ( e->next!=NULL ) e = e->next;
		e->next = temp;
	    } else
		sc->layers[layer].splines = temp;
	}
	if ( !sc->searcherdummy )
	    APMerge(sc,paster->u.state.anchor);
#ifdef FONTFORGE_CONFIG_TYPE3
	if ( paster->u.state.u.images!=NULL && sc->parent->multilayer ) {
	    ImageList *new, *cimg;
	    for ( cimg = paster->u.state.u.images; cimg!=NULL; cimg=cimg->next ) {
		new = galloc(sizeof(ImageList));
		*new = *cimg;
		new->selected = true;
		new->next = sc->layers[layer].images;
		sc->layers[layer].images = new;
	    }
	    SCOutOfDateBackground(sc);
	}
#else
	/* Ignore any images, can't be in foreground level */
	/* but might be hints */
#endif
	if ( paster->undotype==ut_statehint || paster->undotype==ut_statename )
	    if ( doclear )	/* Hints aren't meaningful unless we've cleared first */
		ExtractHints(sc,paster->u.state.u.hints,true);
	if ( paster->undotype==ut_statename ) {
	    SCSetMetaData(sc,paster->u.state.charname,
		    paster->u.state.unicodeenc==0xffff?-1:paster->u.state.unicodeenc,
		    paster->u.state.comment);
	    PSTFree(sc->possub);
	    for ( fvs = fv_list; fvs!=NULL && fvs->sf!=paster->u.state.copied_from; fvs=fvs->next );
	    sc->possub = PSTCopy(paster->u.state.possub,sc,fvs==NULL?NULL:fvs->sf);
	}
	if ( paster->u.state.refs!=NULL ) {
	    RefChar *new, *refs;
	    SplineChar *rsc;
	    double scale = PasteFigureScale(sc->parent,paster->u.state.copied_from);
	    for ( refs = paster->u.state.refs; refs!=NULL; refs=refs->next ) {
		if ( sc->searcherdummy )
		    rsc = FindCharacter(sc->views->searcher->fv->sf,refs);
		else
		    rsc = FindCharacter(sc->parent,refs);
		if ( rsc!=NULL && SCDependsOnSC(rsc,sc))
		    GWidgetErrorR(_STR_SelfRef,_STR_AttemptSelfRef);
		else if ( rsc!=NULL ) {
		    new = RefCharCreate();
		    *new = *refs;
		    new->transform[4] *= scale; new->transform[5] *= scale;
		    new->layers[0].splines = NULL;
		    new->sc = rsc;
		    new->next = sc->layers[layer].refs;
		    sc->layers[layer].refs = new;
		    SCReinstanciateRefChar(sc,new);
		    SCMakeDependent(sc,rsc);
		} else {
		    int refstate = fv->refstate;
		    PasteNonExistantRefCheck(sc,paster,refs,&refstate);
		    fv->refstate = refstate;
		}
	    }
	}
	SCCharChangedUpdate(sc);
      break;
      case ut_possub:
	SCAppendPosSub(sc,paster->u.possub.pst,paster->u.possub.data);
      break;
      case ut_width:
	SCPreserveWidth(sc);
	SCSynchronizeWidth(sc,paster->u.width,sc->width,fv);
	SCCharChangedUpdate(sc);
      break;
      case ut_vwidth:
	if ( !sc->parent->hasvmetrics )
	    GWidgetErrorR(_STR_NoVerticalMetrics,_STR_FontNoVerticalMetrics);
	else {
	    SCPreserveVWidth(sc);
	    sc->vwidth = paster->u.width;
	    SCCharChangedUpdate(sc);
	}
      break;
      case ut_rbearing:
	SCPreserveWidth(sc);
	SplineCharFindBounds(sc,&bb);
	SCSynchronizeWidth(sc,bb.maxx + paster->u.rbearing,sc->width,fv);
	SCCharChangedUpdate(sc);
      break;
      case ut_lbearing:
	SplineCharFindBounds(sc,&bb);
	transform[0] = transform[3] = 1.0;
	transform[1] = transform[2] = transform[5] = 0;
	transform[4] = paster->u.lbearing-bb.minx;
	if ( transform[4]!=0 )
	    FVTrans(fv,sc,transform,NULL,false);
	/* FVTrans will preserve the state and update the chars */
      break;
    }
}

#ifdef FONTFORGE_CONFIG_TYPE3
static void PasteToSC(SplineChar *sc,Undoes *paster,FontView *fv,int doclear) {
    if ( paster->undotype==ut_layers && sc->parent->multilayer ) {
	int lc, start, layer;
	Undoes *pl;
	for ( lc=0, pl = paster->u.multiple.mult; pl!=NULL; pl=pl->next, ++lc );
	if ( doclear ) {
	    start = ly_fore;
	    for ( layer=ly_fore; layer<sc->layer_cnt; ++layer ) {
		SplinePointListsFree(sc->layers[layer].splines);
		sc->layers[layer].splines = NULL;
		ImageListsFree(sc->layers[layer].images);
		sc->layers[layer].images = NULL;
		SCRemoveLayerDependents(sc,layer);
	    }
	} else
	    start = sc->layer_cnt;
	if ( start+lc > sc->layer_cnt ) {
	    sc->layers = grealloc(sc->layers,(start+lc)*sizeof(Layer));
	    for ( layer = sc->layer_cnt; layer<start+lc; ++layer )
		LayerDefault(&sc->layers[layer]);
	    sc->layer_cnt = start+lc;
	}
	for ( lc=0, pl = paster->u.multiple.mult; pl!=NULL; pl=pl->next, ++lc )
	    _PasteToSC(sc,pl,fv,doclear,start+lc);
	SCMoreLayers(sc);
    } else if ( paster->undotype==ut_layers ) {
	Undoes *pl;
	for ( pl = paster->u.multiple.mult; pl!=NULL; pl=pl->next );
	    _PasteToSC(sc,pl,fv,doclear,ly_fore);
    } else
	_PasteToSC(sc,paster,fv,doclear,ly_fore);
}
#endif

static void _PasteToCV(CharView *cv,SplineChar *cvsc,Undoes *paster) {
    int refstate = 0;
    DBounds bb;
    real transform[6];

    if ( copybuffer.undotype == ut_none ) {
	SCCheckXClipboard(cv->gw,cvsc,cv->drawmode,false);
return;
    }

    anchor_lost_warning = false;

    cv->lastselpt = NULL;
    switch ( paster->undotype ) {
      case ut_noop:
      break;
      case ut_state: case ut_statehint: case ut_statename:
	if ( cv->drawmode==dm_fore && cvsc->layers[ly_fore].splines==NULL && cvsc->layers[ly_fore].refs==NULL ) {
	    SCSynchronizeWidth(cvsc,paster->u.state.width,cvsc->width,NULL);
	    cvsc->vwidth = paster->u.state.vwidth;
	}
#ifdef FONTFORGE_CONFIG_TYPE3
	if ( cv->drawmode==dm_fore && cv->layerheads[dm_fore]->splines==NULL &&
		cv->layerheads[dm_fore]->refs==NULL && cv->layerheads[dm_fore]->images==NULL &&
		cvsc->parent->multilayer ) {
	    /* pasting into an empty layer sets the fill/stroke */
	    cv->layerheads[dm_fore]->fill_brush = paster->u.state.fill_brush;
	    cv->layerheads[dm_fore]->stroke_pen = paster->u.state.stroke_pen;
	    cv->layerheads[dm_fore]->dofill = paster->u.state.dofill;
	    cv->layerheads[dm_fore]->dostroke = paster->u.state.dostroke;
	    cv->layerheads[dm_fore]->fillfirst = paster->u.state.fillfirst;
	}
#endif
	if ( paster->u.state.splines!=NULL ) {
	    SplinePointList *spl, *new = SplinePointListCopy(paster->u.state.splines);
	    if ( paster->was_order2 != cvsc->parent->order2 )
		new = SplineSetsConvertOrder(new,cvsc->parent->order2 );
	    SplinePointListSelect(new,true);
	    for ( spl = new; spl->next!=NULL; spl = spl->next );
	    spl->next = cv->layerheads[cv->drawmode]->splines;
	    cv->layerheads[cv->drawmode]->splines = new;
	}
	if ( paster->undotype==ut_state && paster->u.state.u.images!=NULL ) {
#ifdef FONTFORGE_CONFIG_TYPE3
	    int dm = cvsc->parent->multilayer ? cv->drawmode : dm_back;
#else
	    const int dm = dm_back;
	    /* Images can only be pasted into background, so do that */
	    /*  even if we aren't in background mode */
#endif
	    ImageList *new, *cimg;
	    for ( cimg = paster->u.state.u.images; cimg!=NULL; cimg=cimg->next ) {
		new = galloc(sizeof(ImageList));
		*new = *cimg;
		new->selected = true;
		new->next = cv->layerheads[dm]->images;
		cv->layerheads[dm]->images = new;
	    }
	    SCOutOfDateBackground(cvsc);
	} else if ( paster->undotype==ut_statehint && cv->searcher==NULL )
	    ExtractHints(cvsc,paster->u.state.u.hints,true);
	if ( paster->u.state.anchor!=NULL && cv->drawmode==dm_fore && !cvsc->searcherdummy )
	    APMerge(cvsc,paster->u.state.anchor);
	if ( paster->u.state.refs!=NULL && cv->drawmode==dm_fore ) {
	    RefChar *new, *refs;
	    SplineChar *sc;
	    for ( refs = paster->u.state.refs; refs!=NULL; refs=refs->next ) {
		if ( cv->searcher!=NULL )
		    sc = FindCharacter(cv->searcher->fv->sf,refs);
		else
		    sc = FindCharacter(cvsc->parent,refs);
		if ( sc!=NULL && SCDependsOnSC(sc,cvsc))
		    GWidgetErrorR(_STR_SelfRef,_STR_AttemptSelfRef);
		else if ( sc!=NULL ) {
		    new = RefCharCreate();
		    *new = *refs;
		    new->layers[0].splines = NULL;
		    new->sc = sc;
		    new->selected = true;
		    new->next = cvsc->layers[ly_fore].refs;
		    cvsc->layers[ly_fore].refs = new;
		    SCReinstanciateRefChar(cvsc,new);
		    SCMakeDependent(cvsc,sc);
		} else {
		    PasteNonExistantRefCheck(cvsc,paster,refs,&refstate);
		}
	    }
	} else if ( paster->u.state.refs!=NULL && cv->drawmode==dm_back ) {
	    /* Paste the CONTENTS of the referred character into this one */
	    /*  (background contents I think) */
	    RefChar *refs;
	    SplineChar *sc;
	    SplinePointList *new, *spl;
	    for ( refs = paster->u.state.refs; refs!=NULL; refs=refs->next ) {
		if ( cv->searcher!=NULL )
		    sc = FindCharacter(cv->searcher->fv->sf,refs);
		else
		    sc = FindCharacter(cvsc->parent,refs);
		if ( sc!=NULL ) {
		    new = SplinePointListTransform(SplinePointListCopy(sc->layers[ly_back].splines),refs->transform,true);
		    SplinePointListSelect(new,true);
		    if ( new!=NULL ) {
			for ( spl = new; spl->next!=NULL; spl = spl->next );
			spl->next = cvsc->layers[ly_back].splines;
			cvsc->layers[ly_back].splines = new;
		    }
		}
	    }
	}
	if ( paster->undotype==ut_statename ) {
	    SCSetMetaData(cvsc,paster->u.state.charname,
		    paster->u.state.unicodeenc==0xffff?-1:paster->u.state.unicodeenc,
		    paster->u.state.comment);
	    PSTFree(cvsc->possub);
	    cvsc->possub = paster->u.state.possub;
	}
      break;
      case ut_possub:
	SCAppendPosSub(cvsc,paster->u.possub.pst,paster->u.possub.data);
      break;
      case ut_width:
	SCSynchronizeWidth(cvsc,paster->u.width,cvsc->width,NULL);
      break;
      case ut_vwidth:
	if ( !cvsc->parent->hasvmetrics )
	    GWidgetErrorR(_STR_NoVerticalMetrics,_STR_FontNoVerticalMetrics);
	else
	    cvsc->vwidth = paster->u.state.vwidth;
      break;
      case ut_rbearing:
	SplineCharFindBounds(cvsc,&bb);
	SCSynchronizeWidth(cvsc,bb.maxx + paster->u.rbearing,cvsc->width,cv->fv);
      break;
      case ut_lbearing:
	SplineCharFindBounds(cvsc,&bb);
	transform[0] = transform[3] = 1.0;
	transform[1] = transform[2] = transform[5] = 0;
	transform[4] = paster->u.lbearing-bb.minx;
	if ( transform[4]!=0 )
	    FVTrans(cv->fv,cvsc,transform,NULL,false);
	/* FVTrans will preserve the state and update the chars */
	/* CVPaste depends on this behavior */
      break;
      case ut_composit:
	if ( paster->u.composit.state!=NULL )
	    _PasteToCV(cv,cvsc,paster->u.composit.state);
      break;
      case ut_multiple: case ut_layers:
	_PasteToCV(cv,cvsc,paster->u.multiple.mult);
      break;
    }
}

void PasteToCV(CharView *cv) {
    _PasteToCV(cv,cv->sc,&copybuffer);
    if ( cv->sc->blended && cv->drawmode==dm_fore ) {
	int j, enc = cv->sc->enc;
	MMSet *mm = cv->sc->parent->mm;
	for ( j=0; j<mm->instance_count; ++j )
	    _PasteToCV(cv,mm->instances[j]->chars[enc],&copybuffer);
    }
}


SplineSet *ClipBoardToSplineSet(void) {
    Undoes *paster = &copybuffer;

    while ( paster!=NULL ) {
	switch ( paster->undotype ) {
	  default:
	  case ut_noop: case ut_none:
return( NULL );
	  break;
	  case ut_state: case ut_statehint: case ut_statename:
	    if ( paster->u.state.refs!=NULL )
return( NULL );

return( paster->u.state.splines );
	  break;
	  case ut_width:
return( NULL );
	  case ut_vwidth:
return( NULL );
	  case ut_rbearing:
return( NULL );
	  case ut_lbearing:
return( NULL );
	  case ut_composit:
	    paster = paster->u.composit.state;
	  break;
	  case ut_multiple:
	    paster = paster->u.multiple.mult;
	  break;
	}
    }
return( NULL );
}

void BCCopySelected(BDFChar *bc,int pixelsize,int depth) {

    CopyBufferFreeGrab();

    memset(&copybuffer,'\0',sizeof(copybuffer));
    copybuffer.undotype = ut_bitmapsel;
    if ( bc->selection!=NULL )
	copybuffer.u.bmpstate.selection = BDFFloatCopy(bc->selection);
    else
	copybuffer.u.bmpstate.selection = BDFFloatCreate(bc,bc->xmin,bc->xmax,
		bc->ymin,bc->ymax, false);
    copybuffer.u.bmpstate.pixelsize = pixelsize;
    copybuffer.u.bmpstate.depth = depth;
}

static Undoes *BCCopyAll(BDFChar *bc,int pixelsize, int depth) {
    Undoes *cur;

    cur = chunkalloc(sizeof(Undoes));
    if ( bc==NULL )
	cur->undotype = ut_noop;
    else {
	cur->undotype = ut_bitmap;
	cur->u.bmpstate.width = bc->width;
	cur->u.bmpstate.xmin = bc->xmin;
	cur->u.bmpstate.xmax = bc->xmax;
	cur->u.bmpstate.ymin = bc->ymin;
	cur->u.bmpstate.ymax = bc->ymax;
	cur->u.bmpstate.bytes_per_line = bc->bytes_per_line;
	cur->u.bmpstate.bitmap = bmpcopy(bc->bitmap,bc->bytes_per_line,
		bc->ymax-bc->ymin+1);
	cur->u.bmpstate.selection = BDFFloatCopy(bc->selection);
    }
    cur->u.bmpstate.pixelsize = pixelsize;
    cur->u.bmpstate.depth = depth;
return( cur );
}

static void _PasteToBC(BDFChar *bc,int pixelsize, int depth, Undoes *paster, int clearfirst, FontView *fv) {
    BDFFloat temp;

    switch ( paster->undotype ) {
      case ut_noop:
      break;
      case ut_bitmapsel:
	BCPreserveState(bc);
	BCFlattenFloat(bc);
	if ( clearfirst )
	    memset(bc->bitmap,'\0',bc->bytes_per_line*(bc->ymax-bc->ymin+1));
	bc->selection = BDFFloatConvert(paster->u.bmpstate.selection,depth,paster->u.bmpstate.depth);
	BCCharChangedUpdate(bc);
      break;
      case ut_bitmap:
	BCPreserveState(bc);
	BCFlattenFloat(bc);
	memset(bc->bitmap,0,bc->bytes_per_line*(bc->ymax-bc->ymin+1));
	temp.xmin = paster->u.bmpstate.xmin; temp.xmax = paster->u.bmpstate.xmax;
	temp.ymin = paster->u.bmpstate.ymin; temp.ymax = paster->u.bmpstate.ymax;
	temp.bytes_per_line = paster->u.bmpstate.bytes_per_line; temp.byte_data = depth!=1;
	temp.bitmap = paster->u.bmpstate.bitmap;
	bc->selection = BDFFloatConvert(&temp,depth,paster->u.bmpstate.depth);
	BCFlattenFloat(bc);
	BCCompressBitmap(bc);
	bc->selection = BDFFloatConvert(paster->u.bmpstate.selection,depth,paster->u.bmpstate.depth);
	bc->width = paster->u.bmpstate.width;
	BCCharChangedUpdate(bc);
      break;
      case ut_composit:
	/* if there's only one bitmap and no outline state (so we only copied a bitmap) */
	/*  then paste that thing with no questions. Otherwise search for a */
	/*  bitmap with the right pixel size. If we find it, paste it, else */
	/*  noop */
	if ( paster->u.composit.bitmaps==NULL )
	    /* Nothing to be done */;
	else if ( paster->u.composit.state==NULL && paster->u.composit.bitmaps->next==NULL )
	    _PasteToBC(bc,pixelsize,depth,paster->u.composit.bitmaps,clearfirst,fv);
	else {
	    Undoes *b;
	    for ( b = paster->u.composit.bitmaps;
		    b!=NULL && b->u.bmpstate.pixelsize!=pixelsize;
		    b = b->next );
	    if ( b!=NULL )
		_PasteToBC(bc,pixelsize,depth,paster->u.composit.bitmaps,clearfirst,fv);
	}
      break;
      case ut_multiple:
	_PasteToBC(bc,pixelsize,depth,paster->u.multiple.mult,clearfirst,fv);
      break;
    }
}

void PasteToBC(BDFChar *bc,int pixelsize,int depth,FontView *fv) {
    _PasteToBC(bc,pixelsize,depth,&copybuffer,false,fv);
}

void FVCopyWidth(FontView *fv,enum undotype ut) {
    Undoes *head=NULL, *last=NULL, *cur;
    int i, any=false;
    SplineChar *sc;
    DBounds bb;

    CopyBufferFreeGrab();

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
	any = true;
	cur = chunkalloc(sizeof(Undoes));
	cur->undotype = ut;
	if ( (sc=fv->sf->chars[i])!=NULL ) {
	    switch ( ut ) {
	      case ut_width:
		cur->u.width = sc->width;
	      break;
	      case ut_vwidth:
		cur->u.width = sc->width;
	      break;
	      case ut_lbearing:
		SplineCharFindBounds(sc,&bb);
		cur->u.lbearing = bb.minx;
	      break;
	      case ut_rbearing:
		SplineCharFindBounds(sc,&bb);
		cur->u.rbearing = sc->width-bb.maxx;
	      break;
	    }
	} else
	    cur->undotype = ut_noop;
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
    copybuffer.undotype = ut_multiple;
    copybuffer.u.multiple.mult = head;
    if ( !any )
	fprintf( stderr, "No selection\n" );
}

void FVCopy(FontView *fv, int fullcopy) {
    int i, any = false;
    BDFFont *bdf;
    Undoes *head=NULL, *last=NULL, *cur;
    Undoes *bhead=NULL, *blast=NULL, *bcur;
    Undoes *state;
    extern int onlycopydisplayed;

    for ( i=0; i<fv->sf->charcnt; ++i ) if ( fv->selected[i] ) {
	any = true;
	if ( onlycopydisplayed && fv->filled==fv->show ) {
	    cur = SCCopyAll(fv->sf->chars[i],fullcopy);
	} else if ( onlycopydisplayed ) {
	    cur = BCCopyAll(fv->show->chars[i],fv->show->pixelsize,BDFDepth(fv->show));
	} else {
	    state = SCCopyAll(fv->sf->chars[i],fullcopy);
	    bhead = NULL;
	    for ( bdf=fv->sf->bitmaps; bdf!=NULL; bdf = bdf->next ) {
		bcur = BCCopyAll(bdf->chars[i],bdf->pixelsize,BDFDepth(bdf));
		if ( bhead==NULL )
		    bhead = bcur;
		else
		    blast->next = bcur;
		blast = bcur;
	    }
	    if ( bhead!=NULL || state!=NULL ) {
		cur = chunkalloc(sizeof(Undoes));
		cur->undotype = ut_composit;
		cur->u.composit.state = state;
		cur->u.composit.bitmaps = bhead;
	    } else
		cur = NULL;
	}
	if ( cur!=NULL ) {
	    if ( head==NULL )
		head = cur;
	    else
		last->next = cur;
	    last = cur;
	}
    }

    if ( !any )
	fprintf( stderr, "No selection\n" );

    if ( head==NULL )
return;
    CopyBufferFreeGrab();
    copybuffer.undotype = ut_multiple;
    copybuffer.u.multiple.mult = head;

    XClipCheckEps();
}

void MVCopyChar(MetricsView *mv, SplineChar *sc, int fullcopy) {
    BDFFont *bdf;
    Undoes *cur=NULL;
    Undoes *bhead=NULL, *blast=NULL, *bcur;
    Undoes *state;
    extern int onlycopydisplayed;

    if ( onlycopydisplayed && mv->bdf==NULL ) {
	cur = SCCopyAll(sc,fullcopy);
    } else if ( onlycopydisplayed ) {
	cur = BCCopyAll(mv->bdf->chars[sc->enc],mv->bdf->pixelsize,BDFDepth(mv->bdf));
    } else {
	state = SCCopyAll(sc,fullcopy);
	bhead = NULL;
	for ( bdf=mv->fv->sf->bitmaps; bdf!=NULL; bdf = bdf->next ) {
	    bcur = BCCopyAll(bdf->chars[sc->enc],bdf->pixelsize,BDFDepth(bdf));
	    if ( bhead==NULL )
		bhead = bcur;
	    else
		blast->next = bcur;
	    blast = bcur;
	}
	if ( bhead!=NULL || state!=NULL ) {
	    cur = chunkalloc(sizeof(Undoes));
	    cur->undotype = ut_composit;
	    cur->u.composit.state = state;
	    cur->u.composit.bitmaps = bhead;
	} else
	    cur = NULL;
    }

    if ( cur==NULL )
return;
    CopyBufferFreeGrab();
    copybuffer.undotype = ut_multiple;
    copybuffer.u.multiple.mult = cur;

    XClipCheckEps();
}

static BDFFont *BitmapCreateCheck(FontView *fv,int *yestoall, int first, int pixelsize, int depth) {
    int yes = 0;
    BDFFont *bdf = NULL;

    if ( *yestoall>0 && first ) {
	static int buts[] = { _STR_Yes, _STR_YesToAll, _STR_NoToAll, _STR_No, 0 };
	char buf[20]; unichar_t ubuf[400];
	if ( depth!=1 )
	    sprintf( buf, "%d@%d", pixelsize, depth );
	else
	    sprintf( buf, "%d", pixelsize );
	u_strcpy(ubuf,GStringGetResource(_STR_ClipContainsPre,NULL));
	uc_strcat(ubuf,buf);
	u_strcat(ubuf,GStringGetResource(_STR_ClipContainsPost,NULL));
	yes = GWidgetAskCenteredR_(_STR_BitmapPaste,buts,0,3,ubuf);
	if ( yes==1 )
	    *yestoall = true;
	else if ( yes==2 )
	    *yestoall = -1;
	else
	    yes= yes!=3;
    }
    if ( yes==1 || *yestoall ) {
	void *freetypecontext = FreeTypeFontContext(fv->sf,NULL,NULL);
	if ( freetypecontext )
	    bdf = SplineFontFreeTypeRasterize(freetypecontext,pixelsize,depth);
	else
	    bdf = SplineFontAntiAlias(fv->sf,pixelsize,1<<(depth/2));
	bdf->next = fv->sf->bitmaps;
	fv->sf->bitmaps = bdf;
	fv->sf->changed = true;
	SFOrderBitmapList(fv->sf);
    }
return( bdf );
}

void PasteIntoFV(FontView *fv,int doclear) {
    Undoes *cur=NULL, *bmp;
    BDFFont *bdf;
    int i, j, cnt=0;
    int yestoall=0, first=true;
    uint8 *oldsel = fv->selected;
    extern int onlycopydisplayed;
    SplineFont *sf = fv->sf, *origsf = sf;
    MMSet *mm = sf->mm;

    fv->refstate = 0;

    cur = &copybuffer;
    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] )
	++cnt;
    if ( cnt==0 ) {
	fprintf( stderr, "No Selection\n" );
return;
    }

    if ( copybuffer.undotype == ut_none ) {
	j = -1;
	forever {
	    for ( i=0; i<sf->charcnt; ++i ) if ( fv->selected[i] )
		SCCheckXClipboard(fv->gw,SFMakeChar(sf,i),dm_fore,doclear);
	    ++j;
	    if ( mm==NULL || mm->normal!=origsf || j>=mm->instance_count )
	break;
	    sf = mm->instances[j];
	}
return;
    }

    /* If they select exactly one character but there are more things in the */
    /*  copy buffer, then temporarily change the selection so that everything*/
    /*  in the copy buffer gets pasted (into chars immediately following sele*/
    /*  cted one (unless we run out of chars...)) */
    if ( cnt==1 && cur->undotype==ut_multiple && cur->u.multiple.mult->next!=NULL ) {
	Undoes *tot; int j;
	for ( cnt=0, tot=cur->u.multiple.mult; tot!=NULL; ++cnt, tot=tot->next );
	fv->selected = galloc(sf->charcnt);
	memcpy(fv->selected,oldsel,sf->charcnt);
	for ( i=0; i<sf->charcnt && !fv->selected[i]; ++i );
	for ( j=0; j<cnt && i+j<sf->charcnt; ++j )
	    fv->selected[i+j] = 1;
	cnt = j;
    }

    anchor_lost_warning = false;
    GProgressStartIndicatorR(10,_STR_Pasting,_STR_Pasting,0,cnt,1);

    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    /* This little gem of code is to keep us from throwing out forward */
    /*  references. Say we are pasting into both "i" and dotlessi (and */
    /*  dotlessi isn't defined yet) without this the paste to "i" will */
    /*  search for dotlessi, not find it and ignore the reference */
    if ( cur->undotype==ut_state || cur->undotype==ut_statehint || cur->undotype==ut_statename ||
	    (cur->undotype==ut_composit && cur->u.composit.state!=NULL)) {
	for ( i=0; i<sf->charcnt; ++i )
	    if ( fv->selected[i] && sf->chars[i]==NULL )
		SFMakeChar(sf,i);
    }
    cur = NULL;

    for ( i=0; i<origsf->charcnt; ++i ) if ( fv->selected[i] ) {
	j=-1;
	if ( cur==NULL ) {
	    cur = &copybuffer;
	    if ( cur->undotype==ut_multiple )
		cur = cur->u.multiple.mult;
	}
	forever {
	    switch ( cur->undotype ) {
	      case ut_noop:
	      break;
	      case ut_state: case ut_width: case ut_vwidth:
	      case ut_lbearing: case ut_rbearing: case ut_possub:
	      case ut_statehint: case ut_statename:
	      case ut_layers:
		if ( !sf->hasvmetrics && cur->undotype==ut_vwidth) {
		    GWidgetErrorR(_STR_NoVerticalMetrics,_STR_FontNoVerticalMetrics);
 goto err;
		}
		PasteToSC(SFMakeChar(sf,i),cur,fv,doclear);
	      break;
	      case ut_bitmapsel: case ut_bitmap:
		if ( onlycopydisplayed && fv->show!=fv->filled )
		    _PasteToBC(BDFMakeChar(fv->show,i),fv->show->pixelsize,BDFDepth(fv->show),cur,doclear,fv);
		else {
		    for ( bdf=sf->bitmaps; bdf!=NULL && (bdf->pixelsize!=cur->u.bmpstate.pixelsize || BDFDepth(bdf)!=cur->u.bmpstate.depth); bdf=bdf->next );
		    if ( bdf==NULL ) {
			bdf = BitmapCreateCheck(fv,&yestoall,first,cur->u.bmpstate.pixelsize,cur->u.bmpstate.depth);
			first = false;
		    }
		    if ( bdf!=NULL )
			_PasteToBC(BDFMakeChar(bdf,i),bdf->pixelsize,BDFDepth(bdf),cur,doclear,fv);
		}
	      break;
	      case ut_composit:
		if ( cur->u.composit.state!=NULL )
		    PasteToSC(SFMakeChar(sf,i),cur->u.composit.state,fv,doclear);
		for ( bmp=cur->u.composit.bitmaps; bmp!=NULL; bmp = bmp->next ) {
		    for ( bdf=sf->bitmaps; bdf!=NULL &&
			    (bdf->pixelsize!=bmp->u.bmpstate.pixelsize || BDFDepth(bdf)!=bmp->u.bmpstate.depth);
			    bdf=bdf->next );
		    if ( bdf==NULL )
			bdf = BitmapCreateCheck(fv,&yestoall,first,bmp->u.bmpstate.pixelsize,bmp->u.bmpstate.depth);
		    if ( bdf!=NULL )
			_PasteToBC(BDFMakeChar(bdf,i),bdf->pixelsize,BDFDepth(bdf),bmp,doclear,fv);
		}
		first = false;
	      break;
	    }
	    ++j;
	    if ( mm==NULL || mm->normal!=origsf || j>=mm->instance_count )
	break;
	    sf = mm->instances[j];
	}
	cur = cur->next;
	if ( !GProgressNext())
    break;
    }
 err:
    GProgressEndIndicator();
    if ( oldsel!=fv->selected )
	free(oldsel);
}

void PasteIntoMV(MetricsView *mv,SplineChar *sc, int doclear) {
    Undoes *cur=NULL, *bmp;
    BDFFont *bdf;
    int yestoall=0, first=true;
    extern int onlycopydisplayed;

    cur = &copybuffer;


    if ( copybuffer.undotype == ut_none ) {
	SCCheckXClipboard(mv->gw,sc,dm_fore,doclear);
return;
    }

    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    switch ( cur->undotype ) {
      case ut_noop:
      break;
      case ut_state: case ut_width: case ut_vwidth:
      case ut_lbearing: case ut_rbearing:
      case ut_statehint: case ut_statename:
	if ( !mv->fv->sf->hasvmetrics && cur->undotype==ut_vwidth) {
	    GWidgetErrorR(_STR_NoVerticalMetrics,_STR_FontNoVerticalMetrics);
return;
	}
	PasteToSC(sc,cur,mv->fv,doclear);
      break;
      case ut_bitmapsel: case ut_bitmap:
	if ( onlycopydisplayed && mv->bdf!=NULL )
	    _PasteToBC(BDFMakeChar(mv->bdf,sc->enc),mv->bdf->pixelsize,BDFDepth(mv->bdf),cur,doclear,mv->fv);
	else {
	    for ( bdf=mv->fv->sf->bitmaps; bdf!=NULL &&
		    (bdf->pixelsize!=cur->u.bmpstate.pixelsize || BDFDepth(bdf)!=cur->u.bmpstate.depth);
		    bdf=bdf->next );
	    if ( bdf==NULL ) {
		bdf = BitmapCreateCheck(mv->fv,&yestoall,first,cur->u.bmpstate.pixelsize,cur->u.bmpstate.depth);
		first = false;
	    }
	    if ( bdf!=NULL )
		_PasteToBC(BDFMakeChar(bdf,sc->enc),bdf->pixelsize,BDFDepth(bdf),cur,doclear,mv->fv);
	}
      break;
      case ut_composit:
	if ( cur->u.composit.state!=NULL )
	    PasteToSC(sc,cur->u.composit.state,mv->fv,doclear);
	for ( bmp=cur->u.composit.bitmaps; bmp!=NULL; bmp = bmp->next ) {
	    for ( bdf=mv->fv->sf->bitmaps; bdf!=NULL &&
		    (bdf->pixelsize!=bmp->u.bmpstate.pixelsize || BDFDepth(bdf)!=bmp->u.bmpstate.depth);
		    bdf=bdf->next );
	    if ( bdf==NULL )
		bdf = BitmapCreateCheck(mv->fv,&yestoall,first,bmp->u.bmpstate.pixelsize,bmp->u.bmpstate.depth);
	    if ( bdf!=NULL )
		_PasteToBC(BDFMakeChar(bdf,sc->enc),bdf->pixelsize,BDFDepth(bdf),bmp,doclear,mv->fv);
	}
	first = false;
      break;
    }
}

/* Look through the copy buffer. If it wasn't copied from the given font, then */
/*  we can stop. Otherwise: */
/*	If "from" is NULL, then remove all anchorpoints from the buffer */
/*	If "into" is NULL, then remove all anchorpoints with class "from" */
/*	Else replace the anchor class of all anchorpoints with class "from" with "info" */
static void _PasteAnchorClassManip(SplineFont *sf,AnchorClass *into,AnchorClass *from) {
    Undoes *cur = &copybuffer, *temp;

    if ( cur->undotype==ut_multiple )
	cur = cur->u.multiple.mult;
    while ( cur!=NULL ) {
	temp = cur;
	switch ( temp->undotype ) {
	  case ut_composit:
	    if ( temp->u.composit.state==NULL )
	break;
	    temp = temp->u.composit.state;
	    /* Fall through */;
	  case ut_state: case ut_statehint: case ut_statename:
	    if ( temp->u.state.copied_from!=sf )
return;
	    if ( from==NULL ) {
		AnchorPointsFree(temp->u.state.anchor);
		temp->u.state.anchor = NULL;
	    } else
		temp->u.state.anchor = APAnchorClassMerge(temp->u.state.anchor,into,from);
	  break;
	}
	cur=cur->next;
    }
}

void PasteRemoveSFAnchors(SplineFont *sf) {
    _PasteAnchorClassManip(sf,NULL,NULL);
}

void PasteAnchorClassMerge(SplineFont *sf,AnchorClass *into,AnchorClass *from) {
    _PasteAnchorClassManip(sf,into,from);
}

void PasteRemoveAnchorClass(SplineFont *sf,AnchorClass *dying) {
    _PasteAnchorClassManip(sf,NULL,dying);
}

void PosSubCopy(enum possub_type type, char **data) {

    CopyBufferFreeGrab();

    copybuffer.undotype = ut_possub;
    copybuffer.u.possub.pst = type;
    copybuffer.u.possub.data = data;
    XClipCheckEps();
}
