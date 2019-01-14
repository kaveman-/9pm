#include "win.h"

#undef SetClassLong
DWORD WINAPI
SetClassLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	if(win_useunicode)
		return SetClassLongW(hWnd, nIndex, dwNewLong);
	return SetClassLongA(hWnd, nIndex, dwNewLong);
}
