#include "win.h"

#undef MessageBoxEx
int WINAPI
MessageBoxEx(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId)
{
	/* MessageBoxW implement on Windows 95 */
	return MessageBoxExW(hWnd, lpText, lpCaption, uType, wLanguageId);
}
