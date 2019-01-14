#include <u.h>
#include <9pm/libc.h>

/*
 *  force a connection to hangup
 */
int
hangup(int ctl)
{
	return write(ctl, "hangup", sizeof("hangup")-1) != sizeof("hangup")-1;
}
