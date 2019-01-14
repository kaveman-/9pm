#include "win.h"

#undef RemoveDirectory
BOOL WINAPI
RemoveDirectory(LPCWSTR lpPathName)
{
	char path[MAX_PATH];

	if(win_useunicode)
		return RemoveDirectoryW(lpPathName);
	win_wstr2utf(path, sizeof(path), (Rune*)lpPathName);
	return RemoveDirectoryA(path);
}
