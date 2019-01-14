#include "win.h"

#undef GetModuleFileName
DWORD WINAPI
GetModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
{
	char path[MAX_PATH];
	int n;

	if(win_useunicode)
		return GetModuleFileNameW(hModule, lpFilename, nSize);

	n = GetModuleFileNameA(hModule, path, sizeof(path));
	if(n == 0)
		return 0;
	return win_utf2wstr(lpFilename, nSize, path);
}
