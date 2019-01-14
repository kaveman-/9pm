#include "win.h"

#undef CreateFile

HANDLE WINAPI
CreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	char path[MAX_PATH];

	if(win_useunicode)
		return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);

	win_wstr2utf(path, MAX_PATH, (Rune*)lpFileName);
	return CreateFileA(path, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);	
}
