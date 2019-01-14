#include <u.h>
#include <9pm/libc.h>

void	intr(void);

void
main(int argc, char *argv[])
{
	int pfd[2], n, i;
	uchar buf[1000];

	ARGBEGIN{
	}ARGEND;

	consonintr(intr);
	
	if(pipeeof(pfd) < 0)
		fatal("pipe: %r");

	if(proc(argv, pfd[0], 1, 2, 0) == 0)
		fatal("proc: %r");

	close(1);
	close(2);

	for(;;){
		n = read(0, buf, sizeof(buf));
		if(n <= 0)
			break;
		if(0)
		for(i=0; i<n; ) {
			if(buf[i] != 0x7f) {
				i++;
				continue;
			}
			consintr();
			memmove(buf+i, buf+i+1, n-(i+1));
			n--;
		}
		for(i=0; i<n; i++) {
			if(buf[i] == 0x7f)
				consintr();
		}
		if(write(pfd[1], buf, n) != n)
			break;
	}
	exits(0);
}

void
intr(void)
{
}
