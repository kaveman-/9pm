#include <u.h>
#include <9pm/libc.h>
#include "9pm.h"

int
tas(int *p)
{	
	int v;
	
	_asm {
		mov	eax, p
		mov	ebx, 1
		xchg	ebx, [eax]
		mov	v, ebx
	}

	return v;
}

