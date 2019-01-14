#include <u.h>
#include <9pm/libc.h>

long
atol(char *s)
{
	long n;
	int f;

	n = 0;
	f = 0;
	while(*s == ' ' || *s == '\t')
		s++;
	if(*s == '-' || *s == '+') {
		if(*s++ == '-')
			f = 1;
		while(*s == ' ' || *s == '\t')
			s++;
	}

	while(*s >= '0' && *s <= '9')
		n = n*10 + *s++ - '0';

	if(f)
		n = -n;
	return n;
}

int
atoi(char *s)
{

	return atol(s);
}
