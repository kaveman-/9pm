#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

/* a = s (func(f)) d, where f involves d */
#define fcodefun(a,s,d,f) switch(f) {\
	case DnorS:	a = ~((d)|(s));	break; \
	case DandnotS:	a = (d)&~(s);	break; \
	case notDandS:	a = ~(d)&(s);	break; \
	case notD:	a = ~(d);	break; \
	case DxorS:	a = (d)^(s);	break; \
	case DnandS:	a = ~((d)&(s));	break; \
	case DandS:	a = (d)&(s);	break; \
	case DxnorS:	a = ~((d)^(s));	break; \
	case D:		a = d;		break; \
	case DornotS:	a = (d)|~(s);	break; \
	case notDorS:	a = ~(d)|(s);	break; \
	case DorS:	a = (d)|(s);	break; \
	default:	a = d;		break; \
	}
/*
 * texture - use blt, logarithmically if f doesn't involve D,
 * but special case some textures whose rows can be packed in 32 bits
 */

void
texture(Bitmap *dm, Rectangle r, Bitmap *sm, int f)	
{
	Point p, dp;
	int x, y, d, dunused, fconst;
	Rectangle savdr;

	if(rectclip(&r, dm->clipr) == 0)
		return;
	dp = sub(sm->clipr.max, sm->clipr.min);
	if(dp.x==0 || dp.y==0 || Dx(r)==0 || Dy(r)==0)
		return;

	bbound(dm, r);

	p.x = r.min.x - (r.min.x % dp.x);
	p.y = r.min.y - (r.min.y % dp.y);
	f &= 0xF;
	fconst = (f==F || f==Zero);
	dunused = (f==S || f==notS || fconst);
	if(dunused){
		/* logarithmic tiling */
		savdr = dm->clipr;
		rectclip(&dm->clipr, r);
		if(!eqpt(p, r.min)){
			bitblt(dm, p, sm, sm->clipr, f);
			d = dp.x;
			bitblt(dm, Pt(p.x+d, p.y), sm, sm->clipr, f);
			for(x=p.x+d; x+d<r.max.x; d+=d)
				bitblt(dm, Pt(x+d, p.y), dm, Rect(x, p.y, x+d, p.y+dp.y), S);
			d = dp.y;
			bitblt(dm, Pt(p.x, p.y+d), sm, sm->clipr, f);
			for(y=p.y+d; y+d<r.max.y; d+=d)
				bitblt(dm, Pt(p.x, y+d), dm, Rect(p.x, y, p.x+dp.x, y+d), S);
			p = add(p, dp);
		}
		bitblt(dm, p, sm, sm->clipr, f);
		for(d=dp.x; p.x+d<r.max.x; d+=d)
			bitblt(dm, Pt(p.x+d, p.y), dm, Rect(p.x, p.y, p.x+d, p.y+dp.y), S);
		for(d=dp.y; p.y+d<r.max.y; d+=d)
			bitblt(dm, Pt(p.x, p.y+d), dm, Rect(p.x, p.y, r.max.x, p.y+d), S);
		dm->clipr = savdr;
	}else{
		savdr = dm->clipr;
		rectclip(&dm->clipr, r);
		for(y=p.y; y<r.max.y; y+=dp.y)
			for(x=p.x; x<r.max.x; x+=dp.x)
				bitblt(dm, Pt(x, y), sm, sm->clipr, f);
		dm->clipr = savdr;
	}
}
