#include <u.h>
#include <9pm/libc.h>

Rune*
wstrdup(Rune *s)
{
	int n;
	Rune *s2;

	n = (wstrlen(s)+1)*sizeof(Rune);
	s2 = malloc(n);
	memcpy(s2, s, n);
	return s2;
}
