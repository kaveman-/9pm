#include "win.h"

#undef GetModuleHandle
HMODULE WINAPI
GetModuleHandle(LPCWSTR lpModuleName)
{
	char *path, buf[MAX_PATH];

	if(win_useunicode)
		return GetModuleHandleW(lpModuleName);

	path = 0;
	if(lpModuleName) {
		win_wstr2utf(buf, sizeof(buf), (Rune*)lpModuleName);	
		path = buf;
	}
	
	return GetModuleHandleA(path);
}
