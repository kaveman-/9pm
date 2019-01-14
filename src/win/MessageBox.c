#include "win.h"


#undef MessageBox
int WINAPI
MessageBox(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	/* MessageBoxW implement on Windows 95 */
	return MessageBoxW(hWnd, lpText, lpCaption, uType);
}
