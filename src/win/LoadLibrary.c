#include "win.h"

#undef LoadLibrary
HMODULE WINAPI
LoadLibrary(LPCWSTR lpLibFileName)
{
	char file[MAX_PATH];

	if(win_useunicode)
		return LoadLibraryW(lpLibFileName);
	win_wstr2utf(file, sizeof(file), (Rune*)lpLibFileName);
	return LoadLibraryA(file);
}
