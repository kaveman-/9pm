#include "win.h"

#undef SendMessage
LRESULT WINAPI
SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if(win_useunicode)
		return SendMessageW(hWnd, Msg, wParam, lParam);

	return SendMessageA(hWnd, Msg, wParam, lParam);
}
