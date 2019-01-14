#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

static	int	qid;

Subfont*
subfalloc(int n, int height, int ascent, Fontchar *info, Bitmap *b)
{
	Subfont *f;

	f = malloc(sizeof(Subfont));
	if (f == 0)
		return 0;
	f->bits = b;
	f->height = height;
	f->ascent = ascent;
	f->n = n;
	f->info = info;
	f->qid = qid++;

	return f;
}

void
subffree(Subfont *f)
{
	USED(f);
}
