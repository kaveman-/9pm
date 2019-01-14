#include "win.h"

#undef GetMessage
BOOL WINAPI 
GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	if(win_useunicode)
		return GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);

	return GetMessageA(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
}
