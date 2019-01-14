#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>
#include <9pm/bio.h>

/*
 * This code (and the devbit interface) will have to change
 * if we ever get bitmaps with ldepth > 3, because the
 * colormap will have to be written in chunks
 */

void
wrcolmap(Bitmap *scr, RGB *m)
{
	int i, n, fd;
	char buf[32];
	ulong r, g, b;

	USED(scr);

	fd = open("/dev/colormap", OWRITE);
	if(fd < 0)
		berror("wrcolmap: open /dev/colormap failed");
	for(i = 0; i < 256; i++) {
		r = m[i].red>>24;
		g = m[i].green>>24;
		b = m[i].blue>>24;
		n = sprint(buf, "%d %d %d %d", 255-i, r, g, b);
		if(write(fd, buf, n+1) != n+1) {
			close(fd);
			berror("wrcolmap: bad write");
			return;
		}
	}
	close(fd);
}

static ulong
getval(char **p)
{
	ulong v;
	char *q;

	v = strtoul(*p, &q, 0);
	v |= v<<8;
	v |= v<<16;
	*p = q;
	return v;
}

void
rdcolmap(Bitmap *screen, struct RGB *colmap)
{
	int i;
	char *p, *q;
	Biobuf *b;

	USED(screen);

	b = Bopen("/dev/colormap", OREAD);
	if(b == 0) {
		fprint(2, "rdcolmap: Bopen %r\n");
		exits("bad");
	}

	for(;;) {
		p = Brdline(b, '\n');
		if(p == 0)
			break;
		i = strtoul(p, &q, 0);
		if(i < 0 || i > 255) {
			fprint(2, "rdcolmap: bad index\n");
			exits("bad");
		}
		p = q;
		colmap[i].red = getval(&p);
		colmap[i].green = getval(&p);
		colmap[i].blue = getval(&p);
	}
	Bterm(b);
}
