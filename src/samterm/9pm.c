#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>
#include <9pm/frame.h>
#include <9pm/srv.h>
#include <9pm/windows.h>
#include "flayer.h"
#include "samterm.h"

static char exname[64];
	
static void	extthread(void*);

extern	HWND	libg_window;

void
getscreen(char *font)
{
	binit(panic, font, "sam");
	bitblt(&screen, screen.clipr.min, &screen, screen.clipr, 0);
}

int
screensize(int *w, int *h)
{
	int fd, n;
	char buf[5*12+1];

	fd = open("/dev/screen", OREAD);
	if(fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n != sizeof(buf)-1)
		return 0;
	buf[n] = 0;
	if(h){
		*h = atoi(buf+4*12)-atoi(buf+2*12);
		if (*h < 0)
			return 0;
	}
	if(w){
		*w = atoi(buf+3*12)-atoi(buf+1*12);
		if (*w < 0)
			return 0;
	}
	return 1;
}

int
snarfswap(char *fromsam, int nc, char **tosam)
{
	char *s1;
	int n;

	*tosam = s1 = alloc(SNARFSIZE);
	n = clipread(*tosam, SNARFSIZE-1);
	clipwrite(fromsam, nc);
	if(n == 0){
		*tosam = 0;
		free(s1);
	}else
		s1[n] = 0;

	return n;
}

void
dumperrmsg(int count, int type, int count0, int c)
{
	fprint(2, "samterm: host mesg: count %d %ux %ux %ux %s...ignored\n",
		count, type, count0, c, rcvstring());
}

void
extstart(void)
{
	int p[2];

	if(pipe(p) < 0)
		return;
	estart(Eextern, p[0], 1000);
	thread(extthread, (void*)p[1]);
}

void
extthread(void *a)
{
	int fd;
	int asid, csid, n;
	char buf[1000], adir[Spathlen];

	fd = (int)a;

	srvinit();
	if(!srvmount("smc", &srvsmc, 0))
		fatal("srvmount failed: %r");

	sprint(buf, "smc!sam.%s", getuser());

	asid = announce(buf, adir);

	if(asid<0)
		fatal("smc announce: %r");

	for(;;) {
		csid = listen(asid, adir);
		if(csid<0)
			fatal("smc listen: %r");
		if(accept(csid)<0)
			fatal("smc accept: %r");
		for(;;) {
			n = srvread(csid, buf, sizeof(buf));
			if(n <= 0)
				break;
			write(fd, buf, n);
			if(IsIconic(libg_window))
				ShowWindow(libg_window, SW_RESTORE);
			SetForegroundWindow(libg_window);
		}
		srvclose(csid);
	}
}

