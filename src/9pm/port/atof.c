#include <u.h>
#include <9pm/libc.h>

double
atof(char *cp)
{
	return strtod(cp, 0);
}
