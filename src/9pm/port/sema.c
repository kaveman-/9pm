#include <u.h>
#include <9pm/libc.h>
#include "9pm.h"

void
swait(Sema *s)
{
	lock(&s->lk);
	
	assert(s->cnt >= 0);

	if(s->cnt > 0) {
		s->cnt--;
		unlock(&s->lk);
		return;
	}

	PM_queue((Thread**)&s->first, (Thread**)&s->last);
	unlock(&s->lk);
	PM_tsleep();
}

void
ssignal(Sema *s)
{
	Thread *t;

	lock(&s->lk);
	assert(s->cnt >= 0);
	t = PM_dequeue((Thread**)&s->first, (Thread**)&s->last);
	if(t == 0) {
		s->cnt++;
		unlock(&s->lk);
	} else {
		unlock(&s->lk);
		PM_twakeup(t);
	}
}
