#include <u.h>
#include <9pm/libc.h>

char*
strdup(char *s) 
{  
	char *os;

	os = malloc(strlen(s) + 1);
	if(os == 0)
		return 0;
	return strcpy(os, s);
}

