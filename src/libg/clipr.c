#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

int
clipr(Bitmap *d, Rectangle r)
{
	if(rectclip(&r, d->r) == 0)
		return 0;

	d->clipr = r;
	return 1;
}
