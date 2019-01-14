#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

int
bltclip(Bitmap *dm, Point *p, Bitmap *sm, Rectangle *r)
{
	int dx, dy;
	int i;

	dx = Dx(*r);
	dy = Dy(*r);
	if(p->x < dm->clipr.min.x){
		i = dm->clipr.min.x-p->x;
		r->min.x += i;
		p->x += i;
		dx -= i;
	}
	if(p->y < dm->clipr.min.y){
		i = dm->clipr.min.y-p->y;
		r->min.y += i;
		p->y += i;
		dy -= i;
	}
	if(p->x+dx > dm->clipr.max.x){
		i = p->x+dx-dm->clipr.max.x;
		r->max.x -= i;
		dx -= i;
	}
	if(p->y+dy > dm->clipr.max.y){
		i = p->y+dy-dm->clipr.max.y;
		r->max.y -= i;
		dy -= i;
	}
	if(r->min.x < sm->clipr.min.x){
		i = sm->clipr.min.x-r->min.x;
		p->x += i;
		r->min.x += i;
		dx -= i;
	}
	if(r->min.y < sm->clipr.min.y){
		i = sm->clipr.min.y-r->min.y;
		p->y += i;
		r->min.y += i;
		dy -= i;
	}
	if(r->max.x > sm->clipr.max.x){
		i = r->max.x-sm->clipr.max.x;
		r->max.x -= i;
		dx -= i;
	}
	if(r->max.y > sm->clipr.max.y){
		i = r->max.y-sm->clipr.max.y;
		r->max.y -= i;
		dy -= i;
	}
	return dx>0 && dy>0;
}
