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
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <utype.h>
#include <ustring.h>
#include <chardata.h>

/* This file contains utility routines for 2 order bezier splines (ie. truetype) */
/* The most interesting thing */
/*  it does is to figure out a quadratic approximation to the cubic splines */
/*  that postscript uses. We do this by looking at each spline and running */
/*  from the end toward the beginning, checking approximately every emunit */
/*  There is only one quadratic spline possible for any given interval of the */
/*  cubic. The start and end points are the interval end points (obviously) */
/*  the control point is where the two slopes (at start and end) intersect. */
/* If this spline is a close approximation to the cubic spline (doesn't */
/*  deviate from it by more than an emunit or so), then we use this interval */
/*  as one of our quadratic splines. */
/* It may turn out that the "quadratic" spline above is actually linear. Well */
/*  that's ok. It may also turn out that we can't find a good approximation. */
/*  If that's true then just insert a linear segment for an emunit stretch. */
/*  (actually this failure mode may not be possible), but I'm not sure */
/* Then we play the same trick for the rest of the cubic spline (if any) */

/* Does the quadratic spline in ttf approximate the cubic spline in ps */
/*  within one pixel between tmin and tmax (on ps. presumably ttf between 0&1 */
/* dim is the dimension in which there is the greatest change */
static int comparespline(Spline *ps, Spline *ttf, real tmin, real tmax, real err) {
    int dim=0, other;
    real dx, dy, ddim, dt, t;
    real d, o;
    real ttf_t, sq, val;
    DBounds bb;

    /* Are all points on ttf near points on ps? */
    /* This doesn't answer that question, but rules out gross errors */
    bb.minx = bb.maxx = ps->from->me.x; bb.miny = bb.maxy = ps->from->me.y;
    if ( ps->from->nextcp.x>bb.maxx ) bb.maxx = ps->from->nextcp.x;
    else bb.minx = ps->from->nextcp.x;
    if ( ps->from->nextcp.y>bb.maxy ) bb.maxy = ps->from->nextcp.y;
    else bb.miny = ps->from->nextcp.y;
    if ( ps->to->prevcp.x>bb.maxx ) bb.maxx = ps->to->prevcp.x;
    else if ( ps->to->prevcp.x<bb.minx ) bb.minx = ps->to->prevcp.x;
    if ( ps->to->prevcp.y>bb.maxy ) bb.maxy = ps->to->prevcp.y;
    else if ( ps->to->prevcp.y<bb.miny ) bb.miny = ps->to->prevcp.y;
    if ( ps->to->me.x>bb.maxx ) bb.maxx = ps->to->me.x;
    else if ( ps->to->me.x<bb.minx ) bb.minx = ps->to->me.x;
    if ( ps->to->me.y>bb.maxy ) bb.maxy = ps->to->me.y;
    else if ( ps->to->me.y<bb.miny ) bb.miny = ps->to->me.y;
    for ( t=.1; t<1; t+= .1 ) {
	d = (ttf->splines[0].b*t+ttf->splines[0].c)*t+ttf->splines[0].d;
	o = (ttf->splines[1].b*t+ttf->splines[1].c)*t+ttf->splines[1].d;
	if ( d<bb.minx || d>bb.maxx || o<bb.miny || o>bb.maxy )
return( false );
    }

    /* Are all points on ps near points on ttf? */
    dx = ((ps->splines[0].a*tmax+ps->splines[0].b)*tmax+ps->splines[0].c)*tmax -
	 ((ps->splines[0].a*tmin+ps->splines[0].b)*tmin+ps->splines[0].c)*tmin ;
    dy = ((ps->splines[1].a*tmax+ps->splines[1].b)*tmax+ps->splines[1].c)*tmax -
	 ((ps->splines[1].a*tmin+ps->splines[1].b)*tmin+ps->splines[1].c)*tmin ;
    if ( dx<0 ) dx = -dx;
    if ( dy<0 ) dy = -dy;
    if ( dx>dy ) {
	dim = 0;
	ddim = dx;
    } else {
	dim = 1;
	ddim = dy;
    }
    other = !dim;

    t = tmin;
    dt = (tmax-tmin)/ddim;
    for ( t=tmin; t<=tmax; t+= dt ) {
	if ( t>tmax-dt/8. ) t = tmax;		/* Avoid rounding errors */
	d = ((ps->splines[dim].a*t+ps->splines[dim].b)*t+ps->splines[dim].c)*t+ps->splines[dim].d;
	o = ((ps->splines[other].a*t+ps->splines[other].b)*t+ps->splines[other].c)*t+ps->splines[other].d;
	if ( ttf->splines[dim].b == 0 ) {
	    ttf_t = (d-ttf->splines[dim].d)/ttf->splines[dim].c;
	} else {
	    sq = ttf->splines[dim].c*ttf->splines[dim].c -
		    4*ttf->splines[dim].b*(ttf->splines[dim].d-d);
	    if ( sq<0 )
return( false );
	    sq = sqrt(sq);
	    ttf_t = (-ttf->splines[dim].c-sq)/(2*ttf->splines[dim].b);
	    if ( ttf_t>=0 && ttf_t<=1.0 ) {
		val = (ttf->splines[other].b*ttf_t+ttf->splines[other].c)*ttf_t+
			    ttf->splines[other].d;
		if ( val>o-err && val<o+err )
    continue;
	    }
	    ttf_t = (-ttf->splines[dim].c+sq)/(2*ttf->splines[dim].b);
	}
	if ( ttf_t>=-0.0001 && ttf_t<=1.0001 ) {	/* Optimizer gives us rounding errors */
	    val = (ttf->splines[other].b*ttf_t+ttf->splines[other].c)*ttf_t+
			ttf->splines[other].d;
	    if ( val>o-err && val<o+err )
    continue;
	}
return( false );
    }

return( true );
}

static SplinePoint *MakeQuadSpline(SplinePoint *start,Spline *ttf,real x,
	real y, real tmax,SplinePoint *oldend) {
    Spline *new = chunkalloc(sizeof(Spline));
    SplinePoint *end = chunkalloc(sizeof(SplinePoint));

    if ( tmax==1 ) {
	end->roundx = oldend->roundx; end->roundy = oldend->roundy; end->dontinterpolate = oldend->dontinterpolate;
	x = oldend->me.x; y = oldend->me.y;	/* Want it to compare exactly */
    }
    end->ttfindex = 0xfffe;
    end->me.x = end->nextcp.x = x;
    end->me.y = end->nextcp.y = y;
    end->nonextcp = true;

    *new = *ttf;
    new->from = start;		start->next = new;
    new->to = end;		end->prev = new;
    if ( new->splines[0].b==0 && new->splines[1].b==0 ) {
	end->noprevcp = true;
	end->prevcp.x = x; end->prevcp.y = y;
	new->islinear = new->knownlinear = true;
    } else {
	end->prevcp.x = start->nextcp.x = ttf->splines[0].c/2+ttf->splines[0].d;
	end->prevcp.y = start->nextcp.y = ttf->splines[1].c/2+ttf->splines[1].d;
	start->nonextcp = end->noprevcp = false;
	new->isquadratic = true;
    }
    new->order2 = true;
return( end );
}

static int buildtestquads(Spline *ttf,real xmin,real ymin,real cx,real cy,
	real x,real y,real tmin,real t,real err,Spline *ps) {
#if 0
    BasePoint norm;
    real sq;
#endif

    /* test the control points are reasonable */
    if ( xmin<x ) {
	if ( cx<xmin-.1 || cx>x+.1 )
return( false );
    } else {
	if ( cx<x-.1 || cx>xmin+.1 )
return( false );
    }
    if ( ymin<y ) {
	if ( cy<ymin-.1 || cy>y+.1 )
return( false );
    } else {
	if ( cy<y-.1 || cy>ymin+.1 )
return( false );
    }

    ttf->splines[0].d = xmin;
    ttf->splines[0].c = 2*(cx-xmin);
    ttf->splines[0].b = xmin+x-2*cx;
    ttf->splines[1].d = ymin;
    ttf->splines[1].c = 2*(cy-ymin);
    ttf->splines[1].b = ymin+y-2*cy;
    if ( comparespline(ps,ttf,tmin,t,err) )
return( true );

#if 0
    /* In a few cases, the following code will find a match when the above */
    /*  would not. We move the control point slightly along a vector normal */
    /*  to the vector between the end-points. What I really want is along */
    /*  a vector midway between the two slopes, but that's too hard to figure */
    sq = sqrt((x-xmin)*(x-xmin) + (y-ymin)*(y-ymin));
    norm.x = (ymin-y)/sq; norm.y = (x-xmin)/sq;

    ttf->splines[0].c += err*norm.x;
    ttf->splines[0].b -= err*norm.x;
    ttf->splines[1].c += err*norm.y;
    ttf->splines[1].b -= err*norm.y;
    if ( comparespline(ps,ttf,tmin,t,err) )
return( true );

    ttf->splines[0].c -= 2*err*norm.x;
    ttf->splines[0].b += 2*err*norm.x;
    ttf->splines[1].c -= 2*err*norm.y;
    ttf->splines[1].b += 2*err*norm.y;
    if ( comparespline(ps,ttf,tmin,t,err) )
return( true );

    ttf->splines[0].c = 2*(cx-xmin);
    ttf->splines[0].b = xmin+x-2*cx;
    ttf->splines[1].c = 2*(cy-ymin);
    ttf->splines[1].b = ymin+y-2*cy;
#endif
return( false );
}

static SplinePoint *LinearSpline(Spline *ps,SplinePoint *start, real tmax) {
    real x,y;
    Spline *new = chunkalloc(sizeof(Spline));
    SplinePoint *end = chunkalloc(sizeof(SplinePoint));

    x = ((ps->splines[0].a*tmax+ps->splines[0].b)*tmax+ps->splines[0].c)*tmax+ps->splines[0].d;
    y = ((ps->splines[1].a*tmax+ps->splines[1].b)*tmax+ps->splines[1].c)*tmax+ps->splines[1].d;
    if ( tmax==1 ) {
	SplinePoint *oldend = ps->to;
	end->roundx = oldend->roundx; end->roundy = oldend->roundy; end->dontinterpolate = oldend->dontinterpolate;
	x = oldend->me.x; y = oldend->me.y;	/* Want it to compare exactly */
    }
    end->ttfindex = 0xfffe;
    end->me.x = end->nextcp.x = end->prevcp.x = x;
    end->me.y = end->nextcp.y = end->prevcp.y = y;
    end->nonextcp = end->noprevcp = start->nonextcp = true;
    new->from = start;		start->next = new;
    new->to = end;		end->prev = new;
    new->splines[0].d = start->me.x;
    new->splines[0].c = (x-start->me.x);
    new->splines[1].d = start->me.y;
    new->splines[1].c = (y-start->me.y);
    new->order2 = true;
return( end );
}

static SplinePoint *LinearTest(Spline *ps,real tmin,real tmax,
	real xmax,real ymax, SplinePoint *start) {
    Spline ttf;
    int dim=0, other;
    real dx, dy, ddim, dt, t;
    real d, o, val;
    real ttf_t;

    memset(&ttf,'\0',sizeof(ttf));
    ttf.splines[0].d = start->me.x;
    ttf.splines[0].c = (xmax-start->me.x);
    ttf.splines[1].d = start->me.y;
    ttf.splines[1].c = (ymax-start->me.y);

    if ( ( dx = xmax-start->me.x)<0 ) dx = -dx;
    if ( ( dy = ymax-start->me.y)<0 ) dy = -dy;
    if ( dx>dy ) {
	dim = 0;
	ddim = dx;
    } else {
	dim = 1;
	ddim = dy;
    }
    other = !dim;

    dt = (tmax-tmin)/ddim;
    for ( t=tmin; t<=tmax; t+= dt ) {
	d = ((ps->splines[dim].a*t+ps->splines[dim].b)*t+ps->splines[dim].c)*t+ps->splines[dim].d;
	o = ((ps->splines[other].a*t+ps->splines[other].b)*t+ps->splines[other].c)*t+ps->splines[other].d;
	ttf_t = (d-ttf.splines[dim].d)/ttf.splines[dim].c;
	val = ttf.splines[other].c*ttf_t+ ttf.splines[other].d;
	if ( val<o-1 || val>o+1 )
return( NULL );
    }
return( MakeQuadSpline(start,&ttf,xmax,ymax,tmax,ps->to));
}

static SplinePoint *_ttfapprox(Spline *ps,real tmin, real tmax, SplinePoint *start) {
    int dim=0, other;
    real dx, dy, ddim, dt, t, err;
    real x,y, xmin, ymin;
    real dxdtmin, dydtmin, dxdt, dydt;
    SplinePoint *sp;
    real cx, cy;
    Spline ttf;
    int cnt = -1, forceit, unforceable;
    BasePoint end, rend, dend;

    if ( RealNearish(ps->splines[0].a,0) && RealNearish(ps->splines[1].a,0) ) {
	/* Already Quadratic, just need to find the control point */
	/* Or linear, in which case we don't need to do much of anything */
	Spline *spline;
	sp = chunkalloc(sizeof(SplinePoint));
	sp->me.x = ps->to->me.x; sp->me.y = ps->to->me.y;
	sp->roundx = ps->to->roundx; sp->roundy = ps->to->roundy; sp->dontinterpolate = ps->to->dontinterpolate;
	sp->ttfindex = 0xfffe;
	sp->nonextcp = true;
	spline = chunkalloc(sizeof(Spline));
	spline->order2 = true;
	spline->from = start;
	spline->to = sp;
	spline->splines[0] = ps->splines[0]; spline->splines[1] = ps->splines[1];
	start->next = sp->prev = spline;
	if ( ps->knownlinear ) {
	    spline->islinear = spline->knownlinear = true;
	    start->nonextcp = sp->noprevcp = true;
	    start->nextcp = start->me;
	    sp->prevcp = sp->me;
	} else {
	    start->nonextcp = sp->noprevcp = false;
	    start->nextcp.x = sp->prevcp.x = (ps->splines[0].c+2*ps->splines[0].d)/2;
	    start->nextcp.y = sp->prevcp.y = (ps->splines[1].c+2*ps->splines[1].d)/2;
	}
return( sp );
    }

    rend.x = ((ps->splines[0].a*tmax+ps->splines[0].b)*tmax+ps->splines[0].c)*tmax + ps->splines[0].d;
    rend.y = ((ps->splines[1].a*tmax+ps->splines[1].b)*tmax+ps->splines[1].c)*tmax + ps->splines[1].d;
    end.x = rint( rend.x );
    end.y = rint( rend.y );
    dend.x = (3*ps->splines[0].a*tmax+2*ps->splines[0].b)*tmax+ps->splines[0].c;
    dend.y = (3*ps->splines[1].a*tmax+2*ps->splines[1].b)*tmax+ps->splines[1].c;
    memset(&ttf,'\0',sizeof(ttf));

  tail_recursion:
    ++cnt;

    xmin = start->me.x;
    ymin = start->me.y;
    dxdtmin = (3*ps->splines[0].a*tmin+2*ps->splines[0].b)*tmin + ps->splines[0].c;
    dydtmin = (3*ps->splines[1].a*tmin+2*ps->splines[1].b)*tmin + ps->splines[1].c;

    dx = ((ps->splines[0].a*tmax+ps->splines[0].b)*tmax+ps->splines[0].c)*tmax -
	 ((ps->splines[0].a*tmin+ps->splines[0].b)*tmin+ps->splines[0].c)*tmin ;
    dy = ((ps->splines[1].a*tmax+ps->splines[1].b)*tmax+ps->splines[1].c)*tmax -
	 ((ps->splines[1].a*tmin+ps->splines[1].b)*tmin+ps->splines[1].c)*tmin ;
    if ( dx<0 ) dx = -dx;
    if ( dy<0 ) dy = -dy;
    if ( dx>dy ) {
	dim = 0;
	ddim = dx;
    } else {
	dim = 1;
	ddim = dy;
    }
    other = !dim;
    if (( err = ddim/3000 )<1 ) err = 1;

    if ( ddim<2 ||
	    (dend.x==0 && rint(start->me.x)==end.x && dy<=10 && cnt!=0) ||
	    (dend.y==0 && rint(start->me.y)==end.y && dx<=10 && cnt!=0) ) {
	if ( cnt==0 || start->noprevcp )
return( LinearSpline(ps,start,tmax));
	/* If the end point is very close to where we want to be, then just */
	/*  pretend it's right */
	start->prev->splines[0].b += ps->to->me.x-start->me.x;
	start->prev->splines[1].b += ps->to->me.y-start->me.y;
	start->prevcp.x += rend.x-start->me.x;
	start->prevcp.y += rend.y-start->me.y;
	if ( start->prev!=NULL && !start->prev->from->nonextcp )
	    start->prev->from->nextcp = start->prevcp;
	start->me = rend;
return( start );
    }

    dt = (tmax-tmin)/ddim;
    forceit = false;
 force_end:
    unforceable = false;
    for ( t=tmax; t>tmin+dt/128; t-= dt ) {		/* dt/128 is a hack to avoid rounding errors */
	x = ((ps->splines[0].a*t+ps->splines[0].b)*t+ps->splines[0].c)*t+ps->splines[0].d;
	y = ((ps->splines[1].a*t+ps->splines[1].b)*t+ps->splines[1].c)*t+ps->splines[1].d;
	dxdt = (3*ps->splines[0].a*t+2*ps->splines[0].b)*t + ps->splines[0].c;
	dydt = (3*ps->splines[1].a*t+2*ps->splines[1].b)*t + ps->splines[1].c;
	/* if the slopes are parallel at the ends there can be no bezier quadratic */
	/*  (control point is where the splines intersect. But if they are */
	/*  parallel and colinear then there is a line between 'em */
	if ( ( dxdtmin==0 && dxdt==0 ) || (dydtmin==0 && dydt==0) ||
		( dxdt!=0 && dxdtmin!=0 &&
		    RealNearish(dydt/dxdt,dydtmin/dxdtmin)) ) {
	    if (( dxdt==0 && x==xmin ) || (dydt==0 && y==ymin) ||
		    (dxdt!=0 && x!=xmin && RealNearish(dydt/dxdt,(y-ymin)/(x-xmin))) ) {
		if ( (sp = LinearTest(ps,tmin,t,x,y,start))!=NULL ) {
		    if ( t==tmax )
return( sp );
		    tmin = t;
		    start = sp;
  goto tail_recursion;
	      }
	  }
    continue;
	}
	if ( dxdt==0 )
	    cx=x;
	else if ( dxdtmin==0 )
	    cx=xmin;
	else
	    cx = -(ymin-(dydtmin/dxdtmin)*xmin-y+(dydt/dxdt)*x)/(dydtmin/dxdtmin-dydt/dxdt);
	if ( dydt==0 )
	    cy=y;
	else if ( dydtmin==0 )
	    cy=ymin;
	else
	    cy = -(xmin-(dxdtmin/dydtmin)*ymin-x+(dxdt/dydt)*y)/(dxdtmin/dydtmin-dxdt/dydt);
	if ( t==tmax && ((cy==y && cx==x) || (cy==ymin && cx==xmin)) )
	    unforceable = true;
	/* Make the quadratic spline from (xmin,ymin) through (cx,cy) to (x,y)*/
	if ( forceit || buildtestquads(&ttf,xmin,ymin,cx,cy,x,y,tmin,t,err,ps)) {
	    if ( !forceit && !unforceable && (rend.x-x)*(rend.x-x)+(rend.y-y)*(rend.y-y)<4*4 ) {
		forceit = true;
 goto force_end;
	    }
	    sp = MakeQuadSpline(start,&ttf,x,y,t,ps->to);
	    forceit = false;
	    if ( t==tmax )
return( sp );
	    tmin = t;
	    start = sp;
  goto tail_recursion;
	}
	ttf.splines[0].d = xmin;
	ttf.splines[0].c = x-xmin;
	ttf.splines[0].b = 0;
	ttf.splines[1].d = ymin;
	ttf.splines[1].c = y-ymin;
	ttf.splines[1].b = 0;
	if ( comparespline(ps,&ttf,tmin,t,err) ) {
	    sp = LinearSpline(ps,start,t);
	    if ( t==tmax )
return( sp );
	    tmin = t;
	    start = sp;
  goto tail_recursion;
	}
    }
    tmin += dt;
    start = LinearSpline(ps,start,tmin);
  goto tail_recursion;
}

static SplinePoint *ttfApprox(Spline *ps,real tmin, real tmax, SplinePoint *start) {
#if 0
/* Hmm. With my algorithem, checking for points of inflection actually makes */
/*  things worse. It uses more points and the splines don't join as nicely */
    real inflect[2];
    int i=0;
    /* no points of inflection in quad splines */

    if ( ps->splines[0].a!=0 ) {
	inflect[i] = -ps->splines[0].b/(3*ps->splines[0].a);
	if ( inflect[i]>tmin && inflect[i]<tmax )
	    ++i;
    }
    if ( ps->splines[1].a!=0 ) {
	inflect[i] = -ps->splines[1].b/(3*ps->splines[1].a);
	if ( inflect[i]>tmin && inflect[i]<tmax )
	    ++i;
    }
    if ( i==2 ) {
	if ( RealNearish(inflect[0],inflect[1]) )
	    --i;
	else if ( inflect[0]>inflect[1] ) {
	    real temp = inflect[0];
	    inflect[0] = inflect[1];
	    inflect[1] = temp;
	}
    }
    if ( i!=0 ) {
	start = _ttfapprox(ps,tmin,inflect[0],start);
	tmin = inflect[0];
	if ( i==2 ) {
	    start = _ttfapprox(ps,tmin,inflect[1],start);
	    tmin = inflect[1];
	}
    }
#endif
return( _ttfapprox(ps,tmin,tmax,start));
}

SplinePoint *SplineTtfApprox(Spline *ps) {
    SplinePoint *from;
    from = chunkalloc(sizeof(SplinePoint));
    *from = *ps->from;
    ttfApprox(ps,0,1,from);
return( from );
}

SplineSet *SSttfApprox(SplineSet *ss) {
    SplineSet *ret = chunkalloc(sizeof(SplineSet));
    Spline *spline, *first;

    ret->first = chunkalloc(sizeof(SplinePoint));
    *ret->first = *ss->first;
    ret->last = ret->first;

    first = NULL;
    for ( spline=ss->first->next; spline!=NULL && spline!=first; spline=spline->to->next ) {
	ret->last = ttfApprox(spline,0,1,ret->last);
	if ( first==NULL ) first = spline;
    }
    if ( ss->first==ss->last ) {
	if ( ret->last!=ret->first ) {
	    ret->first->prevcp = ret->last->prevcp;
	    ret->first->noprevcp = ret->last->noprevcp;
	    ret->first->prev = ret->last->prev;
	    ret->last->prev->to = ret->first;
	    SplinePointFree(ret->last);
	    ret->last = ret->first;
	}
    }
return( ret );
}

SplineSet *SplineSetsTTFApprox(SplineSet *ss) {
    SplineSet *head=NULL, *last, *cur;

    while ( ss!=NULL ) {
	cur = SSttfApprox(ss);
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
	ss = ss->next;
    }
return( head );
}

SplineSet *SSPSApprox(SplineSet *ss) {
    SplineSet *ret = chunkalloc(sizeof(SplineSet));
    Spline *spline, *first;
    SplinePoint *to;

    ret->first = chunkalloc(sizeof(SplinePoint));
    *ret->first = *ss->first;
    ret->last = ret->first;

    first = NULL;
    for ( spline=ss->first->next; spline!=NULL && spline!=first; spline=spline->to->next ) {
	to = chunkalloc(sizeof(SplinePoint));
	*to = *spline->to;
	if ( !spline->knownlinear ) {
	    ret->last->nextcp.x = spline->splines[0].c/3 + ret->last->me.x;
	    ret->last->nextcp.y = spline->splines[1].c/3 + ret->last->me.y;
	    to->prevcp.x = ret->last->nextcp.x+ (spline->splines[0].b+spline->splines[0].c)/3;
	    to->prevcp.y = ret->last->nextcp.y+ (spline->splines[1].b+spline->splines[1].c)/3;
	}
	SplineMake3(ret->last,to);
	ret->last = to;
	if ( first==NULL ) first = spline;
    }
    if ( ss->first==ss->last ) {
	if ( ret->last!=ret->first ) {
	    ret->first->prevcp = ret->last->prevcp;
	    ret->first->noprevcp = ret->last->noprevcp;
	    ret->first->prev = ret->last->prev;
	    ret->last->prev->to = ret->first;
	    SplinePointFree(ret->last);
	    ret->last = ret->first;
	}
    }
return( ret );
}

SplineSet *SplineSetsPSApprox(SplineSet *ss) {
    SplineSet *head=NULL, *last, *cur;

    while ( ss!=NULL ) {
	cur = SSPSApprox(ss);
	if ( head==NULL )
	    head = cur;
	else
	    last->next = cur;
	last = cur;
	ss = ss->next;
    }
return( head );
}

SplineSet *SplineSetsConvertOrder(SplineSet *ss, int to_order2) {
    SplineSet *new;

    if ( to_order2 )
	new = SplineSetsTTFApprox(ss);
    else
	new = SplineSetsPSApprox(ss);
    SplinePointListFree(ss);
return( new );
}

void SCConvertToOrder2(SplineChar *sc) {
    SplineSet *new;

    if ( sc==NULL )
return;

    new = SplineSetsTTFApprox(sc->splines);
    SplinePointListsFree(sc->splines);
    sc->splines = new;

    new = SplineSetsTTFApprox(sc->backgroundsplines);
    SplinePointListsFree(sc->backgroundsplines);
    sc->backgroundsplines = new;

    UndoesFree(sc->undoes[0]); UndoesFree(sc->undoes[1]);
    UndoesFree(sc->redoes[0]); UndoesFree(sc->redoes[1]);
    sc->undoes[0] = sc->undoes[1] = NULL;
    sc->redoes[0] = sc->redoes[1] = NULL;

    MinimumDistancesFree(sc->md); sc->md = NULL;
}

static void SCConvertRefs(SplineChar *sc) {
    RefChar *rf;

    sc->ticked = true;
    for ( rf=sc->refs; rf!=NULL; rf=rf->next ) {
	if ( !rf->sc->ticked )
	    SCConvertRefs(rf->sc);
	SCReinstanciateRefChar(sc,rf);	/* Conversion is done by reinstanciating */
		/* Since the base thing will have been converted, all we do is copy its data */
    }
}

void SFConvertToOrder2(SplineFont *_sf) {
    int i, k;
    SplineSet *new;
    SplineFont *sf;

    if ( _sf->cidmaster!=NULL ) _sf=_sf->cidmaster;
    k = 0;
    do {
	sf = _sf->subfonts==NULL ? _sf : _sf->subfonts[k];
	for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	    SCConvertToOrder2(sf->chars[i]);
	    sf->chars[i]->ticked = false;
	}
	for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && !sf->chars[i]->ticked )
	    SCConvertRefs(sf->chars[i]);

	new = SplineSetsTTFApprox(sf->gridsplines);
	SplinePointListsFree(sf->gridsplines);
	sf->gridsplines = new;

	UndoesFree(sf->gundoes); UndoesFree(sf->gredoes);
	sf->gundoes = sf->gredoes = NULL;
	sf->order2 = true;
	++k;
    } while ( k<_sf->subfontcnt );
    _sf->order2 = true;
}
    
void SCConvertToOrder3(SplineChar *sc) {
    SplineSet *new;

    new = SplineSetsPSApprox(sc->splines);
    SplinePointListsFree(sc->splines);
    sc->splines = new;

    new = SplineSetsPSApprox(sc->backgroundsplines);
    SplinePointListsFree(sc->backgroundsplines);
    sc->backgroundsplines = new;

    UndoesFree(sc->undoes[0]); UndoesFree(sc->undoes[1]);
    UndoesFree(sc->redoes[0]); UndoesFree(sc->redoes[1]);
    sc->undoes[0] = sc->undoes[1] = NULL;
    sc->redoes[0] = sc->redoes[1] = NULL;

    MinimumDistancesFree(sc->md); sc->md = NULL;

    free(sc->ttf_instrs);
    sc->ttf_instrs = NULL; sc->ttf_instrs_len = 0;
    /* If this character has any cv's showing instructions then remove the instruction pane!!!!! */
}

void SCConvertOrder(SplineChar *sc, int to_order2) {
    if ( to_order2 )
	SCConvertToOrder2(sc);
    else
	SCConvertToOrder3(sc);
}

void SFConvertToOrder3(SplineFont *_sf) {
    int i, k;
    SplineSet *new;
    SplineFont *sf;

    if ( _sf->cidmaster!=NULL ) _sf=_sf->cidmaster;
    k = 0;
    do {
	sf = _sf->subfonts==NULL ? _sf : _sf->subfonts[k];
	for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL ) {
	    SCConvertToOrder3(sf->chars[i]);
	    sf->chars[i]->ticked = false;
	}
	for ( i=0; i<sf->charcnt; ++i ) if ( sf->chars[i]!=NULL && !sf->chars[i]->ticked )
	    SCConvertRefs(sf->chars[i]);

	new = SplineSetsPSApprox(sf->gridsplines);
	SplinePointListsFree(sf->gridsplines);
	sf->gridsplines = new;

	UndoesFree(sf->gundoes); UndoesFree(sf->gredoes);
	sf->gundoes = sf->gredoes = NULL;

	TtfTablesFree(sf->ttf_tables);
	sf->ttf_tables = NULL;

	sf->order2 = false;
	++k;
    } while ( k<_sf->subfontcnt );
    _sf->order2 = false;
}

/* ************************************************************************** */

void SplineRefigure2(Spline *spline) {
    SplinePoint *from = spline->from, *to = spline->to;
    Spline1D *xsp = &spline->splines[0], *ysp = &spline->splines[1];

#ifdef DEBUG
    if ( RealNear(from->me.x,to->me.x) && RealNear(from->me.y,to->me.y))
	GDrawIError("Zero length spline created");
#endif

    if ( from->nonextcp || to->noprevcp ||
	    ( from->nextcp.x==from->me.x && from->nextcp.y == from->me.y ) ||
	    ( to->prevcp.x==to->me.x && to->prevcp.y == to->me.y )) {
	from->nonextcp = to->noprevcp = true;
	from->nextcp = from->me;
	to->prevcp = to->me;
    }

    if ( from->nonextcp && to->noprevcp )
	/* Ok */;
    else if ( from->nonextcp || to->noprevcp || from->nextcp.x!=to->prevcp.x ||
	    from->nextcp.y!=to->prevcp.y ) {
	if ( RealNear(from->nextcp.x,to->prevcp.x) &&
		RealNear(from->nextcp.y,to->prevcp.y)) {
	    from->nextcp.x = to->prevcp.x = (from->nextcp.x+to->prevcp.x)/2;
	    from->nextcp.y = to->prevcp.y = (from->nextcp.y+to->prevcp.y)/2;
	} else
	    GDrawIError("Invalid 2nd order spline in SplineRefigure2" );
    }

    xsp->d = from->me.x; ysp->d = from->me.y;
    if ( from->nonextcp && to->noprevcp ) {
	spline->islinear = true;
	xsp->c = to->me.x-from->me.x;
	ysp->c = to->me.y-from->me.y;
	xsp->a = xsp->b = 0;
	ysp->a = ysp->b = 0;
    } else {
	/* from p. 393 (Operator Details, curveto) Postscript Lang. Ref. Man. (Red book) */
	xsp->c = 2*(from->nextcp.x-from->me.x);
	ysp->c = 2*(from->nextcp.y-from->me.y);
	xsp->b = to->me.x-from->me.x-xsp->c;
	ysp->b = to->me.y-from->me.y-ysp->c;
	xsp->a = 0;
	ysp->a = 0;
	if ( RealNear(xsp->c,0)) xsp->c=0;
	if ( RealNear(ysp->c,0)) ysp->c=0;
	if ( RealNear(xsp->b,0)) xsp->b=0;
	if ( RealNear(ysp->b,0)) ysp->b=0;
	spline->islinear = false;
	if ( ysp->b==0 && xsp->b==0 )
	    spline->islinear = true;	/* This seems extremely unlikely... */
    }
    if ( isnan(ysp->b) || isnan(xsp->b) )
	GDrawIError("NaN value in spline creation");
    LinearApproxFree(spline->approx);
    spline->approx = NULL;
    spline->knowncurved = false;
    spline->knownlinear = spline->islinear;
    SplineIsLinear(spline);
    spline->isquadratic = !spline->knownlinear;
    spline->order2 = true;
}

void SplineRefigure(Spline *spline) {
    if ( spline->order2 )
	SplineRefigure2(spline);
    else
	SplineRefigure3(spline);
}

void SplineRefigureFixup(Spline *spline) {
    SplinePoint *from, *to, *prev, *next;
    BasePoint foff, toff, unit, new;
    double len;

    if ( !spline->order2 ) {
	SplineRefigure3(spline);
return;
    }
    from = spline->from; to = spline->to;

    unit.x = from->nextcp.x-from->me.x;
    unit.y = from->nextcp.y-from->me.y;
    len = sqrt(unit.x*unit.x + unit.y*unit.y);
    if ( len!=0 )
	unit.x /= len; unit.y /= len;

    if ( from->nextcpdef && to->prevcpdef ) switch ( from->pointtype*3+to->pointtype ) {
      case pt_corner*3+pt_corner:
      case pt_corner*3+pt_tangent:
      case pt_tangent*3+pt_corner:
      case pt_tangent*3+pt_tangent:
	from->nonextcp = to->noprevcp = true;
	from->nextcp = from->me;
	to->prevcp = to->me;
      break;
      case pt_curve*3+pt_curve:
      case pt_curve*3+pt_corner:
      case pt_corner*3+pt_curve:
      case pt_tangent*3+pt_curve:
      case pt_curve*3+pt_tangent:
	if ( from->prev!=NULL && from->pointtype==pt_tangent ) {
	    prev = from->prev->from;
	    foff.x = prev->me.x;
	    foff.y = prev->me.y;
	} else if ( from->prev!=NULL ) {
	    prev = from->prev->from;
	    foff.x = to->me.x-prev->me.x  + from->me.x;
	    foff.y = to->me.y-prev->me.y  + from->me.y;
	} else {
	    foff.x = from->me.x + (to->me.x-from->me.x)-(to->me.y-from->me.y);
	    foff.y = from->me.y + (to->me.x-from->me.x)+(to->me.y-from->me.y);
	}
	if ( to->next!=NULL && to->pointtype==pt_tangent ) {
	    next = to->next->to;
	    toff.x = next->me.x;
	    toff.y = next->me.y;
	} else if ( to->next!=NULL ) {
	    next = to->next->to;
	    toff.x = next->me.x-from->me.x  + to->me.x;
	    toff.y = next->me.y-from->me.y  + to->me.y;
	} else {
	    toff.x = to->me.x + (to->me.x-from->me.x)+(to->me.y-from->me.y);
	    toff.y = to->me.y - (to->me.x-from->me.x)+(to->me.y-from->me.y);
	}
	if ( IntersectLinesClip(&from->nextcp,&foff,&from->me,&toff,&to->me)) {
	    from->nonextcp = to->noprevcp = false;
	    to->prevcp = from->nextcp;
	    if ( from->pointtype==pt_curve && !from->noprevcp && from->prev!=NULL ) {
		prev = from->prev->from;
		if ( IntersectLinesClip(&from->prevcp,&from->nextcp,&from->me,&prev->nextcp,&prev->me)) {
		    prev->nextcp = from->prevcp;
		    SplineRefigure2(from->prev);
		}
	    }
	    if ( to->pointtype==pt_curve && !to->nonextcp && to->next!=NULL ) {
		next = to->next->to;
		if ( IntersectLinesClip(&to->nextcp,&to->prevcp,&to->me,&next->prevcp,&next->me)) {
		    next->prevcp = to->nextcp;
		    SplineRefigure(to->next);
		}
	    }
	}
      break;
    } else {
	/* Can't set things arbetrarily here, but make sure they are consistant */
	if ( from->pointtype==pt_curve && !from->noprevcp && !from->nonextcp ) {
	    unit.x = from->nextcp.x-from->me.x;
	    unit.y = from->nextcp.y-from->me.y;
	    len = sqrt(unit.x*unit.x + unit.y*unit.y);
	    if ( len!=0 ) {
		unit.x /= len; unit.y /= len;
		len = sqrt((from->prevcp.x-from->me.x)*(from->prevcp.x-from->me.x) + (from->prevcp.y-from->me.y)*(from->prevcp.y-from->me.y));
		new.x = -len*unit.x + from->me.x; new.y = -len*unit.y + from->me.y;
		if ( new.x-from->prevcp.x<-1 || new.x-from->prevcp.x>1 ||
			 new.y-from->prevcp.y<-1 || new.y-from->prevcp.y>1 ) {
		    if ( from->prev!=NULL && (prev = from->prev->from)!=NULL &&
			    IntersectLinesClip(&from->prevcp,&new,&from->me,&prev->nextcp,&prev->me)) {
			prev->nextcp = from->prevcp;
			SplineRefigure2(from->prev);
		    } else
			from->prevcp = new;
		}
	    }
	} else if ( from->pointtype==pt_tangent ) {
	    if ( from->prev!=NULL ) {
		prev = from->prev->from;
		if ( !from->noprevcp && !prev->nonextcp &&
			IntersectLinesClip(&from->prevcp,&to->me,&from->me,&prev->nextcp,&prev->me)) {
		    prev->nextcp = from->prevcp;
		    SplineRefigure2(from->prev);
		}
		if ( !from->nonextcp && !to->noprevcp &&
			IntersectLinesClip(&from->nextcp,&prev->me,&from->me,&to->prevcp,&to->me))
		    to->prevcp = from->nextcp;
	    }
	}
	if ( to->pointtype==pt_curve && !to->noprevcp && !to->nonextcp ) {
	    unit.x = to->prevcp.x-to->nextcp.x;
	    unit.y = to->prevcp.y-to->nextcp.y;
	    len = sqrt(unit.x*unit.x + unit.y*unit.y);
	    if ( len!=0 ) {
		unit.x /= len; unit.y /= len;
		len = sqrt((to->nextcp.x-to->me.x)*(to->nextcp.x-to->me.x) + (to->nextcp.y-to->me.y)*(to->nextcp.y-to->me.y));
		new.x = -len*unit.x + to->me.x; new.y = -len*unit.y + to->me.y;
		if ( new.x-to->nextcp.x<-1 || new.x-to->nextcp.x>1 ||
			new.y-to->nextcp.y<-1 || new.y-to->nextcp.y>1 ) {
		    if ( to->next!=NULL && (next = to->next->to)!=NULL &&
			    IntersectLinesClip(&to->nextcp,&new,&to->me,&next->prevcp,&next->me)) {
			next->prevcp = to->nextcp;
			SplineRefigure2(to->next);
		    } else
			to->nextcp = new;
		}
	    }
	} else if ( to->pointtype==pt_tangent ) {
	    if ( to->next!=NULL ) {
		next = to->next->to;
		if ( !to->nonextcp && !next->noprevcp &&
			IntersectLinesClip(&to->nextcp,&from->me,&to->me,&next->prevcp,&next->me)) {
		    next->prevcp = to->nextcp;
		    SplineRefigure2(to->next);
		}
		if ( !from->nonextcp && !to->noprevcp &&
			IntersectLinesClip(&from->nextcp,&next->me,&to->me,&from->nextcp,&from->me))
		    to->prevcp = from->nextcp;
	    }
	}
    }
    if ( from->nonextcp && to->noprevcp )
	/* Ok */;
    else if ( from->nonextcp || to->noprevcp ) {
	from->nonextcp = to->noprevcp = true;
    } else if (( from->nextcp.x==from->me.x && from->nextcp.y==from->me.y ) ||
		( to->prevcp.x==to->me.x && to->prevcp.y==to->me.y ) ) {
	from->nonextcp = to->noprevcp = true;
    } else if ( from->nonextcp || to->noprevcp || from->nextcp.x!=to->prevcp.x ||
	    from->nextcp.y!=to->prevcp.y ) {
	if ( !IntersectLinesClip(&from->nextcp,
		(from->pointtype==pt_tangent && from->prev!=NULL)?&from->prev->from->me:&from->nextcp, &from->me,
		(to->pointtype==pt_tangent && to->next!=NULL)?&to->next->to->me:&to->prevcp, &to->me)) {
	    from->nextcp.x = (from->me.x+to->me.x)/2;
	    from->nextcp.y = (from->me.y+to->me.y)/2;
	}
	to->prevcp = from->nextcp;
	if (( from->nextcp.x==from->me.x && from->nextcp.y==from->me.y ) ||
		( to->prevcp.x==to->me.x && to->prevcp.y==to->me.y ) ) {
	    from->nonextcp = to->noprevcp = true;
	    from->nextcp = from->me;
	    to->prevcp = to->me;
	}
    }
    SplineRefigure2(spline);
}

Spline *SplineMake2(SplinePoint *from, SplinePoint *to) {
    Spline *spline = chunkalloc(sizeof(Spline));

    spline->from = from; spline->to = to;
    from->next = to->prev = spline;
    spline->order2 = true;
    SplineRefigure2(spline);
return( spline );
}

Spline *SplineMake(SplinePoint *from, SplinePoint *to, int order2) {
    if ( order2 )
return( SplineMake2(from,to));
    else
return( SplineMake3(from,to));
}

int IntersectLines(BasePoint *inter,
	BasePoint *line1_1, BasePoint *line1_2,
	BasePoint *line2_1, BasePoint *line2_2) {
    double s1, s2;

    if ( line1_1->x == line1_2->x ) {
	inter->x = line1_1->x;
	if ( line2_1->x == line2_2->x ) {
	    if ( line2_1->x!=line1_1->x )
return( false );		/* Parallel vertical lines */
	    inter->y = (line1_1->y+line2_1->y)/2;
	} else
	    inter->y = line2_1->y + (inter->x-line2_1->x) * (line2_2->y - line2_1->y)/(line2_2->x - line2_1->x);
return( true );
    } else if ( line2_1->x == line2_2->x ) {
	inter->x = line2_1->x;
	inter->y = line1_1->y + (inter->x-line1_1->x) * (line1_2->y - line1_1->y)/(line1_2->x - line1_1->x);
return( true );
    } else {
	s1 = (line1_2->y - line1_1->y)/(line1_2->x - line1_1->x);
	s2 = (line2_2->y - line2_1->y)/(line2_2->x - line2_1->x);
	if ( RealNear(s1,s2)) {
	    if ( !RealNear(line1_1->y + (line2_1->x-line1_1->x) * s1,line2_1->y))
return( false );
	    inter->x = (line1_2->x+line2_2->x)/2;
	    inter->y = (line1_2->y+line2_2->y)/2;
	} else {
	    inter->x = (s1*line1_1->x - s2*line2_1->x - line1_1->y + line2_1->y)/(s1-s2);
	    inter->y = line1_1->y + (inter->x-line1_1->x) * s1;
	}
return( true );
    }
}

int IntersectLinesClip(BasePoint *inter,
	BasePoint *line1_1, BasePoint *line1_2,
	BasePoint *line2_1, BasePoint *line2_2) {
    BasePoint old = *inter, unit;
    double len, val;

    if ( !IntersectLines(inter,line1_1,line1_2,line2_1,line2_2))
return( false );
    else {
	unit.x = line2_2->x-line1_2->x;
	unit.y = line2_2->y-line1_2->y;
	len = sqrt(unit.x*unit.x + unit.y*unit.y);
	if ( len==0 )
return( false );
	else {
	    unit.x /= len; unit.y /= len;
	    val = unit.x*(inter->x-line1_2->x) + unit.y*(inter->y-line1_2->y);
	    if ( val<=0 || val>=len ) {
		*inter = old;
return( false );
	    }
	}
    }
return( true );
}
    
void SplinePointPrevCPChanged2(SplinePoint *sp, int fixnext) {
    SplinePoint *p, *pp, *n;
    BasePoint p_pcp, ncp;

    if ( sp->prev!=NULL ) {
	p = sp->prev->from;
	p->nextcp = sp->prevcp;
	if ( sp->noprevcp ) {
	    p->nonextcp = true;
	    p->nextcp = p->me;
	    SplineRefigure2(sp->prev);
	} else if ( p->pointtype==pt_curve && !p->noprevcp ) {
	    SplineRefigure2(sp->prev);
	    if ( p->prev==NULL ) {
		double len1, len2;
		len1 = sqrt((p->nextcp.x-p->me.x)*(p->nextcp.x-p->me.x) +
			    (p->nextcp.y-p->me.y)*(p->nextcp.y-p->me.y));
		len2 = sqrt((p->prevcp.x-p->me.x)*(p->prevcp.x-p->me.x) +
			    (p->prevcp.y-p->me.y)*(p->prevcp.y-p->me.y));
		len2 /= len1;
		p->prevcp.x = len2 * (p->me.x-p->prevcp.x) + p->me.x;
		p->prevcp.y = len2 * (p->me.y-p->prevcp.y) + p->me.y;
	    } else {
		pp = p->prev->from;
		/* Find the intersection (if any) of the lines between */
		/*  pp->nextcp&pp->me with p->prevcp&p->me */
		if ( IntersectLines(&p_pcp,&pp->nextcp,&pp->me,&p->nextcp,&p->me)) {
		    double len = (pp->me.x-p->me.x)*(pp->me.x-p->me.x) + (pp->me.y-p->me.y)*(pp->me.y-p->me.y);
		    double d1 = (p_pcp.x-p->me.x)*(pp->me.x-p->me.x) + (p_pcp.y-p->me.y)*(pp->me.y-p->me.y);
		    double d2 = (p_pcp.x-pp->me.x)*(p->me.x-pp->me.x) + (p_pcp.y-pp->me.y)*(p->me.y-pp->me.y);
		    if ( d1>=0 && d1<=len && d2>=0 && d2<=len ) {
			p->prevcp = pp->nextcp = p_pcp;
			SplineRefigure2(p->prev);
		    }
		}
	    }
	}
    }

    if ( fixnext && sp->pointtype==pt_curve && !sp->nonextcp && sp->next!=NULL ) {
	n = sp->next->to;
	if ( IntersectLines(&ncp,&sp->prevcp,&sp->me,&n->prevcp,&n->me)) {
	    double len = (n->me.x-sp->me.x)*(n->me.x-sp->me.x) + (n->me.y-sp->me.y)*(n->me.y-sp->me.y);
	    double d1 = (ncp.x-sp->me.x)*(n->me.x-sp->me.x) + (ncp.y-sp->me.y)*(n->me.y-sp->me.y);
	    double d2 = (ncp.x-n->me.x)*(sp->me.x-n->me.x) + (ncp.y-n->me.y)*(sp->me.y-n->me.y);
	    if ( d1>=0 && d1<=len && d2>=0 && d2<=len ) {
		n->prevcp = ncp;
		sp->nextcp = ncp;
		SplineRefigure2(sp->next);
	    }
	}
    }
}
    
void SplinePointNextCPChanged2(SplinePoint *sp, int fixprev) {
    SplinePoint *p, *nn, *n;
    BasePoint n_ncp, pcp;

    if ( sp->next!=NULL ) {
	n = sp->next->to;
	n->prevcp = sp->nextcp;
	if ( sp->nonextcp ) {
	    n->noprevcp = true;
	    n->prevcp = n->me;
	    SplineRefigure2(sp->next);
	} else if ( n->pointtype==pt_curve && !n->nonextcp ) {
	    SplineRefigure2(sp->next);
	    if ( n->next==NULL ) {
		double len1, len2;
		len1 = sqrt((n->prevcp.x-n->me.x)*(n->prevcp.x-n->me.x) +
			    (n->prevcp.y-n->me.y)*(n->prevcp.y-n->me.y));
		len2 = sqrt((n->nextcp.x-n->me.x)*(n->nextcp.x-n->me.x) +
			    (n->nextcp.y-n->me.y)*(n->nextcp.y-n->me.y));
		len2 /= len1;
		n->nextcp.x = len2 * (n->me.x-n->nextcp.x) + n->me.x;
		n->nextcp.y = len2 * (n->me.y-n->nextcp.y) + n->me.y;
	    } else {
		nn = n->next->to;
		/* Find the intersection (if any) of the lines between */
		/*  nn->prevcp&nn->me with n->nextcp&.->me */
		if ( IntersectLines(&n_ncp,&nn->prevcp,&nn->me,&n->prevcp,&n->me)) {
		    double len = (nn->me.x-n->me.x)*(nn->me.x-n->me.x) + (nn->me.y-n->me.y)*(nn->me.y-n->me.y);
		    double d1 = (n_ncp.x-n->me.x)*(nn->me.x-n->me.x) + (n_ncp.y-n->me.y)*(nn->me.y-n->me.y);
		    double d2 = (n_ncp.x-nn->me.x)*(n->me.x-nn->me.x) + (n_ncp.y-nn->me.y)*(n->me.y-nn->me.y);
		    if ( d1>=0 && d1<=len && d2>=0 && d2<=len ) {
			n->nextcp = nn->prevcp = n_ncp;
			SplineRefigure2(n->next);
		    }
		}
	    }
	}
    }

    if ( fixprev && sp->pointtype==pt_curve && !sp->noprevcp && sp->prev!=NULL ) {
	p = sp->prev->from;
	if ( IntersectLines(&pcp,&sp->nextcp,&sp->me,&p->nextcp,&p->me)) {
	    double len = (p->me.x-sp->me.x)*(p->me.x-sp->me.x) + (p->me.y-sp->me.y)*(p->me.y-sp->me.y);
	    double d1 = (pcp.x-sp->me.x)*(p->me.x-sp->me.x) + (pcp.y-sp->me.y)*(p->me.y-sp->me.y);
	    double d2 = (pcp.x-p->me.x)*(sp->me.x-p->me.x) + (pcp.y-p->me.y)*(sp->me.y-p->me.y);
	    if ( d1>=0 && d1<=len && d2>=0 && d2<=len ) {
		p->nextcp = pcp;
		sp->prevcp = pcp;
		SplineRefigure2(sp->prev);
	    }
	}
    }
}
