#include <u.h>
#include <9pm/libc.h>

Rune*
wstrecpy(Rune *s1, Rune *s2, Rune *e)
{
	assert(s1<e);
		
	while(s1<e && *s2)
		*s1++ = *s2++;

	if(s1 == e)
		s1--;
	
	*s1 = 0;

	return s1;
}
