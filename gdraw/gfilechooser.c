/* Copyright (C) 2000-2002 by George Williams */
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
#include "ggadgetP.h"
#include "gio.h"
#include "gfile.h"
#include "ustring.h"
#include "utype.h"
#include "gicons.h"

/* This isn't really a gadget, it's just a collection of gadgets with some glue*/
/*  to make them work together. Therefore there are no expose routines here, */
/*  nor mouse, nor key. That's all handled by the individual gadgets themselves*/
/* Instead we've got a bunch of routines that make them work as a whole */
static GBox gfilechooser_box = { 0 };	/* no box */
static unichar_t *lastdir;

/* Handles *?{}[] wildcards */
int GGadgetWildMatch(unichar_t *pattern, unichar_t *name,int ignorecase) {
    unichar_t ch, *ppt, *npt, *ept;

    if ( pattern==NULL )
return( true );

    while (( ch = *pattern)!='\0' ) {
	if ( ch=='*' ) {
	    for ( npt=name; ; ++npt ) {
		if ( GGadgetWildMatch(pattern+1,npt,ignorecase))
return( true );
		if ( *npt=='\0' )
return( false );
	    }
	} else if ( ch=='?' ) {
	    if ( *name=='\0' )
return( false );
	    ++name;
	} else if ( ch=='[' ) {
	    /* [<char>...] matches the chars
	    /* [<char>-<char>...] matches any char within the range (inclusive)
	    /* the above may be concattenated and the resultant pattern matches
	    /*		anything thing which matches any of them.
	    /* [^<char>...] matches any char which does not match the rest of
	    /*		the pattern
	    /* []...] as a special case a ']' immediately after the '[' matches
	    /*		itself and does not end the pattern */
	    int found = 0, not=0;
	    ++pattern;
	    if ( pattern[0]=='^' ) { not = 1; ++pattern; }
	    for ( ppt = pattern; (ppt!=pattern || *ppt!=']') && *ppt!='\0' ; ++ppt ) {
		ch = *ppt;
		if ( ppt[1]=='-' && ppt[2]!=']' && ppt[2]!='\0' ) {
		    unichar_t ch2 = ppt[2];
		    if ( (*name>=ch && *name<=ch2) ||
			    (ignorecase && islower(ch) && islower(ch2) &&
				    *name>=toupper(ch) && *name<=toupper(ch2)) ||
			    (ignorecase && isupper(ch) && isupper(ch2) &&
				    *name>=tolower(ch) && *name<=tolower(ch2))) {
			if ( !not ) {
			    found = 1;
	    break;
			}
		    } else {
			if ( not ) {
			    found = 1;
	    break;
			}
		    }
		    ppt += 2;
		} else if ( ch==*name || (ignorecase && tolower(ch)==tolower(*name)) ) {
		    if ( !not ) {
			found = 1;
	    break;
		    }
		} else {
		    if ( not ) {
			found = 1;
	    break;
		    }
		}
	    }
	    if ( !found )
return( false );
	    while ( *ppt!=']' && *ppt!='\0' ) ++ppt;
	    pattern = ppt;
	    ++name;
	} else if ( ch=='{' ) {
	    /* matches any of a comma seperated list of substrings */
	    for ( ppt = pattern+1; *ppt!='\0' ; ppt = ept ) {
		for ( ept=ppt; *ept!='}' && *ept!=',' && *ept!='\0'; ++ept );
		for ( npt = name; ppt<ept; ++npt, ++ppt ) {
		    if ( *ppt != *npt && (!ignorecase || tolower(*ppt)!=tolower(*npt)) )
		break;
		}
		if ( ppt==ept ) {
		    unichar_t *ecurly = ept;
		    while ( *ecurly!='}' && *ecurly!='\0' ) ++ecurly;
		    if ( GGadgetWildMatch(ecurly+1,npt,ignorecase))
return( true );
		}
		if ( *ept=='}' )
return( false );
		if ( *ept==',' ) ++ept;
	    }
	} else if ( ch==*name ) {
	    ++name;
	} else if ( ignorecase && tolower(ch)==tolower(*name)) {
	    ++name;
	} else
return( false );
	++pattern;
    }
    if ( *name=='\0' )
return( true );

return( false );
}

enum fchooserret GFileChooserDefFilter(GGadget *g,GDirEntry *ent) {
    GFileChooser *gfc = (GFileChooser *) g;
    int i;
    unichar_t *mime;

    if ( uc_strcmp(ent->name,".")==0 )	/* Don't show the current directory entry */
return( fc_hide );
    if ( ent->isdir )			/* Show all other directories */
return( fc_show );
					/* If we've got a wildcard, and it matches, then show */
    if ( gfc->wildcard!=NULL && GGadgetWildMatch(gfc->wildcard,ent->name,true))
return( fc_show );
					/* If no mimetypes then we're done */
    if ( gfc->mimetypes==NULL )
return( fc_hide );
					/* match the mimetypes */
    mime = ent->mimetype==NULL?GIOguessMimeType(ent->name,ent->isdir):ent->mimetype;
    for ( i=0; gfc->mimetypes[i]!=NULL; ++i )
	if ( u_strstartmatch(gfc->mimetypes[i],mime))
return( fc_show );

return( fc_hide );
}

static GImage *GFileChooserPickIcon(GDirEntry *e) {
    unichar_t *m = e->mimetype;

    if ( e->isdir ) {
	if ( uc_strcmp(e->name,"..")==0 )
return( &_GIcon_updir );
return( &_GIcon_dir );
    }
    if ( m==NULL ) m = GIOguessMimeType(e->name,e->isdir);
    if ( cu_strstartmatch("text/",m)) {
	if ( cu_strstartmatch("text/html",m))
return( &_GIcon_texthtml );
	if ( cu_strstartmatch("text/xml",m))
return( &_GIcon_textxml );
	if ( cu_strstartmatch("text/css",m))
return( &_GIcon_textcss );
	if ( cu_strstartmatch("text/c",m))
return( &_GIcon_textc );
	if ( cu_strstartmatch("text/java",m))
return( &_GIcon_textjava );
	if ( cu_strstartmatch("text/make",m))
return( &_GIcon_textmake );
	if ( cu_strstartmatch("text/fontps",m))
return( &_GIcon_textfontps );
	if ( cu_strstartmatch("text/fontsfd",m))
return( &_GIcon_textfontsfd );
	if ( cu_strstartmatch("text/font",m))
return( &_GIcon_textfontbdf );
	if ( cu_strstartmatch("text/ps",m))
return( &_GIcon_textps );

return( &_GIcon_textplain );
    }

    if ( cu_strstartmatch("image/",m))
return( &_GIcon_image );
    if ( cu_strstartmatch("video/",m))
return( &_GIcon_video );
    if ( cu_strstartmatch("audio/",m))
return( &_GIcon_audio );
    if ( cu_strstartmatch("application/x-navid",m))
return( &_GIcon_dir );
    if ( cu_strstartmatch("application/x-object",m) )
return( &_GIcon_object );
    if ( cu_strstartmatch("application/x-core",m) )
return( &_GIcon_core );
    if ( cu_strstartmatch("application/x-tar",m) )
return( &_GIcon_tar );
    if ( cu_strstartmatch("application/x-compressed",m) )
return( &_GIcon_compressed );
    if ( cu_strstartmatch("application/x-font/ttf",m) || cu_strstartmatch("application/x-font/otf",m))
return( &_GIcon_ttf );
    if ( cu_strstartmatch("application/x-font/cid",m) )
return( &_GIcon_cid );
    if ( cu_strstartmatch("application/x-macbinary",m) || cu_strstartmatch("application/x-mac-binhex40",m) )
return( &_GIcon_mac );
    if ( cu_strstartmatch("application/x-mac-dfont",m) ||
	    cu_strstartmatch("application/x-mac-suit",m) )
return( &_GIcon_macttf );
    if ( cu_strstartmatch("application/x-font/pcf",m) || cu_strstartmatch("application/x-font/snf",m))
return( &_GIcon_textfontbdf );

return( &_GIcon_unknown );
}

static void GFileChooserFillList(GFileChooser *gfc,GDirEntry *first) {
    GDirEntry *e;
    int len;
    GTextInfo **ti;

    len = 0;
    for ( e=first; e!=NULL; e=e->next ) {
	e->fcdata = (gfc->filter)(&gfc->g,e);
	if ( e->fcdata!=fc_hide )
	    ++len;
    }

    ti = galloc((len+1)*sizeof(GTextInfo *));
    len = 0;
    for ( e=first; e!=NULL; e=e->next ) {
	if ( e->fcdata!=fc_hide ) {
	    ti[len] = gcalloc(1,sizeof(GTextInfo));
	    ti[len]->text = u_copy(e->name);
	    ti[len]->image = GFileChooserPickIcon(e);
	    ti[len]->fg = COLOR_DEFAULT;
	    ti[len]->bg = COLOR_DEFAULT;
	    ti[len]->font = NULL;
	    ti[len]->disabled = e->fcdata==fc_showdisabled;
	    ti[len]->image_precedes = true;
	    ti[len]->checked = e->isdir;
	    ++len;
	}
    }
    ti[len] = gcalloc(1,sizeof(GTextInfo));
    GGadgetSetList(&gfc->files->g,ti,false);

    GGadgetScrollListToText(&gfc->files->g,u_GFileNameTail(_GGadgetGetTitle(&gfc->name->g)),true);
}

static void GFileChooserIntermediateDir(GIOControl *gc) {
    GFileChooser *gfc = (GFileChooser *) (gc->userdata);

    GFileChooserFillList(gfc,GIOgetDirData(gc));
}

static void GFileChooserReceiveDir(GIOControl *gc) {
    GFileChooser *gfc = (GFileChooser *) (gc->userdata);

    GGadgetSetEnabled(&gfc->files->g,true);
    if ( gfc->lastname!=NULL ) {
	GGadgetSetTitle(&gfc->name->g,gfc->lastname);
	free(gfc->lastname);
	gfc->lastname=NULL;
    }
    GFileChooserFillList(gfc,GIOgetDirData(gc));
    GIOclose(gc);
    gfc->outstanding = NULL;
    GDrawSetCursor(gfc->g.base,gfc->old_cursor);
}

static void GFileChooserErrorDir(GIOControl *gc) {
    GFileChooser *gfc = (GFileChooser *) (gc->userdata);
    GTextInfo *ti[3], _ti[3];
    static unichar_t nullstr[] = { 0 };

    memset(_ti,'\0',sizeof(_ti));
    _ti[0].text = gc->error;
    if ( gc->status[0]!='\0' )
	_ti[1].text = gc->status;
    _ti[0].fg = _ti[0].bg = _ti[1].fg = _ti[1].bg = COLOR_DEFAULT;
    ti[0] = _ti; ti[1] = _ti+1; ti[2] = _ti+2;
    GGadgetSetEnabled(&gfc->files->g,false);
    GGadgetSetList(&gfc->files->g,ti,true);
    if ( gfc->lastname!=NULL ) {
	GGadgetSetTitle(&gfc->name->g,gfc->lastname);
	free(gfc->lastname);
	gfc->lastname=NULL;
    } else
	GGadgetSetTitle(&gfc->name->g,nullstr);
    if ( gfc->filterb!=NULL && gfc->ok!=NULL ) 
	_GWidget_MakeDefaultButton(&gfc->ok->g);
    GIOcancel(gc);
    gfc->outstanding = NULL;
    GDrawSetCursor(gfc->g.base,gfc->old_cursor);
}
    
static void GFileChooserScanDir(GFileChooser *gfc,unichar_t *dir) {
    GTextInfo **ti=NULL;
    int cnt, tot=0;
    unichar_t *pt, *ept;

    dir = u_GFileNormalize(dir);
    while ( 1 ) {
	ept = dir;
	cnt = 0;
	if ( (pt=uc_strstr(dir,"://"))!=NULL ) {
	    ept = u_strchr(pt+3,'/');
	    if ( ept==NULL )
		ept = pt+u_strlen(pt);
	    else
		++ept;
	} else if ( *dir=='/' )
	    ept = dir+1;
	if ( ept!=dir ) {
	    if ( ti!=NULL ) {
		ti[tot-cnt] = gcalloc(1,sizeof(GTextInfo));
		ti[tot-cnt]->text = u_copyn(dir,ept-dir);
		ti[tot-cnt]->fg = ti[tot-cnt]->bg = COLOR_DEFAULT;
	    }
	    cnt = 1;
	}
	for ( pt = ept; *pt!='\0'; pt = ept ) {
	    for ( ept = pt; *ept!='/' && *ept!='\0'; ++ept );
	    if ( ti!=NULL ) {
		ti[tot-cnt] = gcalloc(1,sizeof(GTextInfo));
		ti[tot-cnt]->text = u_copyn(pt,ept-pt);
		ti[tot-cnt]->fg = ti[tot-cnt]->bg = COLOR_DEFAULT;
	    }
	    ++cnt;
	    if ( *ept=='/' ) ++ept;
	}
	if ( ti!=NULL )
    break;
	ti = galloc((cnt+1)*sizeof(GTextInfo *));
	tot = cnt-1;
    }
    ti[cnt] = gcalloc(1,sizeof(GTextInfo));

    GGadgetSetList(&gfc->directories->g,ti,false);
    GGadgetSelectOneListItem(&gfc->directories->g,0);

    if ( gfc->outstanding!=NULL ) {
	GIOcancel(gfc->outstanding);
	gfc->outstanding = NULL;
    } else {
	gfc->old_cursor = GDrawGetCursor(gfc->g.base);
	GDrawSetCursor(gfc->g.base,ct_watch);
    }

    gfc->outstanding = GIOCreate(dir,gfc,GFileChooserReceiveDir,
	    GFileChooserErrorDir);
    gfc->outstanding->receiveintermediate = GFileChooserIntermediateDir;
    GIOdir(gfc->outstanding);
}

/* Handle events from the text field */
static int GFileChooserTextChanged(GGadget *t,GEvent *e) {
    GFileChooser *gfc;
    const unichar_t *pt, *spt;

    if ( e->type!=et_controlevent || e->u.control.subtype!=et_textchanged )
return( true );
    pt = spt = _GGadgetGetTitle(t);
    if ( pt==NULL )
return( true );
    while ( *pt && *pt!='/' && *pt!='*' && *pt!='?' && *pt!='[' && *pt!='{' )
	++pt;
    gfc = (GFileChooser *) GGadgetGetUserData(t);

    /* if there are no wildcards and no directories in the filename */
    /*  then as it gets changed the list box should show the closest */
    /*  approximation to it */
    if ( *pt=='\0' ) {
	GGadgetScrollListToText(&gfc->files->g,spt,true);
	if ( gfc->filterb!=NULL && gfc->ok!=NULL ) 
	    _GWidget_MakeDefaultButton(&gfc->ok->g);
    } else if ( *pt=='/' && e->u.control.u.tf_changed.from_pulldown!=-1 ) {
#if 1
	GEvent e;
	e.type = et_controlevent;
	e.u.control.subtype = et_buttonactivate;
	e.u.control.g = (gfc->ok!=NULL?&gfc->ok->g:&gfc->g);
	e.u.control.u.button.clicks = 0;
	e.w = e.u.control.g->base;
	if ( e.u.control.g->handle_controlevent != NULL )
	    (e.u.control.g->handle_controlevent)(e.u.control.g,&e);
	else
	    GDrawPostEvent(&e);
#else
	unichar_t *temp = u_copy(spt);
	GGadgetSetTitle((GGadget *) gfc,temp);
	free(temp);
#endif
    } else {
	if ( gfc->filterb!=NULL && gfc->ok!=NULL ) 
	    _GWidget_MakeDefaultButton(&gfc->filterb->g);
    }
    free(gfc->lastname); gfc->lastname = NULL;
return( true );
}

static unichar_t *GFileChooserGetCurDir(GFileChooser *gfc,int dirindex) {
    GTextInfo **ti;
    int len, j, cnt;
    unichar_t *dir, *pt;

    ti = GGadgetGetList(&gfc->directories->g,&len);
    if ( dirindex==-1 )
	dirindex = 0;
    dirindex = dirindex;

    for ( j=len-1, cnt=0; j>=dirindex; --j )
	cnt += u_strlen(ti[j]->text)+1;
    pt = dir = galloc((cnt+1)*sizeof(unichar_t));
    for ( j=len-1; j>=dirindex; --j ) {
	u_strcpy(pt,ti[j]->text);
	pt += u_strlen(pt);
	if ( pt[-1]!='/' )
	    *pt++ = '/';
    }
    *pt = '\0';
return( dir );
}

/* Handle events from the directory dropdown list */
static int GFileChooserDListChanged(GGadget *pl,GEvent *e) {
    GFileChooser *gfc;
    int i, len;
    unichar_t *dir;
    GTextInfo **ti;

    if ( e->type!=et_controlevent || e->u.control.subtype!=et_listselected )
return( true );
    i = GGadgetGetFirstListSelectedItem(pl);
    if ( i==-1 )
return(true);
    ti = GGadgetGetList(pl,&len);
    if ( i==0 )		/* Nothing changed */
return( true );

    gfc = (GFileChooser *) GGadgetGetUserData(pl);
    dir = GFileChooserGetCurDir(gfc,i);
    GFileChooserScanDir(gfc,dir);
    free(dir);
return( true );
}

/* Handle events from the file list list */
static int GFileChooserFListSelected(GGadget *gl,GEvent *e) {
    GFileChooser *gfc;
    int i;
    int listlen, len, cnt, dirpos, apos;
    unichar_t *dir, *newdir;
    GTextInfo *ti, **all;

    if ( e->type!=et_controlevent ||
	    ( e->u.control.subtype!=et_listselected &&
	      e->u.control.subtype!=et_listdoubleclick ))
return( true );
    if ( ((GList *) gl)->multiple_sel ) {
	all = GGadgetGetList(gl,&listlen);
	len = cnt = 0;
	dirpos = apos = -1;
	for ( i=0; i<listlen; ++i ) {
	    if ( !all[i]->selected )
		/* Not selected? ignore it */;
	    else if ( all[i]->checked )		/* Directory */
		dirpos = i;
	    else {
		len += u_strlen( all[i]->text ) + 2;
		++cnt;
		apos = i;
	    }
	}
	if ( dirpos!=-1 && cnt>0 ) {
	    /* If a directory was selected, clear everthing else */
	    for ( i=0; i<listlen; ++i ) if ( i!=dirpos )
		all[i]->selected = false;
	    _ggadget_redraw(gl);
	}
	if ( dirpos!=-1 ) { cnt = 1; apos = dirpos; }
    } else {
	cnt = 1;
	apos = GGadgetGetFirstListSelectedItem(gl);
    }
    if ( apos==-1 )
return(true);
    gfc = (GFileChooser *) GGadgetGetUserData(gl);
    ti = GGadgetGetListItem(gl,apos);
    if ( e->u.control.subtype==et_listselected && cnt==1 ) {
	/* Nope, quite doesn't work. Goal is to remember first filename. But */
	/*  if user types into the list box we'll (probably) get several diff*/
	/*  filenames before we hit the directory. So we just ignore the typ-*/
	/*  ing case for now. */
	if ( ti->checked /* it's a directory */ && e->u.control.u.list.from_mouse &&
		gfc->lastname==NULL )
	    gfc->lastname = GGadgetGetTitle(&gfc->name->g);
	if ( ti->checked ) {
	    unichar_t *val = galloc((u_strlen(ti->text)+2)*sizeof(unichar_t));
	    u_strcpy(val,ti->text);
	    uc_strcat(val,"/");
	    GGadgetSetTitle(&gfc->name->g,val);
	    free(val);
	    if ( gfc->filterb!=NULL && gfc->ok!=NULL )
		_GWidget_MakeDefaultButton(&gfc->filterb->g);
	} else {
	    GGadgetSetTitle(&gfc->name->g,ti->text);
	    if ( gfc->filterb!=NULL && gfc->ok!=NULL )
		_GWidget_MakeDefaultButton(&gfc->ok->g);
	}
    } else if ( e->u.control.subtype==et_listselected ) {
	unichar_t *val, *upt = galloc((len+1)*sizeof(unichar_t));
	val = upt;
	for ( i=0; i<listlen; ++i ) {
	    if ( all[i]->selected ) {
		u_strcpy(upt,all[i]->text);
		upt += u_strlen(upt);
		if ( --cnt>0 ) {
		    uc_strcpy(upt,"; ");
		    upt += 2;
		}
	    }
	}
	GGadgetSetTitle(&gfc->name->g,val);
	free(val);
    } else if ( ti->checked /* it's a directory */ ) {
	dir = GFileChooserGetCurDir(gfc,-1);
	newdir = u_GFileAppendFile(dir,ti->text,true);
	GFileChooserScanDir(gfc,newdir);
	free(dir); free(newdir);
    } else {
	/* Post the double click (on a file) to the parent */
	/*  if we know what the ok button is then pretend it got pressed */
	/*  otherwise just send a list double click from ourselves */
	if ( gfc->ok!=NULL ) {
	    e->u.control.subtype = et_buttonactivate;
	    e->u.control.g = &gfc->ok->g;
	    e->u.control.u.button.clicks = 0;
	    e->w = e->u.control.g->base;
	} else
	    e->u.control.g = &gfc->g;
	if ( e->u.control.g->handle_controlevent != NULL )
	    (e->u.control.g->handle_controlevent)(e->u.control.g,e);
	else
	    GDrawPostEvent(e);
    }
return( true );
}

/* Routine to be called by the filter button */
void GFileChooserFilterIt(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
    unichar_t *pt, *spt, *slashpt, *dir, *temp;
    int wasdir;

    wasdir = gfc->lastname!=NULL;

    spt = (unichar_t *) _GGadgetGetTitle(&gfc->name->g);
    if (( slashpt = u_strrchr(spt,'/'))==NULL )
	slashpt = spt;
    else
	++slashpt;
    pt = slashpt;
    while ( *pt && *pt!='*' && *pt!='?' && *pt!='[' && *pt!='{' )
	++pt;
    if ( *pt!='\0' ) {
	free(gfc->wildcard);
	gfc->wildcard = u_copy(slashpt);
    } else if ( gfc->lastname==NULL )
	gfc->lastname = u_copy(slashpt);
    if ( uc_strstr(spt,"://")!=NULL || *spt=='/' )	/* IsAbsolute */
	dir = u_copyn(spt,slashpt-spt);
    else {
	unichar_t *curdir = GFileChooserGetCurDir(gfc,-1);
	if ( slashpt!=spt ) {
	    temp = u_copyn(spt,slashpt-spt);
	    dir = u_GFileAppendFile(curdir,temp,true);
	    free(temp);
	} else if ( wasdir && *pt=='\0' )
	    dir = u_GFileAppendFile(curdir,spt,true);
	else
	    dir = curdir;
	if ( dir!=curdir )
	    free(curdir);
    }
    GFileChooserScanDir(gfc,dir);
    free(dir);
}

/* A function that may be connected to a filter button as its handle_controlevent */
int GFileChooserFilterEh(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate )
	GFileChooserFilterIt(GGadgetGetUserData(g));
return( true );
}

void GFileChooserConnectButtons(GGadget *g,GGadget *ok, GGadget *filter) {
    GFileChooser *gfc = (GFileChooser *) g;
    gfc->ok = (GButton *) ok;
    gfc->filterb = (GButton *) filter;
}

void GFileChooserSetFilterText(GGadget *g,const unichar_t *wildcard) {
    GFileChooser *gfc = (GFileChooser *) g;
    free(gfc->wildcard);
    gfc->wildcard = u_copy(wildcard);
}

unichar_t *GFileChooserGetFilterText(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
return( gfc->wildcard );
}

void GFileChooserSetMimetypes(GGadget *g,unichar_t **mimetypes) {
    GFileChooser *gfc = (GFileChooser *) g;
    int i;

    if ( gfc->mimetypes ) {
	for ( i=0; gfc->mimetypes[i]!=NULL; ++i )
	    free( gfc->mimetypes[i]);
	free(gfc->mimetypes);
    }
    if ( mimetypes ) {
	for ( i=0; mimetypes[i]!=NULL; ++i );
	gfc->mimetypes = galloc((i+1)*sizeof(unichar_t *));
	for ( i=0; mimetypes[i]!=NULL; ++i )
	    gfc->mimetypes[i] = u_copy(mimetypes[i]);
	gfc->mimetypes[i] = NULL;
    } else
	gfc->mimetypes = NULL;
}

unichar_t **GFileChooserGetMimetypes(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
return( gfc->mimetypes );
}

/* Change the current file, or the current directory/file */
static void GFileChooserSetTitle(GGadget *g,const unichar_t *tit) {
    GFileChooser *gfc = (GFileChooser *) g;
    unichar_t *pt, *curdir, *temp, *dir, *base;

    if ( tit==NULL ) {
	curdir = GFileChooserGetCurDir(gfc,-1);
	GFileChooserScanDir(gfc,curdir);
	free(curdir);
return;
    }

    pt = u_strrchr(tit,'/');
    free(gfc->lastname);
    gfc->lastname = NULL;
    if ( (base=uc_strstr(tit,"://"))!=NULL || *(base=(unichar_t *) tit)=='/' ) {
	if ( pt>base ) {
	    if ( pt[1]!='\0' )
		gfc->lastname = u_copy(pt+1);
	    dir = u_copyn(tit,pt-tit);
	} else {
	    gfc->lastname = NULL;
	    dir = u_copy(tit);
	}
	GFileChooserScanDir(gfc,dir);
	free(dir);
    } else if ( pt==NULL ) {
	GGadgetSetTitle(&gfc->name->g,tit);
	curdir = GFileChooserGetCurDir(gfc,-1);
	GFileChooserScanDir(gfc,curdir);
	free(curdir);
    } else {
	curdir = GFileChooserGetCurDir(gfc,-1);
	temp = u_copyn(tit,pt-tit);
	dir = u_GFileAppendFile(curdir,temp,true);
	free(temp); free(curdir);
	free(gfc->lastname);
	if ( pt[1]!='\0' )
	    gfc->lastname = u_copy(pt+1);
	GFileChooserScanDir(gfc,dir);
	free(dir);
    }
}

void GFileChooserRefreshList(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
    unichar_t *curdir;

    curdir = GFileChooserGetCurDir(gfc,-1);
    GFileChooserScanDir(gfc,curdir);
    free(curdir);
}

/* Get the current directory/file */
static unichar_t *GFileChooserGetTitle(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
    unichar_t *spt, *curdir, *file;

    spt = (unichar_t *) _GGadgetGetTitle(&gfc->name->g);
    if ( uc_strstr(spt,"://")!=NULL || *spt=='/' )	/* IsAbsolute */
	file = u_copy(spt);
    else {
	curdir = GFileChooserGetCurDir(gfc,-1);
	file = u_GFileAppendFile(curdir,spt,gfc->lastname!=NULL);
	free(curdir);
    }
return( file );
}

static void GFileChooser_destroy(GGadget *g) {
    GFileChooser *gfc = (GFileChooser *) g;
    int i;

    free(lastdir);
    lastdir = GFileChooserGetCurDir(gfc,-1);

    if ( gfc->outstanding )
	GIOcancel(gfc->outstanding);
    GGadgetDestroy(&gfc->name->g);
    GGadgetDestroy(&gfc->files->g);
    GGadgetDestroy(&gfc->directories->g);
    free(gfc->wildcard);
    free(gfc->lastname);
    if ( gfc->mimetypes ) {
	for ( i=0; gfc->mimetypes[i]!=NULL; ++i )
	    free( gfc->mimetypes[i]);
	free(gfc->mimetypes);
    }
    _ggadget_destroy(&gfc->g);
}

static int gfilechooser_expose(GWindow pixmap, GGadget *g, GEvent *event) {
return( true );
}

static int gfilechooser_noop(GGadget *g, GEvent *event) {
return( false );
}

static int gfilechooser_timer(GGadget *g, GEvent *event) {
return( false );
}

static void gfilechooser_move(GGadget *g, int32 x, int32 y ) {
    GFileChooser *gfc = (GFileChooser *) g;
    GGadgetMove(&gfc->files->g,x+(gfc->files->g.r.x-g->r.x),y+(gfc->files->g.r.y-g->r.y));
    GGadgetMove(&gfc->directories->g,x+(gfc->directories->g.r.x-g->r.x),y+(gfc->directories->g.r.y-g->r.y));
    GGadgetMove(&gfc->name->g,x+(gfc->name->g.r.x-g->r.x),y+(gfc->name->g.r.y-g->r.y));
    _ggadget_move(g,x,y);
}

static void gfilechooser_resize(GGadget *g, int32 width, int32 height ) {
    GFileChooser *gfc = (GFileChooser *) g;

    if ( width!=gfc->g.r.width ) {
	GGadgetMove(&gfc->directories->g,gfc->g.r.x,gfc->g.r.y);
	GGadgetMove(&gfc->name->g,gfc->g.r.x,gfc->g.r.y+height-gfc->name->g.r.height);

	GGadgetResize(&gfc->directories->g,width,gfc->directories->g.r.height);
	GGadgetResize(&gfc->name->g,width,gfc->name->g.r.height);
    } else {
	GGadgetMove(&gfc->name->g,gfc->name->g.r.x,gfc->g.r.y+height-gfc->name->g.r.height);
    }
    GGadgetResize(&gfc->files->g,width,height-gfc->directories->g.r.height-gfc->name->g.r.height);
    _ggadget_resize(g,width,height);
}

static void gfilechooser_setvisible(GGadget *g, int visible ) {
    GFileChooser *gfc = (GFileChooser *) g;
    GGadgetSetVisible(&gfc->files->g,visible);
    GGadgetSetVisible(&gfc->directories->g,visible);
    GGadgetSetVisible(&gfc->name->g,visible);
    _ggadget_setvisible(g,visible);
}

static void gfilechooser_setenabled(GGadget *g, int enabled ) {
    GFileChooser *gfc = (GFileChooser *) g;
    GGadgetSetEnabled(&gfc->files->g,enabled);
    GGadgetSetEnabled(&gfc->directories->g,enabled);
    GGadgetSetEnabled(&gfc->name->g,enabled);
    _ggadget_setenabled(g,enabled);
}

struct gfuncs GFileChooser_funcs = {
    0,
    sizeof(struct gfuncs),

    gfilechooser_expose,
    gfilechooser_noop,
    gfilechooser_noop,
    NULL,
    NULL,
    gfilechooser_timer,
    NULL,

    _ggadget_redraw,
    gfilechooser_move,
    gfilechooser_resize,
    gfilechooser_setvisible,
    gfilechooser_setenabled,
    _ggadget_getsize,
    _ggadget_getinnersize,

    GFileChooser_destroy,

    GFileChooserSetTitle,
    NULL,
    GFileChooserGetTitle
};

static void GFileChooserCreateChildren(GFileChooser *gfc, int flags) {
    GGadgetData gd;
    int space = GDrawPointsToPixels(gfc->g.base,3);

    memset(&gd,'\0',sizeof(gd));
    gd.pos.y = gfc->g.r.y; gd.pos.height = 0;
    gd.pos.width = GGadgetScale(GDrawPointsToPixels(gfc->g.base,150));
    if ( gd.pos.width>gfc->g.r.width ) gd.pos.width = gfc->g.r.width;
    gd.pos.x = gfc->g.r.x+(gfc->g.r.width - gd.pos.width)/2;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_list_exactlyone;
    gd.handle_controlevent = GFileChooserDListChanged;
    gfc->directories = (GListButton *) GListButtonCreate(gfc->g.base,&gd,gfc);
    gfc->directories->g.contained = true;

    gd.pos.height = 0/*gfc->directories->g.r.height*/;
    gd.pos.y = gfc->g.r.y+gfc->g.r.height-gfc->directories->g.r.height;
    gd.pos.width = gfc->g.r.width;
    gd.pos.x = gfc->g.r.x;
    gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels;
    gd.handle_controlevent = GFileChooserTextChanged;
    if ( flags&gg_file_pulldown )
	gfc->name = (GTextField *) GListFieldCreate(gfc->g.base,&gd,gfc);
    else
	gfc->name = (GTextField *) GTextFieldCreate(gfc->g.base,&gd,gfc);
    gfc->name->g.contained = true;

    gd.pos.height = gfc->g.r.height-
	    (2*gfc->directories->g.r.height+2*space);
    gd.pos.y = gfc->g.r.y+gfc->directories->g.r.height+space;
    if ( flags & gg_file_multiple )
	gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_list_alphabetic|gg_list_multiplesel;
    else
	gd.flags = gg_visible|gg_enabled|gg_pos_in_pixels|gg_list_alphabetic|gg_list_exactlyone;
    gd.handle_controlevent = GFileChooserFListSelected;
    gfc->files = (GList *) GListCreate(gfc->g.base,&gd,gfc);
    gfc->files->g.contained = true;
}

GGadget *GFileChooserCreate(struct gwindow *base, GGadgetData *gd,void *data) {
    GFileChooser *gfc = (GFileChooser *) gcalloc(1,sizeof(GFileChooser));;
    
    gfc->g.funcs = &GFileChooser_funcs;
    _GGadget_Create(&gfc->g,base,gd,data,&gfilechooser_box);
    gfc->g.takes_input = gfc->g.takes_keyboard = false; gfc->g.focusable = false;
    if ( gfc->g.r.width == 0 )
	gfc->g.r.width = GGadgetScale(GDrawPointsToPixels(base,140));
    if ( gfc->g.r.height == 0 )
	gfc->g.r.height = GDrawPointsToPixels(base,100);
    gfc->g.inner = gfc->g.r;
    _GGadget_FinalPosition(&gfc->g,base,gd);

    GFileChooserCreateChildren(gfc, gd->flags);
    gfc->filter = GFileChooserDefFilter;
    
    if ( gd->flags & gg_group_end )
	_GGadgetCloseGroup(&gfc->g);

    if ( lastdir==NULL ) {
	static unichar_t dot[] = { '.', '\0' };
	unichar_t buffer[1025];
	lastdir = u_copy(u_GFileGetAbsoluteName(dot,buffer,sizeof(buffer)/sizeof(unichar_t)));
    }
    if ( gd->label==NULL || gd->label->text==NULL )
	GFileChooserSetTitle(&gfc->g,lastdir);
    else if ( uc_strstr(gd->label->text,"://")!=NULL || *gd->label->text=='/' )
	GFileChooserSetTitle(&gfc->g,gd->label->text);
    else {
	unichar_t *temp = u_GFileAppendFile(lastdir,gd->label->text,false);
	temp = u_GFileNormalize(temp);
	GFileChooserSetTitle(&gfc->g,temp);
	free(temp);
    }

return( &gfc->g );
}

void GFileChooserSetDir(GGadget *g,unichar_t *dir) {
    GFileChooserScanDir((GFileChooser *) g,dir);
}

unichar_t *GFileChooserGetDir(GGadget *g) {
return( GFileChooserGetCurDir((GFileChooser *) g,-1));
}

GIOControl *GFileChooserReplaceIO(GGadget *g,GIOControl *gc) {
    GFileChooser *gfc = (GFileChooser *)g;

    if ( gfc->outstanding!=NULL ) {
	GIOclose(gfc->outstanding);
	gfc->outstanding = NULL;
	GDrawSetCursor(gfc->g.base,gfc->old_cursor);
    }
    if ( gc!=NULL ) {
	gfc->old_cursor = GDrawGetCursor(gfc->g.base);
	GDrawSetCursor(gfc->g.base,ct_watch);
	gfc->outstanding = gc;
    }
return( gc );
}

void GFileChooserGetChildren(GGadget *g,GGadget **pulldown, GGadget **list, GGadget **tf) {
    GFileChooser *gfc = (GFileChooser *)g;

    if ( pulldown!=NULL ) *pulldown = &gfc->directories->g;
    if ( tf!=NULL )	  *tf = &gfc->name->g;
    if ( list!=NULL )	  *list = &gfc->files->g;
}
