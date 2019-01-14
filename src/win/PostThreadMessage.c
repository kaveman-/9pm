#include "win.h"

#undef PostThreadMessage
BOOL WINAPI
PostThreadMessage(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if(win_useunicode)
		return PostThreadMessageW(idThread, Msg, wParam, lParam);

	return PostThreadMessageA(idThread, Msg, wParam, lParam);
}
