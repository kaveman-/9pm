#include <u.h>
#include <9pm/libc.h>

void
main(int argc, char *argv[])
{
	if(argc>1)
		sleep(1000*atol(argv[1]));
	exits(0);
}
