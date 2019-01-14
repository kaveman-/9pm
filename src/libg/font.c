#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

#define	PJW	0	/* use NUL==pjw for invisible characters */

enum
{
	/* starting values */
	LOG2NFCACHE =	6,
	NFLOOK =	5,				/* #chars to scan in cache */
	NFCACHE = 	((1<<LOG2NFCACHE)+NFLOOK),	/* #chars cached */
	/* max value */
	MAXFCACHE =	(1<<16)+NFLOOK,	/* generous upper limit */
};

typedef struct Glyph		Glyph;
typedef struct Runecache	Runecache;
typedef struct Centry		Centry;


struct Glyph {
	Subfont *f;
	int	off;
};

struct Centry
{
	uint		age;
	uchar		height;
	uchar		ascent;	
	uint		id;	/* id of sub font */
	int		off;	/* offset within subfont */
	int		x;	/* offset in image */
	uchar		top;	/* first non-zero scan-line */
	uchar		bottom;	/* last non-zero scan-line + 1 */
	uchar		left;	/* first non-zero bit */
	uchar		right;	/* last non-zero bit + 1 */
};

struct Runecache {
	uint		age;
	int		width;		/* largest seen so far */
	int		nentry;
	Centry		*entry;
	Bitmap		*b;
};


static int	opensubf(char *fname, char *name);
static Glyph	maprune(Font *f, Rune r);
static void	_string(Bitmap *d, Point pt, int height, int ascent, int n, Glyph *g, Fcode fc);
static void	glyph(Bitmap *b, Point pt, int height, int ascent, Glyph g, int code);
static Centry	*loadchar(Runecache*, Glyph *, int, int);
static uint	agecache(Runecache*);
static void	resize(Runecache*, int, int);

/* one cache for each power of two for font hieght and each ldepth */
static Runecache	cache[8][4];


int
charwidth(Font *f, Rune r)
{
	Glyph g;

	g = maprune(f, r);
	if (g.f == 0)
		return 0;
	return g.f->info[g.off].width;
}

static Glyph
maprune(Font *f, Rune r)
{
	int i, fd;
	Fontmap *m;
	Glyph g;

	for(i=0; i < f->nmap; i++) {
		m = f->map[i];
		if (r < m->min || r > m->max)
			continue;
		if (m->f == 0) {
			fd = opensubf(f->name, m->name);
			if (fd < 0)
				goto TryPJW;
			m->f = rdsubfontfile(fd, 0);
			close(fd);
		}
		if (m->f == 0)
			goto TryPJW;
		r -= m->min;
		r += m->offset;
		if(r >= m->f->n)
			goto TryPJW;
		g.f = m->f;
		g.off = r;
		return g;
	}

TryPJW:
	if (r != PJW)
		return maprune(f, PJW);

	g.f = 0;
	g.off = 0;
	return g;
}

int
strwidth(Font *f, char *s)
{
	int twid;
	Rune r;
	Glyph g;
	Fontchar *c;

	twid = 0;
	while(*s){
		r = s[0];
		if(r < Runeself)
			s++;
		else{
			s += chartorune(&r, s);
		}
		g = maprune(f, r);
		if (g.f == 0)
			continue;
		c = g.f->info + g.off;
		twid += c->width;
	}
	return twid;
}

Point
strsize(Font *f, char *s)
{
	return Pt(strwidth(f, s), f->height);
}

Point
string(Bitmap *d, Point pt, Font *f, char *s, Fcode fc)
{
	Rune r;
	Glyph g;

	while(*s){
		r = s[0];
		if(r < Runeself)
			s++;
		else{
			s += chartorune(&r, s);
		}
		g = maprune(f, r);
		if (g.f == 0)
			continue;
		glyph(d, pt, f->height, f->ascent, g, fc);
		pt.x += g.f->info[g.off].width;
	}
	return pt;
}



/*
 * Determine minimum ldepth necessary to hold all subfonts accurately.
 * Takes too long, really, so only check the first few subfonts.
 */
int
fminldepth(Font *f)
{
	int i, m, l, fd;
	Fontmap *c;
	char buf[12];

	m = 0;
	for(i=0; i<f->nmap && i<3; i++){
		c = f->map[i];
		fd = opensubf(f->name, c->name);
		if(fd < 0)
    			continue;
		/* ldepth is first word of bitmap */
		l = read(fd, buf, 12);
		close(fd);
		if(l != 12)
			continue;
		l = atoi(buf);
		if(l > m)
			m = l;
	}
	return m;
}


static int
opensubf(char *fname, char *name)
{
	char tmp1[128], tmp2[128];
	char *t, *u;
	int fd;

	/*
	 * prefix name with directory if necessary
	 * and try to find a file suffixed with the right ldepth.
 	*/
	t = name;
	if(t[0] != '/'){
		strcpy(tmp2, fname);
		u = utfrrune(tmp2, '/');
		if(u)
			u[0] = 0;
		else
			strcpy(tmp2, ".");
		sprint(tmp1, "%s/%s", tmp2, t);
		t = tmp1;
	}
	fd = open(t, OREAD);
	if(fd < 0)
		fprint(2, "can't open subfont %s: %r\n", t);

	return fd;
}

static void
glyph(Bitmap *b, Point pt, int height, int ascent, Glyph g, int code)
{
	Runecache *rc;
	int i, full;
	Centry *c;
	int width;
	Rectangle r;

	for(i = 0; i < 7; i++)
		if((1<<i) >= height)
			break;

	rc = &cache[i][b->ldepth];

	if(rc->b == 0){
		rc->b = balloc(Rect(0,0,1,1<<i), b->ldepth);
		resize(rc, g.f->info[g.off].width, NFCACHE);
	}
		
	full = (code&~S)^(D&~S);	/* result involves source */
	width = g.f->info[g.off].width;
	c = loadchar(rc, &g, height, ascent);
	if (full) {
		r = Rect(c->x, 0, c->x+width, height);
		bitblt(b, pt, rc->b, r, code);
	} else {
		r = Rect(c->x+c->left, c->top, c->x+c->right, c->bottom);
		bitblt(b, Pt(pt.x+c->left, pt.y+c->top), rc->b, r, code);
	}
}

static Centry*
loadchar(Runecache *rc, Glyph *g, int height, int ascent)
{
	uint sh, h, th;
	Centry *c, *ec, *tc;
	int width, off, nc;
	Fontchar *fc;
	uint a;
	Rectangle r;

	fc = &g->f->info[g->off];
	
	width = fc->width;

	if(width > rc->width)
		resize(rc, width, rc->nentry);

	sh = ((g->f->qid*17 + g->off)*17 + height+ascent) & (rc->nentry-NFLOOK-1);
	c = &rc->entry[sh];
	ec = c+NFLOOK;
	for(;c < ec; c++)
		if(c->height == height && c->ascent == ascent && c->id == g->f->qid &&
				c->off == g->off) {
			c->age = agecache(rc);
			return c;
		}

	/*
	 * Not found; toss out oldest entry
	 */
	a = ~0U;
	th = sh;
	h = 0;
	tc = &rc->entry[th];
	while(tc < ec){
		if(tc->age < a){
			a = tc->age;
			h = th;
			c = tc;
		}
		tc++;
		th++;
	}

	if(a>0 && (rc->age-a)<500){	/* kicking out too recent; resize */
		nc = 2*(rc->nentry-NFLOOK) + NFLOOK;
		if(nc <= MAXFCACHE) {
			resize(rc, rc->width, nc);
			return loadchar(rc, g, height, ascent);
		}
	}
	c->height = height;
	c->ascent = ascent;
	c->id = g->f->qid;
	c->off = g->off;
	c->x = h*rc->width;

	off = ascent - g->f->ascent;

	if(fc->left > 0)
		c->left = fc->left;
	else
		c->left = 0;

	c->right = fc[1].x-fc[0].x + fc->left;
	if(c->right > width)
		c->right = width;

	if(fc->top+off >= 0)
		c->top = c->top;
	else
		c->top = 0;

	if(fc->bottom+off <= height)
		c->bottom = fc->bottom+off;
	else
		c->bottom = g->f->height;

	bitblt(rc->b, Pt(c->x, 0), rc->b, Rect(0, 0, width, height), Zero);
	clipr(rc->b, Rect(c->x, 0, c->x+width, height));
	r = Rect(fc->x, fc->top, fc[1].x, fc->bottom);
	bitblt(rc->b, Pt(c->x+fc->left, fc->top+off), g->f->bits, r, S);
	clipr(rc->b, rc->b->r);

	c->age = agecache(rc);
	return c;
}

static uint
agecache(Runecache *rc)
{
	Centry *c, *ec;

	rc->age++;
	if(rc->age < (1<<31))
		return rc->age;

	/*
	 * Renormalize ages
	 */
	c = rc->entry;
	ec = c+rc->nentry;
	for(;c < ec; c++)
		if(c->age > 1)
			c->age >>= 2;
	return rc->age >>=2;
}

static void
resize(Runecache *rc, int wid, int nentry)
{
	int ldepth, height;

	ldepth = rc->b->ldepth;
	height = rc->b->r.max.y;

	bfree(rc->b);
	rc->b = balloc(Rect(0, 0, nentry*wid, height), ldepth);
	rc->width = wid;
	rc->entry = realloc(rc->entry, nentry*sizeof(Centry));
	rc->nentry = nentry;
	/* just wipe the cache clean and things will be ok */
	memset(rc->entry, 0, rc->nentry*sizeof(Centry));
}
