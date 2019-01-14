#include <u.h>
#include <9pm/libc.h>

int
wstrlen(Rune *s)
{
	int n;

	for(n=0; *s; s++,n++)
		;
	return n;
}
