#include "win.h"

#undef WriteConsole
BOOL WINAPI
WriteConsole(HANDLE hConsoleOutput, CONST VOID *lpBuffer,
	DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
	char *buf, *p;
	int n, i;
	BOOL b;

	if(win_useunicode)
		return WriteConsoleW(hConsoleOutput, lpBuffer,
			nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);

	n = nNumberOfCharsToWrite*UTFmax;
	buf = malloc(n+1);
	for(p=buf,i=0; i<(int)nNumberOfCharsToWrite; i++)
		p += win_runetochar(p, ((Rune*)lpBuffer)+i);
	b = WriteConsoleA(hConsoleOutput, buf, p-buf, lpNumberOfCharsWritten, lpReserved);
	free(buf);
	return b;
}
