#include	<u.h>
#include	<9pm/libc.h>
#include	<9pm/bio.h>

int
Bfildes(Biobuf *bp)
{

	return bp->fid;
}
