#include <u.h>
#include <9pm/libc.h>

int
wstrcmp(Rune *s1, Rune *s2)
{
	Rune r1, r2;

	for(;;) {
		r1 = *s1++;
		r2 = *s2++;
		if(r1 != r2) {
			if(r1 > r2)
				return 1;
			return -1;
		}
		if(r1 == 0)
			return 0;
	}
}
