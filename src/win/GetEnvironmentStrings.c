#include "win.h"

/* for some reason this is different from the others */
#undef GetEnvironmentStrings
LPWSTR WINAPI
_GetEnvironmentStrings(VOID)
{
	int n;
	Rune *buf, *q;
	char *s, *p;

	if(win_useunicode)
		return GetEnvironmentStringsW();

	s = GetEnvironmentStrings();
	for(p=s,n=0; *p; p+=strlen(p)+1)
		n += win_utflen(p)+1;
	n++;
	buf = malloc(n*sizeof(Rune));
	for(p=s,q=buf; *p; p+=strlen(p)+1)
		 q += win_utf2wstr(q, n, p)+1;
	*q++ = 0;
	FreeEnvironmentStringsA(s);
	return buf;
}
