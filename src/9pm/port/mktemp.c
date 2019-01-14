#include <u.h>
#include <9pm/libc.h>

char*
mktemp(char *as)
{
	char *s;
	unsigned tid;
	int i;
	char err[ERRLEN];

	tid = gettid();
	s = as;
	while(*s++)
		;
	s--;
	while(*--s == 'X') {
		*s = tid % 10 + '0';
		tid = tid/10;
	}
	s++;
	i = 'a';
	while(access(as, 0) != -1) {
		if (i == 'z')
			return "/";
		*s = i++;
	}
	err[0] = 0;
	errstr(err);	/* clear the error */
	return as;
}
