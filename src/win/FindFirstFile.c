#include "win.h"

#undef FindFirstFile
HANDLE WINAPI
FindFirstFile(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	char path[MAX_PATH];
	WIN32_FIND_DATAA data;
	HANDLE h;

	if(win_useunicode)
		return FindFirstFileW(lpFileName, lpFindFileData);

	win_wstr2utf(path, sizeof(path), (Rune*)lpFileName);
	h = FindFirstFileA(path, &data);
	if(h == INVALID_HANDLE_VALUE)
		return h;
	win_convfiledata(lpFindFileData, &data);
	return h;
}
