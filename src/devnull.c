#include <u.h>
#include <9pm/libc.h>

void	cat(int f, char *s);

void
main(int argc, char *argv[])
{
	char buf[8192];
	long n;

	while((n=read(0, buf, (long)sizeof buf))>0)
		;
	if(n < 0)
		fatal("error reading: %r");
	exits(0);
}
