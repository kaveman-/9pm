#include "win.h"

#undef SetCurrentDirectory
BOOL WINAPI
SetCurrentDirectory(LPCWSTR lpPathName)
{
	char path[MAX_PATH];

	if(win_useunicode)
		return SetCurrentDirectoryW(lpPathName);

	win_wstr2utf(path, sizeof(path), (Rune*)lpPathName);
	return SetCurrentDirectoryA(path);
}

