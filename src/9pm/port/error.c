#include <u.h>
#include <9pm/libc.h>

void
error(char *fmt, ...)
{
	char buf[ERRLEN];
	va_list arg;

	va_start(arg, fmt);
	if(fmt) {
		doprint(buf, buf+sizeof(buf), fmt, arg);
		errstr(buf);
	}	
	va_end(arg);

	nexterror();
}

