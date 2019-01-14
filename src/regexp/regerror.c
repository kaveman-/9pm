#include <u.h>
#include <9pm/libc.h>
#include <9pm/regexp.h>

void
regerror(char *s)
{
	char buf[132];

	strcpy(buf, "regerror: ");
	strcat(buf, s);
	strcat(buf, "\n");
	write(2, buf, strlen(buf));
	exits("regerr");
}
