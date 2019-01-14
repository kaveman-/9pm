#include <u.h>
#include <9pm/libc.h>
#include "9pm.h"

enum {
	Hashmul = 151,
	Hashmod	= 1000000007,
};

static void	addrlock(RWlock *q);
static void	subrlock(RWlock *q);

void
rlock(RWlock *q)
{
	Thread *t;

	for(;;) {
		lock(&q->lk);

		if(q->hold == 0)
			break;
		PM_queue((Thread**)&q->first, (Thread**)&q->last);
		unlock(&q->lk);
		PM_tsleep();
	}

	q->readers++;
	addrlock(q);
	t = PM_dequeue((Thread**)&q->first, (Thread**)&q->last);
	unlock(&q->lk);
	
	if(t)
		PM_twakeup(t);
}

int
canrlock(RWlock *q)
{
	lock(&q->lk);
	if(q->hold == 0) {
		q->readers++;
		addrlock(q);
		unlock(&q->lk);
		return 1;
	}
	unlock(&q->lk);
	return 0;
}

void
runlock(RWlock *q)
{
	int n;

	lock(&q->lk);

	q->readers--;
	subrlock(q);

	n = q->readers;
	assert(n >= 0);

	if(n == 0 && q->hold) {
		unlock(&q->lk);
		PM_twakeup(q->hold);
		return;
	}

	unlock(&q->lk);
}

void
wlock(RWlock *q)
{
	for(;;) {
		lock(&q->lk);

		if(q->hold == 0)
			break;
		PM_queue((Thread**)&q->first, (Thread**)&q->last);
		unlock(&q->lk);
		PM_tsleep();
	}

	q->hold = CT;

	if(q->readers) {
		unlock(&q->lk);
		PM_tsleep();
		return;
	}

	unlock(&q->lk);
}

int
canwlock(RWlock *q)
{
	lock(&q->lk);
	if(q->hold == 0 && q->readers == 0) {
		q->hold = CT;
		unlock(&q->lk);
		return 1;
	}
	unlock(&q->lk);
	return 0;
}

void
wunlock(RWlock *q)
{
	Thread * t;

	lock(&q->lk);
	assert(q->hold == CT);
	q->hold = 0;
	t = PM_dequeue((Thread**)&q->first, (Thread**)&q->last);
	unlock(&q->lk);

	if(t)
		PM_twakeup(t);
}

int
holdwlock(RWlock *q)
{
	return q->hold == CT;
}

static void
addrlock(RWlock *q)
{
	uint h;
	int i, n;
	Thread *ct;
	RWlock **p;


	ct = CT;

	n = ct->nrtab;

	h = q->hash;
	if(h == 0) {
		h = (((int)q * Hashmul) % Hashmod);
		h &= n-1;
		h++;
		q->hash = h;
	}

	p = ct->rtab+h;
	for(i = 0; i < Nrlook; i++, p++)
		assert(*p != q);

	p = ct->rtab+h;
	for(i=0; i < Nrlook; i++, p++) {
		if(*p)
			continue;
		*p = q;
		return;
	}

	/* expand table */
	p = ct->rtab;
	ct->nrtab *= 4;
	ct->rtab = mallocz((ct->nrtab+Nrlook+1)*sizeof(RWlock*));
	n += Nrlook+1;
	for(i=0; i<n; i++) {
		if(p[i] == 0)
			continue;
		p[i]->hash = 0;
		addrlock(p[i]);
	}

	free(p);
}

static void
subrlock(RWlock *q)
{
	uint h;
	int i;
	Thread *ct;
	RWlock **p;

	ct = CT;

	h = q->hash;

	p = ct->rtab+h;
	for(i = 0; i < Nrlook; i++, p++) {
		if(*p == q) {
			*p = 0;
			return;
		}
	}

	assert(0);
}

int
holdrlock(RWlock *q)
{
	uint h;
	int i;
	Thread *ct;
	RWlock **p;

	ct = CT;

	h = q->hash;

	p = ct->rtab+h;
	for(i = 0; i < Nrlook; i++, p++) {
		if(*p == q)
			return 1;
	}

	return 0;
}

int
holdrwlock(RWlock *q)
{
	return holdwlock(q) | holdrlock(q);
}

