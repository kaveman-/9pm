#include <u.h>
#include <9pm/libc.h>

Rune*
wstrcpy(Rune *s0, Rune *s1)
{
	Rune *os0;

	os0 = s0;
	while(*s1)
		*s0++ = *s1++;
	*s0 = 0;
	return os0;
}
