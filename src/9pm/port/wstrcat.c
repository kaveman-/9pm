#include <u.h>
#include <9pm/libc.h>

Rune*
wstrcat(Rune *s0, Rune *s1)
{
	return wstrcpy(wstrchr(s0, 0), s1);
}

