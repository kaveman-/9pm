#include "9pm.h"

void
werrstr(char *s)
{
	char buf[ERRLEN];

	strncpy(buf, s, ERRLEN);

	errstr(buf);
}

int
errstr(char *e)
{
	char tmp[ERRLEN];
	
	memmove(tmp, e, ERRLEN);
	memmove(e, CT->error, ERRLEN);
	memmove(CT->error, tmp, ERRLEN);
	CT->error[ERRLEN-1] = 0;

	return 0;
}

void
winerror(void)
{
	int e, r;
	Rune buf[100];
	char buf2[100], *p, *q;

	e = GetLastError();
	
	r = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		0, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), 0);

	if(r == 0)
		wsnprint(buf, sizeof(buf), L"windows error %d", e);

	wstr2utf(buf2, nelem(buf2), buf);

	for(p=q=buf2; *p; p++) {
		if(*p == '\r')
			continue;
		if(*p == '\n')
			*q++ = ' ';
		else
			*q++ = *p;
	}
	*q = 0;

	errstr(buf2);
}

void
nexterror(void)
{
	int n;
	Thread *t;

	t = CT;
	
	n = --t->nerrlbl;
	if(n < 0)
		fatal(L"error: %r");
	longjmp(t->errlbl[n], 1);
}

void
error(char *fmt)
{
	if(fmt)
		werrstr(fmt);
	nexterror();
}

void
poperror(void)
{
	Thread *t;

	t = CT;
	if(t->nerrlbl <= 0)
		fatal(L"error stack underflow");
	t->nerrlbl--;
}

long *
pm_waserror(void)
{
	Thread *t;
	int n;

	t = CT;
	n = t->nerrlbl++;
	if(n >= Nerrlbl)
		fatal(L"error stack underflow");
	return t->errlbl[n];
}
