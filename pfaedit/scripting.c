/* Copyright (C) 2002-2003 by George Williams */
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
/*			   Yet another interpreter			      */

#include "pfaeditui.h"
#include <gfile.h>
#include <gresource.h>
#include <utype.h>
#include <ustring.h>
#include <gkeysym.h>
#include <chardata.h>
#include <unistd.h>
#include <math.h>
#include <setjmp.h>

static int verbose = -1;

struct dictentry {
    char *name;
    Val val;
};

struct dictionary {
    struct dictentry *entries;
    int cnt, max;
};

static struct dictionary globals;

typedef struct array {
    int argc;
    Val *vals;
} Array;

#define TOK_MAX	100
enum token_type { tt_name, tt_string, tt_number, tt_unicode,
	tt_lparen, tt_rparen, tt_comma, tt_eos,		/* eos is end of statement, semicolon, newline */
	tt_lbracket, tt_rbracket,
	tt_minus, tt_plus, tt_not, tt_bitnot, tt_colon,
	tt_mul, tt_div, tt_mod, tt_and, tt_or, tt_bitand, tt_bitor, tt_xor,
	tt_eq, tt_ne, tt_gt, tt_lt, tt_ge, tt_le,
	tt_assign, tt_pluseq, tt_minuseq, tt_muleq, tt_diveq, tt_modeq,
	tt_incr, tt_decr,

	tt_if, tt_else, tt_elseif, tt_endif, tt_while, tt_foreach, tt_endloop,
	tt_shift, tt_return,

	tt_eof,

	tt_error = -1
};

typedef struct context {
    struct context *caller;
    Array a;		/* args */
    Array **dontfree;
    struct dictionary locals;
    FILE *script;
    unsigned int backedup: 1;
    unsigned int donteval: 1;
    unsigned int returned: 1;
    char tok_text[TOK_MAX+1];
    enum token_type tok;
    Val tok_val;
    Val return_val;
    Val trace;
    Val argsval;
    char *filename;
    int lineno;
    int ungotch;
    FontView *curfv;
    jmp_buf *err_env;
} Context;

struct keywords { enum token_type tok; char *name; } keywords[] = {
    { tt_if, "if" },
    { tt_else, "else" },
    { tt_elseif, "elseif" },
    { tt_endif, "endif" },
    { tt_while, "while" },
    { tt_foreach, "foreach" },
    { tt_endloop, "endloop" },
    { tt_shift, "shift" },
    { tt_return, "return" },
    { 0, NULL }
};

static const char *toknames[] = {
    "name", "string", "number", "unicode id", 
    "lparen", "rparen", "comma", "end of statement",
    "lbracket", "rbracket",
    "minus", "plus", "logical not", "bitwise not", "colon",
    "multiply", "divide", "mod", "logical and", "logical or", "bitwise and", "bitwise or", "bitwise xor",
    "equal to", "not equal to", "greater than", "less than", "greater than or equal to", "less than or equal to",
    "assignment", "plus equals", "minus equals", "mul equals", "div equals", "mod equals",
    "increment", "decrement",
    "if", "else", "elseif", "endif", "while", "foreach", "endloop",
    "shift", "return",
    "End Of File",
    NULL };

static void arrayfree(Array *a) {
    int i;

    for ( i=0; i<a->argc; ++i ) {
	if ( a->vals[i].type==v_str )
	    free(a->vals[i].u.sval);
	else if ( a->vals[i].type==v_arr )
	    arrayfree(a->vals[i].u.aval);
    }
    free(a->vals);
    free(a);
}

static Array *arraycopy(Array *a) {
    int i;
    Array *c;

    c = galloc(sizeof(Array));
    c->argc = a->argc;
    c->vals = galloc(c->argc*sizeof(Val));
    memcpy(c->vals,a->vals,c->argc*sizeof(Val));
    for ( i=0; i<a->argc; ++i ) {
	if ( a->vals[i].type==v_str )
	    c->vals[i].u.sval = copy(a->vals[i].u.sval);
	else if ( a->vals[i].type==v_arr )
	    c->vals[i].u.aval = arraycopy(a->vals[i].u.aval);
    }
return( c );
}

void DictionaryFree(struct dictionary *dica) {
    int i;

    if ( dica==NULL )
return;

    for ( i=0; i<dica->cnt; ++i ) {
	free(dica->entries[i].name );
	if ( dica->entries[i].val.type == v_str )
	    free( dica->entries[i].val.u.sval );
	if ( dica->entries[i].val.type == v_arr )
	    arrayfree( dica->entries[i].val.u.aval );
    }
    free( dica->entries );
}

static int DicaLookup(struct dictionary *dica,char *name,Val *val) {
    int i;

    if ( dica!=NULL && dica->entries!=NULL ) {
	for ( i=0; i<dica->cnt; ++i )
	    if ( strcmp(dica->entries[i].name,name)==0 ) {
		val->type = v_lval;
		val->u.lval = &dica->entries[i].val;
return( true );
	    }
    }
return( false );
}

static void DicaNewEntry(struct dictionary *dica,char *name,Val *val) {

    if ( dica->entries==NULL ) {
	dica->max = 10;
	dica->entries = galloc(dica->max*sizeof(struct dictentry));
    } else if ( dica->cnt>=dica->max ) {
	dica->max += 10;
	dica->entries = grealloc(dica->entries,dica->max*sizeof(struct dictentry));
    }
    dica->entries[dica->cnt].name = copy(name);
    dica->entries[dica->cnt].val.type = v_void;
    val->type = v_lval;
    val->u.lval = &dica->entries[dica->cnt].val;
    ++dica->cnt;
}


static void calldatafree(Context *c) {
    int i;

    for ( i=1; i<c->a.argc; ++i ) {	/* child may have freed some args itself by shifting, but argc will reflect the proper values none the less */
	if ( c->a.vals[i].type == v_str )
	    free( c->a.vals[i].u.sval );
	if ( c->a.vals[i].type == v_arrfree || (c->a.vals[i].type == v_arr && c->dontfree[i]!=c->a.vals[i].u.aval ))
	    arrayfree( c->a.vals[i].u.aval );
    }
    DictionaryFree(&c->locals);

    if ( c->script!=NULL )
	fclose(c->script);
}

static void traceback(Context *c) {
    int cnt = 0;
    while ( c!=NULL ) {
	if ( cnt==1 ) printf( "Called from...\n" );
	if ( cnt>0 ) fprintf( stderr, " %s:%d\n", c->filename, c->lineno );
	calldatafree(c);
	if ( c->err_env!=NULL )
	    longjmp(*c->err_env,1);
	c = c->caller;
	++cnt;
    }
    exit(1);
}

static void showtoken(Context *c,enum token_type got) {
    if ( got==tt_name || got==tt_string )
	fprintf( stderr, " \"%s\"\n", c->tok_text );
    else if ( got==tt_number )
	fprintf( stderr, " %d (0x%x)\n", c->tok_val.u.ival, c->tok_val.u.ival );
    else if ( got==tt_unicode )
	fprintf( stderr, " 0u%x\n", c->tok_val.u.ival );
    else
	fprintf( stderr, "\n" );
    traceback(c);
}

static void expect(Context *c,enum token_type expected, enum token_type got) {
    if ( got!=expected ) {
	fprintf( stderr, "%s:%d Expected %s, got %s",
		c->filename, c->lineno, toknames[expected], toknames[got] );
    if ( screen_display!=NULL ) {
	static unichar_t umsg[] = { '%','h','s',':','%','d',' ','E','x','p','e','c','t','e','d',' ','%','h','s',',',' ','g','o','t',' ','%','h','s',  0 };
	GWidgetError(NULL,umsg,c->filename, c->lineno, toknames[expected], toknames[got] );
    }
	showtoken(c,got);
    }
}

static void unexpected(Context *c,enum token_type got) {
    fprintf( stderr, "%s:%d Unexpected %s found",
	    c->filename, c->lineno, toknames[got] );
    if ( screen_display!=NULL ) {
	static unichar_t umsg[] = { '%','h','s',':','%','d',' ','U','n','e','x','p','e','c','t','e','d',' ','%','h','s',  0 };
	GWidgetError(NULL,umsg,c->filename, c->lineno, toknames[got] );
    }
    showtoken(c,got);
}

static void error( Context *c, char *msg ) {
    fprintf( stderr, "%s:%d %s\n", c->filename, c->lineno, msg );
    if ( screen_display!=NULL ) {
	static unichar_t umsg[] = { '%','h','s',':','%','d',' ','%','h','s',  0 };
	GWidgetError(NULL,umsg,c->filename, c->lineno, msg );
    }
    traceback(c);
}

static void errors( Context *c, char *msg, char *name) {
    fprintf( stderr, "%s:%d %s: %s\n", c->filename, c->lineno, msg, name );
    if ( screen_display!=NULL ) {
	static unichar_t umsg[] = { '%','h','s',':','%','d',' ','%','h','s',':',' ','%','h','s',  0 };
	GWidgetError(NULL,umsg,c->filename, c->lineno, msg, name );
    }
    traceback(c);
}

static void dereflvalif(Val *val) {
    if ( val->type == v_lval ) {
	*val = *val->u.lval;
	if ( val->type==v_str )
	    val->u.sval = copy(val->u.sval);
    }
}

/* *************************** Built in Functions *************************** */

static void PrintVal(Val *val) {
    int j;

    if ( val->type==v_str )
	printf( "%s", val->u.sval );
    else if ( val->type==v_arr ) {
	putchar( '[' );
	if ( val->u.aval->argc>0 ) {
	    PrintVal(&val->u.aval->vals[0]);
	    for ( j=1; j<val->u.aval->argc; ++j ) {
		putchar(',');
		PrintVal(&val->u.aval->vals[j]);
	    }
	}
	putchar( ']' );
    } else if ( val->type==v_int )
	printf( "%d", val->u.ival );
    else if ( val->type==v_unicode )
	printf( "0u%x", val->u.ival );
    else if ( val->type==v_void )
	printf( "<void>");
    else
	printf( "<???>");
}

static void bPrint(Context *c) {
    int i;

    for ( i=1; i<c->a.argc; ++i )
	PrintVal(&c->a.vals[i] );
    printf( "\n" );
}

static void bError(Context *c) {

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Expected string argument" );

    error( c, c->a.vals[1].u.sval );
}

static void bAskUser(Context *c) {
    char *quest, *def="";

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || ( c->a.argc==3 &&  c->a.vals[2].type!=v_str) )
	error( c, "Expected string argument" );
    quest = c->a.vals[1].u.sval;
    if ( c->a.argc==3 )
	def = c->a.vals[2].u.sval;
    if ( screen_display==NULL ) {
	char buffer[300];
	printf( "%s", quest );
	buffer[0] = '\0';
	c->return_val.type = v_str;
	if ( fgets(buffer,sizeof(buffer),stdin)==NULL ) {
	    clearerr(stdin);
	    c->return_val.u.sval = copy("");
	} else if ( buffer[0]=='\0' )
	    c->return_val.u.sval = copy(def);
	else
	    c->return_val.u.sval = copy(buffer);
    } else {
	unichar_t *t1, *t2, *ret;
	static unichar_t format[] = { '%','s', 0 };
	t1 = uc_copy(quest);
	ret = GWidgetAskString(t1,t2=uc_copy(def),format, t1);
	free(t1);
	free(t2);
	c->return_val.type = v_str;
	c->return_val.u.sval = cu_copy(ret);
	if ( ret==NULL )
	    c->return_val.u.sval = copy("");
	else
	    free(ret);
    }
}

static void bArray(Context *c) {
    int i;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_int )
	error( c, "Expected integer argument" );
    else if ( c->a.vals[1].u.ival<=0 )
	error( c, "Argument must be positive" );
    c->return_val.type = v_arrfree;
    c->return_val.u.aval = galloc(sizeof(Array));
    c->return_val.u.aval->argc = c->a.vals[1].u.ival;
    c->return_val.u.aval->vals = galloc(c->a.vals[1].u.ival*sizeof(Val));
    for ( i=0; i<c->a.vals[1].u.ival; ++i )
	c->return_val.u.aval->vals[i].type = v_void;
}

static void bStrlen(Context *c) {

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Bad type for argument" );

    c->return_val.type = v_int;
    c->return_val.u.ival = strlen( c->a.vals[1].u.sval );
}

static void bStrstr(Context *c) {
    char *pt;

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_str )
	error( c, "Bad type for argument" );

    c->return_val.type = v_int;
    pt = strstr(c->a.vals[1].u.sval,c->a.vals[2].u.sval);
    c->return_val.u.ival = pt==NULL ? -1 : pt-c->a.vals[1].u.sval;
}

static void bStrcasestr(Context *c) {
    char *pt;

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_str )
	error( c, "Bad type for argument" );

    c->return_val.type = v_int;
    pt = strstrmatch(c->a.vals[1].u.sval,c->a.vals[2].u.sval);
    c->return_val.u.ival = pt==NULL ? -1 : pt-c->a.vals[1].u.sval;
}

static void bStrrstr(Context *c) {
    char *pt;
    char *haystack, *needle;
    int nlen;

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_str )
	error( c, "Bad type for argument" );

    c->return_val.type = v_int;
    haystack = c->a.vals[1].u.sval; needle = c->a.vals[2].u.sval;
    nlen = strlen( needle );
    for ( pt=haystack+strlen(haystack)-nlen; pt>=haystack; --pt )
	if ( strncmp(pt,needle,nlen)==0 )
    break;
    c->return_val.u.ival = pt-haystack;
}

static void bStrsub(Context *c) {
    int start, end;
    char *str;

    if ( c->a.argc!=3 && c->a.argc!=4 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_int ||
	    (c->a.argc==4 && c->a.vals[3].type!=v_int))
	error( c, "Bad type for argument" );

    str = c->a.vals[1].u.sval;
    start = c->a.vals[2].u.ival;
    end = c->a.argc==4? c->a.vals[3].u.ival : strlen(str);
    if ( start<0 || start>strlen(str) || end<start || end>strlen(str) )
	error( c, "Arguments out of bounds" );
    c->return_val.type = v_str;
    c->return_val.u.sval = copyn(str+start,end-start);
}

static void bStrcasecmp(Context *c) {

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_str )
	error( c, "Bad type for argument" );

    c->return_val.type = v_int;
    c->return_val.u.ival = strmatch(c->a.vals[1].u.sval,c->a.vals[2].u.sval);
}

static void bStrtol(Context *c) {
    int base = 10;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || (c->a.argc==3 && c->a.vals[2].type!=v_int) )
	error( c, "Bad type for argument" );
    else if ( c->a.argc==3 ) {
	base = c->a.vals[2].u.ival;
	if ( base<0 || base==1 || base>36 )
	    error( c, "Argument out of bounds" );
    }

    c->return_val.type = v_int;
    c->return_val.u.ival = strtol(c->a.vals[1].u.sval,NULL,base);
}

static void bStrskipint(Context *c) {
    int base = 10;
    char *end;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || (c->a.argc==3 && c->a.vals[2].type!=v_int) )
	error( c, "Bad type for argument" );
    else if ( c->a.argc==3 ) {
	base = c->a.vals[2].u.ival;
	if ( base<0 || base==1 || base>36 )
	    error( c, "Argument out of bounds" );
    }

    c->return_val.type = v_int;
    strtol(c->a.vals[1].u.sval,&end,base);
    c->return_val.u.ival = end-c->a.vals[1].u.sval;
}

static void bGetPrefs(Context *c) {

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Bad type for argument" );
    if ( !GetPrefs(c->a.vals[1].u.sval,&c->return_val) )
	errors( c, "Unknown Preference variable", c->a.vals[1].u.sval );
}

static void bSetPrefs(Context *c) {
    int ret;

    if ( c->a.argc!=3 && c->a.argc!=4 )
	error( c, "Wrong number of arguments" );
    else if ( c->a.vals[1].type!=v_str || (c->a.argc==4 && c->a.vals[3].type!=v_int) )
	error( c, "Bad type for argument" );
    if ( (ret=SetPrefs(c->a.vals[1].u.sval,&c->a.vals[2],c->a.argc==4?&c->a.vals[3]:NULL))==0 )
	errors( c, "Unknown Preference variable", c->a.vals[1].u.sval );
    else if ( ret==-1 )
	errors( c, "Bad type for preference variable",  c->a.vals[1].u.sval);
}

/* **** File menu **** */

static void bQuit(Context *c) {
    if ( verbose>0 ) putchar('\n');
    if ( c->a.argc==1 )
exit(0);
    if ( c->a.argc>2 )
	error( c, "Too many arguments" );
    else if ( c->a.vals[1].type!=v_int )
	error( c, "Expected integer argument" );
    else
exit(c->a.vals[1].u.ival );
exit(1);
}

static FontView *FVAppend(FontView *fv) {
    /* Normally fontviews get added to the fv list when their windows are */
    /*  created. but we don't create any windows here, so... */
    FontView *test;

    if ( fv_list==NULL ) fv_list = fv;
    else {
	for ( test = fv_list; test->next!=NULL; test=test->next );
	test->next = fv;
    }
return( fv );
}

static void bOpen(Context *c) {
    SplineFont *sf;
    int openflags=0;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Open expects a filename" );
    else if ( c->a.argc==3 ) {
	if ( c->a.vals[2].type!=v_int )
	    error( c, "Open expects an integer for second argument" );
	openflags = c->a.vals[2].u.ival;
    }
    sf = LoadSplineFont(c->a.vals[1].u.sval,openflags);
    if ( sf==NULL )
	errors(c, "Failed to open", c->a.vals[1].u.sval);
    if ( sf->fv!=NULL )
	/* All done */;
    else if ( screen_display!=NULL )
	FontViewCreate(sf);
    else
	FVAppend(_FontViewCreate(sf));
    c->curfv = sf->fv;
}

static void bNew(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    c->curfv = FVAppend(_FontViewCreate(SplineFontNew()));
}

static void bClose(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    if ( fv_list==c->curfv )
	fv_list = c->curfv->next;
    else {
	FontView *n;
	for ( n=fv_list; n->next!=c->curfv; n=n->next );
	n->next = c->curfv->next;
    }
    FontViewFree(c->curfv);
    c->curfv = NULL;
}

static void bSave(Context *c) {
    SplineFont *sf = c->curfv->sf;

    if ( c->a.argc>2 )
	error( c, "Wrong number of arguments");
    if ( c->a.argc==2 ) {
	if ( c->a.vals[1].type!=v_str )
	    error(c,"If an argument is given to Save it must be a filename");
	if ( !SFDWrite(c->a.vals[1].u.sval,sf))
	    error(c,"Save As failed" );
    } else {
	if ( sf->filename==NULL )
	    error(c,"This font has no associated sfd file yet, you must specify a filename" );
	if ( !SFDWriteBak(sf) )
	    error(c,"Save failed" );
    }
}

static void bGenerate(Context *c) {
    SplineFont *sf = c->curfv->sf;
    char *bitmaptype = "";
    int fmflags = -1;
    int res = -1;
    char *subfontdirectory = NULL;

    if ( c->a.argc!=2 && c->a.argc!=3 && c->a.argc!=4 && c->a.argc!=5 && c->a.argc!=6 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str ||
	    (c->a.argc>=3 && c->a.vals[2].type!=v_str ) ||
	    (c->a.argc>=4 && c->a.vals[3].type!=v_int ) ||
	    (c->a.argc>=5 && c->a.vals[4].type!=v_int ) ||
	    (c->a.argc>=6 && c->a.vals[5].type!=v_str ))
	error( c, "Bad type of argument");
    if ( c->a.argc>=3 )
	bitmaptype = c->a.vals[2].u.sval;
    if ( c->a.argc>=4 )
	fmflags = c->a.vals[3].u.ival;
    if ( c->a.argc>=5 )
	res = c->a.vals[4].u.ival;
    if ( c->a.argc>=6 )
	subfontdirectory = c->a.vals[5].u.sval;
    if ( !GenerateScript(sf,c->a.vals[1].u.sval,bitmaptype,fmflags,res,subfontdirectory,NULL) )
	error(c,"Save failed");
}

static void bGenerateFamily(Context *c) {
    SplineFont *sf = NULL;
    char *bitmaptype = "";
    int fmflags = -1;
    struct sflist *sfs, *cur;
    Array *fonts;
    FontView *fv;
    int i;
    uint16 psstyle;
    SplineFont *styles[48];

    memset(styles,0,sizeof(styles));
    if ( c->a.argc!=5 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str || c->a.vals[2].type!=v_str ||
	    c->a.vals[3].type!=v_int ||
	    c->a.vals[4].type!=v_arr )
	error( c, "Bad type of argument");
    bitmaptype = c->a.vals[2].u.sval;
    fmflags = c->a.vals[3].u.ival;

    fonts = c->a.vals[4].u.aval;
    sfs = NULL;
    for ( i=0; i<fonts->argc; ++i ) {
	if ( fonts->vals[i].type!=v_str )
	    error(c,"Values in the fontname array must be strings");
	for ( fv=fv_list; fv!=NULL; fv=fv->next ) if ( fv->sf!=sf )
	    if ( strcmp(fonts->vals[i].u.sval,fv->sf->origname)==0 ||
		    (fv->sf->filename!=NULL && strcmp(fonts->vals[i].u.sval,fv->sf->origname)==0 ))
	break;
	if ( fv==NULL )
	    for ( fv=fv_list; fv!=NULL; fv=fv->next ) if ( fv->sf!=sf )
		if ( strcmp(fonts->vals[i].u.sval,fv->sf->fontname)==0 )
	    break;
	if ( fv==NULL ) {
	    fprintf( stderr, "%s\n", fonts->vals[i].u.sval );
	    error( c, "The above font is not loaded" );
	}
	if ( sf==NULL )
	    sf = fv->sf;
	if ( fv->sf->encoding_name!=sf->encoding_name ) {
	    fprintf( stderr, "%s and %s\n", fv->sf->filename, sf->filename );
	    error( c, "The above fonts have different encodings" );
	}
	if ( strcmp(fv->sf->familyname,sf->familyname)!=0 )
	    fprintf( stderr, "Warning: %s has a different family name than does %s (GenerateFamily)\n",
		    fv->sf->fontname, sf->fontname );
	MacStyleCode(fv->sf,&psstyle);
	if ( psstyle>=48 ) {
	    fprintf( stderr, "%s(%s)\n", fv->sf->origname, fv->sf->fontname );
	    error( c, "A font can't be both Condensed and Expanded" );
	} else if ( styles[psstyle]==NULL || styles[psstyle]==fv->sf)
	    styles[psstyle] = fv->sf;
	else {
	    fprintf( stderr, "%s(%s) and %s(%s) 0x%x\n",
		    styles[psstyle]->origname, styles[psstyle]->fontname,
		    fv->sf->origname, fv->sf->fontname,
		    psstyle );
	    error( c, "Two array entries given with the same style (in the Mac's postscript style set)" );
	}
    }
    if ( styles[0]==NULL ) {
	if ( MacStyleCode(c->curfv->sf,NULL)==0 && strcmp(c->curfv->sf->familyname,sf->familyname)!=0 )
	    styles[0] = c->curfv->sf;
	if ( styles[0]==NULL )
	    error(c,"At least one of the specified fonts must have a plain style");
    }
    for ( i=47; i>=0; --i ) if ( styles[i]!=NULL ) {
	cur = chunkalloc(sizeof(struct sflist));
	cur->next = sfs;
	sfs = cur;
	cur->sf = styles[i];
    }

    if ( !GenerateScript(sf,c->a.vals[1].u.sval,bitmaptype,fmflags,-1,NULL,sfs) )
	error(c,"Save failed");
}

static void Bitmapper(Context *c,int isavail) {
    int32 *sizes;
    int i;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_arr )
	error( c, "Bad type of argument");
    for ( i=0; i<c->a.vals[1].u.aval->argc; ++i )
	if ( c->a.vals[1].u.aval->vals[i].type!=v_int ||
		c->a.vals[1].u.aval->vals[i].u.ival<=2 )
	    error( c, "Bad type of array component");
    sizes = galloc((c->a.vals[1].u.aval->argc+1)*sizeof(int32));
    for ( i=0; i<c->a.vals[1].u.aval->argc; ++i ) {
	sizes[i] = c->a.vals[1].u.aval->vals[i].u.ival;
	if ( (sizes[i]>>16)==0 )
	    sizes[i] |= 0x10000;
    }
    sizes[i] = 0;
    
    if ( !BitmapControl(c->curfv,sizes,isavail) )
	error(c,"Bitmap operation failed");		/* Storage leak here longjmp avoids free */
    free(sizes);
}

static void bBitmapsAvail(Context *c) {
    Bitmapper(c,true);
}

static void bBitmapsRegen(Context *c) {
    Bitmapper(c,false);
}

static void bImport(Context *c) {
    char *ext, *filename;
    int format, back, ok, isimage;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error( c, "Bad type of argument");
    filename = GFileMakeAbsoluteName(c->a.vals[1].u.sval);
    ext = strrchr(filename,'.');
    if ( ext==NULL ) {
	int len = strlen(filename);
	ext = filename+len-2;
	if ( ext[0]!='p' || ext[1]!='k' )
	    errors( c, "No extension in", filename);
    }
    back = 0;
    if ( strmatch(ext,".bdf")==0 )
	format = 0;
    else if ( strmatch(ext,".pcf")==0 )
	format = 5;
    else if ( strmatch(ext,".ttf")==0 )
	format = 1;
    else if ( strmatch(ext,"pk")==0 || strmatch(ext,".pk")==0 ) {
	format = 2;
	back = 1;
    } else if ( strchr(filename,'*')==NULL )
	format = 3;
    else
	format = 4;
    isimage = true;
    if (( format==3 || format==4 ) &&
	    (strstrmatch(filename,".eps")!=NULL ||
	     strstrmatch(filename,".ps")!=NULL ))
	isimage = false;
    if ( c->a.argc==3 )
	back = c->a.vals[2].u.ival;
    if ( format==0 )
	ok = FVImportBDF(c->curfv,filename,false, back);
    else if ( format==5 )
	ok = FVImportBDF(c->curfv,filename,2, back);
    else if ( format==1 )
	ok = FVImportMult(c->curfv,filename, back, bf_ttf);
    else if ( format==2 )
	ok = FVImportBDF(c->curfv,filename,true, back);
    else if ( format==3 )
	ok = FVImportImages(c->curfv,filename,isimage);
    else
	ok = FVImportImageTemplate(c->curfv,filename,isimage);
    free(filename);
    if ( !ok )
	error(c,"Import failed" );
}

#ifdef PFAEDIT_CONFIG_WRITE_PFM
static void bWritePfm(Context *c) {
    SplineFont *sf = c->curfv->sf;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str )
	error( c, "Bad type of argument");
    if ( !WritePfmFile(c->a.vals[1].u.sval,sf,0) )
	error(c,"Save failed");
}
#endif

static void bExport(Context *c) {
    int format,i ;
    BDFFont *bdf;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error( c, "Bad type of arguments");
    if ( strmatch(c->a.vals[1].u.sval,"eps")==0 )
	format = 0;
    else if ( strmatch(c->a.vals[1].u.sval,"fig")==0 )
	format = 1;
    else if ( strmatch(c->a.vals[1].u.sval,"xbm")==0 )
	format = 2;
    else if ( strmatch(c->a.vals[1].u.sval,"bmp")==0 )
	format = 3;
#ifndef _NO_LIBPNG
    else if ( strmatch(c->a.vals[1].u.sval,"png")==0 )
	format = 4;
    else
	error( c, "Bad format (first arg must be eps/fig/xbm/bmp/png)");
#else
    else
	error( c, "Bad format (first arg must be eps/fig/xbm/bmp)");
#endif
    if (( format>=2 && c->a.argc!=3 ) || (format<2 && c->a.argc==3 ))
	error( c, "Wrong number of arguments");
    bdf=NULL;
    if ( format>=2 ) {
	for ( bdf = c->curfv->sf->bitmaps; bdf!=NULL; bdf=bdf->next )
	    if (( BDFDepth(bdf)==1 && bdf->pixelsize==c->a.vals[2].u.ival) ||
		    (bdf->pixelsize!=(c->a.vals[2].u.ival&0xffff) &&
			    BDFDepth(bdf)==(c->a.vals[2].u.ival>>16)) )
	break;
	if ( bdf==NULL )
	    error(c, "No bitmap font matches the specified size");
    }
    for ( i=0; i<c->curfv->sf->charcnt; ++i )
	if ( SCWorthOutputting(c->curfv->sf->chars[i]) )
	    ScriptExport(c->curfv->sf,bdf,format,i);
}

static void bMergeKern(Context *c) {
    int isafm;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_str )
	error( c, "Bad type of arguments");
    isafm = strstrmatch(c->a.vals[1].u.sval,".afm")!=NULL;
    if ( (isafm && !LoadKerningDataFromAfm(c->curfv->sf,c->a.vals[1].u.sval)) ||
	    (!isafm && !LoadKerningDataFromTfm(c->curfv->sf,c->a.vals[1].u.sval)) )
	error( c, "Failed to find kern info in file" );
}

static void bPrintSetup(Context *c) {

    if ( c->a.argc!=2 && c->a.argc!=3 && c->a.argc!=5 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int )
	error( c, "Bad type for first argument");
    if ( c->a.argc>=3 && c->a.vals[2].type!=v_int )
	error( c, "Bad type for second argument");
    if ( c->a.argc==5 ) {
	if ( c->a.vals[3].type!=v_int )
	    error( c, "Bad type for third argument");
	if ( c->a.vals[4].type!=v_int )
	    error( c, "Bad type for fourth argument");
	pagewidth = c->a.vals[3].u.ival;
	pageheight = c->a.vals[4].u.ival;
    }
    printtype = c->a.vals[1].u.ival;
    if ( c->a.argc>=3 && printtype==4 )
	printcommand = copy(c->a.vals[2].u.sval);
    else if ( c->a.argc>=3 && (printtype==0 || printtype==1) )
	printlazyprinter = copy(c->a.vals[2].u.sval);
}

static void bPrintFont(Context *c) {
    int type, i;
    int32 *pointsizes=NULL;
    char *sample=NULL, *output=NULL;

    if ( c->a.argc!=2 && c->a.argc!=3 && c->a.argc!=4 && c->a.argc!=5 )
	error( c, "Wrong number of arguments");
    type = c->a.vals[1].u.ival;
    if ( c->a.vals[1].type!=v_int || type<0 || type>3 )
	error( c, "Bad type for first argument");
    if ( c->a.argc>=3 ) {
	if ( c->a.vals[2].type==v_int ) {
	    if ( c->a.vals[2].u.ival>0 ) { 
		pointsizes = gcalloc(2,sizeof(int32));
		pointsizes[0] = c->a.vals[2].u.ival;
	    }
	} else if ( c->a.vals[2].type==v_arr ) {
	    Array *a = c->a.vals[2].u.aval;
	    pointsizes = galloc((a->argc+1)*sizeof(int32));
	    for ( i=0; i<a->argc; ++i ) {
		if ( a->vals[i].type!=v_int )
		    error( c, "Bad type for array contents");
		pointsizes[i] = a->vals[i].u.ival;
	    }
	    pointsizes[i] = 0;
	} else
	    error( c, "Bad type for second argument");
    }
    if ( c->a.argc>=4 ) {
	if ( c->a.vals[3].type!=v_str )
	    error( c, "Bad type for third argument");
	else if ( *c->a.vals[3].u.sval!='\0' )
	    sample = c->a.vals[3].u.sval;
    }
    if ( c->a.argc>=5 ) {
	if ( c->a.vals[4].type!=v_str )
	    error( c, "Bad type for fourth argument");
	else if ( *c->a.vals[4].u.sval!='\0' )
	    output = c->a.vals[4].u.sval;
    }
    ScriptPrint(c->curfv,type,pointsizes,sample,output);
    free(pointsizes);
}

/* **** Edit menu **** */
static void doEdit(Context *c, int cmd) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,cmd);
}

static void bCut(Context *c) {
    doEdit(c,0);
}

static void bCopy(Context *c) {
    doEdit(c,1);
}

static void bCopyReference(Context *c) {
    doEdit(c,2);
}

static void bCopyWidth(Context *c) {
    doEdit(c,3);
}

static void bCopyVWidth(Context *c) {
    if ( c->curfv!=NULL && !c->curfv->sf->hasvmetrics )
	error(c,"Vertical metrics not enabled in this font");
    doEdit(c,10);
}

static void bCopyLBearing(Context *c) {
    doEdit(c,11);
}

static void bCopyRBearing(Context *c) {
    doEdit(c,12);
}

static void bPaste(Context *c) {
    doEdit(c,4);
}

static void bPasteInto(Context *c) {
    doEdit(c,9);
}

static void bClear(Context *c) {
    doEdit(c,5);
}

static void bClearBackground(Context *c) {
    doEdit(c,6);
}

static void bCopyFgToBg(Context *c) {
    doEdit(c,7);
}

static void bUnlinkReference(Context *c) {
    doEdit(c,8);
}

static void bJoin(Context *c) {
    doEdit(c,13);
}

static void bSelectAll(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    memset(c->curfv->selected,1,c->curfv->sf->charcnt);
}

static void bSelectNone(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    memset(c->curfv->selected,0,c->curfv->sf->charcnt);
}

static int ParseCharIdent(Context *c, Val *val) {
    SplineFont *sf = c->curfv->sf;
    int bottom = -1;

    if ( val->type==v_int )
	bottom = val->u.ival;
    else if ( val->type==v_unicode || val->type==v_str ) {
	int uni = -1;
	char *str = NULL;
	if ( val->type==v_unicode )
	    uni = val->u.ival;
	else {
	    str = val->u.sval;
	    if ( sscanf(str,"uni%x", &uni)==0 )
		if ( sscanf(str,"u%x", &uni)==0 )
		    sscanf( str, "U+%x", &uni);
	}
	bottom = SFFindChar(sf,uni,str);
    } else
	error( c, "Bad type for argument");
    if ( bottom==-1 )
	error( c, "Character not found" );
    else if ( bottom<0 || bottom>=sf->charcnt )
	error( c, "Character is not in font" );
return( bottom );
}

static void bDoSelect(Context *c) {
    int top, bottom, i,j;

    if ( c->a.argc==2 && (c->a.vals[1].type==v_arr || c->a.vals[1].type==v_arrfree)) {
	struct array *arr = c->a.vals[1].u.aval;
	for ( i=0; i<arr->argc && i<c->curfv->sf->charcnt; ++i ) {
	    if ( arr->vals[i].type!=v_int )
		error(c,"Bad type within selection array");
	    else if ( arr->vals[i].u.ival )
		c->curfv->selected[i] = true;
	}
return;
    }

    for ( i=1; i<c->a.argc; i+=2 ) {
	bottom = ParseCharIdent(c,&c->a.vals[i]);
	if ( i+1==c->a.argc )
	    top = bottom;
	else {
	    top = ParseCharIdent(c,&c->a.vals[i+1]);
	}
	if ( top<bottom ) { j=top; top=bottom; bottom = j; }
	for ( j=bottom; j<=top; ++j )
	    c->curfv->selected[j] = true;
    }
}

static void bSelectMore(Context *c) {
    if ( c->a.argc==1 )
	error( c, "SelectMore needs at least one argument");
    bDoSelect(c);
}

static void bSelect(Context *c) {
    memset(c->curfv->selected,0,c->curfv->sf->charcnt);
    bDoSelect(c);
}

/* **** Element Menu **** */
#define em_unknown (em_none-3)
static void bReencode(Context *c) {
    Encoding *item=NULL;
    int i;
    enum charset new_map = em_unknown;
    FontView *fvs;
    int force = 0;
    static struct encdata {
	int val;
	char *name;
    } encdata[] = {
	{ em_compacted, "compacted" },
	{ em_custom, "custom" },
	{ em_iso8859_1, "iso8859-1" },
	{ em_iso8859_1, "isolatin1" },
	{ em_iso8859_1, "latin1" },
	{ em_iso8859_2, "iso8859-2" },
	{ em_iso8859_2, "latin2" },
	{ em_iso8859_3, "iso8859-3" },
	{ em_iso8859_3, "latin3" },
	{ em_iso8859_4, "iso8859-4" },
	{ em_iso8859_4, "latin4" },
	{ em_iso8859_5, "iso8859-5" },
	{ em_iso8859_5, "isocyrillic" },
	{ em_iso8859_6, "iso8859-6" },
	{ em_iso8859_6, "isoarabic" },
	{ em_iso8859_7, "iso8859-7" },
	{ em_iso8859_7, "isogreek" },
	{ em_iso8859_8, "iso8859-8" },
	{ em_iso8859_8, "isohebrew" },
	{ em_iso8859_9, "iso8859-9" },
	{ em_iso8859_9, "latin5" },
	{ em_iso8859_10, "iso8859-10" },
	{ em_iso8859_10, "latin6" },
	{ em_iso8859_13, "iso8859-13" },
	{ em_iso8859_13, "latin7" },
	{ em_iso8859_14, "iso8859-14" },
	{ em_iso8859_14, "latin8" },
	{ em_iso8859_15, "iso8859-15" },
	{ em_iso8859_15, "latin0" },
	{ em_iso8859_15, "latin9" },
	{ em_koi8_r, "koi8-r" },
	{ em_koi8_r, "koi8r" },
	{ em_jis201, "jis201" },
	{ em_adobestandard, "AdobeStandardEncoding" },
	{ em_adobestandard, "Adobe" },
	{ em_win, "win" },
	{ em_mac, "mac" },
	{ em_ksc5601, "ksc5601" },
	{ em_wansung, "wansung" },
	{ em_big5, "big5" },
	{ em_big5hkscs, "big5hkscs" },
	{ em_johab, "johab" },
	{ em_jis208, "jis208" },
	{ em_sjis, "sjis" },
	{ em_unicode, "unicode" },
	{ em_unicode4, "unicode4" },
	{ em_unicode, "iso10646" },
	{ em_unicode, "iso10646-1" },
	{ em_unicode4, "ucs4" },
	{ 0, NULL}};
    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str || ( c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type");
    if ( c->a.argc==3 )
	force = c->a.vals[2].u.ival;
    for ( i=0; encdata[i].name!=NULL; ++i )
	if ( strmatch(encdata[i].name,c->a.vals[1].u.sval)==0 ) {
	    new_map = encdata[i].val;
    break;
	}
    if ( new_map == em_unknown ) {
	for ( item=enclist; item!=NULL ; item=item->next )
	    if ( strmatch(c->a.vals[1].u.sval,item->enc_name )==0 ) {
		new_map = item->enc_num;
	break;
	    }
    }
    if ( new_map==em_unknown && strstart(c->a.vals[1].u.sval,"unicode-plane-")!=NULL ) {
	int plane=0;
	sscanf( c->a.vals[1].u.sval,"unicode-plane-%x", &plane );
	if ( plane==0 ) new_map = em_unicode;
	else new_map = em_unicodeplanes+plane;
    }
    if ( new_map==em_unknown )
	errors(c,"Unknown encoding", c->a.vals[1].u.sval);
    if ( force )
	SFForceEncoding(c->curfv->sf,new_map);
    else
	SFReencodeFont(c->curfv->sf,new_map);
    for ( fvs=c->curfv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	free( fvs->selected );
	fvs->selected = gcalloc(fvs->sf->charcnt,1);
    }
    if ( screen_display!=NULL )
	FontViewReformatAll(c->curfv->sf);
    c->curfv->sf->changed = true;
    c->curfv->sf->changed_since_autosave = true;
    c->curfv->sf->changed_since_xuidchanged = true;
}

static void bSetCharCnt(Context *c) {
    int oldcnt = c->curfv->sf->charcnt, i;
    FontView *fvs;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_int )
	error(c,"Bad argument type");
    else if ( c->a.vals[1].u.ival<=0 && c->a.vals[1].u.ival>10*65536 )
	error(c,"Argument out of bounds");

    if ( c->curfv->sf->charcnt==c->a.vals[1].u.ival )
return;

    SFAddDelChars(c->curfv->sf,c->a.vals[1].u.ival);
    for ( fvs=c->curfv->sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	fvs->selected = grealloc(fvs->selected,c->a.vals[1].u.ival);
	for ( i=oldcnt; i<c->a.vals[1].u.ival; ++i )
	    fvs->selected[i] = false;
    }
    if ( screen_display!=NULL )
	FontViewReformatAll(c->curfv->sf);
    c->curfv->sf->changed = true;
    c->curfv->sf->changed_since_autosave = true;
    c->curfv->sf->changed_since_xuidchanged = true;
}

static void _SetFontNames(Context *c,SplineFont *sf) {
    int i;

    if ( c->a.argc==1 || c->a.argc>6 )
	error( c, "Wrong number of arguments");
    for ( i=1; i<c->a.argc; ++i )
	if ( c->a.vals[i].type!=v_str )
	    error(c,"Bad argument type");
    if ( *c->a.vals[1].u.sval!='\0' ) {
	free(sf->fontname);
	sf->fontname = copy(c->a.vals[1].u.sval);
    }
    if ( c->a.argc>2 && *c->a.vals[2].u.sval!='\0' ) {
	free(sf->familyname);
	sf->familyname = copy(c->a.vals[2].u.sval);
    }
    if ( c->a.argc>3 && *c->a.vals[3].u.sval!='\0' ) {
	free(sf->fullname);
	sf->fullname = copy(c->a.vals[3].u.sval);
    }
    if ( c->a.argc>4 && *c->a.vals[4].u.sval!='\0' ) {
	free(sf->weight);
	sf->weight = copy(c->a.vals[4].u.sval);
    }
    if ( c->a.argc>5 && *c->a.vals[5].u.sval!='\0' ) {
	free(sf->copyright);
	sf->copyright = copy(c->a.vals[5].u.sval);
    }
}

static void bSetFontNames(Context *c) {
    SplineFont *sf = c->curfv->sf;
    _SetFontNames(c,sf);
}

static void bSetItalicAngle(Context *c) {
    int denom=1;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.argc==3 ) {
	if ( c->a.vals[2].type!=v_int )
	    error(c,"Bad argument type");
	denom=c->a.vals[2].u.ival;
    }
    if ( c->a.vals[1].type!=v_int )
	error(c,"Bad argument type");
    c->curfv->sf->italicangle = c->a.vals[1].u.ival/ (double) denom;
}
 
static void bSetUniqueID(Context *c) {

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int )
	error(c,"Bad argument type");
    c->curfv->sf->uniqueid = c->a.vals[1].u.ival;
}

static SplineChar *GetOneSelChar(Context *c) {
    SplineFont *sf = c->curfv->sf;
    int i, found = -1;

    for ( i=0; i<sf->charcnt; ++i ) if ( c->curfv->selected[i] ) {
	if ( found==-1 )
	    found = i;
	else
	    error(c,"More than one character selected" );
    }
    if ( found==-1 )
	error(c,"No characters selected" );
return( SFMakeChar(sf,found));
}

static void bSetCharName(Context *c) {
    SplineChar *sc;
    char *name, *end;
    int uni;
    unichar_t *comment;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type");
    sc = GetOneSelChar(c);
    uni = sc->unicodeenc;
    name = c->a.vals[1].u.sval;
    comment = u_copy(sc->comment);

    if ( c->a.argc!=3 || c->a.vals[2].u.ival==-1 ) {
	if ( name[0]=='u' && name[1]=='n' && name[2]=='i' && strlen(name)==7 &&
		(uni = strtol(name+3,&end,16), *end=='\0'))
	    /* Good */;
	else if ( name[0]=='u' && strlen(name)<=7 &&
		(uni = strtol(name+1,&end,16), *end=='\0'))
	    /* Good */;
	else {
	    for ( uni=psunicodenames_cnt-1; uni>=0; --uni )
		if ( psunicodenames[uni]!=NULL && strcmp(name,psunicodenames[uni])==0 )
	    break;
	}
    }
    SCSetMetaData(sc,name,uni,comment);
    SCLigDefault(sc);
}

static void bSetUnicodeValue(Context *c) {
    SplineChar *sc;
    char *name;
    int uni;
    unichar_t *comment;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( (c->a.vals[1].type!=v_int && c->a.vals[1].type!=v_unicode) ||
	    (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type");
    sc = GetOneSelChar(c);
    uni = c->a.vals[1].u.ival;
    name = copy(sc->name);
    comment = u_copy(sc->comment);

    if ( c->a.argc!=3 || c->a.vals[2].u.ival ) {
	free(name);
	if ( uni>=0 && uni<psunicodenames_cnt && psunicodenames[uni]!=NULL )
	    name = copy(psunicodenames[uni]);
	else if (( uni>=32 && uni<0x7f ) || uni>=0xa1 ) {
	    char buf[12];
	    if ( uni<0x10000 )
		sprintf( buf,"uni%04X", uni );
	    else
		sprintf( buf,"u%04X", uni );
	    name = copy(buf);
	} else
	    name = copy(".notdef");
    }
    SCSetMetaData(sc,name,uni,comment);
    SCLigDefault(sc);
}

static void bSetCharColor(Context *c) {
    SplineFont *sf = c->curfv->sf;
    SplineChar *sc;
    int i;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_int )
	error(c,"Bad argument type");
    for ( i=0; i<sf->charcnt; ++i ) if ( c->curfv->selected[i] ) {
	sc = SFMakeChar(sf,i);
	sc->color = c->a.vals[1].u.ival;
    }
    c->curfv->sf->changed = true;
}

static void bSetCharComment(Context *c) {
    SplineChar *sc;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str )
	error(c,"Bad argument type");
    sc = GetOneSelChar(c);
    sc->comment = *c->a.vals[1].u.sval=='\0'?NULL:def2u_copy(c->a.vals[1].u.sval);
    c->curfv->sf->changed = true;
}

static void bTransform(Context *c) {
    real trans[6];
    BVTFunc bvts[1];

    if ( c->a.argc!=7 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_int || c->a.vals[2].type!=v_int ||
	      c->a.vals[3].type!=v_int || c->a.vals[4].type!=v_int ||
	      c->a.vals[5].type!=v_int || c->a.vals[6].type!=v_int )
	error(c,"Bad argument type in Transform");
    trans[0] = c->a.vals[1].u.ival/100.;
    trans[1] = c->a.vals[2].u.ival/100.;
    trans[2] = c->a.vals[3].u.ival/100.;
    trans[3] = c->a.vals[4].u.ival/100.;
    trans[4] = c->a.vals[5].u.ival/100.;
    trans[5] = c->a.vals[6].u.ival/100.;
    bvts[0].func = bvt_none;
    FVTransFunc(c->curfv,trans,0,bvts,true);
}

static void bHFlip(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];

    trans[0] = -1; trans[3] = 1;
    trans[1] = trans[2] = trans[4] = trans[5] = 0;
    if ( c->a.argc==1 )
	/* default to center of char for origin */;
    else if ( c->a.argc==2 || c->a.argc==3 ) {
	if ( c->a.vals[1].type!=v_int || ( c->a.argc==3 && c->a.vals[2].type!=v_int ))
	    error(c,"Bad argument type in HFlip");
	trans[4] = 2*c->a.vals[1].u.ival;
	otype = 0;
    } else
	error( c, "Wrong number of arguments");
    bvts[0].func = bvt_fliph;
    bvts[1].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bVFlip(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];

    trans[0] = 1; trans[3] = -1;
    trans[1] = trans[2] = trans[4] = trans[5] = 0;
    if ( c->a.argc==1 )
	/* default to center of char for origin */;
    else if ( c->a.argc==2 || c->a.argc==3 ) {
	if ( c->a.vals[1].type!=v_int || ( c->a.argc==3 && c->a.vals[2].type!=v_int ))
	    error(c,"Bad argument type in VFlip");
	if ( c->a.argc==2 )
	    trans[5] = 2*c->a.vals[1].u.ival;
	else
	    trans[5] = 2*c->a.vals[2].u.ival;
	otype = 0;
    } else
	error( c, "Wrong number of arguments");
    bvts[0].func = bvt_flipv;
    bvts[1].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bRotate(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];
    double a;

    if ( c->a.argc==1 || c->a.argc==3 || c->a.argc>4 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==4 &&
	    (c->a.vals[2].type!=v_int || c->a.vals[3].type!=v_int )))
	error(c,"Bad argument type in Rotate");
    if ( (c->a.vals[1].u.ival %= 360)<0 ) c->a.vals[1].u.ival += 360;
    a = c->a.vals[1].u.ival *3.1415926535897932/180.;
    trans[0] = trans[3] = cos(a);
    trans[2] = -(trans[1] = sin(a));
    trans[4] = trans[5] = 0;
    if ( c->a.argc==4 ) {
	trans[4] = c->a.vals[2].u.ival-(trans[0]*c->a.vals[2].u.ival+trans[2]*c->a.vals[3].u.ival);
	trans[5] = c->a.vals[3].u.ival-(trans[1]*c->a.vals[2].u.ival+trans[3]*c->a.vals[3].u.ival);
	otype = 0;
    }
    bvts[0].func = bvt_none;
    if ( c->a.vals[1].u.ival==90 )
	bvts[0].func = bvt_rotate90ccw;
    else if ( c->a.vals[1].u.ival==180 )
	bvts[0].func = bvt_rotate180;
    else if ( c->a.vals[1].u.ival==270 )
	bvts[0].func = bvt_rotate90cw;
    bvts[1].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bScale(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];
    double xfact, yfact;
    int i;
    /* Arguments:
	1 => same scale factor both directions, origin at center
	2 => different scale factors for each direction, origin at center
	    (scale factors are per cents)
	3 => same scale factor both directions, origin last two args
	4 => different scale factors for each direction, origin last two args
    */

    if ( c->a.argc==1 || c->a.argc>5 )
	error( c, "Wrong number of arguments");
    for ( i=1; i<c->a.argc; ++i )
	if ( c->a.vals[i].type!=v_int )
	    error(c,"Bad argument type in Scale");
    i = 1;
    if ( c->a.argc&1 ) {
	xfact = c->a.vals[1].u.ival/100.;
	yfact = c->a.vals[2].u.ival/100.;
	i = 3;
    } else {
	xfact = yfact = c->a.vals[1].u.ival/100.;
	i = 2;
    }
    trans[0] = xfact; trans[3] = yfact;
    trans[2] = trans[1] = 0;
    trans[4] = trans[5] = 0;
    if ( c->a.argc>i ) {
	trans[4] = c->a.vals[i].u.ival-(trans[0]*c->a.vals[i].u.ival);
	trans[5] = c->a.vals[i+1].u.ival-(trans[3]*c->a.vals[i+1].u.ival);
	otype = 0;
    }
    bvts[0].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bSkew(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];
    double a;

    if ( c->a.argc==1 || c->a.argc==3 || c->a.argc>4 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==4 &&
	    (c->a.vals[2].type!=v_int || c->a.vals[3].type!=v_int )))
	error(c,"Bad argument type in Skew");
    if ( (c->a.vals[1].u.ival %= 360)<0 ) c->a.vals[1].u.ival += 360;
    a = c->a.vals[1].u.ival *3.1415926535897932/180.;
    trans[0] = trans[3] = 1;
    trans[1] = 0; trans[2] = tan(a);
    trans[4] = trans[5] = 0;
    if ( c->a.argc==4 ) {
	trans[4] = c->a.vals[2].u.ival-(trans[0]*c->a.vals[2].u.ival+trans[2]*c->a.vals[3].u.ival);
	trans[5] = c->a.vals[3].u.ival-(trans[1]*c->a.vals[2].u.ival+trans[3]*c->a.vals[3].u.ival);
	otype = 0;
    }
    skewselect(&bvts[0],trans[2]);
    bvts[1].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bMove(Context *c) {
    real trans[6];
    int otype = 1;
    BVTFunc bvts[2];

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || c->a.vals[2].type!=v_int )
	error(c,"Bad argument type");
    trans[0] = trans[3] = 1;
    trans[1] = trans[2] = 0;
    trans[4] = c->a.vals[1].u.ival; trans[5] = c->a.vals[2].u.ival;
    bvts[0].func = bvt_transmove;
    bvts[0].x = trans[4]; bvts[0].y = trans[5];
    bvts[1].func = bvt_none;
    FVTransFunc(c->curfv,trans,otype,bvts,true);
}

static void bScaleToEm(Context *c) {
    int i;

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    for ( i=1; i<c->a.argc; ++i )
	if ( c->a.vals[i].type!=v_int || c->a.vals[i].u.ival<0 || c->a.vals[i].u.ival>16384 )
	    error(c,"Bad argument type");
    SFScaleToEm(c->curfv->sf,c->a.vals[1].u.ival,c->a.vals[2].u.ival);
}

static void bExpandStroke(Context *c) {
    StrokeInfo si;
    /* Arguments:
	2 => stroke width (implied butt, round)
	4 => stroke width, line cap, line join
	5 => stroke width, caligraphic angle, thickness-numerator, thickness-denom
    */

    if ( c->a.argc!=2 && c->a.argc!=4 && c->a.argc!=7 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int ||
	    (c->a.argc>=4 && c->a.vals[2].type!=v_int ) ||
	    (c->a.argc>=4 && c->a.vals[3].type!=v_int ) ||
	    (c->a.argc>=5 && c->a.vals[4].type!=v_int ))
	error(c,"Bad argument type in ExpandStroke");
    memset(&si,0,sizeof(si));
    si.radius = c->a.vals[1].u.ival/2.;
    if ( c->a.argc==2 ) {
	si.join = lj_round;
	si.cap = lc_butt;
    } else if ( c->a.argc==4 ) {
	si.cap = c->a.vals[2].u.ival;
	si.join = c->a.vals[3].u.ival;
    } else {
	si.caligraphic = true;
	si.penangle = 3.1415926535897932*c->a.vals[2].u.ival/180;
	si.ratio = c->a.vals[3].u.ival / (double) c->a.vals[4].u.ival;
    }
    FVStrokeItScript(c->curfv, &si);
}

static void bRemoveOverlap(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,100);
}

static void bSimplify(Context *c) {
    double err = .75;
    int type = 0;

    if ( c->a.argc==3 ) {
	if ( c->a.vals[1].type!=v_int || c->a.vals[2].type!=v_int )
	    error( c, "Bad type for argument" );
	type = c->a.vals[1].u.ival;
	err = c->a.vals[2].u.ival;
    } else if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    _FVSimplify(c->curfv,type,err);
}

static void bAddExtrema(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,102);
}

static void bRoundToInt(Context *c) {
    double err = .75;
    int flags = 0;

    if ( c->a.argc==3 ) {
    } else if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    _FVSimplify(c->curfv,flags,err);
}

static void bAutotrace(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVAutoTrace(c->curfv,false);
}

static void bCorrectDirection(Context *c) {
    int i;
    SplineFont *sf = c->curfv->sf;
    int changed, refchanged;
    int checkrefs = true;
    RefChar *ref;
    SplineChar *sc;

    if ( c->a.argc!=1 && c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    else if ( c->a.argc==2 && c->a.vals[1].type!=v_int )
	error(c,"Bad argument type");
    else
	checkrefs = c->a.vals[1].u.ival;
    for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && c->curfv->selected[i] ) {
	sc = sf->chars[i];
	changed = refchanged = false;
	if ( checkrefs ) {
	    for ( ref=sc->refs; ref!=NULL; ref=ref->next ) {
		if ( ref->transform[0]*ref->transform[3]<0 ||
			(ref->transform[0]==0 && ref->transform[1]*ref->transform[2]>0)) {
		    if ( !refchanged ) {
			refchanged = true;
			SCPreserveState(sc,false);
		    }
		    SCRefToSplines(sc,ref);
		}
	    }
	}
	if ( !refchanged )
	    SCPreserveState(sc,false);
	sc->splines = SplineSetsCorrect(sc->splines,&changed);
	if ( changed || refchanged )
	    SCCharChangedUpdate(sc);
    }
}

static void bBuildComposit(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVBuildAccent(c->curfv,false);
}

static void bBuildAccented(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVBuildAccent(c->curfv,true);
}

static void bMergeFonts(Context *c) {
    SplineFont *sf;
    int openflags=0;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str )
	error( c, "MergeFonts expects a filename" );
    else if ( c->a.argc==3 ) {
	if ( c->a.vals[2].type!=v_int )
	    error( c, "MergeFonts expects an integer for second argument" );
	openflags = c->a.vals[2].u.ival;
    }
    sf = LoadSplineFont(c->a.vals[1].u.sval,openflags);
    if ( sf==NULL )
	errors(c,"Can't find font", c->a.vals[1].u.sval);
    MergeFont(c->curfv,sf);
}

static void bAutoHint(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,200);
}

static void bAutoInstr(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,202);
}

static void bClearHints(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVFakeMenus(c->curfv,201);
}

static void bSetWidth(Context *c) {
    int incr = 0;
    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type in SetWidth");
    if ( c->a.argc==3 )
	incr = c->a.vals[2].u.ival;
    FVSetWidthScript(c->curfv,wt_width,c->a.vals[1].u.ival,incr);
}

static void bSetVWidth(Context *c) {
    int incr = 0;
    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type in SetVWidth");
    if ( c->a.argc==3 )
	incr = c->a.vals[2].u.ival;
    FVSetWidthScript(c->curfv,wt_vwidth,c->a.vals[1].u.ival,incr);
}

static void bSetLBearing(Context *c) {
    int incr = 0;
    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type in SetLBearing");
    if ( c->a.argc==3 )
	incr = c->a.vals[2].u.ival;
    FVSetWidthScript(c->curfv,wt_lbearing,c->a.vals[1].u.ival,incr);
}

static void bSetRBearing(Context *c) {
    int incr = 0;
    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || (c->a.argc==3 && c->a.vals[2].type!=v_int ))
	error(c,"Bad argument type in SetRBearing");
    if ( c->a.argc==3 )
	incr = c->a.vals[2].u.ival;
    FVSetWidthScript(c->curfv,wt_rbearing,c->a.vals[1].u.ival,incr);
}

static void bAutoWidth(Context *c) {
    SplineFont *sf = c->curfv->sf;

    if ( c->a.argc != 2 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int )
	error(c,"Bad argument type in AutoWidth");
    if ( !AutoWidthScript(sf,c->a.vals[1].u.ival))
	error(c,"No characters selected.");
}

static void bAutoKern(Context *c) {
    SplineFont *sf = c->curfv->sf;

    if ( c->a.argc != 3 && c->a.argc != 4 )
	error( c, "Wrong number of arguments");
    if ( c->a.vals[1].type!=v_int || c->a.vals[2].type!=v_int ||
	    (c->a.argc==4 && c->a.vals[3].type!=v_str))
	error(c,"Bad argument type");
    if ( !AutoKernScript(sf,c->a.vals[1].u.ival,c->a.vals[2].u.ival,
	    c->a.argc==4?c->a.vals[3].u.sval:NULL) )
	error(c,"No characters selected.");
}

static void bCenterInWidth(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    FVMetricsCenter(c->curfv,true);
}

static void bSetKern(Context *c) {
    SplineFont *sf = c->curfv->sf;
    SplineChar *sc1, *sc2;
    int i, kern, ch2;
    KernPair *kp;

    if ( c->a.argc!=3 )
	error( c, "Wrong number of arguments" );
    ch2 = ParseCharIdent(c,&c->a.vals[1]);
    if ( c->a.vals[2].type!=v_int )
	error(c,"Bad argument type");
    kern = c->a.vals[2].u.ival;
    if ( kern!=0 )
	sc2 = SFMakeChar(sf,ch2);
    else {
	sc2 = sf->chars[ch2];
	if ( sc2==NULL )
return;		/* It already has a kern==0 with everything */
    }

    for ( i=0; i<sf->charcnt; ++i ) if ( c->curfv->selected[i] ) {
	if ( kern!=0 )
	    sc1 = SFMakeChar(sf,i);
	else {
	    if ( (sc1 = sf->chars[i])==NULL )
    continue;
	}
	for ( kp = sc1->kerns; kp!=NULL && kp->sc!=sc2; kp = kp->next );
	if ( kp==NULL && kern==0 )
    continue;
	else if ( kp!=NULL )
	    kp->off = kern;
	else {
	    kp = chunkalloc(sizeof(KernPair));
	    kp->next = sc1->kerns;
	    sc1->kerns = kp;
	    kp->sc = sc2;
	    kp->off = kern;
	}
    }
}

static void bClearAllKerns(Context *c) {

    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments" );
    FVRemoveKerns(c->curfv);
}

/* **** CID menu **** */

static void bCIDChangeSubFont(Context *c) {
    SplineFont *sf = c->curfv->sf, *new;
    int i;

    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments" );
    if ( c->a.vals[1].type!=v_str )
	error( c, "Bad argument type" );
    if ( sf->cidmaster==NULL )
	errors( c, "Not a cid-keyed font", sf->fontname );
    for ( i=0; i<sf->cidmaster->subfontcnt; ++i )
	if ( strcmp(sf->cidmaster->subfonts[i]->fontname,c->a.vals[1].u.sval)==0 )
    break;
    if ( i==sf->cidmaster->subfontcnt )
	errors( c, "Not in the current cid font", c->a.vals[1].u.sval );
    new = sf->cidmaster->subfonts[i];

    if ( screen_display!=NULL ) {
	MetricsView *mv, *mvnext;
	for ( mv=c->curfv->metrics; mv!=NULL; mv = mvnext ) {
	    /* Don't bother trying to fix up metrics views, just not worth it */
	    mvnext = mv->next;
	    GDrawDestroyWindow(mv->gw);
	}
	GDrawSync(NULL);
	GDrawProcessPendingEvents(NULL);
    }
    if ( sf->charcnt!=new->charcnt ) {
	FontView *fvs;
	for ( fvs=sf->fv; fvs!=NULL; fvs=fvs->nextsame ) {
	    free(fvs->selected);
	    fvs->selected = gcalloc(new->charcnt,sizeof(char));
	}
    }
    c->curfv->sf = new;
    if ( screen_display!=NULL ) {
	FVSetTitle(c->curfv);
	FontViewReformatAll(c->curfv->sf);
    }
}

static void bCIDSetFontNames(Context *c) {
    SplineFont *sf = c->curfv->sf;
    
    if ( sf->cidmaster==NULL )
	errors( c, "Not a cid-keyed font", sf->fontname );
    _SetFontNames(c,sf->cidmaster);
}

static void bCIDFlattenByCMap(Context *c) {
    SplineFont *sf = c->curfv->sf;
    
    if ( sf->cidmaster==NULL )
	errors( c, "Not a cid-keyed font", sf->fontname );
    else if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Argument must be a filename");
    
    SFFlattenByCMap(sf,c->a.vals[1].u.sval);
}

/* **** Info routines **** */

static void bCharCnt(Context *c) {
    if ( c->a.argc!=1 )
	error( c, "Wrong number of arguments");
    c->return_val.type = v_int;
    c->return_val.u.ival = c->curfv->sf->charcnt;
}

static void bInFont(Context *c) {
    SplineFont *sf = c->curfv->sf;
    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    c->return_val.type = v_int;
    if ( c->a.vals[1].type==v_int )
	c->return_val.u.ival = c->a.vals[1].u.ival>=0 && c->a.vals[1].u.ival<sf->charcnt;
    else if ( c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_str ) {
	int enc, uni = -1;
	char *str = NULL;
	if ( c->a.vals[1].type==v_unicode )
	    uni = c->a.vals[1].u.ival;
	else {
	    str = c->a.vals[1].u.sval;
	    if ( sscanf(str,"uni%x", &uni)==0 )
		if ( sscanf(str,"u%x", &uni)==0 )
		    sscanf( str, "U+%x", &uni);
	}
	enc = SFFindChar(sf,uni,str);
	c->return_val.u.ival = (enc!=-1);
    } else
	error( c, "Bad type of argument to InFont");
}

static void bWorthOutputting(Context *c) {
    SplineFont *sf = c->curfv->sf;
    if ( c->a.argc!=2 )
	error( c, "Wrong number of arguments");
    c->return_val.type = v_int;
    if ( c->a.vals[1].type==v_int )
	c->return_val.u.ival = c->a.vals[1].u.ival>=0 &&
		c->a.vals[1].u.ival<sf->charcnt &&
		SCWorthOutputting(sf->chars[c->a.vals[1].u.ival]);
    else if ( c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_str ) {
	int enc, uni = -1;
	char *str = NULL;
	if ( c->a.vals[1].type==v_unicode )
	    uni = c->a.vals[1].u.ival;
	else {
	    str = c->a.vals[1].u.sval;
	    if ( sscanf(str,"uni%x", &uni)==0 )
		if ( sscanf(str,"u%x", &uni)==0 )
		    sscanf( str, "U+%x", &uni);
	}
	enc = SFFindChar(sf,uni,str);
	c->return_val.u.ival = enc!=-1 && SCWorthOutputting(sf->chars[enc]);
    } else
	error( c, "Bad type of argument to InFont");
}

static void bCharInfo(Context *c) {
    SplineFont *sf = c->curfv->sf;
    SplineChar *sc;
    DBounds b;

    if ( c->a.argc!=2 && c->a.argc!=3 )
	error( c, "Wrong number of arguments");
    else if ( c->a.vals[1].type!=v_str )
	error( c, "Bad type for argument");

    sc = GetOneSelChar(c);

    c->return_val.type = v_int;
    if ( c->a.argc==3 ) {
	int ch2 = ParseCharIdent(c,&c->a.vals[2]);
	if ( strmatch( c->a.vals[1].u.sval,"Kern")==0 ) {
	    c->return_val.u.ival = 0;
	    if ( sf->chars[ch2]!=NULL ) { KernPair *kp;
		for ( kp = sc->kerns; kp!=NULL && kp->sc!=sf->chars[ch2]; kp=kp->next );
		if ( kp!=NULL )
		    c->return_val.u.ival = kp->off;
	    }
	} else
	    errors(c,"Unknown tag", c->a.vals[1].u.sval);
    } else {
	if ( strmatch( c->a.vals[1].u.sval,"Name")==0 ) {
	    c->return_val.type = v_str;
	    c->return_val.u.sval = copy(sc->name);
	} else if ( strmatch( c->a.vals[1].u.sval,"Unicode")==0 )
	    c->return_val.u.ival = sc->unicodeenc;
	else if ( strmatch( c->a.vals[1].u.sval,"Encoding")==0 )
	    c->return_val.u.ival = sc->enc;
	else if ( strmatch( c->a.vals[1].u.sval,"Width")==0 )
	    c->return_val.u.ival = sc->width;
	else if ( strmatch( c->a.vals[1].u.sval,"VWidth")==0 )
	    c->return_val.u.ival = sc->vwidth;
	else if ( strmatch( c->a.vals[1].u.sval,"Changed")==0 )
	    c->return_val.u.ival = sc->changed;
	else if ( strmatch( c->a.vals[1].u.sval,"Color")==0 )
	    c->return_val.u.ival = sc->color;
	else if ( strmatch( c->a.vals[1].u.sval,"Comment")==0 ) {
	    c->return_val.type = v_str;
	    c->return_val.u.sval = sc->comment?u2def_copy(sc->comment):copy("");
	} else {
	    SplineCharFindBounds(sc,&b);
	    if ( strmatch( c->a.vals[1].u.sval,"LBearing")==0 )
		c->return_val.u.ival = b.minx;
	    else if ( strmatch( c->a.vals[1].u.sval,"RBearing")==0 )
		c->return_val.u.ival = sc->width-b.maxx;
	    else
		errors(c,"Unknown tag", c->a.vals[1].u.sval);
	}
    }
}
    
struct builtins { char *name; void (*func)(Context *); int nofontok; } builtins[] = {
/* Generic utilities */
    { "Print", bPrint, 1 },
    { "Error", bError, 1 },
    { "AskUser", bAskUser, 1 },
    { "Array", bArray, 1 },
    { "Strsub", bStrsub, 1 },
    { "Strlen", bStrlen, 1 },
    { "Strstr", bStrstr, 1 },
    { "Strrstr", bStrrstr, 1 },
    { "Strcasestr", bStrcasestr, 1 },
    { "Strcasecmp", bStrcasecmp, 1 },
    { "Strtol", bStrtol, 1 },
    { "Strskipint", bStrskipint, 1 },
    { "GetPref", bGetPrefs, 1 },
    { "SetPref", bSetPrefs, 1 },
/* File menu */
    { "Quit", bQuit, 1 },
    { "Open", bOpen, 1 },
    { "New", bNew, 1 },
    { "Close", bClose },
    { "Save", bSave },
    { "Generate", bGenerate },
    { "GenerateFamily", bGenerateFamily },
#ifdef PFAEDIT_CONFIG_WRITE_PFM
    { "WritePfm", bWritePfm },
#endif
    { "Import", bImport },
    { "Export", bExport },
    { "MergeKern", bMergeKern },
    { "PrintSetup", bPrintSetup },
    { "PrintFont", bPrintFont },
/* Edit Menu */
    { "Cut", bCut },
    { "Copy", bCopy },
    { "CopyReference", bCopyReference },
    { "CopyWidth", bCopyWidth },
    { "CopyVWidth", bCopyVWidth },
    { "CopyLBearing", bCopyLBearing },
    { "CopyRBearing", bCopyRBearing },
    { "Paste", bPaste },
    { "PasteInto", bPasteInto },
    { "Clear", bClear },
    { "ClearBackground", bClearBackground },
    { "CopyFgToBg", bCopyFgToBg },
    { "UnlinkReference", bUnlinkReference },
    { "Join", bJoin },
    { "SelectAll", bSelectAll },
    { "SelectNone", bSelectNone },
    { "SelectMore", bSelectMore },
    { "Select", bSelect },
/* Element Menu */
    { "Reencode", bReencode },
    { "SetCharCnt", bSetCharCnt },
    { "SetFontNames", bSetFontNames },
    { "SetItalicAngle", bSetItalicAngle },
    { "SetUniqueID", bSetUniqueID },
    { "SetCharName", bSetCharName },
    { "SetUnicodeValue", bSetUnicodeValue },
    { "SetCharColor", bSetCharColor },
    { "SetCharComment", bSetCharComment },
    { "BitmapsAvail", bBitmapsAvail },
    { "BitmapsRegen", bBitmapsRegen },
    { "Transform", bTransform },
    { "HFlip", bHFlip },
    { "VFlip", bVFlip },
    { "Rotate", bRotate },
    { "Scale", bScale },
    { "Skew", bSkew },
    { "Move", bMove },
    { "ScaleToEm", bScaleToEm },
    { "ExpandStroke", bExpandStroke },
    { "RemoveOverlap", bRemoveOverlap },
    { "Simplify", bSimplify },
    { "AddExtrema", bAddExtrema },
    { "RoundToInt", bRoundToInt },
    { "Autotrace", bAutotrace },
    { "CorrectDirection", bCorrectDirection },
    { "BuildComposit", bBuildComposit },
    { "BuildAccented", bBuildAccented },
    { "MergeFonts", bMergeFonts },
/*  Menu */
    { "AutoHint", bAutoHint },
    { "AutoInstr", bAutoInstr },
    { "ClearHints", bClearHints },
    { "SetWidth", bSetWidth },
    { "SetVWidth", bSetVWidth },
    { "SetLBearing", bSetLBearing },
    { "SetRBearing", bSetRBearing },
    { "CenterInWidth", bCenterInWidth },
    { "AutoWidth", bAutoWidth },
    { "AutoKern", bAutoKern },
    { "SetKern", bSetKern },
    { "RemoveAllKerns", bClearAllKerns },
/* CID Menu */
    { "CIDChangeSubFont", bCIDChangeSubFont },
    { "CIDSetFontNames", bCIDSetFontNames },
    { "CIDFlattenByCMap", bCIDFlattenByCMap },
/* ***** */
    { "CharCnt", bCharCnt },
    { "InFont", bInFont },
    { "WorthOutputting", bWorthOutputting },
    { "CharInfo", bCharInfo },
    { NULL }
};

/* ******************************* Interpreter ****************************** */

static void expr(Context*,Val *val);
static void statement(Context*);

static int cgetc(Context *c) {
    int ch;
    if ( c->ungotch ) {
	ch = c->ungotch;
	c->ungotch = 0;
return( ch );
    }
    ch = getc(c->script);
    if ( verbose>0 )
	putchar(ch);
    if ( ch=='\r' ) {
	int nch = getc(c->script);
	if ( nch!='\n' )
	    ungetc(nch,c->script);
	else if ( verbose>0 )
	    putchar('\n');
	++c->lineno;
    } else if ( ch=='\n' )
	++c->lineno;
return( ch );
}

static void cungetc(int ch,Context *c) {
    if ( c->ungotch )
	fprintf( stderr, "Internal error: Attempt to unget two characters\n" );
    c->ungotch = ch;
}

static long ctell(Context *c) {
    long pos = ftell(c->script);
    if ( c->ungotch )
	--pos;
return( pos );
}

static void cseek(Context *c,long pos) {
    fseek(c->script,pos,SEEK_SET);
    c->ungotch = 0;
    c->backedup = false;
}

static enum token_type NextToken(Context *c) {
    int ch;
    enum token_type tok = tt_error;

    if ( c->backedup ) {
	c->backedup = false;
return( c->tok );
    }
    do {
	ch = cgetc(c);
	if ( isalpha(ch) || ch=='$' || ch=='_' || ch=='.' || ch=='@' ) {
	    char *pt = c->tok_text, *end = c->tok_text+TOK_MAX;
	    int toolong = false;
	    while ( (isalnum(ch) || ch=='$' || ch=='_' || ch=='.' || ch=='@' ) && pt<end ) {
		*pt++ = ch;
		ch = cgetc(c);
	    }
	    *pt = '\0';
	    while ( isalnum(ch) || ch=='$' || ch=='_' || ch=='.' ) {
		ch = cgetc(c);
		toolong = true;
	    }
	    cungetc(ch,c);
	    tok = tt_name;
	    if ( toolong )
		error( c, "Name too long" );
	    else {
		int i;
		for ( i=0; keywords[i].name!=NULL; ++i )
		    if ( strcmp(keywords[i].name,c->tok_text)==0 ) {
			tok = keywords[i].tok;
		break;
		    }
	    }
	} else if ( isdigit(ch) ) {
	    int val=0;
	    tok = tt_number;
	    if ( ch!='0' ) {
		while ( isdigit(ch)) {
		    val = 10*val+(ch-'0');
		    ch = cgetc(c);
		}
	    } else if ( isdigit(ch=cgetc(c)) ) {
		while ( isdigit(ch) && ch<'8' ) {
		    val = 8*val+(ch-'0');
		    ch = cgetc(c);
		}
	    } else if ( ch=='X' || ch=='x' || ch=='u' || ch=='U' ) {
		if ( ch=='u' || ch=='U' ) tok = tt_unicode;
		ch = cgetc(c);
		while ( isdigit(ch) || (ch>='a' && ch<='f') || (ch>='A'&&ch<='F')) {
		    if ( isdigit(ch))
			ch -= '0';
		    else if ( ch>='a' && ch<='f' )
			ch += 10-'a';
		    else
			ch += 10-'A';
		    val = 16*val+ch;
		    ch = cgetc(c);
		}
	    }
	    cungetc(ch,c);
	    c->tok_val.u.ival = val;
	    c->tok_val.type = tok==tt_number ? v_int : v_unicode;
	} else if ( ch=='\'' || ch=='"' ) {
	    int quote = ch;
	    char *pt = c->tok_text, *end = c->tok_text+TOK_MAX;
	    int toolong = false;
	    ch = cgetc(c);
	    while ( ch!=EOF && ch!='\r' && ch!='\n' && ch!=quote ) {
		if ( ch=='\\' ) {
		    ch=cgetc(c);
		    if ( ch=='\n' || ch=='\r' ) {
			cungetc(ch,c);
			ch = '\\';
		    } else if ( ch==EOF )
			ch = '\\';
		}
		if ( pt<end )
		    *pt++ = ch;
		else
		    toolong = true;
		ch = cgetc(c);
	    }
	    *pt = '\0';
	    if ( ch=='\n' || ch=='\r' )
		cungetc(ch,c);
	    tok = tt_string;
	    if ( toolong )
		error( c, "String too long" );
	} else switch( ch ) {
	  case EOF:
	    tok = tt_eof;
	  break;
	  case ' ': case '\t':
	    /* Ignore spaces */
	  break;
	  case '#':
	    /* Ignore comments */
	    while ( (ch=cgetc(c))!=EOF && ch!='\r' && ch!='\n' );
	    if ( ch=='\r' || ch=='\n' )
		cungetc(ch,c);
	  break;
	  case '(':
	    tok = tt_lparen;
	  break;
	  case ')':
	    tok = tt_rparen;
	  break;
	  case '[':
	    tok = tt_lbracket;
	  break;
	  case ']':
	    tok = tt_rbracket;
	  break;
	  case ',':
	    tok = tt_comma;
	  break;
	  case ':':
	    tok = tt_colon;
	  break;
	  case ';': case '\n': case '\r':
	    tok = tt_eos;
	  break;
	  case '-':
	    tok = tt_minus;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_minuseq;
	    else if ( ch=='-' )
		tok = tt_decr;
	    else
		cungetc(ch,c);
	  break;
	  case '+':
	    tok = tt_plus;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_pluseq;
	    else if ( ch=='+' )
		tok = tt_incr;
	    else
		cungetc(ch,c);
	  break;
	  case '!':
	    tok = tt_not;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_ne;
	    else
		cungetc(ch,c);
	  break;
	  case '~':
	    tok = tt_bitnot;
	  break;
	  case '*':
	    tok = tt_mul;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_muleq;
	    else
		cungetc(ch,c);
	  break;
	  case '%':
	    tok = tt_mod;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_modeq;
	    else
		cungetc(ch,c);
	  break;
	  case '/':
	    ch=cgetc(c);
	    if ( ch=='/' ) {
		/* another comment to eol */;
		while ( (ch=cgetc(c))!=EOF && ch!='\r' && ch!='\n' );
		if ( ch=='\r' || ch=='\n' )
		    cungetc(ch,c);
	    } else if ( ch=='*' ) {
		int found=false;
		ch = cgetc(c);
		while ( !found || ch!='/' ) {
		    if ( ch==EOF )
		break;
		    if ( ch=='*' ) found = true;
		    else found = false;
		    ch = cgetc(c);
		}
	    } else if ( ch=='=' ) {
		tok = tt_diveq;
	    } else {
		tok = tt_div;
		cungetc(ch,c);
	    }
	  break;
	  case '&':
	    tok = tt_bitand;
	    ch=cgetc(c);
	    if ( ch=='&' )
		tok = tt_and;
	    else
		cungetc(ch,c);
	  break;
	  case '|':
	    tok = tt_bitor;
	    ch=cgetc(c);
	    if ( ch=='|' )
		tok = tt_or;
	    else
		cungetc(ch,c);
	  break;
	  case '^':
	    tok = tt_xor;
	  break;
	  case '=':
	    tok = tt_assign;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_eq;
	    else
		cungetc(ch,c);
	  break;
	  case '>':
	    tok = tt_gt;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_ge;
	    else
		cungetc(ch,c);
	  break;
	  case '<':
	    tok = tt_lt;
	    ch=cgetc(c);
	    if ( ch=='=' )
		tok = tt_le;
	    else
		cungetc(ch,c);
	  break;
	  default:
	    fprintf( stderr, "%s:%d Unexpected character %c (%d)\n",
		    c->filename, c->lineno, ch, ch);
	    traceback(c);
	}
    } while ( tok==tt_error );

    c->tok = tok;
return( tok );
}

static void backuptok(Context *c) {
    if ( c->backedup )
	fprintf( stderr, "%s:%d Internal Error: Attempt to back token twice\n",
		c->filename, c->lineno );
    c->backedup = true;
}

#define PE_ARG_MAX	20

static void docall(Context *c,char *name,Val *val) {
    /* Be prepared for c->donteval */
    Val args[PE_ARG_MAX];
    Array *dontfree[PE_ARG_MAX];
    int i;
    enum token_type tok;
    Context sub;

    tok = NextToken(c);
    dontfree[0] = NULL;
    if ( tok==tt_rparen )
	i = 1;
    else {
	backuptok(c);
	for ( i=1; tok!=tt_rparen; ++i ) {
	    if ( i>=PE_ARG_MAX )
		error(c,"Too many arguments");
	    expr(c,&args[i]);
	    tok = NextToken(c);
	    if ( tok!=tt_comma )
		expect(c,tt_rparen,tok);
	    dontfree[i]=NULL;
	}
    }

    if ( !c->donteval ) {
	args[0].type = v_str;
	args[0].u.sval = name;
	memset( &sub,0,sizeof(sub));
	sub.caller = c;
	sub.a.vals = args;
	sub.a.argc = i;
	sub.return_val.type = v_void;
	sub.filename = name;
	sub.curfv = c->curfv;
	sub.trace = c->trace;
	sub.dontfree = dontfree;
	for ( i=0; i<sub.a.argc; ++i ) {
	    dereflvalif(&args[i]);
	    if ( args[i].type == v_arrfree )
		args[i].type = v_arr;
	    else if ( args[i].type == v_arr )
		dontfree[i] = args[i].u.aval;
	}

	if ( c->trace.u.ival ) {
	    printf( "%s:%d Calling %s(", GFileNameTail(c->filename), c->lineno,
		    name );
	    for ( i=1; i<sub.a.argc; ++i ) {
		if ( i!=1 ) putchar(',');
		if ( args[i].type == v_int )
		    printf( "%d", args[i].u.ival );
		else if ( args[i].type == v_unicode )
		    printf( "0u%x", args[i].u.ival );
		else if ( args[i].type == v_str )
		    printf( "\"%s\"", args[i].u.sval );
		else if ( args[i].type == v_void )
		    printf( "<void>");
		else
		    printf( "<???>");
	    }
	    printf(")\n");
	}

	for ( i=0; builtins[i].name!=NULL; ++i )
	    if ( strcmp(builtins[i].name,name)==0 )
	break;
	if ( builtins[i].name!=NULL ) {
	    if ( sub.curfv==NULL && !builtins[i].nofontok )
		error(&sub,"This command requires an active font");
	    (builtins[i].func)(&sub);
	} else {
	    if ( strchr(name,'/')==NULL && strchr(c->filename,'/')!=NULL ) {
		char *pt;
		sub.filename = galloc(strlen(c->filename)+strlen(name)+1);
		strcpy(sub.filename,c->filename);
		pt = strrchr(sub.filename,'/');
		strcpy(pt+1,name);
	    }
	    sub.script = fopen(sub.filename,"r");
	    if ( sub.script==NULL )
		error(&sub, "No such script-file or buildin function");
	    else {
		sub.lineno = 1;
		while ( !sub.returned && (tok = NextToken(&sub))!=tt_eof ) {
		    backuptok(&sub);
		    statement(&sub);
		}
		fclose(sub.script); sub.script = NULL;
	    }
	    if ( sub.filename!=name )
		free( sub.filename );
	}
	c->curfv = sub.curfv;
    }
    calldatafree(&sub);
    if ( val->type==v_str )
	free(val->u.sval);
    *val = sub.return_val;
}

static void handlename(Context *c,Val *val) {
    char name[TOK_MAX+1];
    enum token_type tok;
    int temp;
    char *pt;
    SplineFont *sf;

    strcpy(name,c->tok_text);
    val->type = v_void;
    tok = NextToken(c);
    if ( tok==tt_lparen ) {
	docall(c,name,val);
    } else if ( c->donteval ) {
	backuptok(c);
    } else {
	if ( *name=='$' ) {
	    if ( isdigit(name[1])) {
		temp = 0;
		for ( pt = name+1; isdigit(*pt); ++pt )
		    temp = 10*temp+*pt-'0';
		if ( *pt=='\0' && temp<c->a.argc ) {
		    val->type = v_lval;
		    val->u.lval = &c->a.vals[temp];
		}
	    } else if ( strcmp(name,"$argc")==0 || strcmp(name,"$#")==0 ) {
		val->type = v_int;
		val->u.ival = c->a.argc;
	    } else if ( strcmp(name,"$argv")==0 ) {
		val->type = v_arr;
		val->u.aval = &c->a;
	    } else if ( strcmp(name,"$curfont")==0 || strcmp(name,"$nextfont")==0 ||
		    strcmp(name,"$firstfont")==0 ) {
		if ( strcmp(name,"$firstfont")==0 ) {
		    if ( fv_list==NULL ) error(c,"No fonts loaded");
		    sf = fv_list->sf;
		} else {
		    if ( c->curfv==NULL ) error(c,"No current font");
		    if ( strcmp(name,"$curfont")==0 ) 
			sf = c->curfv->sf;
		    else {
			if ( c->curfv->next==NULL ) sf = NULL;
			else sf = c->curfv->next->sf;
		    }
		}
		val->type = v_str;
		val->u.sval = copy(sf==NULL?"":
			sf->filename!=NULL?sf->filename:sf->origname);
	    } else if ( strcmp(name,"$curcid")==0 || strcmp(name,"$nextcid")==0 ||
		    strcmp(name,"$firstcid")==0 ) {
		if ( c->curfv==NULL ) error(c,"No current font");
		if ( c->curfv->sf->cidmaster==NULL ) error(c,"Not a cid keyed font");
		if ( strcmp(name,"$firstcid")==0 ) {
		    sf = c->curfv->sf->cidmaster->subfonts[0];
		} else {
		    sf = c->curfv->sf;
		    if ( strcmp(name,"$nextcid")==0 ) { int i;
			for ( i = 0; i<sf->cidmaster->subfontcnt &&
				sf->cidmaster->subfonts[i]!=sf; ++i );
			if ( i>=sf->cidmaster->subfontcnt-1 )
			    sf = NULL;		/* No next */
			else
			    sf = sf->cidmaster->subfonts[i+1];
		    }
		}
		val->type = v_str;
		val->u.sval = copy(sf==NULL?"":sf->fontname);
	    } else if ( strcmp(name,"$fontname")==0 || strcmp(name,"$familyname")==0 ||
		    strcmp(name,"$fullname")==0 || strcmp(name,"$weight")==0 ||
		    strcmp(name,"$copyright")==0 || strcmp(name,"$filename")==0 ) {
		if ( c->curfv==NULL ) error(c,"No current font");
		val->type = v_str;
		val->u.sval = copy(strcmp(name,"$fontname")==0?c->curfv->sf->fontname:
			name[2]=='a'?c->curfv->sf->familyname:
			name[2]=='u'?c->curfv->sf->fullname:
			name[2]=='e'?c->curfv->sf->weight:
			name[2]=='i'?c->curfv->sf->origname:
				     c->curfv->sf->copyright);
	    } else if ( strcmp(name,"$cidfontname")==0 || strcmp(name,"cidfamilyname")==0 ||
		    strcmp(name,"$cidfullname")==0 || strcmp(name,"$cidweight")==0 ||
		    strcmp(name,"$cidcopyright")==0 ) {
		if ( c->curfv==NULL ) error(c,"No current font");
		val->type = v_str;
		if ( c->curfv->sf->cidmaster==NULL )
		    val->u.sval = copy("");
		else {
		    SplineFont *sf = c->curfv->sf->cidmaster;
		    val->u.sval = copy(strcmp(name,"$cidfontname")==0?sf->fontname:
			    name[5]=='a'?sf->familyname:
			    name[5]=='u'?sf->fullname:
			    name[5]=='e'?sf->weight:
					 sf->copyright);
		}
	    } else if ( strcmp(name,"$italicangle")==0 ) {
		if ( c->curfv==NULL ) error(c,"No current font");
		val->type = v_int;
		val->u.ival = rint(c->curfv->sf->italicangle);
	    } else if ( strcmp(name,"$fontchanged")==0 ) {
		if ( c->curfv==NULL ) error(c,"No current font");
		val->type = v_int;
		val->u.ival = c->curfv->sf->changed;
	    } else if ( strcmp(name,"$bitmaps")==0 ) {
		SplineFont *sf;
		BDFFont *bdf;
		int cnt;
		if ( c->curfv==NULL ) error(c,"No current font");
		sf = c->curfv->sf;
		if ( sf->cidmaster!=NULL ) sf = sf->cidmaster;
		for ( cnt=0, bdf=sf->bitmaps; bdf!=NULL; bdf=bdf->next ) ++cnt;
		val->type = v_arrfree;
		val->u.aval = galloc(sizeof(Array));
		val->u.aval->argc = cnt;
		val->u.aval->vals = galloc((cnt+1)*sizeof(Val));
		for ( cnt=0, bdf=sf->bitmaps; bdf!=NULL; bdf=bdf->next, ++cnt) {
		    val->u.aval->vals[cnt].type = v_int;
		    val->u.aval->vals[cnt].u.ival = bdf->pixelsize;
		    if ( bdf->clut!=NULL )
			val->u.aval->vals[cnt].u.ival |= BDFDepth(bdf)<<16;
		}
	    } else if ( strcmp(name,"$selection")==0 ) {
		SplineFont *sf;
		int i;
		if ( c->curfv==NULL ) error(c,"No current font");
		sf = c->curfv->sf;
		val->type = v_arrfree;
		val->u.aval = galloc(sizeof(Array));
		val->u.aval->argc = sf->charcnt;
		val->u.aval->vals = galloc((sf->charcnt+1)*sizeof(Val));
		for ( i=0; i<sf->charcnt; ++i) {
		    val->u.aval->vals[i].type = v_int;
		    val->u.aval->vals[i].u.ival = c->curfv->selected[i];
		}
	    } else if ( strcmp(name,"$trace")==0 ) {
		val->type = v_lval;
		val->u.lval = &c->trace;
	    } else if ( strcmp(name,"$version")==0 ) {
		extern const char *source_version_str;
		val->type = v_str;
		val->u.sval = copy(source_version_str);
	    } else if ( GetPrefs(name+1,val)) {
		/* Done */
	    }
	} else if ( *name=='@' ) {
	    if ( c->curfv==NULL ) error(c,"No current font");
	    DicaLookup(c->curfv->fontvars,name,val);
	} else if ( *name=='_' ) {
	    DicaLookup(&globals,name,val);
	} else {
	    DicaLookup(&c->locals,name,val);
	}
	if ( tok==tt_assign && val->type==v_void && *name!='$' ) {
	    /* It's ok to create this as a new variable, we're going to assign to it */
	    if ( *name=='@' ) {
		if ( c->curfv->fontvars==NULL )
		    c->curfv->fontvars = gcalloc(1,sizeof(struct dictionary));
		DicaNewEntry(c->curfv->fontvars,name,val);
	    } else if ( *name=='_' ) {
		DicaNewEntry(&globals,name,val);
	    } else {
		DicaNewEntry(&c->locals,name,val);
	    }
	}
	if ( val->type==v_void )
	    errors(c, "Undefined variable", name);
	backuptok(c);
    }
}

static void term(Context *c,Val *val) {
    enum token_type tok = NextToken(c);
    Val temp;

    if ( tok==tt_lparen ) {
	expr(c,val);
	tok = NextToken(c);
	expect(c,tt_rparen,tok);
    } else if ( tok==tt_number || tok==tt_unicode ) {
	*val = c->tok_val;
    } else if ( tok==tt_string ) {
	val->type = v_str;
	val->u.sval = copy( c->tok_text );
    } else if ( tok==tt_name ) {
	handlename(c,val);
    } else if ( tok==tt_minus || tok==tt_plus || tok==tt_not || tok==tt_bitnot ) {
	term(c,val);
	if ( !c->donteval ) {
	    dereflvalif(val);
	    if ( val->type!=v_int )
		error( c, "Invalid type in integer expression" );
	    if ( tok==tt_minus )
		val->u.ival = -val->u.ival;
	    else if ( tok==tt_not )
		val->u.ival = !val->u.ival;
	    else if ( tok==tt_bitnot )
		val->u.ival = ~val->u.ival;
	}
    } else if ( tok==tt_incr || tok==tt_decr ) {
	term(c,val);
	if ( !c->donteval ) {
	    if ( val->type!=v_lval )
		error( c, "Expected lvalue" );
	    if ( val->u.lval->type!=v_int && val->u.lval->type!=v_unicode )
		error( c, "Invalid type in integer expression" );
	    if ( tok == tt_incr )
		++val->u.lval->u.ival;
	    else
		--val->u.lval->u.ival;
	    dereflvalif(val);
	}
    } else
	expect(c,tt_name,tok);

    tok = NextToken(c);
    while ( tok==tt_incr || tok==tt_decr || tok==tt_colon || tok==tt_lparen || tok==tt_lbracket ) {
	if ( tok==tt_colon ) {
	    if ( c->donteval ) {
		tok = NextToken(c);
		expect(c,tt_name,tok);
	    } else {
		dereflvalif(val);
		if ( val->type!=v_str )
		    error( c, "Invalid type in string expression" );
		else {
		    char *pt, *ept;
		    tok = NextToken(c);
		    expect(c,tt_name,tok);
		    if ( strcmp(c->tok_text,"h")==0 ) {
			pt = strrchr(val->u.sval,'/');
			if ( pt!=NULL ) *pt = '\0';
		    } else if ( strcmp(c->tok_text,"t")==0 ) {
			pt = strrchr(val->u.sval,'/');
			if ( pt!=NULL ) {
			    char *ret = copy(pt+1);
			    free(val->u.sval);
			    val->u.sval = ret;
			}
		    } else if ( strcmp(c->tok_text,"r")==0 ) {
			pt = strrchr(val->u.sval,'/');
			if ( pt==NULL ) pt=val->u.sval;
			ept = strrchr(pt,'.');
			if ( ept!=NULL ) *ept = '\0';
		    } else if ( strcmp(c->tok_text,"e")==0 ) {
			pt = strrchr(val->u.sval,'/');
			if ( pt==NULL ) pt=val->u.sval;
			ept = strrchr(pt,'.');
			if ( ept!=NULL ) {
			    char *ret = copy(ept+1);
			    free(val->u.sval);
			    val->u.sval = ret;
			}
		    } else
			errors(c,"Unknown colon substitution", c->tok_text );
		}
	    }
	} else if ( tok==tt_lparen ) {
	    if ( c->donteval ) {
		docall(c,NULL,val);
	    } else {
		dereflvalif(val);
		if ( val->type!=v_str ) {
		    error(c,"Expected string to hold filename in procedure call");
		} else
		    docall(c,val->u.sval,val);
	    }
	} else if ( tok==tt_lbracket ) {
	    expr(c,&temp);
	    tok = NextToken(c);
	    expect(c,tt_rbracket,tok);
	    if ( !c->donteval ) {
		dereflvalif(&temp);
		if ( val->type==v_lval && (val->u.lval->type==v_arr ||val->u.lval->type==v_arrfree))
		    *val = *val->u.lval;
		if ( val->type!=v_arr && val->type!=v_arrfree )
		    error(c,"Array required");
		if (temp.type!=v_int )
		    error(c,"Integer expression required in array");
		else if ( temp.u.ival<0 || temp.u.ival>=val->u.aval->argc )
		    error(c,"Integer expression out of bounds in array");
		else if ( val->type==v_arrfree ) {
		    temp = val->u.aval->vals[temp.u.ival];
		    arrayfree(val->u.aval);
		    *val = temp;
		} else {
		    val->type = v_lval;
		    val->u.lval = &val->u.aval->vals[temp.u.ival];
		}
	    }
	} else if ( !c->donteval ) {
	    Val temp;
	    if ( val->type!=v_lval )
		error( c, "Expected lvalue" );
	    if ( val->u.lval->type!=v_int && val->u.lval->type!=v_unicode )
		error( c, "Invalid type in integer expression" );
	    temp = *val->u.lval;
	    if ( tok == tt_incr )
		++val->u.lval->u.ival;
	    else
		--val->u.lval->u.ival;
	    *val = temp;
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void mul(Context *c,Val *val) {
    Val other;
    enum token_type tok;

    term(c,val);
    tok = NextToken(c);
    while ( tok==tt_mul || tok==tt_div || tok==tt_mod ) {
	other.type = v_void;
	term(c,&other);
	if ( !c->donteval ) {
	    dereflvalif(val);
	    dereflvalif(&other);
	    if ( val->type!=v_int || other.type!=v_int )
		error( c, "Invalid type in integer expression" );
	    else if ( (tok==tt_div || tok==tt_mod ) && other.u.ival==0 )
		error( c, "Division by zero" );
	    else if ( tok==tt_mul )
		val->u.ival *= other.u.ival;
	    else if ( tok==tt_mod )
		val->u.ival %= other.u.ival;
	    else
		val->u.ival /= other.u.ival;
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void add(Context *c,Val *val) {
    Val other;
    enum token_type tok;

    mul(c,val);
    tok = NextToken(c);
    while ( tok==tt_plus || tok==tt_minus ) {
	other.type = v_void;
	mul(c,&other);
	if ( !c->donteval ) {
	    dereflvalif(val);
	    dereflvalif(&other);
	    if ( val->type==v_str && (other.type==v_str || other.type==v_int) && tok==tt_plus ) {
		char *ret, *temp;
		char buffer[10];
		if ( other.type == v_int ) {
		    sprintf(buffer,"%d", other.u.ival);
		    temp = buffer;
		} else
		    temp = other.u.sval;
		ret = galloc(strlen(val->u.sval)+strlen(temp)+1);
		strcpy(ret,val->u.sval);
		strcat(ret,temp);
		if ( other.type==v_str ) free(other.u.sval);
		free(val->u.sval);
		val->u.sval = ret;
	    } else if ( (val->type!=v_int && val->type!=v_unicode) || (other.type!=v_int&&other.type!=v_unicode) )
		error( c, "Invalid type in integer expression" );
	    else if ( tok==tt_plus )
		val->u.ival += other.u.ival;
	    else
		val->u.ival -= other.u.ival;
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void comp(Context *c,Val *val) {
    Val other;
    int cmp;
    enum token_type tok;

    add(c,val);
    tok = NextToken(c);
    while ( tok==tt_eq || tok==tt_ne || tok==tt_gt || tok==tt_lt || tok==tt_ge || tok==tt_le ) {
	other.type = v_void;
	add(c,&other);
	if ( !c->donteval ) {
	    dereflvalif(val);
	    dereflvalif(&other);
	    if ( val->type==v_str && other.type==v_str ) {
		cmp = strcmp(val->u.sval,other.u.sval);
		free(val->u.sval); free(other.u.sval);
	    } else if (( val->type==v_int || val->type==v_unicode ) &&
		    (other.type==v_int || other.type==v_unicode)) {
		cmp = val->u.ival - other.u.ival;
	    } else 
		error( c, "Invalid type in integer expression" );
	    val->type = v_int;
	    if ( tok==tt_eq ) val->u.ival = (cmp==0);
	    else if ( tok==tt_ne ) val->u.ival = (cmp!=0);
	    else if ( tok==tt_gt ) val->u.ival = (cmp>0);
	    else if ( tok==tt_lt ) val->u.ival = (cmp<0);
	    else if ( tok==tt_ge ) val->u.ival = (cmp>=0);
	    else if ( tok==tt_le ) val->u.ival = (cmp<=0);
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void _and(Context *c,Val *val) {
    Val other;
    int old = c->donteval;
    enum token_type tok;

    comp(c,val);
    tok = NextToken(c);
    while ( tok==tt_and || tok==tt_bitand ) {
	other.type = v_void;
	if ( !c->donteval )
	    dereflvalif(val);
	if ( tok==tt_and && val->u.ival==0 )
	    c->donteval = true;
	comp(c,&other);
	c->donteval = old;
	if ( !old ) {
	    dereflvalif(&other);
	    if ( tok==tt_and && val->type==v_int && val->u.ival==0 )
		val->u.ival = 0;
	    else if ( val->type!=v_int || other.type!=v_int )
		error( c, "Invalid type in integer expression" );
	    else if ( tok==tt_and )
		val->u.ival = val->u.ival && other.u.ival;
	    else
		val->u.ival &= other.u.ival;
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void _or(Context *c,Val *val) {
    Val other;
    int old = c->donteval;
    enum token_type tok;

    _and(c,val);
    tok = NextToken(c);
    while ( tok==tt_or || tok==tt_bitor || tok==tt_xor ) {
	other.type = v_void;
	if ( !c->donteval )
	    dereflvalif(val);
	if ( tok==tt_or && val->u.ival!=0 )
	    c->donteval = true;
	_and(c,&other);
	c->donteval = old;
	if ( !c->donteval ) {
	    dereflvalif(&other);
	    if ( tok==tt_or && val->type==v_int && val->u.ival!=0 )
		val->u.ival = 1;
	    else if ( val->type!=v_int || other.type!=v_int )
		error( c, "Invalid type in integer expression" );
	    else if ( tok==tt_or )
		val->u.ival = val->u.ival || other.u.ival;
	    else if ( tok==tt_bitor )
		val->u.ival |= other.u.ival;
	    else
		val->u.ival ^= other.u.ival;
	}
	tok = NextToken(c);
    }
    backuptok(c);
}

static void assign(Context *c,Val *val) {
    Val other;
    enum token_type tok;

    _or(c,val);
    tok = NextToken(c);
    if ( tok==tt_assign || tok==tt_pluseq || tok==tt_minuseq || tok==tt_muleq || tok==tt_diveq || tok==tt_modeq ) {
	other.type = v_void;
	assign(c,&other);		/* that's the evaluation order here */
	if ( !c->donteval ) {
	    dereflvalif(&other);
	    if ( val->type!=v_lval )
		error( c, "Expected lvalue" );
	    else if ( other.type == v_void )
		error( c, "Void found on right side of assignment" );
	    else if ( tok==tt_assign ) {
		Val temp;
		int argi;
		temp = *val->u.lval;
		*val->u.lval = other;
		if ( other.type==v_arr )
		    val->u.lval->u.aval = arraycopy(other.u.aval);
		else if ( other.type==v_arrfree )
		    val->u.lval->type = v_arr;
		argi = val->u.lval-c->a.vals;
		/* Have to free things after we copy them */
		if ( argi>=0 && argi<c->a.argc && temp.type==v_arr &&
			temp.u.aval==c->dontfree[argi] )
		    c->dontfree[argi] = NULL;		/* Don't free it */
		else if ( temp.type == v_arr )
		    arrayfree(temp.u.aval);
		else if ( temp.type == v_str )
		    free( temp.u.sval);
	    } else if (( val->u.lval->type==v_int || val->u.lval->type==v_unicode ) && (other.type==v_int || other.type==v_unicode)) {
		if ( tok==tt_pluseq ) val->u.lval->u.ival += other.u.ival;
		else if ( tok==tt_minuseq ) val->u.lval->u.ival -= other.u.ival;
		else if ( tok==tt_muleq ) val->u.lval->u.ival *= other.u.ival;
		else if ( other.u.ival==0 )
		    error(c,"Divide by zero");
		else if ( tok==tt_modeq ) val->u.lval->u.ival %= other.u.ival;
		else val->u.lval->u.ival /= other.u.ival;
	    } else if ( tok==tt_pluseq && val->u.lval->type==v_str &&
		    (other.type==v_str || other.type==v_int)) {
		char *ret, *temp;
		char buffer[10];
		if ( other.type == v_int ) {
		    sprintf(buffer,"%d", other.u.ival);
		    temp = buffer;
		} else
		    temp = other.u.sval;
		ret = galloc(strlen(val->u.lval->u.sval)+strlen(temp)+1);
		strcpy(ret,val->u.lval->u.sval);
		strcat(ret,temp);
		if ( other.type==v_str ) free(other.u.sval);
		free(val->u.lval->u.sval);
		val->u.sval = ret;
	    } else
		error( c, "Invalid types in assignment");
	}
    } else
	backuptok(c);
}

static void expr(Context *c,Val *val) {
    val->type = v_void;
    assign(c,val);
}

static void doforeach(Context *c) {
    long here = ctell(c);
    int lineno = c->lineno;
    enum token_type tok;
    int i, selsize;
    char *sel;
    int nest;

    if ( c->curfv==NULL )
	error(c,"foreach requires an active font");
    selsize = c->curfv->sf->charcnt;
    sel = galloc(selsize);
    memcpy(sel,c->curfv->selected,selsize);
    memset(c->curfv->selected,0,selsize);
    i = 0;

    while ( 1 ) {
	while ( i<selsize && i<c->curfv->sf->charcnt && !sel[i]) ++i;
	if ( i>=selsize || i>=c->curfv->sf->charcnt )
    break;
	c->curfv->selected[i] = true;
	while ( (tok=NextToken(c))!=tt_endloop && tok!=tt_eof && !c->returned ) {
	    backuptok(c);
	    statement(c);
	}
	c->curfv->selected[i] = false;
	if ( tok==tt_eof )
	    error(c,"End of file found in foreach loop" );
	cseek(c,here);
	c->lineno = lineno;
	++i;
    }

    nest = 0;
    while ( (tok=NextToken(c))!=tt_endloop || nest>0 ) {
	if ( tok==tt_eof )
	    error(c,"End of file found in foreach loop" );
	else if ( tok==tt_while ) ++nest;
	else if ( tok==tt_foreach ) ++nest;
	else if ( tok==tt_endloop ) --nest;
    }
    if ( selsize==c->curfv->sf->charcnt )
	memcpy(c->curfv->selected,sel,selsize);
    free(sel);
}

static void dowhile(Context *c) {
    long here = ctell(c);
    int lineno = c->lineno;
    enum token_type tok;
    Val val;
    int nest;

    while ( 1 ) {
	tok=NextToken(c);
	expect(c,tt_lparen,tok);
	val.type = v_void;
	expr(c,&val);
	tok=NextToken(c);
	expect(c,tt_rparen,tok);
	dereflvalif(&val);
	if ( val.type!=v_int )
	    error( c, "Expected integer expression in while condition");
	if ( val.u.ival==0 )
    break;
	while ( (tok=NextToken(c))!=tt_endloop && tok!=tt_eof && !c->returned ) {
	    backuptok(c);
	    statement(c);
	}
	if ( tok==tt_eof )
	    error(c,"End of file found in while loop" );
	cseek(c,here);
	c->lineno = lineno;
    }

    nest = 0;
    while ( (tok=NextToken(c))!=tt_endloop || nest>0 ) {
	if ( tok==tt_eof )
	    error(c,"End of file found in while loop" );
	else if ( tok==tt_while ) ++nest;
	else if ( tok==tt_foreach ) ++nest;
	else if ( tok==tt_endloop ) --nest;
    }
}

static void doif(Context *c) {
    enum token_type tok;
    Val val;
    int nest;

    while ( 1 ) {
	tok=NextToken(c);
	expect(c,tt_lparen,tok);
	val.type = v_void;
	expr(c,&val);
	tok=NextToken(c);
	expect(c,tt_rparen,tok);
	dereflvalif(&val);
	if ( val.type!=v_int )
	    error( c, "Expected integer expression in if condition");
	if ( val.u.ival!=0 ) {
	    while ( (tok=NextToken(c))!=tt_endif && tok!=tt_eof && tok!=tt_else && tok!=tt_elseif && !c->returned ) {
		backuptok(c);
		statement(c);
	    }
	    if ( tok==tt_eof )
		error(c,"End of file found in if statement" );
    break;
	} else {
	    nest = 0;
	    while ( ((tok=NextToken(c))!=tt_endif && tok!=tt_else && tok!=tt_elseif ) || nest>0 ) {
		if ( tok==tt_eof )
		    error(c,"End of file found in if statement" );
		else if ( tok==tt_if ) ++nest;
		else if ( tok==tt_endif ) --nest;
	    }
	    if ( tok==tt_else ) {
		while ( (tok=NextToken(c))!=tt_endif && tok!=tt_eof && !c->returned ) {
		    backuptok(c);
		    statement(c);
		}
    break;
	    } else if ( tok==tt_endif )
    break;
	}
    }
    if ( c->returned )
return;
    if ( tok!=tt_endif && tok!=tt_eof ) {
	nest = 0;
	while ( (tok=NextToken(c))!=tt_endif || nest>0 ) {
	    if ( tok==tt_eof )
return;
	    else if ( tok==tt_if ) ++nest;
	    else if ( tok==tt_endif ) --nest;
	}
    }
}

static void doshift(Context *c) {
    int i;

    if ( c->a.argc==1 )
	error(c,"Attempt to shift when there are no arguments left");
    if ( c->a.vals[1].type==v_str )
	free(c->a.vals[1].u.sval );
    if ( c->a.vals[1].type==v_arr && c->a.vals[1].u.aval != c->dontfree[1] )
	arrayfree(c->a.vals[1].u.aval );
    --c->a.argc;
    for ( i=1; i<c->a.argc ; ++i ) {
	c->a.vals[i] = c->a.vals[i+1];
	c->dontfree[i] = c->dontfree[i+1];
    }
}

static void statement(Context *c) {
    enum token_type tok = NextToken(c);
    Val val;

    if ( tok==tt_while )
	dowhile(c);
    else if ( tok==tt_foreach )
	doforeach(c);
    else if ( tok==tt_if )
	doif(c);
    else if ( tok==tt_shift )
	doshift(c);
    else if ( tok==tt_else || tok==tt_elseif || tok==tt_endif || tok==tt_endloop ) {
	unexpected(c,tok);
    } else if ( tok==tt_return ) {
	tok = NextToken(c);
	backuptok(c);
	c->returned = true;
	c->return_val.type = v_void;
	if ( tok!=tt_eos ) {
	    expr(c,&c->return_val);
	    dereflvalif(&c->return_val);
	    if ( c->return_val.type==v_arr ) {
		c->return_val.type = v_arrfree;
		c->return_val.u.aval = arraycopy(c->return_val.u.aval);
	    }
	}
    } else if ( tok==tt_eos ) {
	backuptok(c);
    } else {
	backuptok(c);
	expr(c,&val);
	if ( val.type == v_str )
	    free( val.u.sval );
    }
    tok = NextToken(c);
    if ( tok!=tt_eos && tok!=tt_eof && !c->returned )
	error( c, "Unterminated statement" );
}

static FILE *CopyNonSeekableFile(FILE *former) {
    int ch = '\n';
    FILE *temp = tmpfile();
    int istty = isatty(fileno(former)) && former==stdin;

    if ( temp==NULL )
return( former );
    if ( istty )
	printf( "Type in your script file. Processing will not begin until all the script\n" );
	printf( " has been input (ie. until you have pressed ^D)\n" );
    while ( 1 ) {
	if ( ch=='\n' && istty )
	    printf( "> " );
	ch = getc(former);
	if ( ch==EOF )
    break;
	putc(ch,temp);
    }
    if ( istty )
	printf( "\n" );
    rewind(temp);
return( temp );
}

static void VerboseCheck(void) {
    if ( verbose==-1 )
	verbose = getenv("PFAEDIT_VERBOSE")!=NULL;
}

static void ProcessScript(int argc, char *argv[], FILE *script) {
    int i,j;
    Context c;
    enum token_type tok;

    VerboseCheck();

    i=1;
    if ( script!=NULL )
	i = 0;
    else if ( strcmp(argv[1],"-script")==0 )
	++i;
    memset( &c,0,sizeof(c));
    c.a.argc = argc-i;
    c.a.vals = galloc(c.a.argc*sizeof(Val));
    c.dontfree = gcalloc(c.a.argc,sizeof(Array*));
    for ( j=i; j<argc; ++j ) {
	c.a.vals[j-i].type = v_str;
	c.a.vals[j-i].u.sval = copy(argv[j]);
    }
    c.return_val.type = v_void;
    if ( script!=NULL ) {
	c.filename = "<stdin>";
	c.script = script;
    } else if ( i<argc && strcmp(argv[i],"-")!=0 ) {
	c.filename = argv[i];
	c.script = fopen(c.filename,"r");
    } else {
	c.filename = "<stdin>";
	c.script = stdin;
    }
    /* On Mac OS/X fseek/ftell appear to be broken and return success even */
    /*  for terminals. They should return -1, EBADF */
    if ( c.script!=NULL && (ftell(c.script)==-1 || isatty(fileno(c.script))) )
	c.script = CopyNonSeekableFile(c.script);
    if ( c.script==NULL )
	error(&c, "No such file");
    else {
	c.lineno = 1;
	while ( !c.returned && (tok = NextToken(&c))!=tt_eof ) {
	    backuptok(&c);
	    statement(&c);
	}
	fclose(c.script);
    }
    for ( i=0; i<c.a.argc; ++i )
	free(c.a.vals[i].u.sval);
    free(c.a.vals);
    free(c.dontfree);
    exit(0);
}

static void _CheckIsScript(int argc, char *argv[]) {
    if ( argc==1 )
return;
    if ( strcmp(argv[1],"-script")==0 || strcmp(argv[1],"--script")==0 )
	ProcessScript(argc, argv,NULL);
    if ( access(argv[1],X_OK|R_OK)==0 ) {
	FILE *temp = fopen(argv[1],"r");
	char buffer[200];
	if ( temp==NULL )
return;
	buffer[0] = '\0';
	fgets(buffer,sizeof(buffer),temp);
	fclose(temp);
	if ( buffer[0]=='#' && buffer[1]=='!' && strstr(buffer,"pfaedit")!=NULL )
	    ProcessScript(argc, argv,NULL);
    }
}

#ifdef X_DISPLAY_MISSING
static void _doscriptusage(void) {
    printf( "pfaedit [options]\n" );
    printf( "\t-usage\t\t\t (displays this message, and exits)\n" );
    printf( "\t-help\t\t\t (displays this message, invokes a browser)\n\t\t\t\t  (Using the BROWSER environment variable)\n" );
    printf( "\t-version\t\t (prints the version of pfaedit and exits)\n" );
    printf( "\t-script scriptfile\t (executes scriptfile)\n" );
    printf( "\n" );
    printf( "If no scriptfile is given (or if it's \"-\") PfaEdit will read stdin\n" );
    printf( "pfaedit will read postscript (pfa, pfb, ps, cid), opentype (otf),\n" );
    printf( "\ttruetype (ttf,ttc), macintosh resource fonts (dfont,bin,hqx),\n" );
    printf( "\tand bdf and pcf fonts. It will also read it's own format --\n" );
    printf( "\tsfd files.\n" );
    printf( "Any arguments after the script file will be passed to it.\n");
    printf( "If the first argument is an executable filename, and that file's first\n" );
    printf( "\tline contains \"pfaedit\" then it will be treated as a scriptfile.\n\n" );
    printf( "For more information see:\n\thttp://pfaedit.sourceforge.net/\n" );
    printf( "Send bug reports to:\tpfaedit-devel@lists.sourceforge.net\n" );
}

static void doscriptusage(void) {
    _doscriptusage();
exit(0);
}

static void doscripthelp(void) {
    _doscriptusage();
    help("overview.html");
exit(0);
}
#endif

void CheckIsScript(int argc, char *argv[]) {
    _CheckIsScript(argc, argv);
#ifdef X_DISPLAY_MISSING
    if ( argc==2 ) {
	char *pt = argv[1];
	if ( *pt=='-' && pt[1]=='-' ) ++pt;
	if ( strcmp(pt,"-usage")==0 )
	    doscriptusage();
	else if ( strcmp(pt,"-help")==0 )
	    doscripthelp();
	else if ( strcmp(pt,"-version")==0 )
	    doversion();
    }
    ProcessScript(argc, argv,stdin);
#endif
}

void ExecuteScriptFile(FontView *fv, char *filename) {
    Context c;
    Val argv[1];
    Array *dontfree[1];
    enum token_type tok;
    jmp_buf env;

    VerboseCheck();

    memset( &c,0,sizeof(c));
    c.a.argc = 1;
    c.a.vals = argv;
    c.dontfree = dontfree;
    argv[0].type = v_str;
    argv[0].u.sval = filename;
    c.filename = filename;
    c.return_val.type = v_void;
    c.err_env = &env;
    c.curfv = fv;
    if ( setjmp(env)!=0 )
return;				/* Error return */

    c.script = fopen(c.filename,"r");
    if ( c.script==NULL )
	error(&c, "No such file");
    else {
	c.lineno = 1;
	while ( !c.returned && (tok = NextToken(&c))!=tt_eof ) {
	    backuptok(&c);
	    statement(&c);
	}
	fclose(c.script);
    }
}

struct sd_data {
    int done;
    FontView *fv;
    GWindow gw;
};

#define SD_Width	250
#define SD_Height	270
#define CID_Script	1001

static int SD_Call(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	static unichar_t filter[] = { '*',/*'.','p','e',*/  0 };
	unichar_t *fn;
	unichar_t *insert;
    
	fn = GWidgetOpenFile(GStringGetResource(_STR_CallScript,NULL), NULL, filter, NULL, NULL);
	if ( fn==NULL )
return(true);
	insert = galloc((u_strlen(fn)+10)*sizeof(unichar_t));
	*insert = '"';
	u_strcpy(insert+1,fn);
	uc_strcat(insert,"\"()");
	GTextFieldReplace(GWidgetGetControl(GGadgetGetWindow(g),CID_Script),insert);
	free(insert);
	free(fn);
    }
return( true );
}

static int SD_OK(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	struct sd_data *sd = GDrawGetUserData(GGadgetGetWindow(g));
	Context c;
	Val args[1];
	Array *dontfree[1];
	jmp_buf env;
	enum token_type tok;

	memset( &c,0,sizeof(c));
	memset( args,0,sizeof(args));
	memset( dontfree,0,sizeof(dontfree));
	c.a.argc = 1;
	c.a.vals = args;
	c.dontfree = dontfree;
	c.filename = args[0].u.sval = "ScriptDlg";
	args[0].type = v_str;
	c.return_val.type = v_void;
	c.err_env = &env;
	c.curfv = sd->fv;
	if ( setjmp(env)!=0 )
return( true );			/* Error return */

	c.script = tmpfile();
	if ( c.script==NULL )
	    error(&c, "Can't create temporary file");
	else {
	    const unichar_t *ret = _GGadgetGetTitle(GWidgetGetControl(sd->gw,CID_Script));
	    while ( *ret ) {
		/* There's a bug here. Filenames need to be converted to the local charset !!!! */
		putc(*ret,c.script);
		++ret;
	    }
	    rewind(c.script);
	    VerboseCheck();
	    c.lineno = 1;
	    while ( !c.returned && (tok = NextToken(&c))!=tt_eof ) {
		backuptok(&c);
		statement(&c);
	    }
	    fclose(c.script);
	    sd->done = true;
	}
    }
return( true );
}

static void SD_DoCancel(struct sd_data *sd) {
    sd->done = true;
}

static int SD_Cancel(GGadget *g, GEvent *e) {
    if ( e->type==et_controlevent && e->u.control.subtype == et_buttonactivate ) {
	SD_DoCancel( GDrawGetUserData(GGadgetGetWindow(g)));
    }
return( true );
}

static int sd_e_h(GWindow gw, GEvent *event) {
    if ( event->type==et_close ) {
	SD_DoCancel( GDrawGetUserData(gw));
    } else if ( event->type==et_char ) {
	if ( event->u.chr.keysym == GK_F1 || event->u.chr.keysym == GK_Help ) {
	    help("scripting.html");
return( true );
	}
return( false );
    } else if ( event->type == et_map ) {
	/* Above palettes */
	GDrawRaise(gw);
    }
return( true );
}

void ScriptDlg(FontView *fv) {
    GRect pos;
    static GWindow gw;
    GWindowAttrs wattrs;
    GGadgetCreateData gcd[10];
    GTextInfo label[10];
    struct sd_data sd;
    FontView *list;

    memset(&sd,0,sizeof(sd));
    sd.fv = fv;

    if ( gw==NULL ) {
	memset(&wattrs,0,sizeof(wattrs));
	wattrs.mask = wam_events|wam_cursor|wam_wtitle|wam_undercursor|wam_restrict|wam_isdlg;
	wattrs.event_masks = ~(1<<et_charup);
	wattrs.restrict_input_to_me = 1;
	wattrs.undercursor = 1;
	wattrs.cursor = ct_pointer;
	wattrs.window_title = GStringGetResource(_STR_ExecuteScript,NULL);
	wattrs.is_dlg = true;
	pos.x = pos.y = 0;
	pos.width = GDrawPointsToPixels(NULL,GGadgetScale(SD_Width));
	pos.height = GDrawPointsToPixels(NULL,SD_Height);
	gw = GDrawCreateTopWindow(NULL,&pos,sd_e_h,&sd,&wattrs);

	memset(&gcd,0,sizeof(gcd));
	memset(&label,0,sizeof(label));

	gcd[0].gd.pos.x = 10; gcd[0].gd.pos.y = 10;
	gcd[0].gd.pos.width = SD_Width-20; gcd[0].gd.pos.height = SD_Height-54;
	gcd[0].gd.flags = gg_visible | gg_enabled | gg_textarea_wrap;
	gcd[0].gd.cid = CID_Script;
	gcd[0].creator = GTextAreaCreate;

	gcd[1].gd.pos.x = 25-3; gcd[1].gd.pos.y = SD_Height-32-3;
	gcd[1].gd.pos.width = -1; gcd[1].gd.pos.height = 0;
	gcd[1].gd.flags = gg_visible | gg_enabled | gg_but_default;
	label[1].text = (unichar_t *) _STR_OK;
	label[1].text_in_resource = true;
	gcd[1].gd.mnemonic = 'O';
	gcd[1].gd.label = &label[1];
	gcd[1].gd.handle_controlevent = SD_OK;
	gcd[1].creator = GButtonCreate;

	gcd[2].gd.pos.x = -25; gcd[2].gd.pos.y = SD_Height-32;
	gcd[2].gd.pos.width = -1; gcd[2].gd.pos.height = 0;
	gcd[2].gd.flags = gg_visible | gg_enabled | gg_but_cancel;
	label[2].text = (unichar_t *) _STR_Cancel;
	label[2].text_in_resource = true;
	gcd[2].gd.label = &label[2];
	gcd[2].gd.mnemonic = 'C';
	gcd[2].gd.handle_controlevent = SD_Cancel;
	gcd[2].creator = GButtonCreate;

	gcd[3].gd.pos.x = (SD_Width-GIntGetResource(_NUM_Buttonsize)*100/GIntGetResource(_NUM_ScaleFactor))/2; gcd[3].gd.pos.y = SD_Height-40;
	gcd[3].gd.pos.width = -1; gcd[3].gd.pos.height = 0;
	gcd[3].gd.flags = gg_visible | gg_enabled;
	label[3].text = (unichar_t *) _STR_Call;
	label[3].text_in_resource = true;
	gcd[3].gd.label = &label[3];
	gcd[3].gd.mnemonic = 'a';
	gcd[3].gd.handle_controlevent = SD_Call;
	gcd[3].creator = GButtonCreate;

	gcd[4].gd.pos.x = 2; gcd[4].gd.pos.y = 2;
	gcd[4].gd.pos.width = pos.width-4; gcd[4].gd.pos.height = pos.height-4;
	gcd[4].gd.flags = gg_enabled | gg_visible | gg_pos_in_pixels;
	gcd[4].creator = GGroupCreate;

	GGadgetsCreate(gw,gcd);
    }
    sd.gw = gw;
    GDrawSetUserData(gw,&sd);
    GDrawSetVisible(gw,true);
    while ( !sd.done )
	GDrawProcessOneEvent(NULL);
    GDrawSetVisible(gw,false);

    /* Selection may be out of date, force a refresh */
    for ( list = fv_list; list!=NULL; list=list->next )
	GDrawRequestExpose(list->v,NULL,false);
}
