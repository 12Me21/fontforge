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
#include "ustring.h"
#include "utype.h"
#include "gfile.h"
#include "chardata.h"

static RefChar *RefCharsCopy(RefChar *ref) {
    RefChar *rhead=NULL, *last=NULL, *cur;

    while ( ref!=NULL ) {
	cur = chunkalloc(sizeof(RefChar));
	*cur = *ref;
	cur->splines = NULL;	/* Leave the old sc, we'll fix it later */
	cur->next = NULL;
	if ( rhead==NULL )
	    rhead = cur;
	else
	    last->next = cur;
	last = cur;
	ref = ref->next;
    }
return( rhead );
}


static int FixupSLI(int sli,SplineFont *from,SplineChar *sc) {
    SplineFont *to = sc->parent;
    int i,j,k;

    /* Find a script lang index in the new font which matches that of the old */
    /*  font. There are some cases when we don't know what the old font was. */
    /*  well, just make a reasonable guess */

    if ( from==NULL )
return( SFAddScriptLangIndex(to,SCScriptFromUnicode(sc),DEFAULT_LANG));
    if ( from->cidmaster ) from = from->cidmaster;
    if ( to->cidmaster ) to = to->cidmaster;
    if ( from==to )
return( sli );

    /* does the new font have exactly what we want? */
    i = 0;
    if ( to->script_lang!=NULL ) {
	for ( i=0; to->script_lang[i]!=NULL; ++i ) {
	    for ( j=0; to->script_lang[i][j].script!=0 &&
		    to->script_lang[i][j].script==from->script_lang[sli][j].script; ++j ) {
		for ( k=0; to->script_lang[i][j].langs[k]!=0 &&
			to->script_lang[i][j].langs[k]==from->script_lang[sli][j].langs[k]; ++k );
		if ( to->script_lang[i][j].langs[k]!=0 || from->script_lang[sli][j].langs[k]!=0 )
	    break;
	    }
	    if ( to->script_lang[i][j].script==0 && from->script_lang[sli][j].script==0 )
return( i );
	}
    }

    /* Add it */
    if ( to->script_lang==NULL )
	to->script_lang = galloc(2*sizeof(struct script_record *));
    else
	to->script_lang = grealloc(to->script_lang,(i+2)*sizeof(struct script_record *));
    to->script_lang[i+1] = NULL;
    for ( j=0; from->script_lang[sli][j].script!=0; ++j );
    to->script_lang[i] = galloc((j+1)*sizeof(struct script_record));
    for ( j=0; from->script_lang[sli][j].script!=0; ++j ) {
	to->script_lang[i][j].script = from->script_lang[sli][j].script;
	for ( k=0; from->script_lang[sli][j].langs[k]!=0; ++k );
	to->script_lang[i][j].langs = galloc((k+1)*sizeof(uint32));
	for ( k=0; from->script_lang[sli][j].langs[k]!=0; ++k )
	    to->script_lang[i][j].langs[k] = from->script_lang[sli][j].langs[k];
	to->script_lang[i][j].langs[k] = 0;
    }
    to->script_lang[i][j].script = 0;
return( i );
}

PST *PSTCopy(PST *base,SplineChar *sc,SplineFont *from) {
    PST *head=NULL, *last=NULL, *cur;

    for ( ; base!=NULL; base = base->next ) {
	cur = chunkalloc(sizeof(PST));
	*cur = *base;
	if ( from!=sc->parent )
	    cur->script_lang_index = FixupSLI(cur->script_lang_index,from,sc);
	if ( cur->type==pst_ligature ) {
	    cur->u.lig.components = copy(cur->u.lig.components);
	    cur->u.lig.lig = sc;
	} else if ( cur->type==pst_lcaret ) {
	    cur->u.lcaret.carets = galloc(cur->u.lcaret.cnt*sizeof(uint16));
	    memcpy(cur->u.lcaret.carets,base->u.lcaret.carets,cur->u.lcaret.cnt*sizeof(uint16));
	} else if ( cur->type==pst_substitution || cur->type==pst_multiple || cur->type==pst_alternate )
	    cur->u.subs.variant = copy(cur->u.subs.variant);
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
    }
return( head );
}

SplineChar *SplineCharCopy(SplineChar *sc,SplineFont *into) {
    SplineChar *nsc = SplineCharCreate();

    *nsc = *sc;
    nsc->parent = into;
    nsc->enc = -2;
    nsc->name = copy(sc->name);
    nsc->splines = SplinePointListCopy(nsc->splines);
    nsc->hstem = StemInfoCopy(nsc->hstem);
    nsc->vstem = StemInfoCopy(nsc->vstem);
    nsc->dstem = DStemInfoCopy(nsc->dstem);
    nsc->md = MinimumDistanceCopy(nsc->md);
    nsc->refs = RefCharsCopy(nsc->refs);
    nsc->views = NULL;
    nsc->changed = true;
    nsc->dependents = NULL;		/* Fix up later when we know more */
    nsc->backgroundsplines = NULL;
    nsc->backimages = NULL;
    nsc->undoes[0] = nsc->undoes[1] = nsc->redoes[0] = nsc->redoes[1] = NULL;
    nsc->kerns = NULL;
    nsc->possub = PSTCopy(nsc->possub,nsc,sc->parent);
    if ( sc->parent!=NULL && into->order2!=sc->parent->order2 )
	SCConvertOrder(nsc,into->order2);
return(nsc);
}

static KernPair *KernsCopy(KernPair *kp,int *mapping,SplineFont *into,SplineFont *from) {
    KernPair *head = NULL, *last=NULL, *new;
    int index;

    while ( kp!=NULL ) {
	if ( (index=mapping[kp->sc->enc])>=0 && index<into->charcnt &&
		into->chars[index]!=NULL ) {
	    new = chunkalloc(sizeof(KernPair));
	    new->off = kp->off;
	    new->sli = kp->sli;
	    new->sc = into->chars[index];
	    if ( head==NULL )
		head = new;
	    else
		last->next = new;
	    last = new;
	}
	kp = kp->next;
    }
return( head );
}

BDFChar *BDFCharCopy(BDFChar *bc) {
    BDFChar *nbc = galloc(sizeof( BDFChar ));

    *nbc = *bc;
    nbc->views = NULL;
    nbc->selection = NULL;
    nbc->undoes = NULL;
    nbc->redoes = NULL;
    nbc->bitmap = galloc((nbc->ymax-nbc->ymin+1)*nbc->bytes_per_line);
    memcpy(nbc->bitmap,bc->bitmap,(nbc->ymax-nbc->ymin+1)*nbc->bytes_per_line);
return( nbc );
}

void BitmapsCopy(SplineFont *to, SplineFont *from, int to_index, int from_index ) {
    BDFFont *f_bdf, *t_bdf;

    /* We assume that the bitmaps are ordered. They should be */
    for ( t_bdf=to->bitmaps, f_bdf=from->bitmaps; t_bdf!=NULL && f_bdf!=NULL; ) {
	if ( t_bdf->pixelsize == f_bdf->pixelsize ) {
	    if ( f_bdf->chars[from_index]!=NULL ) {
		BDFCharFree(t_bdf->chars[to_index]);
		t_bdf->chars[to_index] = BDFCharCopy(f_bdf->chars[from_index]);
		t_bdf->chars[to_index]->sc = to->chars[to_index];
		t_bdf->chars[to_index]->enc = to_index;
	    }
	    t_bdf = t_bdf->next;
	    f_bdf = f_bdf->next;
	} else if ( t_bdf->pixelsize < f_bdf->pixelsize )
	    t_bdf = t_bdf->next;
	
	    f_bdf = f_bdf->next;
    }
}

#define GN_HSIZE	257

struct glyphnamebucket {
    SplineChar *sc;
    struct glyphnamebucket *next;
};

struct glyphnamehash {
    struct glyphnamebucket *table[GN_HSIZE];
};

#ifndef __GNUC__
# define __inline__
#endif

static __inline__ int hashname(const char *pt) {
    int val = 0;

    while ( *pt )
	val += *pt++;
    val %= GN_HSIZE;
return( val );
}

static void _GlyphHashFree(SplineFont *sf) {
    struct glyphnamebucket *test, *next;
    int i;

    if ( sf->glyphnames==NULL )
return;
    for ( i=0; i<GN_HSIZE; ++i ) {
	for ( test = sf->glyphnames->table[i]; test!=NULL; test = next ) {
	    next = test->next;
	    chunkfree(test,sizeof(struct glyphnamebucket));
	}
    }
    free(sf->glyphnames);
    sf->glyphnames = NULL;
}

void GlyphHashFree(SplineFont *sf) {
    _GlyphHashFree(sf);
    if ( sf->cidmaster )
	_GlyphHashFree(sf->cidmaster);
}

static void GlyphHashCreate(SplineFont *sf) {
    int i, k, hash;
    SplineFont *_sf;
    struct glyphnamehash *gnh;
    struct glyphnamebucket *new;

    if ( sf->glyphnames!=NULL )
return;
    sf->glyphnames = gnh = gcalloc(1,sizeof(*gnh));
    k = 0;
    do {
	_sf = k<sf->subfontcnt ? sf->subfonts[k] : sf;
	for ( i=0; i<_sf->charcnt; ++i ) if ( _sf->chars[i]!=NULL ) {
	    new = chunkalloc(sizeof(struct glyphnamebucket));
	    new->sc = _sf->chars[i];
	    hash = hashname(new->sc->name);
	    new->next = gnh->table[hash];
	    gnh->table[hash] = new;
	}
	++k;
    } while ( k<sf->subfontcnt );
}

static SplineChar *SFHashName(SplineFont *sf,char *name) {
    struct glyphnamebucket *test;

    if ( sf->glyphnames==NULL )
	GlyphHashCreate(sf);

    for ( test=sf->glyphnames->table[hashname(name)]; test!=NULL; test = test->next )
	if ( strcmp(test->sc->name,name)==0 )
return( test->sc );

return( NULL );
}

static int _SFFindChar(SplineFont *sf, int unienc, char *name ) {
    int index= -1;

    if ( (sf->encoding_name==em_unicode || sf->encoding_name==em_unicode4) &&
	    unienc!=-1 ) {
	index = unienc;
	if ( index>=sf->charcnt || sf->chars[index]==NULL )
	    index = -1;
    } else if ( (sf->encoding_name>=em_unicodeplanes && sf->encoding_name<=em_unicodeplanesmax) &&
	    unienc!=-1 ) {
	index = unienc-((sf->encoding_name-em_unicodeplanes)<<16);
	if ( index<0 || index>=sf->charcnt || sf->chars[index]==NULL )
	    index = -1;
    } else if ( unienc!=-1 ) {
	if ( unienc<sf->charcnt && sf->chars[unienc]!=NULL &&
		sf->chars[unienc]->unicodeenc==unienc )
	    index = unienc;
	else for ( index = sf->charcnt-1; index>=0; --index ) if ( sf->chars[index]!=NULL ) {
	    if ( sf->chars[index]->unicodeenc==unienc )
	break;
	}
    } else {
	SplineChar *sc = SFHashName(sf,name);
	if ( sc!=NULL ) index = sc->enc;
    }
return( index );
}

int SFFindChar(SplineFont *sf, int unienc, char *name ) {
    int index=-1;
    char *end;

    if ( (sf->encoding_name==em_unicode || sf->encoding_name==em_unicode4) &&
	    unienc!=-1 ) {
	index = unienc;
	if ( index>=sf->charcnt )
	    index = -1;
    } else if ( (sf->encoding_name>=em_unicodeplanes && sf->encoding_name<=em_unicodeplanesmax) &&
	    unienc!=-1 ) {
	index = unienc-((sf->encoding_name-em_unicodeplanes)<<16);
	if ( index<0 || index>=sf->charcnt || sf->chars[index]==NULL )
	    index = -1;
    } else if ( unienc!=-1 ) {
	if ( unienc<sf->charcnt && sf->chars[unienc]!=NULL &&
		sf->chars[unienc]->unicodeenc==unienc )
	    index = unienc;
	else for ( index = sf->charcnt-1; index>=0; --index ) if ( sf->chars[index]!=NULL ) {
	    if ( sf->chars[index]->unicodeenc==unienc )
	break;
	}
    } else if ( name!=NULL ) {
	SplineChar *sc = SFHashName(sf,name);
	if ( sc!=NULL ) index = sc->enc;
	if ( index==-1 ) {
	    if ( name[0]=='u' && name[1]=='n' && name[2]=='i' && strlen(name)==7 ) {
		if ( unienc=strtol(name+3,&end,16), *end!='\0' )
		    unienc = -1;
	    } else if ( name[0]=='u' && strlen(name)<=7 ) {
		if ( unienc=strtol(name+1,&end,16), *end!='\0' )
		    unienc = -1;
	    }
	    if ( unienc!=-1 )
return( SFFindChar(sf,unienc,NULL));
	    for ( unienc=psunicodenames_cnt-1; unienc>=0; --unienc )
		if ( psunicodenames[unienc]!=NULL &&
			strcmp(psunicodenames[unienc],name)==0 )
return( SFFindChar(sf,unienc,NULL));
	}
    }

 /* Ok. The character is not in the font, but that might be because the font */
 /*  has a hole. check to see if it is in the encoding */
    if ( index==-1 && unienc>=0 && unienc<=65535 && sf->encoding_name!=em_none ) {
	if ( sf->encoding_name==em_none )
	    index = -1;
	else if ( sf->encoding_name>=em_base ) {
	    Encoding *item=NULL;
	    for ( item=enclist; item!=NULL && item->enc_num!=sf->encoding_name; item=item->next );
	    if ( item!=NULL ) {
		for ( index=item->char_cnt-1; index>=0; --index )
		    if ( item->unicode[index]==unienc )
		break;
	    }
	} else if ( sf->encoding_name==em_adobestandard ) {
	    for ( index=255; index>=0; --index )
		if ( unicode_from_adobestd[index]==unienc )
	    break;
	} else if ( sf->encoding_name<em_first2byte ) {
	    unichar_t * table = unicode_from_alphabets[sf->encoding_name+3];
	    for ( index=255; index>=0; --index )
		if ( table[index]==unienc )
	    break;
	} else {
	    struct charmap2 *table2 = NULL;
	    unsigned short *plane2;
	    int highch, temp;

	    if ( sf->encoding_name==em_jis208 || sf->encoding_name==em_jis212 ||
		    sf->encoding_name==em_sjis )
		table2 = &jis_from_unicode;
	    else if ( sf->encoding_name==em_gb2312 )
		table2 = &gb2312_from_unicode;
	    else if ( sf->encoding_name==em_ksc5601 || sf->encoding_name==em_wansung )
		table2 = &ksc5601_from_unicode;
	    else if ( sf->encoding_name==em_big5 )
		table2 = &big5_from_unicode;
	    else if ( sf->encoding_name==em_big5hkscs )
		table2 = &big5hkscs_from_unicode;
	    else if ( sf->encoding_name==em_johab )
		table2 = &johab_from_unicode;
	    if ( table2!=NULL ) {
		highch = unienc>>8;
		if ( highch>=table2->first && highch<=table2->last &&
			(plane2 = table2->table[highch-table2->first])!=NULL &&
			(temp=plane2[unienc&0xff])!=0 ) {
		    index = temp;
		    if ( sf->encoding_name==em_jis212 ) {
			if ( !(index&0x8000 ) )
			    index=-1;
			else
			    index &= 0x8000;
		    } else if ( (sf->encoding_name==em_jis208 || sf->encoding_name==em_sjis ) &&
			    (index&0x8000) )
			index = -1;
		} else if ( unienc<0x80 &&
			(sf->encoding_name==em_big5 || sf->encoding_name==em_big5hkscs ||
			 sf->encoding_name==em_johab ||
			 sf->encoding_name==em_sjis || sf->encoding_name==em_wansung ))
		    index = unienc;
		else if ( sf->encoding_name==em_sjis && unienc>=0xFF61 &&
			unienc<=0xFF9F )
		    index = unienc-0xFF61 + 0xa1;	/* katakana */
		if ( index>0x100 &&
			(sf->encoding_name==em_jis208 || sf->encoding_name==em_jis212 ||
			 sf->encoding_name==em_ksc5601 || sf->encoding_name==em_gb2312 )) {
		    index -= 0x2121;
		    index = (index>>8)*96 + (index&0xff)+1;
		} else if ( index>0x100 && sf->encoding_name==em_sjis ) {
		    int j1 = index>>8, j2 = index&0xff;
		    int ro = j1<95 ? 112 : 176;
		    int co = (j1&1) ? (j2>95?32:31) : 126;
		    index = ( (((j1+1)>>1) + ro )<<8 ) | (j2+co);
		}
	    }
	}
    }
return( index );
}

int SFCIDFindChar(SplineFont *sf, int unienc, char *name ) {
    int j,ret;

    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;

    if ( unienc==-1 && name!=NULL ) {
	SplineChar *sc = SFHashName(sf,name);
	if ( sc!=NULL )
return( sc->enc );
    }

    if ( sf->subfonts==NULL )
return( _SFFindChar(sf,unienc,name));
    for ( j=0; j<sf->subfontcnt; ++j )
	if (( ret = _SFFindChar(sf->subfonts[j],unienc,name))!=-1 )
return( ret );
return( -1 );
}

int SFHasCID(SplineFont *sf,int cid) {
    int i;
    /* What subfont (if any) contains this cid? */
    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;
    for ( i=0; i<sf->subfontcnt; ++i )
	if ( cid<sf->subfonts[i]->charcnt && sf->subfonts[i]->chars[cid]!=NULL )
return( i );

return( -1 );
}

SplineChar *SFGetChar(SplineFont *sf, int unienc, char *name ) {
    int ind;
    int j;

    if ( unienc==-1 && name!=NULL )
return( SFHashName(sf,name));

    ind = SFCIDFindChar(sf,unienc,name);
    if ( ind==-1 )
return( NULL );

    if ( sf->subfonts==NULL && sf->cidmaster==NULL )
return( sf->chars[ind]);

    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;
    for ( j=0; j<sf->subfontcnt; ++j )
	if ( ind<sf->subfonts[j]->charcnt && sf->subfonts[j]->chars[ind]!=NULL )
return( sf->subfonts[j]->chars[ind] );

return( NULL );
}

SplineChar *SFGetCharDup(SplineFont *sf, int unienc, char *name ) {
return( SCDuplicate(SFGetChar(sf,unienc,name)) );
}

SplineChar *SFGetOrMakeChar(SplineFont *sf, int unienc, char *name ) {
    int ind = SFFindChar(sf,unienc,name);

    if ( ind==-1 )
return( NULL );

return( SFMakeChar(sf,ind));
}

int SFFindExistingChar(SplineFont *sf, int unienc, char *name ) {
    int i = _SFFindChar(sf,unienc,name);

    if ( i==-1 || sf->chars[i]==NULL )
return( -1 );
    if ( sf->chars[i]->splines!=NULL || sf->chars[i]->refs!=NULL ||
	    sf->chars[i]->widthset )
return( i );

return( -1 );
}

int SFCIDFindExistingChar(SplineFont *sf, int unienc, char *name ) {
    int j,ret;

    if ( sf->subfonts==NULL && sf->cidmaster==NULL )
return( SFFindExistingChar(sf,unienc,name));
    if ( sf->cidmaster!=NULL )
	sf=sf->cidmaster;
    for ( j=0; j<sf->subfontcnt; ++j )
	if (( ret = _SFFindChar(sf->subfonts[j],unienc,name))!=-1 &&
		sf->subfonts[j]->chars[ret]!=NULL )
return( ret );
return( -1 );
}

static void MFixupSC(SplineFont *sf, SplineChar *sc,int i) {
    RefChar *ref, *prev;
    FontView *fvs;

    sc->enc = i;
    sc->parent = sf;
 retry:
    for ( ref=sc->refs; ref!=NULL; ref=ref->next ) {
	/* The sc in the ref is from the old font. It's got to be in the */
	/*  new font too (was either already there or just got copied) */
	ref->local_enc =  _SFFindChar(sf,ref->sc->unicodeenc,ref->sc->name);
	if ( ref->local_enc==-1 ) {
	    GDrawIError("Bad reference, can't fix it up");
	    if ( ref==sc->refs ) {
		sc->refs = ref->next;
 goto retry;
	    } else {
		for ( prev=sc->refs; prev->next!=ref; prev=prev->next );
		prev->next = ref->next;
		chunkfree(ref,sizeof(*ref));
		ref = prev;
	    }
	} else {
	    ref->sc = sf->chars[ref->local_enc];
	    if ( ref->sc->enc==-2 )
		MFixupSC(sf,ref->sc,ref->local_enc);
	    SCReinstanciateRefChar(sc,ref);
	    SCMakeDependent(sc,ref->sc);
	}
    }
    /* I shan't automagically generate bitmaps for any bdf fonts */
    /*  but I have copied over the ones which match */
    for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->nextsame ) if ( fvs->filled!=NULL )
	fvs->filled->chars[i] = SplineCharRasterize(sc,fvs->filled->pixelsize);
}

static void MergeFixupRefChars(SplineFont *sf) {
    int i;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && sf->chars[i]->enc==-2 ) {
	MFixupSC(sf,sf->chars[i], i);
    }
}

static int SFHasChar(SplineFont *sf, int unienc, char *name ) {
    int index;

    index = _SFFindChar(sf,unienc,name);
    if ( index>=0 && index<sf->charcnt && sf->chars[index]!=NULL ) {
	if ( sf->chars[index]->refs!=NULL ||
		sf->chars[index]->splines!=NULL ||
		sf->chars[index]->width!=sf->ascent+sf->descent ||
		sf->chars[index]->widthset ||
		sf->chars[index]->backimages != NULL ||
		sf->chars[index]->backgroundsplines != NULL )
return( true );
    }
return( false );
}

static int SFHasName(SplineFont *sf, char *name ) {
    int index;

    for ( index = sf->charcnt-1; index>=0; --index ) if ( sf->chars[index]!=NULL ) {
	if ( strcmp(sf->chars[index]->name,name)==0 )
    break;
    }
return( index );
}

static int SFEncodingCnt(SplineFont *sf) {
    int cnt = CountOfEncoding(sf->encoding_name);
    if ( sf->encoding_name == em_none )
return( sf->charcnt );
    if ( cnt==0 )
return( sf->charcnt );
return( cnt );
}

static void _MergeFont(SplineFont *into,SplineFont *other) {
    int i,cnt, doit, emptypos, index, enc_cnt, k;
    SplineFont *o_sf, *bitmap_into;
    BDFFont *bdf;
    FontView *fvs;
    int *mapping;

    enc_cnt = SFEncodingCnt(into);
    for ( i=into->charcnt-1; i>=enc_cnt && into->chars[i]==NULL; --i );
    emptypos = i+1;
    mapping = galloc(other->charcnt*sizeof(int));
    memset(mapping,-1,other->charcnt*sizeof(int));

    bitmap_into = into->cidmaster!=NULL? into->cidmaster : into;

    for ( doit=0; doit<2; ++doit ) {
	cnt = 0;
	k = 0;
	do {
	    o_sf = ( other->subfonts==NULL ) ? other : other->subfonts[k];
	    for ( i=0; i<o_sf->charcnt; ++i ) if ( o_sf->chars[i]!=NULL ) {
		if ( doit && (index = mapping[i])!=-1 ) {
		    /* Bug here. Suppose someone has a reference to our empty */
		    /*  char */
		    SplineCharFree(into->chars[index]);
		    into->chars[index] = SplineCharCopy(o_sf->chars[i],into);
		    if ( into->bitmaps!=NULL && other->bitmaps!=NULL )
			BitmapsCopy(bitmap_into,other,index,i);
		} else if ( !doit ) {
		    if ( o_sf->chars[i]->splines==NULL && o_sf->chars[i]->refs==NULL &&
			    !o_sf->chars[i]->widthset )
			/* Don't bother to copy it */;
		    else if ( SCDuplicate(o_sf->chars[i])!=o_sf->chars[i] )
			/* Don't bother to copy it */;
		    else if ( !SFHasChar(into,o_sf->chars[i]->unicodeenc,o_sf->chars[i]->name)) {
			if ( o_sf->chars[i]->unicodeenc==-1 ) {
			    if ( (index=SFHasName(into,o_sf->chars[i]->name))== -1 )
				index = emptypos+cnt++;
			} else if ( (index = SFFindChar(into,o_sf->chars[i]->unicodeenc,NULL))==-1 )
			    index = emptypos+cnt++;
			mapping[i] = index;
		    }
		}
	    }
	    if ( !doit ) {
		if ( emptypos+cnt >= into->charcnt ) {
		    into->chars = grealloc(into->chars,(emptypos+cnt)*sizeof(SplineChar *));
		    for ( fvs = into->fv; fvs!=NULL; fvs=fvs->nextsame )
			if ( fvs->sf == into )
			    fvs->selected = grealloc(fvs->selected,emptypos+cnt);
		    for ( bdf = bitmap_into->bitmaps; bdf!=NULL; bdf=bdf->next )
			if ( emptypos+cnt > bdf->charcnt )
			    bdf->chars = grealloc(bdf->chars,(emptypos+cnt)*sizeof(SplineChar *));
		    for ( fvs = into->fv; fvs!=NULL; fvs=fvs->nextsame )
			if ( fvs->filled!=NULL )
			    fvs->filled->chars = grealloc(fvs->filled->chars,(emptypos+cnt)*sizeof(SplineChar *));
		    for ( i=into->charcnt; i<emptypos+cnt; ++i ) {
			into->chars[i] = NULL;
			for ( fvs = into->fv; fvs!=NULL; fvs=fvs->nextsame )
			    if ( fvs->filled!=NULL )
				fvs->selected[i] = false;
			for ( bdf = bitmap_into->bitmaps; bdf!=NULL; bdf=bdf->next )
			    if ( emptypos+cnt > bdf->charcnt )
				bdf->chars[i] = NULL;
			for ( fvs = into->fv; fvs!=NULL; fvs=fvs->nextsame )
			    if ( fvs->filled!=NULL )
				fvs->filled->chars[i] = NULL;
		    }
		    into->charcnt = emptypos+cnt;
		    for ( fvs = into->fv; fvs!=NULL; fvs=fvs->nextsame )
			if ( fvs->filled!=NULL )
			    fvs->filled->charcnt = emptypos+cnt;
		    for ( bdf = bitmap_into->bitmaps; bdf!=NULL; bdf=bdf->next )
			if ( emptypos+cnt > bdf->charcnt )
			    bdf->charcnt = emptypos+cnt;
		}
	    }
	    ++k;
	} while ( k<other->subfontcnt );
    }
    for ( i=0; i<other->charcnt; ++i ) if ( (index=mapping[i])!=-1 )
	into->chars[index]->kerns = KernsCopy(other->chars[i]->kerns,mapping,into,other);
    free(mapping);
    GlyphHashFree(into);
    MergeFixupRefChars(into);
    if ( other->fv==NULL )
	SplineFontFree(other);
    into->changed = true;
    FontViewReformatAll(into);
    GlyphHashFree(into);
}

static void CIDMergeFont(SplineFont *into,SplineFont *other) {
    int i,j,k;
    SplineFont *i_sf, *o_sf;
    FontView *fvs;

    k = 0;
    do {
	o_sf = other->subfonts[k];
	i_sf = into->subfonts[k];
	for ( i=o_sf->charcnt-1; i>=0 && o_sf->chars[i]==NULL; --i );
	if ( i>=i_sf->charcnt ) {
	    i_sf->chars = grealloc(i_sf->chars,(i+1)*sizeof(SplineChar *));
	    for ( j=i_sf->charcnt; j<=i; ++j )
		i_sf->chars[j] = NULL;
	    for ( fvs = i_sf->fv; fvs!=NULL; fvs=fvs->nextsame )
		if ( fvs->sf==i_sf ) {
		    fvs->selected = grealloc(fvs->selected,i+1);
		    for ( j=i_sf->charcnt; j<=i; ++j )
			fvs->selected[j] = false;
		}
	    i_sf->charcnt = i+1;
	}
	for ( i=0; i<o_sf->charcnt; ++i ) if ( o_sf->chars[i]!=NULL ) {
	    if ( o_sf->chars[i]->splines==NULL && o_sf->chars[i]->refs==NULL &&
		    !o_sf->chars[i]->widthset )
		/* Don't bother to copy it */;
	    else if ( SFHasCID(into,i)==-1 ) {
		SplineCharFree(i_sf->chars[i]);
		i_sf->chars[i] = SplineCharCopy(o_sf->chars[i],i_sf);
		if ( into->bitmaps!=NULL && other->bitmaps!=NULL )
		    BitmapsCopy(into,other,i,i);
	    }
	}
	MergeFixupRefChars(i_sf);
	++k;
    } while ( k<other->subfontcnt );
    FontViewReformatAll(into);
    into->changed = true;
    GlyphHashFree(into);
}

void MergeFont(FontView *fv,SplineFont *other) {

    if ( fv->sf==other ) {
	GWidgetErrorR(_STR_MergingProb,_STR_MergingFontSelf);
return;
    }
    if ( fv->sf->cidmaster!=NULL && other->subfonts!=NULL &&
	    (strcmp(fv->sf->cidmaster->cidregistry,other->cidregistry)!=0 ||
	     strcmp(fv->sf->cidmaster->ordering,other->ordering)!=0 ||
	     fv->sf->cidmaster->supplement<other->supplement ||
	     fv->sf->cidmaster->subfontcnt<other->subfontcnt )) {
	GWidgetErrorR(_STR_MergingProb,_STR_MergingCIDMismatch);
return;
    }
    /* Ok. when merging CID fonts... */
    /*  If fv is normal and other is CID then just flatten other and merge everything into fv */
    /*  If fv is CID and other is normal then merge other into the currently active font */
    /*  If both are CID then merge each subfont seperately */
    if ( fv->sf->cidmaster!=NULL && other->subfonts!=NULL )
	CIDMergeFont(fv->sf->cidmaster,other);
    else
	_MergeFont(fv->sf,other);
}

static void MergeAskFilename(FontView *fv) {
    char *filename = GetPostscriptFontName(NULL,true);
    SplineFont *sf;
    char *eod, *fpt, *file, *full;

    if ( filename==NULL )
return;
    eod = strrchr(filename,'/');
    *eod = '\0';
    file = eod+1;
    do {
	fpt = strstr(file,"; ");
	if ( fpt!=NULL ) *fpt = '\0';
	full = galloc(strlen(filename)+1+strlen(file)+1);
	strcpy(full,filename); strcat(full,"/"); strcat(full,file);
	sf = LoadSplineFont(full,0);
	free(full);
	if ( sf==NULL )
	    /* Do Nothing */;
	else if ( sf->fv==fv )
	    GWidgetErrorR(_STR_MergingProb,_STR_MergingFontSelf);
	else
	    MergeFont(fv,sf);
	file = fpt+2;
    } while ( fpt!=NULL );
    free(filename);
}

static GTextInfo *BuildFontList(FontView *except) {
    FontView *fv;
    int cnt=0;
    GTextInfo *tf;

    for ( fv=fv_list; fv!=NULL; fv = fv->next )
	++cnt;
    tf = gcalloc(cnt+3,sizeof(GTextInfo));
    for ( fv=fv_list, cnt=0; fv!=NULL; fv = fv->next ) if ( fv!=except ) {
	tf[cnt].fg = tf[cnt].bg = COLOR_DEFAULT;
	tf[cnt].text = uc_copy(fv->sf->fontname);
	++cnt;
    }
    tf[cnt++].line = true;
    tf[cnt].fg = tf[cnt].bg = COLOR_DEFAULT;
    tf[cnt].text_in_resource = true;
    tf[cnt++].text = (unichar_t *) _STR_Other;
return( tf );
}

static void TFFree(GTextInfo *tf) {
    int i;

    for ( i=0; tf[i].text!=NULL || tf[i].line ; ++i )
	if ( !tf[i].text_in_resource )
	    free( tf[i].text );
    free(tf);
}

struct mf_data {
    int done;
    FontView *fv;
    GGadget *other;
    GGadget *amount;
};

static int MF_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	GWindow gw = GGadgetGetWindow(g);
	struct mf_data *d = GDrawGetUserData(gw);
	int i, index = GGadgetGetFirstListSelectedItem(d->other);
	FontView *fv;
	for ( i=0, fv=fv_list; fv!=NULL; fv=fv->next ) {
	    if ( fv==d->fv )
	continue;
	    if ( i==index )
	break;
	    ++i;
	}
	if ( fv==NULL )
	    MergeAskFilename(d->fv);
	else
	    MergeFont(d->fv,fv->sf);
	d->done = true;
    }
return( true );
}

static int MF_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	GWindow gw = GGadgetGetWindow(g);
	struct mf_data *d = GDrawGetUserData(gw);
	d->done = true;
    }
return( true );
}

static int mv_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	struct mf_data *d = GDrawGetUserData(gw);
	d->done = true;
    } else if ( event->type == et_char ) {
return( false );
    }
return( true );
}

void FVMergeFonts(FontView *fv) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[6];
    GTextInfo label[6];
    struct mf_data d;
    unichar_t buffer[80];

    /* If there's only one font loaded, then it's the current one, and there's*/
    /*  no point asking the user if s/he wants to merge any of the loaded */
    /*  fonts, go directly to searching the disk */
    if ( fv_list==fv && fv_list->next==NULL )
	MergeAskFilename(fv);
    else {
	memset(&wattrs,0,sizeof(wattrs));
	wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
	wattrs.event_masks = ~(1<<et_charup);
	wattrs.restrict_input_to_me = 1;
	wattrs.undercursor = 1;
	wattrs.cursor = ct_pointer;
	wattrs.window_title = GStringGetResource(_STR_Mergefonts,NULL);
	pos.x = pos.y = 0;
	pos.width = GGadgetScale(GDrawPointsToPixels(NULL,150));
	pos.height = GDrawPointsToPixels(NULL,88);
	gw = GDrawCreateTopWindow(NULL,&pos,mv_e_h,&d,&wattrs);

	memset(&label,0,sizeof(label));
	memset(&gcd,0,sizeof(gcd));

	u_sprintf( buffer, GStringGetResource(_STR_FontToMergeInto,NULL), fv->sf->fontname );
	label[0].text = buffer;
	gcd[0].gd.label = &label[0];
	gcd[0].gd.pos.x = 12; gcd[0].gd.pos.y = 6; 
	gcd[0].gd.flags = gg_visible | gg_enabled;
	gcd[0].creator = GLabelCreate;

	gcd[1].gd.pos.x = 20; gcd[1].gd.pos.y = 21;
	gcd[1].gd.pos.width = 110;
	gcd[1].gd.flags = gg_visible | gg_enabled;
	gcd[1].gd.u.list = BuildFontList(fv);
	gcd[1].gd.label = &gcd[1].gd.u.list[0];
	gcd[1].gd.u.list[0].selected = true;
	gcd[1].creator = GListButtonCreate;

	gcd[2].gd.pos.x = 15-3; gcd[2].gd.pos.y = 55-3;
	gcd[2].gd.pos.width = -1; gcd[2].gd.pos.height = 0;
	gcd[2].gd.flags = gg_visible | gg_enabled | gg_but_default;
	label[2].text = (unichar_t *) _STR_OK;
	label[2].text_in_resource = true;
	gcd[2].gd.mnemonic = 'O';
	gcd[2].gd.label = &label[2];
	gcd[2].gd.handle_controlevent = MF_OK;
	gcd[2].creator = GButtonCreate;

	gcd[3].gd.pos.x = -15; gcd[3].gd.pos.y = 55;
	gcd[3].gd.pos.width = -1; gcd[3].gd.pos.height = 0;
	gcd[3].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
	label[3].text = (unichar_t *) _STR_Cancel;
	label[3].text_in_resource = true;
	gcd[3].gd.label = &label[3];
	gcd[3].gd.mnemonic = 'C';
	gcd[3].gd.handle_controlevent = MF_Cancel;
	gcd[3].creator = GButtonCreate;

	GGadgetsCreate(gw,gcd);

	memset(&d,'\0',sizeof(d));
	d.other = gcd[1].ret;
	d.fv = fv;

	GWidgetHidePalettes();
	GDrawSetVisible(gw,true);
	while ( !d.done )
	    GDrawProcessOneEvent(NULL);
	GDrawDestroyWindow(gw);
	TFFree(gcd[1].gd.u.list);
    }
}

static RefChar *InterpRefs(RefChar *base, RefChar *other, real amount, SplineChar *sc) {
    RefChar *head=NULL, *last=NULL, *cur;
    RefChar *test;
    int i;

    for ( test = other; test!=NULL; test=test->next )
	test->checked = false;

    while ( base!=NULL ) {
	for ( test = other; test!=NULL; test=test->next ) {
	    if ( test->checked )
		/* Do nothing */;
	    else if ( test->unicode_enc==base->unicode_enc &&
		    (test->unicode_enc!=-1 || strcmp(test->sc->name,base->sc->name)==0 ) )
	break;
	}
	if ( test!=NULL ) {
	    test->checked = true;
	    cur = chunkalloc(sizeof(RefChar));
	    *cur = *base;
	    cur->local_enc = cur->sc->enc;
	    for ( i=0; i<6; ++i )
		cur->transform[i] = base->transform[i] + amount*(other->transform[i]-base->transform[i]);
	    cur->splines = NULL;
	    cur->checked = false;
	    if ( head==NULL )
		head = cur;
	    else
		last->next = cur;
	    last = cur;
	} else
	    fprintf( stderr, "In character %s, could not find reference to %s\n",
		    sc->name, base->sc->name );
	base = base->next;
	if ( test==other && other!=NULL )
	    other = other->next;
    }
return( head );
}

static void InterpPoint(SplineSet *cur, SplinePoint *base, SplinePoint *other, real amount ) {
    SplinePoint *p = chunkalloc(sizeof(SplinePoint));

    p->me.x = base->me.x + amount*(other->me.x-base->me.x);
    p->me.y = base->me.y + amount*(other->me.y-base->me.y);
    p->prevcp.x = base->prevcp.x + amount*(other->prevcp.x-base->prevcp.x);
    p->prevcp.y = base->prevcp.y + amount*(other->prevcp.y-base->prevcp.y);
    p->nextcp.x = base->nextcp.x + amount*(other->nextcp.x-base->nextcp.x);
    p->nextcp.y = base->nextcp.y + amount*(other->nextcp.y-base->nextcp.y);
    p->nonextcp = ( p->nextcp.x==p->me.x && p->nextcp.y==p->me.y );
    p->noprevcp = ( p->prevcp.x==p->me.x && p->prevcp.y==p->me.y );
    p->prevcpdef = base->prevcpdef && other->prevcpdef;
    p->nextcpdef = base->nextcpdef && other->nextcpdef;
    p->selected = false;
    p->pointtype = (base->pointtype==other->pointtype)?base->pointtype:pt_corner;
    /*p->flex = 0;*/
    if ( cur->first==NULL )
	cur->first = p;
    else
	SplineMake(cur->last,p,base->prev->order2);
    cur->last = p;
}
    
static SplineSet *InterpSplineSet(SplineSet *base, SplineSet *other, real amount, SplineChar *sc) {
    SplineSet *cur = chunkalloc(sizeof(SplineSet));
    SplinePoint *bp, *op;

    for ( bp=base->first, op = other->first; ; ) {
	InterpPoint(cur,bp,op,amount);
	if ( bp->next == NULL && op->next == NULL )
return( cur );
	if ( bp->next!=NULL && op->next!=NULL &&
		bp->next->to==base->first && op->next->to==other->first ) {
	    SplineMake(cur->last,cur->first,bp->next->order2);
	    cur->last = cur->first;
return( cur );
	}
	if ( bp->next == NULL || bp->next->to==base->first ) {
	    fprintf( stderr, "In character %s, there are too few points on a path in the base\n", sc->name);
	    if ( bp->next!=NULL ) {
		SplineMake(cur->last,cur->first,bp->next->order2);
		cur->last = cur->first;
	    }
return( cur );
	} else if ( op->next==NULL || op->next->to==other->first ) {
	    fprintf( stderr, "In character %s, there are too many points on a path in the base\n", sc->name);
	    while ( bp->next!=NULL && bp->next->to!=base->first ) {
		bp = bp->next->to;
		InterpPoint(cur,bp,op,amount);
	    }
	    if ( bp->next!=NULL ) {
		SplineMake(cur->last,cur->first,bp->next->order2);
		cur->last = cur->first;
	    }
return( cur );
	}
	bp = bp->next->to;
	op = op->next->to;
    }
}

static SplineSet *InterpSplineSets(SplineSet *base, SplineSet *other, real amount, SplineChar *sc) {
    SplineSet *head=NULL, *last=NULL, *cur;

    /* we could do something really complex to try and figure out which spline*/
    /*  set goes with which, but I'm not sure that it would really accomplish */
    /*  much, if things are off the computer probably won't figure it out */
    /* Could use path open/closed, direction, point count */
    while ( base!=NULL && other!=NULL ) {
	cur = InterpSplineSet(base,other,amount,sc);
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
	base = base->next;
	other = other->next;
    }
return( head );
}

static KernPair *InterpKerns(KernPair *kp1, KernPair *kp2, real amount, SplineFont *new) {
    KernPair *head=NULL, *last, *nkp, *k;

    if ( kp1==NULL || kp2==NULL )
return( NULL );
    while ( kp1!=NULL ) {
	for ( k=kp2; k!=NULL && k->sc->enc!=kp1->sc->enc; k=k->next );
	if ( k!=NULL ) {
	    if ( k==kp2 ) kp2 = kp2->next;
	    nkp = chunkalloc(sizeof(KernPair));
	    nkp->sc = new->chars[k->sc->enc];
	    nkp->off = kp1->off + amount*(k->off-kp1->off);
	    nkp->sli = kp1->sli;
	    if ( head==NULL )
		head = nkp;
	    else
		last->next = nkp;
	    last = nkp;
	}
	kp1 = kp1->next;
    }
return( head );
}

static void InterpolateChar(SplineFont *new, int enc, SplineChar *base, SplineChar *other, real amount) {
    SplineChar *sc;

    if ( base==NULL || other==NULL )
return;
    sc = SplineCharCreate();
    *sc = *base;
    sc->enc = enc;
    new->chars[enc] = sc;
    sc->views = NULL;
    sc->parent = new;
    sc->changed = true;
    sc->dependents = NULL;
    sc->backgroundsplines = NULL;
    sc->backimages = NULL;
    sc->undoes[0] = sc->undoes[1] = sc->redoes[0] = sc->redoes[1] = NULL;
    sc->kerns = NULL;
    sc->name = copy(sc->name);
    sc->width = base->width + amount*(other->width-base->width);
    sc->lsidebearing = base->lsidebearing + amount*(other->lsidebearing-base->lsidebearing);
    sc->splines = InterpSplineSets(base->splines,other->splines,amount,sc);
    sc->refs = InterpRefs(base->refs,other->refs,amount,sc);
    sc->changedsincelasthinted = true;
}

static void IFixupSC(SplineFont *sf, SplineChar *sc,int i) {
    RefChar *ref;

    for ( ref=sc->refs; ref!=NULL; ref=ref->next ) if ( !ref->checked ) {
	/* The sc in the ref is from the base font. It's got to be in the */
	/*  new font too (the ref only gets created if it's present in both fonts)*/
	ref->checked = true;
	ref->local_enc = _SFFindChar(sf,ref->sc->unicodeenc,ref->sc->name);
	ref->sc = sf->chars[ref->local_enc];
	IFixupSC(sf,ref->sc,ref->local_enc);
	SCReinstanciateRefChar(sc,ref);
	SCMakeDependent(sc,ref->sc);
    }
}

static void InterpFixupRefChars(SplineFont *sf) {
    int i;

    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	IFixupSC(sf,sf->chars[i], i);
    }
}

static void InterpolateFont(SplineFont *base, SplineFont *other, real amount) {
    SplineFont *new;
    int i, index;

    if ( base==other ) {
	GWidgetErrorR(_STR_InterpolatingProb,_STR_InterpolatingFontSelf);
return;
    } else if ( base->order2!=other->order2 ) {
	GWidgetErrorR(_STR_InterpolatingProb,_STR_InterpolatingFontsDiffOrder);
return;
    }
    new = SplineFontBlank(base->encoding_name,base->charcnt);
    new->ascent = base->ascent + amount*(other->ascent-base->ascent);
    new->descent = base->descent + amount*(other->descent-base->descent);
    if ( base->encoding_name==other->encoding_name ) {
	for ( i=0; i<base->charcnt && i<other->charcnt; ++i )
	    InterpolateChar(new,i,base->chars[i],other->chars[i],amount);
    } else {
	for ( i=0; i<base->charcnt; ++i ) if ( base->chars[i]!=NULL ) {
	    index = _SFFindChar(other,base->chars[i]->unicodeenc,base->chars[i]->name);
	    if ( other->chars[index]!=NULL )
		InterpolateChar(new,i,base->chars[i],other->chars[index],amount);
	}
    }
    /* Only do kerns if the encodings match. Too hard otherwise */
    if ( base->encoding_name==other->encoding_name ) {
	for ( i=0; i<base->charcnt && i<other->charcnt; ++i )
	    if ( new->chars[i]!=NULL )
		new->chars[i]->kerns = InterpKerns(base->chars[i]->kerns,other->chars[i]->kerns,amount,new);
    }
    InterpFixupRefChars(new);
    new->changed = true;
    FontViewCreate(new);
}

static void InterAskFilename(FontView *fv, real amount) {
    char *filename = GetPostscriptFontName(NULL,false);
    SplineFont *sf;

    if ( filename==NULL )
return;
    sf = LoadSplineFont(filename,0);
    free(filename);
    if ( sf==NULL )
return;
    InterpolateFont(fv->sf,sf,amount);
}

#define CID_Amount	1000
static real last_amount=50;

static int IF_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	GWindow gw = GGadgetGetWindow(g);
	struct mf_data *d = GDrawGetUserData(gw);
	int i, index = GGadgetGetFirstListSelectedItem(d->other);
	FontView *fv;
	int err=false;
	real amount;
	
	amount = GetRealR(gw,CID_Amount, _STR_Amount,&err);
	if ( err )
return( true );
	last_amount = amount;
	for ( i=0, fv=fv_list; fv!=NULL; fv=fv->next ) {
	    if ( fv==d->fv )
	continue;
	    if ( i==index )
	break;
	    ++i;
	}
	if ( fv==NULL )
	    InterAskFilename(d->fv,last_amount/100.0);
	else
	    InterpolateFont(d->fv->sf,fv->sf,last_amount/100.0);
	d->done = true;
    }
return( true );
}

void FVInterpolateFonts(FontView *fv) {
    GRect pos;
    GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[8];
    GTextInfo label[8];
    struct mf_data d;
    unichar_t buffer[80]; char buf2[30];

    memset(&wattrs,0,sizeof(wattrs));
    wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict;
    wattrs.event_masks = ~(1<<et_charup);
    wattrs.restrict_input_to_me = 1;
    wattrs.undercursor = 1;
    wattrs.cursor = ct_pointer;
    wattrs.window_title = GStringGetResource(_STR_Interp,NULL);
    pos.x = pos.y = 0;
    pos.width = GGadgetScale(GDrawPointsToPixels(NULL,200));
    pos.height = GDrawPointsToPixels(NULL,118);
    gw = GDrawCreateTopWindow(NULL,&pos,mv_e_h,&d,&wattrs);

    memset(&label,0,sizeof(label));
    memset(&gcd,0,sizeof(gcd));

    u_sprintf( buffer, GStringGetResource(_STR_InterpBetween,NULL), fv->sf->fontname );
    label[0].text = buffer;
    gcd[0].gd.label = &label[0];
    gcd[0].gd.pos.x = 12; gcd[0].gd.pos.y = 6; 
    gcd[0].gd.flags = gg_visible | gg_enabled;
    gcd[0].creator = GLabelCreate;

    gcd[1].gd.pos.x = 20; gcd[1].gd.pos.y = 21;
    gcd[1].gd.pos.width = 110;
    gcd[1].gd.flags = gg_visible | gg_enabled;
    gcd[1].gd.u.list = BuildFontList(fv);
    if ( gcd[1].gd.u.list[0].text!=NULL ) {
	gcd[1].gd.label = &gcd[1].gd.u.list[0];
	gcd[1].gd.u.list[0].selected = true;
    } else {
	gcd[1].gd.label = &gcd[1].gd.u.list[1];
	gcd[1].gd.u.list[1].selected = true;
	gcd[1].gd.flags = gg_visible;
    }
    gcd[1].creator = GListButtonCreate;

    sprintf( buf2, "%g", last_amount );
    label[2].text = (unichar_t *) buf2;
    label[2].text_is_1byte = true;
    gcd[2].gd.pos.x = 20; gcd[2].gd.pos.y = 51;
    gcd[2].gd.pos.width = 40;
    gcd[2].gd.flags = gg_visible | gg_enabled;
    gcd[2].gd.label = &label[2];
    gcd[2].gd.cid = CID_Amount;
    gcd[2].creator = GTextFieldCreate;

    gcd[3].gd.pos.x = 5; gcd[3].gd.pos.y = 51+6;
    gcd[3].gd.flags = gg_visible | gg_enabled;
    label[3].text = (unichar_t *) _STR_By;
    label[3].text_in_resource = true;
    gcd[3].gd.label = &label[3];
    gcd[3].creator = GLabelCreate;

    gcd[4].gd.pos.x = 20+40+3; gcd[4].gd.pos.y = 51+6;
    gcd[4].gd.flags = gg_visible | gg_enabled;
    label[4].text = (unichar_t *) "%";
    label[4].text_is_1byte = true;
    gcd[4].gd.label = &label[4];
    gcd[4].creator = GLabelCreate;

    gcd[5].gd.pos.x = 15-3; gcd[5].gd.pos.y = 85-3;
    gcd[5].gd.pos.width = -1; gcd[5].gd.pos.height = 0;
    gcd[5].gd.flags = gg_visible | gg_enabled | gg_but_default;
    label[5].text = (unichar_t *) _STR_OK;
    label[5].text_in_resource = true;
    gcd[5].gd.mnemonic = 'O';
    gcd[5].gd.label = &label[5];
    gcd[5].gd.handle_controlevent = IF_OK;
    gcd[5].creator = GButtonCreate;

    gcd[6].gd.pos.x = -15; gcd[6].gd.pos.y = 85;
    gcd[6].gd.pos.width = -1; gcd[6].gd.pos.height = 0;
    gcd[6].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
    label[6].text = (unichar_t *) _STR_Cancel;
    label[6].text_in_resource = true;
    gcd[6].gd.label = &label[6];
    gcd[6].gd.mnemonic = 'C';
    gcd[6].gd.handle_controlevent = MF_Cancel;
    gcd[6].creator = GButtonCreate;

    GGadgetsCreate(gw,gcd);

    memset(&d,'\0',sizeof(d));
    d.other = gcd[1].ret;
    d.fv = fv;

    GWidgetHidePalettes();
    GDrawSetVisible(gw,true);
    while ( !d.done )
	GDrawProcessOneEvent(NULL);
    GDrawDestroyWindow(gw);
    TFFree(gcd[1].gd.u.list);
}
