#include "win.h"

#undef DeleteFile
BOOL WINAPI
DeleteFile(LPCWSTR lpFileName)
{
	char file[MAX_PATH];

	if(win_useunicode)
		return DeleteFileW(lpFileName);
	
	win_wstr2utf(file, sizeof(file), (Rune*)lpFileName);
	return DeleteFileA(file);
}
