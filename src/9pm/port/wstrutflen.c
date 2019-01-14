#include <u.h>
#include <9pm/libc.h>

int
wstrutflen(Rune *s)
{
	int n;
	
	for(n=0; *s; n+=runelen(*s),s++)
		;
	return n;
}
