#include <u.h>
#include <9pm/libc.h>

#define	NEXIT	33

static void	(*onex[NEXIT])(void);
static Lock	atlock;

atexit(void (*f)(void))
{
	int i;

	lock(&atlock);
	for(i=0; i<NEXIT; i++) {
		if(onex[i] == 0) {
			onex[i] = f;
			unlock(&atlock);
			return 1;
		}
	}
	unlock(&atlock);
	return 0;
}

void
atexitdont(void (*f)(void))
{
	int i;

	lock(&atlock);
	for(i=0; i<NEXIT; i++)
		if(onex[i] == f)
			onex[i] = 0;
	unlock(&atlock);
}

void
exits(char *s)
{
	int i;
	void (*f)(void);

	for(i = NEXIT-1; i >= 0; i--) {
		lock(&atlock);
		f = onex[i];
		onex[i] = 0;
		unlock(&atlock);

		if(f)
			(*f)();
	}
	_exits(s);
}

void
fatal(char *fmt, ...)
{
	char buf[512];
	va_list arg;

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);

	fprint(2, "%s: %s\n", argv0, buf);
	_exits(buf);
}

