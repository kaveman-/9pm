#include <u.h>
#include <9pm/libc.h>
#include <9pm/auth.h>
#include <9pm/fcall.h>

int
dirwstat(char *name, Dir *dir)
{
	char buf[DIRLEN];

	convD2M(dir, buf);
	return wstat(name, buf);
}
