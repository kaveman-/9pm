#include <u.h>
#include <9pm/libc.h>

void	
pkushort(uchar *p, uint i)
{
	p[0] = i;
	p[1] = i>>8;
}

void	
pklong(uchar *p, long i)
{
	p[0] = i;
	p[1] = i>>8;
	p[2] = i>>16;
	p[3] = i>>24;
}

void	
pkulong(uchar *p, ulong i)
{
	p[0] = i;
	p[1] = i>>8;
	p[2] = i>>16;
	p[3] = i>>24;
}

uint	
unpkushort(uchar *p)
{
	return (p[1] << 8) | p[0];
}

long	
unpklong(uchar *p)
{
	return ((long)(signed char)p[3] << 24) | ((uint)p[2]<<16) |
			((uint)p[1]<<8) | p[0];
}

ulong	
unpkulong(uchar *p)
{
	return ((ulong)p[3] << 24) | ((ulong)p[2]<<16) |
			((ulong)p[1]<<8) | p[0];
}
