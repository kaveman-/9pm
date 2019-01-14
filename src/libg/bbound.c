#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

int	nowaste;
int	noflush;

void
bbound(Bitmap *b, Rectangle r)
{
	int abb, ar, anbb;
	Rectangle nbb;

	if(b != &screen)
		return;

	if(!rectclip(&r, b->r))
		return;

	if (nowaste) {
		if(b->bb.min.x < b->bb.max.x)
			bshow(b, b->bb);
		b->bb = r;
		b->waste = 0;
		return;
	}

	if(b->bb.min.x >= b->bb.max.x){
		b->bb = r;
		b->waste = 0;
		return;
	}

	nbb = b->bb;
	if(r.min.x < nbb.min.x)
		nbb.min.x = r.min.x;
	if(r.min.y < nbb.min.y)
		nbb.min.y = r.min.y;
	if(r.max.x > nbb.max.x)
		nbb.max.x = r.max.x;
	if(r.max.y > nbb.max.y)
		nbb.max.y = r.max.y;

	if (noflush) {
		b->bb = nbb;
		return;
	}

	ar = (r.max.x-r.min.x)*(r.max.y-r.min.y);
	abb = (b->bb.max.x-b->bb.min.x)*(b->bb.max.y-b->bb.min.y);
	anbb = (nbb.max.x-nbb.min.x)*(nbb.max.y-nbb.min.y);
	/*
	 * Area of new waste is area of new bb minus area of old bb,
	 * less the area of the new segment, which we assume is not waste.
	 * This could be negative, but that's OK.
	 */
	b->waste += anbb-abb-ar;
	if(b->waste < 0)
		b->waste = 0;
	/*
	 * absorb if:
	 *	total area is small
	 *	waste is less than ½ total area
	 * 	rectangles touch
	 */
	if(anbb<=1024 || b->waste*2<anbb || rectXrect(b->bb, r)){
		b->bb = nbb;
		return;
	}
	/* emit current state */
	bshow(b, b->bb);
	b->bb = r;
	b->waste = 0;
}
