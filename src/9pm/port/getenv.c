#include <u.h>
#include <9pm/libc.h>

Qlock	envlock;

extern	int	pm_getenv(char*, char*, int);
extern	int	pm_getenvall(char*, int);
extern	int	pm_putenv(char*, char*);
extern	int	pm_putenvall(char*);

char *
getenv(char *name)
{
	char buf[200], *p;
	int n;

	qlock(&envlock);

	n = pm_getenv(name, buf, sizeof(buf));
	if(n == 0) {
		qunlock(&envlock);
		return 0;
	}
	if(n < sizeof(buf)) {
		qunlock(&envlock);
		p = malloc(n+1);
		memmove(p, buf, n+1);
		return p;
	}

	p = malloc(n);
	n = pm_getenv(name, p, n);
	qunlock(&envlock);
	if(n == 0) {
		free(p);
		return 0;
	}
	return p;
}

char**
getenvall(void)
{
	char buf[4096], *p, *q, **a;
	int i, n;

	qlock(&envlock);

	p = buf;
	n = pm_getenvall(p, sizeof(buf));
	if(n >= sizeof(buf)) {
		p = malloc(n);
		pm_getenvall(p, n);
	}
	qunlock(&envlock);

	for(q=p,n=0; *q; q+=strlen(q)+1)
		n++;
	a = malloc((n+1)*sizeof(char*));
	for(q=p,i=0; *q; q+=strlen(q)+1,i++)
		a[i] = strdup(q);
	a[i] = 0;

	if(p != buf)
		free(p);
	
	return a;
}

int
putenv(char *name, char *val)
{
	int r;

	qlock(&envlock);
	r = pm_putenv(name, val);
	qunlock(&envlock);

	return r;
}
