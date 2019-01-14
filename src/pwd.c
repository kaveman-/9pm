#include <u.h>
#include <9pm/libc.h>

void
main(int argc, char *argv[])
{
	char path[512];
	
	getwd(path, sizeof(path));
	print("%s\n", path);
	exits(0);
}
