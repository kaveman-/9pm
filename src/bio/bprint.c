#include	<u.h>
#include	<9pm/libc.h>
#include	<9pm/bio.h>

int	printcol;

int
Bprint(Biobuf *bp, char *fmt, ...)
{
	va_list arg;
	int n, pcol;
	char *ip, *ep, *out;

	va_start(arg, fmt);
	ep = (char*)bp->ebuf;
	ip = ep + bp->ocount;
	pcol = printcol;
	out = doprint(ip, ep, fmt, arg);
	if(out >= ep-5) {
		Bflush(bp);
		ip = ep + bp->ocount;
		printcol = pcol;
		out = doprint(ip, ep, fmt, arg);
		if(out >= ep-5) {
			va_end(arg);
			return Beof;
		}
	}
	n = out-ip;
	bp->ocount += n;
	va_end(arg);
	return n;
}
