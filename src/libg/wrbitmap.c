#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

void
wrbitmap(Bitmap *b, int miny, int maxy, uchar *data)
{
	bload(b, Rect(b->r.min.x, miny, b->r.max.x, maxy), data);	
}

void
bload(Bitmap *i, Rectangle r, uchar *data)
{
	int y, l, lpart, rpart, mx, m, mr;
	uchar *q;

	if(!rectinrect(r, i->r))
		fatal("bload: bad rectangle %R; %R\n", i->r, r);

	bbound(i, r);

	l = linesize(i, r);
	q = (uchar*)(i->base+i->zero+(r.min.y*i->width))+(r.min.x>>(3-i->ldepth));
	mx = 7>>i->ldepth;
	lpart = (r.min.x & mx) << i->ldepth;
	rpart = (r.max.x & mx) << i->ldepth;
	m = 0xFF >> lpart;
	/* may need to do bit insertion on edges */
	if(l == 1){	/* all in one byte */
		if(rpart)
			m ^= 0xFF >> rpart;
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			q += i->width*sizeof(ulong);
			data++;
		}
		return;
	}
	if(lpart==0 && rpart==0){	/* easy case */
		for(y=r.min.y; y<r.max.y; y++){
			memmove(q, data, l);
			q += i->width*sizeof(ulong);
			data += l;
		}
		return;
	}
	mr = 0xFF ^ (0xFF >> rpart);
	if(lpart!=0 && rpart==0){
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			if(l > 1)
				memmove(q+1, data+1, l-1);
			q += i->width*sizeof(ulong);
			data += l;
		}
		return;
	}
	if(lpart==0 && rpart!=0){
		for(y=r.min.y; y<r.max.y; y++){
			if(l > 1)
				memmove(q, data, l-1);
			q[l-1] ^= (data[l-1]^q[l-1]) & mr;
			q += i->width*sizeof(ulong);
			data += l;
		}
		return;
	}
	for(y=r.min.y; y<r.max.y; y++){
		*q ^= (*data^*q) & m;
		if(l > 2)
			memmove(q+1, data+1, l-2);
		q[l-1] ^= (data[l-1]^q[l-1]) & mr;
		q += i->width*sizeof(ulong);
		data += l;
	}
}

