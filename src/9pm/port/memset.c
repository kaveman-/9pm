#include	<u.h>
#include	<9pm/libc.h>

void*
memset(void *ap, int c, ulong n)
{
	char *p;

	p = ap;
	while(n > 0) {
		*p++ = c;
		n--;
	}
	return ap;
}
