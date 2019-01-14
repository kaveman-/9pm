#include "win.h"

#undef SetFileAttributes
BOOL WINAPI
SetFileAttributes(LPCWSTR lpFileName, DWORD dwFileAttributes)
{
	char file[MAX_PATH];

	if(win_useunicode)
		return SetFileAttributesW(lpFileName, dwFileAttributes);
	win_wstr2utf(file, sizeof(file), (Rune*)lpFileName);
	return SetFileAttributesA(file, dwFileAttributes);
}
