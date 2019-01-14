#include "win.h"

#undef CreateFileMapping
HANDLE WINAPI
CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow,
		LPCWSTR lpName)
{
	char name[MAX_PATH];
	char *p;

	if(win_useunicode)
		return CreateFileMappingW(hFile, lpFileMappingAttributes,
			flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
	
	if(lpName) {
		win_wstr2utf(name, sizeof(name), (Rune*)lpName);
		p = name;
	} else
		p = 0;

	return CreateFileMappingA(hFile, lpFileMappingAttributes,
			flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, 0);
}

