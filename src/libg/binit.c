#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

Bitmap	screen;
Font	*font;

int	_eventfd;


static	void	(*onerr)(char*);
static	int	wid;
static	ulong	expld1[256];	/* expand an ldepth one byte into a word */
static  uchar	cmap[256][3];

static	void	expld1init(void);
static int	colorfind(int r, int g, int b);

extern int	pm_wininit(int*, int*, Rectangle*, uchar [256][3]);
extern int	pm_winalloc(int, Rectangle*, char*);
extern int	pm_winload(int, Rectangle, int, uchar*, Point, int);
extern int	pm_winreshape(int, Rectangle *r);
extern void	_bltinit(void);


void	
bflush(void)
{
	Bitmap *b;

	b = &screen;

	if (b->bb.min.x >= b->bb.max.x)
		return;

/*	bshade(b->bb); */
	bshow(b, b->bb);

	b->bb = Rect(0,0,0,0);
	b->waste = 0;
}

static void
packline(ulong *p, uchar *s, int n)
{
	while(n&3) {
		*p = expld1[*s];
		p++;
		s++;
		n--;
	}
	n >>= 2;
	while(n) {
		p[0] = expld1[s[0]];
		p[1] = expld1[s[1]];
		p[2] = expld1[s[2]];
		p[3] = expld1[s[3]];
		p += 4;
		s += 4;
		n--;
	}
}

static uchar lightgreybits[] = {
	0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
	0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
	0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
	0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
};


void
bshade(Rectangle r)
{
	Bitmap *b, *tex;

 	tex = balloc(Rect(0, 0, 16, 16), 0);
	bload(tex, Rect(0, 0, 16, 16), lightgreybits);

	b = balloc(r, 1);
	
	texture(b, r, tex, S);
	bshow(b, r);
	bfree(b);
	bfree(tex);
	sleep(500);
}


/* only works for ldepth one */
void
bshow(Bitmap *b, Rectangle r)
{
	ulong buf[30000], *p;
	uchar *s;
	int step, w, n;
	int minx, maxx, dy, y;

	assert(b->ldepth == 1);

//fprint(2, "bshow = %R\n", r);

	if(!rectclip(&r, b->r))
		return;


	minx = r.min.x & ~3;
	maxx = (r.max.x + 3) & ~3;
	w = maxx-minx;
	step = w<<2;
	dy = sizeof(buf)/step;
	assert(dy > 0);

	s = (uchar*)(b->base + b->zero + r.min.y*b->width);
	s += minx>>2;
	for(y=r.min.y; y<r.max.y; y+=dy) {
		p = buf;
		if(y+dy > r.max.y)
			dy = r.max.y-y;
		n = dy;
		while(n) {
			packline(p, s, w);
			p += w;
			s += b->width<<2;
			n--;
		}
		pm_winload(wid, Rect(r.min.x, y, r.max.x, y+dy), 0, (uchar*)buf, Pt(minx,y), step);
	}
}

int
Rconv(va_list *arg, Fconv *f)
{
	Rectangle r;
	char buf[128];

	r = va_arg(*arg, Rectangle);
	sprint(buf, "%P %P", r.min, r.max);
	strconv(buf, f);
	return 0;
}

int
Pconv(va_list *arg, Fconv *f)
{
	Point p;
	char buf[64];

	p = va_arg(*arg, Point);
	sprint(buf, "[%ld %ld]", p.x, p.y);
	strconv(buf, f);
	return 0;
}

void
binit(void (*f)(char *), char *s, char *label)
{
	Rectangle r;
	char buf[200], *p;
	
	fmtinstall('P', Pconv);
	fmtinstall('R', Rconv);
	_bltinit();
	onerr = f;

	if(s == 0)
		s = getenv("font");
	else
		s = strdup(s);

	if(s == 0) {
		s = getenv("9pm");
		if(s == 0)
			berror("binit: $9pm not found");
		sprint(buf, "%s/font/pelm.8.font", s);
		free(s);
		s = strdup(buf);
	}

	/* convert any backslashes to slash */
	for(p=s; *p; p++)
		if(*p == '\\')
			*p = '/';

	pm_wininit(&_eventfd, 0, &r, cmap);
	r = Rect(0,0,0,0);

	expld1init();

	if(label == 0)
		label = argv0;

	wid = pm_winalloc(0, &r, label);

	if(wid < 0)
		berror("binit: could not create window");

	font = rdfontfile(s);
	if(font == 0) {
		sprint(buf, "binit font load: font=%s", s);
		berror(buf);
	}
	free(s);

	if(brealloc(&screen, r, 1) == 0)
		berror("could not allocate screen bitmap");

	atexit(bexit);

	bflush();
}

void
bclose(void)
{
	atexitdont(bexit);
}

void
bexit(void)
{
	bflush();
}

void
berror(char *s)
{
	char err[ERRLEN];

	if(onerr)
		(*onerr)(s);
	else{
		errstr(err);
		fprint(2, "libg: %s: %s\n", s, err);
		exits(s);
	}
}

double
bloadbandwidth(void)
{
	int i, n, minx, maxx, step;
	double t;
	Rectangle r;
	uchar *p;

	n = 256;
	r = screen.r;
	
	minx = r.min.x & ~0x7;
	maxx = (r.max.x+7) & ~0x7;
	step = maxx-minx;
	p = mallocz(step*Dy(r));

	t = -realtime();
	for(i=0; i<n; i++) {
		pm_winload(wid, r, 0, (uchar*)p, Pt(minx,r.min.y), step);
		/* bshow(&screen, screen.r); */		
	}
	t += realtime();

	free(p);

	return (double)Dx(r)*Dy(r)*n/(1024*1024)/t;
}



Rectangle
bscreenrect(Rectangle *clipr)
{
	Rectangle r;

	r = screen.r;

	if(clipr)
		*clipr = r;

	return r;
}

void
_reshape(void)
{
	Rectangle r;

	if(!pm_winreshape(wid, &r))
		berror("_reshape failed");

/* fprint(2, "_reshape %R\n", r); */

	bfreemem(&screen);

	if (brealloc(&screen, r, screen.ldepth) == 0)
		berror("could not allocate screen bitmap");
}

static	void
expld1init(void)
{
	int i, j;
	uchar v[4];
	uchar *p;


	/* find 4 colors in cmap */
	for(i=0; i<256; i++)
	v[0] = colorfind(255,255,255);
	v[1] = colorfind(170,170,170);
	v[2] = colorfind(85,85,85);
	v[3] = colorfind(0,0,0);

	p = (uchar*)expld1;

	for(i=0; i<256; i++) {
		for(j=0; j<4; j++) {
			*p = v[(i>>((3-j)*2))&3];
			p++;
		}
	}
}

static int
colorfind(int r, int g, int b)
{
	int i;

	for(i=0; i<256; i++)
		if(cmap[i][0] == r && cmap[i][1] == g && cmap[i][2] == b)
			return i;

	fatal("binit: could not find color [%d, %d, %d]", r, g, b);
}
