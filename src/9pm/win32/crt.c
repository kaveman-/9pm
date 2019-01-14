#include <u.h>
#include <9pm/libc.h>
#include <9pm/windows.h>
#include "9pm.h"

char *argv0;

extern int	main(int, char*[]);

extern void	pm_argv0(char*);

static int	args(char *argv[], int n, char *p);

void
mainCRTStartup(void)
{
	int argc, n;
	char *arg, *p, **argv;
	Rune *warg;

	win_useunicode = win_hasunicode();

	warg = GetCommandLine();
	n = (wstrlen(warg)+1)*UTFmax;
	arg = malloc(n);
	wstrtoutf(arg, warg, n);

	/* conservative guess at the number of args */
	for(argc=4,p=arg; *p; p++)
		if(*p == ' ' || *p == '\t')
			argc++;
	argv = malloc(argc*sizeof(char*));
	argc = args(argv, argc, arg);
	argv0 = argv[0];

	/* set argv0 in 9pm.dll */
	pm_argv0(argv0);

	main(argc, argv);
	threadexit();
}

void
WinMainCRTStartup(void)
{
	mainCRTStartup();
}

/*
 * Break the command line into arguments
 * The rules for this are not documented but appear to be the following
 * according to the source for the microsoft C library.
 * Words are seperated by space or tab
 * Words containing a space or tab can be quoted using "
 * 2N backslashes + " ==> N backslashes and end quote
 * 2N+1 backslashes + " ==> N backslashes + literal "
 * N backslashes not followed by " ==> N backslashes
 */
static int
args(char *argv[], int n, char *p)
{
	char *p2;
	int i, j, quote, nbs;

	for(i=0; *p && i<n-1; i++) {
		while(*p == ' ' || *p == '\t')
			p++;
		quote = 0;
		argv[i] = p2 = p;
		for(;*p; p++) {
			if(!quote && (*p == ' ' || *p == '\t'))
				break;
			for(nbs=0; *p == '\\'; p++,nbs++)
				;
			if(*p == '"') {
				for(j=0; j<(nbs>>1); j++)
					*p2++ = '\\';
				if(nbs&1)
					*p2++ = *p;
				else
					quote = !quote;
			} else {
				for(j=0; j<nbs; j++)
					*p2++ = '\\';
				*p2++ = *p;
			}
		}
		/* move p up one to avoid pointing to null at end of p2 */
		if(*p)
			p++;
		*p2 = 0;	
	}
	argv[i] = 0;

	return i;
}

