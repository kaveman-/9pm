#include <u.h>
#include <9pm/libc.h>

int
utftowstr(Rune *t, char *s, int n)
{
	int i;

	if(n <= 0)
		return utflen(s)+1;
	for(i=0; i<n; i++,t++) {
		if(*s == 0) {
			*t = 0;
			return i;
		}
		s += chartorune(t, s);
	}
	t[-1] = 0;
	return n + utflen(s)+1;
}

