#include <u.h>
#include <9pm/libc.h>

uint
strlen(char *s)
{

	return strchr(s, 0) - s;
}
