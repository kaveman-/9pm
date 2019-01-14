#include	<u.h>
#include	<9pm/libc.h>
#include	<9pm/bio.h>

int
Bputc(Biobuf *bp, int c)
{
	int i, j;

loop:
	i = bp->ocount;
	j = i+1;
	if(i != 0) {
		bp->ocount = j;
		bp->ebuf[i] = c;
		return 0;
	}
	if(bp->state != Bwactive)
		return Beof;
	j = writex(bp->fid, bp->bbuf, bp->bsize, bp->offset);
	if(j == bp->bsize) {
		bp->ocount = -bp->bsize;
		bp->offset += j;
		goto loop;
	}
	fprint(2, "Bputc: write error\n");
	bp->state = Binactive;
	bp->ocount = 0;
	return Beof;
}
