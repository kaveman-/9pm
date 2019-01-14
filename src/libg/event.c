#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

typedef struct	Slave		Slave;
typedef struct	Ebuf		Ebuf;

struct Slave{
	Qlock	lk;
	int	pid;
	int	fd;
	int	maxqueue;	/* zero mean one shot */
	int	nqueue;
	Event	ready;
	int	n;		/* size of Ebuf */
	Ebuf	*head;		/* queue of messages for this descriptor */
	Ebuf	*tail;
};

struct Ebuf{
	Ebuf	*next;
	int	n;		/* number of bytes in buf */
	uchar	buf[1];
};

static	Slave	eslave[MAXSLAVE];
static	int	Skeyboard = -1;
static	int	Smouse = -1;
static	int	Stimer = -1;
static	int	logfid;

extern	void	_reshape(void);

extern	int	_eventfd;	/* events from 9pm */

static	int	nslave;
static	Event	econsume;

static struct {
	Qlock	lk;
	Rectangle r;
	int	reshape;
	int	refresh;
} er;

static	int	eforkslave(ulong, void (*f)(void*), int, int, int);
static	void	extract(void);
static	Ebuf	*ebread(Slave *s);
static	void	efileslave(void *a);
static	void	etimerslave(void *a);
static	void	econv(void *a);
static	void	equeue(Slave *s, uchar *buf, int n);
static	Ebuf	*edequeue(Slave *s);

void
einit(ulong keys)
{
	if(keys&Ekeyboard){
		for(Skeyboard=0; Ekeyboard & ~(1<<Skeyboard); Skeyboard++)
			;
		eforkslave(Ekeyboard, 0, -1, 0, 100);
	}
	if(keys&Emouse){
		for(Smouse=0; Emouse & ~(1<<Smouse); Smouse++)
			;
		eforkslave(Emouse, 0, -1, 0, 0);
	}
	thread(econv, 0);
}


int
ecanread(ulong keys)
{
	int i;
	int reshape;
	int refresh;
	Rectangle r;

	bflush();

	if(er.refresh || er.reshape) {
		qlock(&er.lk);
		refresh = er.refresh;
		r = er.r;
		reshape = er.reshape;
		er.refresh = 0;
		er.reshape = 0;
		qunlock(&er.lk);

		if(refresh) {
/* bshade(er.r); /**/
			bshow(&screen, r);
		}

		if(reshape) {
			_reshape();
			ereshaped(screen.r);
			bflush();
		}
	}

	for(i=0; i<nslave; i++) {
		if((keys & (1<<i)) && eslave[i].head)
			return i+1;
	}

	return 0;
}

int
ecanmouse(void)
{
	return ecanread(Emouse);
}

int
ecankbd(void)
{
	return ecanread(Ekeyboard);
}

ulong
etimer(ulong key, int n)
{
	if(Stimer != -1)
		berror("events: timer started twice");
	Stimer = eforkslave(key, etimerslave, 0, n, 0);
	return 1<<Stimer;

}

ulong
estart(ulong key, int fd, int n)
{
	int i;

	if(fd < 0)
		berror("events: bad file descriptor");
	if(n <= 0 || n > EMAXMSG)
		n = EMAXMSG;
	i = eforkslave(key, efileslave, fd, n, 1);
	return 1<<i;
}

ulong
event(GEvent *e)
{
	return eread(~0UL, e);
}

ulong
eread(ulong keys, GEvent *e)
{
	Ebuf *eb;
	int i;

	if(keys == 0)
		return 0;
	for(;;) {
		i = ecanread(keys);
		if(i)
			break;
		ewait(&econsume);
	}
	i--;
	eb = edequeue(eslave + i);
	if(i == Smouse)
		e->mouse = *(Mouse*)eb->buf;
	else if(i == Skeyboard)
		e->kbdc = *(Rune*)eb->buf;
	else {
		e->n = eb->n;
		memmove(e->data, eb->buf, e->n);
	}
	free(eb);
		
	return 1<<i;
}

Mouse
emouse(void)
{
	GEvent e;

	eread(Emouse, &e);
	return e.mouse;
}

int
ekbd(void)
{
	GEvent e;

	eread(Ekeyboard, &e);
	return e.kbdc;
}

static int
eforkslave(ulong key, void (*f)(void *), int fd, int n, int maxqueue)
{
	int i;

	for(i=0; i<MAXSLAVE; i++)
		if((key & ~(1<<i)) == 0 && eslave[i].pid == 0){
			if(nslave <= i)
				nslave = i + 1;			
			eslave[i].fd = fd;
			eslave[i].n = n;
			eslave[i].maxqueue = maxqueue;
			eslave[i].nqueue = 0;
			eslave[i].head = eslave[i].tail = 0;
			if(f)
				eslave[i].pid = thread(f, eslave+i);
			else
				eslave[i].pid = -1;
			return i;
		}
	berror("events: bad slave assignment");
	return 0;
}

static void
efileslave(void *a)
{
	Slave *s = a;
	char buf[EMAXMSG];
	int n;

	for(;;) {
		n = read(s->fd, buf, s->n);
		if(n <= 0)
			break;
		
		equeue(s, buf, n);	
	}
	exits(0);
}


static void
etimerslave(void *a)
{
	uchar buf;
	Slave *s = a;

	if(s->n <= 0)
		s->n = 1000;
	for(;;) {
		sleep(s->n);
		equeue(s, &buf, 0);
	}
}


static void
econv(void *a)
{
	uchar buf[EMAXMSG], *p;
	int n, n2;
	Mouse m;
	Rectangle rect;
	int wid;
	Rune r;
	
	/* convert 9pm events in mouse and keyboard events */
	p = buf;
	n = 0;
	for(;;) {
Top:
		memcpy(buf, p, n);
		n2 = read(_eventfd, buf+n, sizeof(buf)-n);
		if(n2 <= 0)
			break;
		n += n2;
		p = buf;
		while(n>0) {
			switch(p[0]) {
			default:
				fatal("econv: unknown 9pm event: %c\n", p[0]);
			case 'k':
				/*
				 * 'k'		1
				 * Rune		2
				 */
				if(n < 3)
					goto Top;
				r = BGSHORT(p+1);
//fprint(2, "k: %x\n", r);
				if(Skeyboard >= 0)
					equeue(eslave+Skeyboard, (uchar*)&r, sizeof(r));
				n -= 3;
				p += 3;
				break;
			case 'm':
				/*
				 * 'r'		1
				 * wid		4
				 * Point	8
				 * int		4
				 * int		4
				 */
				if(n < 21)
					goto Top;
				wid = BGLONG(p+1);
				m.xy.x = BGLONG(p+5);
				m.xy.y = BGLONG(p+9);
				m.buttons = BGLONG(p+13);
				m.msec = BGLONG(p+17);
//fprint(2, "m: wid %d pt %P b %d msec =%d\n", wid, m.xy, m.buttons, m.msec);
				n -= 21;
				if(Smouse >= 0)
					equeue(eslave+Smouse, (uchar*)&m, sizeof(m));
				p += 21;
				break;
			case 'r':
				/*
				 * 'r'		1
				 * wid		4
				 * Rect		16
				 */
				if(n < 21)
					goto Top;
				wid = BGLONG(p+1);
				rect.min.x = BGLONG(p+5);
				rect.min.y = BGLONG(p+9);
				rect.max.x = BGLONG(p+13);
				rect.max.y = BGLONG(p+17);
//fprint(2, "r: %d %R\n", wid, rect);
				qlock(&er.lk);
				if(er.refresh == 0) {
					er.refresh = 1;
					er.r = rect;
				} else {
					if(rect.min.x < er.r.min.x)
						er.r.min.x = rect.min.x;
					if(rect.min.y < er.r.min.y)
						er.r.min.y = rect.min.y;
					if(rect.max.x > er.r.max.x)
						er.r.max.x = rect.max.x;
					if(rect.max.y > er.r.max.y)
						er.r.max.y = rect.max.y;
				}
				if(!rectinrect(er.r, screen.r))
					er.refresh = 0;
				qunlock(&er.lk);
				esignal(&econsume);
				n -= 21;
				p += 21;
				break;
			case 't':
				/*
				 * 't'		1
				 * wid		4
				 */
				if(n < 5)
					goto Top;
				wid = BGLONG(p+1);
//fprint(2, "t: %d\n", wid);
				qlock(&er.lk);
				er.refresh = 0;
				er.reshape = 1;
				qunlock(&er.lk);
				esignal(&econsume);
				n -= 5;
				p += 5;
				break;
			}
		}
	}
}

static void
equeue(Slave *s, uchar *buf, int n)
{
	Ebuf *eb;

	eb = malloc(sizeof(*eb) - 1 + n);
	eb->n = n;
	memmove(eb->buf, buf, n);
	eb->next = 0;

	qlock(&s->lk);
	while(s->maxqueue && s->nqueue >= s->maxqueue) {
		qunlock(&s->lk);
		ewait(&s->ready);
		qlock(&s->lk);
	}

	if(s->head) {
		if(s->maxqueue == 0) {
			free(s->head);
			s->head = s->tail = eb;
			s->nqueue = 0;
		} else {
			s->tail = s->tail->next = eb;
		}
	} else
		s->head = s->tail = eb;
	s->nqueue++;
	qunlock(&s->lk);

	esignal(&econsume);
}

static Ebuf *
edequeue(Slave *s)
{
	Ebuf *eb;

	qlock(&s->lk);

	eb = s->head;
	s->head = s->head->next;
	if(s->head == 0)
		s->tail = 0;
	s->nqueue--;
	assert(s->nqueue >= 0);
	qunlock(&s->lk);

	if(s->nqueue+1 == s->maxqueue)
		esignal(&s->ready);

	return eb;
}

