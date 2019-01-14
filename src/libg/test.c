#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

void
main(int argv, char *argc[])
{
	int i;
	Point p;

	USED(argv);
	USED(argc);

	binit(0, 0, 0);

	p = screen.r.min;
	for (i = 0; i < 200; i += 2) {
		segment(&screen, add(p, Pt(i,0)), add(p, Pt(100,100)), ~0, S);
		segment(&screen, add(p, Pt(200,i)), add(p, Pt(100,100)), ~0, S);
		segment(&screen, add(p, Pt(200-i,200)), add(p, Pt(100,100)), ~0, S);
		segment(&screen, add(p, Pt(0,200-i)), add(p, Pt(100,100)), ~0, S);
	}

	string(&screen, screen.r.min, font, "Hello World", S);

	bflush();
	sleep(20000);
}
