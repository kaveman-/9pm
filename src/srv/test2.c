#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

void
main(int argc, char *argv[])
{
	int sid;
	char buf[100];
	int i, n;
	double t;

	ARGBEGIN{
	}ARGEND

	srvinit();

	if(!srvmount("smc", &srvsmc, 0))
		fatal("srvmount of smc failed: %r");
	
	sid = dial("smc!test", 0, 0);
	if(sid < 0)
		fatal("dial failed: %r");
	
	n = sprint(buf, "hello world");

	t = -realtime();
	for(i=0;i<10000; i++) {
		if(srvwrite(sid, buf, n) != n)
			fatal("srvwrite failed: %r");
		n = srvread(sid, buf, sizeof(buf));
		if(n != 11)
			fprint(2, "read = %d\n", n);
		if(n < 0)
			fatal("srvread failed: %r");
		if(n == 0)
			break;
	}
	t += realtime();

	print("time = %f\n", t);
	srvclose(sid);
}
