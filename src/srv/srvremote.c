#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

#define	SDB	if(0) fprint(2, 
#define EDB	);

typedef struct Rpc	Rpc;
typedef struct Call	Call;
typedef struct Proxy	Proxy;
typedef struct Map	Map;
typedef struct Client	Client;

enum {
	Header	= 16,	
	Extra	= 100,
	Nhash	= (1<<7),	/* must be a power of two */
};

/* message types */
enum {
	Tnop,
	Rnop,
	Terror,		/* illegal */
	Rerror,
	Tattach,
	Rattach,
	Tdetach,
	Rdetach,
	Topen,
	Ropen,
	Tclose,
	Rclose,
	Tread,
	Rread,
	Twrite,
	Rwrite,
	Trpc,
	Rrpc,
};

struct Call
{
	char	op;
	short	tag;
	int	id;
	union {
		char	error[ERRLEN];
		struct {
			int	nread;
			int	nwrite;
			uchar	*data;
		};
		struct {
			char	path[Spathlen];
			char	type[Snamelen];
		};
		Sinfo	info;
	};
};

struct Rpc {
	int	tag;
	Call	*call;
	int	done;
	Event	e;
	Rpc	*next;
};

struct Client {
	Qlock		lk;
	int		linkid;	
	int		linksize;
	int		dead;
	Qlock		wlk;		/* write lock */
	int		bufsize;	/* for 9pm message reads */
	uchar		nbuf;		/* number of character in the buffer */
	uchar		*buf;
	int		ntag;		/* next tag to alloc */
	Rpc		*reader;	/* current reader */
	Rpc		*rpc;		/* list of outstanding rpcs */
	Rpc		*free;
};


struct Proxy {
	Qlock		lk;
	int		ref;
	Map		*map[Nhash];

	int		exit;

	Qlock		wlk;	/* write lock */

	int		linkid;
	int		linksize;

	/* read buffer */
	uchar		buf[Smesgsize+Extra];
	int		nbuf;		/* amount of data in the buffer */
	uchar		*bufp;		/* read point in the buffer */

	int		nwait;
	int		reading;
	Event		eread;

	int		mesgsize;
};
	
struct Map {
	int	rid;		/* remote id */
	int	lid;		/* local id */
	Map	*next;		/* for hash */
};

static int	crpc(Client*, Call*);
static void	cwrite(Client*, Call*);
static void	cerror(Client*);
static void	cread(Client*);
static void	cmux(Client*, Rpc*);
static void	cfree(Client*);

static void	proxythread(Proxy*);
static void	proxyerror(Proxy*);
static void	proxyexit(Proxy*);
static int	proxyread(Proxy*, Call*, uchar*, int);
static void	proxyexec(Proxy*, Call*, int);
static void	proxyreply(Proxy*, Call*, char*);
static void	proxydetach(Proxy*, Call*);

static int	pkcall(uchar*, Call*);
static int	unpkcall(Call*, uchar*, int);
static int	callcheck(Call*, Call*);
static void	callcpy(Call*, Call*);
static int	callconv(va_list*, Fconv*);
static void	dumpsome(char*, char*, long);

static Client	*sinit(int*, Sinfo*);
static int	sfini(Client*);
static void	*sopen(Client*, int, char*, char*, Sinfo*);
static int	sclose(Client*, void*);
static int	sfree(Client*, void*);
static int	sread(Client*, void*, uchar*, int);
static int	swrite(Client*, void*, uchar*, int);
static int	srpc(Client*, void*, uchar*, int, int);

Stype	srvremote = {
	sinit,
	sfini,
	sopen,
	sclose,
	sfree,
	sread,
	swrite,
	srpc,
	0,
};

void
srvproxy(int id)
{
	Sinfo info;
	Proxy *p;

	if(!srvinfo(id, &info) || strcmp(info.type, "link") != 0)
		fatal("sproxy: bad server connection");
	
	p = mallocz(sizeof(Proxy));
	p->linkid = id;
	p->linksize = info.mesgsize;
	p->ref = 1;
	p->nwait = 1;

	thread(proxythread, p);
}

static Client *
sinit(int *a, Sinfo *info)
{
	Sinfo linkinfo;
	int linkid;
	Client *c;
	Call call;

	linkid = *a;

	if(srvinfo(linkid, &linkinfo) < 0 || strcmp(linkinfo.type, "link") != 0) {
		werrstr("remote: bad link connection");
		return 0;
	}

	c = mallocz(sizeof(Client));
	c->linkid = linkid;
	c->linksize = linkinfo.mesgsize;
	if(c->linksize == 0)
		c->linksize = Extra+Smesgsize;

	c->bufsize = Smesgsize+Extra;
	c->buf = malloc(c->bufsize);

	fmtinstall('C', callconv);

	call.op = Tattach;
	call.id = 0;
	if(!crpc(c, &call)) {
		cfree(c);
		return 0;
	}
	memcpy(info, &call.info, sizeof(Sinfo));

	return c;
}

static int
sfini(Client *c)
{
	Call call;

	SDB "remote: fini linkid = %d\n", c->linkid EDB

	/* should be no outstanding messages */
	assert(c->rpc == 0);
	call.op = Tdetach;
	call.id = 0;
	crpc(c, &call);

	cfree(c);
	return 1;
}

static void *
sopen(Client *c, int id, char *path, char *type, Sinfo *info)
{
	Call call;

	if(strlen(path) >= Spathlen || strlen(type) >= Snamelen) {
		werrstr("remote: open: invalid path or type");
		return 0;
	}

	call.op = Topen;
	call.id = id;
	strcpy(call.path, path);
	strcpy(call.type, type);
	if(!crpc(c, &call))
		return 0;
	memcpy(info, &call.info, sizeof(Sinfo));
	/* make mesgsize = link size-header, but not less than Smesgsize */
	if(info->mesgsize > c->linksize-Header)
		info->mesgsize = c->linksize-Header;
	if(info->mesgsize < Smesgsize)
		info->mesgsize = Smesgsize;	
	return (void*)id;
}

static int
sclose(Client *c, void *id)
{
	Call call;

	call.op = Tclose;
	call.id = (int)id;

	return crpc(c, &call);
}

static int
sfree(Client *c, void *id)
{
	return 1;
}

static int
sread(Client *c, void *id, uchar *buf, int n)
{
	Call call;

	call.op = Tread;
	call.id = (int)id;
	call.data = buf;
	call.nread = n;

	if(!crpc(c, &call))
		return -1;

	return call.nread;
}

static int
swrite(Client *c, void *id, uchar *buf, int n)
{
	Call call;

	call.op = Twrite;
	call.id = (int)id;
	call.data = buf;
	call.nwrite = n;

	if(!crpc(c, &call))
		return -1;

	return call.nwrite;
}

static int
srpc(Client *c, void *id, uchar *buf, int nw, int nr)
{
	Call call;

	call.op = Trpc;
	call.id = (int)id;
	call.data = buf;
	call.nwrite = nw;
	call.nread = nr;

	if(!crpc(c, &call))
		return -1;

	return call.nread;
}

static int
crpc(Client *c, Call *call)
{
	Rpc *rpc;

	qlock(&c->lk);
	if(c->dead) {
		qunlock(&c->lk);
		werrstr("remote: connection is dead");
		return 0;
	}

	if(c->free == 0) {
		rpc = mallocz(sizeof(Rpc));
		rpc->tag = c->ntag++;
	} else {
		rpc = c->free;
		c->free = rpc->next;
	}

	call->tag = rpc->tag;
	rpc->call = call;
	rpc->next = c->rpc;
	c->rpc = rpc;
	rpc->done = 0;
	qunlock(&c->lk);

	cwrite(c, call);

	qlock(&c->lk);

	while(!rpc->done && c->reader != 0) {
		qunlock(&c->lk);
		ewait(&rpc->e);
		qlock(&c->lk);
	}

	if(c->reader == 0) {
		c->reader = rpc;
		do {
			qunlock(&c->lk);
			cread(c);
			qlock(&c->lk);

			cmux(c, rpc);
		} while(!rpc->done);
		c->reader = 0;
		if(c->rpc)
			esignal(&c->rpc->e);
	}

assert(c->reader != rpc);

	/* already taken out of client->rpc */
	assert(rpc->next == 0);

	/* put on the free list */
	rpc->next = c->free;
	c->free = rpc;
	rpc->call = 0;
	qunlock(&c->lk);
	if(call->op == Rerror)
		werrstr("%s", call->error);
	return call->op != Rerror;
}

/* this could be async */
static void
cwrite(Client *c, Call *call)
{
	uchar buf[Smesgsize+Extra];
	int n, n2;

	n = pkcall(buf, call);
	assert(n < Smesgsize+Extra);

	qlock(&c->wlk);
	SDB "cwrite: %d %C\n", c->linkid, call EDB
	n2 = srvwrite(c->linkid, buf, n);
	if(n2 != n)
		cerror(c);
	qunlock(&c->wlk);
}

static void
cread(Client *c)
{
	int n;

	n = c->bufsize - c->nbuf;
	if(n > c->linksize)
		n = c->linksize;
	if(n == 0) {
		werrstr("reply buffer full");
		cerror(c);
		return;
	}
	n = srvread(c->linkid, c->buf+c->nbuf, n);
	if(n <= 0) {
		cerror(c);
		return;
	}
	c->nbuf += n;
}

static void
cmux(Client *c, Rpc *r)
{	
	uchar *p;
	int n, n2;
	Rpc *q, *oq;
	Call reply;

assert(holdqlock(&c->lk));

	p = c->buf;
	n = c->nbuf;
	for(;;) {
		n2 = unpkcall(&reply, p, n);
		if(n2 < 0)
			goto Bad;
		if(n2 == 0)
			break;

		SDB "cmux: n = %d n2 = %d %d %C\n", n, n2, c->linkid, &reply EDB
		n -= n2;
		p += n2;

		for(q=c->rpc,oq=0; q; oq=q,q=q->next) {
			if(q->tag == reply.tag)
				break;
		}

		if(q == 0) {
			werrstr("reply for unknown tag: %d", reply.tag);
			goto Bad;
		}
		if(!callcheck(q->call, &reply))
			goto Bad;
			
		if(oq == 0)
			c->rpc = q->next;
		else
			oq->next = q->next;
		callcpy(q->call, &reply);
		q->done = 1;
		q->next = 0;
		if(q != r)
			esignal(&q->e);
	}

	/* move the last part of the message down in the buffer */
	if(p != c->buf) {
		memmove(c->buf, p, n);
		c->nbuf = n;
	}

	return;

Bad:
	qunlock(&c->lk);
	cerror(c);
	qlock(&c->lk);
}
		

static void
cerror(Client *c)
{
	Rpc *r, *nr;
	char err[ERRLEN];
	
	qlock(&c->lk);

	err[0] = 0;
	errstr(err);
fprint(2, "remote error: %d: %s\n", c->linkid, err);
	SDB "remote error: %d: %s\n", c->linkid, err EDB

	c->dead = 1;
	for(r=c->rpc; r; r=nr) {
		nr = r->next;
		r->call->op = Terror;
		memcpy(r->call->error, err, ERRLEN);
		r->done = 1;
		epoison(&r->e);
		r->next = 0;
	}
	c->rpc = 0;

	qunlock(&c->lk);

	/* try and wakeup threads doing io on the link */
	srvclose(c->linkid);

}

void
cfree(Client *c)
{
	Rpc *r;

	for(r=c->free; r; r=r->next)
		free(r);
	srvclose(c->linkid);
	free(c);
}

void
proxythread(Proxy *p)
{
	Call call;
	uchar buf[Smesgsize+Extra];
	int lid;
	uint h;
	Map *m;

	qlock(&p->lk);
	p->nwait--;
	qunlock(&p->lk);

	for(;;) {
		qlock(&p->lk);
		while(p->reading) {
			p->nwait++;
			qunlock(&p->lk);
			if(ewait(&p->eread) < 0)
				proxyexit(p);	/* does not return */
			qlock(&p->lk);
			p->nwait--;
		}
		p->reading = 1;
		qunlock(&p->lk);

		call.data = buf;
		if(!proxyread(p, &call, buf, sizeof(buf)))
			proxyerror(p);	/* does not return */

		qlock(&p->lk);
		lid = -1;
		h = call.id & (Nhash-1);
		for(m=p->map[h]; m; m=m->next) {
			if(m->rid == call.id) {
				lid = m->lid;
				break;
			}
		}

		/* make sure there is some else to read */
		if(p->nwait == 0) {
			p->ref++;
			p->nwait++;	/* inc here to indicate a thread is on the way */
			thread(proxythread, p);
		}
		p->reading = 0;
		esignal(&p->eread);

		qunlock(&p->lk);

		proxyexec(p, &call, lid);
	}
}

int
proxyread(Proxy *p, Call *call, uchar *buf, int bufsize)
{
	int n;
	Call call2;
	uchar *q;

	q = buf;
	if(p->nbuf) {
		/* use data in proxy buffer */
		n = unpkcall(&call2, p->bufp, p->nbuf);
		if(n < 0)
			return 0;
		if(n > 0) {
			callcpy(call, &call2);
			p->bufp += n;
			p->nbuf -= n;
			return 1;
		}
		memmove(q, p->bufp, p->nbuf);
		q += p->nbuf;
		p->nbuf = 0;
	}

	
	for(;;) {
		n = bufsize - (q-buf);
		if(p->linksize && n > p->linksize)
			n = p->linksize;
		if(n == 0) {
			werrstr("message too large");
			return 0;
		}
		n = srvread(p->linkid, q, n);
		if(p->exit)
			proxyexit(p);
		if(n <= 0)
			return 0;
		q += n;
		
		n = unpkcall(call, buf, q-buf);
		if(n < 0)
			return 0;
		if(n == 0) {
			SDB "proxyread: only have %d\n", q-buf EDB
			continue;
		}
		
		/* move left over into proxy buffer */
		if(buf+n < q) {
			memmove(p->buf, buf+n, q-(buf+n));	
			p->bufp = p->buf;
			p->nbuf = q-(buf+n);
		}
		return 1;
	}
	return 0;	/* never reach here */
}

void
proxyexec(Proxy *p, Call *call, int lid)
{
	char err[ERRLEN];
	Map *m, *om;
	uint h;

	SDB "proxyexec: lid = %d %C\n", lid, call EDB

	err[0] = 0;
	switch(call->op) {
	default:
		proxyreply(p, call, "bad proxy request");
		return;
	case Tnop:
		break;
	case Tattach:
		strcpy(call->info.name, "/");
		strcpy(call->info.type, "dir");
		/* really should open "/" */
		srvgenid(call->info.id, p);
		call->info.mesgsize = 0;
		break;
	case Tdetach:
		proxydetach(p, call);
		return;
	case Topen:
		if(lid != -1) {
			proxyreply(p, call, "id already in use");
			return;
		}
		lid = srvopen(call->path, call->type);
		if(lid < 0 || !srvinfo(lid, &call->info)) {
			errstr(err);
			proxyreply(p, call, err);
			return;
		}
		
		m = mallocz(sizeof(Map));
		m->lid = lid;
		m->rid = call->id;
		h = call->id & (Nhash-1);
		qlock(&p->lk);
		if(p->exit) {
			qunlock(&p->lk);
			srvclose(lid);
			free(m);
			proxyreply(p, call, "proxy exited");
			return;
		}	
		m->next = p->map[h];
		p->map[h] = m;
		qunlock(&p->lk);
		break;
	case Tclose:
		if(!srvclose(lid)) {
			errstr(err);
			proxyreply(p, call, err);
			return;
		}
		h = call->id & (Nhash-1);
		qlock(&p->lk);
		for(m=p->map[h],om=0; m; om=m,m=m->next)
			if(m->rid == call->id)
				break;
		if(m) {
			if(om == 0)
				p->map[h] = m->next;
			else
				om->next = m->next;
		}
		qunlock(&p->lk);
		break;
	case Tread:
		/* check read size */
		call->nread = srvread(lid, call->data, call->nread);
		if(call->nread < 0) {
			errstr(err);
			proxyreply(p, call, err);
			return;
		}
		break;
	case Twrite:
		/* check write size */
		call->nwrite = srvwrite(lid, call->data, call->nwrite);
		if(call->nwrite < 0) {
			errstr(err);
			proxyreply(p, call, err);
			return;
		}
		break;
	case Trpc:
		/* check read/write size */
		call->nread = srvrpc(lid, call->data, call->nwrite, call->nread);
		if(call->nread < 0) {
			errstr(err);
			proxyreply(p, call, err);
			return;
		}
		break;
	}
	proxyreply(p, call, 0);	
}

void
proxydetach(Proxy *p, Call *c)
{
	int bad;	
	int i;
	Map *m;

	bad = 0;
	qlock(&p->lk);
	for(i=0; i<Nhash; i++) {
		if(p->map[i] == 0)
			continue;
		bad = 1;
		for(m=p->map[i]; m; m=m->next)
			SDB "proxydetach: id %d: not closed", m->rid EDB
	}
	qunlock(&p->lk);

	if(bad) {
		proxyreply(p, c, "servers still open");
		proxyerror(p);	/* does not return */
	}

	proxyreply(p, c, 0);

	/* kick proxythread out of proxyread */
	p->exit = 1;
	srvclose(p->linkid);

	SDB "proxy: linkid = %d: clean close\n", p->linkid EDB
/*	epoison(&p->eread); */
	proxyexit(p);	/* does not return */
}

void
proxyreply(Proxy *p, Call *c, char *err)
{
	uchar buf[Smesgsize+Extra];
	int n, n2;

	if(err == 0)
		c->op++;
	else {
		c->op = Rerror;
		memmove(c->error, err, ERRLEN);
	}
	n = pkcall(buf, c);
	
	SDB "proxyreply: n = %d %C\n", n, c EDB
	
	qlock(&p->wlk);
	n2 = srvwrite(p->linkid, buf, n);
	if(n2 != n) {
		qunlock(&p->wlk);
		proxyerror(p);	/* does not return */
	}
	qunlock(&p->wlk);
}


void
proxyerror(Proxy *p)
{
	Map *map[Nhash], *m, *nm;
	int i;

	SDB "proxy error: linkid = %d %r\n", p->linkid EDB

	epoison(&p->eread);

	qlock(&p->lk);
	p->exit = 1;
	memcpy(map, p->map, sizeof(map));
	memset(p->map, 0, sizeof(p->map));
	qunlock(&p->lk);

	/* close all the map channels */
	for(i=0; i<Nhash; i++) {
		for(m=map[i]; m; m=nm) {
			nm = m->next;
			srvclose(m->lid);
			free(m);
		}
	}
	proxyexit(p);
}

void
proxyexit(Proxy *p)
{
	qlock(&p->lk);
	if(p->ref > 1) {
		p->ref--;
		qunlock(&p->lk);
		threadexit();
	}

	/* last oneturns off the lights */
	srvclose(p->linkid);
	free(p);
	threadexit();
}


static int
pkcall(uchar *p, Call *c)
{
	p[0] = 0x96;	/* magic number */
	p[1] = c->op;
	PKUSHORT(p+2, c->tag);
	PKULONG(p+4, c->id);
	
	switch(c->op) {
	default:
		fatal("pkcall: unknown op: %d", c->op);
	case Tnop:
	case Rnop:
		return 8;
	case Rerror:
		memmove(p+8, c->error, ERRLEN);
		return 8+ERRLEN;
	case Tattach:
		return 8;
	case Rattach:
		pksinfo(p+8, &c->info);
		return 8+Sinfolen;
	case Tdetach:
		return 8;
	case Rdetach:
		return 8;
	case Topen:
		memmove(p+8, c->path, Spathlen);
		memmove(p+8+Spathlen, c->type, Snamelen);
		return 8+Spathlen+Snamelen;
	case Ropen:
		pksinfo(p+8, &c->info);
		return 8+Sinfolen;
	case Tclose:
	case Rclose:
		return 8;
	case Tread:
		PKULONG(p+8, c->nread);
		return 12;
	case Rread:
		PKULONG(p+8, c->nread);
		memmove(p+12, c->data, c->nread);
		return 12+c->nread;
	case Twrite:
		PKULONG(p+8, c->nwrite);
		memmove(p+12, c->data, c->nwrite);
		return 12+c->nwrite;
	case Rwrite:
		PKULONG(p+8, c->nwrite);
		return 12;
	case Trpc:
		PKULONG(p+8, c->nread);
		PKULONG(p+12, c->nwrite);
		memmove(p+16, c->data, c->nwrite);
		return 16+c->nwrite;
	case Rrpc:
		PKULONG(p+8, c->nread);
		memmove(p+12, c->data, c->nread);
		return 12+c->nread;
	}
}

static int
unpkcall(Call *c, uchar *p, int n)
{
	if(n < 8)
		return 0;
	if(p[0] != 0x96) {
		werrstr("bad magic in packet");
		return -1;
	}
	c->op = p[1];
	c->tag = UNPKUSHORT(p+2);
	c->id = UNPKULONG(p+4);

	switch(c->op) {
	default:
		werrstr("bad format in packet");
		return -1;
	case Tnop:
	case Rnop:
		return 8;
	case Rerror:
		if(n < 8+ERRLEN)
			return 0;
		memmove(c->error, p+8, ERRLEN);
		return 8+ERRLEN;
	case Tattach:
		return 8;
	case Rattach:
		if(n < 8+Sinfolen)
			return 0;
		unpksinfo(&c->info, p+8);
		return 8+Sinfolen;
	case Tdetach:
		return 8;
	case Rdetach:
		return 8;
	case Topen:
		if(n < 8+Spathlen+Snamelen)
			return 0;
		memmove(c->path, p+8, Spathlen);
		memmove(c->type, p+8+Spathlen, Snamelen);
		return 8+Spathlen+Snamelen;
	case Ropen:
		if(n < 8+Sinfolen)
			return 0;
		unpksinfo(&c->info, p+8);
		return 8+Sinfolen;
	case Tclose:
	case Rclose:
		return 8;
	case Tread:
		if(n < 12)
			return 0;
		c->nread = UNPKULONG(p+8);
		return 12;
	case Rread:
		if(n < 12)
			return 0;
		c->nread = UNPKULONG(p+8);
		if(n < 12+c->nread)
			return 0;
		c->data = p+12;
		return 12+c->nread;
	case Twrite:
		if(n < 12)
			return 0;
		c->nwrite = UNPKULONG(p+8);
		if(n < 12+c->nwrite)
			return 0;
		c->data = p+12;
		return 12+c->nwrite;
		break;
	case Rwrite:
		c->nwrite = UNPKLONG(p+8);
		return 12;
	case Trpc:
		if(n < 16)
			return 0;
		c->nread = UNPKULONG(p+8);
		c->nwrite = UNPKULONG(p+12);
		if(n < 16+c->nwrite)
			return 0;
		c->data = p+16;
		return 16+c->nwrite;
	case Rrpc:
		if(n < 12)
			return 0;
		c->nread = UNPKULONG(p+8);
		if(n < 12+c->nread)
			return 0;
		c->data = p+12;
		return 12+c->nread;
	}
}

static int
callcheck(Call *c, Call *r)
{
	if(r->id != c->id) {
		werrstr("reply id mistmatch: got %d expected %d", r->id, c->id);
		return 0;
	}
	if(r->op != c->op+1 && r->op != Rerror) {
		werrstr("reply op mistmatch: got %d expected %d", r->op, c->op);
		return 0;
	}
	if((r->op == Rread || r->op == Rrpc) && r->nread > c->nread) {
		werrstr("reply has invalid nread");
		return 0;
	}
	if(r->op == Rwrite && r->nwrite > c->nwrite) {
		werrstr("reply has invalid nwrite");
		return 0;
	}
	return 1;
}

static void
callcpy(Call *d, Call *s)
{
	d->op = s->op;
	d->tag = s->tag;
	d->id = s->id;

	switch(s->op) {
	default:
		fatal("callcpy: unknown op: %d", s->op);
	case Tnop:
	case Rnop:
		break;
	case Rerror:
		memmove(d->error, s->error, ERRLEN);
		break;
	case Tattach:
		break;
	case Rattach:
		memmove(&d->info, &s->info, sizeof(Sinfo));
		break;
	case Tdetach:
		break;
	case Rdetach:
		break;
	case Topen:
		memmove(d->path, s->path, Spathlen);
		memmove(d->type, s->type, Snamelen);
		break;
	case Ropen:
		memmove(&d->info, &s->info, sizeof(Sinfo));
		break;
	case Tclose:
	case Rclose:
		break;
	case Tread:
		d->nread = s->nread;
		break;
	case Rread:
		d->nread = s->nread;
		memmove(d->data, s->data, s->nread);
		break;
	case Twrite:
		d->nwrite = s->nwrite;
		memmove(d->data, s->data, s->nwrite);
		break;
	case Rwrite:
		d->nwrite = s->nwrite;
		break;
	case Trpc:
		d->nwrite = s->nwrite;
		d->nread = s->nread;
		memmove(d->data, s->data, s->nwrite);
		break;
	case Rrpc:
		d->nread = s->nread;
		memmove(d->data, s->data, s->nread);
		break;
	}
}

static int
callconv(va_list *arg, Fconv *f1)
{
	Call *c;
	int id, op, tag, n;
	char buf[512];

	c = va_arg(*arg, Call*);
	op = c->op;
	id = c->id;
	tag = c->tag;
	switch(op){
	case Tnop:
		sprint(buf, "Tnop tag %ud", tag);
		break;
	case Rnop:
		sprint(buf, "Rnop tag %ud", tag);
		break;
	case Rerror:
		sprint(buf, "Rerror tag %ud error %.64s", tag, c->error);
		break;
	case Tattach:
		sprint(buf, "Tattach tag %ud id %d", tag, id);
		break;
	case Rattach:
		sprint(buf, "Rattach tag %ud id %d name '%.28s' type '%.28s' id[%ux %ux %ux %ux] size %d",
			tag, id, c->info.name, c->info.type, c->info.id[0], c->info.id[1],
			c->info.id[2], c->info.id[3], c->info.mesgsize);
		break;
	case Tdetach:
		sprint(buf, "Tdetach tag %ud", tag);
		break;
	case Rdetach:
		sprint(buf, "Rdetach tag %ud", tag);
		break;
	case Topen:
		sprint(buf, "Topen tag %ud id %d %.64s %.28s", tag, id, c->path, c->type);
		break;
	case Ropen:
		sprint(buf, "Ropen tag %ud id %d name '%.28s' type '%.28s' id[%ux %ux %ux %ux] size %d",
			tag, id, c->info.name, c->info.type, c->info.id[0], c->info.id[1],
			c->info.id[2], c->info.id[3], c->info.mesgsize);
		break;
	case Tclose:
		sprint(buf, "Tclose tag %ud id", tag, id);
		break;
	case Rclose:
		sprint(buf, "Rclose tag %ud id", tag, id);
		break;
	case Tread:
		sprint(buf, "Tread tag %ud id %d count %d",
			tag, id, c->nread);
		break;
	case Rread:
		n = sprint(buf, "Rread tag %ud id %d count %d ", tag, id, c->nread);
		dumpsome(buf+n, c->data, c->nread);
		break;
	case Twrite:
		n = sprint(buf, "Twrite tag %ud id %d count %d ",
			tag, id, c->nwrite);
		dumpsome(buf+n, c->data, c->nwrite);
		break;
	case Rwrite:
		sprint(buf, "Rwrite tag %ud fid %d count %d", tag, id, c->nwrite);
		break;
	case Trpc:
		n = sprint(buf, "Trpc tag %ud id %d write %d read %d",
			tag, id, c->nwrite, c->nread);
		dumpsome(buf+n, c->data, c->nwrite);
		break;
	case Rrpc:
		n = sprint(buf, "Rrpc tag %ud id %d count %d ", tag, id, c->nread);
		dumpsome(buf+n, c->data, c->nread);
		break;
	default:
		sprint(buf,  "unknown op %d", op);
	}
	strconv(buf, f1);
	return 0;
}


/*
 * dump out count (or DUMPL, if count is bigger) bytes from
 * buf to ans, as a string if they are all printable,
 * else as a series of hex bytes
 */
#define DUMPL 24

static void
dumpsome(char *ans, char *buf, long count)
{
	int i, printable;
	char *p;

	printable = 1;
	if(count > DUMPL)
		count = DUMPL;
	for(i=0; i<count && printable; i++)
		if((buf[i]<32 && buf[i] !='\n' && buf[i] !='\t') || buf[i]>127)
			printable = 0;
	p = ans;
	*p++ = '\'';
	if(printable){
		memmove(p, buf, count);
		p += count;
	}else{
		for(i=0; i<count; i++){
			if(i>0 && i%4==0)
				*p++ = ' ';
			sprint(p, "%2.2ux", buf[i]);
			p += 2;
		}
	}
	*p++ = '\'';
	*p = 0;
}
