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
#include "pfaedit.h"
#include <math.h>
#include <locale.h>
#include <ustring.h>
#include "utype.h"
#include "psfont.h"
#include "sd.h"

/* This is not a "real" structure. It is a temporary hack that encompasses */
/*  various possibilities, the combination of which won't occur in reality */
typedef struct entitychar {
    Entity *splines;
    RefChar *refs;
    int width;
    SplineChar *sc;
} EntityChar;

typedef struct _io {
    char *macro, *start;
    FILE *ps;
    int backedup;
    struct _io *prev;
} _IO;

typedef struct io {
    struct _io *top;
} IO;

typedef struct growbuf {
    char *pt;
    char *base;
    char *end;
} GrowBuf;

static void GrowBuffer(GrowBuf *gb,int len) {
    if ( len<400 ) len = 400;
    if ( gb->base==NULL ) {
	gb->base = gb->pt = galloc(len);
	gb->end = gb->base + len;
    } else {
	int off = gb->pt-gb->base;
	len += (gb->end-gb->base);
	gb->base = grealloc(gb->base,len);
	gb->end = gb->base + len;
	gb->pt = gb->base+off;
    }
}

static void AddTok(GrowBuf *gb,char *buf,int islit) {
    int len = islit + strlen(buf) + 1;

    if ( gb->pt+len+1 >= gb->end )
	GrowBuffer(gb,len+1);
    if ( islit )
	*(gb->pt++) = '/';
    strcpy(gb->pt,buf);
    gb->pt += strlen(buf);
    *gb->pt++ = ' ';
}

/**************************** Postscript Importer *****************************/
/* It's really dumb. It ignores almost everything except linetos and curvetos */
/*  anything else, function calls, ... is thrown out, if this breaks a lineto */
/*  or curveto or moveto (if there aren't enough things on the stack) then we */
/*  ignore that too */

enum pstoks { pt_eof=-1, pt_moveto, pt_rmoveto, pt_curveto, pt_rcurveto,
    pt_lineto, pt_rlineto, pt_arc, pt_arcn, pt_arct, pt_arcto,
    pt_newpath, pt_closepath, pt_dup, pt_pop, pt_index,
    pt_exch, pt_roll, pt_clear, pt_copy, pt_count,
    pt_setcachedevice, pt_setcharwidth,
    pt_translate, pt_scale, pt_rotate, pt_concat, pt_end, pt_exec,
    pt_add, pt_sub, pt_mul, pt_div, pt_idiv, pt_mod, pt_neg,
    pt_abs, pt_round, pt_ceiling, pt_floor, pt_truncate, pt_max, pt_min,
    pt_ne, pt_eq, pt_gt, pt_ge, pt_lt, pt_le, pt_and, pt_or, pt_xor, pt_not,
    pt_true, pt_false,
    pt_if, pt_ifelse, pt_def, pt_bind, pt_load,
    pt_setlinecap, pt_setlinejoin, pt_setlinewidth,
    pt_currentlinecap, pt_currentlinejoin, pt_currentlinewidth,
    pt_setgray, pt_currentgray, pt_sethsbcolor, pt_currenthsbcolor,
    pt_setrgbcolor, pt_currentrgbcolor, pt_setcmykcolor, pt_currentcmykcolor,
    pt_fill, pt_stroke,

    pt_transform, pt_itransform, pt_dtransform, pt_idtransform,

    /* things we sort of pretend to do, but actually do something wrong */
    pt_gsave, pt_grestore, pt_save, pt_restore, pt_currentmatrix, pt_setmatrix,
    pt_setmiterlimit, pt_setdash,

    pt_mark, pt_counttomark, pt_cleartomark, pt_array, pt_aload, pt_astore,
    pt_print, pt_cvi, pt_cvlit, pt_cvn, pt_cvr, pt_cvrs, pt_cvs, pt_cvx, pt_stringop,

    pt_opencurly, pt_closecurly, pt_openarray, pt_closearray, pt_string,
    pt_number, pt_unknown, pt_namelit, pt_output, pt_outputd };

char *toknames[] = { "moveto", "rmoveto", "curveto", "rcurveto",
	"lineto", "rlineto", "arc", "arcn", "arct", "arcto",
	"newpath", "closepath", "dup", "pop", "index",
	"exch", "roll", "clear", "copy", "count",
	"setcachedevice", "setcharwidth",
	"translate", "scale", "rotate", "concat", "end", "exec",
	"add", "sub", "mul", "div", "idiv", "mod", "neg",
	"abs", "round", "ceiling", "floor", "truncate", "max", "min",
	"ne", "eq", "gt", "ge", "lt", "le", "and", "or", "xor", "not",
	"true", "false", 
	"if", "ifelse", "def", "bind", "load",
	"setlinecap", "setlinejoin", "setlinewidth",
	"currentlinecap", "currentlinejoin", "currentlinewidth",
	"setgray", "currentgray", "sethsbcolor", "currenthsbcolor",
	"setrgbcolor", "currentrgbcolor", "setcmykcolor", "currentcmykcolor",
	"fill", "stroke",

	"transform", "itransform", "dtransform", "idtransform",

	"gsave", "grestore", "save", "restore", "currentmatrix", "setmatrix",
	"setmiterlimit", "setdash",

	"mark", "counttomark", "cleartomark", "array", "aload", "astore",
	"print", "cvi", "cvlit", "cvn", "cvr", "cvrs", "cvs", "cvx", "string",

	"opencurly", "closecurly", "openarray", "closearray", "string",
	"number", "unknown", "namelit", "=", "==",

	NULL };

/* length (of string)
    sqrt sin ...
    fill eofill stroke
    gsave grestore
*/

static int nextch(IO *wrapper) {
    int ch;
    _IO *io = wrapper->top;

    while ( io!=NULL ) {
	if ( io->backedup!=EOF ) {
	    ch = io->backedup;
	    io->backedup = EOF;
return( ch );
	} else if ( io->ps!=NULL ) {
	    if ( (ch = getc(io->ps))!=EOF )
return( ch );
	} else {
	    if ( (ch = *(io->macro++))!='\0' )
return( ch );
	}
	wrapper->top = io->prev;
	free(io->start);
	free(io);
	io = wrapper->top;
    }
return( EOF );
}

static void unnextch(int ch,IO *wrapper) {
    if ( ch==EOF )
return;
    if ( wrapper->top==NULL )
	fprintf( stderr, "Can't back up with nothing on stack\n" );
    else if ( wrapper->top->backedup!=EOF )
	fprintf( stderr, "Attempt to back up twice\n" );
    else
	wrapper->top->backedup = ch;
}

static void pushio(IO *wrapper, FILE *ps, char *macro) {
    _IO *io = galloc(sizeof(_IO));

    io->prev = wrapper->top;
    io->ps = ps;
    io->macro = io->start = copy(macro);
    io->backedup = EOF;
    wrapper->top = io;
}

static int nextpstoken(IO *wrapper, real *val, char *tokbuf, int tbsize) {
    int ch, r, i;
    char *pt, *end;

    /* Eat whitespace and comments. Comments last to eol (or formfeed) */
    while ( 1 ) {
	while ( isspace(ch = nextch(wrapper)) );
	if ( ch!='%' )
    break;
	while ( (ch=nextch(wrapper))!=EOF && ch!='\r' && ch!='\n' && ch!='\f' );
    }

    if ( ch==EOF )
return( pt_eof );

    pt = tokbuf;
    end = pt+tbsize-1;
    *pt++ = ch; *pt='\0';

    if ( ch=='(' ) {
	int nest=1, quote=0;
	while ( (ch=nextch(wrapper))!=EOF ) {
	    if ( pt<end ) *pt++ = ch;
	    if ( quote )
		quote=0;
	    else if ( ch=='(' )
		++nest;
	    else if ( ch==')' ) {
		if ( --nest==0 )
	break;
	    } else if ( ch=='\\' )
		quote = 1;
	}
	*pt='\0';
return( pt_string );
    } else if ( ch=='<' ) {
	ch = nextch(wrapper);
	if ( pt<end ) *pt++ = ch;
	if ( ch=='>' )
	    /* Done */;
	else if ( ch!='~' ) {
	    while ( (ch=nextch(wrapper))!=EOF && ch!='>' )
		if ( pt<end ) *pt++ = ch;
	} else {
	    int twiddle=0;
	    while ( (ch=nextch(wrapper))!=EOF ) {
		if ( pt<end ) *pt++ = ch;
		if ( ch=='~' ) twiddle = 1;
		else if ( twiddle && ch=='>' )
	    break;
		else twiddle = 0;
	    }
	}
	*pt='\0';
return( pt_string );
    } else if ( ch==')' || ch=='>' || ch=='[' || ch==']' || ch=='{' || ch=='}' ) {
	if ( ch=='{' )
return( pt_opencurly );
	else if ( ch=='}' )
return( pt_closecurly );
	if ( ch=='[' )
return( pt_openarray );
	else if ( ch==']' )
return( pt_closearray );

return( pt_unknown );	/* single character token */
    } else if ( ch=='/' ) {
	pt = tokbuf;
	while ( (ch=nextch(wrapper))!=EOF && !isspace(ch) && ch!='%' &&
		ch!='(' && ch!=')' && ch!='<' && ch!='>' && ch!='[' && ch!=']' &&
		ch!='{' && ch!='}' && ch!='/' )
	    if ( pt<tokbuf+tbsize-2 )
		*pt++ = ch;
	*pt = '\0';
	unnextch(ch,wrapper);
return( pt_namelit );	/* name literal */
    } else {
	while ( (ch=nextch(wrapper))!=EOF && !isspace(ch) && ch!='%' &&
		ch!='(' && ch!=')' && ch!='<' && ch!='>' && ch!='[' && ch!=']' &&
		ch!='{' && ch!='}' && ch!='/' ) {
	    if ( pt<tokbuf+tbsize-2 )
		*pt++ = ch;
	}
	*pt = '\0';
	unnextch(ch,wrapper);
	r = strtol(tokbuf,&end,10);
	pt = end;
	if ( *pt=='\0' ) {		/* It's a normal integer */
	    *val = r;
return( pt_number );
	} else if ( *pt=='#' ) {
	    r = strtol(pt+1,&end,r);
	    if ( *end=='\0' ) {		/* It's a radix integer */
		*val = r;
return( pt_number );
	    }
	} else {
	    *val = strtod(tokbuf,&end);
	    if ( *end=='\0' )		/* It's a real */
return( pt_number );
	}
	/* It's not a number */
	for ( i=0; toknames[i]!=NULL; ++i )
	    if ( strcmp(tokbuf,toknames[i])==0 )
return( i );

return( pt_unknown );
    }
}

static void Transform(BasePoint *to, BasePoint *from, real trans[6]) {
    to->x = trans[0]*from->x+trans[2]*from->y+trans[4];
    to->y = trans[1]*from->y+trans[3]*from->y+trans[5];
}

void MatMultiply(real m1[6], real m2[6], real to[6]) {
    real trans[6];

    trans[0] = m1[0]*m2[0] +
		m1[1]*m2[2];
    trans[1] = m1[0]*m2[1] +
		m1[1]*m2[3];
    trans[2] = m1[2]*m2[0] +
		m1[3]*m2[2];
    trans[3] = m1[2]*m2[1] +
		m1[3]*m2[3];
    trans[4] = m1[4]*m2[0] +
		m1[5]*m2[2] +
		m2[4];
    trans[5] = m1[4]*m2[1] +
		m1[5]*m2[3] +
		m2[5];
    memcpy(to,trans,sizeof(trans));
}

static void MatInverse(real into[6], real orig[6]) {
    real det = orig[0]*orig[3] - orig[1]*orig[2];

    if ( det==0 ) {
	fprintf(stderr, "Attempt to invert a singular matrix\n" );
	memset(into,0,sizeof(*into));
    } else {
	into[0] =  orig[3]/det;
	into[1] = -orig[1]/det;
	into[2] = -orig[2]/det;
	into[3] =  orig[0]/det;
	into[4] = -orig[4];
	into[5] = -orig[5];
    }
}

static void ECCatagorizePoints( EntityChar *ec ) {
    SplineChar sc;
    Entity *ent;

    memset(&sc,'\0',sizeof(sc));
    for ( ent=ec->splines; ent!=NULL; ent=ent->next ) if ( ent->type == et_splines ) {
	sc.splines = ent->u.splines.splines;
	SCCatagorizePoints(&sc);
    }
}

struct pskeydict {
    int16 cnt, max;
    uint8 is_executable;
    struct pskeyval *entries;
};

struct psstack {
    enum pstype { ps_void, ps_num, ps_bool, ps_string, ps_instr, ps_lit,
		  ps_mark, ps_array, ps_dict } type;
    union vals {
	real val;
	int tf;
	char *str;
	struct pskeydict dict;		/* and for arrays too */
    } u;
};

struct pskeyval {
    enum pstype type;
    union vals u;
    char *key;
};

static int AddEntry(struct pskeydict *dict,struct psstack *stack, int sp) {
    int i;

    if ( dict->cnt>=dict->max ) {
	if ( dict->cnt==0 ) {
	    dict->max = 30;
	    dict->entries = galloc(dict->max*sizeof(struct pskeyval));
	} else {
	    dict->max += 30;
	    dict->entries = grealloc(dict->entries,dict->max*sizeof(struct pskeyval));
	}
    }
    if ( sp<2 )
return(sp);
    if ( stack[sp-2].type!=ps_string && stack[sp-2].type!=ps_lit ) {
	fprintf( stderr, "Key for a def must be a string or name literal\n" );
return(sp-2);
    }
    for ( i=0; i<dict->cnt; ++i )
	if ( strcmp(dict->entries[i].key,stack[sp-2].u.str)==0 )
    break;
    if ( i!=dict->cnt ) {
	free(stack[sp-2].u.str);
	if ( dict->entries[i].type==ps_string || dict->entries[i].type==ps_instr ||
		dict->entries[i].type==ps_lit )
	    free(dict->entries[i].u.str);
    } else {
	memset(&dict->entries[i],'\0',sizeof(struct pskeyval));
	dict->entries[i].key = stack[sp-2].u.str;
	++dict->cnt;
    }
    dict->entries[i].type = stack[sp-1].type;
    dict->entries[i].u = stack[sp-1].u;
return(sp-2);
}

static int rollstack(struct psstack *stack, int sp) {
    int n,j,i;
    struct psstack *temp;

    if ( sp>1 ) {
	n = stack[sp-2].u.val;
	j = stack[sp-1].u.val;
	sp-=2;
	if ( sp>=n && n>0 ) {
	    j %= n;
	    if ( j<0 ) j += n;
	    temp = galloc(n*sizeof(struct psstack));
	    for ( i=0; i<n; ++i )
		temp[i] = stack[sp-n+i];
	    for ( i=0; i<n; ++i )
		stack[sp-n+(i+j)%n] = temp[i];
	    free(temp);
	}
    }
return( sp );
}

static void circlearcto(real a1, real a2, real cx, real cy, real r,
	SplineSet *cur, real *transform ) {
    SplinePoint *pt;
    BasePoint temp, base, cp;
    real cplen;
    int sign=1;
    real s1, s2, c1, c2;

    if ( a1==a2 )
return;

    cplen = (a2-a1)/90 * r * .552;
    a1 *= 3.1415926535897932/180; a2 *= 3.1415926535897932/180;
    s1 = sin(a1); s2 = sin(a2); c1 = cos(a1); c2 = cos(a2);
    temp.x = cx+r*c2; temp.y = cy+r*s2;
    base.x = cx+r*c1; base.y = cy+r*s1;
    pt = chunkalloc(sizeof(SplinePoint));
    Transform(&pt->me,&temp,transform);
    cp.x = temp.x-cplen*s2; cp.y = temp.y + cplen*c2;
    if ( (cp.x-base.x)*(cp.x-base.x)+(cp.y-base.y)*(cp.y-base.y) >
	     (temp.x-base.x)*(temp.x-base.x)+(temp.y-base.y)*(temp.y-base.y) ) {
	sign = -1;
	cp.x = temp.x+cplen*s2; cp.y = temp.y - cplen*c2;
    }
    Transform(&pt->prevcp,&cp,transform);
    pt->nonextcp = true;
    cp.x = base.x + sign*cplen*s1; cp.y = base.y - sign*cplen*c1;
    Transform(&cur->last->nextcp,&cp,transform);
    cur->last->nonextcp = false;
    SplineMake3(cur->last,pt);
    cur->last = pt;
}

static void circlearcsto(real a1, real a2, real cx, real cy, real r,
	SplineSet *cur, real *transform, int clockwise ) {
    int a;
    real last;

    while ( a1<0 ) a1 += 360; while ( a2<0 ) a2 += 360;
    while ( a1>=360 ) a1 -= 360; while ( a2>=360 ) a2 -= 360;
    if ( !clockwise ) {
	if ( a1>a2 )
	    a2 += 360;
	last = a1;
	for ( a=((int) (a1+90)/90)*90; a<a2; a += 90 ) {
	    circlearcto(last,a,cx,cy,r,cur,transform);
	    last = a;
	}
	circlearcto(last,a2,cx,cy,r,cur,transform);
    } else {
	if ( a2>a1 )
	    a1 += 360;
	last = a1;
	for ( a=((int) (a1-90)/90)*90+90; a>a2; a -= 90 ) {
	    circlearcto(last,a,cx,cy,r,cur,transform);
	    last = a;
	}
	circlearcto(last,a2,cx,cy,r,cur,transform);
    }
}

static struct pskeyval *lookup(struct pskeydict *dict,char *tokbuf) {
    int i;

    for ( i=0; i<dict->cnt; ++i )
	if ( strcmp(dict->entries[i].key,tokbuf)==0 )
return( &dict->entries[i] );

return( NULL );
}

static void dictfree(struct pskeydict *dict) {
    int i;

    for ( i=0; i<dict->cnt; ++i ) {
	if ( dict->entries[i].type==ps_string || dict->entries[i].type==ps_instr ||
		dict->entries[i].type==ps_lit )
	    free(dict->entries[i].u.str);
	else if ( dict->entries[i].type==ps_array || dict->entries[i].type==ps_dict )
	    dictfree(&dict->entries[i].u.dict);
    }
}

static void copyarray(struct pskeydict *to,struct pskeydict *from) {
    int i;
    struct pskeyval *oldent = from->entries;

    *to = *from;
    to->entries = gcalloc(to->cnt,sizeof(struct pskeyval));
    for ( i=0; i<to->cnt; ++i ) {
	to->entries[i] = oldent[i];
	if ( to->entries[i].type==ps_string || to->entries[i].type==ps_instr ||
		to->entries[i].type==ps_lit )
	    to->entries[i].u.str = copy(to->entries[i].u.str);
	else if ( to->entries[i].type==ps_array || to->entries[i].type==ps_dict )
	    copyarray(&to->entries[i].u.dict,&oldent[i].u.dict);
    }
}

static int aload(int sp, struct psstack *stack,int stacktop) {
    int i;

    if ( sp>=1 && stack[sp-1].type==ps_array ) {
	struct pskeydict dict;
	--sp;
	dict = stack[sp].u.dict;
	for ( i=0; i<dict.cnt; ++i ) {
	    if ( sp<stacktop ) {
		stack[sp].type = dict.entries[i].type;
		stack[sp].u = dict.entries[i].u;
		if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
			stack[sp].type==ps_lit )
		    stack[sp].u.str = copy(stack[sp].u.str);
/* The following is incorrect behavior, but as I don't do garbage collection */
/*  and I'm not going to implement reference counts, this will work in most cases */
		else if ( stack[sp].type==ps_array )
		    copyarray(&stack[sp].u.dict,&stack[sp].u.dict);
		++sp;
	    }
	}
	if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
	    stack[sp].type = ps_array;
	    stack[sp].u.dict = dict;
	    ++sp;
	}
    }
return( sp );
}

static void printarray(struct pskeydict *dict) {
    int i;

    printf("[" );
    for ( i=0; i<dict->cnt; ++i ) {
	switch ( dict->entries[i].type ) {
	  case ps_num:
	    printf( "%g", dict->entries[i].u.val );
	  break;
	  case ps_bool:
	    printf( "%s", dict->entries[i].u.tf ? "true" : "false" );
	  break;
	  case ps_string: case ps_instr: case ps_lit:
	    printf( dict->entries[i].type==ps_lit ? "/" :
		    dict->entries[i].type==ps_string ? "(" : "{" );
	    printf( "%s", dict->entries[i].u.str );
	    printf( dict->entries[i].type==ps_lit ? "" :
		    dict->entries[i].type==ps_string ? ")" : "}" );
	  break;
	  case ps_array:
	    printarray(&dict->entries[i].u.dict);
	  break;
	  case ps_void:
	    printf( "-- void --" );
	  break;
	  default:
	    printf( "-- nostringval --" );
	  break;
	}
	printf(" ");
    }
    printf( "]" );
}

static void freestuff(struct psstack *stack, int sp, struct pskeydict *dict, GrowBuf *gb) {
    int i;

    free(gb->base);
    for ( i=0; i<dict->cnt; ++i ) {
	if ( dict->entries[i].type==ps_string || dict->entries[i].type==ps_instr ||
		dict->entries[i].type==ps_lit )
	    free(dict->entries[i].u.str);
	free(dict->entries[i].key);
    }
    free( dict->entries );
    for ( i=0; i<sp; ++i ) {
	if ( stack[i].type==ps_string || stack[i].type==ps_instr ||
		stack[i].type==ps_lit )
	    free(stack[i].u.str);
	else if ( stack[i].type==ps_array || stack[i].type==ps_dict )
	    dictfree(&stack[i].u.dict);
    }
}

static void DoMatTransform(int tok,int sp,struct psstack *stack) {
    real invt[6], t[6];

    if ( stack[sp-1].u.dict.cnt==6 && stack[sp-1].u.dict.entries[0].type==ps_num ) {
	double x = stack[sp-3].u.val, y = stack[sp-2].u.val;
	--sp;
	t[5] = stack[sp].u.dict.entries[5].u.val;
	t[4] = stack[sp].u.dict.entries[4].u.val;
	t[3] = stack[sp].u.dict.entries[3].u.val;
	t[2] = stack[sp].u.dict.entries[2].u.val;
	t[1] = stack[sp].u.dict.entries[1].u.val;
	t[0] = stack[sp].u.dict.entries[0].u.val;
	dictfree(&stack[sp].u.dict);
	if ( tok==pt_itransform || tok==pt_idtransform ) {
	    MatInverse(invt,t);
	    memcpy(t,invt,sizeof(t));
	}
	stack[sp-2].u.val = t[0]*x + t[1]*y;
	stack[sp-1].u.val = t[2]*x + t[3]*y;
	if ( tok==pt_transform || tok==pt_itransform ) {
	    stack[sp-2].u.val += t[4];
	    stack[sp-1].u.val += t[5];
	}
    }
}

static int DoMatOp(int tok,int sp,struct psstack *stack) {
    real temp[6], t[6];
    int nsp=sp;

    if ( stack[sp-1].u.dict.cnt==6 && stack[sp-1].u.dict.entries[0].type==ps_num ) {
	t[5] = stack[sp-1].u.dict.entries[5].u.val;
	t[4] = stack[sp-1].u.dict.entries[4].u.val;
	t[3] = stack[sp-1].u.dict.entries[3].u.val;
	t[2] = stack[sp-1].u.dict.entries[2].u.val;
	t[1] = stack[sp-1].u.dict.entries[1].u.val;
	t[0] = stack[sp-1].u.dict.entries[0].u.val;
	switch ( tok ) {
	  case pt_translate:
	    if ( sp>=3 ) {
		stack[sp-1].u.dict.entries[5].u.val += stack[sp-3].u.val*t[0]+stack[sp-2].u.val*t[2];
		stack[sp-1].u.dict.entries[4].u.val += stack[sp-3].u.val*t[1]+stack[sp-2].u.val*t[3];
		nsp = sp-2;
	    }
	  break;
	  case pt_scale:
	    if ( sp>=2 ) {
		stack[sp-1].u.dict.entries[0].u.val *= stack[sp-3].u.val;
		stack[sp-1].u.dict.entries[1].u.val *= stack[sp-3].u.val;
		stack[sp-1].u.dict.entries[2].u.val *= stack[sp-2].u.val;
		stack[sp-1].u.dict.entries[3].u.val *= stack[sp-2].u.val;
		/* transform[4,5] are unchanged */
		nsp = sp-2;
	    }
	  break;
	  case pt_rotate:
	    if ( sp>=1 ) {
		--sp;
		temp[0] = temp[3] = cos(stack[sp].u.val);
		temp[1] = sin(stack[sp].u.val);
		temp[2] = -temp[1];
		temp[4] = temp[5] = 0;
		MatMultiply(temp,t,t);
		stack[sp-1].u.dict.entries[5].u.val = t[5];
		stack[sp-1].u.dict.entries[4].u.val = t[4];
		stack[sp-1].u.dict.entries[3].u.val = t[3];
		stack[sp-1].u.dict.entries[2].u.val = t[2];
		stack[sp-1].u.dict.entries[1].u.val = t[1];
		stack[sp-1].u.dict.entries[0].u.val = t[0];
		nsp = sp-1;
	    }
	  break;
	}
	stack[nsp-1] = stack[sp-1];
    }
return(nsp);
}

static Entity *EntityCreate(SplinePointList *head,int linecap,int linejoin,
	real linewidth, real *transform) {
    Entity *ent = gcalloc(1,sizeof(Entity));
    ent->type = et_splines;
    ent->u.splines.splines = head;
    ent->u.splines.cap = linecap;
    ent->u.splines.join = linejoin;
    ent->u.splines.stroke_width = linewidth;
    ent->u.splines.fill.col = 0xffffffff;
    ent->u.splines.stroke.col = 0xffffffff;
    memcpy(ent->u.splines.transform,transform,6*sizeof(real));
return( ent );
}

static void InterpretPS(FILE *ps, EntityChar *ec) {
    SplinePointList *cur=NULL, *head=NULL;
    BasePoint current, temp;
    int tok, i, j;
    struct psstack stack[100];
    real dval;
    int sp=0;
    SplinePoint *pt;
    RefChar *ref, *lastref=NULL;
    real transform[6], t[6];
    real transstack[30][6];
    int tsp = 0;
    int ccnt=0;
    char tokbuf[100];
    IO wrapper;
    GrowBuf gb;
    struct pskeydict dict;
    struct pskeyval *kv;
    Color fore=0;
    int linecap=lc_butt, linejoin=lj_miter; real linewidth=1;
    Entity *ent;
    char *oldloc;
    int warned = 0;

    oldloc = setlocale(LC_NUMERIC,"C");

    wrapper.top = NULL;
    pushio(&wrapper,ps,NULL);

    memset(&gb,'\0',sizeof(GrowBuf));
    memset(&dict,'\0',sizeof(dict));

    memset(ec,'\0',sizeof(EntityChar));
    transform[0] = transform[3] = 1.0;
    transform[1] = transform[2] = transform[4] = transform[5] = 0;
    current.x = current.y = 0;

    while ( (tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf)))!=pt_eof ) {
	if ( ccnt>0 ) {
	    if ( tok==pt_closecurly )
		--ccnt;
	    else if ( tok==pt_opencurly )
		++ccnt;
	    if ( ccnt>0 )
		AddTok(&gb,tokbuf,tok==pt_namelit);
	    else {
		if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		    *gb.pt = '\0'; gb.pt = gb.base;
		    stack[sp].type = ps_instr;
		    stack[sp++].u.str = copy(gb.base);
		}
	    }
	} else if ( tok==pt_unknown && (kv=lookup(&dict,tokbuf))!=NULL ) {
	    if ( kv->type == ps_instr )
		pushio(&wrapper,NULL,copy(kv->u.str));
	    else if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = kv->type;
		stack[sp++].u = kv->u;
		if ( kv->type==ps_instr || kv->type==ps_lit || kv->type==ps_string )
		    stack[sp-1].u.str = copy(stack[sp-1].u.str);
		else if ( kv->type==ps_array || kv->type==ps_dict ) {
		    copyarray(&stack[sp-1].u.dict,&stack[sp-1].u.dict);
		    if ( stack[sp-1].u.dict.is_executable )
			sp = aload(sp,stack,sizeof(stack)/sizeof(stack[0]));
		}
	    }
	} else {
#if 0	/* /ps{count /foo exch def count copy foo array astore ==}def */
	/* printstack */
int ii;
for ( ii=0; ii<sp; ++ii )
 if ( stack[ii].type==ps_num )
  printf( "%g ", stack[ii].u.val );
 else if ( stack[ii].type==ps_bool )
  printf( "%s ", stack[ii].u.tf ? "true" : "false" );
 else if ( stack[ii].type==ps_string )
  printf( "(%s) ", stack[ii].u.str );
 else if ( stack[ii].type==ps_lit )
  printf( "/%s ", stack[ii].u.str );
 else if ( stack[ii].type==ps_instr )
  printf( "-%s- ", stack[ii].u.str );
 else if ( stack[ii].type==ps_mark )
  printf( "--mark-- " );
 else if ( stack[ii].type==ps_array )
  printf( "--[]-- " );
 else
  printf( "--???-- " );
printf( "-%s-\n", toknames[tok]);
#endif
	switch ( tok ) {
	  case pt_number:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = dval;
	    }
	  break;
	  case pt_string:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_string;
		stack[sp++].u.str = copyn(tokbuf+1,strlen(tokbuf)-2);
	    }
	  break;
	  case pt_true: case pt_false:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_bool;
		stack[sp++].u.tf = tok==pt_true;
	    }
	  break;
	  case pt_opencurly:
	    ++ccnt;
	  break;
	  case pt_closecurly:
	    --ccnt;
	    if ( ccnt<0 ) {
 goto done;
	    }
	  break;
	  case pt_count:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp].u.val = sp;
		++sp;
	    }
	  break;
	  case pt_pop:
	    if ( sp>0 ) {
		--sp;
		if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
			stack[sp].type==ps_lit )
		    free(stack[sp].u.str);
		else if ( stack[sp].type==ps_array || stack[sp].type==ps_dict )
		    dictfree(&stack[sp].u.dict);
	    }
	  break;
	  case pt_clear:
	    while ( sp>0 ) {
		--sp;
		if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
			stack[sp].type==ps_lit )
		    free(stack[sp].u.str);
		else if ( stack[sp].type==ps_array || stack[sp].type==ps_dict )
		    dictfree(&stack[sp].u.dict);
	    }
	  break;
	  case pt_dup:
	    if ( sp>0 && sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp] = stack[sp-1];
		if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
			stack[sp].type==ps_lit )
		    stack[sp].u.str = copy(stack[sp].u.str);
    /* The following is incorrect behavior, but as I don't do garbage collection */
    /*  and I'm not going to implement reference counts, this will work in most cases */
		else if ( stack[sp].type==ps_array )
		    copyarray(&stack[sp].u.dict,&stack[sp].u.dict);
		++sp;
	    }
	  break;
	  case pt_copy:
	    if ( sp>0 ) {
		int n = stack[--sp].u.val;
		if ( n+sp<sizeof(stack)/sizeof(stack[0]) ) {
		    int i;
		    for ( i=0; i<n; ++i ) {
			stack[sp] = stack[sp-n];
			if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
				stack[sp].type==ps_lit )
			    stack[sp].u.str = copy(stack[sp].u.str);
    /* The following is incorrect behavior, but as I don't do garbage collection */
    /*  and I'm not going to implement reference counts, this will work in most cases */
			else if ( stack[sp].type==ps_array )
			    copyarray(&stack[sp].u.dict,&stack[sp].u.dict);
			++sp;
		    }
		}
	    }
	  break;
	  case pt_exch:
	    if ( sp>1 ) {
		struct psstack temp;
		temp = stack[sp-1];
		stack[sp-1] = stack[sp-2];
		stack[sp-2] = temp;
	    }
	  break;
	  case pt_roll:
	    sp = rollstack(stack,sp);
	  break;
	  case pt_index:
	    if ( sp>0 ) {
		i = stack[--sp].u.val;
		if ( sp>=i && i>0 ) {
		    stack[sp] = stack[sp-i];
		    if ( stack[sp].type==ps_string || stack[sp].type==ps_instr ||
			    stack[sp].type==ps_lit )
			stack[sp].u.str = copy(stack[sp].u.str);
    /* The following is incorrect behavior, but as I don't do garbage collection */
    /*  and I'm not going to implement reference counts, this will work in most cases */
		    else if ( stack[sp].type==ps_array )
			copyarray(&stack[sp].u.dict,&stack[sp].u.dict);
		    ++sp;
		}
	    }
	  break;
	  case pt_add:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val += stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_sub:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val -= stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_mul:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val *= stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_div:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val /= stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_idiv:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val = ((int) stack[sp-2].u.val) / ((int) stack[sp-1].u.val);
		--sp;
	    }
	  break;
	  case pt_mod:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		stack[sp-2].u.val = ((int) stack[sp-2].u.val) % ((int) stack[sp-1].u.val);
		--sp;
	    }
	  break;
	  case pt_max:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		if ( stack[sp-2].u.val < stack[sp-1].u.val )
		    stack[sp-2].u.val = stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_min:
	    if ( sp>=2 && stack[sp-1].type==ps_num && stack[sp-2].type==ps_num ) {
		if ( stack[sp-2].u.val > stack[sp-1].u.val )
		    stack[sp-2].u.val = stack[sp-1].u.val;
		--sp;
	    }
	  break;
	  case pt_neg:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num )
		    stack[sp-1].u.val = -stack[sp-1].u.val;
	    }
	  break;
	  case pt_abs:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num )
		    if ( stack[sp-1].u.val < 0 )
			stack[sp-1].u.val = -stack[sp-1].u.val;
	    }
	  break;
	  case pt_round:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num )
		    stack[sp-1].u.val = rint(stack[sp-1].u.val);
		    /* rint isn't quite right, round will take 6.5 to 7, 5.5 to 6, etc. while rint() will take both to 6 */
	    }
	  break;
	  case pt_floor:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num )
		    stack[sp-1].u.val = floor(stack[sp-1].u.val);
	    }
	  break;
	  case pt_ceiling:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num )
		    stack[sp-1].u.val = ceil(stack[sp-1].u.val);
	    }
	  break;
	  case pt_truncate:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_num ) {
		    if ( stack[sp-1].u.val<0 )
			stack[sp-1].u.val = ceil(stack[sp-1].u.val);
		    else
			stack[sp-1].u.val = floor(stack[sp-1].u.val);
		}
	    }
	  break;
	  case pt_ne: case pt_eq:
	    if ( sp>=2 ) {
		if ( stack[sp-2].type!=stack[sp-1].type )
		    stack[sp-2].u.tf = false;
		else if ( stack[sp-2].type==ps_num )
		    stack[sp-2].u.tf = (stack[sp-2].u.val == stack[sp-1].u.val);
		else if ( stack[sp-2].type==ps_bool )
		    stack[sp-2].u.tf = (stack[sp-2].u.tf == stack[sp-1].u.tf);
		else
		    stack[sp-2].u.tf = strcmp(stack[sp-2].u.str,stack[sp-1].u.str)==0 ;
		stack[sp-2].type = ps_bool;
		if ( tok==pt_ne ) stack[sp-2].u.tf = !stack[sp-2].u.tf;
		--sp;
	    }
	  break;
	  case pt_gt: case pt_le: case pt_lt: case pt_ge:
	    if ( sp>=2 ) {
		if ( stack[sp-2].type!=stack[sp-1].type )
		    stack[sp-2].u.tf = false;
		else if ( stack[sp-2].type==ps_array )
		    fprintf( stderr, "Can't compare arrays\n" );
		else {
		    int cmp;
		    if ( stack[sp-2].type==ps_num )
			cmp = (stack[sp-2].u.val > stack[sp-1].u.val)?1:
				(stack[sp-2].u.val == stack[sp-1].u.val)?0:-1;
		    else if ( stack[sp-2].type==ps_bool )
			cmp = (stack[sp-2].u.tf - stack[sp-1].u.tf);
		    else
			cmp = strcmp(stack[sp-2].u.str,stack[sp-1].u.str);
		    if ( tok==pt_gt )
			stack[sp-2].u.tf = cmp>0;
		    else if ( tok==pt_lt )
			stack[sp-2].u.tf = cmp<0;
		    else if ( tok==pt_le )
			stack[sp-2].u.tf = cmp<=0;
		    else
			stack[sp-2].u.tf = cmp>=0;
		}
		stack[sp-2].type = ps_bool;
		--sp;
	    }
	  break;
	  case pt_not:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type == ps_bool )
		    stack[sp-1].u.tf = !stack[sp-1].u.tf;
	    }
	  break;
	  case pt_and:
	    if ( sp>=2 ) {
		if ( stack[sp-2].type == ps_num )
		    stack[sp-2].u.val = ((int) stack[sp-1].u.val) & (int) stack[sp-1].u.val;
		else if ( stack[sp-2].type == ps_bool )
		    stack[sp-2].u.tf &= stack[sp-1].u.tf;
		--sp;
	    }
	  break;
	  case pt_or:
	    if ( sp>=2 ) {
		if ( stack[sp-2].type == ps_num )
		    stack[sp-2].u.val = ((int) stack[sp-1].u.val) | (int) stack[sp-1].u.val;
		else if ( stack[sp-2].type == ps_bool )
		    stack[sp-2].u.tf |= stack[sp-1].u.tf;
		--sp;
	    }
	  break;
	  case pt_xor:
	    if ( sp>=2 ) {
		if ( stack[sp-2].type == ps_num )
		    stack[sp-2].u.val = ((int) stack[sp-1].u.val) ^ (int) stack[sp-1].u.val;
		else if ( stack[sp-2].type == ps_bool )
		    stack[sp-2].u.tf ^= stack[sp-1].u.tf;
		--sp;
	    }
	  break;
	  case pt_if:
	    if ( sp>=2 ) {
		if ( ((stack[sp-2].type == ps_bool && stack[sp-2].u.tf) ||
			(stack[sp-2].type == ps_num && strstr(stack[sp-1].u.str,"setcachedevice")!=NULL)) &&
			stack[sp-1].type==ps_instr )
		    pushio(&wrapper,NULL,stack[sp-1].u.str);
		if ( stack[sp-1].type==ps_string || stack[sp-1].type==ps_instr || stack[sp-1].type==ps_lit )
		    free(stack[sp-1].u.str);
		sp -= 2;
	    } else if ( sp==1 && stack[sp-1].type==ps_instr ) {
		/*This can happen when reading our type3 fonts, we get passed */
		/* values on the stack which the interpreter knows nothing */
		/* about, but the interp needs to learn the width of the char */
		if ( strstr(stack[sp-1].u.str,"setcachedevice")!=NULL ||
			strstr(stack[sp-1].u.str,"setcharwidth")!=NULL )
		    pushio(&wrapper,NULL,stack[sp-1].u.str);
		free(stack[sp-1].u.str);
		sp = 0;
	    }
	  break;
	  case pt_ifelse:
	    if ( sp>=3 ) {
		if ( stack[sp-3].type == ps_bool && stack[sp-3].u.tf ) {
		    if ( stack[sp-2].type==ps_instr )
			pushio(&wrapper,NULL,stack[sp-2].u.str);
		} else {
		    if ( stack[sp-1].type==ps_instr )
			pushio(&wrapper,NULL,stack[sp-1].u.str);
		}
		if ( stack[sp-1].type==ps_string || stack[sp-1].type==ps_instr || stack[sp-1].type==ps_lit )
		    free(stack[sp-1].u.str);
		if ( stack[sp-2].type==ps_string || stack[sp-2].type==ps_instr || stack[sp-2].type==ps_lit )
		    free(stack[sp-2].u.str);
		sp -= 3;
	    }
	  break;
	  case pt_load:
	    if ( sp>=1 && stack[sp-1].type==ps_lit ) {
		kv = lookup(&dict,stack[sp-1].u.str);
		if ( kv!=NULL ) {
		    free( stack[sp-1].u.str );
		    stack[sp-1].type = kv->type;
		    stack[sp-1].u = kv->u;
		    if ( kv->type==ps_instr || kv->type==ps_lit )
			stack[sp-1].u.str = copy(stack[sp-1].u.str);
		} else
		    stack[sp-1].type = ps_instr;
	    }
	  break;
	  case pt_def:
	    sp = AddEntry(&dict,stack,sp);
	  break;
	  case pt_bind:
	    /* a noop in this context */
	  break;
	  case pt_setcachedevice:
	    if ( sp>=6 ) {
		ec->width = stack[sp-6].u.val;
		/* I don't care about the bounding box */
		/* I don't support a "height" (ie. for CJK vertical spacing) */
		sp-=6;
	    }
	  break;
	  case pt_setcharwidth:
	    if ( sp>=1 )
		ec->width = stack[--sp].u.val;
	  break;
	  case pt_translate:
	    if ( sp>=1 && stack[sp-1].type==ps_array )
		sp = DoMatOp(tok,sp,stack);
	    else if ( sp>=2 ) {
		transform[4] += stack[sp-2].u.val*transform[0]+stack[sp-1].u.val*transform[2];
		transform[5] += stack[sp-2].u.val*transform[1]+stack[sp-1].u.val*transform[3];
		sp -= 2;
	    }
	  break;
	  case pt_scale:
	    if ( sp>=1 && stack[sp-1].type==ps_array )
		sp = DoMatOp(tok,sp,stack);
	    else if ( sp>=2 ) {
		transform[0] *= stack[sp-2].u.val;
		transform[1] *= stack[sp-2].u.val;
		transform[2] *= stack[sp-1].u.val;
		transform[3] *= stack[sp-1].u.val;
		/* transform[4,5] are unchanged */
		sp -= 2;
	    }
	  break;
	  case pt_rotate:
	    if ( sp>=1 && stack[sp-1].type==ps_array )
		sp = DoMatOp(tok,sp,stack);
	    else if ( sp>=1 ) {
		--sp;
		t[0] = t[3] = cos(stack[sp].u.val);
		t[1] = sin(stack[sp].u.val);
		t[2] = -t[1];
		t[4] = t[5] = 0;
		MatMultiply(t,transform,transform);
	    }
	  break;
	  case pt_concat:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type==ps_array ) {
		    if ( stack[sp-1].u.dict.cnt==6 && stack[sp-1].u.dict.entries[0].type==ps_num ) {
			--sp;
			t[5] = stack[sp].u.dict.entries[5].u.val;
			t[4] = stack[sp].u.dict.entries[4].u.val;
			t[3] = stack[sp].u.dict.entries[3].u.val;
			t[2] = stack[sp].u.dict.entries[2].u.val;
			t[1] = stack[sp].u.dict.entries[1].u.val;
			t[0] = stack[sp].u.dict.entries[0].u.val;
			dictfree(&stack[sp].u.dict);
			MatMultiply(t,transform,transform);
		    }
		}
	    }
	  break;
	  case pt_transform:
	    if ( sp>=1 && stack[sp-1].type==ps_array ) {
		if ( sp>=3 ) {
		    DoMatTransform(tok,sp,stack);
		    --sp;
		}
	    } else if ( sp>=2 ) {
		double x = stack[sp-2].u.val, y = stack[sp-1].u.val;
		stack[sp-2].u.val = transform[0]*x + transform[1]*y + transform[4];
		stack[sp-1].u.val = transform[2]*x + transform[3]*y + transform[5];
	    }
	  break;
	  case pt_itransform:
	    if ( sp>=1 && stack[sp-1].type==ps_array ) {
		if ( sp>=3 ) {
		    DoMatTransform(tok,sp,stack);
		    --sp;
		}
	    } else if ( sp>=2 ) {
		double x = stack[sp-2].u.val-transform[4], y = stack[sp-1].u.val-transform[5];
		MatInverse(t,transform);
		stack[sp-2].u.val = t[0]*x + t[1]*y + t[4];
		stack[sp-1].u.val = t[2]*x + t[3]*y + t[5];
	    }
	  case pt_dtransform:
	    if ( sp>=1 && stack[sp-1].type==ps_array ) {
		if ( sp>=3 ) {
		    DoMatTransform(tok,sp,stack);
		    --sp;
		}
	    } else if ( sp>=2 ) {
		double x = stack[sp-2].u.val, y = stack[sp-1].u.val;
		stack[sp-2].u.val = transform[0]*x + transform[1]*y;
		stack[sp-1].u.val = transform[2]*x + transform[3]*y;
	    }
	  break;
	  case pt_idtransform:
	    if ( sp>=1 && stack[sp-1].type==ps_array ) {
		if ( sp>=3 ) {
		    DoMatTransform(tok,sp,stack);
		    --sp;
		}
	    } else if ( sp>=2 ) {
		double x = stack[sp-2].u.val, y = stack[sp-1].u.val;
		MatInverse(t,transform);
		stack[sp-2].u.val = t[0]*x + t[1]*y;
		stack[sp-1].u.val = t[2]*x + t[3]*y;
	    }
	  break;
	  case pt_namelit:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_lit;
		stack[sp++].u.str = copy(tokbuf);
	    }
	  break;
	  case pt_exec:
	    if ( sp>0 && stack[sp-1].type == ps_lit ) {
		ref = chunkalloc(sizeof(RefChar));
		ref->sc = (SplineChar *) stack[--sp].u.str;
		memcpy(ref->transform,transform,sizeof(transform));
		if ( ec->refs==NULL )
		    ec->refs = ref;
		else
		    lastref->next = ref;
		lastref = ref;
	    }
	  break;
	  case pt_newpath:
	  break;
	  case pt_lineto: case pt_rlineto:
	  case pt_moveto: case pt_rmoveto:
	    if ( sp>=2 || tok==pt_newpath ) {
		if ( tok==pt_rlineto || tok==pt_rmoveto ) {
		    current.x += stack[sp-2].u.val;
		    current.y += stack[sp-1].u.val;
		    sp -= 2;
		} else if ( tok==pt_lineto || tok == pt_moveto ) {
		    current.x = stack[sp-2].u.val;
		    current.y = stack[sp-1].u.val;
		    sp -= 2;
		}
		pt = chunkalloc(sizeof(SplinePoint));
		Transform(&pt->me,&current,transform);
		pt->noprevcp = true; pt->nonextcp = true;
		if ( tok==pt_moveto || tok==pt_rmoveto ) {
		    SplinePointList *spl = chunkalloc(sizeof(SplinePointList));
		    spl->first = spl->last = pt;
		    if ( cur!=NULL )
			cur->next = spl;
		    else
			head = spl;
		    cur = spl;
		} else {
		    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
			SplineMake3(cur->last,pt);
			cur->last = pt;
		    }
		}
	    } else
		sp = 0;
	  break;
	  case pt_curveto: case pt_rcurveto:
	    if ( sp>=6 ) {
		if ( tok==pt_rcurveto ) {
		    stack[sp-1].u.val += current.y;
		    stack[sp-3].u.val += current.y;
		    stack[sp-5].u.val += current.y;
		    stack[sp-2].u.val += current.x;
		    stack[sp-4].u.val += current.x;
		    stack[sp-6].u.val += current.x;
		}
		current.x = stack[sp-2].u.val;
		current.y = stack[sp-1].u.val;
		if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
		    temp.x = stack[sp-6].u.val; temp.y = stack[sp-5].u.val;
		    Transform(&cur->last->nextcp,&temp,transform);
		    cur->last->nonextcp = false;
		    pt = chunkalloc(sizeof(SplinePoint));
		    temp.x = stack[sp-4].u.val; temp.y = stack[sp-3].u.val;
		    Transform(&pt->prevcp,&temp,transform);
		    Transform(&pt->me,&current,transform);
		    pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		}
		sp -= 6;
	    } else
		sp = 0;
	  break;
	  case pt_arc: case pt_arcn:
	    if ( sp>=5 ) {
		real cx, cy, r, a1, a2;
		cx = stack[sp-5].u.val;
		cy = stack[sp-4].u.val;
		r = stack[sp-3].u.val;
		a1 = stack[sp-2].u.val;
		a2 = stack[sp-1].u.val;
		sp -= 5;
		temp.x = cx+r*cos(a1/180 * 3.1415926535897932);
		temp.y = cy+r*sin(a1/180 * 3.1415926535897932);
		if ( temp.x!=current.x || temp.y!=current.y ) {
		    pt = chunkalloc(sizeof(SplinePoint));
		    Transform(&pt->me,&temp,transform);
		    pt->noprevcp = true; pt->nonextcp = true;
		    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
			SplineMake3(cur->last,pt);
			cur->last = pt;
		    } else {	/* if no current point, then start here */
			SplinePointList *spl = chunkalloc(sizeof(SplinePointList));
			spl->first = spl->last = pt;
			if ( cur!=NULL )
			    cur->next = spl;
			else
			    head = spl;
			cur = spl;
		    }
		}
		circlearcsto(a1,a2,cx,cy,r,cur,transform,tok==pt_arcn);
		current.x = cx+r*cos(a2/180 * 3.1415926535897932);
		current.y = cy+r*sin(a2/180 * 3.1415926535897932);
	    } else
		sp = 0;
	  break;
	  case pt_arct: case pt_arcto:
	    if ( sp>=5 ) {
		real x1, y1, x2, y2, r;
		real xt1, xt2, yt1, yt2;
		x1 = stack[sp-5].u.val;
		y1 = stack[sp-4].u.val;
		x2 = stack[sp-3].u.val;
		y2 = stack[sp-2].u.val;
		r = stack[sp-1].u.val;
		sp -= 5;

		xt1 = xt2 = x1; yt1 = yt2 = y1;
		if ( cur==NULL || cur->first==NULL || (cur->first==cur->last && cur->first->next!=NULL) )
		    /* Error */;
		else if ( current.x==x1 && current.y==y1 )
		    /* Error */;
		else if (( x1==x2 && y1==y2 ) ||
			(current.x-x1)*(y2-y1) == (x2-x1)*(current.y-y1) ) {
		    /* Degenerate case */
		    current.x = x1; current.y = y1;
		    pt = chunkalloc(sizeof(SplinePoint));
		    Transform(&pt->me,&current,transform);
		    pt->noprevcp = true; pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		} else {
		    real l1 = sqrt((current.x-x1)*(current.x-x1)+(current.y-y1)*(current.y-y1));
		    real l2 = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
		    real dx = ((current.x-x1)/l1 + (x2-x1)/l2);
		    real dy = ((current.y-y1)/l1 + (y2-y1)/l2);
		    /* the line from (x1,y1) to (x1+dx,y1+dy) contains the center*/
		    real l3 = sqrt(dx*dx+dy*dy);
		    real cx, cy, t, tmid;
		    real a1, amid, a2;
		    int clockwise = true;
		    dx /= l3; dy /= l3;
		    a1 = atan2(current.y-y1,current.x-x1);
		    a2 = atan2(y2-y1,x2-x1);
		    amid = atan2(dy,dx) - a1;
		    tmid = r/sin(amid);
		    t = r/tan(amid);
		    if ( t<0 ) {
			clockwise = false;
			t = -t;
			tmid = -tmid;
		    }
		    cx = x1+ tmid*dx; cy = y1 + tmid*dy;
		    xt1 = x1 + t*(current.x-x1)/l1; yt1 = y1 + t*(current.y-y1)/l1;
		    xt2 = x1 + t*(x2-x1)/l2; yt2 = y1 + t*(y2-y1)/l2;
		    if ( xt1!=current.x || yt1!=current.y ) {
			BasePoint temp;
			temp.x = xt1; temp.y = yt1;
			pt = chunkalloc(sizeof(SplinePoint));
			Transform(&pt->me,&temp,transform);
			pt->noprevcp = true; pt->nonextcp = true;
			SplineMake3(cur->last,pt);
			cur->last = pt;
		    }
		    a1 = 3*3.1415926535897932/2+a1;
		    a2 = 3.1415926535897932/2+a2;
		    if ( !clockwise ) {
			a1 += 3.1415926535897932;
			a2 += 3.1415926535897932;
		    }
		    circlearcsto(a1*180/3.1415926535897932,a2*180/3.1415926535897932,
			    cx,cy,r,cur,transform,clockwise);
		}
		if ( tok==pt_arcto ) {
		    stack[sp].type = stack[sp+1].type = stack[sp+2].type = stack[sp+3].type = ps_num;
		    stack[sp++].u.val = xt1;
		    stack[sp++].u.val = yt1;
		    stack[sp++].u.val = xt2;
		    stack[sp++].u.val = yt2;
		}
		current.x = xt2; current.y = yt2;
	    }
	  break;
	  case pt_closepath:
	    if ( cur!=NULL && cur->first!=NULL && cur->first!=cur->last ) {
		if ( cur->first->me.x==cur->last->me.x && cur->first->me.y==cur->last->me.y ) {
		    SplinePoint *oldlast = cur->last;
		    cur->first->prevcp = oldlast->prevcp;
		    cur->first->noprevcp = false;
		    oldlast->prev->from->next = NULL;
		    cur->last = oldlast->prev->from;
		    SplineFree(oldlast->prev);
		    SplinePointFree(oldlast);
		}
		SplineMake3(cur->last,cur->first);
		cur->last = cur->first;
	    }
	  break;
	  case pt_setlinecap:
	    if ( sp>=1 )
		linecap = stack[--sp].u.val;
	  break;
	  case pt_setlinejoin:
	    if ( sp>=1 )
		linejoin = stack[--sp].u.val;
	  break;
	  case pt_setlinewidth:
	    if ( sp>=1 )
		linewidth = stack[--sp].u.val;
	  break;
	  case pt_currentlinecap: case pt_currentlinejoin:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = tok==pt_currentlinecap?linecap:linejoin;
	    }
	  break;
	  case pt_currentlinewidth:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = linewidth;
	    }
	  break;
	  case pt_currentgray:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = (3*((fore>>16)&0xff) + 6*((fore>>8)&0xff) + (fore&0xff))/2550.;
	    }
	  break;
	  case pt_setgray:
	    if ( sp>=1 ) {
		fore = stack[--sp].u.val*255;
		fore *= 0x010101;
	    }
	  break;
	  case pt_setrgbcolor:
	    if ( sp>=3 ) {
		fore = (((int) (stack[sp-3].u.val*255))<<16) +
			(((int) (stack[sp-2].u.val*255))<<8) +
			(int) (stack[sp-1].u.val*255);
		sp -= 3;
	    }
	  break;
	  case pt_currenthsbcolor: case pt_currentrgbcolor:
	    if ( sp+2<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = stack[sp+1].type = stack[sp+2].type = ps_num;
		if ( tok==pt_currentrgbcolor ) {
		    stack[sp++].u.val = ((fore>>16)&0xff)/255.;
		    stack[sp++].u.val = ((fore>>8)&0xff)/255.;
		    stack[sp++].u.val = (fore&0xff)/255.;
		} else {
		    int r=fore>>16, g=(fore>>8)&0xff, bl=fore&0xff;
		    int mx, mn;
		    real h, s, b;
		    mx = mn = r;
		    if ( mx>g ) mn=g; else mx=g;
		    if ( mx<bl ) mx = bl; if ( mn>bl ) mn = bl;
		    b = mx/255.;
		    s = h = 0;
		    if ( mx>0 )
			s = ((real) (mx-mn))/mx;
		    if ( s!=0 ) {
			real rdiff = ((real) (mx-r))/(mx-mn);
			real gdiff = ((real) (mx-g))/(mx-mn);
			real bdiff = ((real) (mx-bl))/(mx-mn);
			if ( rdiff==0 )
			    h = bdiff-gdiff;
			else if ( gdiff==0 )
			    h = 2 + rdiff-bdiff;
			else
			    h = 4 + gdiff-rdiff;
			h /= 6;
			if ( h<0 ) h += 1;
		    }
		    stack[sp++].u.val = h;
		    stack[sp++].u.val = s;
		    stack[sp++].u.val = b;
		}
	    }
	  break;
	  case pt_sethsbcolor:
	    if ( sp>=3 ) {
		real h = stack[sp-3].u.val, s = stack[sp-2].u.val, b = stack[sp-1].u.val;
		int r,g,bl;
		if ( s==0 )	/* it's grey */
		    fore = ((int) (b*255)) * 0x010101;
		else {
		    real sextant = (h-floor(h))*6;
		    real mod = sextant-floor(sextant);
		    real p = b*(1-s), q = b*(1-s*mod), t = b*(1-s*(1-mod));
		    switch( (int) sextant) {
		      case 0:
			r = b*255.; g = t*255.; bl = p*255.;
		      break;
		      case 1:
			r = q*255.; g = b*255.; bl = p*255.;
		      break;
		      case 2:
			r = p*255.; g = b*255.; bl = t*255.;
		      break;
		      case 3:
			r = p*255.; g = q*255.; bl = b*255.;
		      break;
		      case 4:
			r = t*255.; g = p*255.; bl = b*255.;
		      break;
		      case 5:
			r = b*255.; g = p*255.; bl = q*255.;
		      break;
		    }
		    fore = COLOR_CREATE(r,g,bl);
		}
		sp -= 3;
	    }
	  break;
	  case pt_currentcmykcolor:
	    if ( sp+3<sizeof(stack)/sizeof(stack[0]) ) {
		real c,m,y,k;
		stack[sp].type = stack[sp+1].type = stack[sp+2].type = stack[sp+3].type = ps_num;
		y = 1.-(fore&0xff)/255.;
		m = 1.-((fore>>8)&0xff)/255.;
		c = 1.-((fore>>16)&0xff)/255.;
		k = y; if ( k>m ) k=m; if ( k>c ) k=c;
		if ( k!=1 ) {
		    y = (y-k)/(1-k);
		    m = (m-k)/(1-k);
		    c = (c-k)/(1-k);
		} else
		    y = m = c = 0;
		stack[sp++].u.val = c;
		stack[sp++].u.val = m;
		stack[sp++].u.val = y;
		stack[sp++].u.val = k;
	    }
	  break;
	  case pt_setcmykcolor:
	    if ( sp>=4 ) {
		real c=stack[sp-4].u.val,m=stack[sp-3].u.val,y=stack[sp-2].u.val,k=stack[sp-1].u.val;
		sp -= 4;
		if ( k==1 )
		    fore = 0x000000;
		else {
		    if (( y = (1-k)*y+k )<0 ) y=0; else if ( y>1 ) y=1;
		    if (( m = (1-k)*m+k )<0 ) m=0; else if ( m>1 ) m=1;
		    if (( c = (1-k)*c+k )<0 ) c=0; else if ( c>1 ) c=1;
		    fore = ((int) ((1-c)*255.)<<16) |
			    ((int) ((1-m)*255.)<<8) |
			    ((int) ((1-y)*255.));
		}
	    }
	  break;
	  case pt_fill: case pt_stroke:
	    if ( head==NULL && ec->splines!=NULL ) {
		/* assume they did a "gsave fill grestore stroke" (or reverse)*/
		ent = ec->splines;
		if ( tok==pt_stroke ) {
		    ent->u.splines.cap = linecap; ent->u.splines.join = linejoin;
		    ent->u.splines.stroke_width = linewidth;
		    memcpy(ent->u.splines.transform,transform,sizeof(transform));
		}
	    } else {
		ent = EntityCreate(head,linecap,linejoin,linewidth,transform);
		ent->next = ec->splines;
		ec->splines = ent;
	    }
	    if ( tok==pt_fill )
		ent->u.splines.fill.col = fore;
	    else
		ent->u.splines.stroke.col = fore;
	    head = NULL; cur = NULL;
	  break;

	  /* We don't do these right, but at least we'll avoid some errors with this hack */
	  case pt_save: case pt_currentmatrix:
	    /* push some junk on the stack */
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = 0;
	    }
	    if ( tsp<30 )
		memcpy(transstack[tsp++],transform,sizeof(transform));
	  break;
	  case pt_restore: case pt_setmatrix:
	    /* pop some junk off the stack */
	    if ( sp>=1 )
		--sp;
	    if ( tsp>0 )
		memcpy(transform,transstack[--tsp],sizeof(transform));
	  break;
	  case pt_gsave:
	    if ( tsp<30 )
		memcpy(transstack[tsp++],transform,sizeof(transform));
	  break;
	  case pt_grestore:
	    if ( tsp>0 )
		memcpy(transform,transstack[--tsp],sizeof(transform));
	  break;
	  case pt_setmiterlimit:
	    /* pop some junk off the stack */
	    if ( sp>=1 )
		--sp;
	  break;
	  case pt_setdash:
	    /* pop some junk off the stack */
	    if ( sp>=1 )
		--sp;
	  break;

	  case pt_openarray: case pt_mark:
	    if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp++].type = ps_mark;
	    }
	  break;
	  case pt_counttomark:
	    for ( i=0; i<sp; ++i )
		if ( stack[sp-1-i].type==ps_mark )
	    break;
	    if ( i==sp )
		fprintf( stderr, "No mark in counttomark\n" );
	    else if ( sp<sizeof(stack)/sizeof(stack[0]) ) {
		stack[sp].type = ps_num;
		stack[sp++].u.val = i;
	    }
	  break;
	  case pt_cleartomark:
	    for ( i=0; i<sp; ++i )
		if ( stack[sp-1-i].type==ps_mark )
	    break;
	    if ( i==sp )
		fprintf( stderr, "No mark in cleartomark\n" );
	    else
		sp = sp-i-1;
	  break;
	  case pt_closearray:
	    for ( i=0; i<sp; ++i )
		if ( stack[sp-1-i].type==ps_mark )
	    break;
	    if ( i==sp )
		fprintf( stderr, "No mark in ] (close array)\n" );
	    else {
		struct pskeydict dict;
		dict.cnt = dict.max = i;
		dict.entries = gcalloc(i,sizeof(struct pskeyval));
		for ( j=0; j<i; ++j ) {
		    dict.entries[j].type = stack[sp-i+j].type;
		    dict.entries[j].u = stack[sp-i+j].u;
		    /* don't need to copy because the things on the stack */
		    /*  are being popped (don't need to free either) */
		}
		sp = sp-i;
		stack[sp-1].type = ps_array;
		stack[sp-1].u.dict = dict;
	    }
	  break;
	  case pt_array:
	    if ( sp>=1 && stack[sp-1].type==ps_num ) {
		struct pskeydict dict;
		dict.cnt = dict.max = stack[sp-1].u.val;
		dict.entries = gcalloc(dict.cnt,sizeof(struct pskeyval));
		/* all entries are inited to void */
		stack[sp-1].type = ps_array;
		stack[sp-1].u.dict = dict;
	    }
	  break;
	  case pt_aload:
	    sp = aload(sp,stack,sizeof(stack)/sizeof(stack[0]));
	  break;
	  case pt_astore:
	    if ( sp>=1 && stack[sp-1].type==ps_array ) {
		struct pskeydict dict;
		--sp;
		dict = stack[sp].u.dict;
		if ( sp>=dict.cnt ) {
		    for ( i=dict.cnt-1; i>=0 ; --i ) {
			--sp;
			dict.entries[i].type = stack[sp].type;
			dict.entries[i].u = stack[sp].u;
		    }
		}
		stack[sp].type = ps_array;
		stack[sp].u.dict = dict;
		++sp;
	    }
	  break;

	  case pt_output: case pt_outputd: case pt_print:
	    if ( sp>=1 ) {
		--sp;
		switch ( stack[sp].type ) {
		  case ps_num:
		    printf( "%g", stack[sp].u.val );
		  break;
		  case ps_bool:
		    printf( "%s", stack[sp].u.tf ? "true" : "false" );
		  break;
		  case ps_string: case ps_instr: case ps_lit:
		    if ( tok==pt_outputd )
			printf( stack[sp].type==ps_lit ? "/" :
				stack[sp].type==ps_string ? "(" : "{" );
		    printf( "%s", stack[sp].u.str );
		    if ( tok==pt_outputd )
			printf( stack[sp].type==ps_lit ? "" :
				stack[sp].type==ps_string ? ")" : "}" );
		    free(stack[sp].u.str);
		  break;
		  case ps_void:
		    printf( "-- void --" );
		  break;
		  case ps_array:
		    if ( tok==pt_outputd ) {
			printarray(&stack[sp].u.dict);
			dictfree(&stack[sp].u.dict);
		  break;
		    } /* else fall through */
		    dictfree(&stack[sp].u.dict);
		  default:
		    printf( "-- nostringval --" );
		  break;
		}
		if ( tok==pt_output || tok==pt_outputd )
		    printf( "\n" );
	    } else
		fprintf(stderr, "Nothing on stack to print\n" );
	  break;

	  case pt_cvi: case pt_cvr:
	    /* I shan't distinguish between integers and reals */
	    if ( sp>=1 && stack[sp-1].type==ps_string ) {
		double val = strtod(stack[sp-1].u.str,NULL);
		free(stack[sp-1].u.str);
		stack[sp-1].u.val = val;
		stack[sp-1].type = ps_num;
	    }
	  break;
	  case pt_cvlit:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type==ps_array )
		    stack[sp-1].u.dict.is_executable = false;
	    }
	  case pt_cvn:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type==ps_string )
		    stack[sp-1].type = ps_lit;
	    }
	  case pt_cvx:
	    if ( sp>=1 ) {
		if ( stack[sp-1].type==ps_array )
		    stack[sp-1].u.dict.is_executable = true;
	    }
	  break;
	  case pt_cvrs:
	    if ( sp>=3 && stack[sp-1].type==ps_string &&
		    stack[sp-2].type==ps_num &&
		    stack[sp-3].type==ps_num ) {
		if ( stack[sp-2].u.val==8 )
		    sprintf( stack[sp-1].u.str, "%o", (int) stack[sp-3].u.val );
		else if ( stack[sp-2].u.val==16 )
		    sprintf( stack[sp-1].u.str, "%X", (int) stack[sp-3].u.val );
		else /* default to radix 10 no matter what they asked for */
		    sprintf( stack[sp-1].u.str, "%g", stack[sp-3].u.val );
		stack[sp-3] = stack[sp-1];
		sp-=2;
	    }
	  break;
	  case pt_cvs:
	    if ( sp>=2 && stack[sp-1].type==ps_string ) {
		switch ( stack[sp].type ) {
		  case ps_num:
		    sprintf( stack[sp-1].u.str, "%g", stack[sp-2].u.val );
		  break;
		  case ps_bool:
		    sprintf( stack[sp-1].u.str, "%s", stack[sp-2].u.tf ? "true" : "false" );
		  break;
		  case ps_string: case ps_instr: case ps_lit:
		    sprintf( stack[sp-1].u.str, "%s", stack[sp-2].u.str );
		    free(stack[sp].u.str);
		  break;
		  case ps_void:
		    printf( "-- void --" );
		  break;
		  case ps_array:
		    dictfree(&stack[sp].u.dict);
		  default:
		    sprintf( stack[sp-1].u.str, "-- nostringval --" );
		  break;
		}
		stack[sp-2] = stack[sp-1];
		--sp;
	    }
	  break;
	  case pt_stringop:	/* the string keyword, not the () thingy */
	    if ( sp>=1 && stack[sp-1].type==ps_num ) {
		stack[sp-1].type = ps_string;
		stack[sp-1].u.str = gcalloc(stack[sp-1].u.val+1,1);
	    }
	  break;

	  case pt_unknown:
	    if ( !warned ) {
		fprintf( stderr, "Warning: Unable to parse token %s, some features may be lost\n", tokbuf );
		warned = true;
	    }
	  break;

	  default:
	  break;
	}}
    }
 done:
    freestuff(stack,sp,&dict,&gb);
    if ( head!=NULL ) {
	ent = EntityCreate(head,linecap,linejoin,linewidth,transform);
	ent->next = ec->splines;
	ec->splines = ent;
    }
    ECCatagorizePoints(ec);
    setlocale(LC_NUMERIC,oldloc);
}

static SplinePointList *EraseStroke(SplineChar *sc,SplinePointList *head,SplinePointList *erase) {
    SplineSet *spl, *last;
    SplinePoint *sp;

    if ( head==NULL ) {
	/* Pointless, but legal */
	SplinePointListFree(erase);
return( NULL );
    }

    last = NULL;
    for ( spl=head; spl!=NULL; spl=spl->next ) {
	for ( sp=spl->first; sp!=NULL; ) {
	    sp->selected = false;
	    if ( sp->next==NULL )
	break;
	    sp = sp->next->to;
	    if ( sp==spl->first )
	break;
	}
	last = spl;
    }
    for ( spl=erase; spl!=NULL; spl=spl->next ) {
	for ( sp=spl->first; sp!=NULL; ) {
	    sp->selected = true;
	    if ( sp->next==NULL )
	break;
	    sp = sp->next->to;
	    if ( sp==spl->first )
	break;
	}
    }
    last->next = erase;
return( SplineSetRemoveOverlap(sc,head,over_exclude) );
}

static Entity *EntityReverse(Entity *ent) {
    Entity *next, *last = NULL;

    while ( ent!=NULL ) {
	next = ent->next;
	ent->next = last;
	last = ent;
	ent = next;
    }
return( last );
}

static SplinePointList *SplinesFromEntities(EntityChar *ec,int *flags) {
    Entity *ent, *next;
    SplinePointList *head=NULL, *last, *new, *nlast, *temp, *each, *transed;
    StrokeInfo si;
    real inversetrans[6];
    /*SplineSet *spl;*/
    int handle_eraser;

    if ( *flags==-1 )
	*flags = PsStrokeFlagsDlg();
	
    handle_eraser = *flags & sf_handle_eraser;
    if ( handle_eraser )
	ec->splines = EntityReverse(ec->splines);

    for ( ent=ec->splines; ent!=NULL; ent = next ) {
	next = ent->next;
	if ( ent->type == et_splines ) {
	    if ( ent->u.splines.splines!=NULL && ent->u.splines.splines->next==NULL &&
		    !SplinePointListIsClockwise(ent->u.splines.splines))
		SplineSetReverse(ent->u.splines.splines);
	    if ( ent->u.splines.stroke.col!=0xffffffff ) {
		memset(&si,'\0',sizeof(si));
		si.toobigwarn = *flags & sf_toobigwarn ? 1 : 0;
		si.join = ent->u.splines.join;
		si.cap = ent->u.splines.cap;
		si.removeoverlapifneeded = *flags & sf_removeoverlap ? 1 : 0;
		si.radius = ent->u.splines.stroke_width/2;
		new = NULL;
#if 0
		SSBisectTurners(ent->u.splines.splines);
#endif
		MatInverse(inversetrans,ent->u.splines.transform);
		transed = SplinePointListTransform(SplinePointListCopy(
			ent->u.splines.splines),inversetrans,true);
		for ( each = transed; each!=NULL; each=each->next ) {
		    temp = SplineSetStroke(each,&si,ec->sc);
		    if ( new==NULL )
			new=temp;
		    else
			nlast->next = temp;
		    if ( temp!=NULL )
			for ( nlast=temp; nlast->next!=NULL; nlast=nlast->next );
		}
		new = SplinePointListTransform(new,ent->u.splines.transform,true);
		SplinePointListFree(transed);
		if ( handle_eraser && ent->u.splines.stroke.col==0xffffff ) {
		    head = EraseStroke(ec->sc,head,new);
		    last = head;
		    if ( last!=NULL )
			for ( ; last->next!=NULL; last=last->next );
		} else {
		    if ( head==NULL )
			head = new;
		    else
			last->next = new;
		    if ( new!=NULL )
			for ( last = new; last->next!=NULL; last=last->next );
		}
		if ( si.toobigwarn )
		    *flags |= sf_toobigwarn;
	    }
	    if ( ent->u.splines.fill.col==0xffffffff )
		SplinePointListFree(ent->u.splines.splines);
	    else if ( handle_eraser && ent->u.splines.fill.col==0xffffff ) {
		head = EraseStroke(ec->sc,head,ent->u.splines.splines);
		last = head;
		if ( last!=NULL )
		    for ( ; last->next!=NULL; last=last->next );
	    } else {
		new = ent->u.splines.splines;
		if ( head==NULL )
		    head = new;
		else
		    last->next = new;
		if ( new!=NULL )
		    for ( last = new; last->next!=NULL; last=last->next );
	    }
	}
	free(ent);
    }
return( head );
}

SplinePointList *SplinePointListInterpretPS(FILE *ps) {
    EntityChar ec;
    int flags = -1;

    memset(&ec,'\0',sizeof(ec));
    InterpretPS(ps,&ec);
return( SplinesFromEntities(&ec,&flags));
}

Entity *EntityInterpretPS(FILE *ps) {
    EntityChar ec;

    memset(&ec,'\0',sizeof(ec));
    InterpretPS(ps,&ec);
return( ec.splines );
}

static void SCInterpretPS(FILE *ps,SplineChar *sc, int *flags) {
    EntityChar ec;
    real dval;
    char tokbuf[10];
    IO wrapper;

    wrapper.top = NULL;
    pushio(&wrapper,ps,NULL);

    if ( nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf))!=pt_opencurly )
	fprintf(stderr, "We don't understand this font\n" );
    memset(&ec,'\0',sizeof(ec));
    InterpretPS(ps,&ec);
    ec.sc = sc;
    sc->width = ec.width;
    sc->refs = ec.refs;
    sc->splines = SplinesFromEntities(&ec,flags);
    free(wrapper.top);
}
    
void PSFontInterpretPS(FILE *ps,struct charprocs *cp) {
    char tokbuf[100];
    int tok,i, j;
    real dval;
    SplineChar *sc; EntityChar dummy;
    RefChar *p, *ref, *next;
    IO wrapper;
    int flags = -1;

    wrapper.top = NULL;
    pushio(&wrapper,ps,NULL);

    while ( (tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf)))!=pt_eof && tok!=pt_end ) {
	if ( tok==pt_namelit ) {
	    if ( cp->next<cp->cnt ) {
		sc = SplineCharCreate();
		cp->keys[cp->next] = copy(tokbuf);
		cp->values[cp->next++] = sc;
		sc->name = copy(tokbuf);
		SCInterpretPS(ps,sc,&flags);
       		GProgressNext();
	    } else {
		InterpretPS(ps,&dummy);
	    }
	}
    }
    free(wrapper.top);

    /* References were done by name in the postscript. we stored the names in */
    /*  ref->sc (which is a hack). Now look up all those names and replace */
    /*  with the appropriate splinechar. If we can't find anything then throw */
    /*  out the reference */
    for ( i=0; i<cp->next; ++i ) {
	for ( p=NULL, ref=cp->values[i]->refs; ref!=NULL; ref=next ) {
	    next = ref->next;
	    for ( j=0; j<cp->next; ++j )
		if ( strcmp(cp->keys[j],(char *) (ref->sc))==0 )
	    break;
	    free(ref->sc);	/* a string, not a splinechar */
	    if ( j!=cp->next ) {
		ref->sc = cp->values[j];
		SCMakeDependent(cp->values[i],ref->sc);
		ref->adobe_enc = getAdobeEnc(ref->sc->name);
		ref->checked = true;
		p = ref;
	    } else {
		if ( p==NULL )
		    cp->values[i]->refs = next;
		else
		    p->next = next;
		RefCharFree(ref);
	    }
	}
    }
}

/* Slurp up an encoding in the form:
 /Enc-name [
    /charname
    ...
 ] def
We're not smart here no: 0 1 255 {1 index exch /.notdef put} for */
Encoding *PSSlurpEncodings(FILE *file) {
    char *names[1024];
    int32 encs[1024];
    Encoding *item, *head = NULL, *last;
    char *encname;
    char tokbuf[200];
    IO wrapper;
    real dval;
    int i, max, any, enc;
    int tok;

    wrapper.top = NULL;
    pushio(&wrapper,file,NULL);

    while ( (tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf)))!=pt_eof ) {
	encname = NULL;
	if ( tok==pt_namelit ) {
	    encname = copy(tokbuf);
	    tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf));
	}
	if ( tok!=pt_openarray )
return( head );
	for ( i=0; i<1024; ++i ) {
	    encs[i] = 0;
	    names[i]=NULL;
	}

	max = -1; any = 0; i=0;
	while ( (tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf)))!=pt_eof &&
		tok!=pt_closearray ) {
	    if ( tok==pt_namelit && i<1024 ) {
		max = i;
		if ( strcmp(tokbuf,".notdef")==0 ) {
		    encs[i] = 0;
		    if ( i<32 || (i>=0x7f && i<0xa0))
			encs[i] = i;
		} else if ( (enc=UniFromName(tokbuf))!=-1 )
		    encs[i] = enc;
		else {
		    names[i] = copy(tokbuf);
		    any = 1;
		}
	    }
	    ++i;
	}
	if ( encname!=NULL ) {
	    tok = nextpstoken(&wrapper,&dval,tokbuf,sizeof(tokbuf));
	    if ( tok==pt_def )
		/* Good */;
	}
	if ( max!=-1 ) {
	    if ( ++max<256 ) max = 256;
	    item = gcalloc(1,sizeof(Encoding));
	    item->enc_name = encname;
	    item->char_cnt = max;
	    item->unicode = galloc(max*sizeof(int32));
	    memcpy(item->unicode,encs,max*sizeof(int32));
	    if ( any ) {
		item->psnames = gcalloc(max,sizeof(char *));
		memcpy(item->psnames,names,max*sizeof(char *));
	    }
	    if ( head==NULL )
		head = item;
	    else
		last->next = item;
	    last = item;
	}
    }

return( head );
}

static void closepath(SplinePointList *cur, int is_type2) {
    if ( cur!=NULL && cur->first==cur->last && cur->first->prev==NULL && is_type2 )
return;		/* The "path" is just a single point created by a moveto */
		/* Probably we're just doing another moveto */
    if ( cur!=NULL && cur->first!=NULL && cur->first!=cur->last ) {
	if ( cur->first->me.x==cur->last->me.x && cur->first->me.y==cur->last->me.y ) {
	    SplinePoint *oldlast = cur->last;
	    cur->first->prevcp = oldlast->prevcp;
	    cur->first->noprevcp = false;
	    oldlast->prev->from->next = NULL;
	    cur->last = oldlast->prev->from;
	    chunkfree(oldlast->prev,sizeof(*oldlast));
	    chunkfree(oldlast,sizeof(*oldlast));
	}
	SplineMake3(cur->last,cur->first);
	cur->last = cur->first;
    }
}

/* this handles either Type1 or Type2 charstrings. Type2 charstrings have */
/*  more operators than Type1s and the old operators have extended meanings */
/*  (ie. the rlineto operator can produce more than one line). But pretty */
/*  much it's a superset and if we parse for type2 (with a few additions) */
/*  we'll get it right */
/* Char width is done differently. Moveto starts a newpath. 0xff starts a 16.16*/
/*  number rather than a 32 bit number */
SplineChar *PSCharStringToSplines(uint8 *type1, int len, int is_type2,
	struct pschars *subrs, struct pschars *gsubrs, const char *name) {
    real stack[50]; int sp=0, v;		/* Type1 stack is about 25 long, Type2 stack is 48 */
    real transient[32];
    SplineChar *ret = SplineCharCreate();
    SplinePointList *cur=NULL, *oldcur=NULL;
    RefChar *r1, *r2, *rlast=NULL;
    BasePoint current, oldcurrent;
    real dx, dy, dx2, dy2, dx3, dy3, dx4, dy4, dx5, dy5, dx6, dy6;
    SplinePoint *pt;
    /* subroutines may be nested to a depth of 10 */
    struct substate { unsigned char *type1; int len; } pcstack[11];
    int pcsp=0;
    StemInfo *hint, *hp;
    real pops[30];
    int popsp=0;
    int base, polarity;
    real coord;
    struct pschars *s;
    int hint_cnt = 0;

    ret->name = copy( name );
    ret->unicodeenc = -1;
    ret->width = (int16) 0x8000;
    if ( name==NULL ) name = "unnamed";
    ret->manualhints = true;

    current.x = current.y = 0;
    while ( len>0 ) {
	if ( sp>48 ) {
	    fprintf( stderr, "Stack got too big in %s\n", name );
	    sp = 48;
	}
	base = 0;
	--len;
	if ( (v = *type1++)>=32 ) {
	    if ( v<=246) {
		stack[sp++] = v - 139;
	    } else if ( v<=250 ) {
		stack[sp++] = (v-247)*256 + *type1++ + 108;
		--len;
	    } else if ( v<=254 ) {
		stack[sp++] = -(v-251)*256 - *type1++ - 108;
		--len;
	    } else {
		int val = (*type1<<24) | (type1[1]<<16) | (type1[2]<<8) | type1[3];
		stack[sp++] = val;
		type1 += 4;
		len -= 4;
		if ( is_type2 ) {
#ifndef PSFixed_Is_TTF	/* The type2 spec is contradictory. It says this is a */
			/*  two's complement number, but it also says it is a */
			/*  Fixed, which in truetype is not two's complement */
			/*  (mantisa is always unsigned) */
		    stack[sp-1] /= 65536.;
#else
		    int mant = val&0xffff;
		    stack[sp-1] = (val>>16) + mant/65536.;
#endif
		}
	    }
	} else if ( v==28 ) {
	    stack[sp++] = (short) ((type1[0]<<8) | type1[1]);
	    type1 += 2;
	    len -= 2;
	/* In the Dict tables of CFF, a 5byte fixed value is prefixed by a */
	/*  29 code. In Type2 strings the prefix is 255. */
	} else if ( v==12 ) {
	    v = *type1++;
	    --len;
	    switch ( v ) {
	      case 0: /* dotsection */
		sp = 0;
	      break;
	      case 1: /* vstem3 */	/* specifies three v hints zones at once */
		if ( sp<6 ) fprintf(stderr, "Stack underflow on vstem3 in %s\n", name );
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[0] + ret->lsidebearing;
		hint->width = stack[1];
		if ( ret->vstem==NULL )
		    ret->vstem = hp = hint;
		else {
		    for ( hp=ret->vstem; hp->next!=NULL; hp = hp->next );
		    hp->next = hint;
		    hp = hint;
		}
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[2] + ret->lsidebearing;
		hint->width = stack[3];
		hp->next = hint; hp = hint;
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[4] + ret->lsidebearing;
		hint->width = stack[5];
		hp->next = hint; 
		sp = 0;
	      break;
	      case 2: /* hstem3 */	/* specifies three h hints zones at once */
		if ( sp<6 ) fprintf(stderr, "Stack underflow on hstem3 in %s\n", name );
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[0];
		hint->width = stack[1];
		if ( ret->hstem==NULL )
		    ret->hstem = hp = hint;
		else {
		    for ( hp=ret->hstem; hp->next!=NULL; hp = hp->next );
		    hp->next = hint;
		    hp = hint;
		}
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[2];
		hint->width = stack[3];
		hp->next = hint; hp = hint;
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[4];
		hint->width = stack[5];
		hp->next = hint; 
		sp = 0;
	      break;
	      case 6: /* seac */	/* build accented characters */
 seac:
		if ( sp<5 ) fprintf(stderr, "Stack underflow on seac in %s\n", name );
		/* stack[0] must be the lsidebearing of the accent. I'm not sure why */
		r1 = chunkalloc(sizeof(RefChar));
		r2 = chunkalloc(sizeof(RefChar));
		r2->transform[0] = 1; r2->transform[3]=1;
		r2->transform[4] = stack[1] - (stack[0]-ret->lsidebearing);
		r2->transform[5] = stack[2];
		/* the translation of the accent here is said to be relative */
		/*  to the origins of the base character. I think they place */
		/*  the origin at the left bearing. And they don't mean the  */
		/*  base char at all, they mean the current char's lbearing  */
		/*  (which is normally the same as the base char's, except   */
		/*  when I has a big accent (like diaerisis) */
		r1->transform[0] = 1; r1->transform[3]=1;
		r1->adobe_enc = stack[3];
		r2->adobe_enc = stack[4];
		r1->next = r2;
		if ( rlast!=NULL ) rlast->next = r1;
		else ret->refs = r1;
		rlast = r2;
		sp = 0;
	      break;
	      case 7: /* sbw */		/* generalized width/sidebearing command */
		if ( sp<4 ) fprintf(stderr, "Stack underflow on sbw in %s\n", name );
		ret->lsidebearing = stack[0];
		/* stack[1] is lsidebearing y (only for vertical writing styles, CJK) */
		ret->width = stack[2];
		/* stack[3] is height (for vertical writing styles, CJK) */
		sp = 0;
	      break;
	      case 5: case 9: case 14: case 26:
		if ( sp<1 ) fprintf(stderr, "Stack underflow on unary operator in %s\n", name );
		switch ( v ) {
		  case 5: stack[sp-1] = (stack[sp-1]==0); break;	/* not */
		  case 9: if ( stack[sp-1]<0 ) stack[sp-1]= -stack[sp-1]; break;	/* abs */
		  case 14: stack[sp-1] = -stack[sp-1]; break;		/* neg */
		  case 26: stack[sp-1] = sqrt(stack[sp-1]); break;	/* sqrt */
		}
	      break;
	      case 3: case 4: case 10: case 11: case 12: case 15: case 24:
		if ( sp<2 ) fprintf(stderr, "Stack underflow on binary operator in %s\n", name );
		else switch ( v ) {
		  case 3: /* and */
		    stack[sp-2] = (stack[sp-1]!=0 && stack[sp-2]!=0);
		  break;
		  case 4: /* and */
		    stack[sp-2] = (stack[sp-1]!=0 || stack[sp-2]!=0);
		  break;
		  case 10: /* add */
		    stack[sp-2] += stack[sp-1];
		  break;
		  case 11: /* sub */
		    stack[sp-2] -= stack[sp-1];
		  break;
		  case 12: /* div */
		    stack[sp-2] /= stack[sp-1];
		  break;
		  case 24: /* mul */
		    stack[sp-2] *= stack[sp-1];
		  break;
		  case 15: /* eq */
		    stack[sp-2] = (stack[sp-1]==stack[sp-2]);
		  break;
		}
		--sp;
	      break;
	      case 22: /* ifelse */
		if ( sp<4 ) fprintf(stderr, "Stack underflow on ifelse in %s\n", name );
		else {
		    if ( stack[sp-2]>stack[sp-1] )
			stack[sp-4] = stack[sp-3];
		    sp -= 3;
		}
	      break;
	      case 23: /* random */
		/* This function returns something (0,1]. It's not clear to me*/
		/*  if rand includes 0 and RAND_MAX or not, but this approach */
		/*  should work no matter what */
		do {
		    stack[sp] = (rand()/(RAND_MAX-1));
		} while ( stack[sp]==0 || stack[sp]>1 );
		++sp;
	      break;
	      case 16: /* callothersubr */
		/* stack[sp-1] is the number of the thing to call in the othersubr array */
		/* stack[sp-2] is the number of args to grab off our stack and put on the */
		/*  real postscript stack */
		if ( sp<2 || sp < 2+stack[sp-2] ) {
		    fprintf(stderr, "Stack underflow on callothersubr in %s\n", name );
		    sp = 0;
		} else {
		    int tot = stack[sp-2], k;
		    popsp = 0;
		    for ( k=sp-3; k>=sp-2-tot; --k )
			pops[popsp++] = stack[k];
		    /* othersubrs 0-3 must be interpretted. 0-2 are Flex, 3 is Hint Replacement */
		    /* othersubrs 12,13 are for counter hints. We don't need to */
		    /*  do anything to ignore them */
		    if ( stack[sp-1]==3 ) {
			/* when we weren't capabable of hint replacement we */
			/*  punted by putting 3 on the stack (T1 spec page 70) */
			/*  subroutine 3 is a noop */
			/*pops[popsp-1] = 3;*/
			ret->manualhints = false;
			/* We can manage hint substitution from hintmask though*/
			/*  well enough that we needn't clear the manualhints bit */
		    } else if ( stack[sp-1]==1 ) {
			/* We punt for flex too. This is a bit harder */
			/* Essentially what we want to do is draw a line from */
			/*  where we are at the beginning to where we are at */
			/*  the end. So we save the beginning here (this starts*/
			/*  the flex sequence), we ignore all calls to othersub*/
			/*  2, and when we get to othersub 0 we put everything*/
			/*  back to where it should be and free up whatever */
			/*  extranious junk we created along the way and draw */
			/*  our line. */
			/* Let's punt a little less, and actually figure out */
			/*  the appropriate rrcurveto commands and put in a */
			/*  dished serif */
			oldcurrent = current;
			oldcur = cur;
			cur->next = NULL;
		    } else if ( stack[sp-1]==2 )
			/* No op */;
		    else if ( stack[sp-1]==0 ) {
			SplinePointList *spl = oldcur->next;
			if ( spl!=NULL && spl->next!=NULL &&
				spl->next->next!=NULL &&
				spl->next->next->next!=NULL &&
				spl->next->next->next->next!=NULL &&
				spl->next->next->next->next->next!=NULL &&
				spl->next->next->next->next->next->next!=NULL ) {
			    BasePoint old_nextcp, mid_prevcp, mid, mid_nextcp,
				    end_prevcp, end;
			    old_nextcp	= spl->next->first->me;
			    mid_prevcp	= spl->next->next->first->me;
			    mid		= spl->next->next->next->first->me;
			    mid_nextcp	= spl->next->next->next->next->first->me;
			    end_prevcp	= spl->next->next->next->next->next->first->me;
			    end		= spl->next->next->next->next->next->next->first->me;
			    cur = oldcur;
			    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
				cur->last->nextcp = old_nextcp;
				cur->last->nonextcp = false;
				pt = chunkalloc(sizeof(SplinePoint));
				pt->prevcp = mid_prevcp;
				pt->me = mid;
				pt->nextcp = mid_nextcp;
				/*pt->flex = pops[2];*/
				SplineMake3(cur->last,pt);
				cur->last = pt;
				pt = chunkalloc(sizeof(SplinePoint));
				pt->prevcp = end_prevcp;
				pt->me = end;
				pt->nonextcp = true;
				SplineMake3(cur->last,pt);
				cur->last = pt;
			    } else
				fprintf(stderr, "No previous point on path in curveto from flex 0 in %s\n", name );
			} else {
			    /* Um, something's wrong. Let's just draw a line */
			    /* do the simple method, which consists of creating */
			    /*  the appropriate line */
			    pt = chunkalloc(sizeof(SplinePoint));
			    pt->me.x = pops[1]; pt->me.y = pops[0];
			    pt->noprevcp = true; pt->nonextcp = true;
			    SplinePointListFree(oldcur->next); oldcur->next = NULL;
			    cur = oldcur;
			    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
				SplineMake3(cur->last,pt);
				cur->last = pt;
			    } else
				fprintf(stderr, "No previous point on path in lineto from flex 0 in %s\n", name );
			}
			--popsp;
			cur->next = NULL;
			SplinePointListFree(spl);
			oldcur = NULL;
		    }
		    sp = k+1;
		}
	      break;
	      case 20: /* put */
		if ( sp<2 ) fprintf(stderr, "Too few items on stack for put in %s\n", name );
		else if ( stack[sp-1]<0 || stack[sp-1]>=32 ) fprintf(stderr,"Reference to transient memory out of bounds in put in %s\n", name );
		else {
		    transient[(int)stack[sp-1]] = stack[sp-2];
		    sp -= 2;
		}
	      break;
	      case 21: /* get */
		if ( sp<1 ) fprintf(stderr, "Too few items on stack for put in %s\n", name );
		else if ( stack[sp-1]<0 || stack[sp-1]>=32 ) fprintf(stderr,"Reference to transient memory out of bounds in put in %s\n", name );
		else
		    stack[sp-1] = transient[(int)stack[sp-1]];
	      break;
	      case 17: /* pop */
		/* pops something from the postscript stack and pushes it on ours */
		/* used to get a return value from an othersubr call */
		/* Bleah. Adobe wants the pops to return the arguments if we */
		/*  don't understand the call. What use is the subroutine then?*/
		if ( popsp<=0 )
		    fprintf(stderr, "Pop stack underflow on pop in %s\n", name );
		else
		    stack[sp++] = pops[--popsp];
	      break;
	      case 18: /* drop */
		if ( sp>0 ) --sp;
	      break;
	      case 27: /* dup */
		if ( sp>=1 ) {
		    stack[sp] = stack[sp-1];
		    ++sp;
		}
	      break;
	      case 28: /* exch */
		if ( sp>=2 ) {
		    real temp = stack[sp-1];
		    stack[sp-1] = stack[sp-2]; stack[sp-2] = temp;
		}
	      break;
	      case 29: /* index */
		if ( sp>=1 ) {
		    int index = stack[--sp];
		    if ( index<0 || sp<index+1 )
			fprintf( stderr, "Index out of range in %s\n", name );
		    else {
			stack[sp] = stack[sp-index-1];
			++sp;
		    }
		}
	      break;
	      case 30: /* roll */
		if ( sp>=2 ) {
		    int j = stack[sp-1], N=stack[sp-2];
		    if ( N>sp || j>=N || j<0 || N<0 )
			fprintf( stderr, "roll out of range in %s\n", name );
		    else if ( j==0 || N==0 )
			/* No op */;
		    else {
			real *temp = galloc(N*sizeof(real));
			int i;
			for ( i=0; i<N; ++i )
			    temp[i] = stack[sp-N+i];
			for ( i=0; i<N; ++i )
			    stack[sp-N+i] = temp[(i+j)%N];
			free(temp);
		    }
		}
	      break;
	      case 33: /* setcurrentpoint */
		if ( sp<2 ) fprintf(stderr, "Stack underflow on setcurrentpoint in %s\n", name );
		else {
		    current.x = stack[0];
		    current.y = stack[1];
		}
		sp = 0;
	      break;
	      case 34:	/* hflex */
	      case 35:	/* flex */
	      case 36:	/* hflex1 */
	      case 37:	/* flex1 */
		dy = dy3 = dy4 = dy5 = dy6 = 0;
		dx = stack[base++];
		if ( v!=34 )
		    dy = stack[base++];
		dx2 = stack[base++];
		dy2 = stack[base++];
		dx3 = stack[base++];
		if ( v!=34 && v!=36 )
		    dy3 = stack[base++];
		dx4 = stack[base++];
		if ( v!=34 && v!=36 )
		    dy4 = stack[base++];
		dx5 = stack[base++];
		if ( v==34 )
		    dy5 = -dy2;
		else
		    dy5 = stack[base++];
		switch ( v ) {
		    real xt, yt;
		    case 35:    /* flex */
			dx6 = stack[base++];
			dy6 = stack[base++];
			break;
		    case 34:    /* hflex */
			dx6 = stack[base++];
			break;
		    case 36:    /* hflex1 */
			dx6 = stack[base++];
			dy6 = -dy-dy2-dy5;
			break;
		    case 37:    /* flex1 */
			xt = dx+dx2+dx3+dx4+dx5;
			yt = dy+dy2+dy3+dy4+dy5;
			if ( xt<0 ) xt= -xt;
			if ( yt<0 ) yt= -yt;
			if ( xt>yt ) {
			    dx6 = stack[base++];
			    dy6 = -dy-dy2-dy3-dy4-dy5;
			} else {
			    dy6 = stack[base++];
			    dx6 = -dx-dx2-dx3-dx4-dx5;
			}
			break;
		}
		if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
		    current.x += dx; current.y += dy;
		    cur->last->nextcp = current;
		    cur->last->nonextcp = false;
		    current.x += dx2; current.y += dy2;
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->prevcp = current;
		    current.x += dx3; current.y += dy3;
		    pt->me = current;
		    pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;

		    current.x += dx4; current.y += dy4;
		    cur->last->nextcp = current;
		    cur->last->nonextcp = false;
		    current.x += dx5; current.y += dy5;
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->prevcp = current;
		    current.x += dx6; current.y += dy6;
		    pt->me = current;
		    pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		} else
		    fprintf(stderr, "No previous point on path in flex operator in %s\n", name );
		sp = 0;
	      break;
	      default:
		fprintf( stderr, "Uninterpreted opcode 12,%d in %s\n", v, name );
	      break;
	    }
	} else switch ( v ) {
	  case 1: /* hstem */
	  case 18: /* hstemhm */
	    base = 0;
	    if ( sp&1 ) {
		ret->width = stack[0];
		base=1;
	    }
	    if ( sp-base<2 )
		fprintf(stderr, "Stack underflow on hstem in %s\n", name );
	    /* stack[0] is absolute y for start of horizontal hint */
	    /*	(actually relative to the y specified as lsidebearing y in sbw*/
	    /* stack[1] is relative y for height of hint zone */
	    coord = 0;
	    while ( sp-base>=2 ) {
		hint = chunkalloc(sizeof(StemInfo));
		hint->start = stack[base]+coord;
		hint->width = stack[base+1];
		if ( ret->hstem==NULL )
		    ret->hstem = hint;
		else {
		    for ( hp=ret->hstem; hp->next!=NULL; hp = hp->next );
		    hp->next = hint;
		}
		base+=2;
		coord = hint->start+hint->width;
		++hint_cnt;
	    }
	    sp = 0;
	  break;
	  case 19: /* hintmask */
	  case 20: /* cntrmask */
	    /* If there's anything on the stack treat it as a vstem hint */
	    base = 0;
	  case 3: /* vstem */
	  case 23: /* vstemhm */
	    if ( cur==NULL || v==3 || v==23 ) {
		if ( sp&1 ) {
		    ret->width = stack[0];
		    base=1;
		}
		/* I've seen a vstemhm with no arguments. I've no idea what that */
		/*  means. It came right after a hintmask */
		/* I'm confused about v/hstemhm because the manual says it needs */
		/*  to be used if one uses a hintmask, but that's not what the */
		/*  examples show.  Or I'm not understanding. */
		if ( sp-base<2 && v!=19 && v!=20 )
		    fprintf(stderr, "Stack underflow on vstem in %s\n", name );
		/* stack[0] is absolute x for start of vertical hint */
		/*	(actually relative to the x specified as lsidebearing in h/sbw*/
		/* stack[1] is relative x for height of hint zone */
		coord = 0;
		while ( sp-base>=2 ) {
		    hint = chunkalloc(sizeof(StemInfo));
		    hint->start = stack[base] + coord + ret->lsidebearing;
		    hint->width = stack[base+1];
		    if ( ret->vstem==NULL )
			ret->vstem = hint;
		    else {
			for ( hp=ret->vstem; hp->next!=NULL; hp = hp->next );
			hp->next = hint;
		    }
		    base+=2;
		    coord = hint->start+hint->width;
		    ++hint_cnt;
		}
		sp = 0;
	    }
	    if ( v==19 || v==20 ) {		/* hintmask, cntrmask */
		int bytes = (hint_cnt+7)/8;
		type1 += bytes;
		len -= bytes;
	    }
	  break;
	  case 14: /* endchar */
	    /* endchar is allowed to terminate processing even within a subroutine */
	    closepath(cur,is_type2);
	    pcsp = 0;
	    if ( sp==4 ) {
		/* In Type2 strings endchar has a depreciated function of doing */
		/*  a seac (which doesn't exist at all). Except enchar takes */
		/*  4 args and seac takes 5. Bleah */
		stack[4] = stack[3]; stack[3] = stack[2]; stack[2] = stack[1]; stack[1] = stack[0];
		stack[0] = 0;
		sp = 5;
  goto seac;
	    }
	    /* the docs say that endchar must be the last command in a char */
	    /*  (or the last command in a subroutine which is the last in the */
	    /*  char) So in theory if there's anything left we should complain*/
	    /*  In practice though, the EuroFont has a return statement after */
	    /*  the endchar in a subroutine. So we won't try to catch that err*/
	    /*  and just stop. */
  goto done;
	  break;
	  case 13: /* hsbw (set left sidebearing and width) */
	    if ( sp<2 ) fprintf(stderr, "Stack underflow on hsbw in %s\n", name );
	    ret->lsidebearing = stack[0];
	    current.x = stack[0];		/* sets the current point too */
	    ret->width = stack[1];
	    sp = 0;
	  break;
	  case 9: /* closepath */
	    sp = 0;
	    closepath(cur,is_type2);
	  break;
	  case 21: /* rmoveto */
	  case 22: /* hmoveto */
	  case 4: /* vmoveto */
	    if ( is_type2 ) {
		if ( (v==21 && sp==3) || (v!=21 && sp==2)) {
		    /* Character's width may be specified on the first moveto */
		    ret->width = stack[0];
		    stack[0] = stack[1]; stack[1] = stack[2]; --sp;
		}
		closepath(cur,true);
	    }
#if 0	/* This doesn't work. It breaks type1 flex hinting. */
	/* There's now a special hack just before returning which closes any */
	/*  open paths */
	    else if ( cur!=NULL && cur->first->prev==NULL )
		closepath(cur,false);		/* Even in type1 fonts closepath is optional */
#endif
	  case 5: /* rlineto */
	  case 6: /* hlineto */
	  case 7: /* vlineto */
	    polarity = 0;
	    while ( base<sp ) {
		dx = dy = 0;
		if ( v==5 || v==21 ) {
		    if ( sp<2 ) fprintf(stderr, "Stack underflow on rlineto/rmoveto in %s\n", name );
		    dx = stack[base++];
		    dy = stack[base++];
		} else if ( (v==6 && !(polarity&1)) || (v==7 && (polarity&1)) || v==22 ) {
		    if ( sp<1 ) fprintf(stderr, "Stack underflow on hlineto/hmoveto in %s\n", name );
		    dx = stack[base++];
		} else /*if ( (v==7 && !(parity&1)) || (v==6 && (parity&1) || v==4 )*/ {
		    if ( sp<1 ) fprintf(stderr, "Stack underflow on vlineto/vmoveto in %s\n", name );
		    dy = stack[base++];
		}
		++polarity;
		current.x += dx; current.y += dy;
		pt = chunkalloc(sizeof(SplinePoint));
		pt->me = current;
		pt->noprevcp = true; pt->nonextcp = true;
		if ( v==4 || v==21 || v==22 ) {
		    if ( cur!=NULL && cur->first==cur->last && cur->first->prev==NULL && is_type2 ) {
			/* Two adjacent movetos should not create single point paths */
			cur->first->me = current;
			SplinePointFree(pt);
		    } else {
			SplinePointList *spl = chunkalloc(sizeof(SplinePointList));
			spl->first = spl->last = pt;
			if ( cur!=NULL )
			    cur->next = spl;
			else
			    ret->splines = spl;
			cur = spl;
		    }
	    break;
		} else {
		    if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
			SplineMake3(cur->last,pt);
			cur->last = pt;
		    } else
			fprintf(stderr, "No previous point on path in lineto in %s\n", name );
		}
	    }
	    sp = 0;
	  break;
	  case 25: /* rlinecurve */
	    base = 0;
	    while ( sp>base+6 ) {
		current.x += stack[base++]; current.y += stack[base++];
		if ( cur!=NULL ) {
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->me = current;
		    pt->noprevcp = true; pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		}
	    }
	  case 24: /* rcurveline */
	  case 8:  /* rrcurveto */
	  case 31: /* hvcurveto */
	  case 30: /* vhcurveto */
	  case 27: /* hhcurveto */
	  case 26: /* vvcurveto */
	    polarity = 0;
	    while ( sp>base+2 ) {
		dx = dy = dx2 = dy2 = dx3 = dy3 = 0;
		if ( v==8 || v==25 || v==24 ) {
		    if ( sp<6+base ) {
			fprintf(stderr, "Stack underflow on rrcurveto in %s\n", name );
			base = sp;
		    } else {
			dx = stack[base++];
			dy = stack[base++];
			dx2 = stack[base++];
			dy2 = stack[base++];
			dx3 = stack[base++];
			dy3 = stack[base++];
		    }
		} else if ( v==27 ) {		/* hhcurveto */
		    if ( sp<4+base ) {
			fprintf(stderr, "Stack underflow on hhcurveto in %s\n", name );
			base = sp;
		    } else {
			if ( (sp-base)&1 ) dy = stack[base++];
			dx = stack[base++];
			dx2 = stack[base++];
			dy2 = stack[base++];
			dx3 = stack[base++];
		    }
		} else if ( v==26 ) {		/* vvcurveto */
		    if ( sp<4+base ) {
			fprintf(stderr, "Stack underflow on hhcurveto in %s\n", name );
			base = sp;
		    } else {
			if ( (sp-base)&1 ) dx = stack[base++];
			dy = stack[base++];
			dx2 = stack[base++];
			dy2 = stack[base++];
			dy3 = stack[base++];
		    }
		} else if ( (v==31 && !(polarity&1)) || (v==30 && (polarity&1)) ) {
		    if ( sp<4+base ) {
			fprintf(stderr, "Stack underflow on hvcurveto in %s\n", name );
			base = sp;
		    } else {
			dx = stack[base++];
			dx2 = stack[base++];
			dy2 = stack[base++];
			dy3 = stack[base++];
			if ( sp==base+1 )
			    dx3 = stack[base++];
		    }
		} else /*if ( (v==30 && !(polarity&1)) || (v==31 && (polarity&1)) )*/ {
		    if ( sp<4+base ) {
			fprintf(stderr, "Stack underflow on vhcurveto in %s\n", name );
			base = sp;
		    } else {
			dy = stack[base++];
			dx2 = stack[base++];
			dy2 = stack[base++];
			dx3 = stack[base++];
			if ( sp==base+1 )
			    dy3 = stack[base++];
		    }
		}
		++polarity;
		if ( cur!=NULL && cur->first!=NULL && (cur->first!=cur->last || cur->first->next==NULL) ) {
		    current.x += dx; current.y += dy;
		    cur->last->nextcp = current;
		    cur->last->nonextcp = false;
		    current.x += dx2; current.y += dy2;
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->prevcp = current;
		    current.x += dx3; current.y += dy3;
		    pt->me = current;
		    pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		} else
		    fprintf(stderr, "No previous point on path in curveto in %s\n", name );
	    }
	    if ( v==24 ) {
		current.x += stack[base++]; current.y += stack[base++];
		if ( cur!=NULL ) {	/* In legal code, cur can't be null here, but I got something illegal... */
		    pt = chunkalloc(sizeof(SplinePoint));
		    pt->me = current;
		    pt->noprevcp = true; pt->nonextcp = true;
		    SplineMake3(cur->last,pt);
		    cur->last = pt;
		}
	    }
	    sp = 0;
	  break;
	  case 29: /* callgsubr */
	  case 10: /* callsubr */
	    /* stack[sp-1] contains the number of the subroutine to call */
	    if ( sp<1 ) fprintf(stderr, "Stack underflow on callsubr in %s\n", name );
	    else if ( pcsp>10 ) fprintf( stderr, "Too many subroutine calls in %s\n", name );
	    s=subrs; if ( v==29 ) s = gsubrs;
	    if ( s!=NULL ) stack[sp-1] += s->bias;
	    /* Type2 subrs have a bias that must be added to the subr-number */
	    /* Type1 subrs do not. We set the bias on them to 0 */
	    if ( s==NULL || stack[sp-1]>=s->cnt || stack[sp-1]<0 ||
		    s->values[(int) stack[sp-1]]==NULL )
		fprintf(stderr,"Subroutine number out of bounds in %s\n", name );
	    else {
		pcstack[pcsp].type1 = type1;
		pcstack[pcsp].len = len;
		++pcsp;
		type1 = s->values[(int) stack[sp-1]];
		len = s->lens[(int) stack[sp-1]];
	    }
	    if ( --sp<0 ) sp = 0;
	  break;
	  case 11: /* return */
	    /* return from a subr outine */
	    if ( pcsp<1 ) fprintf(stderr, "return when not in subroutine in %s\n", name );
	    else {
		--pcsp;
		type1 = pcstack[pcsp].type1;
		len = pcstack[pcsp].len;
	    }
	  break;
	  default:
	    fprintf( stderr, "Uninterpreted opcode %d in %s\n", v, name );
	  break;
	}
    }
  done:
    if ( pcsp!=0 )
	fprintf(stderr, "end of subroutine reached with no return in %s\n", name );
    SCCatagorizePoints(ret);

    /* Even in type1 fonts all paths should be closed. But if we close them at*/
    /*  the obvious moveto, that breaks flex hints. So we have a hack here at */
    /*  the end which closes any open paths. */
    if ( !is_type2 ) for ( cur = ret->splines; cur!=NULL; cur = cur->next ) if ( cur->first->prev==NULL ) {
	SplineMake3(cur->last,cur->first);
	cur->last = cur->first;
    }

    /* For some reason, when I read splines in, I get their clockwise nature */
    /*  backwards ... at least backwards from fontographer ... so reverse 'em*/
    for ( cur = ret->splines; cur!=NULL; cur = cur->next )
	SplineSetReverse(cur);
    if ( ret->hstem==NULL && ret->vstem==NULL )
	ret->manualhints = false;
    ret->hstem = HintCleanup(ret->hstem,true);
    ret->vstem = HintCleanup(ret->vstem,true);
    SCGuessHHintInstancesList(ret);
    SCGuessVHintInstancesList(ret);
    ret->hconflicts = StemListAnyConflicts(ret->hstem);
    ret->vconflicts = StemListAnyConflicts(ret->vstem);
    if ( name!=NULL && strcmp(name,".notdef")!=0 )
	ret->widthset = true;
return( ret );
}
