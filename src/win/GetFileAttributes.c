#include "win.h"

#undef GetFileAttributes
DWORD WINAPI
GetFileAttributes(LPCWSTR lpFileName)
{
	char path[MAX_PATH];

	if(win_useunicode)
		return GetFileAttributesW(lpFileName);

	win_wstr2utf(path, sizeof(path), (Rune*)lpFileName);
	return GetFileAttributesA(path);
}
