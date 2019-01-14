#include "win.h"

#undef GetEnvironmentVariable
DWORD WINAPI
GetEnvironmentVariable(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize)
{
	char name[MAX_PATH], *buf;
	int n, n2;

	if(win_useunicode)
		return GetEnvironmentVariableW(lpName, lpBuffer, nSize);

	win_wstr2utf(name, sizeof(name), (Rune*)lpName);
	n = nSize*UTFmax;
	buf = malloc(n);
	n2 = GetEnvironmentVariableA(name, buf, n);
	if(n2 == 0) {
		free(buf);
		return 0;
	}
	if(n2 >= n) {
		free(buf);
		lpBuffer[0] = 0;
		return n2;
	}
	n2 = win_utf2wstr(lpBuffer, nSize, buf);
	free(buf);
	
	return n2;
}

