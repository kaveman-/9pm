#include "win.h"

#undef GetDriveType
UINT WINAPI
GetDriveType(LPCWSTR lpRootPathName)
{
	char	path[MAX_PATH];

	if(win_useunicode)
		return GetDriveTypeW(lpRootPathName);
	win_wstr2utf(path, sizeof(path), (Rune*)lpRootPathName);
	return GetDriveTypeA(path);
}
