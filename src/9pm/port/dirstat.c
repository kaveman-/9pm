#include <u.h>
#include <9pm/libc.h>
#include <9pm/auth.h>
#include <9pm/fcall.h>

int
dirstat(char *name, Dir *dir)
{
	char buf[DIRLEN];

	if(stat(name, buf) == -1)
		return -1;
	convM2D(buf, dir);
	return 0;
}
