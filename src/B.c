#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

void
main(int argc, char *argv[])
{
	char buf[8000], files[8000], cd[1000], **oargv;
	char *p;
	int line, id, n;

	oargv = argv;
	
	p = files;
	argv++;
	argc--;

	line = 0;

	for(; argc > 0; argc--,argv++) {
		switch(argv[0][0]) {
		case '-':
			line = atoi(argv[0]+1);
			break;
		case '/':
		case '\\':
			p += sprint(p, " \"%s\"", argv[0]);
			break;
		default:
			if(argv[0][1] == ':')
				p += sprint(p, " \"%s\"", argv[0]);
			else
				p += sprint(p, " \"%s/%s\"", getwd(cd, sizeof(cd)), argv[0]);
		}
	}

	for(p=files; *p; p++)
		if(*p == '\\')
			*p = '/';

	srvinit();
	if(!srvmount("smc", &srvsmc, 0))
		fatal("srvmount failed: %r");
	
	sprint(buf, "smc!sam.%s", getuser());
	id = dial(buf, 0, 0);
	if(id < 0) {
		oargv[0] = "sam";
		proc(oargv, 0, 1, 2, 0);
		sleep(1000);	/* can't exit straight away! */
	} else {
		n = sprint(buf, "B%s\n", files);
		if(srvwrite(id, buf, n) < 0)
			fatal("srvwrite failed: %r");
		if(line != 0) {
			n = sprint(buf, "%d\n", line);
			if(srvwrite(id, buf, p-buf) < 0)
				fatal("srvwrite failed: %r");
		}
		srvclose(id);
	}
}
