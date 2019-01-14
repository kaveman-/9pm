#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

#define	BI2WD		(8*sizeof(ulong))
#define	HDR		sizeof(Header)

static	void		compact(void);

typedef struct Arena
{
	ulong	*words;		/* storage */
	ulong	*wfree;		/* pointer to next free word */
	ulong		nwords;		/* total in arena */
	int		nbusy;		/* number of busy blocks */
} Arena;

typedef struct Header
{
	ulong	nw;
	Arena	*a;
	Bitmap	*i;
	char		slop[Tilehdr];	/* room to drop tile for transmitting data */
} Header;

typedef struct Ialloc
{
	Arena	**arena;		/* array */
	int		narena;		/* number allocated */
} Ialloc;

static Ialloc ialloc;

Bitmap*	 	brealloc(Bitmap*, Rectangle, int);
void	 	bfreemem(Bitmap*);

Bitmap*
balloc(Rectangle r, int ld)
{
	return brealloc(0, r, ld);
}

Bitmap*
brealloc(Bitmap *i, Rectangle r, int ld)
{
	char *err;
	ulong t, l, ws, nw;
	int j, freeit;
	Arena *a, **aa;

	freeit = (i==0);
	err = 0;

	if(ld<0 || ld>3){
		err = "invalid ldepth";
    Error:
		if(err)
			werrstr("balloc: %s", err);
		else
			werrstr("balloc: %r");
		if(freeit)
			free(i);
		return 0;
	}

	ws = BI2WD>>ld;	/* pixels per word */
	if(r.min.x >= 0){
		l = (r.max.x+ws-1)/ws;
		l -= r.min.x/ws;
	}else{			/* make positive before divide */
		t = (-r.min.x)+ws-1;
		t = (t/ws)*ws;
		l = (t+r.max.x+ws-1)/ws;
	}
	nw = l*Dy(r);
	/* first try easy fit */
	for(j=0; j<ialloc.narena; j++){
		a = ialloc.arena[j];
		if(a->wfree+HDR+nw <= a->words+a->nwords)
			goto Found;
	}

	/* compact and try again */
	compact(); 
	for(j=0; j<ialloc.narena; j++){
		a = ialloc.arena[j];
		if(a->wfree+HDR+nw <= a->words+a->nwords)
			goto Found;
	}

	/* need new arena */
	a = mallocz(sizeof(Arena));

	/*
	 * Allocate at least 0.5 megabyte.  Use
	 * the non-clearing allocator so we won't page in memory we don't need.
	 * For long-running active programs we'll eventually touch all the arena
	 * anyway, but for little quiet ones this reduces the memory overhead.
	 */
	t = (512*1024)/sizeof(ulong);
	if(t < HDR+nw)
		t = HDR+nw;
	a->nwords = t;
	a->words = malloc(t*sizeof(ulong));
	aa = malloc((ialloc.narena+1)*sizeof(Arena*));
	if(a->words==0 || aa==0){
		free(a);
		free(aa);
		goto Error;
	}
	memmove(aa, ialloc.arena, ialloc.narena*sizeof(Arena*));
	free(ialloc.arena);
	aa[ialloc.narena++] = a;
	ialloc.arena = aa;
	a->wfree = a->words;
	a->nbusy = 0;
/*	goto Found; */

	/* note: can free up fonts here one day maybe */
	
    Found:
	if(i == 0)
		i = mallocz(sizeof(Bitmap));
	((Header*)a->wfree)->nw = nw;
	((Header*)a->wfree)->a = a;
	((Header*)a->wfree)->i = i;
	a->wfree += HDR;
	memset(a->wfree, 0, nw*sizeof(ulong));
	i->base = a->wfree;
	a->wfree += nw;
	a->nbusy++;
	i->zero = l*r.min.y;
	if(r.min.x >= 0)
		i->zero += r.min.x/ws;
	else
		i->zero -= (-r.min.x+ws-1)/ws;
	i->zero = -i->zero;
	i->width = l;
	i->ldepth = ld;
	i->r = r;
	i->clipr = r;
	i->bb = r;
	return i;
}

static void
arenafree(Arena *a)
{
	int j;
	ulong nb;

	for(j=0; j<ialloc.narena; j++)
		if(ialloc.arena[j] == a){
			nb = a->nwords*sizeof(ulong);
			if(nb >= 2*8192)
				szero(a->words, nb);
			free(a->words);
			free(a);
			ialloc.narena--;
			memmove(ialloc.arena+j, ialloc.arena+j+1, (ialloc.narena-j)*sizeof(Arena*));
			return;
		}
	fprint(2, "arenafree: can't find arena");
	abort();
}

void
bfreemem(Bitmap *i)
{
	Arena *a;
	Header *h;

	h = (Header*)(&i->base[-(int)HDR]);
	h->i = 0;
	i->base = 0;
	a = h->a;
	/* back up free pointer if this is last in arena (very likely) */
	if(a->wfree == (ulong*)h+h->nw+HDR)
		a->wfree = (ulong*)h;
	a->nbusy--;
	if(a->nbusy == 0)
		arenafree(a);
}

void
bfree(Bitmap *i)
{
	if(i == 0)
		return;
	bfreemem(i);
	/* always compact when window is freed or image is large */
	if(Dx(i->r)>100 && Dy(i->r)>100)
		compact();
	free(i);
}

static void
compact(void)
{
	Arena *a, *na;
	ulong *p1, *p2;
	ulong n;
	int j, k;


	for(j=0; j<ialloc.narena; j++){
		a = ialloc.arena[j];
		/* first compact what's here */
		p1 = p2 = a->words;
		while(p2 < a->wfree){
			n = HDR+((Header*)p2)->nw;
			if(((Header*)p2)->i == 0){
				p2 += n;
				continue;
			}
			if(p1 != p2){
				memmove(p1, p2, n*sizeof(ulong));
				((Header*)p1)->i->base = p1+HDR;
			}
			p2 += n;
			p1 += n;
		}
		/* now pull stuff from later arena to fill this one */
		for(k=j+1; k<ialloc.narena && p1<a->words+a->nwords; k++){
			na = ialloc.arena[k];
			p2 = na->words;
			while(p2 < na->wfree){
				n = HDR+((Header*)p2)->nw;
				if(((Header*)p2)->i == 0){
					p2 += n;
					continue;
				}
				if(p1+n < a->words+a->nwords){
					memmove(p1, p2, n*sizeof(ulong));
					((Header*)p1)->i->base = p1+HDR;
					/* block now in new arena... */
					((Header*)p1)->a = a;
					a->nbusy++;
					/* ... not in old arena */
					na->nbusy--;
					((Header*)p2)->i = 0;
					p1 += n;
				}
				p2 += n;
			}
		}
		a->wfree = p1;
	}
	for(j=0; j<ialloc.narena; j++){
		a = ialloc.arena[j];
		if(a->nbusy == 0){
			arenafree(a);
			--j;
		}
	}
}
