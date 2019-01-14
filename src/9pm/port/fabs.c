#include <u.h>
#include <9pm/libc.h>

#pragma function ( fabs )

double
fabs(double arg)
{

	if(arg < 0)
		return -arg;
	return arg;
}
