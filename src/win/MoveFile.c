#include "win.h"

#undef MoveFile
BOOL WINAPI
MoveFile(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName)
{
	char	path[MAX_PATH], newpath[MAX_PATH];

	if(win_useunicode)
		return MoveFileW(lpExistingFileName, lpNewFileName);
	win_wstr2utf(path, sizeof(path), (Rune*)lpExistingFileName);
	win_wstr2utf(newpath, sizeof(newpath), (Rune*)lpNewFileName);
	return MoveFileA(path, newpath);
}
