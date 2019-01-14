#include "win.h"

#undef LoadCursor
HCURSOR WINAPI
LoadCursor(HINSTANCE hInstance, LPCWSTR lpCursorName)
{
	char name[MAX_PATH];

	if(win_useunicode)
		return LoadCursorW(hInstance, lpCursorName);
	if((int)lpCursorName < (1<<16)) {
		/* probably not a string */
		return LoadCursorA(hInstance, (char*)lpCursorName);
	}
	
	win_wstr2utf(name, sizeof(name), (Rune*)lpCursorName);
	return LoadCursorA(hInstance, name);
}
