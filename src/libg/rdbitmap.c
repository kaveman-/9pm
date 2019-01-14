#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

void
rdbitmap(Bitmap *b, int miny, int maxy, uchar *data)
{
	long dy;
	ulong l, n;
	uchar hdr[19];

	l = linesize(b, b->r);
	if(l == 0)
		return;

	qlock(&libglock);

	bneed(0);
	hdr[0] = 'r';
	BPSHORT(hdr+1, b->id);
	while(maxy > miny){
		dy = maxy - miny;
		if(dy*l > Tilesize)
			dy = Tilesize/l;
		BPLONG(hdr+3, b->r.min.x);
		BPLONG(hdr+7, miny);
		BPLONG(hdr+11, b->r.max.x);
		BPLONG(hdr+15, miny+dy);
		n = dy*l;
		if(n == 0)
			berror("rdbitmap read too large");
		if(write(_bitbltfd, hdr, 11) != 11)
			berror("rdbitmap write");
		if(read(_bitbltfd, data, n) != n)
			berror("rdbitmap read");
		data += n;
		miny += dy;
	}
	
	qunlock(&libglock);
}
