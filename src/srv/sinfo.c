#include <u.h>
#include <9pm/libc.h>
#include <9pm/srv.h>

void
pksinfo(uchar *p, Sinfo *info)
{
	PKLONG(p, info->id[0]);
	PKLONG(p+4, info->id[1]);
	PKLONG(p+8, info->id[2]);
	PKLONG(p+12, info->id[3]);
	memmove(p+16, info->name, Snamelen);
	memmove(p+16+Snamelen, info->type, Snamelen);
	PKLONG(p+16+2*Snamelen, info->mesgsize);
}

void
unpksinfo(Sinfo *info, uchar *p)
{
	info->id[0] = UNPKLONG(p);
	info->id[1] = UNPKLONG(p+4);
	info->id[2] = UNPKLONG(p+8);
	info->id[3] = UNPKLONG(p+12);
	memmove(info->name, p+16, Snamelen);
	info->name[Snamelen-1] = 0;
	memmove(info->type, p+16+Snamelen, Snamelen);
	info->type[Snamelen-1] = 0;
	info->mesgsize = UNPKLONG(p+16+2*Snamelen);
}
