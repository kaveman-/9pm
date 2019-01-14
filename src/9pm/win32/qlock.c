#include <u.h>
#include <9pm/libc.h>
#include "9pm.h"

void
qlock(Qlock *q)
{
	lock(&q->lk);

	if(q->hold == 0) {
		q->hold = CT;
		unlock(&q->lk);
		return;
	}
			
	assert(q->hold != CT);

	q->nwait++;
	threadqueue(q);
	unlock(&q->lk);
	threadsleep();
}

int
canqlock(Qlock *q)
{
	lock(&q->lk);
	if(q->hold == 0) {
		q->hold = CT;
		unlock(&q->lk);
		return 1;
	}
	unlock(&q->lk);
	return 0;
}

void
qunlock(Qlock *q)
{
	Thread *t;

	lock(&q->lk);
	assert(q->hold == CT);
	if(q->nwait == 0) {
		q->hold = 0;
		unlock(&q->lk);
		return;
	}

	q->nwait--;
	t = threaddequeue(q);
	q->hold = t;
	unlock(&q->lk);

	threadwakeup(t);
}

int
holdqlock(Qlock *q)
{
	return q->hold == CT;
}
