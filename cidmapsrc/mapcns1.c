#include <stdio.h>
#include <ctype.h>
#include <string.h>

static char used[0x110000];
static int cid_2_unicode[0x10000];
static char *nonuni_names[0x10000];
static int cid_2_rotunicode[0x10000];

#define VERTMARK 0x1000000

static int ucs2_score(int val) {		/* Supplied by KANOU Hiroki */
    if ( val>=0x2e80 && val<=0x2fff )
return( 1 );				/* New CJK Radicals are least important */
    else if ( val>=VERTMARK )
return( 2 );				/* Then vertical guys */
    else if ( val>=0xf000 && val<=0xffff )
return( 3 );
/*    else if (( val>=0x3400 && val<0x3dff ) || (val>=0x4000 && val<=0x4dff))*/
    else if ( val>=0x3400 && val<=0x4dff )
return( 4 );
    else
return( 5 );
}

static int allstars(char *buffer) {
    while ( isdigit(*buffer))
	++buffer;
    while ( *buffer=='\t' || *buffer=='*' )
	++buffer;
    if ( *buffer=='\r' ) ++buffer;
    if ( *buffer=='\n' ) ++buffer;
return( *buffer=='\0' );
}

static int getnth(char *buffer, int col) {
    int i, val=0, best;
    char *end;
    int vals[10];

    if ( col==1 ) {
	/* first column is decimal, others are hex */
	if ( !isdigit(*buffer))
return( -1 );
	while ( isdigit(*buffer))
	    val = 10*val + *buffer++-'0';
return( val );
    }
    for ( i=1; i<col; ++buffer ) {
	if ( *buffer=='\t' )
	    ++i;
	else if ( *buffer=='\0' )
return( -1 );
    }
    val = strtol(buffer,&end,16);
    if ( end==buffer )
return( -1 );
    if ( *end=='v' ) {
	val += VERTMARK;
	++end;
    }
    if ( *end==',' ) {
	/* Multiple guess... now we've got to pick one */
	vals[0] = val;
	i = 1;
	while ( *end==',' && i<9 ) {
	    buffer = end+1;
	    vals[i] = strtol(buffer,&end,16);
	    if ( *end=='v' ) {
		vals[i] += VERTMARK;
		++end;
	    }
	    ++i;
	}
	vals[i] = 0;
	best = 0; val = -1;
	for ( i=0; vals[i]!=0; ++i ) {
	    if ( ucs2_score(vals[i])>best ) {
		val = vals[i];
		best = ucs2_score(vals[i]);
	    }
	}
    }

return( val );
}

int main(int argc, char **argv) {
    char buffer[600];
    int cid, uni, max=0, maxcid=0, i,j;
    extern char *psunicodenames[];

    nonuni_names[0] = ".notdef";
    for ( cid=0; cid<0x10000; ++cid ) cid_2_unicode[cid] = -1;

    while ( fgets(buffer,sizeof(buffer),stdin)!=NULL ) {
	if ( *buffer=='#' /*|| allstars(buffer)*/)
    continue;
	cid = getnth(buffer,1);
	uni = getnth(buffer,11);
	if ( cid==-1 )
    continue;
	maxcid = cid;
	if ( (cid>=17408 && cid<=17599) || cid==17604 || cid==17605 ) {
	    if ( cid>=17408 && cid<=17505 )		/* proportional */
		sprintf( buffer,"cid_%d.vert", cid-17408+1 );
	    else if ( cid>=17506 && cid<=17599 )	/* Half width */
		sprintf( buffer,"cid_%d.vert", cid-17506+13648 );
	    else if ( cid==17604 )
		strcpy( buffer, "cid_17601.vert" );
	    else if ( cid==17605 )
		strcpy( buffer, "cid_17603.vert" );
	    nonuni_names[cid] = strdup(buffer);
	/* Adobe's CNS cids have the rotated and non-rotated forms intermixed */
	} else if ( (cid>=120 && cid<=127 ) && (cid&1) ) {
	    sprintf( buffer,"cid_%d.vert", cid-1 );
	    nonuni_names[cid] = strdup(buffer);
	} else if ( (cid>=128 && cid<=159) && (cid&2) ) {
	    sprintf( buffer,"cid_%d.vert", cid-2 );
	    nonuni_names[cid] = strdup(buffer);
	} else if ( cid>=13648 && cid<=13741 ) {
	    sprintf( buffer, "uni%04X.hw", cid-13648+' ' );
	    nonuni_names[cid] = strdup(buffer);
	} else if ( cid==13742 ) {
	    strcpy( buffer, "uni203E.hw" );
	} else if ( uni==-1 ) {
    continue;
	    sprintf( buffer,"cns1_%d", cid );
	    nonuni_names[cid] = strdup(buffer);
	} else if ( uni>=VERTMARK ) {
	    /* rotated */
	    cid_2_rotunicode[cid] = uni-VERTMARK;
	} else if ( !used[uni] ) {
	    used[uni] = 1;
	    cid_2_unicode[cid] = uni;
	} else {
	    sprintf( buffer, "uni%04X.dup%d", uni, ++used[uni] );
	    nonuni_names[cid] = strdup(buffer);
	}
	max = cid;
    }
    for ( i=0; i<maxcid; ++i ) if ( cid_2_rotunicode[i]!=0 ) {
	for ( j=0; j<maxcid; ++j )
	    if ( cid_2_unicode[j] == cid_2_rotunicode[i] )
	break;
	if ( j==maxcid )
	    sprintf( buffer, "uni%04X.vert", cid_2_rotunicode[i]);
	else
	    sprintf( buffer, "cid_%d.vert", j);
	nonuni_names[i] = strdup(buffer);
    }

    printf("%d %d\n",maxcid, max );

    for ( cid=0; cid<=max; ++cid ) {
	if ( cid_2_unicode[cid]!=-1 ) {
	    for ( i=1; cid+i<=max && cid_2_unicode[cid+i]==cid_2_unicode[cid]+i; ++i );
	    --i;
	    if ( i!=0 ) {
		printf( "%d..%d %04x\n", cid, cid+i, cid_2_unicode[cid] );
		cid += i;
	    } else
		printf( "%d %04x\n", cid, cid_2_unicode[cid] );
	} else if ( nonuni_names[cid]!=NULL )
	    printf( "%d /%s\n", cid, nonuni_names[cid] );
    }
return( 0 );
}
