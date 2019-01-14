#include <u.h>
#include <9pm/libc.h>
#include <9pm/auth.h>
#include <9pm/fcall.h>

int
dirfwstat(int f, Dir *dir)
{
	char buf[DIRLEN];

	convD2M(dir, buf);
	return fwstat(f, buf);
}
