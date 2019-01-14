#include "win.h"

#undef PostMessage
BOOL WINAPI
PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if(win_useunicode)
		return PostMessageW(hWnd, Msg, wParam, lParam);

	return PostMessageA(hWnd, Msg, wParam, lParam);
}
