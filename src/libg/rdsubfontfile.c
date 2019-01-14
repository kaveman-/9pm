#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

void
_unpackinfo(Fontchar *i, uchar *p, int n)
{
	int j;

	for(j=0; j<=n; j++,i++,p+=6){
		i->x = BGSHORT(p);
		i->top = p[2];
		i->bottom = p[3];
		i->left = p[4];
		i->width = p[5];
	}
}

Subfont*
rdsubfontfile(int fd, Bitmap *b)
{
	char hdr[3*12+4+1];
	int n;
	uchar *p;
	Fontchar *info;
	Subfont *f;

	if(b == 0){
		b = rdbitmapfile(fd);
		if(b == 0)
			return 0;
	}

	if(read(fd, hdr, 3*12) != 3*12)
		berror("rdsubfontfile read 2");
	n = atoi(hdr);
	if(6*(n+1) > Tilesize)
		berror("subfont too large");
	p = malloc(6*(n+1));
	if (p == 0)
		berror("rdsubfontfile malloc");
	if(read(fd, p, 6*(n+1)) != 6*(n+1))
		berror("rdsubfontfile read 3");
	info = malloc(sizeof(Fontchar)*(n+1));
	if(info == 0)
		berror("rdsubfontfile malloc");
	_unpackinfo(info, p, n);
	free(p);
	f = subfalloc(n, atoi(hdr+12), atoi(hdr+24), info, b);
	if(f == 0)
		free(info);
	return f;
}
