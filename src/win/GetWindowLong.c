#include "win.h"

#undef GetWindowLong
LONG WINAPI
GetWindowLong(HWND hWnd, int nIndex)
{
	if(win_useunicode)
		return GetWindowLongW(hWnd, nIndex);

	return GetWindowLongA(hWnd, nIndex);
}
