#include <u.h>
#include <9pm/libc.h>

void	cat(int f, char *s);

void
main(int argc, char *argv[])
{
	int f, i;

	if(argc == 1)
		cat(0, "<stdin>");
	else for(i=1; i<argc; i++){
		f = open(argv[i], OREAD);
		if(f < 0)
			fatal("can't open %s: %r", argv[i]);
		else{
			cat(f, argv[i]);
			close(f);
		}
	}
	exits(0);
}

void
cat(int f, char *s)
{
	char buf[8192];
	long n;

	while((n=read(f, buf, (long)sizeof buf))>0)
		if(write(1, buf, n)!=n)
			fatal("write error copying %s: %r", s);
	if(n < 0)
		fatal("error reading %s: %r", s);
}

