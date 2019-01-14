#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

void
lock(Lock *lk)
{
	int i;

	/* easy case */
	if(!tas(&lk->val))
		return;

	/* for muli processor machines */
	for(i=0; i<100; i++)
		if(!tas(&lk->val))
			return;

//	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	for(;;) {
		for(i=0; i<10000; i++) {
			Sleep(0);
			if(!tas(&lk->val)) {
//				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
				return;
			}
		}

		for(i=0; i<100; i++) {
			Sleep(10);
			if(!tas(&lk->val))
				return;
		}

		/* we are in trouble */
		fprint(2, "lock loop: val=%d &lock=%x pc=%x\n", lk->val, lk, getcallerpc(&lk));
	}
}

int
canlock(Lock *lk)
{
	return !tas(&lk->val);
}

void
unlock(Lock *lk)
{
	assert(lk->val);
	lk->val = 0;
}

void
lockwait(Lock *lk)
{
	assert(lk->val);

	threadqueue(lk);
	unlock(lk);
	threadsleep();
	lock(lk);
}

int
locksignal(Lock *lk)
{
	Thread *t;

	assert(lk->val);

	t = threaddequeue(lk);
	if(t)
		threadwakeup(t);

	return t!=0;
}

int
locksignalall(Lock *lk)
{
	Thread *t;
	int n;

	assert(lk->val == 1);

	for(n=0;; n++) {
		t = threaddequeue(lk);
		if(t == 0)
			return n;
		threadwakeup(t);
	}
	return 0;	/* for the compiler */
}

