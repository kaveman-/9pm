#include <u.h>
#include <9pm/libc.h>

int
wstrtoutf(char *s, Rune *t, int n)
{
	int i;
	char *s0;

	s0 = s;
	if(n <= 0)
		return wstrutflen(t)+1;
	while(*t) {
		if(n < UTFmax+1 && n < runelen(*t)+1) {
			*s = 0;
			return i+wstrutflen(t)+1;
		}
		i = runetochar(s, t);
		s += i;
		n -= i;
		t++;
	}
	*s = 0;
	return s-s0;
}
