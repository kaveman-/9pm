#include "win.h"

#undef ReadConsole
BOOL WINAPI
ReadConsole(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
	LPDWORD lpNumberOfCharsRead, LPVOID lpReserved)
{
	char *buf;
	Rune *p;
	int r, i, n;

	if(win_useunicode)
		return ReadConsoleW(hConsoleInput, lpBuffer, nNumberOfCharsToRead,
				lpNumberOfCharsRead, lpReserved);

	buf = malloc(nNumberOfCharsToRead);
	r = ReadConsoleA(hConsoleInput, buf, nNumberOfCharsToRead, lpNumberOfCharsRead,
		lpReserved);
	/* expand to runes */
	if(r) {
		n = *lpNumberOfCharsRead;
		p = lpBuffer;
		for(i=0; i<n; i++)
			p[i] = buf[i];
	}
	free(buf);
	return r;
}
