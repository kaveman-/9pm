#include <u.h>
#include <9pm/libc.h>

Rune*
wstrncpy(Rune *s0, Rune *s1, int n)
{
	Rune *os0;
	int i;

	os0 = s0;
	for(i=0; i<n; i++) {
		if((*s0++ = *s1++) == 0) {
			while(++i < n)
				*s0++= 0;
			return os0;
		}
	}
	return os0;
}

