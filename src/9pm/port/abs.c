#include <u.h>
#include <9pm/libc.h>

#pragma function (abs)
#pragma function (labs)

int
abs(int a)
{
	if(a < 0)
		return -a;
	return a;
}

long
labs(long a)
{
	if(a < 0)
		return -a;
	return a;
}
