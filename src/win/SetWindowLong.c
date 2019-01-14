#include "win.h"

#undef SetWindowLong
LONG WINAPI
SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	if(win_useunicode)
		return SetWindowLongW(hWnd, nIndex, dwNewLong);

	return SetWindowLongA(hWnd, nIndex, dwNewLong);
}
