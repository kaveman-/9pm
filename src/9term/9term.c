#include <u.h>
#include <9pm/libc.h>
#include <9pm/ctype.h>
#include <9pm/libg.h>
#include <9pm/frame.h>

typedef struct Text	Text;

enum
{
	/* these are chosen to use malloc()'s properties well */
	HiWater	= 640000,	/* max size of history */
	LoWater	= 330000,	/* min size of history after max'ed */
	SnarfSize = 64*1024,	/* max snarf size */
};

/* various geometric paramters */
enum
{
	Scrollwid 	= 12,		/* width of scroll bar */
	Scrollgap 	= 4,		/* gap right of scroll bar */
	Maxtab		= 4,
};

enum
{
	Erc		= 0x04,
	Ekick		= 0x08,
};

enum
{
	Cut,
	Paste,
	Snarf,
	Send,
	Scroll,
};


#define	SCROLLKEY	Kdown
#define	ESC		0x1B
#define CUT		0x18	/* ctrl-x */		
#define COPY		0x03	/* crtl-c */
#define PASTE		0x16	/* crtl-v */
#define BACKSCROLLKEY		Kup

struct Text
{
	Frame		*f;		/* frame ofr terminal */
	Mouse		m;
	uint		nr;		/* num of runes in term */
	Rune		*r;		/* runes for term */
	uint		nraw;		/* num of runes in raw buffer */
	Rune		*raw;		/* raw buffer */
	uint		org;		/* first rune on the screen */
	uint		q0;		/* start of selection region */
	uint		q1;		/* end of selection region */
	uint		qh;		/* unix point */
	int		npart;		/* partial runes read from console */
	char		part[UTFmax];	
	int		nsnarf;		/* snarf buffer */
	Rune		*snarf;
};

Rectangle	scrollr;	/* scroll bar rectangle */
Rectangle	lastsr;		/* used for scroll bar */
int		holdon;		/* hold mode */
int		rawon;		/* raw mode */
int		scrolling;	/* window scrolls */
int		clickmsec;	/* time of last click */
uint		clickq0;	/* point of last click */
Bitmap		*darkgrey;	/* texture bitmap for scroll bars */
Bitmap		*lightgrey;	/* texture bitmap for scroll bars */
int		kickfd;
int		rcfd[2];
int		rcpid;
int		reshaped;	/* window was reshaped - i.e. deffered reshape */
int		canreshape;	/* can do reshape now */
int		maxtab;

char *menu2str[] = {
	"cut",
	"paste",
	"snarf",
	"send",
	"scroll",
	0
};

uchar darkgreybits[] = {
	0xDD, 0xDD, 0x77, 0x77, 0xDD, 0xDD, 0x77, 0x77,
};

uchar darkgreysolidbits[] = {
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
};

uchar lightgreysolidbits[] = {
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
};

uchar lightgreybits[] = {
	0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
};

Menu menu2 =
{
	menu2str
};

Text	t;

void	mouse(void);
void	domenu2(int);
void	loop(void);
void	geom(void);
void	fill(void);
void	tcheck(void);
void	updatesel(void);
void	doreshape(void);
void	rcstart(int fd[2]);
void	runewrite(Rune*, int);
void	consread(void);
void	conswrite(char*, int);
int	bswidth(Rune c);
void	cut(void);
void	paste(Rune*, int, int);
void	snarfupdate(void);
void	snarf(void);
void	show(uint);
void	key(Rune);
void	setorigin(uint org, int exact);
uint	line2q(uint);
uint	backnl(uint, uint);
int	cansee(uint);
uint	backnl(uint, uint);
void	addraw(Rune*, int);
void	setcursor(void);
void	select(void);
void	doubleclick(uint *q0, uint *q1);
int	clickmatch(int cl, int cr, int dir, uint *q);
Rune	*strrune(Rune *s, Rune c);
int	consready(void);
Rectangle scrpos(Rectangle r, ulong p0, ulong p1, ulong tot);
void	scrdraw(void);
void	scroll(int);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))

void
main(int argc, char *argv[])
{
	char *font = 0;
	int pfd[2];
	char *p;

	ARGBEGIN{
	case 'f':
		font = ARGF();
		break;
	case 'T':
		p = ARGF();
		if(p == 0)
			break;
		maxtab = strtoul(p, 0, 0);
		break;
	case 's':
		scrolling++;
		break;
	}ARGEND

	p = getenv("tabstop");
	if(p == 0)
		p = getenv("TABSTOP");
	if(p != 0 && maxtab <= 0)
		maxtab = strtoul(p, 0, 0);
	if(maxtab <= 0)
		maxtab = 8;

	binit(0, font, "9term");
	einit(Emouse|Ekeyboard);
	rcstart(rcfd);

	estart(Erc, rcfd[0], 0);
	if(pipe(pfd)<0)
		fatal("could not create pipe");
	estart(Ekick, pfd[0], 1);
	kickfd = pfd[1];

	t.f = mallocz(sizeof(Frame));

	darkgrey = balloc(Rect(0, 0, 16, 4), 0);
	if(darkgrey==0)
		fatal("can't initialize textures");
	wrbitmap(darkgrey, 0, 4, darkgreybits);

	lightgrey = balloc(Rect(0, 0, 16, 4), 1);
	if(lightgrey==0)
		fatal("can't initialize textures");
	wrbitmap(lightgrey, 0, 4, lightgreysolidbits);
	
	geom();

	loop();
}

void
loop(void)
{
	GEvent e;
	int mask, type;

	for(;;) {
		tcheck();

		/* do deferred reshapes */
		if(reshaped)
			doreshape();

		scrdraw();
		mask = ~0;
		if(!scrolling && t.qh > t.org+t.f->nchars)
			mask &= ~Erc;

		canreshape = 1;
		type = eread(mask, &e);
		canreshape = 0;

		switch(type) {
		default:
			fatal("unknown event type: %d", type);
		case Emouse:
			t.m = e.mouse;
			mouse();
			break;
		case Ekeyboard:
			key(e.kbdc);
			break;
		case Erc:
			conswrite(e.data, e.n);
			break;
		case Ekick:
			break;
		}
	}
}

void
ereshaped(Rectangle r)
{
	if(canreshape)
		doreshape();
	else {
		reshaped = 1;
		write(kickfd, "r", 1);
	}
}

/* real reshape is here */
void
doreshape(void)
{
	reshaped = 0;
	geom();
	scrdraw();
}

void
frgetmouse(void)
{
	t.m = emouse();
}

void
geom(void)
{
	Rectangle r;

	r = screen.r;
	scrollr = screen.r;
	scrollr.max.x = r.min.x+Scrollwid;
	lastsr = Rect(0,0,0,0);

	r.min.x += Scrollwid+Scrollgap;

	frclear(t.f);
	frinit(t.f, r, font, &screen);
	t.f->maxtab = maxtab*charwidth(font, '0');
	fill();
	updatesel();
}

void
mouse(void)
{
	int but;
	
	but = t.m.buttons;

	if(but != 1 && but != 2 && but != 4)
		return;

	if (ptinrect(t.m.xy, scrollr)) {
		scroll(but);
		if(t.qh<=t.org+t.f->nchars)
			consread();;
		return;
	}
		
	switch(but) {
	case 1:
		select();
		break;
	case 2:
		domenu2(2);
		break;
	case 4:
		domenu2(3);
		break;
	}
}

void
select(void)
{
	int b, x, y;
	uint q0;

	b = t.m.buttons;
	q0 = frcharofpt(t.f, t.m.xy) + t.org;
	if(t.m.msec-clickmsec<500 && clickq0 == q0 && t.q0==t.q1){
		doubleclick(&t.q0, &t.q1);
		updatesel();
/*		t.t.i->flush(); */
		x = t.m.xy.x;
		y = t.m.xy.y;
		/* stay here until something interesting happens */
		do {
			t.m = emouse();
		} while(t.m.buttons==b && abs(t.m.xy.x-x)<4 && abs(t.m.xy.y-y)<4);
		t.m.xy.x = x;	/* in case we're calling frselect */
		t.m.xy.y = y;
		clickmsec = 0;
	}

	if(t.m.buttons == b) {
		frselect(t.f, &t.m);
		t.q0 = t.f->p0 + t.org;
		t.q1 = t.f->p1 + t.org;
		clickmsec = t.m.msec;
		clickq0 = t.q0;
	}
}

void
domenu2(int but)
{
	if(scrolling)
		menu2str[Scroll] = "noscroll";
	else
		menu2str[Scroll] = "scroll";

	switch(menuhit(but, &t.m, &menu2)){
	case -1:
		break;
	case Cut:
		snarf();
		cut();
		if(scrolling)
			show(t.q0);
		break;
	case Paste:
		snarfupdate();
		paste(t.snarf, t.nsnarf, 0);
		if(scrolling)
			show(t.q0);
		break;
	case Snarf:
		snarf();
		if(scrolling)
			show(t.q0);
		break;
	case Send:
		snarf();
		t.q0 = t.q1 = t.nr;
		updatesel();
		snarfupdate();
		paste(t.snarf, t.nsnarf, 1);
		if(t.nsnarf == 0 || t.snarf[t.nsnarf-1] != '\n')
			paste(L"\n", 1, 1);
		show(t.nr);
		consread();
		break;
	case Scroll:
		scrolling = !scrolling;
		if (scrolling) {
			show(t.nr);
			consread();
		}
		break;
	default:
		fatal("bad menu item");
	}
}

void
key(Rune r)
{
	char buf[1];

	if(r == 0)
		return;
	if(r==SCROLLKEY){	/* scroll key */
		setorigin(line2q(t.f->maxlines*2/3), 1);
		if(t.qh<=t.org+t.f->nchars)
			consread();
		return;
	}else if(r == BACKSCROLLKEY){
		setorigin(backnl(t.org, t.f->maxlines*2/3), 1);
		return;
	}else if(r == CUT){
		snarf();
		cut();
		if(scrolling)
			show(t.q0);
		return;
	}else if(r == COPY){
		snarf();
		if(scrolling)
			show(t.q0);
		return;
	}else if(r == PASTE){
		snarfupdate();
		paste(t.snarf, t.nsnarf, 0);
		if(scrolling)
			show(t.q0);
		return;
	}

	if(rawon && t.q0==t.nr){
		addraw(&r, 1);
		return;
	}

	if(r==ESC || (holdon && r==0x7F)){	/* toggle hold */
		holdon = !holdon;
		setcursor();

		if(!holdon)
			consread();
		if(r == 0x1B)
			return;
	}

	snarf();

	switch(r) {
	case 0x7F:	/* DEL: send interrupt */
		t.qh = t.q0 = t.q1 = t.nr;
		show(t.q0);
		buf[0] = 0x7f;
		if(write(rcfd[1], buf, 1) < 0)
			exits(0);
		/* get rc to print prompt */
//		r = '\n';
//		paste(&r, 1, 1);
		break;
	case 0x08:	/* ^H: erase character */
	case 0x15:	/* ^U: erase line */
	case 0x17:	/* ^W: erase word */
		if (t.q0 != 0 && t.q0 != t.qh)
			t.q0 -= bswidth(r);
		cut();
		break;
	default:
		paste(&r, 1, 1);
		break;
	}
	if(scrolling)
		show(t.q0);
}

int
bswidth(Rune c)
{
	uint q, eq, stop;
	Rune r;
	int skipping;

	/* there is known to be at least one character to erase */
	if(c == 0x08)	/* ^H: erase character */
		return 1;
	q = t.q0;
	stop = 0;
	if(q > t.qh)
		stop = t.qh;
	skipping = 1;
	while(q > stop){
		r = t.r[q-1];
		if(r == '\n'){		/* eat at most one more character */
			if(q == t.q0)	/* eat the newline */
				--q;
			break; 
		}
		if(c == 0x17){
			eq = isalnum(r);
			if(eq && skipping)	/* found one; stop skipping */
				skipping = 0;
			else if(!eq && !skipping)
				break;
		}
		--q;
	}
	return t.q0-q;
}

int
consready(void)
{
	int i, c;

	if(holdon)
		return 0;

	if(rawon) 
		return t.nraw != 0;

	/* look to see if there is a complete line */
	for(i=t.qh; i<t.nr; i++){
		c = t.r[i];
		if(c=='\n' || c=='\004')
			return 1;
	}
	return 0;
}


void
consread(void)
{
	char buf[8000], *p, n;
	int c, width;

	for(;;) {
		if(!consready())
			return;

		n = sizeof(buf);
		p = buf;
		while(n >= UTFmax && (t.qh<t.nr || t.nraw > 0)) {
			if(t.qh == t.nr){
				width = runetochar(p, &t.raw[0]);
				t.nraw--;
				runemove(t.raw, t.raw+1, t.nraw);
			}else
				width = runetochar(p, &t.r[t.qh++]);
			c = *p;
			p += width;
			n -= width;
			if(!rawon && (c == '\n' || c == '\004')) {
				if(c == '\004')
					p--;
				break;
			}
		}
		if(n < UTFmax && t.qh<t.nr && t.r[t.qh]=='\004')
			t.qh++;
		/* put in control-d when doing a zero length write */
		if(p == buf)
			*p++ = '\004';
		if(write(rcfd[1], buf, p-buf) < 0)
			exits(0);
/*		mallocstats(); */
	}
}

void
conswrite(char *p, int n)
{
	int n2, i;
	Rune buf2[1000], *q;

	/* convert to runes */
	i = t.npart;
	if(i > 0){
		/* handle partial runes */
		while(i < UTFmax && n>0) {
			t.part[i] = *p;
			i++;
			p++;
			n--;
			if(fullrune(t.part, i)) {
				t.npart = 0;
				chartorune(buf2, t.part);
				runewrite(buf2, 1);
				break;
			}
		}
		/* there is a little extra room in a message buf */
	}

	while(n >= UTFmax || fullrune(p, n)) {
		n2 = nelem(buf2);
		q = buf2;

		while(n2) {
			if(n < UTFmax && !fullrune(p, n))
				break;
			i = chartorune(q, p);
			p += i;
			n -= i;
			n2--;
			q++;
		}

		runewrite(buf2, q-buf2);
	}

	if(n != 0) {
		assert(n+t.npart < UTFmax);
		memcpy(t.part+t.npart, p, n);
		t.npart += n;
	}

	if(scrolling)
		show(t.qh);
}

void
runewrite(Rune *r, int n)
{
	uint m;
	int i;
	uint initial;
	uint q0, q1;
	uint p0, p1;
	Rune *p, *q;

	if(n == 0)
		return;

	/* get ride of backspaces */
	initial = 0;
	p = q = r;
	for(i=0; i<n; i++) {
		if(*p == '\b') {
			if(q == r)
				initial++;
			else
				--q;
		} else if(*p)
			*q++ = *p;
		p++;
	}
	n = q-r;

	if(initial){
		/* write turned into a delete */

		if(initial > t.qh)
			initial = t.qh;
		q0 = t.qh-initial;
		q1 = t.qh;

		runemove(t.r+q0, t.r+q1, t.nr-q1);
		t.nr -= initial;
		t.qh -= initial;
		if(t.q0 > q1)
			t.q0 -= initial;
		else if(t.q0 > q0)
			t.q0 = q0;
		if(t.q1 > q1)
			t.q1 -= initial;
		else if(t.q1 > q0)
			t.q1 = q0;
		if(t.org > q1)
			t.org -= initial;
		else if(q0 < t.org+t.f->nchars){
			if(t.org < q0)
				p0 = q0 - t.org;
			else {
				t.org = q0;
				p0 = 0;
			}
			p1 = q1 - t.org;
			if(p1 > t.f->nchars)
				p1 = t.f->nchars;
			frdelete(t.f, p0, p1);
			fill();
		}
		updatesel();
		return;
	}

	if(t.nr>HiWater && t.qh>=t.org){
		m = HiWater-LoWater;
		if(m > t.org);
			m = t.org;
		t.org -= m;
		t.qh -= m;
		if(t.q0 > m)
			t.q0 -= m;
		else
			t.q0 = 0;
		if(t.q1 > m)
			t.q1 -= m;
		else
			t.q1 = 0;
		t.nr -= m;
		runemove(t.r, t.r+m, t.nr);
	}
	t.r = runerealloc(t.r, t.nr+n);
	runemove(t.r+t.qh+n, t.r+t.qh, t.nr-t.qh);
	runemove(t.r+t.qh, r, n);
	t.nr += n;
	if(t.qh < t.org)
		t.org += n;
	else if(t.qh <= t.f->nchars+t.org)
		frinsert(t.f, r, r+n, t.qh-t.org);
	if (t.qh <= t.q0)
		t.q0 += n;
	if (t.qh <= t.q1)
		t.q1 += n;
	t.qh += n;
	updatesel();
}


void
cut(void)
{
	uint n, p0, p1;
	uint q0, q1;

	q0 = t.q0;
	q1 = t.q1;

	if (q0 < t.org && q1 >= t.org)
		show(q0);

	n = q1-q0;
	if(n == 0)
		return;
	runemove(t.r+q0, t.r+q1, t.nr-q1);
	t.nr -= n;
	t.q0 = t.q1 = q0;
	if(q1 < t.qh)
		t.qh -= n;
	else if(q0 < t.qh)
		t.qh = q0;
	if(q1 < t.org)
		t.org -= n;
	else if(q0 < t.org+t.f->nchars){
		assert(q0 >= t.org);
		p0 = q0 - t.org;
		p1 = q1 - t.org;
		if(p1 > t.f->nchars)
			p1 = t.f->nchars;
		frdelete(t.f, p0, p1);
		fill();
	}
	updatesel();
}

void
snarfupdate(void)
{
	char buf[SnarfSize];
	int n, i;
	Rune *p;

	n = clipread(buf, SnarfSize);
	if(n < 0) {
		t.nsnarf = 0;
		return;
	}
	t.snarf = runerealloc(t.snarf, n);
	for(i=0,p=t.snarf; i<n; p++)
		i += chartorune(p, buf+i);
	t.nsnarf = p-t.snarf;
}

void
snarf(void)
{
	char buf[SnarfSize], *p;
	int i, n;

	if(t.q1 == t.q0)
		return;
	n = t.q1-t.q0;
	for(i=0,p=buf; i<n && p < buf + SnarfSize-UTFmax; i++)
		p += runetochar(p, t.r+t.q0+i);
	clipwrite(buf, p-buf);
}

void
paste(Rune *r, int n, int advance)
{
	uint m;
	uint q0;

	if(rawon && t.q0==t.nr){
		addraw(r, n);
		return;
	}

	cut();
	if(n == 0)
		return;
	if(t.nr>HiWater && t.q0>=t.org && t.q0>=t.qh){
		m = HiWater-LoWater;
		if(m > t.org)
			m = t.org;
		if(m > t.qh);
			m = t.qh;
		t.org -= m;
		t.qh -= m;
		t.q0 -= m;
		t.q1 -= m;
		t.nr -= m;
		runemove(t.r, t.r+m, t.nr);
	}
	t.r = runerealloc(t.r, t.nr+n);
	q0 = t.q0;
	runemove(t.r+q0+n, t.r+q0, t.nr-q0);
	runemove(t.r+q0, r, n);
	t.nr += n;
	if(q0 < t.qh)
		t.qh += n;
	else
		consread();
	if(q0 < t.org)
		t.org += n;
	else if(q0 <= t.f->nchars+t.org)
		frinsert(t.f, r, r+n, q0-t.org);
	if(advance)
		t.q0 += n;
	t.q1 += n;
	updatesel();
}

void
fill(void)
{
	if (t.f->nlines >= t.f->maxlines)
		return;
	frinsert(t.f, t.r + t.org + t.f->nchars, t.r + t.nr, t.f->nchars);
}

void
updatesel(void)
{
	Frame *f;
	uint n;

	f = t.f;
	if(t.org+f->p0 == t.q0 && t.org+f->p1 == t.q1)
		return;

	n = t.f->nchars;

	frselectp(f, F&~D);
	if (t.q0 >= t.org)
		f->p0 = t.q0-t.org;
	else
		f->p0 = 0;
	if(f->p0 > n)
		f->p0 = n;
	if (t.q1 >= t.org)
		f->p1 = t.q1-t.org;
	else
		f->p1 = 0;
	if(f->p1 > n)
		f->p1 = n;
	frselectp(f, F&~D);

/*
	if(t.qh<=t.org+t.f.nchars && t.cwqueue != 0)
		t.cwqueue->wakeup <-= 0;
*/

	tcheck();
}

void
show(uint q0)
{
	int nl;
	uint q, oq;

	if(cansee(q0))
		return;
	
	if (q0<t.org)
		nl = t.f->maxlines/5;
	else
		nl = 4*t.f->maxlines/5;
	q = backnl(q0, nl);
	/* avoid going in the wrong direction */
	if (q0>t.org && q<t.org)
		q = t.org;
	setorigin(q, 0);
	/* keep trying until q0 is on the screen */
	while(!cansee(q0)) {
		assert(q0 >= t.org);
		oq = q;
		q = line2q(t.f->maxlines-nl);
		assert(q > oq);
		setorigin(q, 1);
	}
}

int
cansee(uint q0)
{
	uint qe;

	qe = t.org+t.f->nchars;

	if(q0>=t.org && q0 < qe)
		return 1;
	if (q0 != qe)
		return 0;
	if (t.f->nlines < t.f->maxlines)
		return 1;
	if (q0 > 0 && t.r[t.nr-1] == '\n')
		return 0;
	return 1;
}


void
setorigin(uint org, int exact)
{
	int i, a;
	uint n;
	
	if(org>0 && !exact){
		/* try and start after a newline */
		/* don't try harder than 256 chars */
		for(i=0; i<256 && org<t.nr; i++){
			if(t.r[org-1] == '\n')
				break;
			org++;
		}
	}
	a = org-t.org;

	if(a>=0 && a<t.f->nchars)
		frdelete(t.f, 0, a);
	else if(a<0 && -a<100*t.f->maxlines){
		n = t.org - org;
		frinsert(t.f, t.r+org, t.r+org+n, 0);
	}else
		frdelete(t.f, 0, t.f->nchars);
	t.org = org;
	fill();
	updatesel();
}


uint
line2q(uint n)
{
	Frame *f;

	f = t.f;
	return frcharofpt(f, Pt(f->r.min.x, f->r.min.y + n*font->height))+t.org;
}

uint
backnl(uint p, uint n)
{
	int i, j;

	for (i = n;; i--) {
		/* at 256 chars, call it a line anyway */
		for(j=256; --j>0 && p>0; p--)
			if(t.r[p-1]=='\n')
				break;
		if (p == 0 || i == 0)
			return p;
		p--;
	}
	return 0; /* alef bug */
}


void
addraw(Rune *r, int nr)
{
	t.raw = runerealloc(t.raw, t.nraw+nr);
	runemove(t.raw+t.nraw, r, nr);
	t.nraw += nr;
/*
	if(t.crqueue != nil)
		t.crqueue->wakeup <-= 0;
*/	
}

void
setcursor(void)
{
/*
	Cursor *p;

	p = nil;
	if(w != nil && mouse->xy.inside(t.rect)) {
		p = t.cursorp;
		if(p==nil && holdon)
			p = &whitearrow;
	}
	mouse->setcursor(p);
*/
}

Rune left1[] =  { '{', '[', '(', '<', '«', 0 };
Rune right1[] = { '}', ']', ')', '>', '»', 0 };
Rune left2[] =  { '\n', 0 };
Rune left3[] =  { '\'', '"', '`', 0 };

Rune *left[] = {
	left1,
	left2,
	left3,
	0
};

Rune *right[] = {
	right1,
	left2,
	left3,
	0
};

void
doubleclick(uint *q0, uint *q1)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	for(i=0; left[i]!=0; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		/* try matching character to left, looking right */
		if(q == 0)
			c = '\n';
		else
			c = t.r[q-1];
		p = strrune(l, c);
		if(p != 0){
			if(clickmatch(c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		/* try matching character to right, looking left */
		if(q == t.nr)
			c = '\n';
		else
			c = t.r[q];
		p = strrune(r, c);
		if(p != 0){
			if(clickmatch(c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<t.nr && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || t.r[0]=='\n')
					(*q0)++;
			}
			return;
		}
	}
	/* try filling out word to right */
	while(*q1<t.nr && isalnum(t.r[*q1]))
		(*q1)++;
	/* try filling out word to left */
	while(*q0>0 && isalnum(t.r[*q0-1]))
		(*q0)--;
}

int
clickmatch(int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == t.nr)
				break;
			c = t.r[*q];
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = t.r[*q];
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}


void
rcstart(int fd[2])
{
	int pfd[2][2];
	int pid;
	char *argv[4];

	argv[0] = "9tty";
	argv[1] = "rcsh";
	argv[2] = "-i";
	argv[3] = 0;

	if(pipe(pfd[0]) < 0)
		fatal("pipe failed");
	if(pipe(pfd[1]) < 0)
		fatal("pipe failed");

	pid = proc(argv, pfd[0][0], pfd[1][1], pfd[1][1], 0);
	if(pid == 0)
		fatal("proc failed: %r");
	close(pfd[0][0]);
	close(pfd[1][1]);
	fd[0] = pfd[1][0];
	fd[1] = pfd[0][1];

	rcpid = pid;
}

void
tcheck(void)
{
	Frame *f;
		
	f = t.f;

	assert(t.q0 <= t.q1 && t.q1 <= t.nr);
	assert(t.org <= t.nr && t.qh <= t.nr);
	assert(f->p0 <= f->p1 && f->p1 <= f->nchars);
	assert(t.org + f->nchars <= t.nr);
	assert(t.org+f->nchars==t.nr || (f->nlines >= f->maxlines));
}

Rune*
strrune(Rune *s, Rune c)
{
	Rune c1;

	if(c == 0) {
		while(*s++)
			;
		return s-1;
	}

	while(c1 = *s++)
		if(c1 == c)
			return s-1;
	return 0;
}

void
scrdraw(void)
{
	Rectangle r, r1, r2;
	static Bitmap *scrx;
	
	r = scrollr;
	r.min.x += 1;	/* border between margin and bar */
	r1 = r;
	if(scrx==0 || scrx->r.max.y < r.max.y){
		if(scrx)
			bfree(scrx);
		scrx = balloc(Rect(0, 0, 32, r.max.y), screen.ldepth);
		if(scrx == 0)
			berror("scroll balloc");
	}
	r1.min.x = 0;
	r1.max.x = Dx(r);
	r2 = scrpos(r1, t.org, t.org+t.f->nchars, t.nr);
	if(!eqrect(r2, lastsr)){
		lastsr = r2;
		bitblt(scrx, r1.min, scrx, r1, F);
		texture(scrx, inset(r1, 1), lightgrey, S);
		bitblt(scrx, r2.min, scrx, r2, 0);
		r2.max.y = r2.min.y;
		r2.min.y--;
		bitblt(scrx, r2.min, scrx, r2, 0xF);
		r2 = lastsr;
		r2.min.y = r2.max.y;
		r2.max.y++;
		bitblt(scrx, r2.min, scrx, r2, 0xF);
		bitblt(&screen, r.min, scrx, r1, S);
	}
}

Rectangle
scrpos(Rectangle r, ulong p0, ulong p1, ulong tot)
{
	long h;
	Rectangle q;

	q = inset(r, 1);
	h = q.max.y-q.min.y;
	if(tot == 0)
		return q;
	if(tot > 1024L*1024L)
		tot >>= 10, p0 >>= 10, p1 >>= 10;
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
scroll(int but)
{
	uint p0, oldp0;
	Rectangle s;
	int x, y, my, h, first, exact;

	s = inset(scrollr, 1);
	h = s.max.y-s.min.y;
	x = (s.min.x+s.max.x)/2;
	oldp0 = ~0;
	first = 1;
	do{
		if(t.m.xy.x<s.min.x || s.max.x<=t.m.xy.x){
			t.m = emouse();
		}else{
			my = t.m.xy.y;
			if(my < s.min.y)
				my = s.min.y;
			if(my >= s.max.y)
				my = s.max.y;
			if(!eqpt(t.m.xy, Pt(x, my)))
				cursorset(Pt(x, my));
			exact = 1;
			if(but == 2){
				y = my;
				if(y > s.max.y-2)
					y = s.max.y-2;
				if(t.nr > 1024*1024)
					p0 = ((t.nr>>10)*(y-s.min.y)/h)<<10;
				else
					p0 = t.nr*(y-s.min.y)/h;
				exact = 0;
			} else if(but == 1)
				p0 = backnl(t.org, (my-s.min.y)/font->height);
			else 
				p0 = t.org+frcharofpt(t.f, Pt(s.max.x, my));

			if(oldp0 != p0)
				setorigin(p0, exact);
			oldp0 = p0;
			scrdraw();
			bflush();
			if(first){
				/* debounce */
				sleep(100);
				first = 0;
			}
			t.m = emouse();
		}
	}while(t.m.buttons & (1<<(but-1)));

	while(t.m.buttons)
		t.m = emouse();
}


