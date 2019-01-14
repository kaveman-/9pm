#include <u.h>
#include <9pm/libc.h>

static struct {
	Qlock	lk;
	int	pg;
	int	id;
	void	(*intrfunc)(void);
} pg;

static void	pgthread(void*);
static void	pgwrite(int pg, char *s);

void
pm_pginit(void)
{
	char *p;

	p = getenv("pgroup");
	if(p == 0) {
		pg.id = -1;
		pg.pg = pgnew();
	} else {
		pg.pg = atoi(p);
		free(p);
		thread(pgthread, (void*)pg.id);
	}
}

int
pgnew(void)
{
	char buf[100];
	int n, id;

	qlock(&pg.lk);
	srvclose(pg.id);
	
	id = srvopen("/9pm/pgroup/new", "pgroup-new");
	if(id < 0)
		fatal("pgnew: could not open /9pm/pgroup/new: %r");
	n = srvread(id, buf, sizeof(buf)-1);
	srvclose(id);
	if(n < 0)
		fatal("pgnew: read failed: %r");
	buf[n] = 0;
	id = atoi(buf);
	qunlock(&pg.lk);

	thread(pgthread, (void*)id);
	
	return id;
}

int
pgcurrent(void)
{	
	int i;

	qlock(&pg.lk);
	i = pg.pg;
	qunlock(&pg.lk);

	return i;
}

void
pgonintr(void (*f)(void))
{
	qlock(&pg.lk);
	pg.intrfunc = f;
	qunlock(&pg.lk);
}

void
pgintr(int pg)
{	
	pgwrite(pg, "interrupt");
}

void
pgkill(int pg)
{
	pgwrite(pg, "kill");
}

static void
pgwrite(int pg, char *s)
{
	char buf[100];
	int id;

	sprint(buf, "/9pm/pgroup/%d", pg);
	id = srvopen(buf, "pgroup");
	if(id < 0)
		return;
	srvwrite(id, s, sizeof(s));
	srvclose(id);
}

void
pgthread(void *a)
{
	int pgid, n, id;
	char buf[100];

	threadnowait();
fprint(2, "pgthread\n");
	pgid = (int)a;
	sprint(buf, "/9pm/pgroup/%d", pgid);
	id = srvopen(buf, "pgroup");
	if(id < 0)
		fatal("could not open pgroup %d", pgid);
	for(;;) {
		n = srvread(id, buf, sizeof(buf)-1);
		if(n < 0)
			break;
		buf[n] = 0;
fprint(2, "pgthread: %s\n", buf);
		qlock(&pg.lk);
		if(strcmp(buf, "interrupt") != 0 || pg.intrfunc == 0)
			_exits(0);
		thread((void(*)(void*))pg.intrfunc, 0);
		qunlock(&pg.lk);
	}
fprint(2, "pgthread: bad read: %r\n");
}