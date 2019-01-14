#include <u.h>
#include <9pm/libc.h>

void
main(void)
{
	char **envp, *p;
	int i;

	envp = getenvall();
	for(i=0; envp[i]; i++)
		print("%s\n", envp[i]);
/*
	for(i=0; envp[i]; i++) {
		p = strchr(envp[i], '=');
		if(p == 0 || p == envp[i])
			continue;
		*p = 0;
		print("%s %s\n", envp[i], getenv(envp[i]));
	}
*/
}
