#include <u.h>
#include <9pm/libc.h>

int
PM_assert(int a)
{
	int pc;

	pc = getcallerpc(&a);

	fprint(2, "%s: assert failed at pc = %ux\n", argv0, pc);
	abort();

	return 0;	/* never gets here */
}
