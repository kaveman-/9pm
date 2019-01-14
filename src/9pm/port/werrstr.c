#include <u.h>
#include <9pm/libc.h>

void
werrstr(char *fmt, ...)
{
	char buf[ERRLEN];
	va_list arg;
	
	va_start(arg, fmt);
	doprint(buf, buf+ERRLEN, fmt, arg);
	va_end(arg);

	errstr(buf);
}
