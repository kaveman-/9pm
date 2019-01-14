#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

void	pipethread(char *);
void	ping(char *path);
void	smc(void);

void
main(int argc, char *argv[])
{
	int id, i;
	int fd[2][2], tmp;
	double t;
	char **p;

	ARGBEGIN{
	} ARGEND;

	srvinit();

	if(!srvmount("ping", &srvping, 0))
		fatal("srvmount: %r");
	
	ping("/ping");

	smc();

	threadexit();

	if(pipe(fd[0]) < 0)
		fatal("pipe failed: %r");
	if(pipe(fd[1]) < 0)
		fatal("pipe failed: %r");
	tmp = fd[0][1];
	fd[0][1] = fd[1][1];
	fd[1][1] = tmp;
	
	if(!srvmount("pipe", &srvpipe, fd[0]))
		fatal("srvmount pipe: %r");
	if((id = srvopen("/pipe", "link")) < 0)
		fatal("srvopen: pipe: %r");
	if(!srvunmount("pipe") < 0)
		fatal("srvunmount: %r");

	srvproxy(id);
	
	if(!srvmount("pipe", &srvpipe, fd[1]))
		fatal("srvmount pipe: %r");
	if((id = srvopen("/pipe", "link")) < 0)
		fatal("srvopen: pipe: %r");

	if(!srvunmount("pipe") < 0)
		fatal("srvunmount: %r");

	if(!srvmount("remote", &srvremote, &id))
		fatal("srvmount, remote: %r");
	
	ping("/remote/ping");

	if(!srvunmount("remote"))
		fatal("remote: srvunmount: %r");

	if(!srvunmount("ping"))
		fatal("ping: srvunmount: %r");
}

void
pipethread(char *path)
{
	int id;
	char buf[100];
	int n;

	if((id = srvopen(path, "link")) < 0)
		fatal("pipethread: srvopen: %r");
	
	n = srvread(id, buf, sizeof(buf));
	print("pipethread %d\n", n);
	write(1, buf, n);

	memset(buf, 0, sizeof(buf));
	strcpy(buf, "hello to you too");
	
	srvwrite(id, buf, strlen(buf));

	close(id);
}

void
ping(char *path)
{
	char buf[100];
	int n, id, i, count;
	double t;

	if((id = srvopen(path, "ping")) < 0)
		fatal("could not open %s: %r", path);

fprint(2, "ping %s\n", path);
	t = -realtime();
	count = 100000;
	for(i=0; i<count; i++) {
		strcpy(buf, "hello world");
		n = srvrpc(id, buf, strlen(buf), strlen(buf));
		if(n != (int)strlen(buf))
			fatal("srcrpc: %r");
		if(memcmp(buf, "ifmmp!xpsme", strlen(buf)) != 0)
			print("bad reply = %s\n", buf);
	}
	t += realtime();
fprint(2, "ping done time=%f usec\n", t*1e6/count);

	srvclose(id);
}

void
smcecho(void *a)
{
	int data, n, i;
	uchar buf[1000];

fprint(2, "smcecho\n");

	data = (int)a;
	for(;;) {
		n = srvread(data, buf, sizeof(buf));
		if(n<0)
			fatal("smcread: %r");
		if(n==0)
			break;
		for(i=0; i<n; i++)
			buf[i]++;
		if(srvwrite(data, buf, n) != n)
			fatal("smcwrite: %r");
	}
fprint(2, "%s: smcecho eof\n", argv0);
	srvclose(data);
}
	
void
smc(void)
{
	int asid, csid;
	char adir[Spathlen], *argv[2];

	print("smc test\n");

	if(!srvmount("smc", &srvsmc, 0))
		fatal("srvmount: %r");


	asid = announce("smc!test", adir);

	if(asid<0)
		fatal("smc announce: %r");

	argv[0] = "test2";
	argv[1] = 0;

	if(proc(argv, 0, 1, 2, 0) == 0)
		fatal("proc: %r");

	for(;;) {
		csid = listen(asid, adir);
fprint(2, "listen %d\n", csid);
		if(csid<0)
			fatal("smc listen: %r");
		if(accept(csid)<0)
			fatal("smc accept: %r");
		srvclose(asid);
		smcecho((void*)csid);
		break;
	}
}

