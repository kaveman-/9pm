#include <u.h>
#include <9pm/libc.h>

void
esignal(Event *e)
{	
	lock(&e->lk);
	e->ready = 1;
	if(e->nwait)
		locksignal(&e->lk);
	unlock(&e->lk);
}

int
ewait(Event *e)
{
	int i;

	lock(&e->lk);
	e->nwait++;
	if(!e->ready && !e->poison)
		lockwait(&e->lk);
	e->nwait--;
	e->ready = 0;
	i = e->poison;
	unlock(&e->lk);

	return !i;
}

int
epoll(Event *e)
{
	int i;

	lock(&e->lk);
	i = e->ready | e->poison;
	e->ready = 0;
	unlock(&e->lk);

	return i;
}

void
epoison(Event *e)
{
	lock(&e->lk);
	e->poison = -1;
	locksignalall(&e->lk);
	unlock(&e->lk);
}

void
ereset(Event *e)
{
	lock(&e->lk);
	e->poison = 0;
	unlock(&e->lk);
}

