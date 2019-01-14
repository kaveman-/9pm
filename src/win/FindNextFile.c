#include "win.h"

#undef FindNextFile
BOOL WINAPI
FindNextFile(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
	WIN32_FIND_DATAA data;
	BOOL b;

	if(win_useunicode)
		return FindNextFileW(hFindFile, lpFindFileData);

	b = FindNextFileA(hFindFile, &data);
	if(!b)
		return b;
	win_convfiledata(lpFindFileData, &data);
	return b;	
}
