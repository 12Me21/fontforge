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
#include "pfaedit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ustring.h>
#include <utype.h>
#include <unistd.h>
#include "psfont.h"
#include <locale.h>

struct fontparse {
    FontDict *fd, *mainfd;
    /* always in font data */
    unsigned int infi:1;
    unsigned int inchars:1;
    unsigned int inprivate:1;
    unsigned int insubs:1;
    unsigned int inencoding: 1;
    unsigned int multiline: 1;
    unsigned int instring: 1;
    unsigned int incidsysteminfo: 1;
    unsigned int iscid: 1;
    unsigned int iscff: 1;
    int fdindex;

    unsigned int alreadycomplained: 1;

    char *vbuf, *vmax, *vpt;
    int depth;
};

static void copyenc(char *encoding[256],char *std[256]) {
    int i;
    for ( i=0; i<256; ++i )
	encoding[i] = strdup(std[i]);
}

char *AdobeStandardEncoding[] = {
/* 0000 */	".notdef",
/* 0001 */	".notdef",
/* 0002 */	".notdef",
/* 0003 */	".notdef",
/* 0004 */	".notdef",
/* 0005 */	".notdef",
/* 0006 */	".notdef",
/* 0007 */	".notdef",
/* 0008 */	".notdef",
/* 0009 */	".notdef",
/* 000a */	".notdef",
/* 000b */	".notdef",
/* 000c */	".notdef",
/* 000d */	".notdef",
/* 000e */	".notdef",
/* 000f */	".notdef",
/* 0010 */	".notdef",
/* 0011 */	".notdef",
/* 0012 */	".notdef",
/* 0013 */	".notdef",
/* 0014 */	".notdef",
/* 0015 */	".notdef",
/* 0016 */	".notdef",
/* 0017 */	".notdef",
/* 0018 */	".notdef",
/* 0019 */	".notdef",
/* 001a */	".notdef",
/* 001b */	".notdef",
/* 001c */	".notdef",
/* 001d */	".notdef",
/* 001e */	".notdef",
/* 001f */	".notdef",
/* 0020 */	"space",
/* 0021 */	"exclam",
/* 0022 */	"quotedbl",
/* 0023 */	"numbersign",
/* 0024 */	"dollar",
/* 0025 */	"percent",
/* 0026 */	"ampersand",
/* 0027 */	"quoteright",
/* 0028 */	"parenleft",
/* 0029 */	"parenright",
/* 002a */	"asterisk",
/* 002b */	"plus",
/* 002c */	"comma",
/* 002d */	"hyphen",
/* 002e */	"period",
/* 002f */	"slash",
/* 0030 */	"zero",
/* 0031 */	"one",
/* 0032 */	"two",
/* 0033 */	"three",
/* 0034 */	"four",
/* 0035 */	"five",
/* 0036 */	"six",
/* 0037 */	"seven",
/* 0038 */	"eight",
/* 0039 */	"nine",
/* 003a */	"colon",
/* 003b */	"semicolon",
/* 003c */	"less",
/* 003d */	"equal",
/* 003e */	"greater",
/* 003f */	"question",
/* 0040 */	"at",
/* 0041 */	"A",
/* 0042 */	"B",
/* 0043 */	"C",
/* 0044 */	"D",
/* 0045 */	"E",
/* 0046 */	"F",
/* 0047 */	"G",
/* 0048 */	"H",
/* 0049 */	"I",
/* 004a */	"J",
/* 004b */	"K",
/* 004c */	"L",
/* 004d */	"M",
/* 004e */	"N",
/* 004f */	"O",
/* 0050 */	"P",
/* 0051 */	"Q",
/* 0052 */	"R",
/* 0053 */	"S",
/* 0054 */	"T",
/* 0055 */	"U",
/* 0056 */	"V",
/* 0057 */	"W",
/* 0058 */	"X",
/* 0059 */	"Y",
/* 005a */	"Z",
/* 005b */	"bracketleft",
/* 005c */	"backslash",
/* 005d */	"bracketright",
/* 005e */	"asciicircum",
/* 005f */	"underscore",
/* 0060 */	"quoteleft",
/* 0061 */	"a",
/* 0062 */	"b",
/* 0063 */	"c",
/* 0064 */	"d",
/* 0065 */	"e",
/* 0066 */	"f",
/* 0067 */	"g",
/* 0068 */	"h",
/* 0069 */	"i",
/* 006a */	"j",
/* 006b */	"k",
/* 006c */	"l",
/* 006d */	"m",
/* 006e */	"n",
/* 006f */	"o",
/* 0070 */	"p",
/* 0071 */	"q",
/* 0072 */	"r",
/* 0073 */	"s",
/* 0074 */	"t",
/* 0075 */	"u",
/* 0076 */	"v",
/* 0077 */	"w",
/* 0078 */	"x",
/* 0079 */	"y",
/* 007a */	"z",
/* 007b */	"braceleft",
/* 007c */	"bar",
/* 007d */	"braceright",
/* 007e */	"asciitilde",
/* 007f */	".notdef",
/* 0080 */	".notdef",
/* 0081 */	".notdef",
/* 0082 */	".notdef",
/* 0083 */	".notdef",
/* 0084 */	".notdef",
/* 0085 */	".notdef",
/* 0086 */	".notdef",
/* 0087 */	".notdef",
/* 0088 */	".notdef",
/* 0089 */	".notdef",
/* 008a */	".notdef",
/* 008b */	".notdef",
/* 008c */	".notdef",
/* 008d */	".notdef",
/* 008e */	".notdef",
/* 008f */	".notdef",
/* 0090 */	".notdef",
/* 0091 */	".notdef",
/* 0092 */	".notdef",
/* 0093 */	".notdef",
/* 0094 */	".notdef",
/* 0095 */	".notdef",
/* 0096 */	".notdef",
/* 0097 */	".notdef",
/* 0098 */	".notdef",
/* 0099 */	".notdef",
/* 009a */	".notdef",
/* 009b */	".notdef",
/* 009c */	".notdef",
/* 009d */	".notdef",
/* 009e */	".notdef",
/* 009f */	".notdef",
/* 00a0 */	".notdef",
/* 00a1 */	"exclamdown",
/* 00a2 */	"cent",
/* 00a3 */	"sterling",
/* 00a4 */	"fraction",
/* 00a5 */	"yen",
/* 00a6 */	"florin",
/* 00a7 */	"section",
/* 00a8 */	"currency",
/* 00a9 */	"quotesingle",
/* 00aa */	"quotedblleft",
/* 00ab */	"guillemotleft",
/* 00ac */	"guilsinglleft",
/* 00ad */	"guilsinglright",
/* 00ae */	"fi",
/* 00af */	"fl",
/* 00b0 */	".notdef",
/* 00b1 */	"endash",
/* 00b2 */	"dagger",
/* 00b3 */	"daggerdbl",
/* 00b4 */	"periodcentered",
/* 00b5 */	".notdef",
/* 00b6 */	"paragraph",
/* 00b7 */	"bullet",
/* 00b8 */	"quotesinglbase",
/* 00b9 */	"quotedblbase",
/* 00ba */	"quotedblright",
/* 00bb */	"guillemotright",
/* 00bc */	"ellipsis",
/* 00bd */	"perthousand",
/* 00be */	".notdef",
/* 00bf */	"questiondown",
/* 00c0 */	".notdef",
/* 00c1 */	"grave",
/* 00c2 */	"acute",
/* 00c3 */	"circumflex",
/* 00c4 */	"tilde",
/* 00c5 */	"macron",
/* 00c6 */	"breve",
/* 00c7 */	"dotaccent",
/* 00c8 */	"dieresis",
/* 00c9 */	".notdef",
/* 00ca */	"ring",
/* 00cb */	"cedilla",
/* 00cc */	".notdef",
/* 00cd */	"hungarumlaut",
/* 00ce */	"ogonek",
/* 00cf */	"caron",
/* 00d0 */	"emdash",
/* 00d1 */	".notdef",
/* 00d2 */	".notdef",
/* 00d3 */	".notdef",
/* 00d4 */	".notdef",
/* 00d5 */	".notdef",
/* 00d6 */	".notdef",
/* 00d7 */	".notdef",
/* 00d8 */	".notdef",
/* 00d9 */	".notdef",
/* 00da */	".notdef",
/* 00db */	".notdef",
/* 00dc */	".notdef",
/* 00dd */	".notdef",
/* 00de */	".notdef",
/* 00df */	".notdef",
/* 00e0 */	".notdef",
/* 00e1 */	"AE",
/* 00e2 */	".notdef",
/* 00e3 */	"ordfeminine",
/* 00e4 */	".notdef",
/* 00e5 */	".notdef",
/* 00e6 */	".notdef",
/* 00e7 */	".notdef",
/* 00e8 */	"Lslash",
/* 00e9 */	"Oslash",
/* 00ea */	"OE",
/* 00eb */	"ordmasculine",
/* 00ec */	".notdef",
/* 00ed */	".notdef",
/* 00ee */	".notdef",
/* 00ef */	".notdef",
/* 00f0 */	".notdef",
/* 00f1 */	"ae",
/* 00f2 */	".notdef",
/* 00f3 */	".notdef",
/* 00f4 */	".notdef",
/* 00f5 */	"dotlessi",
/* 00f6 */	".notdef",
/* 00f7 */	".notdef",
/* 00f8 */	"lslash",
/* 00f9 */	"oslash",
/* 00fa */	"oe",
/* 00fb */	"germandbls",
/* 00fc */	".notdef",
/* 00fd */	".notdef",
/* 00fe */	".notdef",
/* 00ff */	".notdef"
};
static void setStdEnc(char *encoding[256]) {
    copyenc(encoding,AdobeStandardEncoding);
}

static void setLatin1Enc(char *encoding[256]) {
    static char *latin1enc[] = {
/* 0000 */	".notdef",
/* 0001 */	".notdef",
/* 0002 */	".notdef",
/* 0003 */	".notdef",
/* 0004 */	".notdef",
/* 0005 */	".notdef",
/* 0006 */	".notdef",
/* 0007 */	".notdef",
/* 0008 */	".notdef",
/* 0009 */	".notdef",
/* 000a */	".notdef",
/* 000b */	".notdef",
/* 000c */	".notdef",
/* 000d */	".notdef",
/* 000e */	".notdef",
/* 000f */	".notdef",
/* 0010 */	".notdef",
/* 0011 */	".notdef",
/* 0012 */	".notdef",
/* 0013 */	".notdef",
/* 0014 */	".notdef",
/* 0015 */	".notdef",
/* 0016 */	".notdef",
/* 0017 */	".notdef",
/* 0018 */	".notdef",
/* 0019 */	".notdef",
/* 001a */	".notdef",
/* 001b */	".notdef",
/* 001c */	".notdef",
/* 001d */	".notdef",
/* 001e */	".notdef",
/* 001f */	".notdef",
/* 0020 */	"space",
/* 0021 */	"exclam",
/* 0022 */	"quotedbl",
/* 0023 */	"numbersign",
/* 0024 */	"dollar",
/* 0025 */	"percent",
/* 0026 */	"ampersand",
/* 0027 */	"quoteright",
/* 0028 */	"parenleft",
/* 0029 */	"parenright",
/* 002a */	"asterisk",
/* 002b */	"plus",
/* 002c */	"comma",
/* 002d */	"hyphen",
/* 002e */	"period",
/* 002f */	"slash",
/* 0030 */	"zero",
/* 0031 */	"one",
/* 0032 */	"two",
/* 0033 */	"three",
/* 0034 */	"four",
/* 0035 */	"five",
/* 0036 */	"six",
/* 0037 */	"seven",
/* 0038 */	"eight",
/* 0039 */	"nine",
/* 003a */	"colon",
/* 003b */	"semicolon",
/* 003c */	"less",
/* 003d */	"equal",
/* 003e */	"greater",
/* 003f */	"question",
/* 0040 */	"at",
/* 0041 */	"A",
/* 0042 */	"B",
/* 0043 */	"C",
/* 0044 */	"D",
/* 0045 */	"E",
/* 0046 */	"F",
/* 0047 */	"G",
/* 0048 */	"H",
/* 0049 */	"I",
/* 004a */	"J",
/* 004b */	"K",
/* 004c */	"L",
/* 004d */	"M",
/* 004e */	"N",
/* 004f */	"O",
/* 0050 */	"P",
/* 0051 */	"Q",
/* 0052 */	"R",
/* 0053 */	"S",
/* 0054 */	"T",
/* 0055 */	"U",
/* 0056 */	"V",
/* 0057 */	"W",
/* 0058 */	"X",
/* 0059 */	"Y",
/* 005a */	"Z",
/* 005b */	"bracketleft",
/* 005c */	"backslash",
/* 005d */	"bracketright",
/* 005e */	"asciicircum",
/* 005f */	"underscore",
/* 0060 */	"grave",
/* 0061 */	"a",
/* 0062 */	"b",
/* 0063 */	"c",
/* 0064 */	"d",
/* 0065 */	"e",
/* 0066 */	"f",
/* 0067 */	"g",
/* 0068 */	"h",
/* 0069 */	"i",
/* 006a */	"j",
/* 006b */	"k",
/* 006c */	"l",
/* 006d */	"m",
/* 006e */	"n",
/* 006f */	"o",
/* 0070 */	"p",
/* 0071 */	"q",
/* 0072 */	"r",
/* 0073 */	"s",
/* 0074 */	"t",
/* 0075 */	"u",
/* 0076 */	"v",
/* 0077 */	"w",
/* 0078 */	"x",
/* 0079 */	"y",
/* 007a */	"z",
/* 007b */	"braceleft",
/* 007c */	"bar",
/* 007d */	"braceright",
/* 007e */	"asciitilde",
/* 007f */	".notdef",
/* 0080 */	".notdef",
/* 0081 */	".notdef",
/* 0082 */	".notdef",
/* 0083 */	".notdef",
/* 0084 */	".notdef",
/* 0085 */	".notdef",
/* 0086 */	".notdef",
/* 0087 */	".notdef",
/* 0088 */	".notdef",
/* 0089 */	".notdef",
/* 008a */	".notdef",
/* 008b */	".notdef",
/* 008c */	".notdef",
/* 008d */	".notdef",
/* 008e */	".notdef",
/* 008f */	".notdef",
/* 0090 */	"dotlessi",		/* Um, Adobe's Latin1 has some extra chars */
/* 0091 */	"grave",
/* 0092 */	"accute",		/* This is a duplicate... */
/* 0093 */	"circumflex",
/* 0094 */	"tilde",
/* 0095 */	"macron",
/* 0096 */	"breve",
/* 0097 */	"dotaccent",
/* 0098 */	"dieresis",
/* 0099 */	".notdef",
/* 009a */	"ring",
/* 009b */	"cedilla",
/* 009c */	".notdef",
/* 009d */	"hungarumlaut",
/* 009e */	"ogonek",
/* 009f */	"caron",
/* 00a0 */	"space",
/* 00a1 */	"exclamdown",
/* 00a2 */	"cent",
/* 00a3 */	"sterling",
/* 00a4 */	"currency",
/* 00a5 */	"yen",
/* 00a6 */	"brokenbar",
/* 00a7 */	"section",
/* 00a8 */	"dieresis",
/* 00a9 */	"copyright",
/* 00aa */	"ordfeminine",
/* 00ab */	"guillemotleft",
/* 00ac */	"logicalnot",
/* 00ad */	"hyphen",
/* 00ae */	"registered",
/* 00af */	"macron",
/* 00b0 */	"degree",
/* 00b1 */	"plusminus",
/* 00b2 */	"twosuperior",
/* 00b3 */	"threesuperior",
/* 00b4 */	"acute",
/* 00b5 */	"mu",
/* 00b6 */	"paragraph",
/* 00b7 */	"periodcentered",
/* 00b8 */	"cedilla",
/* 00b9 */	"onesuperior",
/* 00ba */	"ordmasculine",
/* 00bb */	"guillemotright",
/* 00bc */	"onequarter",
/* 00bd */	"onehalf",
/* 00be */	"threequarters",
/* 00bf */	"questiondown",
/* 00c0 */	"Agrave",
/* 00c1 */	"Aacute",
/* 00c2 */	"Acircumflex",
/* 00c3 */	"Atilde",
/* 00c4 */	"Adieresis",
/* 00c5 */	"Aring",
/* 00c6 */	"AE",
/* 00c7 */	"Ccedilla",
/* 00c8 */	"Egrave",
/* 00c9 */	"Eacute",
/* 00ca */	"Ecircumflex",
/* 00cb */	"Edieresis",
/* 00cc */	"Igrave",
/* 00cd */	"Iacute",
/* 00ce */	"Icircumflex",
/* 00cf */	"Idieresis",
/* 00d0 */	"Eth",
/* 00d1 */	"Ntilde",
/* 00d2 */	"Ograve",
/* 00d3 */	"Oacute",
/* 00d4 */	"Ocircumflex",
/* 00d5 */	"Otilde",
/* 00d6 */	"Odieresis",
/* 00d7 */	"multiply",
/* 00d8 */	"Oslash",
/* 00d9 */	"Ugrave",
/* 00da */	"Uacute",
/* 00db */	"Ucircumflex",
/* 00dc */	"Udieresis",
/* 00dd */	"Yacute",
/* 00de */	"Thorn",
/* 00df */	"germandbls",
/* 00e0 */	"agrave",
/* 00e1 */	"aacute",
/* 00e2 */	"acircumflex",
/* 00e3 */	"atilde",
/* 00e4 */	"adieresis",
/* 00e5 */	"aring",
/* 00e6 */	"ae",
/* 00e7 */	"ccedilla",
/* 00e8 */	"egrave",
/* 00e9 */	"eacute",
/* 00ea */	"ecircumflex",
/* 00eb */	"edieresis",
/* 00ec */	"igrave",
/* 00ed */	"iacute",
/* 00ee */	"icircumflex",
/* 00ef */	"idieresis",
/* 00f0 */	"eth",
/* 00f1 */	"ntilde",
/* 00f2 */	"ograve",
/* 00f3 */	"oacute",
/* 00f4 */	"ocircumflex",
/* 00f5 */	"otilde",
/* 00f6 */	"odieresis",
/* 00f7 */	"divide",
/* 00f8 */	"oslash",
/* 00f9 */	"ugrave",
/* 00fa */	"uacute",
/* 00fb */	"ucircumflex",
/* 00fc */	"udieresis",
/* 00fd */	"yacute",
/* 00fe */	"thorn",
/* 00ff */	"ydieresis"
    };
    copyenc(encoding,latin1enc);
}

static struct fontdict *MakeEmptyFont(void) {
    struct fontdict *ret;

    ret = gcalloc(1,sizeof(struct fontdict));
    ret->fontinfo = gcalloc(1,sizeof(struct fontinfo));
    ret->chars = gcalloc(1,sizeof(struct pschars));
    ret->private = gcalloc(1,sizeof(struct private));
    ret->private->subrs = gcalloc(1,sizeof(struct pschars));
    ret->private->private = gcalloc(1,sizeof(struct psdict));
    ret->private->leniv = 4;
    ret->encoding_name = em_none;
    ret->fontinfo->fstype = -1;
return( ret );
}

static struct fontdict *PSMakeEmptyFont(void) {
    struct fontdict *ret;

    ret = gcalloc(1,sizeof(struct fontdict));
    ret->fontinfo = gcalloc(1,sizeof(struct fontinfo));
    ret->chars = gcalloc(1,sizeof(struct pschars));
    ret->private = gcalloc(1,sizeof(struct private));
    ret->private->subrs = gcalloc(1,sizeof(struct pschars));
    ret->private->private = gcalloc(1,sizeof(struct psdict));
    ret->private->leniv = 4;
    ret->charprocs = gcalloc(1,sizeof(struct charprocs));
    ret->encoding_name = em_none;
    ret->fontinfo->fstype = -1;
return( ret );
}

static char *myfgets(char *str, int len, FILE *file) {
    char *pt, *end;
    int ch=0;

    for ( pt = str, end = str+len-1; pt<end && (ch=getc(file))!=EOF && ch!='\r' && ch!='\n';
	*pt++ = ch );
    if ( ch=='\n' )
	*pt++ = '\n';
    else if ( ch=='\r' ) {
	*pt++ = '\r';
	if ((ch=getc(file))!='\n' )
	    ungetc(ch,file);
	else
	    *pt++ = '\n';
    }
    if ( pt==str )
return( NULL );
    *pt = '\0';
return( str );
}

static char *getstring(char *start) {
    char *end, *ret;
    int parencnt=0;

    while ( *start!='\0' && *start!='(' ) ++start;
    if ( *start=='(' ) ++start;
    for ( end = start; *end!='\0' && (*end!=')' || parencnt>0); ++end ) {
	if ( *end=='\\' && (end[1]=='(' || end[1]==')'))
	    ++end;
	else if ( *end=='(' ) ++parencnt;
	else if ( *end==')' ) --parencnt;
    }
    ret = galloc(end-start+1);
    if ( end>start )
	strncpy(ret,start,end-start);
    ret[end-start] = '\0';
return( ret );
}

static char *getlongstring(char *start,FILE *in) {
    char buffer[512];

    while ( *start!='\0' && *start!='(' ) ++start;
    if ( *start!='\0' )
return(getstring(start));
    if ( myfgets(buffer,sizeof(buffer),in)==NULL )
return( copy(""));
return(getstring(buffer));
}

static char *gettoken(char *start) {
    char *end, *ret;

    while ( *start!='\0' && *start!='/' ) ++start;
    if ( *start=='/' ) ++start;
    for ( end = start; *end!='\0' && !isspace(*end) && *end!='[' && *end!='/' && *end!='{' && *end!='(' ; ++end );
    ret = galloc(end-start+1);
    if ( end>start )
	strncpy(ret,start,end-start);
    ret[end-start] = '\0';
return( ret );
}

static int getbool(char *start) {

    while ( isspace(*start) ) ++start;
    if ( *start=='T' || *start=='t' )
return( 1 );

return( 0 );
}

static void fillintarray(int *array,char *start,int maxentries) {
    int i;
    char *end;

    while ( *start!='\0' && *start!='[' && *start!='{' ) ++start;
    if ( *start=='[' || *start=='{' ) ++start;
    for ( i=0; i<maxentries && *start!=']' && *start!='}'; ++i ) {
	array[i] = (int) strtod(start,&end);
	if ( start==end )
return;
	start = end;
	while ( isspace(*start) ) ++start;
    }
}

static void fillrealarray(real *array,char *start,int maxentries) {
    int i;
    char *end;

    while ( *start!='\0' && *start!='[' && *start!='{' ) ++start;
    if ( *start=='[' || *start=='{' ) ++start;
    for ( i=0; i<maxentries && *start!=']' && *start!='}'; ++i ) {
	array[i] = strtod(start,&end);
	if ( start==end )
return;
	start = end;
	while ( isspace(*start) ) ++start;
    }
}

static void InitDict(struct psdict *dict,char *line) {
    while ( *line!='/' && *line!='\0' ) ++line;
    while ( !isspace(*line) && *line!='\0' ) ++line;
    dict->cnt += strtol(line,NULL,10);
    if ( dict->next>0 ) { int i;		/* Shouldn't happen, but did in a bad file */
	dict->keys = grealloc(dict->keys,dict->cnt*sizeof(char *));
	dict->values = grealloc(dict->values,dict->cnt*sizeof(char *));
	for ( i=dict->next; i<dict->cnt; ++i ) {
	    dict->keys[i] = NULL; dict->values[i] = NULL;
	}
    } else {
	dict->keys = gcalloc(dict->cnt,sizeof(char *));
	dict->values = gcalloc(dict->cnt,sizeof(char *));
    }
}

static void InitChars(struct pschars *chars,char *line) {
    while ( *line!='/' && *line!='\0' ) ++line;
    while ( !isspace(*line) && *line!='\0' ) ++line;
    chars->cnt = strtol(line,NULL,10);
    if ( chars->cnt>0 ) {
	chars->keys = gcalloc(chars->cnt,sizeof(char *));
	chars->values = gcalloc(chars->cnt,sizeof(char *));
	chars->lens = gcalloc(chars->cnt,sizeof(int));
	GProgressChangeTotal(chars->cnt);
    }
}

static void InitCharProcs(struct charprocs *cp, char *line) {
    while ( *line!='/' && *line!='\0' ) ++line;
    while ( !isspace(*line) && *line!='\0' ) ++line;
    cp->cnt = strtol(line,NULL,10);
    if ( cp->cnt>0 ) {
	cp->keys = gcalloc(cp->cnt,sizeof(char *));
	cp->values = gcalloc(cp->cnt,sizeof(SplineChar *));
	GProgressChangeTotal(cp->cnt);
    }
}

static int mycmp(char *str,char *within, char *end ) {
    while ( within<end ) {
	if ( *str!=*within )
return( *str-*within );
	++str; ++within;
    }
return( *str=='\0'?0:1 );
}

static void ContinueValue(struct fontparse *fp, struct psdict *dict, char *line) {
    int incomment = false;

    while ( *line ) {
	if ( !fp->instring && fp->depth==0 &&
		(strncmp(line,"def",3)==0 ||
		 strncmp(line,"|-",2)==0 || strncmp(line,"ND",2)==0)) {
	    while ( 1 ) {
		while ( fp->vpt>fp->vbuf+1 && isspace(fp->vpt[-1]) )
		    --fp->vpt;
		if ( fp->vpt>fp->vbuf+8 && strncmp(fp->vpt-8,"noaccess",8)==0 )
		    fp->vpt -= 8;
		else if ( fp->vpt>fp->vbuf+8 && strncmp(fp->vpt-8,"readonly",8)==0 )
		    fp->vpt -= 8;
		else
	    break;
	    }
	    dict->values[dict->next] = copyn(fp->vbuf,fp->vpt-fp->vbuf);
	    ++dict->next;
	    fp->vpt = fp->vbuf;
	    fp->multiline = false;
return;
	}
	if ( fp->vpt>=fp->vmax ) {
	    int len = fp->vmax-fp->vbuf+1000, off=fp->vpt-fp->vbuf;
	    fp->vbuf = grealloc(fp->vbuf,len);
	    fp->vpt = fp->vbuf+off;
	    fp->vmax = fp->vbuf+len;
	}
	if ( fp->instring ) {
	    if ( *line==')' ) fp->instring=false;
	} else if ( incomment ) {
	    /* Do Nothing */;
	} else if ( *line=='(' )
	    fp->instring = true;
	else if ( *line=='%' )
	    incomment = true;
	else if ( *line=='[' || *line=='{' )
	    ++fp->depth;
	else if ( *line=='}' || *line==']' )
	    --fp->depth;
	*fp->vpt++ = *line++;
    }
}

static void AddValue(struct fontparse *fp, struct psdict *dict, char *line, char *endtok) {
    char *pt;
    /* Doesn't work for Erode (or any multi-line entry) */

    if ( dict->next>=dict->cnt ) {
	dict->cnt += 10;
	dict->keys = grealloc(dict->keys,dict->cnt*sizeof(char *));
	dict->values = grealloc(dict->values,dict->cnt*sizeof(char *));
    }
    dict->keys[dict->next] = copyn(line+1,endtok-(line+1));
    pt = line+strlen(line)-1;
    while ( isspace(*endtok)) ++endtok;
    while ( pt>endtok && isspace(*pt)) --pt;
    ++pt;
    if ( strncmp(pt-3,"def",3)==0 )
	pt -= 3;
    else if ( strncmp(pt-2,"|-",2)==0 || strncmp(pt-2,"ND",2)==0 )
	pt -= 2;
    else {
	fp->multiline = true;
	ContinueValue(fp,dict,endtok);
return;
    }
    while ( pt-1>endtok && isspace(pt[-1])) --pt;
    dict->values[dict->next] = copyn(endtok,pt-endtok);
    ++dict->next;
}

static void parseline(struct fontparse *fp,char *line,FILE *in) {
    char buffer[200], *pt, *endtok;

    if ( line[0]=='%' && !fp->multiline )
return;

    if ( fp->inencoding && strncmp(line,"dup",3)==0 ) {
	/* Metamorphasis has multiple entries on a line */
	while ( strncmp(line,"dup",3)==0 ) {
	    char *end;
	    int pos = strtol(line+3,&end,10);
	    line = end;
	    while ( isspace( *line )) ++line;
	    if ( *line=='/' ) ++line;
	    for ( pt = buffer; !isspace(*line); *pt++ = *line++ );
	    *pt = '\0';
	    if ( pos>=0 && pos<256 )
		fp->fd->encoding[pos] = strdup(buffer);
	    while ( isspace(*line)) ++line;
	    if ( strncmp(line,"put",3)==0 ) line+=3;
	    while ( isspace(*line)) ++line;
	}
return;
    } else if ( fp->inencoding && strstr(line,"0 1 255")!=NULL ) {
	/* the T1 spec I've got doesn't allow for this, but I've seen it anyway*/
	/* 0 1 255 {1 index exch /.notdef put} for */
	int i;
	for ( i=0; i<256; ++i )
	    fp->fd->encoding[i] = strdup(".notdef");
return;
    }
    fp->inencoding = 0;

    while ( isspace(*line)) ++line;
    endtok = NULL;
    if ( *line=='/' )
	for ( endtok=line+1; !isspace(*endtok) && *endtok!='(' &&
		*endtok!='{' && *endtok!='[' && *endtok!='\0'; ++endtok );
    if ( fp->infi ) {
	if ( endtok==NULL && strncmp(line,"end", 3)==0 ) {
	    fp->infi=0;
return;
	} else if ( endtok==NULL )
return;
	if ( mycmp("version",line+1,endtok)==0 )
	    fp->fd->fontinfo->version = getstring(endtok);
	else if ( mycmp("Notice",line+1,endtok)==0 )
	    fp->fd->fontinfo->notice = getlongstring(endtok,in);
	else if ( mycmp("Copyright",line+1,endtok)==0 )		/* cff spec allows for copyright and notice */
	    fp->fd->fontinfo->notice = getlongstring(endtok,in);
	else if ( mycmp("FullName",line+1,endtok)==0 )
	    fp->fd->fontinfo->fullname = getstring(endtok);
	else if ( mycmp("FamilyName",line+1,endtok)==0 )
	    fp->fd->fontinfo->familyname = getstring(endtok);
	else if ( mycmp("Weight",line+1,endtok)==0 )
	    fp->fd->fontinfo->weight = getstring(endtok);
	else if ( mycmp("ItalicAngle",line+1,endtok)==0 )
	    fp->fd->fontinfo->italicangle = strtod(endtok,NULL);
	else if ( mycmp("UnderlinePosition",line+1,endtok)==0 )
	    fp->fd->fontinfo->underlineposition = strtod(endtok,NULL);
	else if ( mycmp("UnderlineThickness",line+1,endtok)==0 )
	    fp->fd->fontinfo->underlinethickness = strtod(endtok,NULL);
	else if ( mycmp("isFixedPitch",line+1,endtok)==0 )
	    fp->fd->fontinfo->isfixedpitch = getbool(endtok);
	else if ( mycmp("em",line+1,endtok)==0 )
	    fp->fd->fontinfo->em = strtol(endtok,NULL,10);
	else if ( mycmp("ascent",line+1,endtok)==0 )
	    fp->fd->fontinfo->ascent = strtol(endtok,NULL,10);
	else if ( mycmp("descent",line+1,endtok)==0 )
	    fp->fd->fontinfo->descent = strtol(endtok,NULL,10);
	else if ( mycmp("FSType",line+1,endtok)==0 )
	    fp->fd->fontinfo->fstype = strtol(endtok,NULL,10);
	else if ( !fp->alreadycomplained ) {
	    fprintf( stderr, "Didn't understand |%s", line );
	    fp->alreadycomplained = true;
	}
    } else if ( fp->inprivate ) {
	if ( strstr(line,"/CharStrings")!=NULL ) {
	    if ( strstr(line,"dict")==NULL )		/* In my type0 fonts, the string "/CharStrings" pops up many times, only first time is meaningful */
return;
	    if ( fp->fd->chars->next==0 )
		InitChars(fp->fd->chars,line);
	    fp->inchars = 1;
	    fp->insubs = 0;
return;
	} else if ( strstr(line,"/Subrs")!=NULL ) {
	    if ( fp->fd->private->subrs->next>0 )
return;
	    InitChars(fp->fd->private->subrs,line);
	    fp->insubs = 1;
	    fp->inchars = 0;
return;
	} else if ( fp->multiline ) {
	    ContinueValue(fp,fp->fd->private->private,line);
return;
	}
	if ( endtok==NULL ) {
	    char *pt = line;
	    if ( *pt!='/' ) while ( (pt=strstr(pt,"end"))!=NULL ) {
		if ( fp->inchars ) fp->inchars = false;
		else fp->inprivate = false;
		pt += 3;
	    }
return;
	}
	if ( mycmp("ND",line+1,endtok)==0 || mycmp("|-",line+1,endtok)==0 ||
		mycmp("NP",line+1,endtok)==0 || mycmp("|",line+1,endtok)==0 ||
		mycmp("RD",line+1,endtok)==0 || mycmp("-|",line+1,endtok)==0 ||
		mycmp("password",line+1,endtok)==0 ||
		mycmp("MinFeature",line+1,endtok)==0 )
	    /* These conveigh no information, but are required */;
	else {
	    if ( mycmp("lenIV",line+1,endtok)==0 )
		fp->fd->private->leniv = strtol(endtok,NULL,10);	/* We need this value */
	    AddValue(fp,fp->fd->private->private,line,endtok);
	}
    } else if ( fp->incidsysteminfo ) {
	if ( endtok==NULL && strncmp(line,"end", 3)==0 ) {
	    fp->incidsysteminfo=0;
return;
	} else if ( endtok==NULL )
return;
	if ( mycmp("Registry",line+1,endtok)==0 )
	    fp->fd->registry = getstring(endtok);
	else if ( mycmp("Ordering",line+1,endtok)==0 )
	    fp->fd->ordering = getstring(endtok);
	else if ( mycmp("Supplement",line+1,endtok)==0 )		/* cff spec allows for copyright and notice */
	    fp->fd->supplement = strtol(endtok,NULL,0);
    } else {
	if ( strstr(line,"/Private")!=NULL ) {
	    fp->inprivate = 1;
	    InitDict(fp->fd->private->private,line);
return;
	} else if ( strstr(line,"/FontInfo")!=NULL ) {
	    fp->infi = 1;
return;
	} else if ( strstr(line,"/CharStrings")!=NULL ) {
	    if ( strstr(line,"dict")==NULL )		/* In my type0 fonts, the string "/CharStrings" pops up many times, only first time is meaningful */
return;
	    if ( fp->fd->chars->next==0 )
		InitChars(fp->fd->chars,line);
	    fp->inchars = 1;
	    fp->insubs = 0;
return;
	} else if ( mycmp("/CharProcs",line,endtok)==0 ) {
	    InitCharProcs(fp->fd->charprocs,line);
	    fp->insubs = 0;
return;
	} else if ( strstr(line,"/CIDSystemInfo")!=NULL ) {
	    fp->incidsysteminfo = 1;
return;
	}
	if ( endtok==NULL ) {
	    if ( fp->fdindex!=-1 && strstr(line,"end")!=NULL ) {
		if ( ++fp->fdindex>=fp->mainfd->fdcnt )
		    fp->fd = fp->mainfd;
		else
		    fp->fd = fp->mainfd->fds[fp->fdindex];
	    }
return;
	}
	if ( mycmp("FontName",line+1,endtok)==0 )
	    fp->fd->fontname = gettoken(endtok);
	else if ( mycmp("Encoding",line+1,endtok)==0 ) {
	    if ( strstr(endtok,"StandardEncoding")!=NULL ) {
		fp->fd->encoding_name = em_adobestandard;
		setStdEnc(fp->fd->encoding);
	    } else if ( strstr(endtok,"ISOLatin1Encoding")!=NULL ) {
		fp->fd->encoding_name = em_iso8859_1;
		setLatin1Enc(fp->fd->encoding);
	    } else {
		fp->fd->encoding_name = em_none;
		fp->inencoding = 1;
	    }
	} else if ( mycmp("PaintType",line+1,endtok)==0 )
	    fp->fd->painttype = strtol(endtok,NULL,10);
	else if ( mycmp("FontType",line+1,endtok)==0 )
	    fp->fd->fonttype = strtol(endtok,NULL,10);
	else if ( mycmp("FontMatrix",line+1,endtok)==0 )
	     fillrealarray(fp->fd->fontmatrix,endtok,6);
	else if ( mycmp("LanguageLevel",line+1,endtok)==0 )
	    fp->fd->languagelevel = strtol(endtok,NULL,10);
	else if ( mycmp("WMode",line+1,endtok)==0 )
	    fp->fd->wmode = strtol(endtok,NULL,10);
	else if ( mycmp("FontBBox",line+1,endtok)==0 )
	     fillrealarray(fp->fd->fontbb,endtok,4);
	else if ( mycmp("UniqueID",line+1,endtok)==0 )
	    fp->fd->uniqueid = strtol(endtok,NULL,10);
	else if ( mycmp("XUID",line+1,endtok)==0 )
	     fillintarray(fp->fd->xuid,endtok,20);
	else if ( mycmp("StrokeWidth",line+1,endtok)==0 )
	    fp->fd->strokewidth = strtod(endtok,NULL);
	else if ( mycmp("BuildChar",line+1,endtok)==0 )
	    /* Do Nothing */;
	else if ( mycmp("BuildGlyph",line+1,endtok)==0 )
	    /* Do Nothing */;
	else if ( mycmp("CIDFontName",line+1,endtok)==0 )
	    fp->fd->cidfontname = gettoken(endtok);
	else if ( mycmp("CIDFontVersion",line+1,endtok)==0 )
	    fp->fd->cidversion = strtod(endtok,NULL);
	else if ( mycmp("CIDFontType",line+1,endtok)==0 )
	    fp->fd->cidfonttype = strtol(endtok,NULL,10);
	else if ( mycmp("UIDBase",line+1,endtok)==0 )
	    fp->fd->uniqueid = strtol(endtok,NULL,10);
	else if ( mycmp("CIDMapOffset",line+1,endtok)==0 )
	    fp->fd->mapoffset = strtol(endtok,NULL,10);
	else if ( mycmp("FDBytes",line+1,endtok)==0 )
	    fp->fd->fdbytes = strtol(endtok,NULL,10);
	else if ( mycmp("GDBytes",line+1,endtok)==0 )
	    fp->fd->gdbytes = strtol(endtok,NULL,10);
	else if ( mycmp("CIDCount",line+1,endtok)==0 )
	    fp->fd->cidcnt = strtol(endtok,NULL,10);
	else if ( mycmp("FDArray",line+1,endtok)==0 ) { int i;
	    fp->mainfd = fp->fd;
	    fp->fd->fdcnt = strtol(endtok,NULL,10);
	    fp->fd->fds = gcalloc(fp->fd->fdcnt,sizeof(struct fontdict *));
	    for ( i=0; i<fp->fd->fdcnt; ++i )
		fp->fd->fds[i] = MakeEmptyFont();
	    fp->fdindex = 0;
	    fp->fd = fp->fd->fds[0];
	} else if ( mycmp("FontSetInit",line+1,endtok)==0 ) {
	    fp->iscff = true;
	    fp->iscid = false;
	} else if ( mycmp("CIDInit",line+1,endtok)==0 ) {
	    fp->iscid = true;
	    fp->iscff = false;
	} else if ( !fp->alreadycomplained ) {
	    fprintf( stderr, "Didn't understand |%s", line );
	    fp->alreadycomplained = true;
	}
    }
}

static int hex(int ch1, int ch2) {
    if ( ch1>='0' && ch1<='9' )
	ch1 -= '0';
    else if ( ch1>='a' )
	ch1 -= 'a'-10;
    else 
	ch1 -= 'A'-10;
    if ( ch2>='0' && ch2<='9' )
	ch2 -= '0';
    else if ( ch2>='a' )
	ch2 -= 'a'-10;
    else 
	ch2 -= 'A'-10;
return( (ch1<<4)|ch2 );
}

unsigned short r;
#define c1	52845
#define c2	22719

static void initcode(void) {
    r = 55665;
}

static int decode(unsigned char cypher) {
    unsigned char plain = ( cypher ^ (r>>8));
    r = (cypher + r) * c1 + c2;
return( plain );
}

static void dumpzeros(FILE *out, unsigned char *zeros, int zcnt) {
    while ( --zcnt >= 0 )
	fputc(*zeros++,out);
}

static void decodestr(unsigned char *str, int len) {
    unsigned short r = 4330;
    unsigned char plain, cypher;

    while ( len-->0 ) {
	cypher = *str;
	plain = ( cypher ^ (r>>8));
	r = (cypher + r) * c1 + c2;
	*str++ = plain;
    }
}

static void addinfo(struct fontparse *fp,char *line,char *tok,char *binstart,int binlen) {
    decodestr((unsigned char *) binstart,binlen);
    binstart += fp->fd->private->leniv;
    binlen -= fp->fd->private->leniv;

    if ( fp->insubs ) {
	struct pschars *chars = /*fp->insubs ?*/ fp->fd->private->subrs /*: fp->fd->private->othersubrs*/;
	while ( isspace(*line)) ++line;
	if ( strncmp(line,"dup ",4)==0 ) {
	    int i = strtol(line+4,NULL,10);
	    if ( i<chars->cnt ) {
		chars->lens[i] = binlen;
		chars->values[i] = galloc(binlen);
		memcpy(chars->values[i],binstart,binlen);
		if ( i>=chars->next ) chars->next = i+1;
	    } else
		fprintf( stderr, "Index too big (must be <%d) |%s", chars->cnt, line);
	} else if ( !fp->alreadycomplained ) {
	    fprintf( stderr, "Didn't understand |%s", line );
	    fp->alreadycomplained = true;
	}
    } else if ( fp->inchars ) {
	struct pschars *chars = fp->fd->chars;
	if ( *tok=='\0' )
	    fprintf( stderr, "No name for CharStrings dictionary |%s", line );
	else if ( chars->next>=chars->cnt )
	    fprintf( stderr, "Too many entries in CharStrings dictionary |%s", line );
	else {
	    int i = chars->next;
	    chars->lens[i] = binlen;
	    chars->keys[i] = strdup(tok);
	    chars->values[i] = galloc(binlen);
	    memcpy(chars->values[i],binstart,binlen);
	    ++chars->next;
	    GProgressNext();
	}
    } else
	fprintf( stderr, "Shouldn't be in addinfo |%s", line );
}

/* In the book the token which starts a character description is always RD but*/
/*  it's just the name of a subroutine which is defined in the private diction*/
/*  and it could be anything. in one case it was "-|" (hyphen bar) so we can't*/
/*  just look for RD we must be a bit smarter and figure out what the token is*/
/* (oh. I see now. it's allowed to be either one "RD" or "-|", but nothing else*/
/*  right) */
/* It's defined as {string currentfile exch readstring pop} so look for that */
/* Except that in gsf files we've also go "/-!{string currentfile exch readhexstring pop} readonly def" */
/*  NOTE: readhexstring!!! */
static int glorpline(struct fontparse *fp, FILE *temp, char *rdtok) {
    static char *buffer=NULL, *end;
    char *pt, *binstart;
    int binlen;
    int ch;
    int innum, val=0, inbinary, cnt=0, inr, wasspace, nownum, nowr, nowspace, sptok;
    char *rdline = "{string currentfile exch readstring pop}", *rpt;
    char *tokpt = NULL, *rdpt;
    char temptok[255];
    int intok, first;
    int wasminus=false, isminus, nibble, firstnibble, inhex;

    ch = getc(temp);
    if ( ch==EOF )
return( 0 );
    ungetc(ch,temp);

    if ( buffer==NULL ) {
	buffer = galloc(3000);
	end = buffer+3000;
    }
    innum = inr = 0; wasspace = 0; inbinary = 0; rpt = NULL; rdpt = NULL;
    inhex = 0;
    pt = buffer; binstart=NULL; binlen = 0; intok=0; sptok=0; first=1;
    temptok[0] = '\0';
    while ( (ch=getc(temp))!=EOF ) {
	if ( pt>=end ) {
	    char *old = buffer;
	    int len = (end-buffer)+2000;
	    buffer = grealloc(buffer,len);
	    end = buffer+len;
	    pt = buffer+(pt-old);
	    binstart = buffer+(binstart-old);
	}
	*pt++ = ch;
	isminus = ch=='-' && wasspace;
	nownum = nowspace = nowr = 0;
	if ( inbinary ) {
	    if ( --cnt==0 )
		inbinary = 0;
	} else if ( inhex ) {
	    if ( ishexdigit(ch)) {
		int h;
		if ( isdigit(ch)) h = ch-'0';
		else if ( ch>='a' && ch<='f' ) h = ch-'a'+10;
		else h = ch-'A'+10;
		if ( firstnibble ) {
		    nibble = h;
		    --pt;
		} else {
		    pt[-1] = (nibble<<4)|h;
		    if ( --cnt==0 )
			inbinary = inhex = 0;
		}
		firstnibble = !firstnibble;
	    } else {
		--pt;
		/* skip everything not hex */
	    }
	} else if ( ch=='/' ) {
	    intok = 1;
	    tokpt = temptok;
	} else if ( intok && !isspace(ch) && ch!='{' && ch!='[' ) {
	    *tokpt++ = ch;
	} else if ( (intok||sptok) && (ch=='{' || ch=='[')) {
	    *tokpt = '\0';
	    rpt = rdline+1;
	    intok = sptok = 0;
	} else if ( intok ) {
	    *tokpt = '\0';
	    intok = 0;
	    sptok = 1;
	} else if ( sptok && isspace(ch)) {
	    nowspace = 1;
	    if ( ch=='\n' || ch=='\r' )
    break;
	} else if ( sptok && !isdigit(ch))
	    sptok = 0;
	else if ( rpt!=NULL && ch==*rpt ) {
	    if ( *++rpt=='\0' ) {
		/* it matched the character definition string so this is the */
		/*  token we want to search for */
		strcpy(rdtok,temptok);
	    }
	} else if ( isdigit(ch)) {
	    sptok = 0;
	    nownum = 1;
	    if ( innum )
		val = 10*val + ch-'0';
	    else
		val = ch-'0';
	} else if ( isspace(ch)) {
	    nowspace = 1;
	    if ( ch=='\n' || ch=='\r' )
    break;
	} else if ( wasspace && ch==*rdtok ) {
	    nowr = 1;
	    rdpt = rdtok+1;
	} else if ( inr && ch==*rdpt ) {
	    if ( *++rdpt =='\0' ) {
		ch = getc(temp);
		*pt++ = ch;
		if ( isspace(ch) && val!=0 ) {
		    inbinary = 1;
		    cnt = val;
		    binstart = pt;
		    binlen = val;
		}
	    } else
		nowr = 1;
	} else if ( wasminus && ch=='!' ) {
	    ch = getc(temp);
	    *pt++ = ch;
	    if ( isspace(ch) && val!=0 ) {
		inhex = 1;
		cnt = val;
		binstart = pt;
		binlen = val;
		firstnibble = true;
	    }
	}
	innum = nownum; wasspace = nowspace; inr = nowr;
	wasminus = isminus;
	first = 0;
    }
    *pt = '\0';
    if ( binstart==NULL ) {
	parseline(fp,buffer,temp);
    } else {
	addinfo(fp,buffer,temptok,binstart,binlen);
    }
return( 1 );
}

static int nrandombytes[4];
#define EODMARKLEN	16

static void decrypteexec(FILE *in,FILE *temp, int hassectionheads) {
    int ch1, ch2, ch3, ch4, binary;
    int zcnt;
    unsigned char zeros[EODMARKLEN+6+1];

    while ( (ch1=getc(in))!=EOF && isspace(ch1));
    if ( ch1==0200 && hassectionheads ) {
	/* skip the 6 byte section header in pfb files that follows eexec */
	ch1 = getc(in);
	ch1 = getc(in);
	ch1 = getc(in);
	ch1 = getc(in);
	ch1 = getc(in);
	ch1 = getc(in);
    }
    ch2 = getc(in); ch3 = getc(in); ch4 = getc(in);
    binary = 0;
    if ( ch1<'0' || (ch1>'9' && ch1<'A') || ( ch1>'F' && ch1<'a') || (ch1>'f') ||
	     ch2<'0' || (ch2>'9' && ch2<'A') || (ch2>'F' && ch2<'a') || (ch2>'f') ||
	     ch3<'0' || (ch3>'9' && ch3<'A') || (ch3>'F' && ch3<'a') || (ch3>'f') ||
	     ch4<'0' || (ch4>'9' && ch4<'A') || (ch4>'F' && ch4<'a') || (ch4>'f') )
	binary = 1;
    if ( ch1==EOF || ch2==EOF || ch3==EOF || ch4==EOF ) {
return;
    }

    initcode();
    if ( binary ) {
	nrandombytes[0] = decode(ch1);
	nrandombytes[1] = decode(ch2);
	nrandombytes[2] = decode(ch3);
	nrandombytes[3] = decode(ch4);
	zcnt = 0;
	while (( ch1=getc(in))!=EOF ) {
	    if ( hassectionheads ) {
		if ( (zcnt>=1 && zcnt<6) || ( ch1==0200 && zcnt==0 ))
		    zeros[zcnt++] = decode(ch1);
		else if ( zcnt>=6 && isspace(ch1) )
		    zeros[zcnt++] = decode(ch1);
		else if ( zcnt>=6 && ch1=='0' ) {
		    zeros[zcnt++] = decode(ch1);
		    if ( zcnt>EODMARKLEN+7 )
	break;
		} else {
		    dumpzeros(temp,zeros,zcnt);
		    zcnt = 0;
		    putc(decode(ch1),temp);
		}
	    } else {
		if ( ch1=='0' ) ++zcnt; else {dumpzeros(temp,zeros,zcnt); zcnt = 0; }
		if ( zcnt>EODMARKLEN )
	break;
		if ( zcnt==0 )
		    putc(decode(ch1),temp);
		else
		    zeros[zcnt-1] = decode(ch1);
	    }
	}
    } else {
	nrandombytes[0] = decode(hex(ch1,ch2));
	nrandombytes[1] = decode(hex(ch3,ch4));
	ch1 = getc(in); ch2 = getc(in); ch3 = getc(in); ch4 = getc(in);
	nrandombytes[2] = decode(hex(ch1,ch2));
	nrandombytes[3] = decode(hex(ch3,ch4));
	zcnt = 0;
	while (( ch1=getc(in))!=EOF ) {
	    while ( ch1!=EOF && isspace(ch1)) ch1 = getc(in);
	    while ( (ch2=getc(in))!=EOF && isspace(ch2));
	    if ( ch1=='0' && ch2=='0' ) ++zcnt; else { dumpzeros(temp,zeros,zcnt); zcnt = 0;}
	    if ( zcnt>EODMARKLEN )
	break;
	    if ( zcnt==0 )
		putc(decode(hex(ch1,ch2)),temp);
	    else
		zeros[zcnt-1] = decode(hex(ch1,ch2));
	}
    }
    while (( ch1=getc(in))=='0' || isspace(ch1) );
    if ( ch1!=EOF ) ungetc(ch1,in);
}

static void decryptagain(struct fontparse *fp,FILE *temp,char *rdtok) {
    while ( glorpline(fp,temp,rdtok));
}

static void parsetype3(struct fontparse *fp,FILE *in) {
    PSFontInterpretPS(in,fp->fd->charprocs);
}

static unsigned char *readt1str(FILE *temp,int offset,int len,int leniv) {
    int i;
    unsigned char *str, *pt;
    unsigned short r = 4330;
    unsigned char plain, cypher;
    /* The CID spec doesn't mention this, but the type 1 strings are all */
    /*  eexec encrupted (with the nested encryption). Remember leniv varies */
    /*  from fd to fd (potentially) */
    /* I'm told (by Ian Kemmish) that leniv==-1 => no eexec encryption */

    fseek(temp,offset,SEEK_SET);
    if ( leniv<0 ) {
	str = pt = galloc(len+1);
	for (; i<len; ++i )
	    *pt++ = getc(temp);
    } else {
	for ( i=0; i<leniv; ++i ) {
	    cypher = getc(temp);
	    plain = ( cypher ^ (r>>8));
	    r = (cypher + r) * c1 + c2;
	}
	str = pt = galloc(len-leniv+1);
	for (; i<len; ++i ) {
	    cypher = getc(temp);
	    plain = ( cypher ^ (r>>8));
	    r = (cypher + r) * c1 + c2;
	    *pt++ = plain;
	}
    }
    *pt = '\0';
return( str );
}

static void figurecids(struct fontparse *fp,FILE *temp) {
    struct fontdict *fd = fp->mainfd;
    int i,j,k,val;
    int *offsets;
    int cidcnt = fd->cidcnt;
    int leniv;
    /* Some cid formats don't have any of these */

    fd->cidstrs = galloc(cidcnt*sizeof(uint8 *));
    fd->cidlens = galloc(cidcnt*sizeof(int16));
    fd->cidfds = galloc((cidcnt+1)*sizeof(int16));
    offsets = galloc((cidcnt+1)*sizeof(int));
    GProgressChangeTotal(cidcnt);

    fseek(temp,fd->mapoffset,SEEK_SET);
    for ( i=0; i<=fd->cidcnt; ++i ) {
	for ( j=val=0; j<fd->fdbytes; ++j )
	    val = (val<<8) + getc(temp);
	fd->cidfds[i] = val;
	for ( j=val=0; j<fd->gdbytes; ++j )
	    val = (val<<8) + getc(temp);
	offsets[i] = val;
	if ( i!=0 )
	    fd->cidlens[i-1] = offsets[i]-offsets[i-1];
    }

    for ( i=0; i<fd->cidcnt; ++i ) {
	if ( fd->cidlens[i]== 0 )
	    fd->cidstrs[i] = NULL;
	else {
	    fd->cidstrs[i] = readt1str(temp,offsets[i],fd->cidlens[i],
		    fd->fds[fd->cidfds[i]]->private->leniv);
	    fd->cidlens[i] -= fd->fds[fd->cidfds[i]]->private->leniv;
	}
	GProgressNext();
    }
    free(offsets);

    for ( k=0; k<fd->fdcnt; ++k ) {
	struct private *private = fd->fds[k]->private;
	char *ssubroff = PSDictHasEntry(private->private,"SubrMapOffset");
	char *ssbbytes = PSDictHasEntry(private->private,"SDBytes");
	char *ssubrcnt = PSDictHasEntry(private->private,"SubrCount");
	int subroff, sbbytes, subrcnt;

	if ( ssubroff!=NULL && ssbbytes!=NULL && ssubrcnt!=NULL &&
		(subroff=strtol(ssubroff,NULL,10))>=0 &&
		(sbbytes=strtol(ssbbytes,NULL,10))>0 &&
		(subrcnt=strtol(ssubrcnt,NULL,10))>0 ) {
	    private->subrs->cnt = subrcnt;
	    private->subrs->values = gcalloc(subrcnt,sizeof(char *));
	    private->subrs->lens = gcalloc(subrcnt,sizeof(int));
	    leniv = private->leniv;
	    offsets = galloc((subrcnt+1)*sizeof(int));
	    fseek(temp,subroff,SEEK_SET);
	    for ( i=0; i<=subrcnt; ++i ) {
		for ( j=val=0; j<fd->gdbytes; ++j )
		    val = (val<<8) + getc(temp);
		offsets[i] = val;
		if ( i!=0 )
		    private->subrs->lens[i-1] = offsets[i]-offsets[i-1];
	    }
	    for ( i=0; i<subrcnt; ++i ) {
		private->subrs->values[i] = readt1str(temp,offsets[i],
			private->subrs->lens[i],leniv);
	    }
	    free(offsets);
	}
	PSDictRemoveEntry(private->private,"SubrMapOffset");
	PSDictRemoveEntry(private->private,"SDBytes");
	PSDictRemoveEntry(private->private,"SubrCount");
    }
}

static void dodata( struct fontparse *fp, FILE *in, FILE *temp) {
    int binary, cnt, len;
    int ch, ch2;
    char *pt;
    char fontsetname[256];

    while ( (ch=getc(in))!='(' && ch!='/' && ch!=EOF );
    if ( ch=='/' ) {
	/* There appears to be no provision for a hex encoding here */
	/* Why can't they use the same format for routines with the same name? */
	binary = true;
	for ( pt=fontsetname; (ch=getc(in))!=' ' && ch!=EOF; )
	    if ( pt<fontsetname+sizeof(fontsetname)-1 )
		*pt++= ch;
	*pt = '\0';
    } else {
	if ( (ch=getc(in))=='B' || ch=='b' ) binary = true;
	else if ( ch=='H' || ch=='h' ) binary = false;
	else {
	    binary = true;		/* Who knows? */
	    fprintf( stderr, "Failed to parse the StartData command properly\n" );
	}
	fontsetname[0] = '\0';
    }
    while ( (ch=getc(in))!=')' && ch!=EOF );
    if ( fscanf( in, "%d", &len )!=1 || len<=0 ) {
	len = 0;
	fprintf( stderr, "Failed to parse the StartData command properly, bad cnt\n" );
    }
    cnt = len;
    while ( isspace(ch=getc(in)) );
    ungetc(ch,in);
    for ( pt="StartData "; *pt; ++pt )
	getc(in);			/* And if it didn't match, what could I do about it? */
    if ( binary ) {
	while ( cnt>0 ) {
	    ch = getc(in);
	    putc(ch,temp);
	    --cnt;
	}
    } else {
	while ( cnt>0 ) {
	    /* Hex data are allowed to contain whitespace */
	    while ( isspace(ch=getc(in)) );
	    while ( isspace(ch2=getc(in)) );
	    ch = hex(ch,ch2);
	    putc(ch,temp);
	    --cnt;
	}
	if ( (ch=getc(in))!='>' ) ungetc(ch,in);
    }
    rewind(temp);
    if ( fp->iscid )
	figurecids(fp,temp);
    else {
	fp->fd->sf = CFFParse(temp,len,fontsetname);
	fp->fd->wascff = true;
    }
}

static void realdecrypt(struct fontparse *fp,FILE *in, FILE *temp) {
    char buffer[256];
    int first, hassectionheads;
    char rdtok[20];

    strcpy(rdtok,"RD");

    first = 1; hassectionheads = 0;
    while ( myfgets(buffer,sizeof(buffer),in)!=NULL ) {
	if ( first && buffer[0]=='\200' ) {
	    hassectionheads = 1;
	    fp->fd->wasbinary = true;
	    parseline(fp,buffer+6,in);
	} else
	    parseline(fp,buffer,in);
	first = 0;
	if ( strstr(buffer,"%%BeginData: ")!=NULL )
    break;
	if ( strstr(buffer,"currentfile")!=NULL && strstr(buffer, "eexec")!=NULL )
    break;
	if ( strstr(buffer,"CharProcs")!=NULL && strstr(buffer,"begin")!=NULL ) {
	    parsetype3(fp,in);
return;
	}
	if ( strstr(buffer,"CharStrings")!=NULL && strstr(buffer,"begin")!=NULL ) {
	    /* gsf files are not eexec encoded, but the charstrings are encoded*/
	    decryptagain(fp,in,rdtok);
return;
	}
    }

    if ( strstr(buffer,"%%BeginData: ")!=NULL ) {
	/* used by both CID fonts and CFF fonts (and chameleons, whatever they are) */
	dodata(fp,in,temp);
    } else {
	decrypteexec(in,temp,hassectionheads);
	rewind(temp);
	decryptagain(fp,temp,rdtok);
	while ( myfgets(buffer,sizeof(buffer),in)!=NULL ) {
	    if ( buffer[0]!='\200' || !hassectionheads )
		parseline(fp,buffer,in);
	}
    }
}

FontDict *ReadPSFont(char *fontname) {
    FILE *in, *temp;
    struct fontparse fp;
    char *oldloc;

    in = fopen(fontname,"r");
    if ( in==NULL ) {
	fprintf( stderr, "Cannot open %s\n", fontname );
return(NULL);
    }

    temp = tmpfile();
    if ( temp==NULL ) {
	fprintf( stderr, "Cannot open a temporary file\n" );
	fclose(in); 
return(NULL);
    }

    oldloc = setlocale(LC_NUMERIC,"C");
    memset(&fp,'\0',sizeof(fp));
    fp.fd = fp.mainfd = PSMakeEmptyFont();
    fp.fdindex = -1;
    realdecrypt(&fp,in,temp);
    setlocale(LC_NUMERIC,oldloc);

    fclose(in); fclose(temp);
return( fp.fd );
}

void PSCharsFree(struct pschars *chrs) {
    int i;

    if ( chrs==NULL )
return;
    for ( i=0; i<chrs->next; ++i ) {
	if ( chrs->keys!=NULL ) free(chrs->keys[i]);
	free(chrs->values[i]);
    }
    free(chrs->lens);
    free(chrs->keys);
    free(chrs->values);
    free(chrs);
}

void PSDictFree(struct psdict *dict) {
    int i;

    if ( dict==NULL )
return;
    for ( i=0; i<dict->next; ++i ) {
	if ( dict->keys!=NULL ) free(dict->keys[i]);
	free(dict->values[i]);
    }
    free(dict->keys);
    free(dict->values);
    free(dict);
}

static void PrivateFree(struct private *prv) {
    PSCharsFree(prv->subrs);
#if 1
    PSDictFree(prv->private);
#else
    PSCharsFree(prv->othersubrs);
    free(prv->minfeature);
    free(prv->nd);
    free(prv->np);
    free(prv->rd);
#endif
    free(prv);
}

static void FontInfoFree(struct fontinfo *fi) {
    free(fi->familyname);
    free(fi->fullname);
    free(fi->notice);
    free(fi->weight);
    free(fi->version);
    free(fi);
}

void PSFontFree(FontDict *fd) {
    int i;

    for ( i=0; i<256; ++i )
	free( fd->encoding[i]);
    free(fd->fontname);
    free(fd->cidfontname);
    free(fd->registry);
    free(fd->ordering);
    FontInfoFree(fd->fontinfo);
    PSCharsFree(fd->chars);
    PrivateFree(fd->private);
    if ( fd->charprocs!=NULL ) {
	for ( i=0; i<fd->charprocs->cnt; ++i )
	    free(fd->charprocs->keys[i]);
	free(fd->charprocs->keys);
	free(fd->charprocs->values);
	free(fd->charprocs);
    }
    for ( i=0; i<fd->cidcnt; ++i )
	free( fd->cidstrs[i]);
    free(fd->cidstrs);
    free(fd->cidlens);
    free(fd->cidfds);
    for ( i=0; i<fd->fdcnt; ++i )
	PSFontFree(fd->fds[i]);
    free(fd->fds);
    free(fd);
}
