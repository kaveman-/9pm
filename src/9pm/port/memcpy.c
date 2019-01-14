#include	<u.h>
#include	<9pm/libc.h>

void*
memcpy(void *a1, void *a2, ulong n)
{
	return memmove(a1, a2, n);
}
