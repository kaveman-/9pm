#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>
#include <9pm/frame.h>
#include "flayer.h"
#include "samterm.h"

Text	cmd;
Rune	*scratch;
long	nscralloc;
Cursor	*cursor;
extern Bitmap	screen;
Mouse	mouse;
Flayer	*which = 0;
Flayer	*work = 0;
long	snarflen;
long	typestart = -1;
long	typeend = -1;
long	typeesc = -1;
long	modified = 0;		/* strange lookahead for menus */
char	samlock = 1;
char	hasunlocked = 0;
int	canreshape;
int	maxtab;

void	samstart(void);

void
main(int argc, char *argv[])
{
	int i, got, scr;
	Text *t;
	Rectangle r;
	Flayer *nwhich;
	char *termfont = 0;
	int revbut = 0;
	char *p;

	ARGBEGIN{
	case 'b':
		revbut++;
		break;
	case 'f':
		termfont = ARGF();
		break;
	case 'T':
		p = ARGF();
		if(p == 0)
			break;
		maxtab = strtoul(p, 0, 0);
		break;

	// sam options passed to samterm
	case 'r':
	case 't':
	case 's':
		ARGF();
		break;
	}ARGEND

	p = getenv("tabstop");
	if(p == 0)
		p = getenv("TABSTOP");
	if(p != 0 && maxtab <= 0)
		maxtab = strtoul(p, 0, 0);
	if(maxtab <= 0)
		maxtab = 8;

	getscreen(termfont);
	iconinit();
	initio();
	scratch = alloc(100*RUNESIZE);
	nscralloc = 100;
	r = screen.r;
	r.max.y = r.min.y+Dy(r)/5;
	flstart(screen.clipr);
	rinit(&cmd.rasp);
	flnew(&cmd.l[0], gettext, 1, &cmd);
	flinit(&cmd.l[0], r, font);
	cmd.nwin = 1;
	current(&cmd.l[0]);
	cmd.tag = Untagged;
	outTs(Tversion, VERSION);
	startnewfile(Tstartcmdfile, &cmd);

	got = 0;
	for(;;){
		canreshape = hasunlocked;
		got = waitforio();
		canreshape = 0;

		if(hasunlocked && RESHAPED())
			reshape();
		if(got&RHost)
			rcv();
		if(got&RExtern){
			for(i=0; cmd.l[i].textfn==0; i++)
				;
			current(&cmd.l[i]);
			flsetselect(which, cmd.rasp.nrunes, cmd.rasp.nrunes);
			type(which, RExtern);
		}
		if(got&RKeyboard)
			if(which)
				type(which, RKeyboard);
			else
				kbdblock();
		if(got&RMouse){
			if(samlock==2 || !ptinrect(mouse.xy, screen.r)){
				mouseunblock();
				continue;
			}
			nwhich = flwhich(mouse.xy);
			scr = which && ptinrect(mouse.xy, which->scroll);
			if(mouse.buttons)
				flushtyping(1);
			if(mouse.buttons&1){
				if(nwhich){
					if(nwhich!=which)
						current(nwhich);
					else if(scr)
						scroll(which, 1, revbut?3:1);
					else{
						t=(Text *)which->user1;
						if(flselect(which)){
							outTsl(Tdclick, t->tag, which->p0);
							t->lock++;
						}else if(t!=&cmd)
							outcmd();
					}
				}
			}else if((mouse.buttons&2) && which){
				if(scr)
					scroll(which, 2, 2);
				else
					menu2hit();
			}else if((mouse.buttons&4)){
				if(scr)
					scroll(which, 3, revbut?1:3);
				else
					menu3hit();
			}
			mouseunblock();
		}
		if(got&Ekick)
			kick();
	}
}

void
samstart(void)
{
	int ph2t[2], pt2h[2];
	char *argv[3];

	if(pipe(ph2t)==-1 || pipe(pt2h)==-1)
		panic("pipe");

	argv[0] = "sam.exe";
	argv[1] = "-R";
	argv[2] = 0;

	if(proc(argv, ph2t[0], pt2h[1], 1, 0)==-1)
		panic("proc failed");
	dup(pt2h[0], 0);
	dup(ph2t[1], 1);
	close(ph2t[0]);
	close(ph2t[1]);
	close(pt2h[0]);
	close(pt2h[1]);
}

void
reshape(void)
{
	int i;

/* fprint(2, "reshaped %R\n", screen.r); */

	flreshape(screen.clipr);
	for(i = 0; i<nname; i++)
		if(text[i])
			hcheck(text[i]->tag);
}

void
current(Flayer *nw)
{
	Text *t;

	if(which == nw)
		return;

	if(which)
		flborder(which, 0);
	if(nw){
		flushtyping(1);
		flupfront(nw);
		flborder(nw, 1);
		buttons(Up);
		t = (Text *)nw->user1;
		t->front = nw-&t->l[0];
		if(t != &cmd)
			work = nw;
	}
	which = nw;
}

void
closeup(Flayer *l)
{
	Text *t=(Text *)l->user1;
	int m;

	m = whichmenu(t->tag);
	if(m < 0)
		return;
	flclose(l);
	if(l == which){
		which = 0;
		current(flwhich(Pt(0, 0)));
	}
	if(l == work)
		work = 0;
	if(--t->nwin == 0){
		rclear(&t->rasp);
		free((uchar *)t);
		text[m] = 0;
	}else if(l == &t->l[t->front]){
		for(m=0; m<NL; m++)	/* find one; any one will do */
			if(t->l[m].textfn){
				t->front = m;
				return;
			}
		panic("close");
	}
}

Flayer *
findl(Text *t)
{
	int i;
	for(i = 0; i<NL; i++)
		if(t->l[i].textfn==0)
			return &t->l[i];
	return 0;
}

void
duplicate(Flayer *l, Rectangle r, Font *f, int close)
{
	Text *t=(Text *)l->user1;
	Flayer *nl = findl(t);
	Rune *rp;
	ulong n;

	if(nl){
		flnew(nl, gettext, l->user0, (char *)t);
		flinit(nl, r, f);
		nl->origin = l->origin;
		rp = (*l->textfn)(l, l->f.nchars, &n);
		flinsert(nl, rp, rp+n, l->origin);
		flsetselect(nl, l->p0, l->p1);
		if(close){
			flclose(l);
			if(l==which)
				which = 0;
		}else
			t->nwin++;
		current(nl);
		hcheck(t->tag);
	}
	cursorswitch(cursor);
}

void
buttons(int updown)
{
	while(((mouse.buttons&7)!=0) != updown)
		frgetmouse();
}

int
getr(Rectangle *rp)
{
	Point p;
	Rectangle r;

	*rp = getrect(3, &mouse);
	if(rp->max.x && rp->max.x-rp->min.x<=5 && rp->max.y-rp->min.y<=5){
		p = rp->min;
		r = cmd.l[cmd.front].entire;
		*rp = screen.r;
		if(cmd.nwin==1){
			if (p.y <= r.min.y)
				rp->max.y = r.min.y;
			else if (p.y >= r.max.y)
				rp->min.y = r.max.y;
			if (p.x <= r.min.x)
				rp->max.x = r.min.x;
			else if (p.x >= r.max.x)
				rp->min.x = r.max.x;
		}
	}
	return rectclip(rp, screen.r) &&
	   rp->max.x-rp->min.x>100 && rp->max.y-rp->min.y>40;
}

void
snarf(Text *t, int w)
{
	Flayer *l = &t->l[w];

	if(l->p1>l->p0){
		snarflen = l->p1-l->p0;
		outTsll(Tsnarf, t->tag, l->p0, l->p1);
	}
}

void
cut(Text *t, int w, int save, int check)
{
	long p0, p1;
	Flayer *l;

	l = &t->l[w];
	p0 = l->p0;
	p1 = l->p1;
	if(p0 == p1)
		return;
	if(p0 < 0)
		panic("cut");
	if(save)
		snarf(t, w);
	outTsll(Tcut, t->tag, p0, p1);
	flsetselect(l, p0, p0);
	t->lock++;
	hcut(t->tag, p0, p1-p0);
	if(check)
		hcheck(t->tag);
}

void
paste(Text *t, int w)
{
	if(snarflen){
		cut(t, w, 0, 0);
		t->lock++;
		outTsl(Tpaste, t->tag, t->l[w].p0);
	}
}

void
scrorigin(Flayer *l, int but, long p0)
{
	Text *t=(Text *)l->user1;

	switch(but){
	case 1:
		outTsll(Torigin, t->tag, l->origin, p0);
		break;
	case 2:
		outTsll(Torigin, t->tag, p0, 1L);
		break;
	case 3:
		horigin(t->tag,p0);
	}
}

int
alnum(int c)
{
	/*
	 * Hard to get absolutely right.  Use what we know about ASCII
	 * and assume anything above the Latin control characters is
	 * potentially an alphanumeric.
	 */
	if(c<=' ')
		return 0;
	if(0x7F<=c && c<=0xA0)
		return 0;
	if(utfrune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c))
		return 0;
	return 1;
}

int
raspc(Rasp *r, long p)
{
	ulong n;
	rload(r, p, p+1, &n);
	if(n)
		return scratch[0];
	return 0;
}

long
ctlw(Rasp *r, long o, long p)
{
	int c;

	if(--p < o)
		return o;
	if(raspc(r, p)=='\n')
		return p;
	for(; p>=o && !alnum(c=raspc(r, p)); --p)
		if(c=='\n')
			return p+1;
	for(; p>o && alnum(raspc(r, p-1)); --p)
		;
	return p>=o? p : o;
}

long
ctlu(Rasp *r, long o, long p)
{
	for(; p-1>=o && raspc(r, p-1)!='\n'; --p)
		;
	return p>=o? p : o;
}

int
center(Flayer *l, long a)
{
	Text *t;

	t = l->user1;
	if(!t->lock && (a<l->origin || l->origin+l->f.nchars<a)){
		if(a > t->rasp.nrunes)
			a = t->rasp.nrunes;
		outTsll(Torigin, t->tag, a, 2L);
		return 1;
	}
	return 0;
}

int
onethird(Flayer *l, long a)
{
	Text *t;
	Rectangle s;
	long lines;

	t = l->user1;
	if(!t->lock && (a<l->origin || l->origin+l->f.nchars<a)){
		if(a > t->rasp.nrunes)
			a = t->rasp.nrunes;
		s = inset(l->scroll, 1);
		lines = ((s.max.y-s.min.y)/l->f.font->height+1)/3;
		if (lines < 2)
			lines = 2;
		outTsll(Torigin, t->tag, a, lines);
		return 1;
	}
	return 0;
}

void
flushtyping(int clearesc)
{
	Text *t;
	ulong n;

	if(clearesc)
		typeesc = -1;	
	if(typestart == typeend) {
		modified = 0;
		return;
	}
	t = which->user1;
	if(t != &cmd)
		modified = 1;
	rload(&t->rasp, typestart, typeend, &n);
	scratch[n] = 0;
	if(t==&cmd && typeend==t->rasp.nrunes && scratch[typeend-typestart-1]=='\n'){
		setlock();
		outcmd();
	}
	outTslS(Ttype, t->tag, typestart, scratch);
	typestart = -1;
	typeend = -1;
}

#define	SCROLLKEY	Kdown
#define	BACKSCROLLKEY	Kup
#define	ESC		0x1B
#define CUT		0x18	/* ctrl-x */		
#define COPY		0x03	/* crtl-c */
#define PASTE		0x16	/* crtl-v */

void
type(Flayer *l, int res)	/* what a bloody mess this is */
{
	Text *t = (Text *)l->user1;
	Rune buf[100];
	Rune *p = buf;
	int c, backspacing;
	long a, a0;
	int scrollkey;

	scrollkey = 0;
	if(res == RKeyboard)
		scrollkey = qpeekc()==SCROLLKEY || qpeekc()==BACKSCROLLKEY || qpeekc()==COPY || qpeekc()==PASTE;	/* ICK */

	if(samlock || t->lock){
		kbdblock();
		return;
	}
	a = l->p0;
	if(a!=l->p1 && !scrollkey){
		flushtyping(1);
		cut(t, t->front, 1, 1);
		return;	/* it may now be locked */
	}
	backspacing = 0;
	while((c = kbdchar())>0){
		if(res == RKeyboard){
			if(c==SCROLLKEY || c==BACKSCROLLKEY || c==ESC)
				break;
			/* backspace, ctrl-u, ctrl-w, del */
			if(c=='\b' || c==0x15 || c==0x17 || c==0x7F){
				backspacing = 1;
				break;
			}
			if(c==CUT || c==PASTE || c==COPY)
				break;
		}
		*p++ = c;
		if(c == '\n' || p >= buf+sizeof(buf)/sizeof(buf[0]))
			break;
	}
	if(p > buf){
		if(typestart < 0)
			typestart = a;
		if(typeesc < 0)
			typeesc = a;
		hgrow(t->tag, a, p-buf, 0);
		t->lock++;	/* pretend we Trequest'ed for hdatarune*/
		hdatarune(t->tag, a, buf, p-buf);
		a += p-buf;
		l->p0 = a;
		l->p1 = a;
		typeend = a;
		if(c=='\n' || typeend-typestart>100)
			flushtyping(0);
		onethird(l, a);
	}
	if(c == SCROLLKEY){
		flushtyping(0);
		center(l, l->origin+l->f.nchars+1);
		/* backspacing immediately after outcmd(): sorry */
	}else if(c == BACKSCROLLKEY){
		flushtyping(0);
		a0 = l->origin-l->f.nchars;
		if(a0 < 0)
			a0 = 0;
		center(l, a0);
	}else if(c == CUT){
		flushtyping(1);
		cut(t, which-t->l, 1, 1);
	}else if(c == COPY){
		flushtyping(1);
		snarf(t, which-t->l);
	}else if(c == PASTE){
		flushtyping(1);
		paste(t, which-t->l);
	}else if(backspacing && !samlock){
		if(l->f.p0>0 && a>0){
			switch(c){
			case '\b':
			case 0x7F:	/* del */
				l->p0 = a-1;
				break;
			case 0x15:	/* ctrl-u */
				l->p0 = ctlu(&t->rasp, l->origin, a);
				break;
			case 0x17:	/* ctrl-w */
				l->p0 = ctlw(&t->rasp, l->origin, a);
				break;
			}
			l->p1 = a;
			if(l->p1 != l->p0){
				/* cut locally if possible */
				if(typestart<=l->p0 && l->p1<=typeend){
					t->lock++;	/* to call hcut */
					hcut(t->tag, l->p0, l->p1-l->p0);
					/* hcheck is local because we know rasp is contiguous */
					hcheck(t->tag);
				}else{
					flushtyping(0);
					cut(t, t->front, 0, 1);
				}
			}
			if(typeesc >= l->p0)
				typeesc = l->p0;
			if(typestart >= 0){
				if(typestart >= l->p0)
					typestart = l->p0;
				typeend = l->p0;
				if(typestart == typeend){
					typestart = -1;
					typeend = -1;
					modified = 0;
				}
			}
		}
	}else{
		if(c==ESC && typeesc>=0){
			l->p0 = typeesc;
			l->p1 = a;
			flushtyping(1);
		}
		for(l=t->l; l<&t->l[NL]; l++)
			if(l->textfn)
				flsetselect(l, l->p0, l->p1);
	}
}


void
outcmd(void){
	if(work)
		outTsll(Tworkfile, ((Text *)work->user1)->tag, work->p0, work->p1);
}

void
panic(char *s)
{
	char buf[200];

	sprint(buf, "samterm:panic: %s: %r", s);
	fprint(2, "%s\n", buf);
	exits(buf);
}

Rune*
gettext(Flayer *l, long n, ulong *np)
{
	Text *t;

	t = l->user1;
	rload(&t->rasp, l->origin, l->origin+n, np);
	return scratch;
}

long
scrtotal(Flayer *l)
{
	return ((Text *)l->user1)->rasp.nrunes;
}

void*
alloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		panic("alloc");
	memset(p, 0, n);
	return p;
}