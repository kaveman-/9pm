#include <u.h>
#include <9pm/libc.h>

char	output[4096];
void	add(char*, ...);
void	error(char*);
void	notifyf(void*, char*);

void
main(int argc, char *argv[])
{
	int i;
	uint pid;
	double t[3], rt;
	int r;

	if(argc <= 1){
		fprint(2, "usage: time command\n");
		exits("usage");
	}

	rt = -realtime();

	pid = proc(argv+1, 0, 1, 2, 0);
	if(pid == 0)
		error("proc");

	r = procwait(pid, t);

	/* windows 95 does not compute times */
	if(t[2] == 0)
		t[2] = rt + realtime();

	add("%.3fu %.3fs %.3fr", t[0], t[1], t[2]);
	add("\t");
	for(i=1; i<argc; i++){
		add("%s", argv[i], 0);
		if(i>4){
			add("...");
			break;
		}
	}
	if(r)
		add(" # status=%d", r);
	fprint(2, "%s\n", output);
	if(r)
		exits("error");
	exits(0);
}

void
add(char *a, ...)
{
	static beenhere=0;
	va_list arg;

	if(beenhere)
		strcat(output, " ");
	va_start(arg, a);
	doprint(output+strlen(output), output+sizeof(output), a, arg);
	va_end(arg);
	beenhere++;
}

void
error(char *s)
{
	char buf[ERRLEN];

	errstr(buf);
	fprint(2, "time: %s: %s\n", s, buf);
	exits(s);
}
