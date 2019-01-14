#include "win.h"

#undef DefWindowProc
LRESULT WINAPI
DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if(win_useunicode)
		return DefWindowProcW(hWnd, Msg, wParam, lParam);

	return DefWindowProcA(hWnd, Msg, wParam, lParam);
}
