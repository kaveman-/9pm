#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

int
linesize(Bitmap *b, Rectangle r)
{
	int ws, minx, maxx;

	ws = 8>>b->ldepth;		/* pixels per byte */
	minx = r.min.x&~(ws-1);
	maxx = (r.max.x + ws-1)&~(ws-1);
	return (maxx-minx)/ws;
}
