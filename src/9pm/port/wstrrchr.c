#include <u.h>
#include <9pm/libc.h>

Rune*
wstrrchr(Rune *s, Rune r)
{
	Rune *p;

	if(r == 0)
		return wstrchr(s, 0);

	p = 0;
	while(s = wstrchr(s, r))
		p = s++;
	return p;
}


