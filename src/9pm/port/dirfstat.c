#include <u.h>
#include <9pm/libc.h>
#include <9pm/auth.h>
#include <9pm/fcall.h>

int
dirfstat(int f, Dir *dir)
{
	char buf[DIRLEN];

	if(fstat(f, buf) == -1)
		return -1;
	convM2D(buf, dir);
	return 0;
}
