#include "win.h"

#undef CreateWindowEx
HWND WINAPI 
CreateWindowEx(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
	DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent,
	HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	char class[MAX_PATH], name[MAX_PATH];

	if(win_useunicode)
		return CreateWindowExW(dwExStyle, lpClassName, lpWindowName,
			dwStyle, X, Y, nWidth, nHeight, hWndParent,
			hMenu, hInstance, lpParam);

	win_wstr2utf(class, sizeof(class), (Rune*)lpClassName);
	win_wstr2utf(name, sizeof(name), (Rune*)lpWindowName);
	
	return CreateWindowExA(dwExStyle, class, name,
			dwStyle, X, Y, nWidth, nHeight, hWndParent,
			hMenu, hInstance, lpParam);
}
