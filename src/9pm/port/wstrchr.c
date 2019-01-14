#include <u.h>
#include <9pm/libc.h>

Rune*
wstrchr(Rune *s, Rune r)
{
	if(r == 0) {
		while(*s++)
			;
		return s-1;
	}

	while(*s) {
		if(*s == r)
			return s;
		s++;
	}

	return 0;
}

