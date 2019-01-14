#include "win.h"

#undef CreateDirectory

BOOL WINAPI
CreateDirectory(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	char path[MAX_PATH];

	if(win_useunicode)
		return CreateDirectoryW(lpPathName, lpSecurityAttributes);

	win_wstr2utf(path, MAX_PATH, (Rune*)lpPathName);
	return CreateDirectoryA(path, lpSecurityAttributes);	
}
