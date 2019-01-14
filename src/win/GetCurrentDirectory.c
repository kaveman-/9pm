#include "win.h"

#undef GetCurrentDirectory
DWORD WINAPI
GetCurrentDirectory(DWORD nBufferLength, LPWSTR lpBuffer)
{
	char path[MAX_PATH];
	int n;

	if(win_useunicode)
		return GetCurrentDirectoryW(nBufferLength, lpBuffer);

	n = GetCurrentDirectoryA(sizeof(path), path);
	if(n == 0)
		return 0;
	return win_utf2wstr(lpBuffer, nBufferLength, path);
}
