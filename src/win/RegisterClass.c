#include "win.h"

#undef RegisterClass
ATOM WINAPI
RegisterClass(CONST WNDCLASSW *lpWndClass)
{
	WNDCLASSA wc;
	char class[MAX_PATH], menu[MAX_PATH];

	if(win_useunicode)
		return RegisterClassW(lpWndClass);

	memcpy(&wc, lpWndClass, sizeof(wc));
	if(wc.lpszMenuName) {
		win_wstr2utf(menu, sizeof(menu), (Rune*)lpWndClass->lpszMenuName);
		wc.lpszMenuName = menu;
	}
	if((int)wc.lpszClassName >= (1<<16)) {
		win_wstr2utf(class, sizeof(class), (Rune*)lpWndClass->lpszClassName);
		wc.lpszClassName = class;
	}

	return RegisterClassA(&wc);

}
