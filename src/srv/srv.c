#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

typedef struct Mnt	Mnt;
typedef struct Conn	Conn;

enum {
	Nhash	= (1<<8),	/* must be a power of two */
};

struct Mnt {
	Ref	ref;
	Sinfo	info;
	Stype	*type;
	void	*a;
	Mnt	*next;	
};

struct Conn {
	Ref	ref;
	int	id;
	Sinfo	info;
	void	*a;		/* srv's provate data */
	Mnt	*mnt;
	Conn	*next;		/* for hash */
};

struct {
	Qlock	lk;
	int	id;
	Mnt	*mnt;
	Conn	*hash[Nhash];
} srv;

static Conn	*connlookup(int id);
static int	conndec(Conn*);

static void	mntdec(Mnt*);


void
srvinit(void)
{
	srv.id = 1000;		/* keep away from fd space */
	atexit(srvfini);
}

void
srvfini(void)
{
	int i;
	Conn *c;

	atexitdont(srvfini);

	return;

	qlock(&srv.lk);
	fprint(2, "%s: srv connections\n", argv0);
	for(i=0; i<Nhash; i++)
		for(c=srv.hash[i]; c; c=c->next)
			fprint(2, "\t%d: %s:%s ref = %d\n", c->id, c->info.name,
				c->info.type, c->ref.ref);
	qunlock(&srv.lk);
}

int
srvmount(char *name, Stype *type, void *a)
{
	Mnt *p;
	Sinfo info;

	if(strchr(name, '/') != 0 ||
           strchr(name, ' ') != 0 ||
	   strlen(name) >= Snamelen) {
		werrstr("bad srv name");
		return -1;
	}

	memset(&info, 0, sizeof(info));
	a = type->init(a, &info);
	if(a == 0)
		return 0;

assert(info.mesgsize == 0 || info.mesgsize >= Smesgsize);

	qlock(&srv.lk);

	for(p = srv.mnt; p; p=p->next) {
		if(strcmp(p->info.name, name) == 0) {
			qunlock(&srv.lk);
			werrstr("name in use");
			type->fini(a);
			return 0;
		}
	}

	p = mallocz(sizeof(Mnt));
	p->ref.ref = 1;
	memmove(&p->info, &info, sizeof(Sinfo));
	strcpy(p->info.name, name);
	p->type = type;
	p->a = a;

	p->next = srv.mnt;
	srv.mnt = p;

	qunlock(&srv.lk);

	return 1;
}

int
srvunmount(char *name)
{
	Mnt *p, *op;

	qlock(&srv.lk);
	for(p=srv.mnt,op=0; p; op=p,p=p->next)
		if(strcmp(p->info.name, name) == 0)
			break;
	if(p == 0) {
		werrstr("mount point not found");
		qunlock(&srv.lk);
		return 0;
	}
	if(op == 0)
		srv.mnt = p->next;
	else
		op->next = p->next;
	p->next = 0;

	qunlock(&srv.lk);

	mntdec(p);
	return 1;
}

int
srvopen(char *path, char *type)
{
	char name[Snamelen];
	Conn *c;
	char *p, *q;
	int n, id;
	Mnt *m;
	uint h;
	Sinfo info;
	void *a;
	
	if(path[0] == '/')
		path++;
	p = strchr(path, '/');
	if(p == 0)
		n = strlen(path);
	else
		n = p-(path);
	if(n >= Snamelen) {
		werrstr("invalid path");
		return -1;
	}
	memcpy(name, path, n);
	name[n] = 0;
	if(p == 0)
		p = "/";

	qlock(&srv.lk);
	for(m=srv.mnt; m; m=m->next) {
		if(strcmp(m->info.name, name) == 0)
			break;
	}
	if(m == 0) {
		qunlock(&srv.lk);
		werrstr("server not found");
		return -1;
	}

	/* hold mnt up over open */
	refinc(&m->ref);
	id = srv.id++;
	qunlock(&srv.lk);

	q = strrchr(p, '/');
	assert(q != 0);
	if(q[1] == 0)
		strcpy(info.name, "/");
	else {
		strncpy(info.name, q+1, Snamelen);
		info.name[Snamelen-1] = 0;
	}
	strncpy(info.type, type, Snamelen);
	info.type[Snamelen-1] = 0;

	memcpy(info.id, m->info.id, Sidlen*sizeof(int));
	info.mesgsize = m->info.mesgsize;

	a = m->type->open(m->a, id, p, type, &info);
	if(a == 0) {
		mntdec(m);
		return -1;
	}

assert(info.mesgsize == 0 || info.mesgsize >= Smesgsize);

	qlock(&srv.lk);

	c = mallocz(sizeof(Conn));
	c->ref.ref = 1;
	c->id = id;
	c->a = a;
	c->info = info;
	c->mnt = m;

	h = c->id & (Nhash-1);
	c->next = srv.hash[h];
	srv.hash[h] = c;

	/* after we unlock - conn can go away */
	id = c->id;

	qunlock(&srv.lk);

	return id;
}

int
srvinfo(int id, Sinfo *info)
{
	Conn *c;

	if((c = connlookup(id)) == 0)
		return -1;
	memmove(info, &c->info, sizeof(Sinfo));
	conndec(c);
	return 1;
}

int
srvclose(int id)
{
	uint h, r;
	Conn *c, *oc;
	Mnt *m;

	qlock(&srv.lk);
	h = id & (Nhash-1);
	for(c=srv.hash[h],oc=0; c; oc=c,c=c->next)
		if(c->id == id)
			break;
	if(c == 0) {
		werrstr("invalid server connection id");
		qunlock(&srv.lk);
		return 0;
	}

	if(oc == 0)
		srv.hash[h] = c->next;
	else
		oc->next = c->next;
	c->next = 0;
	qunlock(&srv.lk);

	m = c->mnt;
	r = m->type->close(m->a, c->a);
	conndec(c);
	return r;
}


int
srvread(int sid, void *buf, int n)
{
	Conn *c;
	Stype *t;

	if((c = connlookup(sid)) == 0)
		return -1;
	t = c->mnt->type;
	if(t->read == 0) {
		werrstr("server does not support read ops");
		conndec(c);
		return -1;
	}

	if(c->info.mesgsize && n > c->info.mesgsize)
		n=c->info.mesgsize;

	n = t->read(c->mnt->a, c->a, buf, n);
	if(!conndec(c)) {
		werrstr("connection closed locally");
		return -1;
	}

	if(n == 0)
		werrstr("eof on connection");	/* not really an error */
		
	return n;
}

int
srvwrite(int sid, void *buf, int n)
{
	Conn *c;
	Stype *t;
	uchar *p;
	int n2;

	if((c = connlookup(sid)) == 0)
		return -1;
	t = c->mnt->type;
	if(t->write == 0) {
		werrstr("server does not support write ops");
		conndec(c);
		return -1;
	}

	p = buf;
	while(n>0) {
		n2 = n;
		if(c->info.mesgsize && n2 > c->info.mesgsize)
		 	n2 = c->info.mesgsize;
		
		n2 = t->write(c->mnt->a, c->a, p, n2);
		if(n2 <= 0)
			return n2;
		p += n2;
		n -= n2;
	}
	if(!conndec(c)) {
		werrstr("connection closed locally");
		return -1;
	}
	return p-(uchar*)buf;
}

int
srvrpc(int sid, void *buf, int nw, int nr)
{
	Conn *c;
	Stype *t;

	if((c = connlookup(sid)) == 0)
		return -1;
	t = c->mnt->type;
	if(t->rpc == 0) {
		werrstr("server does not support rpc ops");
		conndec(c);
		return -1;
	}

	if(c->info.mesgsize && (c->info.mesgsize < nw || c->info.mesgsize < nr)) {
		werrstr("mesg too big");
		conndec(c);
		return -1;
	}

	nr = t->rpc(c->mnt->a, c->a, buf, nw, nr);
	if(!conndec(c)) {
		werrstr("connection closed locally");
		return -1;
	}
	return nr;
}

int
srvcallback(int sid, void (*f)(int, uchar*, int))
{
	Conn *c;
	Stype *t;
	int r;

	if((c = connlookup(sid)) == 0)
		return -1;
	t = c->mnt->type;
	if(t->callback == 0) {
		werrstr("server does not support callback ops");
		conndec(c);
		return -1;
	}

	r = t->callback(c->mnt->a, c->a, f);
	if(!conndec(c)) {
		werrstr("connection closed locally");
		return -1;
	}
	return r;
}

void
srvgenid(int *p, void *a)
{
	uchar *s, *s2;
	int i;
	static int n;

	/* simple attempt at a golbally unique id */

	p[0] = time(0);
	p[1] = getpid();
	p[2] = (int)a;
	p[3] = n++;

	s = getenv("sysname");
	assert(s != 0);

	for(s2=s; *s2; s2++) {
		p[0] *= 13; p[1] *= 17; p[3] *= 29; p[3] *= 71;
		for(i=0; i<4; i++)
			p[i] += *s2;
	}

	free(s);
}

static Conn *
connlookup(int id)
{
	uint h;
	Conn *p;

	qlock(&srv.lk);
	h = id & (Nhash-1);
	for(p=srv.hash[h]; p; p=p->next) {
		if(p->id == id) {
			refinc(&p->ref);
			break;
		}
	}
	qunlock(&srv.lk);
	if(p == 0)
		werrstr("invalid server connection id");
	return p;
}

static void
mntdec(Mnt *m)
{
	if(refdec(&m->ref) > 0)
		return;
	assert(m->ref.ref == 0);

	/* already removed from mnt list */
	assert(m->next == 0);

	m->type->fini(m->a);
	free(m);
}

static int
conndec(Conn *c)
{
	Mnt *m;

	if(refdec(&c->ref) > 0)
		return 1;
	assert(c->ref.ref == 0);

	/* already removed from hash table */
	assert(c->next == 0);

	m = c->mnt;
	
	m->type->free(m->a, c->a);

	mntdec(m);
	free(c);
	return 0;
}

