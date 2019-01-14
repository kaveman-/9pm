#include	<u.h>
#include	<9pm/libc.h>
#include	<9pm/bio.h>

long
Bseek(Biobuf *bp, long offset, int base)
{
	long n, d;
	Dir dir;

	switch(bp->state) {
	default:
		fprint(2, "Bseek: unknown state %d\n", bp->state);
		return Beof;

	case Bracteof:
		bp->state = Bractive;
		bp->icount = 0;
		bp->gbuf = bp->ebuf;

	case Bractive:
		n = offset;
		if(base == 1) {
			n += Boffset(bp);
			base = 0;
		}

		/*
		 * try to seek within buffer
		 */
		if(base == 0) {
			d = n - Boffset(bp);
			bp->icount += d;
			if(d >= 0) {
				if(bp->icount <= 0)
					return n;
			} else {
				if(bp->ebuf - bp->gbuf >= -bp->icount)
					return n;
			}
		}

		/*
		 * reset the buffer
		 */
		if(base == 2) {
			dirfstat(bp->fid, &dir);
			n += dir.length;
		}
/*		n = seek(bp->fid, n, base); */
		bp->icount = 0;
		bp->gbuf = bp->ebuf;
		break;

	case Bwactive:
		Bflush(bp);
		n = offset;
		switch(base) {
		case 0:
			break;
		case 1:
			n += Boffset(bp);
			break;
		case 2:
			dirfstat(bp->fid, &dir);
			n += dir.length;
			break;
		}
/*		n = seek(bp->fid, offset, base); */
		break;
	}
	bp->offset = n;
	return n;
}

long
Boffset(Biobuf *bp)
{
	long n;

	switch(bp->state) {
	default:
		fprint(2, "Boffset: unknown state %d\n", bp->state);
		n = Beof;
		break;

	case Bracteof:
	case Bractive:
		n = bp->offset + bp->icount;
		break;

	case Bwactive:
		n = bp->offset + (bp->bsize + bp->ocount);
		break;
	}
	return n;
}
