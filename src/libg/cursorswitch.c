#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>

void
cursorswitch(Cursor *c)
{
	uchar curs[2*4+2*2*16];

/*
	if(c == 0)
		write(_cursorfd, curs, 0);
	else{
		BPLONG(curs+0*4, c->offset.x);
		BPLONG(curs+1*4, c->offset.y);
		memmove(curs+2*4, c->clr, 2*2*16);
		write(_cursorfd, curs, sizeof curs);
	}
*/
}
