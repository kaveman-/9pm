#include <u.h>
#include <9pm/libc.h>

#include <9pm/windows.h> 

#include "9pm.h"

typedef struct Ttbl	Ttbl;

enum {
	Nthread = 100,
};

struct Ttbl {
	Lock	lk;
	int	nthread;
	int	nowait;
	int	nalloc;
	Thread	*t[Nthread];
	Thread	*free;
};

_declspec(thread)       Thread *CT;

static Ttbl	tt;

static Thread	*threadalloc(void);
static void	threadfree(Thread *t);

void
threadinit(void)
{
	Thread *t;

	t = threadalloc();
	assert(t != 0);

	CT = t;

	CT->tid = GetCurrentThreadId();
	CT->sema = CreateSemaphore(0, 0, 1000, 0);
}

DWORD WINAPI
tramp(void *t)
{
	CT = t;

	CT->tid = GetCurrentThreadId();
	CT->sema = CreateSemaphore(0, 0, 1000, 0);
	if(CT->sema == 0) {
		winerror();
		fatal("could not create semaphore");
	}

	(*CT->f)(CT->a);

	threadexit();
	return 0;
}

uint
thread(void (*f)(void *), void *a)
{
	Thread *t;
	int tid;
	HANDLE h;

	if((t = threadalloc()) == 0)
		return 0;

	t->f = f;
	t->a = a;

	h = CreateThread(0, 0, tramp, t, 0, &tid);
	if(h == 0)
		return 0;
	CloseHandle(h);

	return tid;
}

void
threadexit(void)
{
	if(tt.nthread == 1 && !CT->nowait)
		(*exits)(0);

	threadfree(CT);
	
	ExitThread(0);
}

int
errstr(char *s)
{
	char tmp[ERRLEN];

	strncpy(tmp, s, ERRLEN);
	tmp[ERRLEN-1] = 0;
	strncpy(s, CT->error, ERRLEN);
	strncpy(CT->error, tmp, ERRLEN);

	return 0;
}

void
winerror(void)
{
	char tmp[ERRLEN];

	win_error(tmp, ERRLEN);
	errstr(tmp);
}


void
threadnowait(void)
{	
	lock(&tt.lk);
	if(!CT->nowait) {
		if(tt.nthread == 1) {
			unlock(&tt.lk);
			(*exits)(0);
		}		
		tt.nthread--;
		CT->nowait = 1;
	}
	unlock(&tt.lk);
}

double
threadtime(void)
{
	FILETIME t[4];
	
	if(!GetThreadTimes(GetCurrentThread(), t, t+1, t+2, t+3))
		return 0.0;
 
	return (t[2].dwLowDateTime + t[3].dwLowDateTime)*1e-7 +
		(t[2].dwHighDateTime + t[3].dwHighDateTime)*4294967296*1e-7;
}


uint
gettid(void)
{
	return CT->tid;
}

int
gettindex(void)
{
	return CT->tindex;
}

void
threadsleep()
{
	WaitForSingleObject(CT->sema, INFINITE);
}

void
threadwakeup(Thread *t)
{
	ReleaseSemaphore(t->sema, 1, 0);
}

static Thread *
threadalloc(void)
{
	Thread *t;

	lock(&tt.lk);
	tt.nthread++;
	if((t = tt.free) != 0) {
		tt.free = t->qnext;
		unlock(&tt.lk);
		return t;
	}

	if(tt.nalloc == Nthread) {
		unlock(&tt.lk);
		return 0;
	}
	t = tt.t[tt.nalloc] = sbrk(sizeof(Thread));
	t->tindex = tt.nalloc;
	tt.nalloc++;
	t->nrtab = 16;
	t->rtab = malloc((t->nrtab+Nrlook+1)*sizeof(RWlock*));
	unlock(&tt.lk);
	return t;
}

static void
threadfree(Thread *t)
{
	CloseHandle(t->sema);
	t->tid = -1;
	memset(t->rtab, 0, (t->nrtab+Nrlook+1)*sizeof(RWlock*));

	lock(&tt.lk);
	if(!t->nowait)
		tt.nthread--;
	t->qnext = tt.free;
	tt.free = t;
	unlock(&tt.lk);
}
