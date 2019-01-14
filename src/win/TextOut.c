#include "win.h"

#undef TextOut
BOOL WINAPI
TextOut(HDC hdc, int x, int y, LPCWSTR s, int n)
{
	/* TextOutW is implemented on Windows 95 */
	return TextOutW(hdc, x, y, s, n);
}

