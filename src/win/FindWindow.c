#include "win.h"

#undef FindWindow
HWND WINAPI
FindWindow(LPCWSTR lpClassName, LPCWSTR lpWindowName)
{
	char class[MAX_PATH], window[MAX_PATH];

	if(win_useunicode)
		return FindWindowW(lpClassName, lpWindowName);
	
	win_wstr2utf(class, sizeof(class), (Rune*)lpClassName);
	win_wstr2utf(window, sizeof(window), (Rune*)lpWindowName);
	return FindWindowA(class, window);
}
